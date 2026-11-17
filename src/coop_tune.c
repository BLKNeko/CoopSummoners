/* coop_tune.c — the co-op tuning values: defaults here, coop-tuning.ini (the game folder) overrides them,
 * the launcher's "Light recharge" / "Light power" (rhost --coop-light-recharge / --coop-light-power) come last.
 *
 * The file: "key = value" lines (whole numbers; times in ms, distances in px x 100 as the engine counts),
 * [sections] and ";" / "#" comments ignored, unknown keys and bad values skipped.  The launcher ships a
 * commented copy with every default (tools/launcher/coop-tuning.ini). */
#include "coop_internal.h"

#include <ctype.h>

coop_tuning g_tune = {
    .swap_ms = 500, .join_ms = 1000, .leave_ms = 1500, .kick_ms = 5000, .notice_ms = 2000,
    .light_idle_ms = 10000, .light_speed = 450, .light_accel = 70, .light_r = 700, .light_grab_r = 1400,
    .light_drag_step = 900, .light_snag = 4800, .light_margin = 1000, .light_mag_r = 9000, .light_mag_step = 600,
    .light_tap_ms = 250,
    .wave_ms = 12000, .wave_power = 1, .wave_r = 5000, .wave_heal = 80, .wave_min = 5, .wave_dmg = 250,
    .wave_same_ms = 3000,
};

static const struct { const char *key; size_t off; int32_t lo, hi; } KEYS[] = {
#define K(name, lo, hi) { #name, offsetof(coop_tuning, name), lo, hi }
    K(swap_ms, 50, 5000), K(join_ms, 100, 20000), K(leave_ms, 100, 20000), K(kick_ms, 500, 60000),
    K(notice_ms, 200, 20000),
    K(light_idle_ms, 1000, 600000), K(light_speed, 50, 5000), K(light_accel, 5, 2000), K(light_r, 100, 20000),
    K(light_grab_r, 100, 20000), K(light_drag_step, 50, 5000), K(light_snag, 500, 50000),
    K(light_margin, 0, 20000), K(light_mag_r, 0, 64000), K(light_mag_step, 10, 5000), K(light_tap_ms, 50, 2000),
    K(wave_ms, 1000, 120000), K(wave_power, 1, 4), K(wave_r, 500, 64000), K(wave_heal, 0, 1000),
    K(wave_min, 0, 9999), K(wave_dmg, 0, 4000), K(wave_same_ms, 0, 60000),
#undef K
};

static int32_t g_set_recharge, g_set_power;   /* the launcher's, re-applied over the file */

void coop_set_light(int32_t recharge_ms, int32_t power)
{
    if (recharge_ms >= 1000 && recharge_ms <= 120000) g_tune.wave_ms = g_set_recharge = recharge_ms;
    if (power >= 1 && power <= 4) g_tune.wave_power = g_set_power = power;
}

void coop_tune_load(const char *path)
{
    FILE *f = path != NULL ? fopen(path, "r") : NULL;
    if (f == NULL) return;
    char line[256];
    int n = 0;
    while (fgets(line, sizeof line, f) != NULL) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == 0 || *p == ';' || *p == '#' || *p == '[') continue;
        char *eq = strchr(p, '=');
        if (eq == NULL) continue;
        char *ke = eq;
        while (ke > p && isspace((unsigned char)ke[-1])) ke--;
        *ke = 0;
        char *end;
        long v = strtol(eq + 1, &end, 10);
        if (end == eq + 1) continue;
        for (size_t k = 0; k < sizeof KEYS / sizeof KEYS[0]; k++) {
            if (strcmp(p, KEYS[k].key) != 0) continue;
            if (v < KEYS[k].lo || v > KEYS[k].hi) { coop_log("tuning: %s = %ld out of %d..%d, kept", p, v, (int)KEYS[k].lo, (int)KEYS[k].hi); break; }
            int32_t iv = (int32_t)v;
            memcpy((uint8_t *)&g_tune + KEYS[k].off, &iv, sizeof iv);
            n++;
            break;
        }
    }
    fclose(f);
    if (g_set_recharge) g_tune.wave_ms = g_set_recharge;
    if (g_set_power) g_tune.wave_power = g_set_power;
    coop_log("tuning: %d values from %s", n, path);
}
