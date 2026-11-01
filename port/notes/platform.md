# Platform: Win32 shim, window, input, audio, files, network

## Win32 shim (`compat/win32_compat.*`)
- `compat/windows.h` wraps DXVK's windows.h via `#include_next` and adds the shim, so anything including d3d9.h gets it. C++ only.
- Uses malloc only: it runs during memory manager setup.
- Null handles fail, not crash: the game waits on a null `g_asyncRenderMutex` (Windows returns WAIT_FAILED).
- Mutexes are recursive, like Win32. `timeSetEvent` runs each timer on its own thread.
- No registry: every key is absent, so callers use defaults.
- `MessageBoxA` shows an SDL box; `KISAK_NO_MSGBOX=1` answers it on stderr instead.

## Files
- `resolve_path`: either slash, each component matched ignoring case. A missing component keeps its spelling (files about to be created).
- Game root = `GetModuleFileNameA`: `$KISAK_GAME_DIR/KisakThug` if set, else the exe path. `Data/` and `Save/` hang off it.
- Overlapped reads complete immediately (`pread`); callers poll `HasOverlappedIoCompleted`.
- `NX/render.cpp Swog` logs to a hard-coded `F:\...`, which on Linux is an odd file name in the cwd: disabled there. Watch for other hard-coded drive paths.

## Window (`Sys/Win32/WinMain.cpp`, `#ifdef __linux__` branches)
- Process entry is `main` there; `Sk/Main.cpp`'s main is renamed `KisakGameMain` on Linux.
- `hWnd` holds the `SDL_Window*`, created with `SDL_WINDOW_VULKAN`.
- `p_siodev.cpp` drains the SDL queue, so `linux_window_event_watch` (an event watch, not a poll) handles quit and focus. Unfocused, `Mainloop.cpp` waits by design.

## Input (`Sys/SIO/Win32/p_siodev.cpp`)
Shared SDL_GameController code; Xinput.h guarded out. No keyboard on either OS yet.

## Audio (`Gel/SoundFX/SDL/Mixer.cpp`)
- Mixers write 5 channels (FL FR LFE BL BR). PulseAudio refuses to open 5 (2, 3, 4 and 6 work), so the device opens as 5.1 and `AudioCallback` widens each block with the centre silent.
- No device is not fatal on Linux: prints `No audio:` and carries on.
- The front mix reaches LFE at the same level: not yet heard on speakers.

## Network
- `compat/win32_net.h`: Winsock names on POSIX sockets. WinSock startup guarded out of `Gel/Net/net.cpp`.
- GameSpy: `_LINUX`, `common/win32/` excluded. Compiles and links only; never run (netplay is a TODO on Windows too).

## Quitting
Everything that quits sets `gbQuit` (`QuitGame` script function, window close). `MainLoop` stops on it and `Sk/Main.cpp` then calls `_Exit(0)`, skipping the closedown code after the main loop: that code has never run and crashes (first in `CViewer::v_stop_cb`, null `mp_server`).
