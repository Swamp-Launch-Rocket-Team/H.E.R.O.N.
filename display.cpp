// display.cpp: the H.E.R.O.N ground station dashboard.
// raylib owns the window and OpenGL context; rlImGui draws Dear ImGui (and ImPlot) through it.
// It follows the newest logs/*_raw.log that gcs.exe writes; start it from the H.E.R.O.N folder.
#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"
#include "rasalhague_packet.h"
#include "session_log.h"
#include "rx_line.h"
#include "log_tail.h"
#include "flight_stats.h"
#include "implot.h"
#include "map_image.h"
#include "history.h"
#include <cmath>
#include <stdio.h>
#ifndef STB_IMAGE_STATIC
#define STB_IMAGE_STATIC   // must match map_image.h: raylib has its own stbi_* functions
#endif
// STB_IMAGE_STATIC makes every stb function private to this file, so -Wall warns about each
// one we never call. raylib silences the same warnings around its own copy.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma GCC diagnostic pop


// Main code
int main(int, char**)
{
    // Window. No FLAG_WINDOW_HIGHDPI: we scale ImGui ourselves (below), as the GLFW version did,
    // and rlImGui only applies its own DPI scale when that flag is set.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "Swamp Launch GCS");
    SetExitKey(KEY_NULL);                     // raylib closes on Esc by default; not on launch day
    float main_scale = GetWindowScaleDPI().x; // monitor scale, e.g. 1.5 at 150%
    SetWindowSize((int)(1280 * main_scale), (int)(800 * main_scale));

    // Dear ImGui + ImPlot. rlImGuiSetup creates the ImGui context, the dark style and the default font.
    IMGUI_CHECKVERSION();
    rlImGuiSetup(true);
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale.
    style.FontScaleDpi = main_scale;        // Set initial font scale.

    MapImage map;
    if(!fillMapImage(map, "maps\\live_site.jpg", "maps\\live_site.txt") &&
   !fillMapImage(map, "maps\\test_site.jpg", "maps\\test_site.txt")) fprintf(stderr, "no map loaded\n");

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    TelemetryPacket pkt;
    RxLine rx;    
    LogTail tail;
    FlightStats stats;
    History hist;
    long long last_packet_ms = 0;
    long long next_check_ms = 0;
    double last_good_latitude = 0;
    double last_good_longitude = 0;
    long long last_good_fix_ms = 0;
    bool have_good_fix = false;
    bool have_packet = false;
    bool map_downloaded = false;
    GsLine gs{};
    bool have_gs = false;
    bool show_gs = true;
    bool show_alt = true, show_vs = true, show_acc = true;
    bool follow = true;
    double x_min = -30, x_max = 2;
    std::string line;
    if(have_packet) last_packet_ms = SessionLog::now_ms();
    double limit_s = std::max(3 * expected_interval_s(pkt.flight_state), 2.0);
    /*
        Graphs,

        Altitude vs Time
        
        Visualizers
            live location via gps
        Buttons to switch btwn them
    
    */
    // Main loop
    while (!WindowShouldClose())
    {
        // Minimized: keep the window responsive (EndDrawing polls input) but skip the work.
        if (IsWindowMinimized())
        {
            BeginDrawing();
            EndDrawing();
            WaitTime(0.01);
            continue;
        }
        double fix_age_s = (SessionLog::now_ms() - last_good_fix_ms) / 1000.0;
        if(SessionLog::now_ms() >= next_check_ms){
            
            std::string before = tail.current();
            tail.follow_newest("logs");
            if(tail.current() != before) {stats = FlightStats{}; hist.clear();}
            next_check_ms = SessionLog::now_ms() + 1000;
        }
        while(tail.next_line(line)){
            size_t tab = line.find('\t');
            if(tab == std::string::npos) continue;
            long long pc_ms = std::strtoll(line.c_str(),nullptr,10);

            std::string text = line.substr(tab+1);
            RxLine new_rx;
            TelemetryPacket new_pkt;
            uint8_t bytes[255];
            
            bool pkt_ok = (parse_rx_line(text, new_rx) 
            && hex_to_bytes(new_rx.hex,bytes,new_rx.hex.size()/2))
            &&decodeTelemetryPacket(bytes, new_rx.hex.size()/2, new_pkt);  
            if(pkt_ok){
                rx = new_rx;
                pkt = new_pkt;
                update_stats(stats,pkt);
                hist.add(pkt,stats);
                last_packet_ms = pc_ms;
                have_packet = true;
                if(pkt.gps_fix_type >= 3){
                   last_good_latitude = pkt.gps_latitude_deg;
                   last_good_longitude = pkt.gps_longitude_deg;
                   last_good_fix_ms = pc_ms;
                   have_good_fix = true;
                    if(stats.have_pad_pos){
                        double east_m, north_m;
                        latlon_to_en(stats, pkt.gps_latitude_deg, pkt.gps_longitude_deg, east_m, north_m);
                        stats.track_e.push_back(east_m);
                        stats.track_n.push_back(north_m);
                    }


                }
                
            } 
            else{
                GsLine new_gs;
                if(parse_gs_line(text,new_gs) && new_gs.fix >=3){
                    gs = new_gs;
                    have_gs = true; 

                } 
            } 
        }

        
        if(stats.have_pad_pos && !map_downloaded){
            map_downloaded = true;
            MapBox box = map_box_around(stats.pad_lat, stats.pad_lon);
            if(download_map(box, "maps\\live_site.jpg", "maps\\live_site.txt")){
                MapImage new_map;
                if(fillMapImage(new_map, "maps\\live_site.jpg", "maps\\live_site.txt")){
                    UnloadTexture(map.tex);
                    map = new_map;
                }
                else fprintf(stderr, "live map failed to load, keeping the old map\n");
            }
        else fprintf(stderr, "live map download failed, keeping the old map\n");
}
        // Start the ImGui frame (rlImGuiBegin calls ImGui::NewFrame)
        rlImGuiBegin();
        //Split Display into 3 windows
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 origin = vp->WorkPos;    // top-left of the usable area
        ImVec2 area   = vp->WorkSize;   // its width and height
        float left_w  = area.x * 0.25f;
        float mid_w   = area.x * 0.50f;
        float right_w = area.x - left_w - mid_w;
        ImGuiWindowFlags fixed = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        double age_s = (SessionLog::now_ms() - last_packet_ms) / 1000.0;
        bool no_signal = have_packet && age_s > limit_s;
        if(no_signal) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::SetNextWindowPos(ImVec2(origin.x + left_w + mid_w, origin.y));
        ImGui::SetNextWindowSize(ImVec2(right_w, area.y));
        ImGui::Begin("Data", nullptr, fixed);
        if(no_signal){
            ImGui::PushFont(nullptr, 32.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "NO SIGNAL");
            ImGui::PopFont();
        } 
        if(!have_packet) ImGui::Text("NO PACKET");
        else {
            ImGui::SeparatorText("Flight");   
            ImGui::Text("Time since last packet %.1f s", age_s);
            ImGui::Text("FC time %.1f s", pkt.timestamp_ms/1000.0);
            if(stats.launched == true) ImGui::Text("T+ %.1f s", stats.t_plus_s);
            else ImGui::Text("T+ -- s");
            ImGui::Text("Flight State: %s", flightStateToString(pkt.flight_state));
            
            ImGui::SeparatorText("Altitude");
            ImGui::Text("Baro alt: %.1f m", pkt.baro_alt_m); 
            ImGui::Text("Vertical Speed %.1f m/s", stats.vspeed_ms);
            if(stats.have_pad) ImGui::Text("Height Above Pad %.1f m\nPeak Altitude %.1f m", stats.agl_m, stats.peak_agl_m);
            else ImGui::Text("Height Above Pad -- m\nPeak Altitude -- m" );
            ImGui::Text("GPS climb: %.1f m/s", -pkt.gps_vel_down_ms);
            ImGui::Text("Temperature: %.1f C\nPressure: %.1f hPa", pkt.baro_temp_c, pkt.baro_pressure_hpa);

            ImGui::SeparatorText("Health and Link");
            ImGui::Text("Battery Voltage %.2f V" , pkt.batt_voltage_v);
            ImGui::Text("RSSI (signal strength): %.1f dBm \nSNR (signal quality) %.1f dB", rx.rssi, rx.snr);
            ImGui::Text("Callsign: %s", pkt.callsign);

            ImGui::SeparatorText("GPS");
            ImGui::Text("GPS altitude: %.1f m", pkt.gps_alt_msl_m);
            if(have_good_fix){
                ImGui::Text("Fix Type: %d, Satellites: %u",pkt.gps_fix_type, pkt.gps_num_sats);
                char label[96];
                std::snprintf(label, sizeof label, "Latitude: %.7f, Longitude: %.7f###copypos", last_good_latitude, last_good_longitude);
                if(ImGui::Button(label)){
                    char buf[64];
                    std::snprintf(buf, sizeof buf, "%.7f, %.7f", last_good_latitude, last_good_longitude);
                    ImGui::SetClipboardText(buf);
                }
            }
            else ImGui::Text("Fix is %fs old", fix_age_s);
            ImGui::Text("Ground Speed: %.1f m/s Headed: %.1f deg", pkt.gps_ground_speed_ms, pkt.gps_heading_deg);

            ImGui::SeparatorText("IMU");
            ImGui::Text("Total Acceleration: %.1f g \n x: %.1fg y: %.1fg z: %.1fg" ,std::sqrt(pkt.accel_x_g*pkt.accel_x_g + pkt.accel_y_g*pkt.accel_y_g +pkt.accel_z_g*pkt.accel_z_g), pkt.accel_x_g,pkt.accel_y_g,pkt.accel_z_g);
            ImGui::Text("Rotation Rate: \nx: %.1f deg/s y: %.1f deg/s z: %.1f deg/s", pkt.gyro_x_dps, pkt.gyro_y_dps, pkt.gyro_z_dps);
        }   
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(origin.x + left_w, origin.y));
        ImGui::SetNextWindowSize(ImVec2(mid_w, area.y));
        ImGui::Begin("Ground track", nullptr, fixed);
        ImGui::Checkbox("Ground Station", &show_gs);
        if(ImPlot::BeginPlot("##track", ImVec2(-1, -1), ImPlotFlags_Equal)){
            ImPlot::SetupAxes("East (m)", "North (m)", 0, 0);
            ImPlot::SetupAxesLimits(-500, 500, -500, 500, ImPlotCond_Once);
            if(map.tex.id != 0 && stats.have_pad_pos){
                double sw_e, sw_n, ne_e, ne_n;
                latlon_to_en(stats, map.south, map.west, sw_e, sw_n);
                latlon_to_en(stats, map.north, map.east, ne_e, ne_n);
                ImPlotSpec spec;
                spec.Flags = ImPlotItemFlags_NoFit;
                ImPlot::PlotImage("##map", (ImTextureID)map.tex.id, ImPlotPoint(sw_e, sw_n), ImPlotPoint(ne_e, ne_n), ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,1), spec);
            }
            double zero = 0.0;
            ImPlot::PlotLine("Track", stats.track_e.data(), stats.track_n.data(), (int)stats.track_e.size());
            ImPlot::PlotScatter("Pad Marker", &zero,&zero,1);
            if(!stats.track_e.empty()) ImPlot::PlotScatter("Last fix", &stats.track_e.back(), &stats.track_n.back(), 1);
            if(show_gs && have_gs && stats.have_pad_pos){
                double gs_e, gs_n;
                latlon_to_en(stats, gs.lat, gs.lon, gs_e, gs_n);  
                ImPlotSpec gs_spec;
                gs_spec.Marker = ImPlotMarker_Square;
                gs_spec.MarkerSize = 6;
                ImPlot::PlotScatter("Ground station", &gs_e, &gs_n, 1, gs_spec);  
            }
            ImPlot::EndPlot();
        }
        ImGui::End();
        ImGui::SetNextWindowPos(ImVec2(origin.x, origin.y));
        ImGui::SetNextWindowSize(ImVec2(left_w, area.y));
        ImGui::Begin("Graphs", nullptr, fixed);
        ImGui::Checkbox("Altitude", &show_alt);
        ImGui::SameLine();
        ImGui::Checkbox("Velocity", &show_vs);
        ImGui::SameLine();
        ImGui::Checkbox("Acceleration", &show_acc);
        ImGui::SameLine(); 
        ImGui::Checkbox("Follow", &follow);
        if(!hist.t_s.empty()){
            double t0 = stats.launched ? stats.liftoff_ms / 1000.0 : hist.t_s.back();
            std::vector<double> x(hist.t_s.size());
            for(size_t i = 0; i < x.size(); i++) x[i] = hist.t_s[i] - t0;
            double now = x.back();
            if(follow){ x_min = now - 30; x_max = now + 2;}
            int n = show_alt + show_vs + show_acc;
            if(n > 0){
                float h = ImGui::GetContentRegionAvail().y / n - ImGui::GetStyle().ItemSpacing.y;
                if(show_alt){
                    if(ImPlot::BeginPlot("Height above pad", ImVec2(-1, h), ImPlotFlags_NoLegend)){
                        ImPlot::SetupAxes("T+ (s)", "m", 0, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
                        ImPlot::SetupAxisLinks(ImAxis_X1, &x_min, &x_max);
                        if(ImPlot::IsPlotHovered() && 
                        (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::GetIO().MouseWheel != 0))
                        follow = false;
                        ImPlot::PlotLine("baro", x.data(), hist.agl_m.data(), (int)x.size());
                        if(stats.launched){ double zero = 0; ImPlot::PlotInfLines("liftoff", &zero, 1); }
                        ImPlot::EndPlot();
                    }
                }
                if(show_vs){
                    if(ImPlot::BeginPlot("Vertical Speed", ImVec2(-1, h))){
        
                        ImPlot::SetupAxes("T+ (s)", "m/s", 0, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
                        ImPlot::SetupAxisLinks(ImAxis_X1, &x_min, &x_max);
                        if(ImPlot::IsPlotHovered() && 
                        (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::GetIO().MouseWheel != 0))
                        follow = false;
                        ImPlot::PlotLine("baro", x.data(), hist.vs_baro.data(), (int)x.size());
                        ImPlot::PlotLine("GPS Climb", x.data(), hist.vs_gps.data(), (int)x.size());
                        if(stats.launched){
                            double zero = 0;
                            ImPlotSpec spec; spec.Flags = ImPlotItemFlags_NoLegend;
                            ImPlot::PlotInfLines("liftoff", &zero, 1, spec);
                        }                        
                        ImPlot::EndPlot();
                    }
                }
                if(show_acc){
                    if(ImPlot::BeginPlot("Acceleration##plot", ImVec2(-1, h), ImPlotFlags_NoLegend)){
                        ImPlot::SetupAxes("T+ (s)", "g", 0, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
                        ImPlot::SetupAxisLinks(ImAxis_X1, &x_min, &x_max);
                        if(ImPlot::IsPlotHovered() && 
                        (ImGui::IsMouseDragging(ImGuiMouseButton_Left) || ImGui::GetIO().MouseWheel != 0))
                        follow = false;
                        ImPlot::PlotLine("|a|", x.data(), hist.accel_g.data(), (int)x.size());
                        if(stats.launched){ double zero = 0; ImPlot::PlotInfLines("liftoff", &zero, 1); }
                        ImPlot::EndPlot();
                        }
                }
            }
            

        }
        ImGui::End();
        if(no_signal)ImGui::PopStyleColor();         
        
        
        
        
        // Rendering (rlImGuiEnd calls ImGui::Render and draws it with raylib)
        BeginDrawing();
        ClearBackground(ColorFromNormalized(Vector4{ clear_color.x, clear_color.y, clear_color.z, clear_color.w }));
        rlImGuiEnd();
        EndDrawing();
    }

    // Cleanup (rlImGuiShutdown destroys the ImGui context)
    if (map.tex.id != 0) UnloadTexture(map.tex);
    ImPlot::DestroyContext();
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
