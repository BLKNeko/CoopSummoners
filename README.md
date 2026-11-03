# CoopSummoners

**Local co-op for _Fortune Summoners: Secret of the Elemental Stone_.**
A second player takes control of a party member that the AI would otherwise drive, on the
same PC, with a second gamepad or the numpad.

In the base game you control one character and the AI controls the others. CoopSummoners
lets a friend take over one of those allies: you explore, fight and cast spells together on
one screen.

> Unofficial fan project. Not affiliated with Lizsoft or Carpe Fulgur. **No game files are
> included:** you need your own copy of the game.

---

## Contents

- [Features](#features)
- [Requirements](#requirements)
- [Install the co-op mod](#install-the-co-op-mod)
- [Controls](#controls)
- [How co-op works in game](#how-co-op-works-in-game)
- [Playing online with friends](#playing-online-with-friends)
- [Known limits](#known-limits)
- [Troubleshooting](#troubleshooting)
- [Uninstall](#uninstall)
- [Building from source](#building-from-source)
- [How it works (technical)](#how-it-works-technical)
- [Tests](#tests)
- [Credits](#credits)
- [License](#license)

---

## Features

- **A launcher** (`CoopSummoners.exe`) that finds the game in Steam, installs the mod in one
  click, backs up your saves, shows which gamepad is which player, and starts the game solo
  or in co-op. English and Portuguese.
- **Player 2 controls a party ally** (Sana, and later Stella) instead of the AI, with the
  numpad or a second gamepad.
- **Player 2 starts in the game** and takes the first free party member automatically.
- **Swap characters LEGO-style**: both players press *switch* within half a second and
  trade characters.
- **Drop out / drop in**: player 2 holds *switch* for 1.5 s to leave (the AI takes the
  character back) and again to rejoin.
- **Fallen characters stay with their player**: a player at 0 HP can still walk, jump and
  investigate, but cannot attack or cast until healed. Game over only when the whole party
  is down, as in the base game.
- **"P2" marker** above player 2's character, and an arrow on the screen edge when player 2
  is off screen.
- **Spell panel for player 2**: when player 2 drives a caster, their spells show in the
  top-right panel.
- **Gamepad hot-plug**: a pad that is unplugged and plugged back in works again within a
  moment.
- **Off by default**: without `--coop` the game runs exactly like the base game.

---

## Requirements

| | |
|---|---|
| Game | **Fortune Summoners: Secret of the Elemental Stone**, Steam 2012 English edition ([store page](https://store.steampowered.com/app/203510/Fortune_Summoners/), app 203510). The stock Steam files work as they are. |
| Not supported | The 2020 **Special Edition** (app 1381770) and the Japanese release. |
| OS | Windows 10 or 11. |
| Controllers | Player 1: keyboard and/or gamepad. Player 2: the numpad and/or a second gamepad. |

Tested with Steam build 29910, `sotes.exe` SHA-256
`7D779F2EB02B3C603857FEDBC52BE6973AC3B0B2C5C1BC696122DDAC89FB9F1B`.

To check your copy, run this in PowerShell inside the game folder:

```powershell
Get-FileHash .\sotes.exe -Algorithm SHA256
```

---

## Install the co-op mod

The mod is a single program, `opensummoners-standalone.exe`, that runs the game in place of
`sotes.exe`. It reads the data from your own game files and **never changes them**.

### Easiest: the CoopSummoners launcher

1. Download **`CoopSummoners.zip`** from the
   [**Releases**](https://github.com/BLKNeko/CoopSummoners/releases) page.
2. Extract the **whole zip** into any folder (keep `CoopSummoners.exe` and
   `opensummoners-standalone.exe` together).
3. Run **`CoopSummoners.exe`**. It finds Fortune Summoners in your Steam libraries and
   checks it is the supported version. If it can't find it, click **Browse...** and choose
   the game folder (the one with `sotes.exe`).
4. Click **Install mod**. The launcher copies the mod into the game folder and, the first
   time, backs up your saves (`user\` → `user-backup-<date>`).
5. Plug in the gamepads and click **Play Co-op** (or **Play Solo**). The game's own options
   window appears as usual: choose the screen mode and click Launch.

![the CoopSummoners launcher](docs/media/coop-launcher.png)

The launcher also:

- lists the **connected gamepads** in the same order the game sees them, so you know which
  one is player 1 and which is player 2;
- shows the full **controls** (button **Controls...**);
- **updates** the mod when you extract a newer zip over the old one (button **Update mod**);
- **uninstalls** it (button **Uninstall**): it removes only the mod's files, and your saves
  and backups stay;
- shows its text in **English or Portuguese**, depending on the Windows language.

It remembers the game folder in `%APPDATA%\CoopSummoners\launcher.ini`.

### Manual install

If you prefer not to use the launcher, or want the mod in a separate folder instead of the
Steam one:

#### Step 1: get `opensummoners-standalone.exe`

- **Download** it from this repository's
  [**Releases**](https://github.com/BLKNeko/CoopSummoners/releases) page, **or**
- **build it** yourself (see [Building from source](#building-from-source)).

> Do not use the `opensummoners-standalone.exe` from the upstream OpenSummoners project:
> it has no co-op.

#### Step 2: make a play folder

You can put the exe straight into the Steam game folder, but a separate copy keeps your
Steam install untouched.

1. Open the game folder. In Steam: right-click **Fortune Summoners** → **Manage** →
   **Browse local files**. The default location is
   `C:\Program Files (x86)\Steam\steamapps\common\Fortune Summoners`.
2. Create a new folder, for example `C:\Games\FortuneSummonersCoop`.
3. Copy these 5 files from the game folder into the new folder:

   ```
   sotes.exe
   sotesd.dll
   sotesw.dll
   sotesp.dll
   lizsoft.spl
   ```

4. **(Optional) keep your progress:** also copy the game folder's `user` folder (your
   saves and settings) into the new folder.

#### Step 3: add the mod

Copy `opensummoners-standalone.exe` into the same folder. It should now contain:

```
FortuneSummonersCoop\
├── opensummoners-standalone.exe   ← the mod
├── sotes.exe
├── sotesd.dll
├── sotesw.dll
├── sotesp.dll
├── lizsoft.spl
└── user\                          ← (optional) your saves
```

#### Step 4: create the shortcuts

Co-op is turned on with the `--coop` option. The easiest way is two small `.bat` files in
the same folder.

1. In the folder, right-click → **New** → **Text Document**. Name it `Play Coop.bat`
   (turn on *View → File name extensions* in Explorer so the name doesn't end in `.txt`).
2. Open it in Notepad, paste the following and save:

   ```bat
   @echo off
   cd /d "%~dp0"
   start "" opensummoners-standalone.exe --coop
   ```

3. (Optional) make `Play Solo.bat` the same way, for the normal single-player game:

   ```bat
   @echo off
   cd /d "%~dp0"
   start "" opensummoners-standalone.exe
   ```

**Alternatives to the `.bat` file:**

- **Desktop shortcut:** right-click the exe → **Create shortcut** → shortcut **Properties**.
  Under **Target**, add ` --coop` after the closing quote, for example:
  `"C:\Games\FortuneSummonersCoop\opensummoners-standalone.exe" --coop`.
- **Steam:** **Games** → **Add a Non-Steam Game to My Library** → pick
  `opensummoners-standalone.exe` → in its **Properties**, set **Launch Options** to `--coop`.
- **Environment variable:** `OPENSUMMONERS_COOP=1` has the same effect as `--coop`.

#### Step 5: play

1. Plug in the gamepads **before** starting the game. The first pad Windows lists is
   player 1's and the second is player 2's.
2. Double-click `Play Coop.bat`.
3. The game's usual launcher window appears: choose the screen mode and click Launch.
4. Start a new game or load a save. Player 2 joins automatically as soon as an ally is in
   the party. At the very start of a new game Arche is alone, so player 2 joins when Sana
   joins the party.

Saves and settings are written to the play folder's `user\` folder. A log and an audit
file go to `opensummoners-standalone\`, and gamepad messages go to `coop.log`.

---

## Controls

| Action | Player 1 | Player 2 |
|---|---|---|
| Device | keyboard (the game's key config) + gamepad 1 | numpad + gamepad 2 |
| Move | arrow keys | numpad **8 / 5 / 4 / 6** (up / down / left / right) |
| Attack | game config (default **X**) | numpad **1** |
| Jump | game config (default **C**) | numpad **2** |
| Use / investigate | game config | numpad **3** |
| Sheathe / skill modifier | game config | numpad **0** |
| Switch character | **Enter** / game config | numpad **+** |

- **Both gamepads use the button layout from the game's own key configuration** (set it in
  the game's options).
- The numpad keys work whether Num Lock is on or off.
- To cast or use a skill, player 2 holds numpad **0** with a direction, just as player 1 does
  with their modifier key.

---

## How co-op works in game

### Who controls what

- **Player 1 is always the leader**: the camera follows player 1's character, and player 1
  talks to people, opens menus and reads signs.
- **Player 2** controls the first party member that is free (not the leader, and not
  knocked down).
- **During cutscenes and dialogue**, player 2's character goes back to the AI for the length
  of the scene, then returns to player 2.

### The switch key

| What you do | What happens |
|---|---|
| **Both players tap switch within 0.5 s** | The two players **swap characters**, even if one of them is down. Player 1 still leads. |
| **One player taps switch alone** | After the 0.5 s window, the base game's normal switch to a free AI member (nothing happens if there is none). |
| **Player 2 holds switch for 1.5 s** | **Player 2 leaves**: the AI takes the character back. Hold again to **rejoin** with the first free member. |

### Falling in battle

- A character at 0 HP is **"Disabled"**, as in the base game: healed by items or by
  returning to town.
- **A fallen character stays with its player.** Player 1 never takes player 2's character.
  If a free AI member exists, the base game's rule still moves player 1 to it.
- A player's fallen character **can walk, jump and investigate, but cannot attack or cast.**
- **Game over** only when the whole party is down, as in the base game.

### On screen

- A blue **P2** badge points at player 2's character. When that character is off the left or
  right of the screen, an arrow on that edge shows where they are.
- The **spell panel** (top right) shows up whenever a player controls a caster. If player 1's
  leader does not cast, it shows player 2's spells; player 1's portrait stays where it is.

---

## Playing online with friends

CoopSummoners is **local** co-op: everyone plays on the PC that runs the game. To play
with friends far away, stream the game from the host PC:

- **[Parsec](https://parsec.app/)**: the host runs the game, friends connect and their
  gamepads show up as extra pads on the host. It is free for this use.
- **Steam Remote Play Together** usually works only with games bought on Steam. The mod is
  a separate program (even when added as a non-Steam game), so it will probably not work.

Neither option has been tested with the mod yet. Real networked co-op (each player on
their own PC) is planned as later work.

---

## Known limits

- **One camera**, on player 1. Player 2 can walk off screen (the edge arrow helps).
- **Player 2 cannot** open menus, talk to people or read signs: player 1 does those.
- **Two players.** A third player (when Stella joins) is not supported yet.
- **One spell panel.** If both players control casters, it shows player 1's.
- **Not tested yet:** saving and loading with co-op on, every cutscene, streaming over
  Parsec.
- Only the **2012 English edition** is supported.

Found a bug? Open an issue with what happened, the save if you can, and the
`coop.log` / `opensummoners-standalone\` files from your play folder.

---

## Troubleshooting

| Problem | What to try |
|---|---|
| Windows SmartScreen blocks the exe | The exes (`CoopSummoners.exe`, `opensummoners-standalone.exe`) aren't signed. Click **More info** → **Run anyway**, but only for files you downloaded from this repository's Releases or built yourself. |
| The launcher can't find the game | Click **Browse...** and choose the folder that has `sotes.exe` (in Steam: right-click the game → **Manage** → **Browse local files** shows it). |
| "Install mod" says access is denied | Close the game if it's running and try again. If the game is under `C:\Program Files`, right-click `CoopSummoners.exe` → **Run as administrator**. |
| Players 1 and 2 have the wrong pads in the launcher's list | Same fix as below: the list shows the order the game will use. |
| Player 2 can't do anything | Is there a second party member yet? In a new game Arche starts alone. Has player 2 left by holding switch? Hold numpad **+** for 1.5 s to rejoin. |
| A gamepad does nothing | Plug both pads in **before** starting the game. A pad unplugged mid-game reconnects within a moment; check `coop.log` if it doesn't. |
| Players 1 and 2 have the wrong pads | Windows decides the order. Unplug both, plug player 1's pad in first, then player 2's, and start the game again. |
| The game starts without co-op | Check that `--coop` is in the `.bat` / shortcut / Steam launch options. |
| The game won't start at all | Check the 5 game files are in the folder next to the exe, and that `sotes.exe` is the 2012 English Steam edition (see [Requirements](#requirements)). |

---

## Uninstall

- **With the launcher:** click **Uninstall**, then delete the launcher's folder. Your saves
  (`user\`) and the save backups (`user-backup-<date>`) stay in the game folder.
- **Manual install:** delete the play folder. If you put the exe straight into the Steam
  game folder, delete `opensummoners-standalone.exe`, the `.bat` files, `coop.log` and the
  `opensummoners-standalone\` folder from it.

The original game files are never changed. (`temp\` and `fs_boot.log` in the game folder
belong to the game itself; the original creates them too.)

---

## Building from source

Everything builds on Linux or **WSL2 on Windows**, inside a Nix shell with the mingw-w64
cross compiler. No game files are needed to build.

### On Windows (WSL2)

1. **Install WSL2 + Ubuntu** in PowerShell as administrator, then reboot:

   ```powershell
   wsl --install -d Ubuntu
   ```

2. **Install Nix** (single-user) in the Ubuntu terminal, and turn on flakes:

   ```bash
   sh <(curl -L https://nixos.org/nix/install) --no-daemon
   mkdir -p ~/.config/nix
   echo "experimental-features = nix-command flakes" >> ~/.config/nix/nix.conf
   ```

   Close and reopen the terminal (or run `. ~/.nix-profile/etc/profile.d/nix.sh`).

3. **Clone inside the Linux file system** (not under `/mnt/c`: it is much slower there, and
   Windows line endings break the build):

   ```bash
   git clone https://github.com/BLKNeko/CoopSummoners.git ~/CoopSummoners
   cd ~/CoopSummoners
   ```

4. **Build:**

   ```bash
   nix develop .#ci --command bash tools/rhost/build.sh
   ```

   The first run downloads the toolchain and takes a few minutes. The result is
   `build/rhost/opensummoners-standalone.exe` (the game, with co-op) and
   `build/rhost/rhost.exe` (the test host).

5. **Copy the exe to Windows**, for example:

   ```bash
   cp build/rhost/opensummoners-standalone.exe /mnt/c/Games/FortuneSummonersCoop/
   ```

6. **The launcher and the release zip:**

   ```bash
   nix develop .#ci --command bash tools/launcher/build.sh --package
   ```

   This gives `build/launcher/CoopSummoners.exe` and `build/launcher/CoopSummoners.zip`
   (the launcher, the mod exe and a short README.txt in English and Portuguese, with no game
   files). Upload the zip to a GitHub release. The launcher accepts `--lang en` / `--lang pt`
   to override the Windows language.

The full pre-push check (the port, the tools and the host test suite) is
`nix develop --command bash tools/ci/build_all.sh`.

---

## How it works (technical)

CoopSummoners is built on **[OpenSummoners](https://github.com/Francesco149/OpenSummoners)**,
an open-source C reimplementation of `sotes.exe` by its own author. OpenSummoners runs the
game entirely on its own code, using only the *data* from your game files. Because the whole
engine is readable C, co-op can be added to the game's own logic instead of being patched
into the original, DRM-protected 32-bit binary.

The engine already had the pieces co-op needs:

- Each room keeps an array of **input managers** (`wl_room.nin` / `.in`); the base game
  creates only one.
- Each party slot holds the **manager that drives it** (`wl_pslot.in`), and the game's
  `pslot_input_set` (0x4cc5a0) switches a member between player control (`char_ai`) and
  the party AI (`char_behave`).
- The leader switch (`room_leader_next`) only picks members with no manager, so a member
  held by player 2 is never taken.

With co-op on, the room gets a **second manager** (the numpad map + joystick slot 1), and
`coop_tick` attaches it to a free party member while player 1 is in control.

| File | What it does |
|---|---|
| `src/coop.c`, `src/coop.h` | Everything co-op: player 2's input, attach/release, the switch rules, the fallen-character filter, the spell panel choice, the P2 marker. |
| `src/scene_loop.c` | Creates the second input manager when co-op is on. |
| `src/room_loop.c` | Feeds player 2's keyboard map; runs `coop_tick`; filters a fallen player's commands. |
| `src/input_dev.c` | Gamepad re-acquire after hot-plug (co-op only). |
| `src/render_draw.c` | The spell panel can show player 2's caster. |
| `src/char_state.c` | NULL guard in `char_add_hp` (a fallen player 2 falling into a pit crashed). |
| `tools/rhost/` | The `--coop` option, the P2 marker overlay drawn before each Flip, and test-only options. |
| `tools/launcher/` | `CoopSummoners.exe`: finds the game in Steam (registry + `libraryfolders.vdf`), checks `sotes.exe` by SHA-256, installs / updates / removes the mod exe in the game folder, backs up `user\`, lists gamepads in the game's DirectInput 7 order, and starts the game with or without `--coop`. Not part of the port. |

Every change is behind `g_coop`: **without `--coop` the game is identical to upstream
OpenSummoners** (frame, tick, random-number state and every frame's pixels on the recorded
scenarios).

To learn how the port itself works (the engine, rhost, verification against the original),
see the [OpenSummoners README](https://github.com/Francesco149/OpenSummoners#readme) and
[`docs/`](docs/) (`docs/standalone.md`, `docs/STATUS.md`, `docs/findings/`).

---

## Tests

The tests use OpenSummoners' **rhost**, which replays recorded keyboard input with a virtual
clock, so a run is the same every time.

- **Co-op off = upstream:** `tools/coop/gate.ps1` runs the recorded scenarios
  (`tests/scenarios/rh-*`) through `rhost.exe --standalone` on a Windows PC. With co-op off,
  frame, tick, random-number state and framebuffer hash match upstream OpenSummoners on all
  10 fresh-start scenarios.
- **Co-op behaviour:** traces in `tools/coop/tests/` (T1–T5: swap, lone taps, drop-out and
  rejoin, late tap; F1: falls, the fallen-player filter, the pit crash). Their expected
  results are in [`tools/coop/tests/README.md`](tools/coop/tests/README.md).
- **Test-only options:** `--coop-test-hp FRAME:WHO:HP` sets a character's HP and
  `--coop-test-drop FRAME:WHO` drops one below the map (WHO: 0 = leader, 1 = player 2).
- `tools/coop/trace_p2.py` strips the numpad from a recorded trace and overlays player 2's
  key holds.

Example (from PowerShell, with the repository in WSL):

```powershell
.\tools\coop\gate.ps1 -Repo \\wsl.localhost\Ubuntu\home\<you>\CoopSummoners `
    -Game C:\fs-base -Out C:\fs-gate -Coop
```

Some things can only be tested by hand: real gamepads (rhost has no joysticks) and long
play sessions.

---

## Credits

- **CoopSummoners** (the co-op mod): BLKNeko.
- **[OpenSummoners](https://github.com/Francesco149/OpenSummoners)** by Francesco149 and
  contributors: the C reimplementation of the engine this mod is built on (MIT). Without
  it this mod would not exist.
- **Fortune Summoners** by **Lizsoft**, English localization by **Carpe Fulgur**. Please
  support them: buy the game on
  [Steam](https://store.steampowered.com/app/203510/Fortune_Summoners/) (2012 edition, the
  one this mod needs) or the
  [Special Edition](https://store.steampowered.com/app/1381770/Fortune_Summoners_Special_Edition/).
- The **Fortune Summoners Fan Discord** ([invite](https://discord.gg/N68c7pt)) for its
  community reverse-engineering work, which OpenSummoners cross-checked against.

---

## License

MIT, see [`LICENSE`](LICENSE). It covers OpenSummoners' code and the CoopSummoners co-op
changes only. **No rights are granted to the original game or its assets**, and none are
included in this repository or its releases.
