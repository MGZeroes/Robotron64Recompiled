# 006 — One controller per player

Date: 2026-10-29. Status: accepted.

## Context
Simultaneous two-player (play style 3) worked, but one PC pad drove every N64 port, so both robots moved
together. The game itself uses four ports, two per player: P1 moves with port 0 and aims with port 1, P2 uses
ports 2 and 3. P2 is bound to port 2 only if that port is connected when the options load (decomp
`player/behavior.c`, `session_input_initialize.c`; see GAME.md Input).

RecompFrontend (`2ddbfc8`) already has local multiplayer: player slots (`recompinput::players`), an Assign
Players screen in the Controls tab, profiles per pad GUID, and per-player right stick and rumble. It treats an
N64 port number as the player number, and assignments are not saved between sessions.

Prior art: WCW vs. nWo World Tour (always multiplayer, forked RecompFrontend with auto-assign), Rush 2049
(option that switches `set_single_player_mode`, patch for join-on-first-press). Upstream's own
auto-open of the Assign Players screen is in the unmerged PR #45.

## Options
- **Joining:** (a) upstream's Assign Players screen as-is; (b) carry a RecompFrontend patch for
  join-on-first-press.
- **Port layout:** (a) keep the game's four ports and map players host-side; (b) patch the game so P2 reads
  port 1 and aiming comes from a hook (5–6 game hooks, including per-frame player code and the menus, and the
  game's control-setup screen would no longer match).

## Decision (by the user)
- **Joining (a)**, behind a new option **Controllers: Shared / One per player** (default Shared). Its change
  callback calls `set_single_player_mode`, as in Rush 2049.
- **Port layout (a).** `main.cpp`: all four ports always report a controller (stable port bindings); port N
  is fed by player N/2, or by player 1 when that slot has no pad; odd ports carry the player's right stick when
  Analog Aiming is on; rumble from port 0/2 goes to that player's pad.
- **Profile fix-up.** The assignment commit only sets the profile of the assigned device, so the keyboard
  leaked into both players (a controller player keeps Keyboard (SP); a keyboard player gets an empty profile
  and its controller profile stays at index 0, which is Keyboard (SP)). `update_player_profiles` in `main.cpp`
  re-applies the profiles after each finished assignment (game thread, after two stable polls): the keyboard
  drives only the player it was assigned to, with their "Keyboard (Player N)" profile, or player 1 when
  nobody took it. Those profiles are created empty upstream; `init_config` fills them once from the normal
  keyboard bindings, so the player card shows the profile that is really used and it can be edited.
- The Assign Players screen is laid out for four players, so with two the cards sit at the far edges. Left as
  upstream draws it.

## Consequences
- No RecompFrontend patch and no game hooks for this feature.
- Players reassign each session; in One per player mode a pad does nothing until assigned (it can still open
  the menu). Each pad starts with the default mapping, not the Shared-mode customisations.
- When the pin moves past PR #45, the Assign Players screen can open by itself; then revisit the fix-up, in
  case upstream fixes the profile defaults.
