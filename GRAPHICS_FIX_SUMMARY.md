# ImGui Graphics Rendering Fixes - Summary

## Problem Statement
Vertical green/teal horizontal stripes appearing on bottom screen when rendering ImGui UI in Moonlight N3DS client, particularly in Mandarine emulator.

## Root Causes Identified
1. **GPU cache flush ignored by Mandarine emulator**: The `GSPGPU_FlushDataCache()` call was being ignored, causing GPU texture cache coherency issues
2. **CPU framebuffer fallback timing issue**: Writing directly to framebuffer AFTER `C3D_FrameEnd()` causes buffer synchronization problems due to double-buffering
3. **Incorrect color format**: Initial assumption was RGB565, but 3DS bottom screen uses BGR565 (Blue in LSBs, Red in MSBs)
4. **Type mismatch delays**: Using u32 pointer access for u16 RGB565 framebuffer caused byte-swapping artifacts

## Fixes Applied

### 1. Removed CPU Framebuffer Fallback (Latest Fix)
**File**: `src/n3ds_main.cpp`
**Method**: `render_imgui_frame()`

Removed the entire CPU-based framebuffer copy fallback that was added after GPU rendering. This path had timing issues:
- Executed after `C3D_FrameEnd(0)` which swaps buffers
- Could write to wrong buffer or interfere with GPU rendering
- Calls to `gfxFlushBuffers()`/`gspWaitForVBlank()` after `C3D_FrameEnd()` were unnecessary

**Result**: Now matches the working sample app approach (GPU rendering only)

### 2. GPU Cache Flush Implementation
**File**: `src/n3ds_main.cpp`
**Function**: `render_imgui_frame()`

Added explicit GPU data cache flush after copying ImGui pixel buffer to GPU texture:
```cpp
GSPGPU_FlushDataCache(imgui_pixel_buffer.tex->data, 512 * 256 * 4);
```

This ensures CPU writes to GPU texture memory are visible to the GPU before rendering.

### 3. Proper ABGR32 to BGR565 Conversion (Intermediate Fix)
**File**: `src/n3ds_main.cpp`

Corrected color format conversion from ImGui's ABGR32 output to 3DS BGR565 framebuffer format:
- ImGui produces ABGR32: bits 0-7=R, 8-15=G, 16-23=B, 24-31=A
- 3DS bottom screen uses BGR565: bits 0-4=B, 5-10=G, 11-15=R

Original incorrect code (RGB565):
```cpp
u16 rgb565 = ((r >> 3) & 0x1F) |          // Red at bits 0-4
             (((g >> 2) & 0x3F) << 5) |   // Green at bits 5-10
             (((b >> 3) & 0x1F) << 11);   // Blue at bits 11-15
```

Corrected code (BGR565):
```cpp
u16 bgr565 = (((b >> 3) & 0x1F) |         // Blue at bits 0-4
              (((g >> 2) & 0x3F) << 5) |  // Green at bits 5-10
              (((r >> 3) & 0x1F) << 11)); // Red at bits 11-15
```

### 4. Type Corrections
**File**: `src/n3ds_main.cpp`
**Change**: Switched from `int` to `u16` for framebuffer dimension variables

The `gfxGetFramebuffer()` function requires `u16*` for dimension output parameters, not `int*`.

### 5. Config/Session Initialization Fixes
**File**: `src/n3ds_main.cpp`
**Function**: `init_3ds()` and `config_menu()`

- Added `mkdir(MOONLIGHT_3DS_PATH, 0777)` to create `/3ds/moonlight` directory on startup
- ImGui display size initialization: `io.DisplaySize = ImVec2(320.0f, 240.0f)`
- Used `gfxInitDefault()` instead of `gfxInit(GSP_RGB565_OES, ...)` for consistency with sample app

## Testing Recommendations

1. **Real 3DS Hardware**: Test streaming UI overlay on actual hardware
   - Expected: Clean ImGui rendering without stripes
   - Both screens should display correctly
   
2. **Mandarine Emulator**: Test with GPU-only rendering path
   - With fixed cache flush: Should see improvement if emulator properly emulates GPU
   - If stripes still appear: Indicates emulator's GPU cache implementation is still broken
   - Fallback may need to be Mandarine-specific rather than universal

3. **Citra Emulator**: Test for compatibility
   - Expected: Same clean rendering as real hardware
   - Citra's GPU emulation is more complete than Mandarine

## Build Status
- **Status**: ✅ Successful
- **Binary**: `moonlight.3dsx` (7,090,828 bytes)
- **Last Built**: 2/21/2026 10:59:19 PM
- **Source**: Latest with GPU-only rendering path

## Key Files Modified
1. `src/n3ds_main.cpp` - Main rendering and initialization logic
2. `Makefile` - Added `-Wno-class-memaccess` compiler flag for ImGui compatibility

## Remaining Known Issues
- **Mandarine emulator GPU cache**: Emulator may not properly emulate GPU data cache, causing stripes even with `GSPGPU_FlushDataCache()`. This is a known limitation of the Mandarine emulator implementation.
- **Workaround options for Mandarine**:
  1. Direct framebuffer writing (previously attempted, had timing issues)
  2. Alternative rendering pipeline using CPU-only paths
  3. Accept Mandarine limitations and focus on real hardware + Citra

## References
- Sample app: `imgui-3ds/source/main.cpp` (renders cleanly using GPU-only path)
- Core rendering: citro2d/citro3d 3DS graphics libraries
- ImGui software renderer: `imgui-3ds/imgui/imgui_sw.cpp`
