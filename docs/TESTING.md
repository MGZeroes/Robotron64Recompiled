# Play-test checklist

Run through this after any change to hooks, patches, submodule pins or the runtime glue. Note the date,
commit and settings in docs/NOTES.md; record failures there with a screenshot description and, for hangs,
a WinDbg stack (`cdbX64.exe -pv -p <pid> -y build-cmake -c "~*kc 30; qd"`).

Status key: ✅ works · ❌ broken · ⬜ not tested yet. Last full pass: 2026-10-28, tag `v0.2-playable`
(35 minutes, 26 waves, no crash).

**Shortcut:** password `BQJDNQMBHC` (LOAD GAME) starts at wave 21 with 155000 points, just after the
wave-20 bonus stage. Good for testing later waves quickly.

## Startup
- ✅ Launcher opens when double-clicking the exe (assets copied next to it)
- ✅ ROM selection accepts `baserom.us.z64`
- ✅ Start after 2+ minutes idle in the launcher enters the game straight away
- ✅ "No Controller Pak" warning, then the Crave intro (matches N64 footage), then the title menu

## Menus
- ✅ Title menu: rotating head, ONE PLAYER / TWO PLAYER / LOAD GAME / SETUP
- ⬜ SETUP: every option changes and returns
- ✅ LOAD GAME / password entry
- ✅ Pause (Start) and resume in game

## Gameplay
- ✅ ONE PLAYER starts (music switch, transition screen "PLAYER 1 UP")
- ✅ Waves 1–26: movement, firing, rescuing humans, scoring, extra lives HUD
- ✅ Bonus stage (every 20 waves)
- ✅ Losing all lives → game over
- ⬜ TWO PLAYER (alternating turns)
- ✅ Long session (35 min) without a hang or crash

## Controls
- ✅ Left stick moves, right stick fires (8 directions, as on the N64: the game only reads direction bits)
- ✅ Keyboard: movement and arrow-key firing

## Audio
- ✅ Music and effects play without crackling or drop-outs
- ✅ Music switches correctly between menu, waves and transitions

## Graphics (compare with N64 footage)
- ✅ Original aspect / original frame rate: matches the N64
- ✅ Framerate setting: no visible change and no bugs (expected: higher rates need interpolation patches)
- ⬜ Transition trail effect: looks inconsistent between transitions; not yet compared closely with the N64 (deferred)
- ❌ Expand aspect: background box (known, needs a widescreen patch; docs/GAME.md)

## Saving
- ❌ Controller Pak: not implemented yet (runtime stubs report no pak)
