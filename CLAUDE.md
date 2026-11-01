# CLAUDE.md: kisak-thug Linux port

32-bit Linux build of the PC port: DXVK native (D3D9 on Vulkan) + SDL2. Index and layout: `port/notes/README.md`. Read only the note for the subsystem you touch, and update it when a lesson is learnt.

## Commands
```bash
cmake -S . -B build-linux -G Ninja
ninja -C build-linux -j12 > port/build-linux.log 2>&1; python3 port/errs.py
SDL_AUDIODRIVER=dummy port/run.sh "<folder holding Data/>"
```
Unattended runs and test hooks: `port/notes/testing.md`.

## Rules that bite
- Windows must keep building, and there is no MSVC here: Linux behaviour goes behind `#ifdef __linux__`, only standard C++ goes unguarded, and always say Windows was not compiled.
- No two paths may differ only by case.
- Global new/delete must stay out of shared libraries: `nm -D --defined-only build-linux/KisakThug | grep _Zn` must be empty.
- The shim runs during memory manager setup: `port/compat/win32_compat.cpp` uses malloc only, never C++ new.
- Edit sources with Python `newline='', encoding='latin-1'` (mixed CRLF, non-UTF-8 bytes). Use `grep -a`: some sources read as binary and silently match nothing.
- Test runs use `SDL_AUDIODRIVER=dummy`, run from a scratch cwd, and stay off save screens against a real install.
