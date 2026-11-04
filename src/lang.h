/*
 * lang.{c,h} — CoopSummoners translations (not a retail function; off unless rhost's --lang loads a file, so every
 * retail path and the parity corpus are unchanged).
 *
 * The EN 2012 edition bakes its text into sotes.exe's .data / .rdata; the port reaches most of it through
 * g_eng.data(va) (the D(0x...) literals: the dialogue and many menu words).  A translation file maps a string's VA to
 * its new text; with one loaded, rhost binds g_eng.data to lang_data, which returns the translated copy for a mapped
 * VA and the identity mapping otherwise.  The copies live on the heap at any length: no D() string is ever written
 * through (checked over src/, 2026-11-03), and the strings the port copies into fixed buffers carry a byte cap in the
 * file (enforced here).
 *
 * Text is single-byte Windows-1252 (accents) while a translation is loaded: g_text_sbcs switches the text layer's
 * SJIS lead-byte tests (text_lead) off, makes accented letters word letters for the word wrap (text_hi_letter), and
 * skips the Japanese ruby (furigana) pass.
 *
 * File format (UTF-8, one string per line, '#' starts a comment line):
 *     VA <TAB> MAX <TAB> TEXT
 * VA in hex (0x855ad4), MAX = the byte cap (0 = none), TEXT with \n \r \t \\ escapes and the game's own codes
 * (%n line break, %s / %d fields, @@KEY key names) kept as they are.  Characters outside Windows-1252 become '?'.
 * MAX "@N" = written IN PLACE: a table string the code reads at base + row * stride, never by its own VA (item /
 * spell / monster / place names, descriptions, books).  rhost copies the text over the image's string before the game
 * starts (lang_inplace), N = the bytes its field holds; lang_data leaves such a VA alone (the image has the text).
 *
 * Several files may be loaded (lang_load again): a VA in a later file replaces the earlier one (the "keep original
 * names" option = the main file + an overlay with the names put back in English).
 */
#ifndef OSS_LANG_H
#define OSS_LANG_H

#include <stddef.h>
#include <stdint.h>

/* 1 while a translation is loaded: text bytes >= 0x80 are single Windows-1252 characters, not SJIS lead bytes */
extern int g_text_sbcs;

/* the text layer's "SJIS lead byte" test (retail: (signed char)c < 0) */
static inline int text_lead(char c) { return !g_text_sbcs && (signed char)c < 0; }
/* an accented letter (Windows-1252 0xC0..0xFF but × ÷) while a translation is loaded */
static inline int text_hi_letter(char c)
{
    uint8_t u = (uint8_t)c;
    return g_text_sbcs && u >= 0xC0 && u != 0xD7 && u != 0xF7;
}

/* load a translation file, added to those loaded before; returns the number of strings read (0 on failure, with a
 * reason in err) */
int lang_load(const char *path, char *err, size_t errn);
/* the i-th in-place string (0..): its VA, text and field cap; 0 past the last */
int lang_inplace(int i, uint32_t *va, const char **text, int *cap);
/* the number of strings mapped (0 = no translation) */
int lang_count(void);
/* g_eng.data with a translation: the translated copy for a mapped VA, else the VA itself */
const void *lang_data(uint32_t va);
/* the translated text for `va`, or NULL */
const char *lang_lookup(uint32_t va);
/* UTF-8 -> Windows-1252 (in place is fine: the output is never longer); returns the output length */
size_t lang_utf8_to_cp1252(const char *in, char *out);

#endif /* OSS_LANG_H */
