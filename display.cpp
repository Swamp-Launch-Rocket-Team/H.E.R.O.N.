// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "rasalhague_packet.h"
#include "session_log.h"
#include "rx_line.h"
#include "log_tail.h"
#include "flight_stats.h"
#include "implot.h"
#include "map_image.h"
#include <cmath>
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Select GL version + let the backend select a GLSL version
    const char* glsl_version = nullptr;
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + generally GLSL 150
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + generally GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "Swamp Launch GCS", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);
    MapImage map;
    if(!fillMapImage(map,"maps\\test_site.jpg","maps\\test_site.txt")) fprintf(stderr, "map failed to load\n");
    else printf("map %d x %d, W %.6f S %.6f E %.6f N %.6f\n", map.width, map.height, map.west, map.south, map.east, map.north);

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
    long long last_packet_ms = 0;
    long long next_check_ms = 0;
    double last_good_latitude = 0;
    double last_good_longitude = 0;
    long long last_good_fix_ms = 0;
    bool have_good_fix = false;
    bool have_packet = false;
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
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }
        double fix_age_s = (SessionLog::now_ms() - last_good_fix_ms) / 1000.0;
        if(SessionLog::now_ms() >= next_check_ms){
            std::string before = tail.current();
            tail.follow_newest("logs");
            if(tail.current() != before) stats = FlightStats{};
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
             
        }
      
        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        double age_s = (SessionLog::now_ms() - last_packet_ms) / 1000.0;
        bool no_signal = have_packet && age_s > limit_s;
        if(no_signal){
            ImGui::PushFont(nullptr, 32.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "NO SIGNAL");
            ImGui::PopFont();
        } 
        if(no_signal) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Begin("Telemetry");
        if(!have_packet) ImGui::Text("NO PACKET");
        else{
            ImGui::Text("Baro alt: %.1f m", pkt.baro_alt_m);     
            
        }
        ImGui::End();
        ImGui::Begin("Flight");
        if(!have_packet) ImGui::Text("NO PACKET");
        else{
            ImGui::Text("Time since last packet %.1f s", age_s);
            ImGui::Text("FC time %.1f s", pkt.timestamp_ms/1000.0);
            if(stats.launched == true) ImGui::Text("T+ %.1f s", stats.t_plus_s);
            else ImGui::Text("T+ -- s");
            ImGui::Text("Flight State: %s", flightStateToString(pkt.flight_state));
        }
        ImGui::End();
        ImGui::Begin("Altitude");
        if(!have_packet) ImGui::Text("NO PACKET");
        else{
            ImGui::Text("Vertical Speed %.1f m/s", stats.vspeed_ms);
            if(stats.have_pad) ImGui::Text("Height Above Pad %.1f m\nPeak Altitude %.1f m", stats.agl_m, stats.peak_agl_m);
            else ImGui::Text("Height Above Pad -- m\nPeak Altitude -- m" );
            ImGui::Text("GPS climb: %.1f m/s", -pkt.gps_vel_down_ms);
            ImGui::Text("Temperature: %.1f C\nPressure: %.1f hPa", pkt.baro_temp_c, pkt.baro_pressure_hpa);
        }
        ImGui::End();
        ImGui::Begin("Health and Link");
        if(!have_packet) ImGui::Text("NO PACKET");
        else{
            ImGui::Text("Battery Voltage %.2f V" , pkt.batt_voltage_v);
            ImGui::Text("RSSI (signal strength): %.1f dBm \nSNR (signal quality) %.1f dB", rx.rssi, rx.snr);
            ImGui::Text("Callsign: %s", pkt.callsign);
        }
        ImGui::End();
        ImGui::Begin("GPS");
        if(!have_packet)ImGui::Text("NO PACKET"); 
        else{ 
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
        }
        ImGui::End();
        ImGui::Begin("IMU");
        if(!have_packet) ImGui::Text("NO PACKET");
        else{
            ImGui::Text("Total Acceleration: %.1f g \n x: %.1fg y: %.1fg z: %.1fg" ,std::sqrt(pkt.accel_x_g*pkt.accel_x_g + pkt.accel_y_g*pkt.accel_y_g +pkt.accel_z_g*pkt.accel_z_g), pkt.accel_x_g,pkt.accel_y_g,pkt.accel_z_g);
            ImGui::Text("Rotation Rate x: %.1f deg/s y: %.1f deg/s z: %.1f deg/s", pkt.gyro_x_dps, pkt.gyro_y_dps, pkt.gyro_z_dps);
        }
        ImGui::End();
        ImGui::Begin("Ground track");
        if(ImPlot::BeginPlot("##track", ImVec2(-1, -1), ImPlotFlags_Equal)){
            ImPlot::SetupAxes("East (m)", "North (m)", 0, 0);
            ImPlot::SetupAxesLimits(-500, 500, -500, 500, ImPlotCond_Once);
            if(map.tex != 0 && stats.have_pad_pos){
                double sw_e, sw_n, ne_e, ne_n;
                latlon_to_en(stats, map.south, map.west, sw_e, sw_n);
                latlon_to_en(stats, map.north, map.east, ne_e, ne_n);
                ImPlotSpec spec;
                spec.Flags = ImPlotItemFlags_NoFit;
                ImPlot::PlotImage("##map", (ImTextureID)map.tex, ImPlotPoint(sw_e, sw_n), ImPlotPoint(ne_e, ne_n), ImVec2(0,0), ImVec2(1,1), ImVec4(1,1,1,1), spec);
            }
            double zero = 0.0;
            ImPlot::PlotLine("Track", stats.track_e.data(), stats.track_n.data(), (int)stats.track_e.size());
            ImPlot::PlotScatter("Pad Marker", &zero,&zero,1);
            if(!stats.track_e.empty()) ImPlot::PlotScatter("Last fix", &stats.track_e.back(), &stats.track_n.back(), 1);
            ImPlot::EndPlot();
        }
        ImGui::End();
        if(no_signal)ImGui::PopStyleColor();         
        
        
        
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
