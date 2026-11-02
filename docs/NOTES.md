# Notes

Running log, newest at top. Failed attempts stay in.

## 2026-11-02 — Android: reinstalls had silently failed; fullscreen confirmed

- Later the same day: saving confirmed. The user saved and loaded across a restart, and
  `saves/robotron64.us.controller_pak_1.bin` (4142 bytes) exists on the phone and survived an `adb install -r` update.
  The user chose landscape only. The icon robot was too high, so it is now centred on its drawing (`ROBOT_CENTER`); the
  trimmed foreground measures 224×210 centred on the 432 px canvas.
- Correction to the entry below: the icon build was never on the phone, and the connection had dropped rather than the
  phone being unplugged. `dumpsys package` showed the first APK (2026-11-01 10:59) was still installed. My command piped
  `adb install` through `tail -1`, which hid the failure, and I reported the install as done.
- Cause: `INSTALL_FAILED_UPDATE_INCOMPATIBLE`. Gradle made a new debug key in `~/.android` of each throwaway container.
  Fixed with `ANDROID_USER_HOME=/build/android-user` in `tools/build_android.sh`, so the key stays in the Docker volume.
  `apksigner` shows the APK's certificate matches that key. The first install with it needed an uninstall, which wipes
  the app folder, so settings were pulled to `build-android/phone-backup/` (gitignored) and pushed back with the ROM.
- `saves/` on the phone was empty before that uninstall, and that is where `controller_pak.cpp` writes. So no save had
  been written on the phone, and saving on Android is still unconfirmed.
- Fullscreen confirmed on the new build: the surface is 3120×1440 at (0,0) in the log and in `dumpsys SurfaceFlinger`, with
  no bars in a screenshot. It also locks the game to landscape; the first build could rotate to portrait. The stray system
  files are gone from the app folder.
- The 3 AdrenoVK "Shader compilation failed" lines came back on this fresh install, and did not appear on a later launch
  of an existing install. So they go with a first launch without a pipeline cache (`.rt64/`), not one specific launch.
  Still not known which pipelines they are.

## 2026-11-01 — Android test build runs on the S26 Ultra

- First launch reached the copyright screen and a wave, on the stock driver, with the Xbox controller. The user reports
  it "worked perfectly, like on Windows". Frame rate not measured. Decision and result: ADR 007.
- Build: `tools/build_android.ps1`, own Docker image. Files build in a Docker volume, not on the mounted Windows folder.
  The `lib/` copy there gets the patches. `git apply --ignore-whitespace` handles the CRLF checkout.
- Errors on the way:
  - `recompinput` and `recompui` need the SDL2 target on Android.
  - rt64's file dialog patch was for an older API; patch 0004 adapts it to the callback version in our pin.
  - `RT64_SDL_WINDOW_VULKAN` must be defined after `add_subdirectory(rt64)`: inside rt64 the Android window is an
    `ANativeWindow`, not an `SDL_Window`.
  - `AssetManager.list("")` also lists Android's own files, so only `assets` and the controller database are unpacked now.
- Driver log on the first launch only: 3 lines of "Shader compilation failed" / "Pipeline create failed" from AdrenoVK
  right at startup, none on the next launch. Which pipelines they are was not found out; nothing looked wrong on screen.
- Icon: adaptive icon, with layers derived from `icons/app.svg` by `tools/make_android_icon_layers.py`. Checked on a
  simulated circular and square mask, not yet seen on the phone's launcher.
- **Not tested:** the fullscreen fix. In the build on the phone the game was inset by the status bar and notch
  (2801×1327 of 3120×1440), and could rotate to portrait. Java immersive flags alone did nothing, because SDLActivity
  resets them for a non-fullscreen window. `create_window` now asks for `SDL_WINDOW_FULLSCREEN` and landscape only on
  Android. It builds, but the phone was unplugged before it was installed.
- Desktop after the CMake and `main.cpp` changes: Windows build links, Linux configure passes (not a full Linux build).

## 2026-11-01 — Android test build: first look (branch `android`)

- Goal: go/no-go on the user's Galaxy S26 Ultra with a controller. See ADR 007.
- Read from the phone with `adb`: SM8850, Adreno 840, Vulkan 1.4.295, Android 17, 4 KB pages. `cmd gpu vkjson` prints
  numbers as floats, so convert before bit shifts. All RT64 extensions are present except optional
  `VK_KHR_present_wait`.
- A plain diff of Goemon's `rt64` fork against upstream shows 70 files because its base is old. The Android commits
  are few and nearly all cherry-pick cleanly onto our pins. Only applying was tested, not building.
- plume's upstream is `renderbag/plume` (`lib/rt64/.gitmodules`), not under `rt64/`.
- `gh` isn't installed here; `api.github.com` via curl gave the licences.

## 2026-10-31 — Levels and floors: the green floor is `swamp.flr`

- The user asked about level themes and lore: was the green floor meant to be a forest? Researched the web and
  the decomp (two agents), then re-checked the ROM decoding with a new script, `tools/level_table.py`. It
  only prints, apart from optional floor PNGs for the gitignored `texture_work/`.
- Chain: `D_80074C08[level]` (16-byte record) → floor index → `D_8007CCC0` file name → `<name>.flr`, a 32×32
  RGBA16 texture. `LEVELNOS.DAT` gives 200 waves + 10 bonus slots. Scene files confirm bosses at waves 100
  and 200, and round floors in waves 60–69, 160–169 and `BONUS2`.
- The script's first version read the scene files little-endian (they're big-endian in the ROM) and printed
  kind 2 as "no background" (`func_80022BFC` maps 1/2/3/6/7/8 to 0). Both fixed. `floor5` and `tile2` exist
  as both `.flr` and `.raw`, so the PNGs keep the extension in their names.
- `--match dump` matched all 6 arena textures plus one "other" texture exactly (5-bit compare). The green
  veins are `swamp.flr` (bonus rounds, waves 140–149). The Electrode Forest (30–39) uses `swirl`, which we had
  labelled `sky_swirl`. Relabelled all floors with their file names in `texture_classes.txt`. Six floors
  aren't in the dump yet.
- Correction to the first-dump note below: not every texture is CI8. The floors are RGBA16 and the font is I8
  (`fmt` 4); the cabinet is CI8.
- Write-up: docs/LEVELS.md (floor per wave, floor list with hashes, bonus rounds, manual facts, cheats).

## 2026-10-31 — Texture packs split into HUD and HD; `-Pack` leaves out work files

- Asked whether players should be able to choose only an HD HUD. Checked how packs combine:
  `RT64Context::check_texture_pack_actions` (RecompFrontend `rt64_render_context.cpp`) passes every enabled
  pack to RT64, sorted by mod order, earlier overriding later. The `_recomp_texture_pack_enabled` mod option
  only toggles a whole pack. So the split has to be into separate packs.
- The user chose two: `robotron64-hud` (font, HUD icons) and `robotron64-hd` (everything else, the cabinet so
  far). The cabinet moved over with its entries; the font's `shiftFilters` stays in the HUD pack only.
- Catch found on the way: RT64 allows Replace / Save directory only when a single pack folder is loaded
  (`fileSystemIsDirectory`, `rt64_texture_cache.cpp`).
- `texture_pack.ps1 -Pack` now recurses and skips Affinity files, `~bak`/`~lock~` and `rt64.json.old`. It
  writes the zip with `ZipArchive` and forward slashes, because Windows PowerShell 5.1's `Compress-Archive`
  stores backslashes in entry names. Tested on copies of both packs: 52 and 6 entries, no work files.

## 2026-10-30 — Cabinet side art: pieces show 31 of 32 rows

- The user's side-art replacement worked in the intro, but the 4 looked cut off at the seam between the middle
  and bottom pieces. The panel's top edge also had a thin red and blue line.
- Measured along the centre column of a screenshot: everything matched the design inside each piece, but about
  32 design px were missing at each seam. The line at the top had the colours of the top piece's bottom row.
- Explanation: the pieces are drawn with texture coordinates 0..31, and they use wrap mode (tile.json). I had
  set shift `none` for `cabinet/*`, copying the font fix, so RT64 cut the bottom 32 px of every 1024 piece and
  blended the top edge with the wrapped bottom row.
- Fix: default shift `half`, which hides 16 px at every edge, plus overlapping slices at y 0/992/1984 on a
  1024×3008 canvas. Documented in TEXTURE_PACKS.md ("Art split over several textures") and GAME.md.
- The user asked why the original isn't cut off. It is. In a screenshot with the original 32×32 pieces as the
  replacement, the centre column is about 1 texel row short at the top/middle seam, and about 1–1.5 rows short
  at the middle/bottom seam: the 8's bottom bar shows 2 rows instead of 3. One 32 px row is hard to see, and
  it falls mostly on blank rows. At 1024 px the same row is 32 px wide, and it cut through the 4.

## 2026-10-30 — Texture packs (HUD, fonts): a mod, outside the repo

- Asked whether improved HUD/font textures should be a mod. Yes: the port already registers RT64 texture
  packs (`.rtz`, content `rt64.json`) as a mod type, toggleable at runtime (main.cpp). Upgraded textures are
  derived from the game's art, so packs stay out of this repo (rule 1) and are shared separately.
- Verified in the pinned sources: a plain folder in the mods folder with `mod.json` + `rt64.json` + images
  loads as a pack (librecomp `mods.cpp` loose mods; RT64 opens folders and zips alike); `mod.json` needs
  `game_id` "robotron64" and `minimum_recomp_version` <= 0.1.0; `rt64.json` must state `hashVersion` 5 (it
  defaults to 1, which blocks the inspector's Replace). Dev Mode is a hidden graphics option
  (`developer_mode` in graphics.json, read at renderer start); F1 opens RT64's inspector (Dump PNG, Copy
  hash, Replace with a single folder pack, Save directory). No file watching: toggle the pack in the Mods
  menu to reload; a new pack folder needs a restart.
- Added `tools/texture_pack.ps1` (-Init pack folder from `tools/texture_pack_template/`, -DevMode On/Off
  keeping the other graphics.json keys, -Pack to a plain-zip `.rtz`) and `docs/TEXTURE_PACKS.md`; README
  sections for First person and Texture packs. The script was tested against a temporary config folder;
  loading a pack in the game is the user's check.
- The user's machine blocks scripts (no execution policy set, so Windows' Restricted default); the tool
  shell here runs with Bypass, which is why build scripts always worked from here. Guide now says how.
- First dump: 134 textures, all 32x32 CI8 (RGBA16 palette), in `dump/` (now gitignored, with
  `texture_work/`). Classified by eye from a 4x overview into `tools/texture_classes.txt`: font 41 (A-Z,
  0-9, period, question, END/DEL/SPC: the name-entry alphabet, one glyph per texture, white with outline,
  tinted by the game), arena 34 (labels are best guesses: banner/strip pieces may belong to title or attract
  screens), effects 46, models 7 (eye, shoes), icons 1 (lives head), other 5. `tools/sort_dump.sh` sorts a
  dump into 4x nearest-neighbour templates per category with labelled sheets and an index. ImageMagick in the
  image has no default font, so sheet labels use assets/InterVariable.ttf.
- The user spotted where the "arena" banners come from: the arcade cabinet in the intro (marquee, screen
  bezel with the coloured symbols, control panel with "2084", coin door, side art). Moved them and three of
  the "other" ones to a new `cabinet` category: now font 41, cabinet 35, effects 46, models 7, arena 3
  (floor tiles, sky swirl), icons 1, other 1. A later dump added the © and ® glyphs (copyright screen):
  font 43, 136 textures in all.

## 2026-10-30 — First-person camera (the game's cut FIRST view)

- Asked what a first-person mode would need. An Explore pass found the game already has one: camera view 8
  "FIRST" is fully coded in the camera director (eye at the player, pitch 0, wide lens, tank controls and
  facing-relative shots via `D_8007394C`) but missing from the L/R cycle table `D_80073864 = {10, 1, 9, 3, 0}`,
  while the name table still lists five names. Details in GAME.md (Camera views, First-person view).
- User's choices: add it to the L/R cycle, keep the game's tank controls, single player only.
- `src/game/first_person.cpp` + hooks (robotron64.us.toml, end of file):
  - Cycle: at 0x8001286C (func_8001276C storing the next view), a wrap from OVERH goes to view 8 instead,
    leaving the index on OVERH so the next press wraps to ARENA. The cycle table can't be extended in place
    (the next table `D_80073878` follows it).
  - HUD name: func_800126E0 entry returns the FIRST name for view 8.
  - Per frame (func_8003A8B0 entry): leave view 8 when not allowed (option off, two players, also after a
    co-op game's first player is out, as it switches to style 0); find the player's object.
  - Hide the player: in the existing 0x8003A990 draw-loop hook, `goto L_8003B20C` (the loop's skip path) for
    the player's object, which also skips its floor ring and aim pose. Toggling the object's enabled flag
    instead would be undone by the invulnerability blink.
  - Culling: the x hooks now get rdram and divide by 0.95/0.5 more in view 8; new y hooks after the second
    abs() call (0x8003D5FC, 0x8003D9B4, 0x8003E484) divide by 0.71/0.5.
  - Near plane: 25 instead of 100 in view 8 (hook at 0x80048F24, value at sp+0x10).
  - Option First-Person Camera (On by default).
- Hook text is compiled as C: `bool` is not available there, so the hide helper returns int.
- Play test: works; two players never get FIRST; the death animation switches to the game's death camera and
  the next round is back in FIRST (the game's own behaviour).

### Modern first-person controls

- User asked for normal first-person controls next to the tank ones: option values Off / Classic / Modern
  (saved 1 keeps meaning Classic).
- `get_n64_input` (main.cpp), while in view 8 with Modern: port 0 keeps the left stick and D-pad, its C
  buttons are removed and A or B adds C-up (shots are facing-relative in this view, so C-up fires ahead);
  port 1 (the aim stick) is muted. The turn input (right stick X, or C-left/right when the stick is still,
  for keyboards and for Analog Aiming off, where the right stick also makes C buttons) goes to an atomic.
- Hook at func_80035E3C entry (the tank controls): turns angle08 by turn x 2048 units/s (half a turn per
  second at full stick, scaled by the game's timestep D_8009EF94), walks forward/back and strafes with port
  0's stick (dead zone 5, full at 80) and the D-pad, velocity = direction (the game's func_8003CC88 /
  func_8003CC58) x the walk speed argument, model yaw and view yaw = angle08, then skips the tank code.
- Play test: strafe direction right, turning analog, walk speed good, A/B fire ahead; Classic, Off and two
  players unchanged.

## 2026-10-30 — App icon: box-art robot; hero head in the launcher (fan art)

- The user asked for icons of the game's characters and chose a fan-art likeness, knowing it is a derivative
  of Midway's designs (CREDITS names the owners). Candidates were drawn from the user's references (PAL box
  art, poster, menu screenshot): the box-art robot, a green Grunt (dropped after the first sheet) and the
  hero's head.
- I misread "the head fits good into the main menu" as picking the head for the app icon (`ef0c5fa`). The user
  meant: the head goes into the launcher like the game's menu, the robot is the app icon. Fixed with
  follow-up commits, no history rewrite.
- `icons/app.svg`: the robot on the box art's navy sky and blue grid floor. First drafts were drawn freehand
  (one fist raised, a fixed depth offset); the user wanted it like the box art, so the visible faces are now
  traced from the user's box-art crop in its own pixel space (mapped onto the tile with one transform): left
  arm held out, right arm hanging and leaning, head behind the torso, striding left leg, upright right leg,
  each face in the art's tone. Each part was checked in zoomed side-by-side crops against the reference
  until it matched (one gap between foot and leg was found that way). The chest emblem stays abstract (no
  "G"; it resembles the publisher's logo; user's choice). A lamp-like glow sits behind the head, no floor
  shadow. Elements with `class="detail"` (grid, emblem stripes) are dropped for `app-small.svg` (16-32 px).
- `assets/launcher/hero.svg`: light-bulb cranium, arched brows, figure-8 goggles with one gradient across both
  lenses (green through lavender to pink, as the game's lenses shift between green and pink), purple irises.
  Transparent, viewBox cropped to the head.
- Launcher layout like the game's main menu (user's pick "head on top, title small"): head 360 dp high at the
  top with a gentle sway (3° turn, 6 dp bob), wordmark shrunk to 60/20 and moved to 45%, and the button menu
  lowered from RecompFrontend's `bottom: 25%` to 7% via the `GameOptionsMenu` returned to `on_launcher_init`.
  As in the game's menu, the items have no highlight box (their public `hover_style`/`focus_style` get a
  transparent background; the pulsing text marks focus), and the menu is 25% wide instead of 50%, so the click
  area hugs the text.

## 2026-10-29 — Launcher look and app icon

- **Research (other ports):** Banjo and Harvest Moon animate hand-drawn fan SVGs in the launcher update
  callback; Snowboard Kids 2 and Dinosaur Planet use credited fan art; WCW committed the official logo (not
  for us). No port shows ROM textures in the launcher.
- **Renderer limits:** recompui's RmlUi render interface has no shader/filter hooks, so RCSS gradients,
  shadows and blur don't draw. SVGs are rasterised by lunasvg, so soft shading goes into the SVGs.
  `<img>` only shows byte-queued images; SVGs load from `assets/`.
- **Launcher:** theme palette after the arena (navy, HUD blue-violet, laser yellow); `bg.svg` (sky, stars,
  horizon glow, vignette) and `floor.svg` (converging grid lines); the cross lines are plain divs placed
  each frame at 1/z below the horizon so the floor scrolls in perspective; four laser streaks; Orbitron
  wordmark in place of the default title.
- **Round 2 (user feedback):** lasers removed; colours now follow the title screen instead of the arena:
  red/purple/blue diagonal bands in `bg.svg`, lavender grid, white + lavender wordmark, theme Primary lavender
  and Secondary a brightened red.
- **Settings on black:** the launcher's Settings/Controls/Mods buttons call `hide_all_contexts()` before
  `config::open()` (RecompFrontend `ui_launcher.cpp`), so only the modal's translucent overlay is left. A still
  copy of the backdrop (`SettingsBackdrop` in `launcher_animation.cpp`) now lives in the config modal's context,
  created on the first launcher update (the modal is created after the launcher; the launcher context has to be
  closed first, as contexts can't nest). It sits behind the modal with an inline `z-index: -1` (recompui has no
  z-index setter and `set_property` is private; RmlUi draws negative z-index first) and hides while
  `ultramodern::is_game_started()`, so the in-game settings still show the game.
- **Round 3 (user request: "a background like the game's menu"):** the arena scene (`bg.svg`, `floor.svg`, the
  scrolling cross lines) is replaced by `bands.svg`, a repeating red/purple/blue gradient, scaled 4x and slowly
  drifted and turned with sines each frame, under a static `shade.svg`. Scaling uses a transform, so lunasvg
  rasterises at window size. The settings backdrop shows the same bands, held still.
  - First try stretched with the window's aspect (the bands element was window-sized with
    `preserveAspectRatio="none"`) and moved too slowly to notice; fixed with a 640 dp square scaled 7× and a
    steady 5°/s spin.
- **Round 4 (user: "more like the game"):** the decomp shows the game's menu background is a 4×4 grid of
  vertex colours, each on its own sine wave (see GAME.md, Menu background), not moving shapes. Replaced the SVG
  bands with `ColorWaveElement`, a custom Rml element (tag `robotron64-colorwave`, registered through
  `Rml::Factory` like RecompFrontend's `recomp-color`). Its `OnRender` builds a 16-vertex mesh each frame with
  `RenderManager::MakeGeometry`; the RT64 render interface shades per vertex. Rejected alternatives: SVG
  gradients can't express it; a per-frame image would go through `LoadTexture`, which creates a texture with a
  GPU wait each time and doesn't keep it. Colours are now the game's (0x80 red/blue), and `shade.svg` is only a
  mild vignette (the user found round 3 a little dark). The settings backdrop uses the same moving field.
- **Icon:** `icons/app.svg` (+ `app-small.svg` for 16-32 px) rendered to `app.png` / `app.ico` by
  `tools/make_icons.sh` in Docker (librsvg + ImageMagick). Windows: `icons/app.rc`; SDL 2.30.3 registers its
  window class with the module's first icon group, so the window and taskbar use it too. Linux: `app.png`
  embedded with `file_to_c` and set with `SDL_SetWindowIcon`. Both builds link.

## 2026-10-29 — Settings text, per-player keyboard profiles

- The Controllers option showed "ONEPERPLAYER": key and display name were swapped in the enum entry
  (librecomp `ConfigOptionEnumOption` is value, key, name). The key is unchanged, so saved settings still match.
- Option descriptions now give each value its own line (`<br />`, as upstream's graphics tab does).
- A keyboard player's card showed "Keyboard (Player 1)", a profile RecompFrontend creates empty, while
  `update_player_profiles` silently used Keyboard (SP). Now those profiles are filled once from the normal
  bindings and kept, so the card shows what is used and each player's keys can be edited.
- The Assign Players screen spreads two cards to the edges (upstream layout for four). Left as is (user's call).

## 2026-10-29 — P2 floor ring and aim pose; reassign fix

- **Ring and pose:** P2 had no yellow floor ring and didn't point its arm. Both come from "last player behavior"
  globals that the draw loop applies to one object, and P1's behavior runs last. Hooks save each player's
  aim/facing at the behavior epilogue and select that player's own object per draw (`coop.cpp`). Gated on a
  co-op flag set at game start, so it stays on after the first player is out. Tested by the user.
- **"Assignment reset" after Shared → One per player:** a temporary log showed the assignment survives the mode
  switch. The real bug: confirming the same layout again made RecompFrontend's commit reset the profiles,
  and `update_player_profiles` only re-ran on a layout change, so the keyboard drove both players again. It
  now re-applies after every finished assignment. Tested by the user; log removed.
- **Not saved across restarts:** RecompFrontend keeps the assignment in memory only (known, ADR 006).

## 2026-10-29 — One controller per player (ADR 006)

- **Option** Controllers: Shared / One per player; the callback switches recompinput's single-player mode.
- **Ports:** the game uses two N64 ports per player (0/1, 2/3). `main.cpp` maps port N to player N/2 (player 1
  when that slot is unassigned), reports all four ports connected so the game's port bindings never change,
  and sends rumble from port 0/2 to that player's pad. Patching the game down to two ports was
  considered and rejected (ADR 006).
- **First test:** P1 pad, P2 keyboard: the keyboard drove both. RecompFrontend's assignment commit sets only
  the assigned device's profile; P1 kept Keyboard (SP), and P2's controller profile stayed at index 0, which
  is Keyboard (SP). An init-time -1 for P2's keyboard was overwritten by the commit.
- **Fix:** `update_player_profiles` (poll callback, after two stable polls) re-applies the profiles after each
  assignment change.
- **Tested by the user:** pad + keyboard both ways, two pads, reassigning, Shared unchanged.

## 2026-10-29 — Simultaneous two-player (Two Player Mode option)

- **Finding:** the game has a complete simultaneous mode (play style 3) that the menu never selects. The new
  option rewrites TWO PLAYER's style 1 to 3 at `func_80022528`'s entry (`robotron64_two_player_style`).
- **First test (style 3 as shipped):** both spawned and played, but enemies only chased `currentPlayer`, menus
  and skip read only one input block (pause looked frozen), a revived player who fell in a crowd could never
  stand up, only one high-score entry, and the out player's HUD wave stopped.
- **Fixes (`src/game/coop.cpp`, hooks in `robotron64.us.toml`):**
  - merge the second player's input block into the session block (menus, pause, skip);
  - swap `currentPlayer` to the nearest living player around each enemy behavior call, restoring it only if
    unchanged;
  - respawn: move to a free spot (`func_80027ED4`) only when an enemy is within 4000, then copy the per-actor
    body of the wave-start drop-in (`func_80035CC8(0)`). The first try beamed the player in with the enemy
    materialize animation; the user found it abrupt and pointed to the wave-start fall, which the earlier
    research had missed. The hook had to move to 0x80033D80, after the revive's own flag store, or the 0x40
    callback flag would have been cleared;
  - high score: remember who ran out first; after the game's entry for the last player, start a second entry
    for them (hook at 0x800237A0; 0x8002379C is a delay slot);
  - HUD: copy the live wave into both players' wave fields.
- **Build snag:** `coop.cpp` needed `<initializer_list>` for a range-for over a braced list.
- **Tested by the user:** drop-in respawn, normal size after the sinking death, free-spot respawn in a crowd
  (on the spot otherwise, e.g. after a projectile), both name entries, Alternating unchanged.
- **Open:** two physical controllers (RecompFrontend maps pad 2 to ports 2/3; the game checks
  `D_8007C334 & 4`); the camera follows `currentPlayer` (`func_80012950`); a player with no lives lies in the
  arena (harmless).

## 2026-10-29 — HUD placement; background for very wide windows

- **HUD:**
  - The in-game HUD (`func_8004B590`) is 3D quads at z = 200 with literal x coordinates, so RT64's HUD
    Placement had no effect: in Expand the lives and WAVE stayed in the 4:3 area.
  - Hooks switch shifting on at the function's entry and off at its only `jr $ra` (0x8004BBBC), and the
    position setter `func_80049DD8` passes x through `robotron64_hud_x` (`ultra_extensions.cpp`).
  - Glyphs are grouped into runs (same y, x step ±8). A run starting at |x| ≥ 36 moves toward its edge by
    `124.9 × (widening − 1)`, where widening follows HUD Placement (Original 1, 16:9 clamped, Expand full).
    The 1P score (x = 10) and the power-up icons stay centred.
- **Very wide windows:**
  - Past ~2.15:1 the widened background X no longer fits in s16. The first version clamped it, leaving edges
    at 21:9 and wider.
  - Instead X, Y and Z are divided by one factor (same projected size, closer quad); safe because the
    render mode has Z compare but no Z update.
  - First test: Z was hooked in only two routines; three more read the extents, and their quads shrank into a
    box with black on all sides (Crave intro, wave 3). Found the rest by grepping every load of the two
    globals in RecompiledFuncs; now all five have a Z hook.
- **Tested by the user:** 4:3, 16:9 and Expand at ~2.6:1 and freely resized: background fills everywhere
  (Crave intro, PRESENTS, title, transitions, waves); HUD placement correct in 1P and 2P.

## 2026-10-28 — Linux build in Docker; ROM-free CI (ADR 005)

- **Linux build:**
  - `tools/build_linux.sh` runs in the build image, which gained `libsdl2-dev libfreetype-dev libgtk-3-dev llvm
    file`.
  - All 765 steps built and linked on the first try, with no source changes. The only "Failed" in the log is a
    CMake probe (`LD_FLAG_Z_NOEXECSTACK`).
  - `ldd` resolves every library. Output: `build-linux/Robotron64Recompiled` (x86-64 ELF) and a 7 MB tarball
    with `assets/` and the controller db.
  - The Linux build also rebuilt `patches/patches.elf` with Ubuntu's clang (MIPS backend), so PATCHES_BUILD_ELF=ON
    works there.
  - The Windows build still links afterwards.
  - **Not run yet:** there's no Linux desktop.
- **CI:**
  - `.github/workflows/ci.yml` checks that no ROM-derived files are tracked, builds N64Recomp/RSPRecomp on
    ubuntu-24.04 and windows-latest, and runs `py_compile` and `bash -n` on the tool scripts. All checks pass
    locally.
  - The template's secret-based workflows were removed.
  - Not run yet: no GitHub remote.

## 2026-10-28 — High frame rate works without a patch

- **Measured by the user** with the NVIDIA overlay and Xbox Game Bar: Framerate Manual 120 → 120 fps, Display → 240 fps
  (240 Hz monitor). No artifacts seen.
- **Why no patch is needed:**
  - Matrices the game hasn't tagged default to `G_EX_ID_AUTO`. RT64 matches them to the previous frame
    heuristically and interpolates objects and the camera (`lib/rt64/src/hle/rt64_game_frame.cpp`,
    `rt64_transform_group.h`).
  - The game gives every object its own matrix (GAME.md, Matrices), so this tracks whole objects.
- **Not interpolated:** joint animation, which is CPU-transformed vertices, so it stays at the game's ≤60 fps.
- **If artifacts show up later:** tag the pool matrices with `gEXMatrixGroup`.
- **The earlier "Framerate has no effect" note was wrong.** The difference just isn't easy to see by eye.

## 2026-10-28 — Widescreen object culling

- **Cause:** three model draw routines skip an object unless `z+500 >= 2|x|` and `z+500 >= 2|y|` in camera space.
  - The routines are `func_8003D43C`, `func_8003D7F4` and `func_8003E220`, binary fallbacks in the decomp
    (read from its disassembly, `build/analysis/cpu/baserom_80000400.text.s`).
  - The check is a fixed square cone that matches the 4:3 view. No global feeds it.
- **Fix:**
  - A hook in each routine, placed right after its `abs(x)` call returns (`0x8003D5D0`, `0x8003D988`,
    `0x8003E458`, each a `lw $t3` that is followed by `sll $t2, $v0, 1`).
  - Each passes `|x|` through `robotron64_cull_abs_x` (`ultra_extensions.cpp`), which divides it by the view
    widening factor (1 in Original) and a 1.2 margin.
  - The y and behind-camera checks are unchanged.
- **First try (no margin):** objects still vanished in the last few pixels. The test uses the object's centre,
  so on hardware the pop-out was hidden in TV overscan. The 1.2 margin fixed it.
- **Tested by the user:** in Expand, enemies, humans and items are visible to the edges, with no other rendering
  issues seen. Original works.

## 2026-10-28 — Widescreen background

- **Cause of the "box":**
  - The background gradient is a 3D quad at Z = 32000 with half-extents `D_8007CCA8`/`D_8007CCAC`.
    `func_800400D0` sets them every frame to 19200 × 14400 (decomp
    `renderer_surfaces/background_dispatch.c`).
  - In the game's 4:3 projection that covers about 96% of the view; on a TV the edge was hidden in overscan.
  - RT64's Expand widens the view, not the quad.
- **Not the cause:** a 2D or fade rectangle. The colour buffer is cleared every frame.
- **Fix:**
  - A hook at `0x80040190` (`L_80040190`, where both extent paths join) passes the extents through
    `robotron64_background_extent_x/y` (`ultra_extensions.cpp`).
  - These grow the quad by 6%, and in Expand also multiply X by `window aspect / (4/3)`.
  - The result is clamped to s16; that's enough up to about 2.1:1 (replaced 2026-10-29, see above).
- **Tested by the user:** with Expand the gradient fills the window; Original still looks right.
- **Next issue:** objects near the left/right edges disappear in Expand. The game culls against its 4:3 view.

## 2026-10-28 — Controller Pak saving

- **Why:** the runtime's `osPfs*` functions are stubs that return `PFS_ERR_NOPACK`, so the game warned "NO CONTROLLER
  PAK" and couldn't save.
- **What:** `src/game/controller_pak.cpp` implements the libultra file API the game calls (GAME.md lists them) for
  channel 0. It works at the note level, not as a raw 32 KiB PFS image. That was simpler to get right without a
  reference implementation to check against. The catch: the file isn't an emulator `.mpk`, and a converter could
  be added later.
  - Storage: `<config>/saves/robotron64.us.controller_pak_1.bin`, written with the runtime's
    `open_output_file_with_backup` / `finalize_output_file_with_backup` (it keeps a `.bak`).
  - The file defines all ten `_recomp` symbols from librecomp's `pak.cpp`, so the static library never links that
    object, and there was no duplicate-symbol error. `osPfsIsPlug` and `__osGetId` moved there from
    `ultra_extensions.cpp`. IsPlug now reports a pak in port 1, and `__osGetId` returns 0.
- **Tested by the user:**
  - the warning is gone;
  - saving and loading work;
  - after quitting completely and restarting, LOAD GAME lists the save and loads it;
  - the file is about 5 KB, as expected for one 4 KiB note plus the header.

## 2026-10-28 — Analog aiming via the game's two-controller mode

**Finding.** The game already supports analog aiming (decomp `src/game/player/behavior.c`). Player 1 moves with
port 0 and, if port 1 is connected, aims with port 1's analog stick (`secondaryX/Y`). The shot angle is
`func_8003CD4C(y, x)`, which has full resolution. Held fire direction bits (D-pad/C buttons) override this with
8-way fire. Player 2 uses ports 2 and 3 the same way. This is the N64 two-controllers-per-player setup. Movement is
analog too, which corrects the previous entry: only the direction bits are 8-way.

**Change.** No game patch.
- `get_n64_input` (`src/main/main.cpp`) wraps RecompFrontend's input callback. In single-player mode, odd ports
  return only the right stick (Y negated) and no buttons.
- The right stick is suppressed from mapped inputs (`recompinput::set_right_analog_suppressed`), so it no longer
  produces C-button bits, which would override the aim.
- Option Settings → General → Analog Aiming (`src/game/config.cpp`, default On) controls both. Off restores the
  C-button mapping.

**Tested by the user:** smooth aiming at any angle; Off brings back 8-way fire; moving with only the left stick
doesn't shoot.

## 2026-10-28 — v0.2-playable: long play test passed

The user tested the build containing the changes below.
- **Passed:**
  - Start right after more than 2 minutes in the launcher, so the throttle works;
  - twin-stick and keyboard;
  - clean audio, with music switching correctly;
  - 35 minutes and 26 waves with no crash, including the bonus stage, game over and pause/resume;
  - password `BQJDNQMBHC` (wave 21).
- **Framerate setting:** no visible change at any value. That's expected, because RT64 needs interpolation
  patches to show more frames than the game renders.
- **Right stick fires in 8 directions only.** This is the original design: `func_8003C1A8` reduces every input to
  direction bits (GAME.md, Input). Analog aiming would be a gameplay patch, behind an option; it's next.
- **Transition trails feel inconsistent.** Deferred.

## 2026-10-28 — Play-tested to wave 5; triage and small fixes

**Play test (user).**
- Waves 1–5 with no crash or hang; the controls feel right.
- The user compared Original and Expand settings with N64 footage of the Crave intro, the "PLAYER 1 UP"
  transition and wave 1.
- At Original settings the picture matches the N64.
- The 4:3 "box" with Expand, and the smeared transitions, are explained in GAME.md (Rendering). Both look like the
  original game's own behaviour, so they're deferred.

**Changes.**
- **Slow Start after a long stay in the launcher.** `ThrottledRenderContext` in `src/main/main.cpp` wraps
  RecompFrontend's render context. It forwards at most one `send_dummy_workload` every 100 ms and drops the rest,
  so a backlog of dummy frames can no longer delay the first game display lists. Nothing in RecompFrontend casts
  the context back to `RT64Context` (grep), so the wrapper is safe.
- **Controls.** The C buttons are labelled "Fire … (right stick)". RecompFrontend's default mapping already puts
  them on the right stick (`recompinput/src/input_mapping.cpp:44-47`), so twin-stick works without new code.
- **Log.** The diagnostic `printf` is gone from the `osAiGetLength` clamp hook.
- **Docs.** README (controls, recommended settings) and the new `docs/TESTING.md` checklist.

**Not changed: the default graphics settings.** Expand and Display are hard-coded in
`RecompFrontend/recompui/src/config/ui_config_tab_graphics.cpp:291,307`, and no setter was found. The README
recommends Original instead.

## 2026-10-27 — ONE PLAYER starts: three runtime timing fixes

The user confirmed that ONE PLAYER now works. Each problem was diagnosed from evidence: WER crash records,
`llvm-symbolizer` on the faulting RVA, and thread stacks from WinDbg's `cdbX64 -pv` (a non-invasive attach). LLVM's
`lldb` can't start here because it needs `python311.dll`. All fixes are `[[patches.hook]]`s in
`robotron64.us.toml`.

1. **Crash pressing Start (sometimes).** `0xC0000005` in `func_80050BD0` (graphics scheduler thread).
   - Cause: it dereferences `D_8014BE40`, the profiling slot that the retrace thread sets on the first VI
     (decomp `scheduler.c:79-87`). On hardware a VI always comes first; under the runtime a task can.
     The audio scheduler (`func_80050928`) has the same dereference.
   - **Failed first fix:** a hook that waited with `yield_self_1ms` until the pointer was set *deadlocked* at
     Start (all game threads idle).
   - Final fix: if it's NULL, point it at slot 0 (`0x8014B9E0`), which is what the first retrace does anyway.
2. **Crash a little later.** `0xC0000005` in host `queue_samples` (`main.cpp`).
   - Cause: the game sizes each audio frame as `D_801901F4 - osAiGetLength()/4 + D_8008D834`
     (`audio_task_build.c`). The runtime reports the host audio queue, which is much larger than real hardware
     would report (logged: up to 6821 bytes against 2112), so the result wraps to about 0xFFF0 frames.
   - Fix: clamp the value `osAiGetLength` returns, in `func_800521C8`. As a backstop, `queue_samples` also drops
     chunks over 0x8000 samples.
3. **Freeze on ONE PLAYER.** The WinDbg stacks showed every game thread in `osRecvMesg` except MAIN, which was
   executing in `func_80053CC0` called from `func_80051680`.
   - Cause: a busy-wait for the audio thread to stop the old music, `while (func_80053CC0(id) == 3) {}`
     (`audio_generation.c:61-64`). Game threads are scheduled cooperatively, so the audio thread never runs.
   - Fix: `yield_self` in the loop.

**Known issue: a slow Start after a long time in the launcher.**
- Stacks showed the host Gfx thread working through `send_dummy_workload` (RecompFrontend), with a `fullSync`
  each time, while MAIN waited for its first graphics task.
- The VI thread queues one dummy workload per VI while the launcher is shown. They pile up when processing is
  slower than 60/s, and the delay after Start grows with launcher time. It doesn't hang; it drains eventually.
- To fix in our code (not the submodules), e.g. by wrapping the render context to skip stale dummy workloads.
  (Done 2026-10-28, see above.)

## 2026-10-27 — Milestone v0.1-title: boots to the title menu

**Confirmed by the user's screenshots**
- The launcher accepts `baserom.us.z64` (XXH3 `rom_hash` matches).
- The game shows "WARNING: NO CONTROLLER PAK. THE GAME CANNOT BE SAVED / OK". That is the expected result of
  our "no pak" stubs (`osPfsIsPlug`, `__osGetId`, runtime `osPfs*`).
- Then the title menu: a rotating 3D head with ONE PLAYER / TWO PLAYER GAME / LOAD GAME / SETUP.
- RT64 renders the F3DEX/F3DLP.Rej display lists correctly, and menu input works.

**Fixed: aborts when double-clicked**
- The first attempts died with `0xC0000409` in `ucrtbase.dll`, fast-fail 7, i.e. `abort()`.
- The exe was double-clicked in `build-cmake\`. RecompFrontend resolves `assets/` and `recompcontrollerdb.txt`
  against the working directory (`recompui/src/util/file.cpp`, `get_program_path()` returns `""` on Windows).
- Fix: a POST_BUILD step copies both next to the exe. Tested: launching with cwd `build-cmake\` reaches the
  launcher, with no crash event.

**Known issue:** the game freezes after choosing ONE PLAYER. Not diagnosed yet; next step.

## 2026-10-27 — Phase 3: native Windows build links

Toolchain: VS 2022 Build Tools 17.14.37806.7, Windows SDK 10.0.26100, LLVM 22.1.8 (`clang-cl`), Ninja.
`tools/build.ps1` imports `vcvars64.bat` and builds `build-cmake/Robotron64Recompiled.exe`.

**Recomp input fixes**
- n64sym misnamed `0x80063560` as `osPiWriteIo`. The game reads ROM words with it
  (decomp `rom_files.c`), so it is `osPiReadIo`.
- `0x800617A0` is `__osGetId` (decomp `pfs_repair_id.c`); the second match at `0x80068F44` stays unnamed.
- Both are `OVERRIDES` in `gen_syms.py`. Unresolved calls are now exactly the host functions in
  `src/game/ultra_extensions.cpp`: `osPiRawReadIo` (bounds-checked; boot reads past the ROM end return 0),
  `osPfsIsPlug` (no pak), `__osGetId` (`PFS_ERR_NOPACK`).
- My unresolved-call check only scanned the runtime, not the template. The template already defines
  `osPiReadIo_recomp` (`src/game/recomp_api.cpp`), which gave a duplicate-symbol link error; mine was removed.

**Build failures, in order**
1. GNU `clang++` driver: `clang++: error: no such file or directory: '/W4'`. The template's CMake adds MSVC
   flags (`/W4`, `/SUBSYSTEM`) whenever clang simulates MSVC; it targets `clang-cl` (its `CMakeSettings.json`
   uses `clang_cl_x64`). Switched to `clang-cl`, which also accepts `-march=nehalem` etc.
2. `vcvars64.bat`: "vswhere.exe not found". It calls vswhere from PATH; added the Installer dir to PATH.
3. Patch ELF: `Unknown command line argument '-mips-ssection-threshold=0'`. **The LLVM 22.1.8 Windows release
   has no MIPS backend** (`clang --print-targets`), so no MIPS code at all.
   - Fix: the patch ELF is built in Docker by `tools/recomp.sh` (`make -C patches`, Ubuntu clang/lld).
   - CMake option `PATCHES_BUILD_ELF` (OFF in `build.ps1`) then only recompiles it with the native
     `N64RecompCLI`.
   - `make` is no longer needed on Windows. The patch steps are proper custom commands now, which also removes
     the template's "two builds needed" TODO.
4. SDL 2.30.3 `SDL_endian.h`: `definition of builtin function '_m_prefetch'`. Still the same in SDL 2.32.10.
   Clang 22 made it a builtin. Fix: define `__PRFCHWINTRIN_H` (the guard SDL checks) for clang on Windows.
5. `recomp::start` API changed with the runtime pin bump (ADR 002): it now takes one
   `recomp::Configuration`. `main.cpp` was ported; `argc`/`argv` are now passed through.

**Host glue**
- `rom_hash = 0x41CAB16167C1368C` (XXH3-64, computed with python3-xxhash in the Docker image).
- `save_type = None`, `is_enabled = true`.
- Thread names from the decomp's `osCreateThread` calls.
- Control descriptions from decomp `docs/controller-input-mapping.md`.

## 2026-10-27 — Phase 2: ELF, symbols, recompilers

**Docker image** (`tools/docker/Dockerfile`, ubuntu:24.04 + n64sym `ccf4600`). It builds in about a minute.

**Decomp build** (`tools/build_elf.ps1` → `tools/build_elf.sh`)
- `make verify`: *"Matched all 8388608 bytes; SHA-256 91d85bae…addd"*.
- `make analyze`: spimdisasm 1.42.4 produced 1,536 function records covering 0x6F040 bytes.
- First attempt "failed": Windows PowerShell 5.1 with `$ErrorActionPreference = "Stop"` turns the first native
  stderr line (an IDO `-mips3` warning) into a terminating error. The detached container kept building and
  exited 0. Fix: no Stop preference; run docker through `cmd /c ... 2>&1` and rely on the exit code.

**n64sym**
- 379 distinct addresses across a thorough run (`-t`) and a normal run (`-h 0x7FFFF400`).
- It prints most addresses as `0x70xxxxxx` and some as `0x80xxxxxx` with the same low 24 bits; normalized.
- Most hits outside function starts are data symbols (`osTvType`, `__osPiDevMgr`, …), which are useful later for
  `datasyms`.
- Hits in game code below `0x8005FBB0` are false positives on tiny functions; rejected, see ADR 004.
- **Watch list for Phase 3:** n64sym called `0x800495BC` `__osSpSetPc` and `0x8005D9E0` `__osDisableInt`.
  If these really are copies of hardware-touching code inside game code, they will misbehave when recompiled.

**gen_syms.py**
- 1,539 functions: 1,467 from the decomp, 216 libultra names from n64sym.
- Gaps total 0x520 bytes, all zero padding.
- First run failed its own check: the decomp's `func_80066A60`/`80067048`/`80067350` (`__osException`,
  `handle_CpU`, `__osCleanupThread`) were merged into neighbours by spimdisasm. Fixed by splitting at decomp
  starts.
- Ambiguous pairs: aliases resolved to their public names. `0x800669B0`/`0x80066A00` both call
  `__osSiDeviceBusy`, so they are `__osSiRawReadIo`/`__osSiRawWriteIo`.
- `__osGetId` matched twice (`0x800617A0`, `0x80068F44`), so both were left unnamed.

**Audio microcode**
- Stock `aspMain`: text ROM `0x72910`, size `0xE20`.
- The 16-entry dispatch table at data ROM `0x97BD0+0x10` is identical to the aspMain configs of drmario64_recomp
  and Quest64-Recomp.
- RSPRecomp gives `rsp/aspMain.cpp` (73 KB) with no warnings. The template's `n_aspMain` was renamed throughout.

**N64Recomp** (`tools/recomp.sh`)
- Function count 1539, exit 0. The only message is `[Info] Indirect tail call in recomp_entrypoint`
  (`_start` jumps to `0x80048170` via a register, which is resolved at runtime). 178 `LOOKUP_FUNC` sites,
  70 jump-table switches.
- Unresolved-call check against the runtime: first run had 14 missing `*_recomp` callees.
  - 12 libultra-internal functions are dead code: reachable only from reimplemented functions or each other.
    They are now `ignored` in `robotron64.us.toml` with reasons.
  - **Still unresolved, for Phase 3 to provide:**
    - `osPiRawReadIo` (boot, `func_80048170`; the reads beyond the end of the ROM);
    - `osPiWriteIo` (`func_8004EBE4`/`func_8004EE30`, to investigate);
    - `osPfsIsPlug` (5 game callers);
    - `__osCheckPackId`, `__osContRamRead`, `__osPfsSelectBank`, `__osRepairPackId`. These are called by
      `func_800617A0` (an `__osGetId`-shaped pak function called from game code via `func_8004C0B0`). Plan:
      make that path report "no pak" like the runtime's `osPfs*` stubs.

## 2026-10-27 — Phase 1: skeleton

- `git init` (branch `main`). Imported N64RecompGameTemplate `be3271c` (ADR 001) after running
  its `configure_game.py` in a scratch copy; fixed display names to "Robotron 64" by hand.
  `GameEntry.internal_name` set to the ROM header name `ROBOTRON-64` (to verify in Phase 3 how
  librecomp compares it); `rom_hash` still a placeholder until we confirm which hash librecomp uses.
- Submodules at heads (ADR 002); `git submodule update --init --recursive` succeeded.
  N64ModernRuntime pins N64Recomp `81213c1` (2 commits behind its head; fine).
- Symbol dumps go to `syms/` (gitignored). The template expected a committed `*RecompSyms` repo;
  we generate them locally from the decomp ELF instead (CLAUDE.md rule 1).
- `tools/check_rom.ps1` / `.sh`: tested on the US ROM (OK), EU ROM (mismatch, exit 1) and a
  byte-swapped copy of the US ROM (converted → OK).
- Environment: LLVM 22.1.8 at `C:\Program Files\LLVM\bin` (not on PATH; has clang, clang-cl,
  ld.lld, llvm-objcopy). **No Visual Studio / Windows SDK installed** — needed for linking on
  Windows (user installing VS 2022 Build Tools). No WSL distro; Docker Desktop 29.7.2 available →
  decomp builds in Docker (ADR 003).

## 2026-10-27 — Phase 0: research

See [RESEARCH.md](RESEARCH.md). Key outcomes:
- No existing Robotron 64 recomp.
- First local ROM (`285c720f…`) was a bad dump: same header/CRCs, 227 bytes differ in the
  asset area `0x4726BC`–`0x47ADFF`. User supplied the correct dump (`44d158bc…`, matches decomp).
- Decomp builds a matching ELF + map but ~128 KB of code is unmatched `.incbin` with no
  symbols — main technical risk for Phase 2.
