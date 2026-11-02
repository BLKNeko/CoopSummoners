/* coop.c — local co-op.  See coop.h. */
#include "coop.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "char_state.h"
#include "engine_calls.h"
#include "input.h"
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

void coop_tick(wl_room *r)
{
    if (!g_coop || r->nin < COOP_NIN || r->in == NULL) return;
    input_mgr *p2 = &r->in[1];
    wl_pslot *held = coop_p2_slot(r);
    if (!p1_in_control(r)) {
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
        snprintf(a, sizeof a, "%d@%d,%d/%d", (int)ld->code, (int)ld->ch->base.body->x, (int)ld->ch->base.body->y,
                 (int)ld->ch->f200);
    if (p2 != NULL && p2->ch != NULL && p2->ch->base.body != NULL)
        snprintf(b, sizeof b, "%d@%d,%d/%d", (int)p2->code, (int)p2->ch->base.body->x, (int)p2->ch->base.body->y,
                 (int)p2->ch->f200);
    snprintf(buf, n, " coop[nin=%u act=%d p1ctl=%d lead=%s p2=%s]", (unsigned)r->nin, nact, p1_in_control(r), a, b);
}
