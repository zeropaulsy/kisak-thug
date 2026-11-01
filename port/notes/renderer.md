# Renderer: DXVK native

## Why
`Gfx/DX9` calls `IDirect3DDevice9` directly, through the `D3DDevice_*` macros in `Core/Defines.h`. hit-and-run-linux's GL/Vulkan renderers implement its `pddi` abstraction, so they don't apply. DXVK native is a real D3D9 on Vulkan: the renderer stays shared with Windows, so fixes in `Gfx/DX9` help both.

## Setup
- Links `port/third_party/dxvk-native/usr/lib32/libdxvk_d3d9.so`; headers from its `include/dxvk`.
- `DXVK_WSI_DRIVER=SDL2` is set in `main` before D3D starts (`setenv` without overwrite).
- Build rpath points at the absolute `third_party` path: a packaged build needs its own arrangement.
- DXVK's shader cache: `~/.cache/dxvk`. Expected log: `Unhandled render state D3DRS_DITHERENABLE` (harmless).

## D3DX stand-in (`compat/d3dx9shader.h`, `d3dx_compat.cpp`)
- No d3dx9 library on Linux. The compat header shadows the SDK one and defines `__D3DX9_H__` first: DXVK's `d3dx9math.h` includes `d3dx9.h`, whose other headers need GDI types.
- Math follows D3DX's documented formulas. `D3DXLoadSurfaceFromSurface` only does same-size, same-format, whole-surface copies, which is all its caller needs.
- A missing function shows up as a link error: add it here.

## Shader assembler (`D3DXAssembleShader`)
- `NX/render.cpp` generates `ps.1.1` text at runtime. Handles the subset it uses: `tex mov add sub mad mul dp3 dp4 lrp cnd cmp`, `_x2 _x4 _d2 _sat`, `+` co-issue, `1-`, `-`, `_bias _bx2 _x2`, masks, swizzles.
- A parse failure prints `ps assembler: cannot parse:` and returns an error.
- Verified by rendering only. Output has never been compared byte for byte with real D3DX.

## Test screenshots
`D3DDevice_Present` goes through `kisak_present` (`compat/linux_debug.cpp`) on Linux: see testing.md.
