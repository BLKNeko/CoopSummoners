/* lang.c — CoopSummoners translations.  See lang.h. */
#include "lang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_text_sbcs;

typedef struct { uint32_t va; int seq; int inplace; int cap; char *text; } lang_ent;
static lang_ent *g_ent;
static int g_n, g_cap, g_seq;

int lang_count(void) { return g_n; }

/* Windows-1252 0x80..0x9F: the code points (0 = unused there) */
static const uint16_t CP1252_HI[32] = {
    0x20ac, 0, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017d, 0,
    0, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0, 0x017e, 0x0178,
};

static char cp1252_of(uint32_t cp)
{
    if (cp < 0x80 || (cp >= 0xa0 && cp <= 0xff)) return (char)cp;
    for (int k = 0; k < 32; k++)
        if (CP1252_HI[k] != 0 && CP1252_HI[k] == cp) return (char)(0x80 + k);
    return '?';
}

size_t lang_utf8_to_cp1252(const char *in, char *out)
{
    const uint8_t *p = (const uint8_t *)in;
    size_t o = 0;
    while (*p) {
        uint32_t cp;
        int extra;
        if (*p < 0x80) { cp = *p; extra = 0; }
        else if ((*p & 0xe0) == 0xc0) { cp = *p & 0x1f; extra = 1; }
        else if ((*p & 0xf0) == 0xe0) { cp = *p & 0x0f; extra = 2; }
        else if ((*p & 0xf8) == 0xf0) { cp = *p & 0x07; extra = 3; }
        else { out[o++] = '?'; p++; continue; }   /* a stray continuation byte */
        p++;
        int ok = 1;
        for (int k = 0; k < extra; k++) {
            if ((*p & 0xc0) != 0x80) { ok = 0; break; }
            cp = (cp << 6) | (*p++ & 0x3f);
        }
        out[o++] = ok ? cp1252_of(cp) : '?';
    }
    out[o] = 0;
    return o;
}

static int hexval(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* \n \r \t \\ \xHH -> the bytes (in place; run AFTER the Windows-1252 conversion, so \xHH can give any raw byte, such
 * as the 0x81 lead of the SJIS arrow key tokens "@@\x81\xa9") */
static size_t unescape(char *s)
{
    char *o = s, *b = s;
    for (; *s; s++) {
        if (*s == '\\' && s[1] == 'x' && hexval(s[2]) >= 0 && hexval(s[3]) >= 0) {
            *o++ = (char)(hexval(s[2]) * 16 + hexval(s[3]));
            s += 3;
        } else if (*s == '\\' && s[1] != 0) {
            s++;
            *o++ = *s == 'n' ? '\n' : *s == 'r' ? '\r' : *s == 't' ? '\t' : *s;
        } else {
            *o++ = *s;
        }
    }
    *o = 0;
    return (size_t)(o - b);
}

static int by_va(const void *a, const void *b)
{
    const lang_ent *x = a, *y = b;
    if (x->va != y->va) return x->va < y->va ? -1 : 1;
    return x->seq < y->seq ? -1 : x->seq > y->seq;
}

/* sort by VA; a VA listed twice (in one file or across files): the later line wins */
static void lang_sort(void)
{
    qsort(g_ent, (size_t)g_n, sizeof *g_ent, by_va);
    int w = 0;
    for (int k = 0; k < g_n; k++) {
        if (w > 0 && g_ent[w - 1].va == g_ent[k].va) { free(g_ent[w - 1].text); g_ent[w - 1] = g_ent[k]; }
        else g_ent[w++] = g_ent[k];
    }
    g_n = w;
}

int lang_load(const char *path, char *err, size_t errn)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) { snprintf(err, errn, "cannot open %s", path); return 0; }
    int n = 0, line_no = 0, skipped = 0;
    static char line[16384];
    while (fgets(line, sizeof line, f) != NULL) {
        line_no++;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;
        char *s = line;
        if (line_no == 1 && (uint8_t)s[0] == 0xef && (uint8_t)s[1] == 0xbb && (uint8_t)s[2] == 0xbf) s += 3;   /* BOM */
        if (s[0] == 0 || s[0] == '#') continue;
        char *t1 = strchr(s, '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        if (t2 == NULL) { skipped++; continue; }
        *t1 = *t2 = 0;
        uint32_t va = (uint32_t)strtoul(s, NULL, 16);
        int inplace = t1[1] == '@';
        long max = strtol(t1 + 1 + inplace, NULL, 10);
        char *text = t2 + 1;
        if (va == 0 || text[0] == 0) { skipped++; continue; }   /* an untranslated row keeps the original */
        if (inplace && max <= 0) { skipped++; continue; }       /* in place needs its field's size */
        lang_utf8_to_cp1252(text, text);
        size_t out = unescape(text);
        if (max > 0 && out > (size_t)max) { skipped++; continue; }   /* too long for its buffer / field */
        if (g_n == g_cap) {
            int nc = g_cap ? g_cap * 2 : 1024;
            lang_ent *ne = realloc(g_ent, sizeof *ne * (size_t)nc);
            if (ne == NULL) break;
            g_ent = ne;
            g_cap = nc;
        }
        char *copy = malloc(out + 1);
        if (copy == NULL) break;
        memcpy(copy, text, out + 1);
        g_ent[g_n++] = (lang_ent){ va, g_seq++, inplace, (int)max, copy };
        n++;
    }
    fclose(f);
    if (n == 0) {
        snprintf(err, errn, "%s: no translated strings (%d lines skipped)", path, skipped);
        return 0;
    }
    lang_sort();
    g_text_sbcs = 1;
    snprintf(err, errn, "%d strings, %d lines skipped", n, skipped);
    return n;
}

int lang_inplace(int i, uint32_t *va, const char **text, int *cap)
{
    for (int k = 0; k < g_n; k++)
        if (g_ent[k].inplace && i-- == 0) {
            *va = g_ent[k].va;
            *text = g_ent[k].text;
            *cap = g_ent[k].cap;
            return 1;
        }
    return 0;
}

const char *lang_lookup(uint32_t va)
{
    int lo = 0, hi = g_n - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (g_ent[m].va == va) return g_ent[m].inplace ? NULL : g_ent[m].text;   /* in place: the image holds it */
        if (g_ent[m].va < va) lo = m + 1;
        else hi = m - 1;
    }
    return NULL;
}

const void *lang_data(uint32_t va)
{
    const char *t = lang_lookup(va);
    return t != NULL ? (const void *)t : (const void *)(uintptr_t)va;
}
