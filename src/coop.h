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
/* the joystick poll (0x5ba650) got `hr` from GetDeviceState: should it Acquire again now?
 * Retail re-acquires only on DIERR_INPUTLOST; a pad unplugged and plugged back in reports
 * another failure instead and stayed dead until the window was reactivated.  Co-op only,
 * any failure while the game holds the device acquired, a few tries per second per device;
 * each new failure code is logged once to coop.log. */
int coop_pad_reacquire_due(const void *dev, int32_t hr, int32_t acquired);
/* a log summary (empty when co-op is off) */
void coop_status(char *buf, size_t n);

/* The P2 marker, drawn by the host onto the finished back buffer right before each Flip
 * (an overlay: no game state is read back or changed).  A blue "P2" badge with a pointer
 * above P2's character; while the character is left / right of the view, an arrow on that
 * screen edge with the badge beside it.  `px` = the locked surface (`pitch` bytes per row,
 * w x h, 16 or 32 bits per pixel with the given channel masks). */
void coop_overlay_draw(uint8_t *px, int32_t pitch, int32_t w, int32_t h, int32_t bpp,
                       uint32_t rmask, uint32_t gmask, uint32_t bmask);

#endif /* OSS_COOP_H */
