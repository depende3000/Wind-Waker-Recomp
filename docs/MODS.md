# Mods

BlueWake has a **Mods** section in the in-game ⋯ menu with four mods. Each takes effect the next time
BlueWake starts, and none of them changes your saves.

| Mod | What it does | What you need |
| --- | --- | --- |
| Widescreen 16:9 | The community 16:9 code (Dolphin's GZLE01 Gecko code): a wider field of view with the HUD placed for 16:9, letterboxed on a 4:3 iPad | Nothing; it is built in |
| Widescreen 16:10 | The same code with its aspect-dependent values recomputed for 16:10 (Mac displays, most iPads' shape is closer to it); one widescreen mod at a time | Nothing; it is built in |
| HD Texture Pack | Replaces the game's textures with a Dolphin-format pack, such as [Hypatia's HD pack](https://forums.dolphin-emu.org/Thread-hypatia-s-tloz-the-wind-waker-hd-pack-v2-0001a) | The pack's `tex1_…` images in `Documents/BlueWake/Load/Textures/GZLE01` |
| Better Wind Waker | [Better Wind Waker](https://github.com/WideBoner/betterww)'s quality-of-life changes from Wind Waker HD, each its own setting: Swift or Brisk Sail, instant text, faster rolling, grappling, block pushing, climbing and crawling, Tingle Chests without the Tuner, an unrestricted boat, no song replays, turning while swinging, a faster Ballad of Gales, skipping the opening movie, an inverted camera, a revealed sea chart | Nothing; it is built in |

On the iPad, `Documents/BlueWake` is **On My iPad › BlueWake › BlueWake** in the Files app.

## Installing

**Widescreen:** turn on *Widescreen 16:9* or *Widescreen 16:10* (turning one on turns the other off) and
restart BlueWake. The 16:10 code is generated from the 16:9 one by `scripts/mods/widescreen_aspect.py`:
each value it changes (the camera aspect, which also widens the game's view culling, the 2D bounds, HUD
and map positions, a few instruction immediates) is interpolated between the game's 4:3 value and the
16:9 value, and the three HUD constants it loads from the game's pool take the nearest pool value. On the
Mac host, `BLUEWAKE_ASPECT=16:10` or `16:9` selects the mod and the frame buffer shape together. At the default 3× render resolution 16:9
renders 2560×1440 and 16:10 2304×1440 (the 4:3 picture's height, widened), so the game's letterbox bars line up with the scene;
if an older device struggles, 2× renders 1707×960.

**HD textures:** download a Dolphin-format pack for GZLE01. On the iPad, Hypatia's *Android-Lite* build
(3× resolution, about 530 MB of PNG files) fits comfortably; the full-size PC builds need several GB of
memory. Copy the folder of `tex1_…` images (in Hypatia's pack, the `GZL` folder) into
`Load/Textures/GZLE01`, turn on *HD Texture Pack* and restart. Subfolders are searched, `_mipN`
sidecar mipmaps are used, and PNG and DDS files both work. The menu shows how many textures it found.

**Better Wind Waker:** turn on *Better Wind Waker*, pick its settings in **⋯ › Mods › Better Wind Waker
Settings** and restart. Each of Better Wind Waker's settings is a switch of its own, on or off by Better
Wind Waker's defaults until you change it: instant text (and holding B to advance), Swift Sail (or Brisk
Sail, which brakes harder), faster rolling, grappling, block pushing and climbing, Tingle Chests without
the Tingle Tuner, turning while swinging, no song replays and a faster Ballad of Gales start on;
an unrestricted boat, skipping the opening movie, an inverted camera and revealing the sea chart start
off. No patched disc is needed. Swift Sail's new sail texture and icons are Wind Waker HD's art and are
not included (an HD texture pack can supply them); its name stays "Sail". On the Mac host,
`BLUEWAKE_MODS=betterww` turns the settings on and `BLUEWAKE_OPTIONS=name,-name,...` changes them
(`none` first turns them all off); `scripts/mac/run_host.sh` takes `BWW=1` and `OPTIONS=`.

## Better Wind Waker's settings as game options

Better Wind Waker patches the disc: assembly patches to the executable and modules, added code, changed
values and changed message data. BlueWake does the same things at runtime, from
`mods/betterww/options.txt`:

- **Option sites.** The instructions a setting changes (a `nop`, a branch made unconditional, a
  different constant) are listed with the setting's switch. DolRecomp's `--option-sites` translates each
  site both ways and picks one by `dolrecomp_option_flags[switch]` at run time, so the game is translated
  once for every combination of settings. Only the 15 chunks that hold a site differ from the base
  translation; they are the `betterww` mod's variants (3 more combine with a widescreen mod).
- **Native code.** What Better Wind Waker's added assembly did is C in `runtime/host/src/game_options.c`,
  called from hook sites: turning with the stick while swinging on a rope, negating the camera's
  C-stick axis, turning the wind to blow from behind King of Red Lions (through the game's own
  `dKyw_tact_wind_set`, entered from the hook) and Swift Sail's braking.
- **Values.** The constants a setting changes (rolling speed, block pushing frames, the boat's speed) are
  written at boot while it is on; a module's go to its data at the linked address, where it stays for the
  session.
- **Message data.** For instant text the host patches the loaded messages as Better Wind Waker patches the
  disc's: every message draws its whole box at once and its timed waits wait no time.

Better Wind Waker's other changes are left out: its randomizer fixes, the custom player model and colours,
random enemy colours and the title and memory-card art.

## How code mods work in a static recompilation

BlueWake runs the game from native code translated ahead of time, so a mod that rewrites game code in
memory (a Gecko code, or a patcher's assembly changes) has no effect at runtime: the instructions it
writes are never executed. Code mods are therefore built into the app:

1. The mod is applied to the game's executable on the Mac (`scripts/mods/gecko_apply.py` for a Gecko
   code), or for game options the translator is given the option sites (above).
2. The patched (or option-sited) `main.dol` and RELs are translated by the same DolRecomp as the base
   game and merged by `scripts/generate_composite.py` into a composite source tree of their own.
3. `scripts/mods/build_mod_variants.py` compares that tree with the base tree. Every translated chunk
   that differs is added to the base composite under a new name (`chunks_mod_<name>/`), code ranges
   only the mod has become extra chunks, and the bytes the mod
   changes in the executable's data and in the relocated REL data become writes. A Gecko code's data
   writes are re-applied at every retrace, as Dolphin does. When two mods change the same chunk, a
   translation of the game patched by both supplies the variant used when both are enabled.
4. At boot the host enables the mods named in `BLUEWAKE_MODS` (the menu sets it):
   `bluewake_composite_apply_mods` points the chunk table at the variants before the first dispatch,
   and the writes go into guest RAM. Chunks never call each other directly, so a variant replaces its
   chunk cleanly, and the base game is untouched when a mod is off.

The chunks include `generated.h`, which is unchanged, and only `module_export.c` sees the dispatcher
with the writable chunk table, so adding mods compiles the new variant chunks only (53 today), not the
748 base chunks.

HD textures need no build step. Aurora's Dolphin-compatible texture replacement (the
`tex1_WxH[_m]_<XXH64>[_<palette XXH64>]_<format>` names, with palettes hashed over the entries the
texture uses) is connected to GXRuntime's texture decode: the first time a texture's bytes are seen,
the pack is consulted before decoding, and the replacement is cached under the same key as a decoded
texture would be. Replacements are decoded on a background thread the first time they are used (the original texture
is drawn meanwhile), so a new area shows its HD textures a moment later instead of stalling a frame.

## Widescreen in the native port

The native port (`native/`, the decompilation built for the Mac and the Switch) has no executable to
patch: the 16:9 code is done in C under `#if TARGET_PC`, as a run option, and the GameCube build of the
decompilation is unchanged. `TWW_ASPECT=4:3` (the default on the Mac), `16:9` (the default on the
Switch, set by `switch/native/source/tww_switch.cpp`) or `16:10` selects it
(`native/include/pc/pc_aspect.h`). A wider aspect makes the window that wide at 720 points (1280x720,
1152x720) and Aurora presents the 640x480 picture stretched to the aspect, letterboxed or pillarboxed in
a window of another shape (`AuroraSetFitAspect`, Aurora patch 0006): the game draws an anamorphic
picture into its EFB, as on a widescreen TV with the Gecko code. Each value the code changes is
interpolated with `t = (A - 4/3) / (16/9 - 4/3)`, as `widescreen_aspect.py` does (t = 0.6 at 16:10);
the native port uses the exact interpolated value where the Gecko code can only load the nearest
pool constant (the HUD shift is 68.4 at 16:10, not 68).

### What each line of the 16:9 code patches

Decoded against the decompilation's symbols (`config/GZLE01/symbols.txt`) and `main.dol`. The code
writes five blocks of new code into unused memory (0x800037E0 and 0x80004038-0x800040F7) and
redirects game code to them; the "native" column names the C that does the same. "114" is the
pool constant 0x803F92CC (114.0), the distance the HUD moves toward each edge at 16:9; "-123"/"767"
are the new 2D screen left and right.

| Address | Symbol (source) | 4:3 (game) | 16:9 (code) | Meaning | Native |
|---|---|---|---|---|---|
| 0x803FA998 | `@17778` .sdata2, `preparation(camera_process_class*)` (d_camera.cpp) | 4/3 | 16/9 (0x3FE38E39) | Camera aspect: the projection and `mDoLib_clipper`'s frustum, so the view and the view culling both widen | `pc_aspect_ratio()` |
| 0x803F7D68 | `@5162` .sdata2: `setUpRectangle`, `mDoGph_Painter` (2x) (m_Do_graphic.cpp) | -9.0 | -123.0 | 2D screen left (the 2D ortho `setOrtho(-9, -21, 659, 524)`, the capture rectangle's ortho) | `pc_aspect_2d_left()` |
| 0x803F7D6C | `@5163` .sdata2, same users | 650.0 | 767.0 | 2D screen right: the 2D screen is 890 wide instead of 659, so 0..640 is the middle 4:3 part | `pc_aspect_2d_right()` |
| 0x803F89B8-C4 | `@6224`-`@6227` .sdata2, `dDlst_list_c::wipeIn` (d_drawlist.cpp) | -9, -21, 659, 524 | -123, -118, 890, 716 | Bounds (x, y, w, h) of the wipe transition | d_drawlist.cpp |
| 0x803FB77C | `@4076` .sdata2, `cnvAddress`, `dDlst_2Dt_Sp_c::draw` (d_ovlp_fade4.cpp) | 1.0296875 | 1.390625 | 2D width scale of the screen-capture fade | d_ovlp_fade4.cpp |
| 0x803FB78C | `@4115` .sdata2, `dDlst_2Dt_Sp_c::draw` | -9.0 | -123.0 | Its left edge | d_ovlp_fade4.cpp |
| 0x803E68E4 | `g_meter_mapHIO`+0x8 (`dMeter_map_HIO_c`, set by its constructor) | 35 | -79 | Minimap left | `dMeter_pcAspectHIO` |
| 0x803E68E8 | `g_meter_mapHIO`+0xC | -180 | -256 | Minimap x when slid out | same |
| 0x803E68F0 | `g_meter_mapHIO`+0x14 | 590 | 704 | Map free-icon x | same |
| 0x803E6958 | `g_meterHIO`+0x50 (`dMeter_HIO_c`) | 7 | 121 | HUD pane x offset (`dMeter_parentPaneTrans` and its siblings) | same |
| 0x803E69A4 | `g_meterHIO`+0x9C | 0 | -114 | x of the minimap's button group (`dMeter_menuPlusMove`) | same |
| 0x8004A428 | `dMap_c::calcScissor`+0x84 | `li r3,116` | `li r3,153` | Minimap scissor width (EFB pixels) | exact 2D-to-EFB conversion (below) |
| 0x8004A444 | `dMap_c::calcScissor`+0xA0 | `li r4,0` | `li r4,32` | Scissor x when the converted x is negative | same |
| 0x800E1630 | `daBoomerang_sightPacket_c::setSight`+0x140 | `lfs f1,8(r1)` | `b 0x80004058` | Added code: the projected x becomes `320 - (320 - x) * 1.3333` (pool 0x803F85A4), so the boomerang's lock-on sights sit on their targets | d_a_boomerang.cpp |
| 0x8016103C | `dPlace_name_c::setScreen`+0xDC | `lwz r4,4(r28)` | `b 0x800037E0` | Added code: `scrn->mBounds.i.x = -123.0` (`lis r0,0xC2F6`), the place name toward the left edge | d_place_name.cpp (`J2DPane::pcSetBoundsLeft`) |
| 0x8018F5CC | `dDlst_GameOverScrnDraw_c::draw`+0x84 | `mr r4,r31` | `li r4,0` | `J2DScreen::draw` with a NULL context: J2DScreen's own 640x480 port, which fills the whole wide picture (game over screen) | d_gameover.cpp |
| 0x8021E6E8 | `dDlst_Ow_mask_c::draw`+0x44 | `mr r4,r31` | `li r4,0` | The same for the Wind Waker conducting mask | d_operate_wind.cpp |
| 0x80234528 | `dScnOpen_proc_c::proc_draw`+0x50 | `mr r4,r31` | `li r4,0` | The same for the opening scene's screen | d_s_open_sub.cpp |
| 0x8019E66C, 0x8019E908, 0x8019E954, 0x8019EC70 | `dMenu_Collect_c::noteAppear`/`noteOpen`/`noteClose` | `stb`, `bl fopMsgM_setInitAlpha`/`setNowAlpha` of `m970` | `nop` | The 640-wide mask pane `m970` behind a note never fades in | d_menu_collect.cpp |
| 0x801CC208, 0x801CC338, 0x801CC384, 0x801CC778, 0x801CA9A8 | `dMenu_Item_c::noteAppear`/`noteOpen`/`noteClose` (`m970`), `subWindowInit` (`m1460`, 'blak') | same | `nop` | The same in the item menu | d_menu_item.cpp |
| 0x801AB610, 0x801AC400, 0x801AC44C, 0x801AC770 | `dMenu_Dmap_c::paneAlpha` (`mMskPane`), `noteOpen`/`noteClose` (`mMsk0Pane`) | `bl fopMsgM_setNowAlpha`/`setInitAlpha` | `nop` | The same in the dungeon map | d_menu_dmap.cpp |
| 0x801B955C | `dMenu_Fmap_c::paneAlphaWarpMsgBack` (`mWts1Pane`) | `bl fopMsgM_setNowAlpha` | `nop` | The same behind the warp message | d_menu_fmap.cpp |
| 0x801D37FC | `dMenu_Option_c::mainMove` (`mCF0`) | `bl fopMsgM_setNowAlpha` | `nop` | The same in the options | d_menu_option.cpp |
| 0x801DAA38, 0x801DAAE8 | `dMenu_save_c::PaneScaleAlphaWipe` (`field_0x14[i]`), `PaneAlphaMask` (`field_0x1d4`) | `bl fopMsgM_setNowAlpha` | `nop` | The same for the save menu's wipe and mask | d_menu_save.cpp |
| 0x801B6968, 0x801B6978 | `dMenu_Fmap_c::paneTransBase`+0x7C/0x8C | `lfs f1,0.0`; `addi r3,r27,0x288C` | `lfs f1,@5162`; `b 0x80004038` | The sea chart's 'cl' mask: x -123, and the added code sets its right edge (`mBounds.f.x`) to `@5163` (767) | d_menu_fmap.cpp (`pcSetBoundsRight`) |
| 0x801C5174, 0x801C5188 | `dMenu_Fmap2_c::paneTransBase`+0x78/0x8C | `lfs f1,0.0`; `cmpwi r28,2` | `lfs f1,@5162`; `b 0x80004048` | The same in the second sea chart | d_menu_fmap2.cpp |
| 0x801F0678/7C, 0x801F0694/98 | `dDlst_2DMETER1_c::draw` | `lfs f1,0.0`; `fmr f2,f1` | `lfs f1,114`; `lfs f2,0.0` | `sScrTimer1`/`sScrTimer2` drawn 114 to the right | d_meter.cpp |
| 0x801F0700/04 | `dDlst_2DMETER2_c::draw` | same | same | `sMainParts2` (main_parts2.blo) drawn 114 to the right | d_meter.cpp |
| 0x801F0B70 | `dMeter_childPaneTransChildTrans`+0x98 | `lha r0,0x50(r5)` (`g_meterHIO.field_0x50`) | `li r0,110` | A fixed x offset of 110 for these panes | d_meter.cpp |
| 0x801F102C, 0x801F103C | `dMeter_heartScaleInit` | `bl fopMsgM_paneTrans` | `bl 0x800040EC` | Added code: x -= 114, then `fopMsgM_paneTrans` (the hearts) | d_meter.cpp |
| 0x801F8F00/08/10 | `dMeter_magicInit` | `bl fopMsgM_setInitAlpha` | `bl 0x800040D8` | Added code: the pane's `mPosTopLeftOrig.x -= 114`, then `fopMsgM_setInitAlpha` (magic meter) | d_meter.cpp |
| 0x801F9BD4, 0x801F9BE0 | `dMeter_magicTransNowInit`+0x98/0xA4 | `fsubs f1,f1,f0`; `bl fopMsgM_paneTrans` | `lfs f1,0.0`; `bl 0x800040EC` | The eight magic panes at x -114 | d_meter.cpp |
| 0x801F9C5C/6C/7C/98 | `dMeter_magicInitTrans` | `bl fopMsgM_paneTrans` | `bl 0x800040EC` | The magic meter at x -114 | d_meter.cpp |
| 0x801FBA20 | `dMeter_menuPlusMove`+0x1108 | `fadds f1,f31,f0` | `fadds f1,f30,f0` | The minimap's buttons take `y` (`field_0x9e`) instead of `x` (`field_0x9c`, now -114) plus their offset from the moved map | d_meter.cpp |
| 0x801FC28C | `dMeter_rupyInit`+0xFC | `lfs f0,@7214` (320) | `lfs f0,@9542` (206) | Screen centre of the rupee counter's sparkle | d_meter.cpp |
| 0x801FD610/20/30 | `dMeter_compassDirOpen` | `bl fopMsgM_paneTrans` | `bl 0x800040EC` | The compass at x -114 | d_meter.cpp |
| 0x801FEFB4 | `dMeter_clockMultiMove`+0x74 | `lfs f1,0x194C(r23)` | `lfs f1,0x1954(r23)` | The clock follows `field_0x1948.mPosTopLeft.x` (where the compass is) instead of `mPosTopLeftOrig.x` | d_meter.cpp |
| 0x80201D8C | `dMeter_swimTekariScroll`+0x60 | `lfs f4,0x36C(g_regHIO)` (`REG6_F(0)`) | `lfs f4,114` | The swim meter's shine 114 to the right | d_meter.cpp |
| 0x8020464C/54/68/70/88 | `dMeter_Draw` | `bl fopMsgM_setAlpha` | `bl 0x800040C0` | Added code: `pane->mMtx[0][3] = pane->mBounds.i.x + 114`, then `fopMsgM_setAlpha` (`field_0x1980`, `0x19b8`, `0x1ad0[2]`, `0x1c20[2]`, `0x1c90`) | d_meter.cpp (`pcSetMtxTransX`) |
| 0x80227A18/1C/20 | `dJle_Pb_c::pictureDraw`+0xD4 | `GXSetViewport(0, 0, 640, 480)` | `(79, 0, 480, 480)` | The Picto Box photo keeps its 4:3 shape | d_picture_box.cpp |
| 0x8022B9E4 | `dJle_Pb_c::draw`+0x6C4 (`blr`) | `blr` | `b 0x80004074` | Added code: `J2DFillBox(-130, -32, 130, 640)` and `J2DFillBox(640, -32, 130, 640)` in black (0x803F891C), bars beside the 4:3 Picto Box view | d_picture_box.cpp |
| 0x802375E4 | `dDlst_2DSCP_c::draw`+0x7C (`blr`) | `blr` | `b 0x80004074` | The same bars for the telescope | d_scope.cpp |

Where the native port differs from the Gecko code: the HUD tuning fields are set when the meter is
created (`dMeter_Create`), not rewritten every frame; the minimap scissor converts the map's 2D
position to EFB pixels with the wider 2D screen's span (the same scissor as the code's 153/32 at
16:9 within a pixel, and right at 16:10, where the interpolated immediates are 16 pixels off);
`dMeter_compassDirClose` gets the same -114 as `dMeter_compassDirOpen`, so the compass slides out
from where it is instead of jumping back first; the place name's screen moves from its real left
edge, 0 (`J2DScreen::set` sets the bounds to the BLO's size), where `widescreen_aspect.py` assumes -9
for 16:10.

## Measured on the iPad Pro (M2)

These results use the developer's optimized build. They do not establish performance parity for a
fresh player build; see [the current performance comparison](BUILDER.md#optimization-profiles).

- Widescreen and the HD pack together, on the pier after loading slot 1: 30 FPS at 100% speed, no late
  frames over more than a minute, main thread about 82% busy (the same as without mods). 5,739 of
  the pack's textures registered; the first file-select frame with new textures took 83 ms.
- All three on the iPad: loading slot 1 and walking the village at 29.9-30 FPS; the pause-menu save
  writes the card, and the saved card reloads with all three mods and with none (30 FPS both). The
  menu tree, checked with `BLUEWAKE_MENU_DUMP=1`, lists the Mods section and the installed pack.
- Better Wind Waker (the patched disc, before the settings became game options): its added code section
  ran (guest program-counter samples), and the Swift Sail was used at sea on the iPad on 2026-09-28.

## Building

The Builder (`scripts/builder/build.sh`, or `scripts/ios/build_device.sh`) adds the mods itself, as its step 6;
`--no-mods` leaves them out. It needs only python3. The step runs

```sh
scripts/mods/build_mods.sh BUILD_DIR
```

which applies the widescreen codes, translates the widescreen executables, the game with Better Wind
Waker's option sites (its executable and modules) and each widescreen executable with them, with the
build's own DolRecomp, generates a composite tree for each, and adds the variants, the option table and
the options' values to the build's composite source before the compile. Nothing needs copying to the
device.
