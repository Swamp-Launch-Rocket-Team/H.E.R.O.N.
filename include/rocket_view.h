#pragma once
// rocket_view.h: the 3D rocket panel.
// Draws Will Huff's rocket models (resources/models, from his RocketVisualizer) into a render
// texture, which display.cpp shows inside an ImGui panel. The orbit camera is adapted from his
// OrbitCamera; it takes mouse input only while the panel is hovered.
//
// Frames (same as Will's visualizer): world = NWU (x north, y west, z up);
// body = the model's own axes, nose along +Z. The quaternion passed to Render() is body -> world.
//
// Use, once per frame:
//   inside the panel's ImGui window:   rocket_view.Draw();      (shows the image, reads the mouse)
//   after the panels, before BeginDrawing():   rocket_view.Render(q);
#include "raylib.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"   // raymath's "= { 0 }" initializers
#include "raymath.h"
#include "rlgl.h"
#pragma GCC diagnostic pop
#include "imgui.h"
#include "rlImGui.h"
#include <cmath>
#include <cstdio>
#include <string>

class RocketView {
public:
    // Loads the body and four fins. Call after InitWindow. False if any file is missing.
    bool Load(const std::string& dir = "resources/models") {
        static const char* files[MODEL_COUNT] = {
            "Rocket_Body.glb", "Rocket_Fin_Top.glb", "Rocket_Fin_Bottom.glb",
            "Rocket_Fin_Left.glb", "Rocket_Fin_Right.glb" };
        for (int i = 0; i < MODEL_COUNT; i++) {
            std::string path = dir + "/" + files[i];
            if (!FileExists(path.c_str())) {
                std::fprintf(stderr, "rocket model missing: %s\n", path.c_str());
                Unload();
                return false;
            }
            models[i] = LoadModel(path.c_str());
            loaded = i + 1;
        }
        cam.up = Vector3{ 0.0f, 0.0f, 1.0f };
        cam.fovy = 45.0f;
        cam.projection = CAMERA_PERSPECTIVE;
        return true;
    }

    // Call before CloseWindow.
    void Unload() {
        for (int i = 0; i < loaded; i++) UnloadModel(models[i]);
        loaded = 0;
        if (target.id != 0) UnloadRenderTexture(target);
        target = RenderTexture2D{};
    }

    // Shows the last render, sized to fill the rest of the current ImGui window.
    void Draw() {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        want_w = (int)avail.x;
        want_h = (int)avail.y;
        if (loaded == MODEL_COUNT && target.id != 0 && target.texture.width == want_w && target.texture.height == want_h) {
            rlImGuiImageRenderTexture(&target);
            hovered = ImGui::IsItemHovered();
        } else {
            ImGui::Dummy(avail);   // first frame, a resize, or no models: nothing to show yet
            hovered = false;
            if (loaded != MODEL_COUNT) ImGui::SetCursorPos(ImVec2(8, 30)), ImGui::TextUnformatted("Rocket models not loaded");
        }
    }

    // Draws the scene into the texture for orientation q. Call outside BeginDrawing/EndDrawing.
    void Render(Quaternion q) {
        if (loaded != MODEL_COUNT || want_w < 8 || want_h < 8) return;
        if (target.id == 0 || target.texture.width != want_w || target.texture.height != want_h) {
            if (target.id != 0) UnloadRenderTexture(target);
            target = LoadRenderTexture(want_w, want_h);
        }
        update_camera(hovered);

        Matrix transform = MatrixMultiply(MatrixScale(MODEL_SCALE, MODEL_SCALE, MODEL_SCALE), QuaternionToMatrix(q));
        for (int i = 0; i < MODEL_COUNT; i++) models[i].transform = transform;

        BeginTextureMode(target);
            ClearBackground(Color{ 20, 20, 20, 255 });
            BeginMode3D(cam);
                for (int i = 0; i < MODEL_COUNT; i++) DrawModel(models[i], Vector3{ 0, 0, 0 }, 1.0f, WHITE);
                draw_ground_grid(10, 1.0f);
                // Body axes drawn on top of the model (x red, y green, nose blue)
                rlDrawRenderBatchActive();
                rlDisableDepthTest();
                draw_axes(Vector3{ 0, 0, 0 }, q, 3.0f, true);
                rlDrawRenderBatchActive();
                rlEnableDepthTest();
                // World reference in the grid corner: north (red), west (green)
                draw_axes(Vector3{ 5.0f, 5.0f, 0.001f }, QuaternionIdentity(), 1.0f, false);
            EndMode3D();
        EndTextureMode();
    }

private:
    static constexpr int MODEL_COUNT = 5;
    static constexpr float MODEL_SCALE = 0.1f;   // Will's scale

    Model models[MODEL_COUNT] = {};
    int loaded = 0;
    RenderTexture2D target = {};
    int want_w = 0, want_h = 0;
    bool hovered = false;

    Camera3D cam = {};
    float distance = 10.0f;     // Will's defaults
    float angle_x = 0.785398f;  // azimuth, rad
    float angle_y = 0.523599f;  // elevation, rad

    // Orbit: drag with either mouse button, zoom with the wheel. Input only while hovered.
    void update_camera(bool take_input) {
        cam.target = Vector3{ 0.0f, 0.0f, 1.0f };
        if (take_input) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                Vector2 d = GetMouseDelta();
                angle_x -= d.x * 0.005f;
                angle_y += d.y * 0.005f;
                if (angle_y > 1.57f) angle_y = 1.57f;
                if (angle_y < -1.57f) angle_y = -1.57f;
            }
            distance -= GetMouseWheelMove();
            if (distance < 1.0f) distance = 1.0f;
        }
        float c = std::cos(angle_y);
        cam.position.x = cam.target.x + distance * c * std::cos(angle_x);
        cam.position.y = cam.target.y + distance * c * std::sin(angle_x);
        cam.position.z = cam.target.z + distance * std::sin(angle_y);
    }

    static void draw_ground_grid(int slices, float spacing) {
        const float half = (float)slices * spacing * 0.5f;
        const Color color = Fade(LIGHTGRAY, 0.35f);
        for (int i = 0; i <= slices; i++) {
            const float o = -half + (float)i * spacing;
            DrawLine3D(Vector3{ o, -half, 0.0f }, Vector3{ o, half, 0.0f }, color);
            DrawLine3D(Vector3{ -half, o, 0.0f }, Vector3{ half, o, 0.0f }, color);
        }
    }

    static void draw_axes(Vector3 p, Quaternion q, float len, bool include_z) {
        Matrix m = QuaternionToMatrix(q);
        Vector3 x = { m.m0, m.m1, m.m2 }, y = { m.m4, m.m5, m.m6 }, z = { m.m8, m.m9, m.m10 };
        DrawLine3D(p, Vector3Add(p, Vector3Scale(x, len)), RED);
        DrawLine3D(p, Vector3Add(p, Vector3Scale(y, len)), GREEN);
        if (include_z) DrawLine3D(p, Vector3Add(p, Vector3Scale(z, len)), BLUE);
    }
};
