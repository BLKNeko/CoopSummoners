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

Player 2 takes the first free party member while player 1 is in control, and is released
during cutscenes.

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