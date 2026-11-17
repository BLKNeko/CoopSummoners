/* coop_light.c — the light of players 2 / 3 with no character, and its wave.  See coop.h; shared state in coop_internal.h. */
#include "coop_internal.h"

/* ---- the light: player 2 / 3 with no character ----
 *
 * A small light the player flies around the screen (through walls) while it plays no character (out, or
 * in and waiting for a free member).  It appears at the leader on the player's first direction / attack /
 * jump / use and goes after g_tune.light_idle_ms without input.  It takes the coins it touches (the gold as the
 * party's own pickup takes it, room_pickups 0x59ca90: the cap, the money total, the coin's "taken" state 3,
 * so the coin's own sound and rise play), never items (the leader's only).  Hold attack: the coins near it
 * fly to it (the magnet).  Hold jump: the first item on the ground it reaches hangs under it until jump is let go (the
 * leader takes it on touch, as always; let go, it falls); a jump press with no item near, or use: a ring where it is
 * (a ping for the others).  Each player's coins this session are counted under the light.  World units are
 * px x 100.
 *
 * A dropped coin / item lives on its body's phase (body +0x58, 0x46e190): 0 / 1 in the air (gravity, bounces),
 * 2 on the ground counting +0x5c to 500 updates, 3 blinking (entity +0xf4 alpha) until released 120 updates
 * later.  Pulled / carried ones go back to phase 1 with the count cleared and full alpha: they fall when let
 * go and their time on the ground starts again.  A carried item moves through the game's own movers
 * (0x54db10 / 0x54e5c0, as a dropped pickup falls): walls and floors stop it, and stuck g_tune.light_snag behind the
 * light it is let go where it is (never inside a wall). */




coop_light g_light[COOP_NIN];

static void light_axis(int32_t *v, int dir, int32_t max)
{
    if (dir != 0) {
        *v += dir * g_tune.light_accel;
        if (*v > max) *v = max;
        if (*v < -max) *v = -max;
    } else {
        *v = *v * 3 / 4;
    }
}

static void light_spawn(coop_light *L, const wl_room *r, const camera_view *v)
{
    const wl_pslot *ld = r->leader;
    if (ld != NULL && ld->ch != NULL && ld->ch->base.body != NULL) {
        const wl_body *b = ld->ch->base.body;
        L->x = b->x + b->w / 2;
        L->y = b->y + b->margin - 2500;
    } else {
        L->x = v->cur_x + v->vp_w / 2;
        L->y = v->cur_y + 100 * v->y_off + v->vp_h / 2;
    }
    L->on = 1;
    L->vx = L->vy = 0;
    L->tn = L->tk = 0;
}

/* a coin (kind 1) / item (2) still there to take (room_pickups' rule: not in the air, taken ...) */
static int band_lying(const wl_entity *e, int32_t kind)
{
    if (e == NULL || e->alive == 0 || e->body == NULL || get32(e, 0x1fc) != kind) return 0;
    int32_t st = get32(e, 0x204);
    if (st > 0 && st <= 4) return 0;
    return e->body->f58 < 100;   /* 100+: a taken pickup's rise */
}

/* moved by the light: back in the air (falls when let go), its time on the ground restarted, not blinking */
static void band_lift(wl_entity *e)
{
    wl_body *b = e->body;
    if (b->f58 >= 2) {
        if (b->f58 == 3) e->tint_f4 = get32(g_eng.sys, 10228);   /* the blink's full alpha (0x46e190 case 3) */
        b->f58 = 1;
    }
    b->act_ctr = 0;
    b->vvel = 0;
    b->vel_x = 0;
}

/* the coins the light reaches (and, with the magnet, pulls) */
static void light_coins(wl_room *r, coop_light *L, int p, uint32_t now)
{
    wl_world *wd = *g_eng.world;
    for (int k = 0; k < 96; k++) {
        wl_entity *e = wd->band96[k];
        if (!band_lying(e, 1) || get32(e, 0x200) <= 0) continue;   /* coins only */
        wl_body *b = e->body;
        int32_t dx = L->x - (b->x + b->w / 2), dy = L->y - (b->y + b->h / 2);
        if (L->magnet && dx > -g_tune.light_mag_r && dx < g_tune.light_mag_r && dy > -g_tune.light_mag_r && dy < g_tune.light_mag_r) {
            int32_t d = isqrt64((int64_t)dx * dx + (int64_t)dy * dy);
            if (d <= g_tune.light_mag_r) {
                int32_t sx = d <= g_tune.light_mag_step ? dx : (int32_t)((int64_t)dx * g_tune.light_mag_step / d);
                int32_t sy = d <= g_tune.light_mag_step ? dy : (int32_t)((int64_t)dy * g_tune.light_mag_step / d);
                b->x += sx;
                b->y += sy;
                band_lift(e);
                dx -= sx;
                dy -= sy;
            }
        }
        if (dx <= -(b->w / 2 + g_tune.light_r) || dx >= b->w / 2 + g_tune.light_r) continue;
        if (dy <= -(b->h / 2 + g_tune.light_r) || dy >= b->h / 2 + g_tune.light_r) continue;
        int32_t gold = get32(r, 0x4014), cap = get32(r, 0x4020), amt = get32(e, 0x200);
        if (cap <= gold) continue;                  /* the purse is full: the coin stays */
        int32_t g = gold + amt > cap ? cap : gold + amt;
        put32(r, 0x4014, g);
        uint32_t tot = (uint32_t)(amt + get32(r, 0x40bc));
        put32(r, 0x40bc, tot > 0x7fffffffu ? 0x7fffffff : (int32_t)tot);
        put32(e, 0x204, 3);
        L->coins += (uint32_t)(g - gold);
        if ((uint32_t)(now - L->gain_at) > LIGHT_GAIN_MS) L->gain = 0;
        L->gain += g - gold;
        L->gain_at = now;
        static int logged;
        if (logged++ < 40) coop_log("player %d's light took %d gold (%u this session)", p + 1, (int)(g - gold), L->coins);
    }
}

/* the nearest item on the ground within reach that no other light carries, or NULL */
static wl_entity *light_item_near(const coop_light *L)
{
    wl_world *wd = *g_eng.world;
    wl_entity *best = NULL;
    int64_t bd = 0;
    for (int k = 0; k < 96; k++) {
        wl_entity *e = wd->band96[k];
        if (!band_lying(e, 2)) continue;
        int taken = 0;
        for (int q = 1; q < COOP_NIN; q++) taken |= g_light[q].drag == e;
        if (taken) continue;
        const wl_body *b = e->body;
        int32_t dx = L->x - (b->x + b->w / 2), dy = L->y - (b->y + b->h / 2);
        if (dx <= -(b->w / 2 + g_tune.light_grab_r) || dx >= b->w / 2 + g_tune.light_grab_r) continue;
        if (dy <= -(b->h / 2 + g_tune.light_grab_r) || dy >= b->h / 2 + g_tune.light_grab_r) continue;
        int64_t d = (int64_t)dx * dx + (int64_t)dy * dy;
        if (best == NULL || d < bd) { best = e; bd = d; }
    }
    return best;
}

static void light_ping(coop_light *L, uint32_t now)
{
    L->px[L->pk] = L->x;
    L->py[L->pk] = L->y;
    L->pat[L->pk] = now;
    L->pk = (L->pk + 1) % LIGHT_PINGS;
}

static void light_drop(coop_light *L, int p, const char *why)
{
    if (L->drag == NULL) return;
    static int logged;
    if (logged++ < 40) coop_log("player %d's light %s item %d", p + 1, why, (int)get32(L->drag, 0x208));
    L->drag = NULL;
}

/* ---- the wave (a tap of attack): heals the allies in reach, hits the foes ----
 *
 * Heal: the healing item's own path (its use in menu_helpers.c: char_add_hp, the "%d" float in colour 7,
 * sound 85): g_tune.wave_heal x g_tune.wave_power of the max HP (hp_max + both bonuses, as char_add_hp caps it); fallen members
 * (0 HP) are not raised.  Hit: a pending hit queued on the foe (char_hit_queue 0x4891c0, as a sword's hit box
 * fills it), taken by the game's own char_take_hit: the defence, the damage number, the stagger / flinch (its
 * armour rules: bosses that never flinch don't), death, EXP and drops.  Its power: the leader's plain physical
 * hit (char_atk_roll 0x4481f0 at 100 %, the middle of its spread) less the foe's defence (char_take_hit's
 * (def stats) / 4) = what the leader would deal; g_tune.wave_dmg x g_tune.wave_power of that (at least 1), plus the defence back so
 * the game's subtraction lands on it.  No attacker (no combo, no record), no direction (no sideways knock),
 * rule 2 (a small hop). */

static struct { const void *who; uint32_t at; } g_wave_mark[48];

/* `who` was not reached by a wave within g_tune.wave_same_ms: mark it now */
static int wave_mark(const void *who, uint32_t now)
{
    int free_k = -1;
    for (int k = 0; k < 48; k++) {
        if (g_wave_mark[k].who == who) {
            if ((uint32_t)(now - g_wave_mark[k].at) < g_tune.wave_same_ms) return 0;
            g_wave_mark[k].at = now;
            return 1;
        }
        if (free_k < 0 && (g_wave_mark[k].who == NULL || (uint32_t)(now - g_wave_mark[k].at) >= g_tune.wave_same_ms))
            free_k = k;
    }
    if (free_k < 0) free_k = 0;
    g_wave_mark[free_k].who = who;
    g_wave_mark[free_k].at = now;
    return 1;
}

static int wave_reaches(const coop_light *L, const wl_body *b)
{
    int32_t dx = L->x - (b->x + b->w / 2), dy = L->y - (b->y + b->h / 2);
    int32_t rx = g_tune.wave_r + b->w / 2, ry = g_tune.wave_r + b->h / 2;
    if (dx <= -rx || dx >= rx || dy <= -ry || dy >= ry) return 0;
    /* inside the ellipse rx x ry (the reach grown by the half box): (dx / rx)^2 + (dy / ry)^2 <= 1 */
    return (int64_t)dx * dx * ry * ry + (int64_t)dy * dy * rx * rx <= (int64_t)rx * rx * ry * ry;
}

static void light_wave(wl_room *r, coop_light *L, int p, uint32_t now)
{
    if (L->waved && (uint32_t)(now - L->wave_at) < (uint32_t)g_tune.wave_ms) { L->deny_at = now; return; }
    L->waved = 1;
    L->wave_at = now;
    L->wave_x = L->x;
    L->wave_y = L->y;
    int healed = 0, hit = 0;
    for (int k = 0; k < 8; k++) {   /* the allies */
        wl_pslot *s = r->slot[k];
        if (s == NULL || s->active != 1 || s->ch == NULL || s->ch->base.body == NULL || s->ch->party == NULL) continue;
        wl_char *c = s->ch;
        wl_party *pt = c->party;
        int32_t max = pt->hp_max + pt->hp_bonus1 + pt->hp_bonus2;
        if (pt->hp <= 0 || pt->hp >= max || !wave_reaches(L, c->base.body) || !wave_mark(c, now)) continue;
        int32_t amt = (int32_t)((int64_t)max * g_tune.wave_heal * g_tune.wave_power / 1000), got = 0;
        if (amt < g_tune.wave_min) amt = g_tune.wave_min;
        char_add_hp(c, amt, &got, 0);
        if (got <= 0) continue;
        wl_body *b = c->base.body;
        float_text(c, (const char *)g_eng.data(0x854540u), got, b, 0, b->x + b->w / 2, b->y + b->h, b->facing, 7, 9, 60, -48000);   /* "%d" */
        sfx_play_at(&c->base, 0, 85, 0, 0, 0);
        healed++;
    }
    const wl_pslot *ld = r->leader;
    wl_char *lc = ld != NULL && ld->active == 1 ? ld->ch : NULL;
    wl_world *wd = *g_eng.world;
    static int32_t wave_id = 0x0c0f0000;
    for (int i = 0; lc != NULL && lc->party != NULL && i < 32; i++) {   /* the foes */
        wl_char *c = wd->chars[i];
        if (c == NULL || c->base.alive == 0 || c->base.body == NULL || c->party == NULL) continue;
        if (faction_relation(lc->base.faction, c->base.faction) != 1 || !char_targetable(c, 0, 1)) continue;
        if (get32(c, 0x15ab0) != 0 || !wave_reaches(L, c->base.body) || !wave_mark(c, now)) continue;
        const uint8_t *fp = (const uint8_t *)c->party;
        int32_t ds = get32(c, 0xc7dc) + get32(fp, 0x90) + get32(fp, 0x78) + get32(fp, 0x68);
        int32_t def = stat_jitter((ds < 0 ? 0 : ds) / 4, 1000, 500);   /* the middle of char_take_hit's defence */
        int32_t full = char_atk_roll(lc, 1000, 500, 0, 0) - def;   /* the leader's plain hit on this foe */
        if (full < 1) full = 1;
        int32_t dmg = (int32_t)((int64_t)full * g_tune.wave_dmg * g_tune.wave_power / 1000);
        if (dmg < 1) dmg = 1;
        const wl_body *b = c->base.body;
        char_hit_queue(c, (int32_t)(intptr_t)lc, 0, 99, 2, b->x + b->w / 2, b->y + b->h / 2, 0, 0, 0, 2, wave_id++,
                       dmg + def, 0, 0, 0, 0);
        coop_log("player %d's wave hits %d (hp %d): leader's hit %d, defence %d -> %d", p + 1, (int)c->base.code,
                 (int)c->party->hp, (int)full, (int)def, (int)dmg);
        hit++;
    }
    coop_log("player %d's wave: %d healed, %d hit", p + 1, healed, hit);
}

void coop_light_tick(wl_room *r)
{
    if (!g_coop || r == NULL || r->nin < 2 || r->in == NULL || g_eng.world == NULL || *g_eng.world == NULL) return;
    const camera_view *v = (*g_eng.world)->view;
    if (v == NULL) return;
    uint32_t now = g_eng.tick();
    int ctl = p1_in_control(r);
    for (int p = 1; p < COOP_NIN && p < r->nin; p++) {
        coop_light *L = &g_light[p];
        const uint8_t *h = g_held[p];
        int jump = h[COOP_A_JUMP], use = h[COOP_A_USE];
        if (!ctl || played(r, p) != NULL) {
            light_drop(L, p, "dropped");
            L->atk_down = h[COOP_A_ATTACK];   /* a press from before is a hold: no wave on its release */
            L->atk_at = now - g_tune.light_tap_ms;
            L->on = 0;
            L->prev_jump = jump;
            L->prev_use = use;
            continue;
        }
        int any = h[COOP_A_UP] || h[COOP_A_DOWN] || h[COOP_A_LEFT] || h[COOP_A_RIGHT] || h[COOP_A_ATTACK] || jump || use;
        if (!L->on) {
            if (!any) { L->prev_jump = jump; L->prev_use = use; continue; }
            light_spawn(L, r, v);
            L->prev_jump = jump;   /* the press that brought it does nothing else */
            L->prev_use = use;
            L->atk_down = h[COOP_A_ATTACK];
            L->atk_at = now - g_tune.light_tap_ms;
        }
        if (any) L->last_input = now;
        if ((uint32_t)(now - L->last_input) > g_tune.light_idle_ms) { light_drop(L, p, "dropped"); L->on = 0; continue; }
        int dx = (int)h[COOP_A_RIGHT] - (int)h[COOP_A_LEFT], dy = (int)h[COOP_A_DOWN] - (int)h[COOP_A_UP];
        int32_t max = dx != 0 && dy != 0 ? g_tune.light_speed * 7 / 10 : g_tune.light_speed;
        light_axis(&L->vx, dx, max);
        light_axis(&L->vy, dy, max);
        L->x += L->vx;
        L->y += L->vy;
        int32_t x0 = v->cur_x + g_tune.light_margin, x1 = v->cur_x + v->vp_w - g_tune.light_margin;
        int32_t y0 = v->cur_y + 100 * v->y_off + g_tune.light_margin, y1 = y0 + v->vp_h - 2 * g_tune.light_margin;
        if (L->x < x0 - 32000 || L->x > x1 + 32000 || L->y < y0 - 32000 || L->y > y1 + 32000) {
            light_drop(L, p, "dropped");
            light_spawn(L, r, v);   /* the view jumped (a new room): back to the leader */
        }
        if (L->x < x0) { L->x = x0; L->vx = 0; }
        if (L->x > x1) { L->x = x1; L->vx = 0; }
        if (L->y < y0) { L->y = y0; L->vy = 0; }
        if (L->y > y1) { L->y = y1; L->vy = 0; }
        if (L->tk++ % 2 == 0) {   /* the trail: a point every other update */
            memmove(L->tx + 1, L->tx, sizeof L->tx - sizeof L->tx[0]);
            memmove(L->ty + 1, L->ty, sizeof L->ty - sizeof L->ty[0]);
            L->tx[0] = L->x;
            L->ty[0] = L->y;
            if (L->tn < LIGHT_TRAIL) L->tn++;
        }
        if (L->drag != NULL && !band_lying(L->drag, 2)) {   /* taken (the leader touched it) or gone */
            static int logged;
            if (logged++ < 40) coop_log("player %d's light: its item was taken", p + 1);
            L->drag = NULL;
        }
        if (jump && L->drag == NULL) {   /* held: the first item it reaches (a press with none near pings) */
            wl_entity *e = light_item_near(L);
            if (e != NULL) {
                L->drag = e;
                static int logged;
                if (logged++ < 40) coop_log("player %d's light picked up item %d", p + 1, (int)get32(e, 0x208));
            } else if (!L->prev_jump) {
                light_ping(L, now);
            }
        }
        if (!jump) light_drop(L, p, "let go of");
        if (use && !L->prev_use) light_ping(L, now);
        L->prev_jump = jump;
        L->prev_use = use;
        if (L->drag != NULL) {   /* pulled under the light through the game's own movers: walls and floors stop it */
            wl_body *b = L->drag->body;
            int32_t tx = L->x - b->w / 2, ty = L->y + 1100;   /* its top just under the glow */
            int32_t mx = tx - b->x, my = ty - b->y;
            if (mx > g_tune.light_drag_step) mx = g_tune.light_drag_step;
            if (mx < -g_tune.light_drag_step) mx = -g_tune.light_drag_step;
            if (my > g_tune.light_drag_step) my = g_tune.light_drag_step;
            if (my < -g_tune.light_drag_step) my = -g_tune.light_drag_step;
            if (mx != 0) entity_move_horizontal(L->drag, b, mx, 1, 1, 0, 0, 0);
            if (my != 0) {
                int32_t ny = b->y;
                entity_move_vertical(L->drag, b, my, 1, 0, &ny, NULL);   /* down through one-way floors */
                b->y = ny;
            }
            band_lift(L->drag);
            int32_t ex = tx - b->x, ey = ty - b->y;
            if (ex > g_tune.light_snag || ex < -g_tune.light_snag || ey > g_tune.light_snag || ey < -g_tune.light_snag)
                light_drop(L, p, "snagged (wall / floor), dropped");
        }
        if (h[COOP_A_ATTACK] && !L->atk_down) { L->atk_down = 1; L->atk_at = now; }
        else if (!h[COOP_A_ATTACK] && L->atk_down) {
            L->atk_down = 0;
            if ((uint32_t)(now - L->atk_at) < g_tune.light_tap_ms) light_wave(r, L, p, now);
        }
        L->magnet = L->atk_down && (uint32_t)(now - L->atk_at) >= g_tune.light_tap_ms;
        light_coins(r, L, p, now);
        L->seen = now;
    }
}

