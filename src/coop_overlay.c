/* coop_overlay.c — the overlay: markers, notices, the lights, the spell sight.  See coop.h; shared state in coop_internal.h. */
#include "coop_internal.h"

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
    { '3', { 0x1f, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0e } }, { '0', { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } },
    { '1', { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e } }, { '4', { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 } },
    { '5', { 0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e } }, { '6', { 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e } },
    { '7', { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } }, { '8', { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e } },
    { '9', { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c } }, { '+', { 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00 } },
    { 'C', { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e } }, { 'M', { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 } },
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
static int coop_marker(int p, int32_t sw, int32_t sh, int32_t *sx, int32_t *sy, int32_t *side, int *down)
{
    if (!g_coop || g_eng.world == NULL || *g_eng.world == NULL) return 0;
    const wl_world *wd = *g_eng.world;
    const wl_room *r = wd->room;
    const camera_view *v = wd->view;
    if (r == NULL || v == NULL || !p1_in_control(r)) return 0;
    if (p == 0 && g_cam == 0) return 0;   /* player 1's badge only while the camera follows someone else */
    const wl_pslot *s = p == 0 ? r->leader : coop_player_slot(r, p);
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
    *down = coop_player_down(r, p);
    return 1;
}

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
    *fill = p == 2 ? ov_rgb(s, 150, 70, 220) : p == 0 ? ov_rgb(s, 230, 90, 160)   /* player 3 purple (Stella), */
                                                      : ov_rgb(s, 48, 144, 255);  /* 1 pink (Arche), 2 blue */
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
        y = ov_row(s, y, t, now - k->down_at, (uint32_t)g_tune.kick_ms, edge, fill, ink);
    }
    if (g_cam_noted && (uint32_t)(now - g_cam_at) < (uint32_t)g_tune.notice_ms) {   /* the camera changed hands */
        snprintf(t, sizeof t, "CAMERA P%d", g_cam + 1);
        player_colors(s, g_cam, &edge, &fill);
        y = ov_row(s, y, t, 0, 0, edge, fill, ink);
    }
    for (int p = 1; p < COOP_NIN; p++) {
        const sw_state *h = &g_sw[p];
        player_colors(s, p, &edge, &fill);
        if (h->down && !h->hold_fired && (uint32_t)(now - h->down_at) >= BAR_AFTER) {
            if (g_out[p]) snprintf(t, sizeof t, g_ui_pt ? "P%d ENTRANDO" : "P%d JOINING", p + 1);
            else snprintf(t, sizeof t, g_ui_pt ? "P%d SAINDO" : "P%d LEAVING", p + 1);
            y = ov_row(s, y, t, now - h->down_at, g_out[p] ? (uint32_t)g_tune.join_ms : (uint32_t)g_tune.leave_ms, edge, fill, ink);
            continue;
        }
        if (g_notice[p] == NOTICE_NONE || (uint32_t)(now - g_notice_at[p]) >= (uint32_t)g_tune.notice_ms) continue;
        const char *w = g_notice[p] == NOTICE_JOINED  ? (g_ui_pt ? "ENTROU" : "JOINED")
                      : g_notice[p] == NOTICE_WAITING ? (g_ui_pt ? "ESPERANDO" : "WAITING")
                      : g_notice[p] == NOTICE_DOWN    ? (g_ui_pt ? "CAIU" : "DOWN")
                                                      : (g_ui_pt ? "SAIU" : "LEFT");
        snprintf(t, sizeof t, "P%d %s", p + 1, w);
        y = ov_row(s, y, t, 0, 0, edge, fill, ink);
    }
}

/* ---- the lights: glow, trail, magnet ring, pings, the coin counter ---- */

static uint8_t ov_unchan(uint32_t mask, uint32_t c)
{
    if (mask == 0) return 0;
    int lo = 0, bits = 0;
    while (!(mask >> lo & 1)) lo++;
    while (lo + bits < 32 && (mask >> (lo + bits) & 1)) bits++;
    uint32_t v = (c & mask) >> lo;
    return (uint8_t)(bits >= 8 ? v >> (bits - 8) : (v << (8 - bits)) | (v >> (2 * bits > 8 ? 2 * bits - 8 : 0)));
}

/* (r, g, b) over the pixel at a / 255 */
static void ov_blend(const ov_surf *s, int32_t x, int32_t y, uint8_t r, uint8_t g, uint8_t b, int32_t a)
{
    if (x < 0 || y < 0 || x >= s->w || y >= s->h || a <= 0) return;
    if (a > 255) a = 255;
    uint8_t *p = s->px + (size_t)y * (size_t)s->pitch;
    uint32_t c = 0;
    if (s->bpp == 16) { uint16_t v; memcpy(&v, p + 2 * x, 2); c = v; }
    else memcpy(&c, p + 4 * x, 4);
    int32_t r0 = ov_unchan(s->rm, c), g0 = ov_unchan(s->gm, c), b0 = ov_unchan(s->bm, c);
    ov_put(s, x, y, ov_rgb(s, (uint8_t)(r0 + (r - r0) * a / 255), (uint8_t)(g0 + (g - g0) * a / 255),
                           (uint8_t)(b0 + (b - b0) * a / 255)));
}

/* a soft disc: `a` at the centre, fading to 0 at radius rad */
static void ov_glow(const ov_surf *s, int32_t cx, int32_t cy, int32_t rad, uint8_t r, uint8_t g, uint8_t b, int32_t a)
{
    int32_t r2 = rad * rad;
    for (int32_t j = -rad; j <= rad; j++)
        for (int32_t i = -rad; i <= rad; i++) {
            int32_t d2 = i * i + j * j;
            if (d2 >= r2) continue;
            int32_t f = 256 - d2 * 256 / r2;   /* 1 - (d / rad)^2 */
            ov_blend(s, cx + i, cy + j, r, g, b, a * f / 256);
        }
}

/* a ring of radius rad, `thick` px either side */
static void ov_ring(const ov_surf *s, int32_t cx, int32_t cy, int32_t rad, int32_t thick, uint8_t r, uint8_t g,
                    uint8_t b, int32_t a)
{
    int32_t out = rad + thick;
    for (int32_t j = -out; j <= out; j++)
        for (int32_t i = -out; i <= out; i++) {
            int32_t d = isqrt64((int64_t)i * i + (int64_t)j * j) - rad;
            if (d < 0) d = -d;
            if (d > thick) continue;
            ov_blend(s, cx + i, cy + j, r, g, b, a * (thick + 1 - d) / (thick + 1));
        }
}

static void ov_text_outlined(const ov_surf *s, const char *t, int32_t x, int32_t y, uint32_t edge, uint32_t ink)
{
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx || dy) ov_text(s, t, x + dx, y + dy, 1, edge);
    ov_text(s, t, x, y, 1, ink);
}

static void player_rgb(int p, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (p == 2) { *r = 170; *g = 90; *b = 255; }   /* Stella's purple */
    else { *r = 80; *g = 170; *b = 255; }
}

/* sin x 1000 in 32 steps (a turn) */
static const int16_t SIN32[32] = { 0, 195, 383, 556, 707, 831, 924, 981, 1000, 981, 924, 831, 707, 556, 383, 195,
                                   0, -195, -383, -556, -707, -831, -924, -981, -1000, -981, -924, -831, -707,
                                   -556, -383, -195 };

/* an arc of radius rad from the top, clockwise, over `el` / `total` of a turn (256 dots) */
static void ov_arc(const ov_surf *s, int32_t cx, int32_t cy, int32_t rad, uint32_t el, uint32_t total, uint8_t r,
                   uint8_t g, uint8_t b, int32_t a)
{
    uint32_t n = total ? (uint32_t)((uint64_t)el * 256 / total) : 256;
    for (uint32_t k = 0; k < n && k < 256; k++) {
        int i = (int)(k / 8), f = (int)(k % 8);   /* between table steps i and i + 1 */
        int32_t sx = (SIN32[i] * (8 - f) + SIN32[(i + 1) % 32] * f) / 8;
        int32_t cxv = (SIN32[(i + 8) % 32] * (8 - f) + SIN32[(i + 9) % 32] * f) / 8;
        ov_blend(s, cx + sx * rad / 1000, cy - cxv * rad / 1000, r, g, b, a);
        ov_blend(s, cx + sx * (rad + 1) / 1000, cy - cxv * (rad + 1) / 1000, r, g, b, a);
        ov_blend(s, cx + sx * (rad - 1) / 1000, cy - cxv * (rad - 1) / 1000, r, g, b, a / 2);
    }
}

static void ov_lights(const ov_surf *s)
{
    if (g_eng.world == NULL || *g_eng.world == NULL) return;
    const camera_view *v = (*g_eng.world)->view;
    if (v == NULL) return;
    uint32_t now = g_eng.tick();
    for (int p = 1; p < COOP_NIN; p++) {
        coop_light *L = &g_light[p];
        if ((uint32_t)(now - L->seen) > 150) continue;   /* not updated: a menu, a pause, a cutscene */
        uint8_t cr, cg, cb;
        player_rgb(p, &cr, &cg, &cb);
        int32_t x, y;
        for (int k = 0; k < LIGHT_PINGS; k++) {   /* the pings (stay where they were made) */
            uint32_t age = now - L->pat[k];
            if (L->pat[k] == 0 || age >= LIGHT_PING_MS) continue;
            view_project(v, L->px[k], L->py[k], 0, 0, 0, 0, &x, &y, 0);
            for (uint32_t w = 0; w < 3; w++) {   /* three waves, 150 ms apart */
                uint32_t wa = age - w * 150;
                if (age < w * 150) break;
                int32_t a = 255 - (int32_t)(wa * 255 / LIGHT_PING_MS), rad = 8 + (int32_t)(wa * 44 / LIGHT_PING_MS);
                ov_ring(s, x, y, rad, 3, cr, cg, cb, a);
                ov_ring(s, x, y, rad, 1, 255, 255, 255, a * 3 / 4);
            }
        }
        uint32_t wage = now - L->wave_at;
        if (L->waved && wage < LIGHT_WAVE_SHOW) {   /* the wave: a burst and a ring out to its reach */
            view_project(v, L->wave_x, L->wave_y, 0, 0, 0, 0, &x, &y, 0);
            int32_t a = 255 - (int32_t)(wage * 255 / LIGHT_WAVE_SHOW);
            int32_t rad = 8 + (int32_t)(wage * (g_tune.wave_r / 100 - 8) / LIGHT_WAVE_SHOW);
            ov_glow(s, x, y, rad, cr, cg, cb, a / 3);
            ov_ring(s, x, y, rad, 4, cr, cg, cb, a);
            ov_ring(s, x, y, rad, 1, 255, 255, 255, a);
            ov_ring(s, x, y, rad * 2 / 3, 2, 255, 255, 255, a / 2);
        }
        if (!L->on) continue;
        if (L->drag != NULL && L->drag->body != NULL) {   /* the carried item: a glowing ring around it */
            const wl_body *db = L->drag->body;
            view_project(v, db->x + db->w / 2, db->y + db->h / 2, 0, 0, 0, 0, &x, &y, 0);
            ov_ring(s, x, y, db->w / 200 + 5, 2, cr, cg, cb, 200);
            ov_ring(s, x, y, db->w / 200 + 5, 0, 255, 255, 255, 220);
        }
        for (int k = L->tn - 1; k >= 1; k--) {   /* the trail */
            view_project(v, L->tx[k], L->ty[k], 0, 0, 0, 0, &x, &y, 0);
            ov_glow(s, x, y, 7 - k / 2, cr, cg, cb, 200 - k * 22);
        }
        view_project(v, L->x, L->y, 0, 0, 0, 0, &x, &y, 0);
        int32_t ph = (int32_t)(now / 40 % 24);
        int32_t pulse = ph < 12 ? ph : 24 - ph;   /* 0..12 */
        if (L->magnet) {   /* the magnet: a faint ring at its reach with 8 bright beads turning on it */
            static const int16_t SIN[32] = { 0, 195, 383, 556, 707, 831, 924, 981, 1000, 981, 924, 831, 707, 556, 383, 195,
                                             0, -195, -383, -556, -707, -831, -924, -981, -1000, -981, -924, -831, -707,
                                             -556, -383, -195 };
            int32_t rad = g_tune.light_mag_r / 100;
            ov_ring(s, x, y, rad, 1, cr, cg, cb, 120);
            int32_t turn = (int32_t)(now / 50 % 32);
            for (int k = 0; k < 8; k++) {
                int idx = (turn + k * 4) % 32;
                int32_t dx = SIN[(idx + 8) % 32] * rad / 1000, dy = SIN[idx] * rad / 1000;
                ov_glow(s, x + dx, y + dy, 5, 255, 255, 255, 230);
                ov_glow(s, x + dx, y + dy, 8, cr, cg, cb, 160);
            }
        }
        ov_glow(s, x, y, 20 + pulse / 3, cr, cg, cb, 150);
        ov_glow(s, x, y, 10, (uint8_t)((cr + 255) / 2), (uint8_t)((cg + 255) / 2), (uint8_t)((cb + 255) / 2), 245);
        int32_t ray = 7 + pulse / 2;   /* the twinkle: a soft cross through the core */
        for (int32_t d = -ray; d <= ray; d++) {
            int32_t a = 230 - (d < 0 ? -d : d) * 200 / ray;
            ov_blend(s, x + d, y, 255, 255, 255, a);
            ov_blend(s, x, y + d, 255, 255, 255, a);
        }
        ov_glow(s, x, y, 5, 255, 255, 255, 255);
        if (L->waved && wage < (uint32_t)g_tune.wave_ms) {   /* recharging: an arc filling round the light */
            int deny = (uint32_t)(now - L->deny_at) < 300 && L->deny_at != 0;
            ov_arc(s, x, y, 24, 128, 128, 20, 24, 40, 170);
            ov_arc(s, x, y, 24, wage, (uint32_t)g_tune.wave_ms, 255, deny ? 60 : 255, deny ? 60 : 255, 240);
        } else if (L->waved && wage < (uint32_t)g_tune.wave_ms + 600) {   /* just ready again: a flash */
            int32_t a = 255 - (int32_t)((wage - (uint32_t)g_tune.wave_ms) * 255 / 600);
            ov_ring(s, x, y, 24 + (int32_t)((wage - (uint32_t)g_tune.wave_ms) * 12 / 600), 2, 255, 255, 255, a);
        }
        uint32_t edge = ov_rgb(s, 16, 24, 48), ink = ov_rgb(s, 255, 255, 255);
        char t[16];
        snprintf(t, sizeof t, "%u", L->coins);
        ov_text_outlined(s, t, x - ov_text_w(t, 1) / 2, L->drag != NULL ? y - 26 : y + 18, edge, ov_rgb(s, 255, 220, 80));   /* coin gold */
        uint32_t age = now - L->gain_at;
        if (L->gain > 0 && age < LIGHT_GAIN_MS) {   /* the coins just taken, rising */
            snprintf(t, sizeof t, "+%d", (int)L->gain);
            ov_text_outlined(s, t, x - ov_text_w(t, 1) / 2, y - 30 - (int32_t)(age * 14 / LIGHT_GAIN_MS), edge,
                             ink);
        }
    }
}

/* ---- the spell aim: a sight on the ally a player's spell will reach ----
 * A spell whose record says "an ally the caster picks" (wl_skill.aim 1 / 3) holds its target in the body
 * (+0x50) while it is being cast (phase +0x58 1..3); the game's own Up / Down taps move it to the next ally
 * (char_ai's target cycling, 0x47adb0) — the base game draws nothing for it.  The sight turns, in the caster's
 * colour (player 1 pink, 2 blue, 3 purple); a second sight on the same ally sits wider. */
/* the point at angle `a` (256 = a turn, 0 = up, clockwise) and radius rad, relative to the centre */
static void aim_point(uint32_t a, int32_t rad, int32_t *dx, int32_t *dy)
{
    a %= 256;
    int i = (int)(a / 8), f = (int)(a % 8);
    int32_t sx = (SIN32[i] * (8 - f) + SIN32[(i + 1) % 32] * f) / 8;
    int32_t cx = (SIN32[(i + 8) % 32] * (8 - f) + SIN32[(i + 9) % 32] * f) / 8;
    *dx = sx * rad / 1000;
    *dy = -cx * rad / 1000;
}

static void ov_aims(const ov_surf *s)
{
    if (g_eng.world == NULL || *g_eng.world == NULL) return;
    const wl_world *wd = *g_eng.world;
    const wl_room *r = wd->room;
    const camera_view *v = wd->view;
    if (r == NULL || v == NULL || !p1_in_control(r)) return;
    const wl_char *aimed[COOP_NIN] = { 0 };
    uint32_t now = g_eng.tick();
    for (int p = 0; p < COOP_NIN; p++) {
        const wl_pslot *ps = p == 0 ? r->leader : coop_player_slot(r, p);
        if (ps == NULL || ps->ch == NULL || ps->ch->base.body == NULL || ps->ch->f200 != 0) continue;
        const wl_body *cb = ps->ch->base.body;
        if (cb->spell == 0 || cb->target == 0 || cb->f58 < 1 || cb->f58 > 3) continue;
        const wl_skill *rec = (const wl_skill *)(intptr_t)cb->spell;
        if (rec->aim != 1 && rec->aim != 3) continue;
        const wl_char *t = (const wl_char *)(intptr_t)cb->target;
        if (t->base.alive == 0 || t->base.body == NULL) continue;
        aimed[p] = t;
        int same = 0;
        for (int q = 0; q < p; q++) if (aimed[q] == t) same++;
        const wl_body *tb = t->base.body;
        int32_t x, y;
        view_project(v, tb->x + tb->w / 2, tb->y + tb->margin + (tb->h - tb->margin) / 2, 0, 0, 0, 0, &x, &y, 0);
        uint8_t cr, cg, cbl;
        if (p == 0) { cr = 255; cg = 120; cbl = 190; } else player_rgb(p, &cr, &cg, &cbl);
        int32_t rad = 20 + 7 * same + (int32_t)((now / 120) % 3 == 0);   /* a small beat */
        uint32_t rot = (now / 12) % 256;                                  /* a turn every ~3 s */
        for (int pass = 0; pass < 2; pass++)   /* a dark outline, then the colour */
            for (int k = 0; k < 4; k++) {   /* four brackets of ~50 degrees, turning */
                for (uint32_t a = 0; a < 36; a++) {
                    int32_t px2, py2;
                    for (int32_t d = -2; d <= 2; d++) {
                        if (pass == 1 && (d < -1 || d > 1)) continue;
                        aim_point(rot + 64u * (uint32_t)k + a - 18u, rad + d, &px2, &py2);
                        if (pass == 0) ov_blend(s, x + px2, y + py2, 16, 24, 48, 200);
                        else ov_blend(s, x + px2, y + py2, d ? cr : 255, d ? cg : 255, d ? cbl : 255, 255);
                    }
                }
                for (int32_t d = -7; d <= -2; d++) {   /* a tick pointing in, between the brackets */
                    int32_t px2, py2;
                    aim_point(rot + 64u * (uint32_t)k + 32u, rad + d, &px2, &py2);
                    if (pass == 0) { for (int e = -1; e <= 1; e++) for (int f = -1; f <= 1; f++) ov_blend(s, x + px2 + e, y + py2 + f, 16, 24, 48, 160); }
                    else ov_blend(s, x + px2, y + py2, cr, cg, cbl, 255);
                }
            }
        ov_glow(s, x, y, 5, cr, cg, cbl, 220);
        ov_blend(s, x, y, 255, 255, 255, 255);
    }
}

/* a fallen player's mark: a red X (dark outline) centred on (cx, cy), `n` px from the centre to a corner */
static void ov_cross(const ov_surf *s, int32_t cx, int32_t cy, int32_t n)
{
    uint32_t dark = ov_rgb(s, 30, 10, 10), red = ov_rgb(s, 235, 40, 40);
    for (int pass = 0; pass < 2; pass++)
        for (int32_t k = -n; k <= n; k++)
            for (int32_t t = pass ? 0 : -1; t <= (pass ? 1 : 2); t++) {
                uint32_t c = pass ? red : dark;
                ov_put(s, cx + k + t, cy + k, c);
                ov_put(s, cx + k + t, cy - k, c);
            }
}

void coop_overlay_draw(uint8_t *px, int32_t pitch, int32_t w, int32_t h, int32_t bpp,
                       uint32_t rmask, uint32_t gmask, uint32_t bmask)
{
    if (!g_coop || px == NULL || (bpp != 16 && bpp != 32)) return;
    ov_surf s = { px, pitch, w, h, bpp, rmask, gmask, bmask };
    ov_lights(&s);
    ov_aims(&s);
    ov_notices(&s);
    uint32_t ink = ov_rgb(&s, 255, 255, 255);
    for (int p = 0; p < COOP_NIN; p++) {
        int32_t x, y, side;
        int down;
        uint32_t edge, fill;
        if (!coop_marker(p, w, h, &x, &y, &side, &down)) continue;
        player_colors(&s, p, &edge, &fill);
        if (down) fill = ov_rgb(&s, 110, 110, 118);   /* a fallen player: a grey badge with a red X beside it */
        int32_t bx, by;
        if (side == 0) {
            ov_arrow(&s, x, y - 4, 0, 5, edge, fill);
            bx = x - BADGE_W / 2; by = y - 10 - BADGE_H;
        } else if (side < 0) {
            ov_arrow(&s, 3, y, -1, 9, edge, fill);
            bx = 15; by = y - BADGE_H / 2;
        } else {
            ov_arrow(&s, w - 4, y, 1, 9, edge, fill);
            bx = w - 16 - BADGE_W; by = y - BADGE_H / 2;
        }
        ov_badge(&s, p, bx, by, edge, fill, ink);
        if (down) ov_cross(&s, side > 0 ? bx - 8 : bx + BADGE_W + 8, by + BADGE_H / 2, 4);
    }
}
