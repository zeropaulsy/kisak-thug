# Build, toolchain, keeping Windows building

## Commands
```bash
cmake -S . -B build-linux -G Ninja
ninja -C build-linux -j12 > port/build-linux.log 2>&1; python3 port/errs.py
```
- Default build type RelWithDebInfo (`port/linux.cmake`). `_DEBUG` stays defined, as on Windows.
- 32-bit only (`-m32`, set before `project()`): the code casts pointers to 32-bit ints and on-disk data assumes them. 64-bit is a separate project.
- Deps: g++-multilib, `libsdl2-dev:i386`, `libvulkan1:i386` + i386 GPU driver. DXVK comes from `fetch_dxvk.sh` (version + sha256 pinned there).

## Windows
- No MSVC on the dev machine: Windows builds are not verified by a Linux build. Say so when reporting.
- Linux-only behaviour goes behind `#ifdef __linux__`. Only standard C++ that both compilers accept goes unguarded (`this->`, forward declarations, `template<>`).
- No two paths may differ only by case (a Windows checkout loses one). Check: `git ls-files | tr A-Z a-z | sort | uniq -d`.
- `port/compat/` is not on the Windows include path: including a compat-only header needs a guard (`win32_net.h` in `Gel/Net/net.h`).

## Allocator isolation (same problem as hit-and-run-linux)
- The game replaces global new/delete. On ELF that leaks into every shared library: Mesa's LLVM crashed at load with `free(): invalid size`. Fix: `-static-libstdc++ -static-libgcc -Wl,--exclude-libs,ALL` + version script `compat/kisak.map`. Check: `nm -D --defined-only build-linux/KisakThug | grep _Zn` must be empty.
- `Core/Defines.h` defines them `inline`, which GCC drops when unused, so libstdc++ kept its own delete: game-heap strings were freed into glibc. On Linux they are out of line in `Core/Support/class.cpp`, including sized and nothrow forms.

## GCC vs MSVC
- Forced flags: `-fpermissive -fms-extensions -fno-strict-aliasing -fno-delete-null-pointer-checks -w`.
- GCC 15: a lookup error in a template is only a `-Wtemplate-body` warning under `-fpermissive`, then `instantiating erroneous template` at instantiation. `-w` hides the warning naming the real cause: drop `-w` from `linux.cmake` to see it. `-Wno-template-body` does not help. `add_this.py` fixes the member-call cases; types and enums need `Base<_T>::` by hand.
- A name first declared in a `friend` is not visible to GCC: add a forward declaration. A friend can't carry a default argument: put it on the forward declaration (`GetNextScript`).
- GCC doesn't mangle return types: `int GetNumSoundtracks()` (`Music/Win32/p_soundtrack.cpp`) clashes with `size_t GetNumSoundtracks()` (`Music/SDL/p_music.cpp`), so the first is guarded out. MSVC treats them as two functions.
- `Core/Xbox_SSE.h`, `__asm` prefetch and `getCycles` have intrinsic/builtin versions.
- `-fpermissive` can hide real conflicts (extern then static): check linkage by hand when adding declarations.

## Editing sources
- Mixed line endings, mostly CRLF, and some non-UTF-8 bytes: edit with Python `newline='', encoding='latin-1'`.
- grep calls some sources binary and prints no matches (`NX/nx_init.cpp`, `MemCard/Win32/p_McMan.cpp`): use `grep -a`.
- Re-run `fix_include_case.py` after merging code that adds includes. It skips NGC/NGPS sources, which aren't built.
