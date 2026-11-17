/* coop_camera.c — the camera: which player it follows.  See coop.h; shared state in coop_internal.h. */
#include "coop_internal.h"

/* ---- the camera: who it follows ---- */

int g_cam;                       /* the player the camera follows (0 = player 1, the leader) */
static uint8_t g_cam_prev[COOP_NIN];    /* each player's camera key last update (a press = an edge) */
uint32_t g_cam_at;               /* when it last changed hands (the notice) */
int g_cam_noted;

/* player p's character when it can be followed */
static wl_char *cam_char(const wl_room *r, int p)
{
    if (p == 0) return r->leader != NULL ? r->leader->ch : NULL;
    if (g_out[p]) return NULL;
    const wl_pslot *s = coop_player_slot(r, p);
    if (s == NULL || s->active != 1 || s->ch == NULL || s->ch->base.body == NULL) return NULL;
    return s->ch;
}

static int g_cam_down;   /* the followed character was down (0 HP) last update: a fall = a 0 -> 1 edge */

/* player p's character is down (0 HP, "Disabled") */
static int cam_down(const wl_room *r, int p)
{
    const wl_char *c = cam_char(r, p);
    return c != NULL && c->party != NULL && c->party->hp <= 0;
}

/* player 1's camera key: their keyboard key, or the button on the game's own gamepad (joystick slot 0) */
static int cam_p1_held(void)
{
    const coop_player_ctl *c = &g_ctl[0];
    uint8_t k = c->keys[COOP_A_CAMERA], b = c->buttons[COOP_A_CAMERA];
    const p2_dev *d = sysp(0x2878);
    if (c->kb && k != 0 && d != NULL && (d->state[k] & 0x80) != 0) return 1;
    const uint8_t *w = sysp(0x287c);
    return c->pad && w != NULL && pad_button((intptr_t)w, b);
}

void cam_tick(const wl_room *r)
{
    int press = 0;
    for (int p = 0; p < COOP_NIN; p++) {
        uint8_t h = (uint8_t)(p == 0 ? cam_p1_held() : g_held[p][COOP_A_CAMERA]);
        if (h && !g_cam_prev[p]) press = 1;
        g_cam_prev[p] = h;
    }
    if (g_cam != 0 && cam_char(r, g_cam) == NULL) g_cam = 0;   /* that player left: back on the leader */
    if (!p1_in_control(r)) return;
    int down = cam_down(r, g_cam);
    if (down && !g_cam_down) {   /* the followed player just fell: on to the next one still standing */
        for (int k = 1; k < COOP_NIN; k++) {
            int q = (g_cam + k) % COOP_NIN;
            if (cam_char(r, q) != NULL && !cam_down(r, q)) {
                coop_log("camera follows player %d (player %d fell)", q + 1, g_cam + 1);
                g_cam = q;
                g_cam_at = g_eng.tick();
                g_cam_noted = 1;
                down = 0;
                break;
            }
        }
    }
    g_cam_down = down;
    if (!press) return;
    for (int k = 1; k <= COOP_NIN; k++) {   /* by hand: any player in the game, fallen or not */
        int q = (g_cam + k) % COOP_NIN;
        if (cam_char(r, q) != NULL) { g_cam = q; break; }
    }
    g_cam_down = cam_down(r, g_cam);   /* a fallen player picked by hand keeps the camera */
    g_cam_at = g_eng.tick();
    g_cam_noted = 1;
    coop_log("camera follows player %d", g_cam + 1);
}

wl_char *coop_camera_char(wl_char *leader)
{
    if (!g_coop || g_cam == 0 || g_eng.world == NULL || *g_eng.world == NULL) return leader;
    const wl_room *r = (*g_eng.world)->room;
    if (r == NULL || !p1_in_control(r)) return leader;
    wl_char *c = cam_char(r, g_cam);
    return c != NULL ? c : leader;
}

