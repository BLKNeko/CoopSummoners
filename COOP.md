# CoopSummoners

Local co-op for **Fortune Summoners: Secret of the Elemental Stone** (Steam 2012 English
edition, app 203510, build 29910), built on [OpenSummoners](https://github.com/Francesco149/OpenSummoners)
(MIT). A second player controls a party ally instead of the AI.

Unofficial fan project. Not affiliated with Lizsoft or Carpe Fulgur. No game files are included:
you need your own copy of the game.

## Play

Build `opensummoners-standalone.exe` (see README.md, *Building*), copy it next to your
`sotes.exe`, `sotesd.dll`, `sotesw.dll`, `sotesp.dll`, `lizsoft.spl`, and run:

    opensummoners-standalone.exe --coop

(or set `OPENSUMMONERS_COOP=1`). Without `--coop` the game behaves exactly like upstream.

| | Player 1 | Player 2 |
|---|---|---|
| Device | keyboard (game's own key config) + gamepad 1 | numpad + gamepad 2 |
| Move | arrows | numpad 8 / 5 / 4 / 6 |
| Attack / Jump / Use | game config (default X / C / …) | numpad 1 / 2 / 3 |
| Sheathe, skill modifier | game config | numpad 0 |
| Switch character | Enter / config | numpad + |

Player 2 starts in the game and takes the first free party member while player 1 is in
control, and is released during cutscenes.

**Switch key (co-op rules):**

| | Result |
|---|---|
| Both players tap within 0.5 s | They swap characters, also when one is fallen. Player 1's character is always the leader (camera, talk, HUD). |
| One player taps alone | After the 0.5 s window, the game's own switch to a free member (nothing if none). |
| Player 2 holds 1.5 s | Player 2 leaves: the AI takes the character back. Hold again to rejoin (first free member). |

A fallen member stays with its player (player 1 does not take player 2's character). With
a free AI member, the game's own rule still moves player 1 to it. Game over only when the
whole party is down, as in the base game. A player's fallen character (0 HP, "Disabled")
can still walk, jump and use/investigate, but cannot attack or cast (`coop_filter_cmd`).

A blue **P2** badge points at player 2's character. When the character is off the left or
right of the screen, an arrow on that edge (with the badge) shows where they are. It is an
overlay drawn on the finished frame (`coop_overlay_draw`, called by the host right before
each Flip), so it never changes game state.

The spell panel (top right: the spells and the selected one) appears whenever a player
controls Sana, player 1 or player 2. When player 1's leader does not cast, it shows player
2's caster (`coop_spell_panel_slot`); the leader portrait stays player 1's. It disappears
only when the caster is driven by the AI. If both players control casters, it shows player
1's (one panel).

## How it works

`src/coop.c`. The engine already keeps one input manager per party slot
(`wl_pslot.in`) and a per-room array of managers (`wl_room.nin` / `.in`); retail creates
one. With co-op on the room gets a second manager (numpad map + joystick slot 1), and
`coop_tick` attaches it with the game's own `pslot_input_set` (0x4cc5a0).

## Tests

- `tools/coop/trace_p2.py`: strip the numpad from a recorded rhost trace, overlay P2 key holds.
- `tools/coop/gate.ps1`: run fresh-start scenarios through `rhost.exe --standalone` on a local
  Windows machine and compare with `expected.json`.
- Co-op off: identical to upstream 7ed6e55 (frame, tick, rng, framebuffer hash) on 10 scenarios.
- Co-op on, rh-combat A/B: P2 walks and jumps on numpad input; P1 unchanged until the
  two interact.

## Gamepads

Tested by hand with two physical pads (P1 and P2, combat included). A pad that is unplugged
and plugged back in is picked up again within a fraction of a second (co-op only; retail
re-acquired only on DIERR_INPUTLOST, so it needed a window minimize / restore). Each new
read failure code is written once to `coop.log` in the game folder.

## Known limits

One camera (follows player 1), player 2 cannot open menus or talk, save/load with co-op
not tested yet.