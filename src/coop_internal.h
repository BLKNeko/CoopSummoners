/*
 * coop_internal.h — what the co-op files share (not part of the engine's interface; that is coop.h).
 *
 *   coop.c          the players' devices and keys, the producers, joining / leaving, the switch key,
 *                   coop_tick, the status line, the fallen-player filter, the spell panel, the test hooks
 *   coop_camera.c   which player the camera follows (the camera key, moving on when that player falls)
 *   coop_light.c    the light of players 2 / 3 with no character: coins, magnet, carrying, ping, the wave
 *   coop_overlay.c  everything drawn over the game: markers, notices, hold bars, the lights, the spell sight
 *   coop_tune.c     the tuning values (coop-tuning.ini)
 */
#ifndef OSS_COOP_INTERNAL_H
#define OSS_COOP_INTERNAL_H

#include "coop.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera_follow.h"
#include "char_attack.h"
#include "char_def.h"
#include "char_hit.h"
#include "char_state.h"
#include "char_status.h"
#include "engine_calls.h"
#include "entity_move.h"
#include "input.h"
#include "party_member.h"
#include "room_load.h"
#include "scene_loop.h"
#include "sound_fx.h"

/* ---- small helpers ---- */
static inline void *sysp(uint32_t off) { void *p; memcpy(&p, (const uint8_t *)g_eng.sys + off, sizeof p); return p; }
static inline int32_t get32(const void *p, uint32_t off) { int32_t v; memcpy(&v, (const uint8_t *)p + off, 4); return v; }
static inline void put32(void *p, uint32_t off, int32_t v) { memcpy((uint8_t *)p + off, &v, 4); }
static inline int32_t isqrt64(int64_t n)
{
    if (n <= 0) return 0;
    int64_t x = n, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return (int32_t)x;
}

/* a line in coop.log (the game folder), stamped with the tick */
void coop_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ---- coop_tune.c: the tuning values (defaults below; coop-tuning.ini in the game folder overrides) ---- */
typedef struct coop_tuning {
    /* the switch key */
    int32_t swap_ms;        /* two taps this close: a swap */
    int32_t join_ms;        /* player 2 / 3 out holds this long: joins */
    int32_t leave_ms;       /* player 2 / 3 in holds this long: leaves */
    int32_t kick_ms;        /* player 1 holds this long: everyone else is dropped out */
    int32_t notice_ms;      /* the on-screen notices' life */
    /* the light */
    int32_t light_idle_ms;  /* it goes after this long without input */
    int32_t light_speed;    /* top speed per update (px x 100) */
    int32_t light_accel;
    int32_t light_r;        /* its reach around its centre (coins) */
    int32_t light_grab_r;   /* hold jump picks an item up this close (beyond its half size) */
    int32_t light_drag_step;/* a carried item's step per update */
    int32_t light_snag;     /* a carried item stuck this far behind is let go */
    int32_t light_margin;   /* kept this far inside the view */
    int32_t light_mag_r;    /* the magnet's range */
    int32_t light_mag_step; /* a pulled coin's step per update */
    int32_t light_tap_ms;   /* attack released sooner = a tap (the wave); longer = the magnet */
    /* the wave */
    int32_t wave_ms;        /* the recharge (the launcher's "Light recharge" overrides) */
    int32_t wave_power;     /* heal and damage x this, 1..4 (the launcher's "Light power" overrides) */
    int32_t wave_r;         /* its reach */
    int32_t wave_heal;      /* per mille of an ally's max HP */
    int32_t wave_min;       /* at least this much healed */
    int32_t wave_dmg;       /* per mille of the leader's plain hit on the foe */
    int32_t wave_same_ms;   /* one wave per character per this long (two lights) */
} coop_tuning;
extern coop_tuning g_tune;

/* ---- coop.c ---- */
/* the keyboard device wrapper, as input_dik.c reads it */
typedef struct p2_dev {
    uint8_t         _p00[0x10];
    uint32_t        n;
    const uint32_t *ev;
    uint8_t         state[256];
} p2_dev;
extern coop_player_ctl g_ctl[COOP_NIN];
/* the slot player p plays: player 1 the leader (while it holds manager 0), else its own */
wl_pslot *played(const wl_room *r, int p);
void ctl_init(void);
extern uint8_t g_held[COOP_NIN][COOP_NACT];   /* the actions each player holds now */
int pad_button(intptr_t w, uint8_t b);
int p1_in_control(const wl_room *r);

extern int g_out[COOP_NIN];       /* 1 = not playing (the AI drives) */
extern int g_waiting[COOP_NIN];   /* joined, waiting for a free member */
enum { NOTICE_NONE, NOTICE_JOINED, NOTICE_LEFT, NOTICE_WAITING, NOTICE_DOWN };
extern int g_notice[COOP_NIN];
extern uint32_t g_notice_at[COOP_NIN];
extern int g_ui_pt;               /* the on-screen text in Portuguese */
void notice(int p, int k);
int players_in(void);
/* player p's character is down (0 HP, "Disabled"); p 0 = the leader */
int coop_player_down(const wl_room *r, int p);

#define SWITCH_ID 20
typedef struct sw_state {
    int          down;        /* the switch is held (a press seen, its release not yet)  */
    uint32_t     down_at;
    int          hold_fired;
    int          tap;         /* a tap waiting for the swap window                       */
    uint32_t     tap_at;
    input_event *posted;      /* the press coop posted for room_keys: not a player's     */
    uint32_t     posted_ts;   /* its time stamp (the record is reused once the ring turns) */
} sw_state;
extern sw_state g_sw[COOP_NIN];

/* ---- coop_camera.c ---- */
extern int g_cam;                 /* the player the camera follows (0 = player 1, the leader) */
extern uint32_t g_cam_at;         /* when it last changed hands (the notice) */
extern int g_cam_noted;
void cam_tick(const wl_room *r);

/* ---- coop_light.c ---- */
#define LIGHT_PING_MS   900
#define LIGHT_GAIN_MS   900      /* the "+N" over the light */
#define LIGHT_TRAIL     8
#define LIGHT_PINGS     3
#define LIGHT_WAVE_SHOW 450      /* the drawn wave's life */

typedef struct coop_light {
    int        on;
    int32_t    x, y, vx, vy;
    uint32_t   last_input, seen;    /* the last input / the last update (drawn only while updated) */
    int        prev_jump, prev_use, magnet;
    int        atk_down;            /* attack held since atk_at (a tap = the wave, a hold = the magnet) */
    uint32_t   atk_at;
    int        waved;               /* a wave was let out (wave_at valid) */
    uint32_t   wave_at, deny_at;    /* the last wave / the last tap while recharging */
    int32_t    wave_x, wave_y;
    wl_entity *drag;                /* the item it carries, or NULL */
    uint32_t   coins;               /* this session's coins */
    int32_t    gain;
    uint32_t   gain_at;
    int32_t    tx[LIGHT_TRAIL], ty[LIGHT_TRAIL];
    int        tn, tk;
    int32_t    px[LIGHT_PINGS], py[LIGHT_PINGS];
    uint32_t   pat[LIGHT_PINGS];
    int        pk;
} coop_light;
extern coop_light g_light[COOP_NIN];

#endif /* OSS_COOP_INTERNAL_H */
