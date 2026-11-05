/* coop.c — local co-op.  See coop.h. */
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

uint16_t coop_room_inputs(void) { return g_coop ? COOP_NIN : 1; }

/* manager 1: joystick 1, plus the P2 keyboard map (coop_p2_produce, keyed on src_kb == 2) */
void coop_inputs_init(wl_room *r)
{
    if (!g_coop || r->nin < COOP_NIN) return;
    input_init(r, 1, 2, 0, 1);
}

/* the keyboard device wrapper, as input_dik.c reads it */
typedef struct p2_dev {
    uint8_t         _p00[0x10];
    uint32_t        n;
    const uint32_t *ev;
    uint8_t         state[256];
} p2_dev;

typedef struct { uint8_t key; uint8_t id; } p2_map;
/* numpad 8 5 4 6 = up down left right (Up also posts 35, as the arrow does) */
static const p2_map P2_DIRS[4]     = { { 0x48, 1 }, { 0x4c, 3 }, { 0x4b, 2 }, { 0x4d, 4 } };
static const p2_map P2_DIRS_ALT[4] = { { 0x48, 12 }, { 0x4c, 13 }, { 0x4b, 14 }, { 0x4d, 15 } };
/* the same id fan-out as the bound keys in input_dik.c: attack (+0x548), jump (+0x564),
 * use (+0x59c), sheathe / skill modifier (+0x580), switch (+0x5b8) */
static const p2_map P2_KEYS[] = {
    { 0x48, 35 },
    { 0x4f, 8 }, { 0x4f, 37 }, { 0x4f, 36 },
    { 0x50, 7 }, { 0x50, 39 },
    { 0x51, 38 }, { 0x51, 36 },
    { 0x52, 9 }, { 0x52, 17 },
    { 0x4e, 20 },
};
#define P2_MOD 0x52   /* numpad 0: the skill modifier, like keycfg +0x590 */

static int p2_held(const p2_dev *d, uint8_t k) { return (d->state[k] & 0x80) != 0; }

static void p2_push(input_mgr *m, int32_t id, uint32_t ts, int32_t down)
{
    input_event *r = m->ring[INPUT_RING_LEN - 1];
    for (int k = INPUT_RING_LEN - 1; k != 0; k--) m->ring[k] = m->ring[k - 1];
    m->ring[0] = r;
    r->id = id;
    r->ts = ts;
    r->flag = down != 0;
}

void coop_p2_produce(input_mgr *m, const void *kb)
{
    const p2_dev *d = kb;
    uint32_t skills_on;
    memcpy(&skills_on, (const uint8_t *)**g_eng.keycfg + 0x104, 4);
    int alt_mode = skills_on != 0 && p2_held(d, P2_MOD);
    for (uint32_t i = 0; i < d->n; i++) {
        const uint32_t *e = d->ev + 4 * i;
        const p2_map *dirs = alt_mode ? P2_DIRS_ALT : P2_DIRS;
        for (int k = 0; k < 4; k++)
            if (e[0] == dirs[k].key) p2_push(m, dirs[k].id, e[2], (int32_t)(e[1] & 0x80));
        for (size_t k = 0; k < sizeof P2_KEYS / sizeof P2_KEYS[0]; k++)
            if (e[0] == P2_KEYS[k].key) p2_push(m, P2_KEYS[k].id, e[2], (int32_t)(e[1] & 0x80));
    }
    if (!alt_mode) {
        if (p2_held(d, 0x48)) m->axis_held[0] = 1;
        if (p2_held(d, 0x4c)) m->axis_held[1] = 1;
        if (p2_held(d, 0x4b)) m->axis_held[2] = 1;
        if (p2_held(d, 0x4d)) m->axis_held[3] = 1;
    }
    if (p2_held(d, 0x4f)) m->axis_held[5] = 1;
    if (p2_held(d, 0x50)) m->axis_held[4] = 1;
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
        FILE *f = fopen("coop.log", "a");
        if (f != NULL) {
            fprintf(f, "pad %d: poll failed hr=0x%08x at tick %u, re-acquiring\n", k, (unsigned)hr,
                    (unsigned)g_eng.tick());
            fclose(f);
        }
        seen[k].last_hr = hr;
    }
    return seen[k].n++ % PAD_RETRY_POLLS == 0;
}

wl_pslot *coop_p2_slot(const wl_room *r)
{
    if (r->nin < COOP_NIN) return NULL;
    const input_mgr *p2 = &r->in[1];
    for (int k = 0; k < 8; k++) {
        wl_pslot *s = r->slot[k];
        if (s != NULL && s->in == p2) return s;
    }
    return NULL;
}

wl_pslot *coop_leader_fallback(const wl_room *r, const wl_pslot *cur)
{
    if (!g_coop || r == NULL) return NULL;
    if (cur != NULL && cur->active == 1 && cur->ch != NULL) return NULL;   /* the leader can still lead */
    wl_pslot *pick = coop_p2_slot(r);
    if (pick == NULL || pick == cur || pick->active != 1 || pick->id == 100000104 || pick->ch == NULL) {
        pick = NULL;
        for (int k = 0; k < 8 && pick == NULL; k++) {
            wl_pslot *s = r->slot[k];
            if (s != NULL && s != cur && s->active == 1 && s->id != 100000104 && s->ch != NULL) pick = s;
        }
    }
    static int logged;
    if (logged < 8) {   /* a few lines into coop.log beside the game: which member took the lead and why */
        FILE *f = fopen("coop.log", "a");
        if (f != NULL) {
            fprintf(f, "leader fallback at tick %u: leader %d (active %d, ch %s) -> %d (%s)\n",
                    (unsigned)g_eng.tick(), cur != NULL ? (int)cur->code : 0, cur != NULL ? (int)cur->active : 0,
                    cur != NULL && cur->ch != NULL ? "yes" : "no", pick != NULL ? (int)pick->code : 0,
                    pick == NULL ? "nobody" : pick == coop_p2_slot(r) ? "P2's member" : "a member present");
            fclose(f);
        }
        logged++;
    }
    return pick;
}

/* player 1 is in control: the leader's character runs on manager 0 (scripts clear the
 * character's +0x158a4 / set +0x200 directly, so look at the character, not the slot) */
static int p1_in_control(const wl_room *r)
{
    const wl_pslot *ld = r->leader;
    if (ld == NULL || ld->active != 1 || ld->ch == NULL || ld->in != &r->in[0]) return 0;
    const wl_char *c = ld->ch;
    return c->input == &r->in[0] && c->f200 == 0;
}

static int p2_can_take(const wl_room *r, const wl_pslot *s)
{
    return s != NULL && s != r->leader && s->active == 1 && s->id != 100000104 && s->in == NULL &&
           s->ch != NULL && char_targetable(s->ch, 0, 1);
}

/* ---- the switch key: taps, the LEGO-style swap, P2's hold to leave / rejoin ----
 *
 * While co-op runs, coop_tick takes every switch event (ring id 20, posted by Enter / the bound
 * switch key / pad button, P2's numpad +) off both managers before room_keys (0x468a20 runs
 * before it in the room update) can see it.  A press and its release make a TAP (counted at the
 * release) or a HOLD (>= COOP_HOLD_MS):
 *   - both players tap within COOP_SWAP_MS: they swap characters (fallen ones too); the leader
 *     goes with P1's manager (room_leader_set), P2's manager goes to P1's old character;
 *   - a lone tap, once the window has passed (at once when no swap is possible): a fresh id 20
 *     press is posted back into that player's ring, so room_keys does the game's own switch to
 *     a free member (its 40-tick retry included);
 *   - P2 holds: leave (the AI takes the character back) / rejoin (the first free member);
 *     P1's hold is just a tap. */

#define SWITCH_ID     20
#define COOP_SWAP_MS  500
#define COOP_HOLD_MS  1500

typedef struct sw_state {
    int          down;        /* the switch is held (a press seen, its release not yet)  */
    uint32_t     down_at;
    int          hold_fired;
    int          tap;         /* a tap waiting for the swap window                       */
    uint32_t     tap_at;
    input_event *posted;      /* the press coop posted for room_keys: not a player's     */
} sw_state;

static sw_state g_sw[COOP_NIN];
static int g_p2_out;          /* 1 = P2 left; the AI drives the character               */

int coop_p2_joined(void) { return g_coop && !g_p2_out; }

static void sw_reset(void)
{
    for (int p = 0; p < COOP_NIN; p++) {
        g_sw[p].down = g_sw[p].hold_fired = g_sw[p].tap = 0;
    }
}

/* the game's own switch for player p: a press for room_keys' input_poll_consume (id 20, flag 1,
 * stamped now: inside its 100 ms window) */
static void sw_post(input_mgr *m, sw_state *s, uint32_t now)
{
    p2_push(m, SWITCH_ID, now, 1);
    s->posted = m->ring[0];
}

/* both players' characters change hands; P1's manager keeps the leader */
static void sw_swap(wl_room *r)
{
    wl_pslot *s1 = r->leader, *s2 = coop_p2_slot(r);
    if (s1 == NULL || s2 == NULL || s1 == s2 || s1->in != &r->in[0]) return;
    room_leader_set(r, s2);              /* s2 takes manager 0 and the lead, s1 goes to the AI */
    pslot_input_set(s1, &r->in[1]);      /* ... and then to P2 */
    room_formation(r);
}

static int sw_can_swap(const wl_room *r)
{
    return !g_p2_out && coop_p2_slot(r) != NULL && r->leader != NULL && r->leader->in == &r->in[0];
}

static void sw_tap(wl_room *r, int p, uint32_t t, uint32_t now)
{
    sw_state *s = &g_sw[p], *o = &g_sw[1 - p];
    if (p == 1 && g_p2_out) return;                 /* not in the game: nothing to switch */
    if (!sw_can_swap(r)) { sw_post(&r->in[p], s, now); return; }
    int32_t gap = (int32_t)(t - o->tap_at);
    if (o->tap && (gap < 0 ? -gap : gap) <= COOP_SWAP_MS) {
        o->tap = 0;
        sw_swap(r);
        return;
    }
    s->tap = 1;
    s->tap_at = t;
}

static void sw_hold(wl_room *r, int p)
{
    if (p != 1) return;
    wl_pslot *held = coop_p2_slot(r);
    if (!g_p2_out) {
        if (held != NULL) pslot_input_set(held, NULL);
        g_p2_out = 1;
        g_sw[1].tap = 0;
    } else {
        g_p2_out = 0;                                /* coop_tick attaches P2 to a free member */
    }
}

static void sw_update(wl_room *r)
{
    uint32_t now = g_eng.tick();
    for (int p = 0; p < COOP_NIN; p++) {
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
        if (s->down && !s->hold_fired && p == 1 && (uint32_t)(now - s->down_at) >= COOP_HOLD_MS) {
            s->hold_fired = 1;
            sw_hold(r, p);
        }
    }
    for (int p = 0; p < COOP_NIN; p++) {
        sw_state *s = &g_sw[p];
        if (s->tap && (uint32_t)(now - s->tap_at) > COOP_SWAP_MS) {
            s->tap = 0;
            sw_post(&r->in[p], s, now);
        }
    }
}

void coop_tick(wl_room *r)
{
    if (!g_coop || r->nin < COOP_NIN || r->in == NULL) return;
    input_mgr *p2 = &r->in[1];
    wl_pslot *held = coop_p2_slot(r);
    if (!p1_in_control(r)) {
        if (held != NULL) pslot_input_set(held, NULL);
        sw_reset();
        return;
    }
    sw_update(r);
    held = coop_p2_slot(r);
    if (g_p2_out) {
        if (held != NULL) pslot_input_set(held, NULL);
        return;
    }
    if (held != NULL) {
        /* a script handed the character to the AI without clearing the slot: take it back */
        if (held->active == 1 && held->ch != NULL && (held->ch->input != p2 || held->ch->f200 != 0))
            pslot_input_set(held, p2);
        if (held->active == 1) return;
        pslot_input_set(held, NULL);
    }
    for (int k = 0; k < 8; k++)
        if (p2_can_take(r, r->slot[k])) {
            pslot_input_set(r->slot[k], p2);
            return;
        }
}

/* a one-line state summary for host logs: the leader's and P2's characters (code, x, y, +0x200) */
void coop_status(char *buf, size_t n)
{
    buf[0] = 0;
    if (!g_coop || g_eng.world == NULL || *g_eng.world == NULL) return;
    const wl_room *r = (*g_eng.world)->room;
    if (r == NULL) { snprintf(buf, n, " coop[no room]"); return; }
    const wl_pslot *ld = r->leader, *p2 = coop_p2_slot(r);
    int nact = 0;
    for (int k = 0; k < 8; k++) if (r->slot[k] != NULL && r->slot[k]->active == 1) nact++;
    char a[96] = "-", b[96] = "-";
    if (ld != NULL && ld->ch != NULL && ld->ch->base.body != NULL)
        snprintf(a, sizeof a, "%d@%d,%d/%d hp%d%s act%d sk%d st%d pose%d", (int)ld->code, (int)ld->ch->base.body->x,
                 (int)ld->ch->base.body->y, (int)ld->ch->f200, (int)ld->ch->party->hp,
                 char_targetable(ld->ch, 0, 1) ? "" : " DOWN", (int)ld->ch->cmd.act, (int)ld->ch->cmd.skill,
                 (int)ld->ch->base.body->state, (int)ld->ch->base.body->pose);
    if (p2 != NULL && p2->ch != NULL && p2->ch->base.body != NULL)
        snprintf(b, sizeof b, "%d@%d,%d/%d hp%d%s", (int)p2->code, (int)p2->ch->base.body->x,
                 (int)p2->ch->base.body->y, (int)p2->ch->f200, (int)p2->ch->party->hp,
                 char_targetable(p2->ch, 0, 1) ? "" : " DOWN");
    snprintf(buf, n, " coop[nin=%u act=%d p1ctl=%d p2out=%d lead=%s p2=%s]", (unsigned)r->nin, nact,
             p1_in_control(r), g_p2_out, a, b);
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
    if (!g_coop || g_p2_out || leader == NULL || coop_casts(leader->ch) || g_eng.world == NULL ||
        *g_eng.world == NULL) return leader;
    const wl_room *r = (*g_eng.world)->room;
    if (r == NULL || r->nin < COOP_NIN || !p1_in_control(r)) return leader;
    wl_pslot *p2 = coop_p2_slot(r);
    if (p2 != NULL && p2->active == 1 && p2->ch != NULL && p2->ch->f200 == 0 && coop_casts(p2->ch)) return p2;
    return leader;
}

/* test only (rhost --coop-test-hp): set the HP of the leader's (who 0) or P2's (who 1) character */
void coop_test_hp(int32_t who, int32_t hp)
{
    if (!g_coop || g_eng.world == NULL || *g_eng.world == NULL || (*g_eng.world)->room == NULL) return;
    const wl_room *r = (*g_eng.world)->room;
    const wl_pslot *s = who == 0 ? r->leader : coop_p2_slot(r);
    if (s != NULL && s->ch != NULL && s->ch->party != NULL) s->ch->party->hp = hp;
}

/* test only (rhost --coop-test-drop): put the leader's (who 0) or P2's (who 1) character far below
 * the map, as a fall into a bottomless pit does (char_apply's "fell" check) */
void coop_test_drop(int32_t who)
{
    if (!g_coop || g_eng.world == NULL || *g_eng.world == NULL || (*g_eng.world)->room == NULL) return;
    const wl_room *r = (*g_eng.world)->room;
    const wl_pslot *s = who == 0 ? r->leader : coop_p2_slot(r);
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

/* ---- the P2 marker overlay ---- */

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
static const uint8_t GLYPH_P[7] = { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 };
static const uint8_t GLYPH_2[7] = { 0x0e, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1f };

static void ov_glyph(const ov_surf *s, const uint8_t *g, int32_t x, int32_t y, uint32_t c)
{
    for (int j = 0; j < 7; j++)
        for (int i = 0; i < 5; i++)
            if (g[j] >> (4 - i) & 1) ov_put(s, x + i, y + j, c);
}

#define BADGE_W 17
#define BADGE_H 13

/* the "P2" badge with its top-left at (x, y) */
static void ov_badge(const ov_surf *s, int32_t x, int32_t y, uint32_t edge, uint32_t fill, uint32_t ink)
{
    ov_rect(s, x + 1, y, BADGE_W - 2, BADGE_H, edge);
    ov_rect(s, x, y + 1, BADGE_W, BADGE_H - 2, edge);
    ov_rect(s, x + 1, y + 1, BADGE_W - 2, BADGE_H - 2, fill);
    ov_glyph(s, GLYPH_P, x + 3, y + 3, ink);
    ov_glyph(s, GLYPH_2, x + 9, y + 3, ink);
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

/* where P2's marker goes this frame: 0 = none, else *side = 0 above the character at (sx, sy)
 * (its head, screen px), -1 / +1 = off the left / right edge, sy clamped */
static int coop_marker(int32_t sw, int32_t sh, int32_t *sx, int32_t *sy, int32_t *side)
{
    if (!g_coop || g_eng.world == NULL || *g_eng.world == NULL) return 0;
    const wl_world *wd = *g_eng.world;
    const wl_room *r = wd->room;
    const camera_view *v = wd->view;
    if (r == NULL || v == NULL || !p1_in_control(r)) return 0;
    const wl_pslot *p2 = coop_p2_slot(r);
    if (p2 == NULL || p2->active != 1 || p2->ch == NULL || p2->ch->base.body == NULL) return 0;
    const wl_body *b = p2->ch->base.body;
    int32_t x, y;
    view_project(v, b->x + b->w / 2, b->y + b->margin, 0, 0, 0, 0, &x, &y, 0);
    *side = x < 0 ? -1 : x >= sw ? 1 : 0;
    if (*side != 0) {   /* the edge arrow stays clear of the HUD: portrait top-left, bars along the bottom */
        if (y < 100) y = 100;
        if (y > sh - 95) y = sh - 95;
    } else if (y < 10 + BADGE_H + 10) {
        y = 10 + BADGE_H + 10;   /* the badge stays on screen */
    }
    *sx = x;
    *sy = y;
    return 1;
}

void coop_overlay_draw(uint8_t *px, int32_t pitch, int32_t w, int32_t h, int32_t bpp,
                       uint32_t rmask, uint32_t gmask, uint32_t bmask)
{
    if (!g_coop || px == NULL || (bpp != 16 && bpp != 32)) return;
    ov_surf s = { px, pitch, w, h, bpp, rmask, gmask, bmask };
    int32_t x, y, side;
    if (!coop_marker(w, h, &x, &y, &side)) return;
    uint32_t edge = ov_rgb(&s, 16, 24, 48), fill = ov_rgb(&s, 48, 144, 255), ink = ov_rgb(&s, 255, 255, 255);
    if (side == 0) {
        ov_arrow(&s, x, y - 4, 0, 5, edge, fill);
        ov_badge(&s, x - BADGE_W / 2, y - 10 - BADGE_H, edge, fill, ink);
    } else if (side < 0) {
        ov_arrow(&s, 3, y, -1, 9, edge, fill);
        ov_badge(&s, 15, y - BADGE_H / 2, edge, fill, ink);
    } else {
        ov_arrow(&s, w - 4, y, 1, 9, edge, fill);
        ov_badge(&s, w - 16 - BADGE_W, y - BADGE_H / 2, edge, fill, ink);
    }
}
