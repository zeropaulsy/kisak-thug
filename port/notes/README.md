# Linux port notes
Read only the file for the subsystem you touch. Keep notes terse: rules, symptoms, file/symbol names. No history (git has it). Update the relevant file when a lesson is learnt.

| file | covers |
|---|---|
| `build.md` | tools, CMake split, allocator isolation, GCC/MSVC differences, keeping Windows building |
| `platform.md` | Win32 shim, window, input, audio, files, network |
| `renderer.md` | DXVK native, D3DX stand-in, shader assembler |
| `testing.md` | unattended run hooks, verified and unverified areas |

## Platform model
Linux is a PC platform: `__PLAT_WN32__` stays defined and the PC code is shared. OS differences are marked `#ifdef __linux__`. `WIN32`/`_WIN32` are not defined; GameSpy gets `_LINUX`.

## Layout
```
port/linux.cmake        Linux build rules (CMakeLists.txt includes it and returns; Windows never reaches it)
port/compat/            forced msvc_compat.h, Win32 shim, D3DX stand-in, test hooks, linker map
port/run.sh             run.sh "<folder holding Data/>" [game args]
port/fetch_dxvk.sh      pinned DXVK download, into git-ignored port/third_party/
port/fix_include_case.py  include paths to on-disk case
port/errs.py            first error per failed source, from port/build-linux.log
port/add_this.py        inserts this-> from GCC diagnostics
build-linux/            build dir (ignored)
```
