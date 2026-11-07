/* coop.c — local co-op (players 2 and 3).  See coop.h. */
#include "coop.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera_follow.h"
#include "char_state.h"
#include "engine_calls.h"
#include "input.h"
#include "party_member.h"
#include "room_load.h"
#include "scene_loop.h"

int g_coop;

void coop_init_from_env(void)
{
    const char *v = getenv("OPENSUMMONERS_COOP");
    if (v != NULL && v[0] == '1') g_coop = 1;
}

static void *sysp(uint32_t off) { void *p; memcpy(&p, (const uint8_t *)g_eng.sys + off, sizeof p); return p; }
static int32_t get32(const void *p, uint32_t off) { int32_t v; memcpy(&v, (const uint8_t *)p + off, 4); return v; }

static void coop_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void coop_log(const char *fmt, ...)
{
    FILE *f = fopen("coop.log", "a");
    if (f == NULL) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, " at tick %u\n", (unsigned)g_eng.tick());
    fclose(f);
}

/* ---- the players' devices and keys ---- */

static coop_player_ctl g_ctl[COOP_NIN];
static int g_ctl_init, g_ctl_loaded;

void coop_controls_default(coop_player_ctl ctl[COOP_NIN])
{
    static const uint8_t P2_KEYS[COOP_NACT] = { 0x48, 0x4c, 0x4b, 0x4d, 0x4f, 0x50, 0x51, 0x52, 0x4e };   /* numpad */
    static const uint8_t P3_KEYS[COOP_NACT] = { 0x17, 0x25, 0x24, 0x26, 0x16, 0x18, 0x19, 0x15, 0x23 };   /* IKJL UOP Y H */
    static const uint8_t BUTTONS[COOP_NACT] = { 0, 0, 0, 0, 2, 0, 1, 4, 3 };   /* attack X, jump A, use B, LB, Y */
    memset(ctl, 0, sizeof(coop_player_ctl) * COOP_NIN);
    for (int p = 0; p < COOP_NIN; p++) {
        ctl[p].pad = 1;
        ctl[p].pad_index = p;
        memcpy(ctl[p].buttons, BUTTONS, COOP_NACT);
    }
    ctl[0].kb = 1;
    ctl[1].kb = 1;
    memcpy(ctl[1].keys, P2_KEYS, COOP_NACT);
    memcpy(ctl[2].keys, P3_KEYS, COOP_NACT);
}

static void ctl_init(void)
{
    if (!g_ctl_init) { coop_controls_default(g_ctl); g_ctl_init = 1; }
}

void coop_set_controls(const coop_player_ctl ctl[COOP_NIN])
{
    memcpy(g_ctl, ctl, sizeof g_ctl);
    g_ctl_init = g_ctl_loaded = 1;
}

/* DirectInput's joystick list in the device manager (id_5ba120's EnumDevices: DIDEVICEINSTANCEA, 580 bytes each,
 * from +4; the count at +0x4884): the index of player p's gamepad, or -1 (not connected) */
static int32_t pad_resolve(intptr_t di, const coop_player_ctl *c)
{
    int32_t n = get32((const void *)di, 0x4884);
    static const uint8_t zero[16];
    int by_guid = memcmp(c->pad_guid, zero, 16) != 0;
    for (int32_t i = 0; i < n && i < 32; i++) {
        const uint8_t *e = (const uint8_t *)di + 4 + 580u * (uint32_t)i;
        if (by_guid) { if (memcmp(e + 4, c->pad_guid, 16) == 0) return i; }
        else if (c->pad_name[0]) { if (strncmp((const char *)e + 40, c->pad_name, sizeof c->pad_name - 1) == 0) return i; }
    }
    if (by_guid || c->pad_name[0]) return -1;
    return c->pad_index < n ? c->pad_index : -1;
}

int32_t coop_game_pad_index(intptr_t dinput, intptr_t slot, int32_t index)
{
    if (!g_ctl_loaded) return index;
    intptr_t s0 = (intptr_t)g_eng.sys + 0x287c;
    if (slot == s0) {
        if (!g_ctl[0].pad) return -2;
        int32_t i = pad_resolve(dinput, &g_ctl[0]);
        return i < 0 ? -2 : i;
    }
    if (slot == s0 + 4) return -2;
    return index;
}

uint16_t coop_room_inputs(void) { return g_coop ? COOP_NIN : 1; }

void coop_inputs_init(wl_room *r)
{
    ctl_init();
    if (g_ctl_loaded && r->nin >= 1 && r->in != NULL) r->in[0].src_kb = g_ctl[0].kb ? 1 : 0;
    if (!g_coop || r->nin < COOP_NIN) return;
    for (uint16_t p = 1; p < COOP_NIN; p++) input_init(r, p, 2, 0, 0);   /* src_kb 2: co-op's producer */
}

/* ---- the producers ---- */

/* the keyboard device wrapper, as input_dik.c reads it */
typedef struct p2_dev {
    uint8_t         _p00[0x10];
    uint32_t        n;
    const uint32_t *ev;
    uint8_t         state[256];
} p2_dev;

static void push_ev(input_mgr *m, int32_t id, uint32_t ts, int32_t down)
{
    input_event *r = m->ring[INPUT_RING_LEN - 1];
    for (int k = INPUT_RING_LEN - 1; k != 0; k--) m->ring[k] = m->ring[k - 1];
    m->ring[0] = r;
    r->id = id;
    r->ts = ts;
    r->flag = down != 0;
}

/* action a went down / up: the ring records the bound key would make (input_dik.c's fan-out: attack +0x548,
 * jump +0x564, use +0x59c, sheathe / skill modifier +0x580, switch +0x5b8; the directions post 12..15 while
 * the skill modifier is held with skills on, and Up also posts 35) */
static void emit(input_mgr *m, int a, uint32_t ts, int down, int alt)
{
    static const int32_t DIR[4] = { 1, 3, 2, 4 }, DIR_ALT[4] = { 12, 13, 14, 15 };
    switch (a) {
    case COOP_A_UP: case COOP_A_DOWN: case COOP_A_LEFT: case COOP_A_RIGHT:
        push_ev(m, alt ? DIR_ALT[a] : DIR[a], ts, down);
        if (a == COOP_A_UP) push_ev(m, 35, ts, down);
        break;
    case COOP_A_ATTACK: push_ev(m, 8, ts, down); push_ev(m, 37, ts, down); push_ev(m, 36, ts, down); break;
    case COOP_A_JUMP:   push_ev(m, 7, ts, down); push_ev(m, 39, ts, down); break;
    case COOP_A_USE:    push_ev(m, 38, ts, down); push_ev(m, 36, ts, down); break;
    case COOP_A_MOD:    push_ev(m, 9, ts, down); push_ev(m, 17, ts, down); break;
    case COOP_A_SWITCH: push_ev(m, 20, ts, down); break;
    default: break;
    }
}

/* a player's own gamepad: the game's joystick wrapper (id_5ba070, 0x78 bytes: the DIJOYSTATE at +0x18), opened
 * from the game's DirectInput manager, retried every PAD_OPEN_EVERY updates while it is missing (hot plug) */
#define PAD_OPEN_EVERY 120
static int32_t g_joy[COOP_NIN];   /* the wrappers (32-bit pointers, as the engine stores them) */
static uint32_t g_joy_try[COOP_NIN];
static uint8_t g_pad_prev[COOP_NIN][COOP_NACT];

static intptr_t pad_get(int p)
{
    if (g_joy[p] != 0) return (intptr_t)(uint32_t)g_joy[p];
    if (g_joy_try[p]++ % PAD_OPEN_EVERY != 0) return 0;
    intptr_t di = (intptr_t)sysp(0x2870);
    if (di == 0) return 0;
    intptr_t hwnd = get32(g_eng.data(0x8a93ecu), 0);
    int32_t tmp = 0;
    g_eng.h5ba120((void *)di, (intptr_t)&tmp, hwnd, 9999);   /* enumerate only (no device at that index) */
    int32_t i = pad_resolve(di, &g_ctl[p]);
    if (i < 0) return 0;
    g_eng.h5ba120((void *)di, (intptr_t)&g_joy[p], hwnd, i);
    if (g_joy[p] == 0) return 0;
    g_eng.h5ba290((void *)(intptr_t)(uint32_t)g_joy[p]);
    coop_log("player %d: gamepad %d opened", p + 1, (int)i);
    return (intptr_t)(uint32_t)g_joy[p];
}

/* the actions held on gamepad `w` (stick or d-pad for the directions, the player's buttons for the rest) */
static void pad_state(intptr_t w, const coop_player_ctl *c, uint8_t on[COOP_NACT])
{
    int32_t th;
    memcpy(&th, (const uint8_t *)**g_eng.keycfg + 1340, 4);   /* the game's gamepad dead zone (of 1000) */
    if (th <= 0 || th >= 1000) th = 500;
    int32_t x = get32((const void *)w, 0x18), y = get32((const void *)w, 0x1c);
    uint32_t pov = (uint32_t)get32((const void *)w, 0x38);
    int pv = (pov & 0xffff) != 0xffff;
    on[COOP_A_UP] = y < -th || (pv && (pov >= 31500 || pov <= 4500));
    on[COOP_A_DOWN] = y > th || (pv && pov >= 13500 && pov <= 22500);
    on[COOP_A_LEFT] = x < -th || (pv && pov >= 22500 && pov <= 31500);
    on[COOP_A_RIGHT] = x > th || (pv && pov >= 4500 && pov <= 13500);
    for (int a = COOP_A_ATTACK; a < COOP_NACT; a++)
        on[a] = c->buttons[a] < 32 && (((const uint8_t *)w)[0x48 + c->buttons[a]] & 0x80) != 0;
}

void coop_produce(wl_room *r, uint16_t p, const void *kb)
{
    if (p >= COOP_NIN || r->in == NULL) return;
    ctl_init();
    input_mgr *m = &r->in[p];
    const coop_player_ctl *c = &g_ctl[p];
    const p2_dev *d = c->kb ? kb : NULL;
    uint32_t skills_on;
    memcpy(&skills_on, (const uint8_t *)**g_eng.keycfg + 0x104, 4);
    uint8_t pad_on[COOP_NACT] = { 0 };
    intptr_t w = c->pad ? pad_get(p) : 0;
    if (w != 0 && g_eng.h5ba650((void *)w)) pad_state(w, c, pad_on);
    int held[COOP_NACT];
    for (int a = 0; a < COOP_NACT; a++)
        held[a] = pad_on[a] || (d != NULL && c->keys[a] != 0 && (d->state[c->keys[a]] & 0x80) != 0);
    int alt = skills_on != 0 && held[COOP_A_MOD];
    if (d != NULL)
        for (uint32_t i = 0; i < d->n; i++) {
            const uint32_t *e = d->ev + 4 * i;
            for (int a = 0; a < COOP_NACT; a++)
                if (c->keys[a] != 0 && e[0] == c->keys[a]) emit(m, a, e[2], (e[1] & 0x80) != 0, alt);
        }
    uint32_t now = g_eng.tick();
    for (int a = 0; a < COOP_NACT; a++)
        if (pad_on[a] != g_pad_prev[p][a]) {
            emit(m, a, now, pad_on[a], alt);
            g_pad_prev[p][a] = pad_on[a];
        }
    if (!alt) for (int a = 0; a < 4; a++) if (held[a]) m->axis_held[a] = 1;
    if (held[COOP_A_ATTACK]) m->axis_held[5] = 1;
    if (held[COOP_A_JUMP]) m->axis_held[4] = 1;
}

#define PAD_RETRY_POLLS 20   /* polls between Acquire tries (~3 per second at 60 fps) */

int coop_pad_reacquire_due(const void *dev, int32_t hr, int32_t acquired)
{
    static struct { const void *dev; uint32_t n; int32_t last_hr; } seen[4];
    if (!g_coop || acquired == 0) return 0;
    int k = 0;
    while (k < 4 && seen[k].dev != NULL && seen[k].dev != dev) k++;
    if (k == 4) k = 0;
    seen[k].dev = dev;
    if (hr != seen[k].last_hr) {   /* each new failure code once, into coop.log beside the game */
        coop_log("pad %d: poll failed hr=0x%08x, re-acquiring", k, (unsigned)hr);
        seen[k].last_hr = hr;
    }
    return seen[k].n++ % PAD_RETRY_POLLS == 0;
}

/* ---- slots ---- */

wl_pslot *coop_player_slot(const wl_room *r, int p)
{
    if (p < 1 || p >= COOP_NIN || r->nin <= p || r->in == NULL) return NULL;
    const input_mgr *m = &r->in[p];
    for (int k = 0; k < 8; k++) {
        wl_pslot *s = r->slot[k];
        if (s != NULL && s->in == m) return s;
    }
    return NULL;
}

wl_pslot *coop_leader_fallback(const wl_room *r, const wl_pslot *cur)
{
    if (!g_coop || r == NULL) return NULL;
    if (cur != NULL && cur->active == 1 && cur->ch != NULL) return NULL;   /* the leader can still lead */
    wl_pslot *pick = NULL;
    for (int p = 1; p < COOP_NIN && pick == NULL; p++) {
        wl_pslot *s = coop_player_slot(r, p);
        if (s != NULL && s != cur && s->active == 1 && s->id != 100000104 && s->ch != NULL) pick = s;
    }
    for (int k = 0; k < 8 && pick == NULL; k++) {
        wl_pslot *s = r->slot[k];
        if (s != NULL && s != cur && s->active == 1 && s->id != 100000104 && s->ch != NULL) pick = s;
    }
    static int logged;
    if (logged++ < 8)
        coop_log("leader fallback: leader %d (active %d, ch %s) -> %d", cur != NULL ? (int)cur->code : 0,
                 cur != NULL ? (int)cur->active : 0, cur != NULL && cur->ch != NULL ? "yes" : "no",
                 pick != NULL ? (int)pick->code : 0);
    return pick;
}

/* player 1 is in control: the leader's character runs on manager 0 (scripts clear the
 * character's +0x158a4 / set +0x200 directly, so look at the character, not the slot) */
static int p1_in_control(const wl_room *r)
{
    const wl_pslot *ld = r->leader;
    if (ld == NULL || ld->active != 1 || ld->ch == NULL || r->in == NULL || ld->in != &r->in[0]) return 0;
    const wl_char *c = ld->ch;
    return c->input == &r->in[0] && c->f200 == 0;
}

static int can_take(const wl_room *r, const wl_pslot *s)
{
    return s != NULL && s != r->leader && s->active == 1 && s->id != 100000104 && s->in == NULL &&
           s->ch != NULL && char_targetable(s->ch, 0, 1);
}

/* ---- players in / out, the notices ---- */

static int g_out[COOP_NIN] = { 0, 0, 1 };   /* 1 = not playing (the AI drives); player 3 starts out */
static int g_waiting[COOP_NIN];            /* joined, waiting for a free member */
static int32_t g_last_code[COOP_NIN];      /* the member a player had last (back after a cutscene) */

void coop_wait(void)
{
    for (int p = 1; p < COOP_NIN; p++) g_out[p] = 1;
}

enum { NOTICE_NONE, NOTICE_JOINED, NOTICE_LEFT, NOTICE_WAITING };
static int g_notice[COOP_NIN];
static uint32_t g_notice_at[COOP_NIN];
static int g_ui_pt;

void coop_set_lang_pt(int pt) { g_ui_pt = pt != 0; }

static void notice(int p, int k)
{
    g_notice[p] = k;
    g_notice_at[p] = g_eng.tick();
}

/* a free member for player p: the one it had last, else the first */
static wl_pslot *free_member(const wl_room *r, int p)
{
    for (int k = 0; k < 8; k++)
        if (can_take(r, r->slot[k]) && r->slot[k]->code == g_last_code[p]) return r->slot[k];
    for (int k = 0; k < 8; k++)
        if (can_take(r, r->slot[k])) return r->slot[k];
    return NULL;
}

static void attach(wl_room *r, int p, wl_pslot *s)
{
    pslot_input_set(s, &r->in[p]);
    g_last_code[p] = s->code;
}

static void player_join(wl_room *r, int p)
{
    g_out[p] = 0;
    wl_pslot *s = free_member(r, p);
    if (s != NULL) {
        attach(r, p, s);
        g_waiting[p] = 0;
        notice(p, NOTICE_JOINED);
        coop_log("player %d joined (%d)", p + 1, (int)s->code);
    } else {
        g_waiting[p] = 1;
        notice(p, NOTICE_WAITING);
        coop_log("player %d joined, waiting for a free member", p + 1);
    }
}

static void player_leave(wl_room *r, int p, const char *why)
{
    wl_pslot *s = coop_player_slot(r, p);
    if (s != NULL) pslot_input_set(s, NULL);
    g_out[p] = 1;
    g_waiting[p] = 0;
    notice(p, NOTICE_LEFT);
    coop_log("player %d %s", p + 1, why);
}

/* ---- the switch key: taps, the LEGO-style swap, the holds ----
 *
 * While co-op runs, coop_tick takes every switch event (ring id 20, posted by Enter / the bound switch key /
 * pad button for player 1, by the players' own switch key / button for 2 and 3) off the managers before
 * room_keys (0x468a20 runs before it in the room update) can see it.  A press and its release make a TAP
 * (counted at the release) or a HOLD:
 *   - two players with a character tap within COOP_SWAP_MS: they swap characters (fallen ones too); when one
 *     is player 1 the lead goes with player 1's manager (room_leader_set);
 *   - a lone tap, once the window has passed (at once when no swap is possible): a fresh id 20 press is posted
 *     back into that player's ring, so room_keys does the game's own switch to a free member (its 40-tick
 *     retry included);
 *   - player 2 / 3 out holds COOP_JOIN_MS: joins; in holds COOP_HOLD_MS: leaves;
 *   - player 1 holds COOP_KICK_MS while anyone else plays: everyone else is dropped out.  A shorter player 1
 *     hold is just a tap. */

#define SWITCH_ID      20
#define COOP_SWAP_MS   500
#define COOP_JOIN_MS   1000
#define COOP_HOLD_MS   1500
#define COOP_KICK_MS   5000

typedef struct sw_state {
    int          down;        /* the switch is held (a press seen, its release not yet)  */
    uint32_t     down_at;
    int          hold_fired;
    int          tap;         /* a tap waiting for the swap window                       */
    uint32_t     tap_at;
    input_event *posted;      /* the press coop posted for room_keys: not a player's     */
} sw_state;

static sw_state g_sw[COOP_NIN];

static void sw_reset(void)
{
    for (int p = 0; p < COOP_NIN; p++) g_sw[p].down = g_sw[p].hold_fired = g_sw[p].tap = 0;
}

static int players_in(void)
{
    int n = 0;
    for (int p = 1; p < COOP_NIN; p++) n += !g_out[p];
    return n;
}

/* the game's own switch for player p: a press for room_keys' input_poll_consume (id 20, flag 1,
 * stamped now: inside its 100 ms window) */
static void sw_post(input_mgr *m, sw_state *s, uint32_t now)
{
    push_ev(m, SWITCH_ID, now, 1);
    s->posted = m->ring[0];
}

/* the slot player p plays: player 1 the leader (while it holds manager 0), else its own */
static wl_pslot *played(const wl_room *r, int p)
{
    if (p == 0) return r->leader != NULL && r->leader->in == &r->in[0] ? r->leader : NULL;
    return g_out[p] ? NULL : coop_player_slot(r, p);
}

/* players a < b swap characters; player 1's manager keeps the lead */
static void sw_swap(wl_room *r, int a, int b)
{
    wl_pslot *sa = played(r, a), *sb = played(r, b);
    if (sa == NULL || sb == NULL || sa == sb) return;
    if (a == 0) {
        room_leader_set(r, sb);              /* sb takes manager 0 and the lead, sa goes to the AI */
        pslot_input_set(sa, &r->in[b]);      /* ... and then to player b */
        room_formation(r);
    } else {
        pslot_input_set(sa, NULL);
        pslot_input_set(sb, NULL);
        pslot_input_set(sa, &r->in[b]);
        pslot_input_set(sb, &r->in[a]);
    }
    g_last_code[b] = sa->code;
    if (a != 0) g_last_code[a] = sb->code;
    coop_log("players %d and %d swapped", a + 1, b + 1);
}

static void sw_tap(wl_room *r, int p, uint32_t t, uint32_t now)
{
    sw_state *s = &g_sw[p];
    if (played(r, p) == NULL) return;   /* no character: nothing to switch or swap */
    int partners = 0;
    for (int q = 0; q < COOP_NIN && q < r->nin; q++) {
        if (q == p || played(r, q) == NULL) continue;
        partners++;
        int32_t gap = (int32_t)(t - g_sw[q].tap_at);
        if (g_sw[q].tap && (gap < 0 ? -gap : gap) <= COOP_SWAP_MS) {
            g_sw[q].tap = 0;
            sw_swap(r, p < q ? p : q, p < q ? q : p);
            return;
        }
    }
    if (partners == 0) { sw_post(&r->in[p], s, now); return; }
    s->tap = 1;
    s->tap_at = t;
}

static void sw_update(wl_room *r)
{
    uint32_t now = g_eng.tick();
    for (int p = 0; p < COOP_NIN && p < r->nin; p++) {
        input_mgr *m = &r->in[p];
        sw_state *s = &g_sw[p];
        for (int i = INPUT_RING_LEN - 1; i >= 0; i--) {   /* oldest first */
            input_event *e = m->ring[i];
            if (e == s->posted) {   /* ours, while room_keys may still take it; the record is reused later */
                if (e->id == SWITCH_ID && e->flag == 1 && (uint32_t)(now - e->ts) <= 200) continue;
                s->posted = NULL;
            }
            if (e->id != SWITCH_ID) continue;
            int pressed = e->flag == 1;
            uint32_t ts = e->ts;
            e->id = 0;
            if (pressed && !s->down) {
                s->down = 1;
                s->down_at = ts;
                s->hold_fired = 0;
            } else if (!pressed && s->down) {
                s->down = 0;
                if (!s->hold_fired) sw_tap(r, p, ts, now);
            }
        }
        if (!s->down || s->hold_fired) continue;
        uint32_t el = now - s->down_at;
        if (p == 0) {
            if (el >= COOP_KICK_MS && players_in() > 0) {
                s->hold_fired = 1;
                for (int q = 1; q < COOP_NIN; q++)
                    if (!g_out[q]) { player_leave(r, q, "dropped out by player 1"); g_sw[q].tap = g_sw[q].down = 0; }
            }
        } else if (g_out[p] ? el >= COOP_JOIN_MS : el >= COOP_HOLD_MS) {
            s->hold_fired = 1;
            s->tap = 0;
            if (g_out[p]) player_join(r, p);
            else player_leave(r, p, "left");
        }
    }
    for (int p = 0; p < COOP_NIN && p < r->nin; p++) {
        sw_state *s = &g_sw[p];
        if (s->tap && (uint32_t)(now - s->tap_at) > COOP_SWAP_MS) {
            s->tap = 0;
            sw_post(&r->in[p], s, now);
        }
    }
}

void coop_tick(wl_room *r)
{
    if (!g_coop || r->nin < 2 || r->in == NULL) return;
    if (!p1_in_control(r)) {   /* a cutscene / script: everyone's member goes to the AI (back afterwards) */
        for (int p = 1; p < COOP_NIN && p < r->nin; p++) {
            wl_pslot *held = coop_player_slot(r, p);
            if (held != NULL) pslot_input_set(held, NULL);
        }
        sw_reset();
        return;
    }
    sw_update(r);
    for (int p = 1; p < COOP_NIN && p < r->nin; p++) {
        input_mgr *m = &r->in[p];
        wl_pslot *held = coop_player_slot(r, p);
        if (g_out[p]) {
            if (held != NULL) pslot_input_set(held, NULL);
            continue;
        }
        if (held != NULL) {
            /* a script handed the character to the AI without clearing the slot: take it back */
            if (held->active == 1 && held->ch != NULL && (held->ch->input != m || held->ch->f200 != 0))
                pslot_input_set(held, m);
            if (held->active == 1) { g_last_code[p] = held->code; continue; }
            pslot_input_set(held, NULL);
        }
        wl_pslot *s = free_member(r, p);   /* joined without a member: the last one, else any free */
        if (s == NULL) continue;
        attach(r, p, s);
        if (g_waiting[p]) {
            g_waiting[p] = 0;
            notice(p, NOTICE_JOINED);
            coop_log("player %d got a free member (%d)", p + 1, (int)s->code);
        }
    }
}

/* a one-line state summary for host logs: the leader's and the players' characters (code, x, y, +0x200) */
void coop_status(char *buf, size_t n)
{
    buf[0] = 0;
    if (!g_coop || g_eng.world == NULL || *g_eng.world == NULL) return;
    const wl_room *r = (*g_eng.world)->room;
    if (r == NULL) { snprintf(buf, n, " coop[no room]"); return; }
    int nact = 0;
    for (int k = 0; k < 8; k++) if (r->slot[k] != NULL && r->slot[k]->active == 1) nact++;
    char a[96] = "-", b[COOP_NIN][96];
    const wl_pslot *ld = r->leader;
    if (ld != NULL && ld->ch != NULL && ld->ch->base.body != NULL)
        snprintf(a, sizeof a, "%d@%d,%d/%d hp%d%s act%d sk%d st%d pose%d", (int)ld->code, (int)ld->ch->base.body->x,
                 (int)ld->ch->base.body->y, (int)ld->ch->f200, (int)ld->ch->party->hp,
                 char_targetable(ld->ch, 0, 1) ? "" : " DOWN", (int)ld->ch->cmd.act, (int)ld->ch->cmd.skill,
                 (int)ld->ch->base.body->state, (int)ld->ch->base.body->pose);
    for (int p = 1; p < COOP_NIN; p++) {
        const wl_pslot *s = coop_player_slot(r, p);
        snprintf(b[p], sizeof b[p], "-");
        if (s != NULL && s->ch != NULL && s->ch->base.body != NULL)
            snprintf(b[p], sizeof b[p], "%d@%d,%d/%d hp%d%s", (int)s->code, (int)s->ch->base.body->x,
                     (int)s->ch->base.body->y, (int)s->ch->f200, (int)s->ch->party->hp,
                     char_targetable(s->ch, 0, 1) ? "" : " DOWN");
    }
    snprintf(buf, n, " coop[nin=%u act=%d p1ctl=%d p2out=%d lead=%s p2=%s p3out=%d p3=%s]", (unsigned)r->nin, nact,
             p1_in_control(r), g_out[1], a, b[1], g_out[2], b[2]);
}

void coop_filter_cmd(wl_char *c)
{
    if (!g_coop || c->party == NULL || c->party->hp > 0) return;
    if (c->cmd.act == 14) c->cmd.act = 0;   /* 14 attack (15 use stays) */
    c->cmd.skill = 0;
    c->cmd.skill_arg = 0;
    c->cmd.skill_ptr = 0;
    c->cmd.skill_kind = 0;
}

/* rd_498960's own gate: the skill keys are live (+0x14898 == 1), and not Arche before she learns magic */
static int coop_casts(const wl_char *c)
{
    return c != NULL && c->skill_input == 1 && !(c->base.code == 50010 && c->party->f466 == 0);
}

wl_pslot *coop_spell_panel_slot(wl_pslot *leader)
{
    if (!g_coop || leader == NULL || coop_casts(leader->ch) || g_eng.world == NULL || *g_eng.world == NULL)
        return leader;
    const wl_room *r = (*g_eng.world)->room;
    if (r == NULL || r->nin < 2 || !p1_in_control(r)) return leader;
    for (int p = 1; p < COOP_NIN; p++) {
        wl_pslot *s = g_out[p] ? NULL : coop_player_slot(r, p);
        if (s != NULL && s->active == 1 && s->ch != NULL && s->ch->f200 == 0 && coop_casts(s->ch)) return s;
    }
    return leader;
}

static const wl_pslot *test_slot(int32_t who)
{
    if (!g_coop || g_eng.world == NULL || *g_eng.world == NULL || (*g_eng.world)->room == NULL) return NULL;
    const wl_room *r = (*g_eng.world)->room;
    return who == 0 ? r->leader : coop_player_slot(r, who);
}

/* test only (rhost --coop-test-hp): set the HP of the leader's (who 0) or player who's character */
void coop_test_hp(int32_t who, int32_t hp)
{
    const wl_pslot *s = test_slot(who);
    if (s != NULL && s->ch != NULL && s->ch->party != NULL) s->ch->party->hp = hp;
}

/* test only (rhost --coop-test-drop): put the leader's (who 0) or player who's character far below
 * the map, as a fall into a bottomless pit does (char_apply's "fell" check) */
void coop_test_drop(int32_t who)
{
    const wl_pslot *s = test_slot(who);
    if (s != NULL && s->ch != NULL && s->ch->base.body != NULL) s->ch->base.body->y = 50000000;
}

void coop_test_leave(int32_t on)
{
    static int32_t saved[8];
    if (!g_coop || g_eng.world == NULL || *g_eng.world == NULL || (*g_eng.world)->room == NULL) return;
    wl_room *r = (*g_eng.world)->room;
    for (int k = 0; k < 8; k++) {   /* the others not targetable (body inactive) while the leader goes, then back */
        wl_pslot *s = r->slot[k];
        if (s == NULL || s == r->leader || s->active != 1 || s->ch == NULL || s->ch->base.body == NULL) continue;
        if (on) { saved[k] = s->ch->base.body->active; s->ch->base.body->active = 2; }
        else s->ch->base.body->active = saved[k];
    }
    if (on && r->leader != NULL && r->leader->active == 1) room_member_leave(r, r->leader, 0);
}

/* ---- the overlay: the players' markers, the notices and the hold bars ---- */

typedef struct ov_surf {
    uint8_t *px;
    int32_t  pitch, w, h, bpp;
    uint32_t rm, gm, bm;
} ov_surf;

static uint32_t ov_chan(uint32_t mask, uint8_t v)
{
    if (mask == 0) return 0;
    int lo = 0, bits = 0;
    while (!(mask >> lo & 1)) lo++;
    while (lo + bits < 32 && (mask >> (lo + bits) & 1)) bits++;
    return ((uint32_t)v >> (8 - bits)) << lo;
}

static uint32_t ov_rgb(const ov_surf *s, uint8_t r, uint8_t g, uint8_t b)
{
    return ov_chan(s->rm, r) | ov_chan(s->gm, g) | ov_chan(s->bm, b);
}

static void ov_put(const ov_surf *s, int32_t x, int32_t y, uint32_t c)
{
    if (x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    uint8_t *p = s->px + (size_t)y * (size_t)s->pitch;
    if (s->bpp == 16) { uint16_t v = (uint16_t)c; memcpy(p + 2 * x, &v, 2); }
    else if (s->bpp == 32) memcpy(p + 4 * x, &c, 4);
}

static void ov_rect(const ov_surf *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c)
{
    for (int32_t j = 0; j < h; j++)
        for (int32_t i = 0; i < w; i++) ov_put(s, x + i, y + j, c);
}

/* 5x7 glyphs, one row per byte (bit 4 = leftmost) */
typedef struct { char c; uint8_t rows[7]; } ov_char;
static const ov_char FONT[] = {
    { 'A', { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } }, { 'D', { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e } },
    { 'E', { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f } }, { 'F', { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 } },
    { 'G', { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f } }, { 'I', { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e } },
    { 'J', { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c } }, { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f } },
    { 'N', { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } }, { 'O', { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } },
    { 'P', { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 } }, { 'R', { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 } },
    { 'S', { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e } }, { 'T', { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } }, { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 } },
    { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a } }, { '2', { 0x0e, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1f } },
    { '3', { 0x1f, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0e } },
};

static const uint8_t *ov_glyph_rows(char ch)
{
    for (size_t k = 0; k < sizeof FONT / sizeof FONT[0]; k++)
        if (FONT[k].c == ch) return FONT[k].rows;
    return NULL;
}

/* `t` (the FONT's characters and spaces) at `sc` x with its top-left at (x, y) */
static void ov_text(const ov_surf *s, const char *t, int32_t x, int32_t y, int32_t sc, uint32_t ink)
{
    for (; *t; t++, x += 6 * sc) {
        const uint8_t *g = ov_glyph_rows(*t);
        if (g == NULL) continue;
        for (int j = 0; j < 7; j++)
            for (int i = 0; i < 5; i++)
                if (g[j] >> (4 - i) & 1) ov_rect(s, x + i * sc, y + j * sc, sc, sc, ink);
    }
}

static int32_t ov_text_w(const char *t, int32_t sc) { return (int32_t)strlen(t) * 6 * sc - sc; }

/* a rounded panel (the badge's look) */
static void ov_panel(const ov_surf *s, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t edge, uint32_t fill)
{
    ov_rect(s, x + 1, y, w - 2, h, edge);
    ov_rect(s, x, y + 1, w, h - 2, edge);
    ov_rect(s, x + 1, y + 1, w - 2, h - 2, fill);
}

#define BADGE_W 17
#define BADGE_H 13

/* the "Pn" badge with its top-left at (x, y) */
static void ov_badge(const ov_surf *s, int p, int32_t x, int32_t y, uint32_t edge, uint32_t fill, uint32_t ink)
{
    char t[3] = { 'P', (char)('1' + p), 0 };
    ov_panel(s, x, y, BADGE_W, BADGE_H, edge, fill);
    ov_text(s, t, x + 3, y + 3, 1, ink);
}

/* a solid triangle pointing `dir` (0 down, -1 left, +1 right) with its tip at (tx, ty), `n` px deep,
 * outlined in `edge` */
static void ov_arrow(const ov_surf *s, int32_t tx, int32_t ty, int32_t dir, int32_t n, uint32_t edge, uint32_t fill)
{
    for (int32_t d = 0; d <= n; d++) {
        int32_t half = d * 3 / 4;
        for (int32_t k = -half - 1; k <= half + 1; k++) {
            uint32_t c = (k < -half || k > half || d == n) ? edge : fill;
            if (dir == 0) ov_put(s, tx + k, ty - d, c);
            else ov_put(s, tx - dir * d, ty + k, c);
        }
    }
}

/* where player p's marker goes this frame: 0 = none, else *side = 0 above the character at (sx, sy)
 * (its head, screen px), -1 / +1 = off the left / right edge, sy clamped */
static int coop_marker(int p, int32_t sw, int32_t sh, int32_t *sx, int32_t *sy, int32_t *side)
{
    if (!g_coop || g_eng.world == NULL || *g_eng.world == NULL) return 0;
    const wl_world *wd = *g_eng.world;
    const wl_room *r = wd->room;
    const camera_view *v = wd->view;
    if (r == NULL || v == NULL || !p1_in_control(r)) return 0;
    const wl_pslot *s = coop_player_slot(r, p);
    if (s == NULL || s->active != 1 || s->ch == NULL || s->ch->base.body == NULL) return 0;
    const wl_body *b = s->ch->base.body;
    int32_t x, y;
    view_project(v, b->x + b->w / 2, b->y + b->margin, 0, 0, 0, 0, &x, &y, 0);
    *side = x < 0 ? -1 : x >= sw ? 1 : 0;
    if (*side != 0) {   /* the edge arrow stays clear of the HUD: portrait top-left, bars along the bottom */
        y += (p - 1) * 20;   /* player 3's below player 2's */
        if (y < 100) y = 100;
        if (y > sh - 95) y = sh - 95;
    } else if (y < 10 + BADGE_H + 10) {
        y = 10 + BADGE_H + 10;   /* the badge stays on screen */
    }
    *sx = x;
    *sy = y;
    return 1;
}

#define NOTICE_MS     2000
#define BAR_AFTER     250     /* a shorter press is a tap: no bar */
#define KICK_AFTER    400
#define NOTICE_Y      34      /* the panels' top: below the screen's top edge, clear of the portrait (left) */

/* a panel with `t` at the top centre (row y), and a bar filled el / total under it when total != 0;
 * returns the next row's y */
static int32_t ov_row(const ov_surf *s, int32_t y, const char *t, uint32_t el, uint32_t total, uint32_t edge,
                      uint32_t fill, uint32_t ink)
{
    int32_t tw = ov_text_w(t, 2), bw = total ? 120 : 0;
    int32_t pw = (tw > bw ? tw : bw) + 16, ph = 14 + 12 + (total ? 13 : 0);
    int32_t px = s->w / 2 - pw / 2;
    ov_panel(s, px, y, pw, ph, edge, fill);
    ov_text(s, t, s->w / 2 - tw / 2, y + 6, 2, ink);
    if (total) {
        if (el > total) el = total;
        int32_t bx = s->w / 2 - bw / 2, by = y + 6 + 14 + 5;
        ov_rect(s, bx - 1, by - 1, bw + 2, 8 + 2, edge);
        ov_rect(s, bx, by, bw, 8, ov_rgb(s, 24, 24, 48));
        ov_rect(s, bx, by, (int32_t)((uint32_t)bw * el / total), 8, ink);
    }
    return y + ph + 4;
}

static void player_colors(const ov_surf *s, int p, uint32_t *edge, uint32_t *fill)
{
    *edge = ov_rgb(s, 16, 24, 48);
    *fill = p == 2 ? ov_rgb(s, 150, 70, 220) : ov_rgb(s, 48, 144, 255);   /* player 3 purple (Stella), 2 blue */
}

static void ov_notices(const ov_surf *s)
{
    uint32_t now = g_eng.tick(), ink = ov_rgb(s, 255, 255, 255), edge, fill;
    int32_t y = NOTICE_Y;
    char t[24];
    const sw_state *k = &g_sw[0];
    int n_in = players_in();
    if (k->down && !k->hold_fired && n_in > 0 && (uint32_t)(now - k->down_at) >= KICK_AFTER) {
        int who = 0;
        for (int p = 1; p < COOP_NIN; p++) if (!g_out[p]) who = p;
        if (n_in == 1) snprintf(t, sizeof t, g_ui_pt ? "TIRAR P%d" : "DROP P%d", who + 1);
        else snprintf(t, sizeof t, "%s", g_ui_pt ? "TIRAR TODOS" : "DROP ALL");
        player_colors(s, 1, &edge, &fill);
        y = ov_row(s, y, t, now - k->down_at, COOP_KICK_MS, edge, fill, ink);
    }
    for (int p = 1; p < COOP_NIN; p++) {
        const sw_state *h = &g_sw[p];
        player_colors(s, p, &edge, &fill);
        if (h->down && !h->hold_fired && (uint32_t)(now - h->down_at) >= BAR_AFTER) {
            if (g_out[p]) snprintf(t, sizeof t, g_ui_pt ? "P%d ENTRANDO" : "P%d JOINING", p + 1);
            else snprintf(t, sizeof t, g_ui_pt ? "P%d SAINDO" : "P%d LEAVING", p + 1);
            y = ov_row(s, y, t, now - h->down_at, g_out[p] ? COOP_JOIN_MS : COOP_HOLD_MS, edge, fill, ink);
            continue;
        }
        if (g_notice[p] == NOTICE_NONE || (uint32_t)(now - g_notice_at[p]) >= NOTICE_MS) continue;
        const char *w = g_notice[p] == NOTICE_JOINED  ? (g_ui_pt ? "ENTROU" : "JOINED")
                      : g_notice[p] == NOTICE_WAITING ? (g_ui_pt ? "ESPERANDO" : "WAITING")
                                                      : (g_ui_pt ? "SAIU" : "LEFT");
        snprintf(t, sizeof t, "P%d %s", p + 1, w);
        y = ov_row(s, y, t, 0, 0, edge, fill, ink);
    }
}

void coop_overlay_draw(uint8_t *px, int32_t pitch, int32_t w, int32_t h, int32_t bpp,
                       uint32_t rmask, uint32_t gmask, uint32_t bmask)
{
    if (!g_coop || px == NULL || (bpp != 16 && bpp != 32)) return;
    ov_surf s = { px, pitch, w, h, bpp, rmask, gmask, bmask };
    ov_notices(&s);
    uint32_t ink = ov_rgb(&s, 255, 255, 255);
    for (int p = 1; p < COOP_NIN; p++) {
        int32_t x, y, side;
        uint32_t edge, fill;
        if (!coop_marker(p, w, h, &x, &y, &side)) continue;
        player_colors(&s, p, &edge, &fill);
        if (side == 0) {
            ov_arrow(&s, x, y - 4, 0, 5, edge, fill);
            ov_badge(&s, p, x - BADGE_W / 2, y - 10 - BADGE_H, edge, fill, ink);
        } else if (side < 0) {
            ov_arrow(&s, 3, y, -1, 9, edge, fill);
            ov_badge(&s, p, 15, y - BADGE_H / 2, edge, fill, ink);
        } else {
            ov_arrow(&s, w - 4, y, 1, 9, edge, fill);
            ov_badge(&s, p, w - 16 - BADGE_W, y - BADGE_H / 2, edge, fill, ink);
        }
    }
}
