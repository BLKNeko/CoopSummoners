/*
 * coop.{c,h} — local co-op: a second player drives a party ally (not a retail function;
 * off unless enabled, so every retail path and the parity corpus are unchanged).
 *
 * The engine already has the pieces: a room owns an array of input managers (room+0x0
 * count, +0x4 array; retail allocates one, fed by the keyboard and joystick 0), each party
 * slot carries the manager that drives it (pslot+0x0), and pslot_input_set 0x4cc5a0 flips a
 * member between player control (char+0x200 = 0, char_ai) and the behaviour AI (= 1).  The
 * leader switch (room_leader_next) only picks members with no manager, so a member held by
 * player 2 is never taken.
 *
 * With co-op on, the room gets a second manager, fed by joystick 1 and by the P2 keyboard
 * map below, and coop_tick attaches it to the first free member while player 1 holds the
 * leader, and releases it whenever player 1 does not (cutscenes and scripts take the
 * leader's input; P2 follows the same rule).
 *
 * The switch key is co-op's (coop.c, "the switch key"): both players tap within 0.5 s →
 * they swap characters (P1 keeps the lead); a lone tap → the game's own switch to a free
 * member once the window has passed; P2 holds it 1.5 s → leaves (the AI takes over); P1 holds
 * it 5 s → P2 is dropped out; while out, P2's next press of switch / jump / attack / use joins.
 * A fallen member keeps its player; game over stays the game's own (all down).
 *
 * P2 keyboard (numpad): 8/5/4/6 up/down/left/right, 1 attack, 2 jump, 3 use,
 * 0 sheathe / skill modifier, + switch character.
 */
#ifndef OSS_COOP_H
#define OSS_COOP_H

#include <stddef.h>
#include <stdint.h>

#include "world_layout.h"

#define COOP_NIN 2

/* 0 = off (retail), 1 = on.  Set by rhost's --coop or OPENSUMMONERS_COOP=1. */
extern int g_coop;
void coop_init_from_env(void);

/* the number of input managers a room gets (1 off, COOP_NIN on) */
uint16_t coop_room_inputs(void);
/* configure the managers past [0] after input_init(r, 0, ...) */
void coop_inputs_init(wl_room *r);
/* the P2 keyboard producer: like input_dik_produce, on the P2 key table */
void coop_p2_produce(input_mgr *m, const void *kb);
/* once per room input update, after the managers are fed: attach / release P2 */
void coop_tick(wl_room *r);
/* the slot P2 holds, or NULL */
wl_pslot *coop_p2_slot(const wl_room *r);
/* room_leader_next found no new leader (it skips members holding an input manager, and in co-op
 * P2's member holds one; a story script may also leave the only free member busy): when the
 * current leader `cur` can no longer lead (gone from the party / no character), return a member
 * to take the lead anyway: P2's, else any member present.  The base game always has a member with
 * no manager (only the leader holds one), so a leader slot never stays empty there; in co-op it
 * could, and the room loop's gimmick pass then read the empty slot's character (a crash after a
 * cutscene that removed the leader from the party).  NULL with co-op off.  Logged to coop.log. */
wl_pslot *coop_leader_fallback(const wl_room *r, const wl_pslot *cur);
/* P2 is in the game (co-op on and P2 has not left with a switch hold) */
int coop_p2_joined(void);
/* rhost --coop-p2-wait (the launcher's co-op): P2 starts out (the AI drives the second member) and joins with a
 * press of jump / attack / use / switch on P2's pad or the numpad.  Without it P2 starts in, as before.
 * Whenever P2 is out (left with a 1.5 s hold, or dropped by P1's 5 s hold), such a press joins again. */
void coop_p2_wait(void);
/* After char_ai built a player-driven character's command (room_char_step): while that
 * character is at 0 HP ("Disabled": in co-op a fallen member keeps its player), drop the
 * attack and the skill / spell from the command; moving, jumping and "use" stay.  The base
 * game never gives a Disabled member to the player, so this only applies with co-op on. */
void coop_filter_cmd(wl_char *c);
/* The HUD's spell panel (top right, rd_498960, drawn by the view render rd_494e60 right after the
 * leader panel rd_497f40) shows the leader's slot, and only when that character casts (skill keys
 * live, and not Arche before she learns magic).  With co-op on, when the leader does not cast but
 * player 2 drives a character that does, return player 2's slot for the spell panel instead (the
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
/* test only (rhost --coop-test-hp F:WHO:HP): the HP of the leader's (0) or P2's (1) character */
void coop_test_hp(int32_t who, int32_t hp);
/* test only (rhost --coop-test-drop F:WHO): drop that character below the map (a pit fall) */
void coop_test_drop(int32_t who);
/* test only (rhost --coop-test-leave F): at F (on = 1) the leader leaves the party while every other
 * member is not targetable (body inactive: no normal new leader); at F+60 (on = 0) their bodies return */
void coop_test_leave(int32_t on);

/* The P2 marker, drawn by the host onto the finished back buffer right before each Flip
 * (an overlay: no game state is read back or changed).  A blue "P2" badge with a pointer
 * above P2's character; while the character is left / right of the view, an arrow on that
 * screen edge with the badge beside it.  `px` = the locked surface (`pitch` bytes per row,
 * w x h, 16 or 32 bits per pixel with the given channel masks). */
void coop_overlay_draw(uint8_t *px, int32_t pitch, int32_t w, int32_t h, int32_t bpp,
                       uint32_t rmask, uint32_t gmask, uint32_t bmask);

#endif /* OSS_COOP_H */
