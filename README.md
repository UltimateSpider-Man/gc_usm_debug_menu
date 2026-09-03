# Ultimate Spider-Man GameCube debug menu

This project ports the supplied Ultimate Spider-Man debug menu to the North
American GameCube retail executable. It builds a freestanding PowerPC payload,
injects it into a separate DOL, and binds the menu to verified GUTE52 retail
functions and data. The clean input DOL is never modified in place.

The port preserves the 14-row root menu and provides target-native actions for
the live GameCube engine: dynamic level, mission, region, entity, animation, and
AI browsers; save/load and screenshots; all retail Devopts and saved settings;
all 51 Debug Render values; the usable NGL debug flags; scripts and progression;
district controls; allocator views; and the camera/gameplay controls.

## Target and output

- Supported executable: unmodified North American GUTE52 `main.dol`
- Required input SHA-256:
  `3DD0AD5EDE2EF9DF27A1ABA1BE9BF9CD5DFD7A14CAA4BB8DA396F2574C89565C`
- Default output: `dist/sys/main.dol`
- Staged DOL text section: `0x80626000` (text slot 3)
- Final runtime payload: `0x81780000`
- UI render hook: `game::render_ui` call site at `0x8016E558`

The injector rejects the wrong revision, an occupied text slot, altered hook or
ArenaHi instructions, an out-of-range branch, an oversized staged image, or
overlapping runtime work areas. It then verifies the written section, entry
point, hook target, arena reservation, and unchanged source DOL.

## Menu lifecycle and controls

The menu can open only during active gameplay or the game's pause process
(states 6 and 7). Opening from gameplay calls the retail `game::pause`; closing
calls `game::unpause` only when this payload owns that pause. If the game was
already paused, the payload leaves the pre-existing pause intact. Actions that
change worlds, levels, heroes, saves, or screenshot processes close the menu and
release its pause before handing control to the retail engine.

`Game > Single Step` arms the retail one-frame field. Disable physics, select
Single Step, and close the menu to advance one frame.

- DualSense `PS`, or DualShock 4 `Share/Select` or `PS`: open/close when using
  the supplied Dolphin SDL profile
- GameCube `L + R` or `Z + START`: native open/close chords
- DS4/DualSense `L2 + R2` or `R1 + Options`: equivalent alternate chords
- D-pad Up/Down: move the selection
- D-pad Left/Right: change a value or activate the selected row
- Cross / GameCube A: enter, activate, or toggle
- Circle / GameCube B: go back; from the root, close
- L2/R2 / GameCube L/R: previous or next page in a long menu

The overlay uses the reference presentation: a compact translucent panel,
green title, yellow selection, gray rows, magenta scroll markers, bitmap-font
shadow, and `name: >` submenu notation.

### Dolphin controller profiles

`dolphin/GCPadNew.ini` targets
`SDL/0/DualSense Wireless Controller`; `dolphin/GCPadNew_DS4.ini` targets
`SDL/0/PS4 Controller`.

| PlayStation control | GameCube/debug action |
| --- | --- |
| Cross / Circle / Square / Triangle | A / B / X / Y |
| R1 / Options | Z / Start |
| L2 / R2 | L / R |
| Left stick / right stick | Main stick / C-stick |
| D-pad | D-pad navigation |
| PS; DS4 Share/Select | Open/close menu |
| Both motors | Rumble |

A GameCube PAD packet has no Guide button. The profiles map SDL `Guide` (and
DS4 `Back`) to the otherwise impossible combination of all four D-pad bits. The
payload recognizes that synthetic signature once per press and suppresses
navigation for the hold; ordinary single D-pad directions still work normally.
Dolphin performs this translation before the game's `PADRead` call.

`src/dinput_ds4_mapping.c` preserves the supplied 23-entry DirectInput DS4
layout as build metadata. Those host-side indices are not read as GameCube PAD
bits by the payload.

## Menu coverage

The root order is DVars, Warp, Game, Missions, Debug Render, District variants,
AI, Memory, Entity Variants, Entity Animations, Level Select, Script,
Progression, and Camera.

| Area | GameCube implementation |
| --- | --- |
| DVars | Edits the six verified live float variables with target-specific bounds and steps. |
| Warp | Lists currently loaded, visible regions. Selection unlocks the district when needed and calls the retail malor-point path with the region position. `--WARP TO POI--` resolves the live mission-arrow target through the IGO entity tracker and warps to it. |
| Game | Retail physics, single-step, slow motion/frame lock, Monkey mode, rumble, god mode, hero/FPS/district displays, controller-2 camera flag, district streaming toggle, save/load, screenshots, Devopts, and saved settings. `Show Districts` also draws a payload-native live district overlay. |
| Missions | Enumerates the live mission manager, global missions, and district groups; filters for the current hero and packs present on the disc; and invokes the verified `force_mission` transaction. Current-mission unload is native. |
| Debug Render | Exposes all 51 values through the retail getter, setter, minimum, and maximum functions. Its 15-row NGL submenu provides 13 working rows and keeps the two absent fields visibly marked unavailable. |
| District variants | Dynamically enumerates regions with multiple variants and applies the selected variant through `terrain::set_district_variant`. |
| AI | Walks the live high-priority AI-core list, filters invalid/dead actors, then enumerates each core block and inode parameter block. Float and integer parameters are editable; other target parameter types are displayed safely as typed read-only values. Block export writes typed, lossless raw values to the GameCube `OSReport` channel. |
| Memory | Shows live arena/slab data for all 44 size classes, including full/partial slab and allocated/free object counts. Aggregate allocated/free-byte checkpoints provide delta reporting. |
| Entity Variants | Enumerates live conglomerates with variant interfaces and applies their actual target-side variant hashes. |
| Entity Animations | Enumerates verified live actors, reads each actor's currently loaded resource-pack animation files, follows the NAL animation chains, and plays the chosen animation on the base layer. A missing controller is allocated through the actor ABI first. |
| Level Select | Reads the retail `level` descriptor resource, filters entries whose `/packs/gc/*.GCPACK` exists, and passes the selected pack to `game::_load_new_level`. Hero Select uses the ten target/reference hero resources and the mission manager's native hero-switch state machine. |
| Script | Shows Char Select and Options plus 223 recovered debug handlers grouped behind 22 resident-script probes. Only handlers whose executable, owner, instance, signature, and linked VM stream validate are dispatched. |
| Progression | Provides the three global progression actions and 23 Spider-Man/Venom checkpoints. It queues an exact resident `CITY_ARENA` handler only after verifying that the selected callback has the same live script owner. |
| Camera | Cycles Chase, User, and Scene Analyzer through the verified retail camera fields and geometry-manager calls. |

### Dynamic resources and transition safety

Live lists are rebuilt from the current world and guarded by MEM1 range, count,
list-link, type, and ownership checks. A selection re-resolves the underlying
object before changing it, so rows are not allowed to retain stale actor,
mission, region, animation, or variant pointers across an engine transition.
Long fixed-memory caches have explicit caps and surface truncation rather than
writing into adjacent MEM1 work areas.

Level Select obtains the `level` resource from the loaded partition-0 resource
directory and uses its real menu and pack names. Entity Animations obtains type
8 animation resources from the selected actor's own pack rather than using a
hard-coded Spider-Man/Venom name list. AI likewise uses the current AI-core and
inode vectors instead of reference-only placeholder rows.

The PC `--Export this block--` action writes a host file. Retail GUTE52's
`os_file::write` is a one-instruction no-op, so the GameCube port preserves the
row and sends the same live block to `OSReport`; enable OSReport logging in
Dolphin to capture it.

Hero Select contains these ten resources from the source menu:

`ultimate_spiderman`, `arachno_man_costume`, `usm_wrestling_costume`,
`usm_blacksuit_costume`, `peter_parker`, `peter_parker_costume`,
`peter_hooded`, `peter_hooded_costume`, `venom`, and `venom_spider`.

The hero switch validates the GCPACK and single-player world, waits for the
retail state machine, and restores the exact prior physics-disabled value.

### Save/load, screenshots, and renderer controls

`Saved Game Settings` edits all 97 fields in the live GUTE52 `game_settings`
block with the recovered bool/int/float layout and field-specific limits.
`Save Game`, `Load Game`, and `Attemp Auto Load` invoke the retail methods after
checking the active slot, loaded-save flags, load transition, and memory-card
operation state. The menu closes before the asynchronous operation starts.

Low-resolution screenshots set the retail NGL capture request. High-resolution
capture uses `HIRES_SCREENSHOT_X/Y`, accepts 640x480 through 8192x8192, creates
the game's 8/9/14 screenshot process, and lets the retail tiled-capture path do
the work. The NGL menu controls `ShowPerfInfo`, `ScreenShot`, `DisableQuads`,
`DisableVSync`, `DisableScratch`, `DebugPrints`, the three dump flags, the two
light/mesh sphere flags, and the two warning-disable flags in both synchronized
copies of the packed retail structure.

### Script and progression ABI

Script rows retain their full typed signatures. Zero-argument handlers use the
retail no-argument `add_thread` overload. A
`(debug_menu_entry)` handler receives the ABI-correct four-byte zero argument:
the GameCube retail `debug_menu_entry` script accessors are no-dereference stubs
that return defaults, so fabricating a PC/Xbox menu-entry object would be less
correct. Before dispatch, the payload validates the linked VM instruction
stream, including optional DSIZE words and operand widths. VM stack-relative
`SP/-4` operands are not misclassified as formal-parameter reads.

Progression first resolves the known `toggle_progression_on` callback to recover
the live `CITY_ARENA` owner, then requires the selected callback to have that
exact same owner before queuing it. If either callback is absent, the action is
blocked. There is intentionally no mission/hero fallback because merely starting
the mapped mission cannot recreate every story global changed by the retail
progression callback. The shared
multi-ID `debug_jump` registrations are not synthesized because their distinct
entry IDs are not recoverable from a single zero-valued fixed-payload argument.

## Genuine retail GameCube omissions

The port does not substitute unrelated behavior for code or fields that the
retail target does not contain:

- `NGL Debug > ShowPerfBar` and `RenderSingleNode` remain visible as unavailable;
  neither field exists in the verified packed 19-byte GameCube NGL debug state.
- Replay playback was linker-removed from retail GUTE52. The retained Replay
  definition is not reachable from the 14-row root, and its action does not
  pretend to start recording or playback.
- Per-allocation Script Memtrack/leak instrumentation and its class/entity
  histories were stripped or made inert. Memory checkpoint/delta totals and
  the full live slab views are implemented, but are not described as a
  replacement for allocation-event history.
- `Level Select > -- REBOOT --` is retained for menu parity, but the Xbox XBE
  reboot operation has no GameCube equivalent and therefore reports that fact.

## Build and inject the DOL

Requirements:

- Python 3.8 or newer
- `powerpc-eabi-gcc`, `objcopy`, and `nm` under either
  `C:\SysGCC\powerpc-eabi\bin` or `C:\devkitPro\devkitPPC\bin`
- `gc_address.txt` and `Ultimatexbox_debug.c` in the directory alongside the
  `gamecube_debug_menu` project directory
- The clean GUTE52 DOL at
  `Ultimate Spider-Man [GUTE52]\sys\main.dol`, or an explicit `-InputDol`

From the directory that contains `gamecube_debug_menu`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\gamecube_debug_menu\build.ps1
```

The build can also use `build/recovered_clean_main.dol` when it has the required
hash. It refuses an already patched or otherwise modified executable. To choose
the paths or Python executable explicitly:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\gamecube_debug_menu\build.ps1 `
  -InputDol C:\path\to\clean\main.dol `
  -OutputDol C:\path\to\patched\main.dol `
  -PythonPath C:\path\to\python.exe
```

For development smoke builds, `-AutoOpenForTest` opens the menu after a stable
single-player gameplay period. `-AutoOpenMenuForTest` accepts the menu names
listed by `build.ps1`, for example:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\gamecube_debug_menu\build.ps1 `
  -AutoOpenForTest -AutoOpenMenuForTest AI
```

Install the generated `dist/sys/main.dol` at the extracted image's
`sys/main.dol` path. Keep the clean executable separately.

For the compact GUTE52 test ISO used by this workspace, create a separate
patched ISO atomically with:

```powershell
python.exe .\gamecube_debug_menu\tools\inject_iso.py `
  .\gamecube_debug_menu\dist\USM_GUTE52_DebugMenu_test.iso `
  .\gamecube_debug_menu\dist\sys\main.dol `
  .\gamecube_debug_menu\dist\USM_GUTE52_DebugMenu_final.iso
```

The ISO injector validates the source layout, repairs and validates the FST,
checks every file extent and the embedded DOL hash, and never edits its source
ISO in place.

## Run with the isolated Dolphin profile

Connect the controller before launching Dolphin. From the directory containing
`gamecube_debug_menu`, launch the default DualSense profile with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\gamecube_debug_menu\run_dolphin_dualsense.ps1
```

For DualShock 4:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\gamecube_debug_menu\run_dolphin_dualsense.ps1 `
  -Controller DualShock4
```

The launcher copies only the supplied Dolphin configuration files to
`build/dolphin_dualsense_user` or `build/dolphin_ds4_user` and supplies that
directory with Dolphin's `-u` option. It does not modify the normal Dolphin
profile. Override discovery with `-DolphinExe`, `-IsoPath`, or `-UserDirectory`.
Close other Dolphin instances first so they cannot retain exclusive controller
access.

The convenience launchers `dist/START_DEBUG_MENU_DUALSENSE.bat` and
`dist/START_DEBUG_MENU_DS4.bat` use the same isolated profiles. If Dolphin uses
a different SDL device name, update the corresponding profile's `Device` line
or select the controller in Dolphin.

## Address provenance and injection design

`gc_address.txt` is a symbol/order guide from a nearby GameCube build, not a
literal address database for this retail DOL. `tools/generate_data.py` first
requires the exact clean-DOL hash, then checks target instruction words and
object layouts in GUTE52. It emits both the version-skewed `GC_LISTED_*` value
and the disassembly-verified retail `ADDR_*` value so they cannot be confused.
Devopt labels are extracted from the two verified GUTE52 tables; the Xbox
decompile supplies reference labels/layout evidence where appropriate, not
unchecked callable addresses.

Some retail functions use the fixed `0x7Fxxxxxx` virtual ARAM image through the
game's pager, including Debug Render, save/load, and the POI tracker path. These
are intentionally called only after the gameplay VM/pager is initialized. The
generator verifies their backing instructions in the clean DOL at virtual
address plus `0x01462000`; they are not guessed flat-RAM pointers.

The patched DOL appends a bootstrap and payload to empty text slot 3 at
`0x80626000` and changes the DOL entry to that bootstrap. At startup the
bootstrap copies the final-address-linked payload to `0x81780000`, flushes data
cache, invalidates instruction cache, and returns to the original entry at
`0x80003100`. The injector forces the retail ArenaHi fallback to make
`0x81780000` an exclusive upper bound, then redirects the verified UI call site
to the payload. Mission, animation, variant, actor, and auxiliary caches occupy
separate checked regions below the physical end of MEM1.

The renderer design and menu reference were adapted from
[MrMartinIden/usm-debug-menu](https://github.com/MrMartinIden/usm-debug-menu).
