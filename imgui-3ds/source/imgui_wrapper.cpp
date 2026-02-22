// This file serves as a simple translation unit that pulls in all of the
// Dear ImGui implementation sources.  It exists so that other code (the
// sample program and the main Moonlight app) can include a single file
// instead of copying the four `#include` lines everywhere.
//
// When the repo is built as part of moonlight-N3DS the main application
// already had the sources directly embedded; to keep things tidy we now
// use this wrapper in both places.

#include "imgui/imgui_sw.cpp"
#include "imgui/imgui.cpp"
#include "imgui/imgui_demo.cpp"
#include "imgui/imgui_draw.cpp"
