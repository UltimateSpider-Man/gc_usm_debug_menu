# Ultimate Spider-Man GameCube debug menu (GUTE52)

This project injects a freestanding PowerPC debug-menu payload into the North
American GameCube retail executable.  It recreates the Xbox menu hierarchy and
renderer, adds the `Char Select` and `Options` pages from the public PC
recreation, adds a live GameCube mission browser and hero selector, and writes
a separate patched DOL.  The source DOL is never edited.

## Target and output

- Supported input: an unmodified North American GUTE52 `main.dol`
- Required input SHA-256:
  `3DD0AD5EDE2EF9DF27A1ABA1BE9BF9CD5DFD7A14CAA4BB8DA396F2574C89565C`
- Generated output: `dist/sys/main.dol`
- Staging address: `0x80626000` (DOL text slot 3)
- Runtime payload address: `0x81780000` (reserved below the MEM1 limit)
- Render hook: `game::render_ui` call site at `0x8016E558`

The injector checks the hash, hook bytes, empty DOL section slot, branch range,
payload size, and post-write section contents before accepting the output.

## Controls

- DualSense `PS` / SDL `Guide`: open or close the menu with one button
- GameCube `Z + START` or DualSense `R1 + Options`: alternate open/close chord
- D-pad Up/Down: move selection
- D-pad Left/Right: change a value
- `A`: enter a submenu, activate an action, or toggle a value
- `B`: go back; on the root page it closes the menu
- `L` / `R`: page through long menus

Opening the menu pauses physics.  The root `Pause` row explicitly shows
`Paused` or `Unpaused` and toggles either state.  Closing the menu restores the
prior state unless pause/physics was explicitly changed in the menu.  `Single
Step` advances one game frame while the menu remains open.

The renderer follows the supplied reference: a compact translucent black panel,
yellow title, green selected row, gray normal rows, bitmap-font shadow, and
`name: >` submenu notation.

### DualSense mapping

The supplied `dolphin/GCPadNew.ini` uses Dolphin's SDL device named
`SDL/0/DualSense Wireless Controller`:

| DualSense | GameCube / debug action |
| --- | --- |
| Cross, Circle, Square, Triangle | A, B, X, Y |
| R1 | Z |
| Options | Start |
| L2, R2 | L, R |
| Left stick | Main stick |
| Right stick | C-stick |
| D-pad | D-pad |
| PS | Open/close debug menu |
| Both motors | Rumble |

GameCube PAD packets have no native PS/Guide bit.  The Dolphin profile therefore
maps SDL `Guide` to the otherwise impossible combination of all four D-pad bits;
the payload recognizes that combination as `PAD_PS`, toggles once on the press
edge, and suppresses D-pad navigation for the entire hold.  Ordinary individual
D-pad directions still navigate normally.

Connect the controller before starting Dolphin, then launch the patched ISO with
the isolated project profile:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\gamecube_debug_menu\run_dolphin_dualsense.ps1
```

The launcher copies only the three supplied Dolphin configuration files into
`build/dolphin_dualsense_user` and passes that directory with Dolphin's `-u`
option.  It does not change the normal Dolphin profile.  Custom paths can be
passed with `-DolphinExe`, `-IsoPath`, and `-UserDirectory`.

Opening the ISO directly from Dolphin uses Dolphin's normal controller profile,
not the profile stored in this project.  For a guaranteed PS-button mapping,
double-click `dist/START_DEBUG_MENU_DUALSENSE.bat` or use the PowerShell command
above.  The isolated profile explicitly enables SDL HIDAPI/PS5 input and maps
`Guide` to the payload's reserved `PAD_PS` signature.  Close every other Dolphin
window first; the launcher deliberately stops with an explanation if another
Dolphin process could retain exclusive access to the controller.

## Menu coverage

The payload contains the Xbox root pages and child pages for DVars, Warp, Game,
Missions, Debug Render, NGL Debug, District variants, Replay, AI, Memory,
Script Memtrack, all four Slab lists, Entity Variants, Entity Animations, Level
Select, Hero Select, Script, and Progression.  It also contains all 225 Xbox
Devopts rows, all 97 Saved Game Settings rows, all 51 Debug Render rows, the 21
NGL Debug rows, Camera, Char Select, and Options.

Rendering, navigation, paging, physics pause/single-step, the verified retail
developer flags, and the retail NGL screenshot request are connected to real
GameCube functions.  Menus whose Xbox handlers depend on removed code or
runtime REL objects remain visible and report a safe explanatory message rather
than calling an unverified address.  Their editable values are session-local.

`Missions` is populated from the live retail mission manager.  It lists global
missions directly, creates district submenus, filters rows for the current hero
and packs present on the disc, and displays `mission (script_data)` or
`mission (instance)`.  Selecting a row calls the exact GUTE52 `force_mission`
transaction and closes the menu.

Both root `Char Select` and `Level Select > Hero Select` expose the same ten
canonical hero resources.  Selection validates the GCPACK and queues the
retail mission-manager hero-switch state machine; input stays locked while the
engine removes and re-adds the player, then the prior pause state is restored.

## Build and inject

From PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\gamecube_debug_menu\build.ps1
```

The build selects the extracted DOL when it is clean and otherwise falls back
to `build/recovered_clean_main.dol` when that verified backup is present.  It
will not accept or double-patch an already modified executable.

The build requires Python and a `powerpc-eabi` GCC toolchain at either
`C:\SysGCC\powerpc-eabi\bin` or `C:\devkitPro\devkitPPC\bin`.  To choose other
input or output paths:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\gamecube_debug_menu\build.ps1 -InputDol C:\path\to\main.dol -OutputDol C:\path\to\patched_main.dol
```

Use the resulting DOL in the extracted `sys/main.dol` position when rebuilding
or launching the game image.  Keep the original executable as a backup.

For the compact GUTE52 test ISO used by this project, inject the patched DOL
atomically with:

```powershell
python.exe .\gamecube_debug_menu\tools\inject_iso.py `
  .\gamecube_debug_menu\dist\USM_GUTE52_DebugMenu_test.iso `
  .\gamecube_debug_menu\dist\sys\main.dol `
  .\gamecube_debug_menu\dist\USM_GUTE52_DebugMenu_final.iso
```

The ISO injector repairs and validates the FST, verifies every file extent and
the embedded DOL hash, and never modifies its source ISO in place.

## Address provenance

`gc_address.txt` comes from a nearby GameCube build and has version-skewed
addresses.  `tools/generate_data.py` records those symbol addresses but emits
retail addresses only after checking the exact GUTE52 DOL.  The retail values
were confirmed against the DOL's PowerPC instructions; `0x7Fxxxxxx` REL analysis
addresses are intentionally never called as fixed RAM addresses.

The renderer design and extra pages were adapted from
[MrMartinIden/usm-debug-menu](https://github.com/MrMartinIden/usm-debug-menu).
