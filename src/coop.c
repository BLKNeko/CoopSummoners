/* coop.c — local co-op (players 2 and 3).  See coop.h. */
#include "coop_internal.h"

int g_coop;

void coop_init_from_env(void)
{
    const char *v = getenv("OPENSUMMONERS_COOP");
    if (v != NULL && v[0] == '1') g_coop = 1;
}


void coop_log(const char *fmt, ...)
{
    FILE *f = fopen("coop.log", "a");
    if (f == NULL) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, " at tick %u\n", g_eng.tick != NULL ? (unsigned)g_eng.tick() : 0u);   /* no clock yet at start (the tuning file) */
    fclose(f);
}

/* ---- the players' devices and keys ---- */

coop_player_ctl g_ctl[COOP_NIN];
static int g_ctl_init, g_ctl_loaded;

void coop_controls_default(coop_player_ctl ctl[COOP_NIN])
{
    static const uint8_t P2_KEYS[COOP_NACT] = { 0x48, 0x4c, 0x4b, 0x4d, 0x4f, 0x50, 0x51, 0x52, 0x4e, 0x4a };   /* numpad (- camera) */
    static const uint8_t P3_KEYS[COOP_NACT] = { 0x17, 0x25, 0x24, 0x26, 0x16, 0x18, 0x19, 0x15, 0x23, 0x31 };   /* IKJL UOP Y H N */
    static const uint8_t BUTTONS[COOP_NACT] = { 0, 0, 0, 0, 2, 0, 1, 4, 3, COOP_PAD_RT };   /* attack X, jump A, use B, LB, Y, RT */
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
    ctl[0].keys[COOP_A_CAMERA] = 0x9d;   /* player 1 plays on the game's keys: only the camera key is co-op's (Right Ctrl; not
                                          * Backspace: the game's cancel, id 39) */
}

void ctl_init(void)
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
uint8_t g_held[COOP_NIN][COOP_NACT];   /* the actions each player holds now (the light reads them) */

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

/* "button" `b` of gamepad `w` held: 0..31 a button; COOP_PAD_RT / COOP_PAD_LT a trigger.  An Xbox-style pad
 * shows both triggers as one axis (the DIJOYSTATE's lZ at +0x20, ±1000 as the game sets every axis, 0x5ba6a0):
 * RT pulls it down, LT up */
int pad_button(intptr_t w, uint8_t b)
{
    if (b < 32) return (((const uint8_t *)w)[0x48 + b] & 0x80) != 0;
    int32_t z = get32((const void *)w, 0x20);
    return b == COOP_PAD_RT ? z < -500 : b == COOP_PAD_LT ? z > 500 : 0;
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
    for (int a = COOP_A_ATTACK; a < COOP_NACT; a++) on[a] = (uint8_t)pad_button(w, c->buttons[a]);
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
    for (int a = 0; a < COOP_NACT; a++) {
        held[a] = pad_on[a] || (d != NULL && c->keys[a] != 0 && (d->state[c->keys[a]] & 0x80) != 0);
        g_held[p][a] = (uint8_t)held[a];
    }
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
int p1_in_control(const wl_room *r)
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

int g_out[COOP_NIN] = { 0, 0, 1 };   /* 1 = not playing (the AI drives); player 3 starts out */
int g_waiting[COOP_NIN];            /* joined, waiting for a free member */
static int32_t g_last_code[COOP_NIN];      /* the member a player had last (back after a cutscene) */

void coop_wait(void)
{
    for (int p = 1; p < COOP_NIN; p++) g_out[p] = 1;
}

int g_notice[COOP_NIN];
uint32_t g_notice_at[COOP_NIN];
int g_ui_pt;

void coop_set_lang_pt(int pt) { g_ui_pt = pt != 0; }

void notice(int p, int k)
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
 *   - two players with a character tap within g_tune.swap_ms: they swap characters (fallen ones too); when one
 *     is player 1 the lead goes with player 1's manager (room_leader_set);
 *   - a lone tap, once the window has passed (at once when no swap is possible): a fresh id 20 press is posted
 *     back into that player's ring, so room_keys does the game's own switch to a free member (its 40-tick
 *     retry included);
 *   - player 2 / 3 out holds g_tune.join_ms: joins; in holds g_tune.leave_ms: leaves;
 *   - player 1 holds g_tune.kick_ms while anyone else plays: everyone else is dropped out.  A shorter player 1
 *     hold is just a tap. */


sw_state g_sw[COOP_NIN];

static void sw_reset(void)
{
    for (int p = 0; p < COOP_NIN; p++) g_sw[p].down = g_sw[p].hold_fired = g_sw[p].tap = 0;
}

int players_in(void)
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
    s->posted_ts = now;
}

/* the slot player p plays: player 1 the leader (while it holds manager 0), else its own */
wl_pslot *played(const wl_room *r, int p)
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
        if (g_sw[q].tap && (gap < 0 ? -gap : gap) <= g_tune.swap_ms) {
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
                /* room_keys left it (no member to switch to): drop it.  Read as a player's press, it had no
                 * release to end it -- a switch "held" forever, and player 1's drop-out hold then fired on the
                 * next player to join */
                if (e->id == SWITCH_ID && e->flag == 1 && e->ts == s->posted_ts) { e->id = 0; continue; }
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
            if (el >= g_tune.kick_ms && players_in() > 0) {
                s->hold_fired = 1;
                for (int q = 1; q < COOP_NIN; q++)
                    if (!g_out[q]) { player_leave(r, q, "dropped out by player 1"); g_sw[q].tap = g_sw[q].down = 0; }
            }
        } else if (g_out[p] ? el >= g_tune.join_ms : el >= g_tune.leave_ms) {
            s->hold_fired = 1;
            s->tap = 0;
            if (g_out[p]) player_join(r, p);
            else player_leave(r, p, "left");
        }
    }
    for (int p = 0; p < COOP_NIN && p < r->nin; p++) {
        sw_state *s = &g_sw[p];
        if (s->tap && (uint32_t)(now - s->tap_at) > g_tune.swap_ms) {
            s->tap = 0;
            sw_post(&r->in[p], s, now);
        }
    }
}

void coop_tick(wl_room *r)
{
    if (!g_coop || r->nin < 2 || r->in == NULL) return;
    cam_tick(r);
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
    static int was_down[COOP_NIN];   /* a player's character falling (0 HP): the "Pn DOWN" notice */
    for (int p = 1; p < COOP_NIN && p < r->nin; p++) {
        int down = coop_player_down(r, p);
        if (down && !was_down[p]) {
            notice(p, NOTICE_DOWN);
            coop_log("player %d fell", p + 1);
        }
        was_down[p] = down;
    }
}

int coop_player_down(const wl_room *r, int p)
{
    const wl_pslot *s = p == 0 ? r->leader : (g_out[p] ? NULL : coop_player_slot(r, p));
    return s != NULL && s->active == 1 && s->ch != NULL && s->ch->party != NULL && s->ch->party->hp <= 0;
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
        if (s != NULL && s->ch != NULL && s->ch->base.body != NULL && s->ch->base.body->spell != 0) {
            const wl_body *cb = s->ch->base.body;   /* a spell up: its id, aim kind, phase and target */
            size_t k = strlen(b[p]);
            const wl_skill *rec = (const wl_skill *)(intptr_t)cb->spell;
            snprintf(b[p] + k, sizeof b[p] - k, " spell=%d/aim%d/ph%d tgt=%d", (int)rec->id, (int)rec->aim, (int)cb->f58,
                     cb->target ? (int)((const wl_char *)(intptr_t)cb->target)->base.code : 0);
        }
    }
    char l[COOP_NIN][64];
    for (int p = 1; p < COOP_NIN; p++) {
        const coop_light *L = &g_light[p];
        if (L->on) snprintf(l[p], sizeof l[p], " l%d=%d,%d%s%s%s c%u", p + 1, (int)L->x, (int)L->y, L->magnet ? " mag" : "", L->drag ? " drag" : "", L->waved && (uint32_t)(g_eng.tick() - L->wave_at) < (uint32_t)g_tune.wave_ms ? " cd" : "", L->coins);
        else if (L->coins) snprintf(l[p], sizeof l[p], " l%d- c%u", p + 1, L->coins);
        else l[p][0] = 0;
    }
    char foe[64] = "";   /* the foe nearest the leader: where (centre), its HP */
    if (ld != NULL && ld->ch != NULL && ld->ch->base.body != NULL) {
        const wl_body *lb = ld->ch->base.body;
        int64_t best = -1;
        for (int i = 0; i < 32; i++) {
            const wl_char *c = (*g_eng.world)->chars[i];
            if (c == NULL || c->base.alive == 0 || c->base.body == NULL || c->party == NULL) continue;
            if (faction_relation(ld->ch->base.faction, c->base.faction) != 1) continue;
            const wl_body *fb = c->base.body;
            int64_t dx = fb->x - lb->x, dy = fb->y - lb->y, d = dx * dx + dy * dy;
            if (best >= 0 && d >= best) continue;
            best = d;
            snprintf(foe, sizeof foe, " foe=%d,%d/hp%d", (int)(fb->x + fb->w / 2), (int)(fb->y + fb->h / 2),
                     (int)c->party->hp);
        }
    }
    char it[64] = "";   /* the first item lying around: where, its body phase, its state */
    for (int k = 0; k < 96; k++) {
        const wl_entity *e = (*g_eng.world)->band96[k];
        if (e == NULL || e->alive == 0 || e->body == NULL || get32(e, 0x1fc) != 2) continue;
        snprintf(it, sizeof it, " item=%d,%d/ph%d/st%d", (int)e->body->x, (int)e->body->y, (int)e->body->f58,
                 (int)get32(e, 0x204));
        break;
    }
    snprintf(buf, n, " coop[nin=%u act=%d p1ctl=%d p2out=%d lead=%s p2=%s p3out=%d p3=%s gold=%d%s%s%s%s]", (unsigned)r->nin,
             nact, p1_in_control(r), g_out[1], a, b[1], g_out[2], b[2], (int)get32(r, 0x4014), l[1], l[2], it, foe);
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

int32_t coop_test_item(void)
{
    if (!g_coop || g_eng.world == NULL || *g_eng.world == NULL || (*g_eng.world)->room == NULL) return 0;
    const wl_pslot *ld = (*g_eng.world)->room->leader;
    if (ld == NULL || ld->ch == NULL || ld->ch->base.body == NULL) return 0;
    int32_t item = get32(g_eng.data(0x6c0cd0u), 0);   /* the item table's first row (736-byte rows, key first) */
    if (item == 0) return 0;
    wl_body at = *ld->ch->base.body;
    if (g_light[1].on) { at.x = g_light[1].x; at.y = g_light[1].y; at.w = at.h = 0; }   /* at player 2's light */
    return pickup_spawn(100162, &at, item) != NULL ? item : 0;
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
