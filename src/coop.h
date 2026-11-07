/*
 * coop.{c,h} — local co-op: players 2 and 3 drive party allies (not a retail function; off
 * unless enabled, so every retail path and the parity corpus are unchanged).
 *
 * The engine already has the pieces: a room owns an array of input managers (room+0x0
 * count, +0x4 array; retail allocates one, fed by the keyboard and joystick 0), each party
 * slot carries the manager that drives it (pslot+0x0), and pslot_input_set 0x4cc5a0 flips a
 * member between player control (char+0x200 = 0, char_ai) and the behaviour AI (= 1).  The
 * leader switch (room_leader_next) only picks members with no manager, so a member held by
 * another player is never taken.
 *
 * With co-op on, the room gets COOP_NIN managers: [0] player 1 (the game's own producers and key
 * config), [1] player 2, [2] player 3, each fed by co-op's producers from the player's device
 * (the keyboard with that player's keys, or a gamepad with that player's buttons; see
 * coop_controls).  coop_tick attaches a joined player to a free member while player 1 holds the
 * leader, and releases them whenever player 1 does not (cutscenes and scripts take the leader's
 * input; the others follow the same rule); after that they get their last member back.
 *
 * The switch key is co-op's (coop.c, "the switch key"): two players tap within 0.5 s → they swap
 * characters (any pair; player 1 keeps the lead); a lone tap → the game's own switch to a free
 * member once the window has passed; player 2 / 3 holds it ~1 s while out → joins (the first
 * free member; none free → stays out), holds it 1.5 s while in → leaves (the AI takes over);
 * player 1 holds it 5 s → every other player is dropped out.  Two players may want the same
 * single free ally: whoever holds first gets it, the other waits until a member is free.
 * A fallen member keeps its player; game over stays the game's own (all down).
 *
 * Default keys without a controls file: player 2 = the numpad (8/5/4/6 move, 1 attack, 2 jump,
 * 3 use, 0 sheathe / skill modifier, + switch) and the 2nd gamepad; player 3 = the 3rd gamepad.
 */
#ifndef OSS_COOP_H
#define OSS_COOP_H

#include <stddef.h>
#include <stdint.h>

#include "world_layout.h"

#define COOP_NIN 3   /* input managers with co-op on: players 1..3 */

/* 0 = off (retail), 1 = on.  Set by rhost's --coop or OPENSUMMONERS_COOP=1. */
extern int g_coop;
void coop_init_from_env(void);

/* ---- the players' devices and keys (rhost reads them from the launcher's coop-controls.ini) ---- */
enum { COOP_A_UP, COOP_A_DOWN, COOP_A_LEFT, COOP_A_RIGHT, COOP_A_ATTACK, COOP_A_JUMP, COOP_A_USE,
       COOP_A_MOD, COOP_A_SWITCH, COOP_NACT };
typedef struct coop_player_ctl {
    int     kb;               /* the keyboard feeds this player (keys[] below; player 1: the game's keys) */
    int     pad;              /* a gamepad feeds it: 1 = the one named by pad_guid / pad_name / pad_index */
    uint8_t pad_guid[16];     /* its DirectInput instance GUID (all zero: none) */
    char    pad_name[64];     /* ... else its instance name */
    int     pad_index;        /* ... else its enumeration index */
    uint8_t keys[COOP_NACT];  /* DIK codes (players 2 / 3) */
    uint8_t buttons[COOP_NACT];   /* gamepad button numbers for ATTACK..SWITCH (directions: stick + d-pad) */
} coop_player_ctl;
/* the defaults (no controls file): P1 keyboard + the game's first gamepad, P2 numpad + 2nd gamepad,
 * P3 the 3rd gamepad */
void coop_controls_default(coop_player_ctl ctl[COOP_NIN]);
/* a controls file was read: from now on the game's joystick slots follow player 1's choice (solo too) */
void coop_set_controls(const coop_player_ctl ctl[COOP_NIN]);
/* the game creates its joystick slot `slot` (sys+0x287c / +0x2880) from enumeration index `index`
 * (id_5ba120, after its EnumDevices): the index to open instead, or -2 = no device.  Unchanged without
 * a controls file; with one, slot 0 = player 1's gamepad (none for "keyboard only"), slot 1 = none
 * (players 2 / 3 open their own) */
int32_t coop_game_pad_index(intptr_t dinput, intptr_t slot, int32_t index);

/* Once per room update (the room loop, right after the party's pickups room_pickups 0x59ca90, so only
 * while the game runs: not in menus or cutscenes): the lights of players 2 / 3 who play no character.
 * Directions fly the light (through walls, kept on screen); it takes the coins it touches (gold as the
 * party's pickup takes it; never items); hold attack = a magnet pulling the coins near it; jump / use =
 * a ping ring.  It appears at the leader on the first input and goes after 10 s without any.  Each
 * player's coins this session are counted under the light. */
void coop_light_tick(wl_room *r);
/* the number of input managers a room gets (1 off, COOP_NIN on) */
uint16_t coop_room_inputs(void);
/* configure the managers after input_init(r, 0, ...): player 1's devices; with co-op, [1..] co-op's */
void coop_inputs_init(wl_room *r);
/* co-op's producer for manager `p` (src_kb == 2): player p's keyboard keys and gamepad */
void coop_produce(wl_room *r, uint16_t p, const void *kb);
/* once per room input update, after the managers are fed: the switch key, attach / release */
void coop_tick(wl_room *r);
/* the slot player p (1..) holds, or NULL */
wl_pslot *coop_player_slot(const wl_room *r, int p);
/* room_leader_next found no new leader (it skips members holding an input manager, and in co-op
 * the other players' members hold one; a story script may also leave the only free member busy):
 * when the current leader `cur` can no longer lead (gone from the party / no character), return a
 * member to take the lead anyway: a player's, else any member present.  The base game always has a
 * member with no manager (only the leader holds one), so a leader slot never stays empty there; in
 * co-op it could, and the room loop's gimmick pass then read the empty slot's character (a crash
 * after a cutscene that removed the leader from the party).  NULL with co-op off.  Logged to coop.log. */
wl_pslot *coop_leader_fallback(const wl_room *r, const wl_pslot *cur);
/* rhost --coop-wait (the launcher's co-op; --coop-p2-wait too): players 2 / 3 start out (the AI drives
 * the members) and join by holding switch.  Without it player 2 starts in, as before. */
void coop_wait(void);
/* the on-screen notices and bars in Portuguese */
void coop_set_lang_pt(int pt);
/* After char_ai built a player-driven character's command (room_char_step): while that
 * character is at 0 HP ("Disabled": in co-op a fallen member keeps its player), drop the
 * attack and the skill / spell from the command; moving, jumping and "use" stay.  The base
 * game never gives a Disabled member to the player, so this only applies with co-op on. */
void coop_filter_cmd(wl_char *c);
/* The HUD's spell panel (top right, rd_498960, drawn by the view render rd_494e60 right after the
 * leader panel rd_497f40) shows the leader's slot, and only when that character casts (skill keys
 * live, and not Arche before she learns magic).  With co-op on, when the leader does not cast but
 * player 2 (else 3) drives a character that does, return that slot for the spell panel instead (the
 * leader panel is untouched); otherwise `leader`.  An AI-driven caster never gets the panel, as in
 * the base game. */
wl_pslot *coop_spell_panel_slot(wl_pslot *leader);
/* the joystick poll (0x5ba650) got `hr` from GetDeviceState: should it Acquire again now?
 * Retail re-acquires only on DIERR_INPUTLOST; a pad unplugged and plugged back in reports
 * another failure instead and stayed dead until the window was reactivated.  Co-op only,
 * any failure while the game holds the device acquired, a few tries per second per device;
 * each new failure code is logged once to coop.log. */
int coop_pad_reacquire_due(const void *dev, int32_t hr, int32_t acquired);
/* a log summary (empty when co-op is off) */
void coop_status(char *buf, size_t n);
/* test only (rhost --coop-test-hp F:WHO:HP): the HP of the leader's (0) or player WHO's character */
void coop_test_hp(int32_t who, int32_t hp);
/* test only (rhost --coop-test-drop F:WHO): drop that character below the map (a pit fall) */
void coop_test_drop(int32_t who);
/* test only (rhost --coop-test-leave F): at F (on = 1) the leader leaves the party while every other
 * member is not targetable (body inactive: no normal new leader); at F+60 (on = 0) their bodies return */
void coop_test_leave(int32_t on);

/* The markers, drawn by the host onto the finished back buffer right before each Flip (an overlay:
 * no game state is read back or changed).  A "P2" (blue) / "P3" (purple) badge with a pointer above
 * that player's character; while the character is left / right of the view, an arrow on that screen
 * edge with the badge beside it.  Plus the notices (joined / left / no free ally) and the hold bars
 * (joining / leaving / player 1's drop-out).  `px` = the locked surface (`pitch` bytes per row,
 * w x h, 16 or 32 bits per pixel with the given channel masks). */
void coop_overlay_draw(uint8_t *px, int32_t pitch, int32_t w, int32_t h, int32_t bpp,
                       uint32_t rmask, uint32_t gmask, uint32_t bmask);

#endif /* OSS_COOP_H */
