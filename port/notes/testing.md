# Testing

## Rules
- Test runs: `SDL_AUDIODRIVER=dummy` (nothing through the speakers). Audio check: `SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=out.raw` (f32, 6 channels, 48 kHz).
- Saves go to the game folder's `Save/`: don't reach save screens in unattended runs against a real install.
- Run from a scratch cwd. Check `git status` afterwards for stray files.

## Hooks (`compat/linux_debug.cpp`)
| env | effect |
|---|---|
| `KISAK_SHOT_EVERY=N` | back buffer to `shot_<frame>.ppm` every N frames |
| `KISAK_SHOT_DIR` | where shots go (default cwd) |
| `KISAK_QUIT_AT=N` | exit after N frames |
| `KISAK_PAD_SCRIPT="button@first-last,..."` | virtual gamepad through the real SDL path. 0 A, 1 B, 2 X, 3 Y, 6 Start, 11 up, 12 down, 13 left, 14 right |
| `KISAK_NO_MSGBOX=1` | message boxes answered on stderr |
| `KISAK_GAME_DIR` | game root (`run.sh` sets it) |

## New story to New Jersey
```bash
SDL_AUDIODRIVER=dummy KISAK_NO_MSGBOX=1 KISAK_SHOT_DIR=shots KISAK_SHOT_EVERY=300 KISAK_QUIT_AT=4800 \
KISAK_PAD_SCRIPT="0@300-306,0@600-606,0@900-906,0@1200-1206,1@1500-1506,11@1650-1656,0@1800-1806,0@2200-2206,0@2500-2506,0@2800-2806,0@3100-3106" \
port/run.sh "<game folder>"
```
Timings are frames, so they drift if loading changes. Debug with `gdb -batch -ex run -ex bt`: asserts raise SIGTRAP, fatal outside a debugger.

## Quit from the main menu
`KISAK_QUIT_AT=1500 KISAK_PAD_SCRIPT="11@200-206,0@350-356,11@450-456,0@550-556"`: up to Quit, accept, up to Yes (No is the default), accept. The game exiting before frame 1500, with no `KISAK_QUIT_AT` line and exit code 0, is a pass. `kill -TERM` raises the same `SDL_QUIT` as closing the window.

## Reading script asserts
`Confused by '<name>'` can name the wrong symbol: unknown checksums print through a shared buffer, so every `%s` in the message shows the last one formatted. Retail scripts are in `Data/pre/qb.pre` and per-level packs such as `mainmenu.pre` (LZSS). They can call functions that `Scripts/` never mentions, so check the retail bytecode too.

## Status
Verified with retail PC data, scripted pad, `-O2`: menus, Create-A-Skater, cutscene, New Jersey skating and bailing, HUD, traffic, 4800 frames with no crash; audio signal present on disk; quit from the main menu; `SDL_QUIT`. By hand: menu quit and the window X.

Not verified: Windows build after these changes, hand play, real gamepad, speakers, save/load, fullscreen and resolution options, other levels, netplay, a clean-machine install. Menu skater draws as a silhouette: not compared with Windows.
