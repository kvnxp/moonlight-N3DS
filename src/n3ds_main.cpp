/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2019 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef __3DS__

#include "config.h"
#include "loop.h"
#include "platform_main.h"

// include ImGui implementation sources directly so they are built with the project
// (the sample app provides a wrapper file that pulls in all the sources; we
// use the same wrapper here to avoid duplicating the list and to keep
// the build consistent with imgui-3ds/Makefile.)
#include "../imgui-3ds/source/imgui_wrapper.cpp"

// headers for using ImGui API
#include "imgui/imgui.h"
#include "imgui/imgui_sw.h"

#include "n3ds/n3ds_connection.hpp"
#include "n3ds/pair_record.hpp"

#include "audio/audio.h"
#include "video/video.h"

#include "input/n3ds_input.hpp"
#include "ui/imgui_menu.hpp"

#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>

#include <Limelight.h>

#include <client.h>
#include <discover.h>

#include <arpa/inet.h>
#include <exception>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SOC_ALIGN 0x1000
// 0x40000 for each enet host (2 hosts total)
// 0x40000 for each platform socket (2 sockets total)
#define SOC_BUFFERSIZE 0x100000

#define MAX_INPUT_CHAR 60

static u32 *SOC_buffer = NULL;

static PrintConsole topScreen;
static PrintConsole bottomScreen;

// ImGui rendering resources
static C3D_Tex *imgui_tex = nullptr;
static C2D_Image imgui_image;
static C3D_RenderTarget* imgui_target = nullptr;
static std::vector<uint32_t> imgui_pixel_buffer;
static imgui_sw::SwOptions imgui_sw_options;

static inline void wait_for_button(std::string prompt = "") {
    if (prompt.empty()) {
        printf("\nPress any button to continue\n");
    } else {
        printf("\n%s\n", prompt.c_str());
    }
    while (aptMainLoop()) {
        gfxSwapBuffers();
        gfxFlushBuffers();
        gspWaitForVBlank();

        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown)
            break;
    }
}

static void n3ds_exit_handler(void) {
    // Allow users to decide when to exit
    wait_for_button("Press any button to quit");

    NDMU_UnlockState();
    NDMU_LeaveExclusiveState();
    ndmuExit();
    irrstExit();
    SOCU_ShutdownSockets();
    SOCU_CloseSockets();
    socExit();
    free(SOC_buffer);
    romfsExit();
    aptExit();
    gfxExit();
    acExit();
}

static int console_selection_prompt(std::string prompt,
                                    std::vector<std::string> options,
                                    int default_idx) {
    // forward to ImGui-driven dialog for now
    return ui::selectionDialog(prompt.c_str(), options, default_idx);
}

static std::string prompt_for_action(PSERVER_DATA server) {
    if (server->paired) {
        std::vector<std::string> actions = {
            "stream",
            "quit stream",
            "stream settings",
            "unpair",
        };
        int idx = console_selection_prompt("Select an action", actions, 0);
        if (idx < 0) {
            return "";
        }
        return actions[idx];
    }
    std::vector<std::string> actions = {"pair"};
    int idx = console_selection_prompt("Select an action", actions, 0);
    if (idx < 0) {
        return "";
    }
    return actions[idx];
}

static std::string prompt_for_address() {
    auto address_list = list_paired_addresses();
    address_list.push_back("new");
    int idx =
        console_selection_prompt("Select a server address", address_list, 0);
    if (idx < 0) {
        return "";
    } else if (address_list[idx] != "new") {
        return address_list[idx];
    }

    // Prompt users for a custom address using ImGui
    return ui::textInputDialog("Address of host to connect to", "", "");
}

static bool prompt_for_boolean(std::string prompt, bool default_val) {
    // use ImGui boolean dialog
    return ui::boolDialog(prompt.c_str(), default_val);
}

static int prompt_for_display_type(int default_val) {
    std::vector<std::string> options = {
        "top",
        "bottom",
        "dual screen (stretch)",
        "dual screen (mirror)",
        "dual screen (magnify)",
    };
    int idx = console_selection_prompt(
        "Which screen should be used to display the stream?", options,
        default_val);
    if (idx < 0) {
        return default_val;
    }
    return idx;
}

static int prompt_for_int(std::string initial_text) {
    int init = 0;
    try {
        init = std::stoi(initial_text);
    } catch(...) {}
    return ui::intDialog(initial_text.c_str(), init);
}

static void prompt_for_stream_settings(PCONFIGURATION config) {
    std::vector<std::string> setting_names = {
        "width",
        "height",
        "fps",
        "display_type",
        "motion_controls",
        "bitrate",
        "packetsize",
        "sops",
        "localaudio",
        "quitappafter",
        "viewonly",
        "hwdecode",
        "swapfacebuttons",
        "swaptriggersandshoulders",
        "usetriggersformouse",
        "debug",
    };
    int idx = 0;
    while (1) {
        std::string prompt = "Select a setting";
        if (config->stream.width % GSP_SCREEN_HEIGHT_TOP &&
            config->stream.width % GSP_SCREEN_HEIGHT_BOTTOM) {
            prompt += "\n\nWARNING: Using an unsupported width may "
                      "cause issues (3DS supports multiples of 400 or 320)\n";
        }
        if (config->stream.height % GSP_SCREEN_WIDTH) {
            prompt += "\n\nWARNING: Using an unsupported height may "
                      "cause issues (3DS supports multiples of 240)\n";
        }
        idx = console_selection_prompt(prompt, setting_names, idx);
        if (idx < 0) {
            break;
        }

        if ("width" == setting_names[idx]) {
            config->stream.width =
                prompt_for_int(std::to_string(config->stream.width));
        } else if ("height" == setting_names[idx]) {
            config->stream.height =
                prompt_for_int(std::to_string(config->stream.height));
        } else if ("display_type" == setting_names[idx]) {
            config->display_type =
                prompt_for_display_type(config->display_type);
        } else if ("motion_controls" == setting_names[idx]) {
            config->motion_controls = prompt_for_boolean(
                "Enable Motion Controls", config->motion_controls);
        } else if ("fps" == setting_names[idx]) {
            config->stream.fps =
                prompt_for_int(std::to_string(config->stream.fps));
        } else if ("bitrate" == setting_names[idx]) {
            config->stream.bitrate =
                prompt_for_int(std::to_string(config->stream.bitrate));
        } else if ("packetsize" == setting_names[idx]) {
            config->stream.packetSize =
                prompt_for_int(std::to_string(config->stream.packetSize));
        } else if ("sops" == setting_names[idx]) {
            config->sops = prompt_for_boolean(
                "Optimize Game settings for streaming", config->sops);
        } else if ("localaudio" == setting_names[idx]) {
            config->localaudio =
                prompt_for_boolean("Enable local audio", config->localaudio);
        } else if ("quitappafter" == setting_names[idx]) {
            config->quitappafter = prompt_for_boolean(
                "Quit app after streaming", config->quitappafter);
        } else if ("viewonly" == setting_names[idx]) {
            config->viewonly = prompt_for_boolean("Disable controller input",
                                                  config->viewonly);
        } else if ("hwdecode" == setting_names[idx]) {
            config->hwdecode = prompt_for_boolean("Use hardware video decoder",
                                                  config->hwdecode);
        } else if ("swapfacebuttons" == setting_names[idx]) {
            config->swap_face_buttons = prompt_for_boolean(
                "Swaps A/B and X/Y to match Xbox controller layout",
                config->swap_face_buttons);
        } else if ("swaptriggersandshoulders" == setting_names[idx]) {
            config->swap_triggers_and_shoulders = prompt_for_boolean(
                "Swaps L/ZL and R/ZR for a more natural feel",
                config->swap_triggers_and_shoulders);
        } else if ("usetriggersformouse" == setting_names[idx]) {
            config->use_triggers_for_mouse =
                prompt_for_boolean("Use ZL/ZR as left/right mouse buttons",
                                   config->use_triggers_for_mouse);
        } else if ("debug" == setting_names[idx]) {
            config->debug_level =
                prompt_for_boolean("Enable debug logs", config->debug_level);
        }
    }

    // Update the config file
    char *config_file_path = (char *)MOONLIGHT_3DS_PATH "/moonlight.conf";
    config_save(config_file_path, config);
}

static void init_3ds() {
    Result status = 0;
    /* ensure the config/keys directory exists on the SD card; silently ignore
       failures (e.g. running outside of a 3DS environment). */
    mkdir(MOONLIGHT_3DS_PATH, 0777);

    acInit();
    /* use the same initialization routine as the standalone demo app so the
       screen formats and memory regions match; this avoids discrepancies in
       where textures are allocated which seem to confuse Mandarine's cache
       emulation. */
    gfxInitDefault();
    // the demo didn't explicitly set double buffering, so leave defaults
    // (bottom is single-buffered, top is double-buffered).  we keep this
    // behaviour since it has proven to work with ImGui rendering.

    // initialize 2D/3D for ImGui painting
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    // target used for drawing ImGui output (bottom screen left)
    imgui_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    consoleInit(GFX_TOP, &topScreen);
    consoleSelect(&topScreen);
    atexit(n3ds_exit_handler);

    // setup ImGui context
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    /* 3DS screens are 320x240; imgui needs a valid display size or it
       will assert during NewFrame.  The stream resolution stored in the
       configuration is unrelated to the UI size. */
    io.DisplaySize = ImVec2(320.0f, 240.0f);
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.MouseDrawCursor = true;
    imgui_sw::bind_imgui_painting();
    // prepare pixel buffer and texture for rendering ImGui
    imgui_pixel_buffer.resize(320 * 240);
    imgui_tex = (C3D_Tex*)malloc(sizeof(C3D_Tex));
    static const Tex3DS_SubTexture subt3x = {512, 256, 0.0f, 1.0f, 1.0f, 0.0f};
    imgui_image = (C2D_Image){imgui_tex, &subt3x};
    C3D_TexInit(imgui_image.tex, 512, 256, GPU_RGBA8);
    C3D_TexSetFilter(imgui_image.tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(imgui_image.tex, GPU_REPEAT, GPU_REPEAT);
    // optionally set up style or SwOptions here

    osSetSpeedupEnable(true);
    aptSetSleepAllowed(false);
    aptInit();

    SOC_buffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    status = socInit(SOC_buffer, SOC_BUFFERSIZE);
    if (R_FAILED(status)) {
        printf("socInit: %08lX\n", status);
        exit(1);
    }

    status = ndmuInit();
    status |= NDMU_EnterExclusiveState(NDM_EXCLUSIVE_STATE_INFRASTRUCTURE);
    status |= NDMU_LockState();
    if (R_FAILED(status)) {
        printf("Warning: failed to enter exclusive NDM state: %08lX\n", status);
        wait_for_button();
    }
}

// helper: render the current ImGui frame onto the bottom screen
void render_imgui_frame() {
    // fill gray background
    std::fill(imgui_pixel_buffer.begin(), imgui_pixel_buffer.end(), 0x19191919u);
    paint_imgui(imgui_pixel_buffer.data(), 320, 240, imgui_sw_options);
    
    // copy to texture with 3DS tiling format
    for (int x = 0; x < 320; x++) {
        for (int y = 0; y < 240; y++) {
            u32 dstPos = ((((y >> 3) * (512 >> 3) + (x >> 3)) << 6)
                        + ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1)
                        | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3))) * 4;
            u32 srcPos = (y * 320 + x) * 4;
            memcpy(&((u8*)imgui_image.tex->data)[dstPos],
                   &((u8*)imgui_pixel_buffer.data())[srcPos], 4);
        }
    }
    
    /* ensure GPU sees our texture data changes; this is critical for proper rendering */
    printf("[moonlight] flushing GPU texture cache at %p\n", imgui_image.tex->data);
    GSPGPU_FlushDataCache(imgui_image.tex->data, 512 * 256 * 4);
    
    /* sync all GPU operations before proceeding */
    gfxFlushBuffers();
    gspWaitForVBlank();
    
    /* render the texture to the bottom screen using 2D/3D graphics pipeline */
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(imgui_target, C2D_Color32(32, 38, 100, 0xFF));
    C2D_SceneBegin(imgui_target);
    C2D_DrawImageAt(imgui_image, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);
    C3D_FrameEnd(0);
    
    /* ensure the frame is properly displayed */
    gfxFlushBuffers();
}

static int prompt_for_app_id(PSERVER_DATA server) {
    PAPP_LIST list = NULL;
    if (gs_applist(server, &list) != GS_OK) {
        printf("Can't get app list\n");
        return -1;
    }

    std::vector<std::string> app_names;
    std::vector<int> app_ids;
    while (list != NULL) {
        printf("%d. %s\n", list->id, list->name);
        app_names.push_back(std::string(list->name));
        app_ids.push_back(list->id);
        list = list->next;
    }

    int id_idx = console_selection_prompt("Select an app", app_names, 0);
    if (id_idx == -1) {
        return -1;
    }
    return app_ids[id_idx];
}

static inline void stream_loop(PCONFIGURATION config) {
    bool done = false;
    while (!done && aptMainLoop()) {
        done = n3ds_connection_closed;
        if (!config->viewonly) {
            done |= n3dsinput_handle_event();
        }
        hidWaitForAnyEvent(true, 0, 1000000000);
    }
}

static void stream(PSERVER_DATA server, PCONFIGURATION config, int appId) {
    int gamepad_mask = 1;
    int ret = gs_start_app(server, &config->stream, appId, config->sops,
                           config->localaudio, gamepad_mask);
    if (ret < 0) {
        if (ret == GS_NOT_SUPPORTED_4K)
            printf("Server doesn't support 4K\n");
        else if (ret == GS_NOT_SUPPORTED_MODE)
            printf("Server doesn't support %dx%d (%d fps) or remove "
                   "--nounsupported option\n",
                   config->stream.width, config->stream.height,
                   config->stream.fps);
        else if (ret == GS_NOT_SUPPORTED_SOPS_RESOLUTION)
            printf(
                "Optimal Playable Settings isn't supported for the resolution "
                "%dx%d, use supported resolution or disable 'sops' option\n",
                config->stream.width, config->stream.height);
        else if (ret == GS_ERROR)
            printf("Gamestream error: %s\n", gs_error);
        else
            printf("Errorcode starting app: %d\n", ret);
        wait_for_button();
        return;
    }

    n3ds_audio_disabled = config->localaudio;
    n3ds_connection_debug = config->debug_level;
    N3DS_RENDER_TYPE = static_cast<n3ds_render_type>(config->display_type);

    int drFlags = 0;
    if (config->fullscreen)
        drFlags |= DISPLAY_FULLSCREEN;

    switch (config->rotate) {
    case 0:
        break;
    case 90:
        drFlags |= DISPLAY_ROTATE_90;
        break;
    case 180:
        drFlags |= DISPLAY_ROTATE_180;
        break;
    case 270:
        drFlags |= DISPLAY_ROTATE_270;
        break;
    default:
        printf("Ignoring invalid rotation value: %d\n", config->rotate);
    }

    n3ds_connection_closed = false;
    n3ds_enable_motion = config->motion_controls;
    PDECODER_RENDERER_CALLBACKS video_callbacks =
        config->hwdecode ? &decoder_callbacks_n3ds_mvd
                         : &decoder_callbacks_n3ds;

    printf(
        "Loading...\nStream %dx%d, %dfps, %dkbps, sops=%d, localaudio=%d, quitappafter=%d,\
 viewonly=%d, rotate=%d, encryption=%x, hwdecode=%d, swapfacebuttons=%d, swaptriggersandshoulders=%d,\
 usetriggersformouse=%d, display_type=%d, motion_controls=%d, debug=%d\n",
        config->stream.width, config->stream.height, config->stream.fps,
        config->stream.bitrate, config->sops, config->localaudio,
        config->quitappafter, config->viewonly, config->rotate,
        config->stream.encryptionFlags, config->hwdecode,
        config->swap_face_buttons, config->swap_triggers_and_shoulders,
        config->use_triggers_for_mouse, config->display_type,
        config->motion_controls, config->debug_level);

    int status = LiStartConnection(&server->serverInfo, &config->stream,
                                   &n3ds_connection_callbacks, video_callbacks,
                                   &audio_callbacks_n3ds, NULL, drFlags,
                                   config->audio_device, 0);

    if (status != 0) {
        n3ds_connection_callbacks.connectionTerminated(status);
        printf("Connection failed with error: %d\n", status);
        wait_for_button();
        return;
    }

    printf("Connected!\n");
    stream_loop(config);

    LiStopConnection();

    if (config->quitappafter) {
        printf("Sending app quit request ...\n");
        gs_quit_app(server);
    }
}

static int init_server(CONFIGURATION *config, SERVER_DATA *server) {
    printf("Connecting to %s:%d...\n", config->address, config->port);
    gs_cleanup();
    int status = gs_init(server, config->address, config->port, config->key_dir,
                         config->debug_level, config->unsupported);
    if (status == GS_OUT_OF_MEMORY) {
        printf("Not enough memory\n");
        return 1;
    } else if (status == GS_ERROR) {
        printf("Gamestream error: %s\n", gs_error);
        return 1;
    } else if (status == GS_INVALID) {
        printf("Invalid data received from server: %s\n", gs_error);
        return 1;
    } else if (status == GS_UNSUPPORTED_VERSION) {
        printf("Unsupported version: %s\n", gs_error);
        return 1;
    } else if (status != GS_OK) {
        printf("Can't connect to server %s:%d\n", config->address,
               config->port);
        return 1;
    }

    if (config->debug_level > 0) {
        printf("GPU: %s, GFE: %s (%s, %s)\n", server->gpuType,
               server->serverInfo.serverInfoGfeVersion, server->gsVersion,
               server->serverInfo.serverInfoAppVersion);
        printf("Server codec flags: 0x%x\n",
               server->serverInfo.serverCodecModeSupport);
    }
    if (server->paired) {
        add_pair_address(config->address, config->port);
    } else {
        remove_pair_address(config->address, config->port);
    }
    return 0;
}

static void action_stream(CONFIGURATION *config, SERVER_DATA *server) {
    int appId = prompt_for_app_id(server);
    if (appId == -1) {
        return;
    }

    config->stream.supportedVideoFormats = VIDEO_FORMAT_H264;

    consoleClear();
    N3dsTouchType touch_type = DISABLED;
    if (config->debug_level) {
        consoleInit(GFX_BOTTOM, &bottomScreen);
        consoleSelect(&bottomScreen);
    } else if (config->display_type == RENDER_DUAL_SCREEN_STRETCH) {
        touch_type = DS_TOUCH;
    } else if (config->display_type == RENDER_DUAL_SCREEN_MAGNIFY) {
        touch_type = MAGNIFY_TOUCH;
    } else if (config->display_type == RENDER_BOTTOM ||
               config->display_type == RENDER_DUAL_SCREEN_MIRROR) {
        touch_type = ABSOLUTE_TOUCH;
    } else {
        touch_type = GAMEPAD;
    }

    if (config->viewonly) {
        if (config->debug_level > 0)
            printf("View-only mode enabled, no input will be sent "
                   "to the host computer\n");
    } else {
        n3dsinput_init(touch_type, config->swap_face_buttons,
                       config->swap_triggers_and_shoulders,
                       config->use_triggers_for_mouse);
    }
    stream(server, config, appId);

    if (!config->viewonly) {
        n3dsinput_cleanup();
    }
}

static void action_pair(CONFIGURATION *config, SERVER_DATA *server) {
    char pin[5];
    if (config->pin > 0 && config->pin <= 9999) {
        sprintf(pin, "%04d", config->pin);
    } else {
        sprintf(pin, "%d%d%d%d", (unsigned)random() % 10,
                (unsigned)random() % 10, (unsigned)random() % 10,
                (unsigned)random() % 10);
    }
    printf("Please enter the following PIN on the target PC:\n%s\n", pin);

    // Actually display the PIN on screen by swapping buffers
    gfxSwapBuffers();
    gfxFlushBuffers();
    gspWaitForVBlank();

    if (gs_pair(server, &pin[0]) != GS_OK) {
        printf("Failed to pair to server: %s\n", gs_error);
    } else {
        printf("Succesfully paired\n");
        // Display success message before breaking
        gfxSwapBuffers();
        gfxFlushBuffers();
        gspWaitForVBlank();
        add_pair_address(config->address, config->port);
    }
}

static void action_unpair(CONFIGURATION *config, SERVER_DATA *server) {
    if (gs_unpair(server) != GS_OK) {
        printf("Failed to unpair from server: %s\n", gs_error);
    } else {
        printf("Succesfully unpaired\n");
        remove_pair_address(config->address, config->port);
    }
}

static void action_quit_stream(SERVER_DATA *server) {
    printf("Sending app quit request ...\n");
    gs_quit_app(server);
    printf("Request completed\n");
}

int main_loop(int argc, char *argv[]) {
    init_3ds();

    CONFIGURATION config;
    config_parse(argc, argv, &config);

    while (aptMainLoop()) {
        auto address_string = prompt_for_address();
        if (address_string.empty()) {
            continue;
        }
        // Split address and port (if specified)
        uint32_t port_delim_pos = address_string.find(':');
        if (port_delim_pos != std::string::npos) {
            std::string port_string = address_string.substr(port_delim_pos + 1);
            address_string = address_string.substr(0, port_delim_pos);
            config.port = std::stoi(port_string);
        }
        config.address = (char *)address_string.c_str();

        SERVER_DATA server;
        if (init_server(&config, &server)) {
            wait_for_button();
            continue;
        }

        while (aptMainLoop()) {
            std::string action = prompt_for_action(&server);
            if (action.empty()) {
                break;
            }
            config.action = (char *)action.c_str();

            if (strcmp("stream", config.action) == 0) {
                action_stream(&config, &server);
                break;
            } else if (strcmp("pair", config.action) == 0) {
                action_pair(&config, &server);
                wait_for_button();
                break;
            } else if (strcmp("stream settings", config.action) == 0) {
                prompt_for_stream_settings(&config);
            } else if (strcmp("unpair", config.action) == 0) {
                action_unpair(&config, &server);
                wait_for_button();
                break;
            } else if (strcmp("quit stream", config.action) == 0) {
                action_quit_stream(&server);
                wait_for_button();
                break;
            } else {
                printf("%s is not a valid action\n", config.action);
                wait_for_button();
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    try {
        main_loop(argc, argv);
    } catch (const std::exception &ex) {
        printf("Moonlight crashed with the following error: %s\n", ex.what());
        return 1;
    } catch (const std::string &ex) {
        printf("Moonlight crashed with the following error message: %s\n",
               ex.c_str());
        return 1;
    } catch (...) {
        printf("Moonlight crashed with an unknown error\n");
        return 1;
    }
    return 0;
}

#endif
