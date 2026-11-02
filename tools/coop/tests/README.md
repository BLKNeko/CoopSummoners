# Co-op switch tests (rhost traces)

Each folder is a trace that loads save "3: Town of Tonkiness" (Arche + Sana) from the save set
derived from the rh-early-game chain (`gate.ps1 -UserDir`), then presses the switch key:
Enter = P1 (DIK 28), numpad + = P2 (DIK 0x4e). Run with `gate.ps1 -Coop -Extra "--log-every 25"`
and read the `coop[...]` status lines.

| Trace | Keys | Expected (2026-11-02 result) |
|---|---|---|
| T1-swap | P1 tap f4500, P2 tap f4510 | lead 50011 (Sana, P1), P2 on 50010 (Arche) at f4525 — PASS |
| T2-p1-lone | P1 tap f4500 | no free member: no change — PASS |
| T3-p2-lone | P2 tap f4500 | no change — PASS |
| T4-drop | P2 hold f4500-4620, P1 tap f4800, P2 hold f5000-5120 | p2out=1 at f4600; P1 to Sana at f4825; P2 back on Arche at f5100 — PASS |
| T5-late | P1 tap f4500, P2 tap f4545 (720 ms) | no swap — PASS |

Falls: `F1-fall` = `rh-combat` with numpad stripped (`trace_p2.py`), no keys after f28500; run
with `--coop-test-hp 27050:0:1` (test only), no save folder:
Arche down at f27130, P1 stays on her, P2 keeps Sana; both down at f28510 → Game Over screen
(f29200). With taps at f27300/f27305 the two swap: P1 on the healthy member, P2 on fallen Arche.
Fallen-member filter (`--frames 28500 --log-every 2`): while Arche is down, `act14` (attack) and
skills never appear in the status line, she still moves (x 0..152288); the run equals the
unfiltered one up to the fall frame — PASS.

Pit fall with a fallen P2 (owner's crash, 2026-11-02): `F1-fall` with `--frames 29200 --log-every 10
--coop-test-hp 27050:1:1 --coop-test-drop 28400:1 --coop-test-drop 28600:1 --coop-test-drop 28800:0`.
Sana falls at f28410 (the first drop), the second drop puts the Disabled Sana below the map.
Before the fix: `AV f28601 ... (write 00000000)` (char_add_hp writing through char_apply's NULL).
After: no crash, Sana respawns on the ground at f28610 with 0 HP; healthy Arche dropped at f28800
respawns and loses 5% HP (77 → 72) as in the base game — PASS.
