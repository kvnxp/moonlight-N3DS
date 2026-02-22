#include "imgui_menu.hpp"
#include "imgui/imgui.h"
#include <3ds.h>
#include <vector>
#include <cstring>

// helper for processing input into ImGui each frame
static void imgui_handle_input() {
    hidScanInput();
    u32 kHeld = hidKeysHeld();
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;

    // map buttons to nav inputs
    #define MAP(NAV,BUTTON) if (kHeld & BUTTON) io.NavInputs[NAV] = 1.0f;
    MAP(ImGuiNavInput_Activate,    KEY_A);
    MAP(ImGuiNavInput_Cancel,      KEY_B);
    MAP(ImGuiNavInput_Menu,        KEY_Y);
    MAP(ImGuiNavInput_Input,       KEY_X);
    MAP(ImGuiNavInput_DpadLeft,    KEY_DLEFT);
    MAP(ImGuiNavInput_DpadRight,   KEY_DRIGHT);
    MAP(ImGuiNavInput_DpadUp,      KEY_DUP);
    MAP(ImGuiNavInput_DpadDown,    KEY_DDOWN);
    MAP(ImGuiNavInput_FocusPrev,   KEY_L);
    MAP(ImGuiNavInput_FocusNext,   KEY_R);
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    #undef MAP

    touchPosition touch;
    hidTouchRead(&touch);
    if (touch.px || touch.py) {
        io.MouseDown[0] = true;
        io.MousePos = ImVec2(touch.px, touch.py);
    } else {
        io.MouseDown[0] = false;
    }
}

int ui::selectionDialog(const char* title, const std::vector<std::string>& options, int default_idx) {
    int selected = default_idx;
    int result = -1;
    bool done = false;
    while (aptMainLoop() && !done) {
        imgui_handle_input();
        ImGui::NewFrame();
        ImGui::Begin(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        for (int i = 0; i < (int)options.size(); ++i) {
            if (ImGui::Selectable(options[i].c_str(), i == selected)) {
                selected = i;
                result = i;
                done = true;
            }
        }
        if (ImGui::Button("Cancel")) {
            result = -1;
            done = true;
        }
        ImGui::End();
        ImGui::Render();
        ::render_imgui_frame();
    }
    return result;
}

std::string ui::textInputDialog(const char* title, const char* hint, const std::string& initial) {
    std::string buf = initial;
    bool done = false;
    while (aptMainLoop() && !done) {
        imgui_handle_input();
        ImGui::NewFrame();
        ImGui::Begin(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::InputText(hint, &buf[0], 256, ImGuiInputTextFlags_AutoSelectAll);
        if (ImGui::Button("OK")) {
            done = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            buf.clear();
            done = true;
        }
        ImGui::End();
        ImGui::Render();
        ::render_imgui_frame();
    }
    return buf;
}

bool ui::boolDialog(const char* title, bool default_val) {
    bool val = default_val;
    bool done = false;
    while (aptMainLoop() && !done) {
        imgui_handle_input();
        ImGui::NewFrame();
        ImGui::Begin(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Checkbox("", &val);
        if (ImGui::Button("OK")) { done = true; }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { val = default_val; done = true; }
        ImGui::End();
        ImGui::Render();
        ::render_imgui_frame();
    }
    return val;
}

int ui::intDialog(const char* title, int default_val) {
    int val = default_val;
    bool done = false;
    char buf[16];
    while (aptMainLoop() && !done) {
        imgui_handle_input();
        ImGui::NewFrame();
        ImGui::Begin(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        sprintf(buf, "%d", val);
        if (ImGui::InputText("", buf, sizeof(buf))) {
            val = atoi(buf);
        }
        if (ImGui::Button("OK")) { done = true; }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { val = default_val; done = true; }
        ImGui::End();
        ImGui::Render();
        ::render_imgui_frame();
    }
    return val;
}
