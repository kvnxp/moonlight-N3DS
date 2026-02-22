#pragma once

#include <string>
#include <vector>

// render the ImGui draw data to screen (defined in n3ds_main.cpp)
void render_imgui_frame();

namespace ui {
    // show a simple selection list; returns index selected or -1 if cancelled
    int selectionDialog(const char* title, const std::vector<std::string>& options, int default_idx);

    // prompt the user to enter text; returns the entered string (empty if cancelled)
    std::string textInputDialog(const char* title, const char* hint, const std::string& initial = "");

    // prompt for boolean choice
    bool boolDialog(const char* title, bool default_val);

    // prompt for integer value, returns the new value or default if cancelled
    int intDialog(const char* title, int default_val);
}