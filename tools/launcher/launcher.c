/* launcher.c — CoopSummoners.exe: the co-op mod's launcher (not part of the port).
 *
 * Ships next to opensummoners-standalone.exe.  It finds the Steam copy of Fortune Summoners, checks it is the
 * supported edition (sotes.exe SHA-256), installs / updates / removes the mod exe IN the game folder (the port
 * reads the game's files from its own folder, rhost.c: gamedir = the exe's directory), backs up the saves (user\)
 * before the first install, and starts the game solo or with --coop.  It lists the joysticks the way the game
 * enumerates them (DirectInput 7, DIDEVTYPE_JOYSTICK, attached only: input_dev.c), so "P1" / "P2" match the game's
 * joystick slots 0 / 1.  English or Portuguese, from the Windows UI language.
 *
 * Never modifies a game file: it only adds / removes opensummoners-standalone.exe, coop.log and the exe's log
 * folder, and copies user\ to user-backup-<date>. */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0700
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <dinput.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define MOD_EXE   L"opensummoners-standalone.exe"
#define GAME_EXE  L"sotes.exe"
#define GAME_SUB  L"\\steamapps\\common\\Fortune Summoners"
/* the supported edition: Steam app 203510, build 29910 (2012 English) */
static const char SOTES_SHA256[] = "7d779f2eb02b3c603857fedbc52be6973ac3b0b2c5c1bc696122ddac89fb9f1b";
static const wchar_t *GAME_FILES[] = { L"sotes.exe", L"sotesd.dll", L"sotesw.dll", L"sotesp.dll", L"lizsoft.spl" };

/* ---------------------------------------------------------------- strings (en / pt) */

enum {
    S_TITLE, S_GAMEDIR, S_BROWSE, S_FIND, S_PICK_FOLDER,
    S_G_NONE, S_G_MISSING, S_G_FILES, S_G_OK, S_G_OTHER,
    S_M_NONE, S_M_OK, S_M_UPDATE, S_M_NOBUNDLE, S_M_NOTHING,
    S_INSTALL, S_UPDATE, S_UNINSTALL, S_SOLO, S_COOP,
    S_PADS, S_PADS_NOTE, S_PAD_NONE, S_PAD_UNUSED, S_CONTROLS, S_FOOTER,
    S_ASK_OTHER, S_ASK_INSTALL, S_ASK_UNINSTALL, S_DONE_INSTALL, S_DONE_BACKUP, S_DONE_UNINSTALL,
    S_ERR_COPY, S_ERR_DENIED, S_ERR_START, S_ERR_NOT_GAME, S_NOT_FOUND, S_CONTROLS_TEXT, S_N
};

static const wchar_t *const STR_EN[S_N] = {
    L"CoopSummoners — Fortune Summoners co-op",
    L"Game folder (Steam):", L"Browse...", L"Find in Steam", L"Choose the Fortune Summoners folder (the one with sotes.exe):",
    L"Game: not found. Click \"Find in Steam\" or \"Browse...\".",
    L"Game: sotes.exe is not in this folder.",
    L"Game: some game files are missing (sotesd.dll, sotesw.dll, sotesp.dll, lizsoft.spl).",
    L"Game: Fortune Summoners 2012 English edition (supported).",
    L"Game: this sotes.exe is not the supported version (Steam 2012 English, build 29910). The mod may not work.",
    L"Mod: not installed in the game folder.",
    L"Mod: installed and up to date.",
    L"Mod: installed, an update is ready (click Update).",
    L"Mod: installed. (opensummoners-standalone.exe is not next to the launcher, so it can't be updated from here.)",
    L"Mod: not installed, and opensummoners-standalone.exe is missing next to the launcher. Extract the whole zip.",
    L"Install mod", L"Update mod", L"Uninstall", L"Play Solo", L"Play Co-op",
    L"Controllers (plug them in before starting the game):",
    L"The first controller is player 1, the second is player 2. Player 2 can also use the numpad.",
    L"No controller connected (keyboard: player 1 = arrows, player 2 = numpad).",
    L"not used",
    L"Controls...",
    L"Unofficial fan mod, built on OpenSummoners. Not affiliated with Lizsoft or Carpe Fulgur.",
    L"This sotes.exe is not the supported version (Steam 2012 English, build 29910).\n\nContinue anyway?",
    L"The mod isn't installed in the game folder yet (or an update is ready).\n\nInstall it now?",
    L"Remove the mod from the game folder?\n\nYour saves (user\\) and the save backups are kept. The game itself is not changed.",
    L"The mod is installed in the game folder.",
    L"\n\nYour saves were backed up to:\n",
    L"The mod was removed.",
    L"Could not copy the mod into the game folder.\n\n",
    L"\n\nWindows denied access to the game folder. Close the game if it is running, or right-click CoopSummoners.exe and choose \"Run as administrator\".",
    L"Could not start the game.\n\n",
    L"That folder doesn't have sotes.exe. Choose the Fortune Summoners folder.",
    L"Fortune Summoners was not found in your Steam libraries. Click \"Browse...\" and choose the game folder.",
    L"Controls\r\n"
    L"\r\n"
    L"                      Player 1               Player 2\r\n"
    L"Device                keyboard + 1st pad     numpad + 2nd pad\r\n"
    L"Move                  arrow keys             numpad 8 / 5 / 4 / 6\r\n"
    L"Attack                game config (X)        numpad 1\r\n"
    L"Jump                  game config (C)        numpad 2\r\n"
    L"Use / investigate     game config            numpad 3\r\n"
    L"Sheathe / skill mod.  game config            numpad 0\r\n"
    L"Switch character      Enter / game config    numpad +\r\n"
    L"\r\n"
    L"Gamepads use the button layout from the game's key configuration.\r\n"
    L"\r\n"
    L"Switch key in co-op\r\n"
    L"- Both players tap within 0.5 s: you swap characters.\r\n"
    L"- One player taps alone: the game's normal switch to a free AI member.\r\n"
    L"- Player 2 holds it 1.5 s: player 2 leaves (AI takes over). Hold again to rejoin.\r\n"
    L"\r\n"
    L"Player 1 is always the leader (camera, talking, menus).\r\n"
    L"A fallen player can walk but not attack until healed.\r\n"
    L"Game over only when the whole party is down.\r\n",
};

static const wchar_t *const STR_PT[S_N] = {
    L"CoopSummoners — co-op de Fortune Summoners",
    L"Pasta do jogo (Steam):", L"Procurar...", L"Achar na Steam", L"Escolha a pasta do Fortune Summoners (a que tem o sotes.exe):",
    L"Jogo: não encontrado. Clique em \"Achar na Steam\" ou \"Procurar...\".",
    L"Jogo: o sotes.exe não está nesta pasta.",
    L"Jogo: faltam arquivos do jogo (sotesd.dll, sotesw.dll, sotesp.dll, lizsoft.spl).",
    L"Jogo: Fortune Summoners, edição inglesa de 2012 (suportada).",
    L"Jogo: este sotes.exe não é a versão suportada (Steam 2012 em inglês, build 29910). O mod pode não funcionar.",
    L"Mod: não instalado na pasta do jogo.",
    L"Mod: instalado e atualizado.",
    L"Mod: instalado, há uma atualização pronta (clique em Atualizar).",
    L"Mod: instalado. (O opensummoners-standalone.exe não está ao lado do launcher, então não dá para atualizar por aqui.)",
    L"Mod: não instalado, e falta o opensummoners-standalone.exe ao lado do launcher. Extraia o zip inteiro.",
    L"Instalar mod", L"Atualizar mod", L"Desinstalar", L"Jogar Solo", L"Jogar Co-op",
    L"Controles (conecte antes de abrir o jogo):",
    L"O primeiro controle é o jogador 1, o segundo é o jogador 2. O jogador 2 também pode usar o teclado numérico.",
    L"Nenhum controle conectado (teclado: jogador 1 = setas, jogador 2 = teclado numérico).",
    L"não usado",
    L"Comandos...",
    L"Mod não oficial feito por fãs, baseado no OpenSummoners. Sem vínculo com a Lizsoft ou a Carpe Fulgur.",
    L"Este sotes.exe não é a versão suportada (Steam 2012 em inglês, build 29910).\n\nContinuar mesmo assim?",
    L"O mod ainda não está instalado na pasta do jogo (ou há uma atualização pronta).\n\nInstalar agora?",
    L"Remover o mod da pasta do jogo?\n\nSeus saves (user\\) e as cópias de segurança são mantidos. O jogo em si não é alterado.",
    L"O mod foi instalado na pasta do jogo.",
    L"\n\nSeus saves foram copiados para:\n",
    L"O mod foi removido.",
    L"Não foi possível copiar o mod para a pasta do jogo.\n\n",
    L"\n\nO Windows negou acesso à pasta do jogo. Feche o jogo se estiver aberto, ou clique com o botão direito no CoopSummoners.exe e escolha \"Executar como administrador\".",
    L"Não foi possível abrir o jogo.\n\n",
    L"Essa pasta não tem o sotes.exe. Escolha a pasta do Fortune Summoners.",
    L"O Fortune Summoners não foi encontrado nas bibliotecas da Steam. Clique em \"Procurar...\" e escolha a pasta do jogo.",
    L"Comandos\r\n"
    L"\r\n"
    L"                      Jogador 1              Jogador 2\r\n"
    L"Dispositivo           teclado + 1º controle  teclado numérico + 2º controle\r\n"
    L"Mover                 setas                  numérico 8 / 5 / 4 / 6\r\n"
    L"Atacar                config. do jogo (X)    numérico 1\r\n"
    L"Pular                 config. do jogo (C)    numérico 2\r\n"
    L"Usar / investigar     config. do jogo        numérico 3\r\n"
    L"Guardar / modif. hab. config. do jogo        numérico 0\r\n"
    L"Trocar personagem     Enter / config.        numérico +\r\n"
    L"\r\n"
    L"Os controles usam os botões da configuração de teclas do jogo.\r\n"
    L"\r\n"
    L"Tecla de troca no co-op\r\n"
    L"- Os dois apertam em até 0,5 s: vocês trocam de personagem.\r\n"
    L"- Só um aperta: a troca normal do jogo para um membro livre da IA.\r\n"
    L"- O jogador 2 segura 1,5 s: sai do jogo (a IA assume). Segure de novo para voltar.\r\n"
    L"\r\n"
    L"O jogador 1 é sempre o líder (câmera, conversas, menus).\r\n"
    L"Um jogador caído pode andar, mas não atacar, até ser curado.\r\n"
    L"Game over só quando o grupo inteiro cai.\r\n",
};

static const wchar_t *const *T = STR_EN;
#define TR(id) (T[(id)])

/* ---------------------------------------------------------------- SHA-256 */

typedef struct { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t n; } sha256;

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
    0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
    0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2,
};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_block(sha256 *s, const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha_init(sha256 *s)
{
    static const uint32_t H0[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(s->h, H0, sizeof H0);
    s->len = 0;
    s->n = 0;
}

static void sha_update(sha256 *s, const uint8_t *p, size_t n)
{
    s->len += n;
    while (n > 0) {
        size_t k = 64 - s->n < n ? 64 - s->n : n;
        memcpy(s->buf + s->n, p, k);
        s->n += k; p += k; n -= k;
        if (s->n == 64) { sha_block(s, s->buf); s->n = 0; }
    }
}

static void sha_hex(sha256 *s, char out[65])
{
    uint64_t bits = s->len * 8;
    uint8_t pad = 0x80, z = 0, lenb[8];
    sha_update(s, &pad, 1);
    while (s->n != 56) sha_update(s, &z, 1);
    for (int i = 0; i < 8; i++) lenb[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha_update(s, lenb, 8);
    for (int i = 0; i < 8; i++) sprintf(out + 8 * i, "%08x", s->h[i]);
    out[64] = 0;
}

/* 1 = hashed into out; 0 = the file can't be read */
static int file_sha256(const wchar_t *path, char out[65])
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    static uint8_t buf[1 << 20];
    sha256 s;
    sha_init(&s);
    DWORD got;
    while (ReadFile(f, buf, sizeof buf, &got, NULL) && got > 0) sha_update(&s, buf, got);
    CloseHandle(f);
    sha_hex(&s, out);
    return 1;
}

/* ---------------------------------------------------------------- paths, settings */

static wchar_t g_self_dir[MAX_PATH];   /* the launcher's folder: the bundled mod exe lives here */
static wchar_t g_game_dir[MAX_PATH];   /* the chosen game folder, "" when none */
static wchar_t g_ini[MAX_PATH];

static int exists(const wchar_t *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
static int is_dir(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static void join(wchar_t *out, const wchar_t *dir, const wchar_t *name) { swprintf(out, MAX_PATH, L"%ls\\%ls", dir, name); }

static void strip_slash(wchar_t *p)
{
    size_t n = wcslen(p);
    while (n > 3 && (p[n - 1] == L'\\' || p[n - 1] == L'/')) p[--n] = 0;
}

static void settings_init(void)
{
    wchar_t appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, appdata))) {
        wchar_t dir[MAX_PATH];
        join(dir, appdata, L"CoopSummoners");
        CreateDirectoryW(dir, NULL);
        join(g_ini, dir, L"launcher.ini");
    } else {
        join(g_ini, g_self_dir, L"CoopSummoners.ini");
    }
}

static void settings_load(void) { GetPrivateProfileStringW(L"launcher", L"GameDir", L"", g_game_dir, MAX_PATH, g_ini); }
static void settings_save(void) { WritePrivateProfileStringW(L"launcher", L"GameDir", g_game_dir, g_ini); }

/* ---------------------------------------------------------------- finding the game in Steam */

static int reg_str(HKEY root, const wchar_t *key, const wchar_t *val, wchar_t *out, DWORD cap)
{
    DWORD n = cap * sizeof(wchar_t), type = 0;
    if (RegGetValueW(root, key, val, RRF_RT_REG_SZ, &type, out, &n) != ERROR_SUCCESS) return 0;
    for (wchar_t *p = out; *p; p++) if (*p == L'/') *p = L'\\';
    strip_slash(out);
    return out[0] != 0;
}

static int try_library(const wchar_t *lib, wchar_t *found)
{
    wchar_t dir[MAX_PATH], exe[MAX_PATH];
    swprintf(dir, MAX_PATH, L"%ls%ls", lib, GAME_SUB);
    join(exe, dir, GAME_EXE);
    if (!exists(exe)) return 0;
    wcscpy(found, dir);
    return 1;
}

/* every "path" in <steam>\steamapps\libraryfolders.vdf (UTF-8, "\\" escaped) */
static int try_vdf(const wchar_t *steam, wchar_t *found)
{
    wchar_t vdf[MAX_PATH];
    swprintf(vdf, MAX_PATH, L"%ls\\steamapps\\libraryfolders.vdf", steam);
    HANDLE f = CreateFileW(vdf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    static char buf[1 << 16];
    DWORD n = 0;
    ReadFile(f, buf, sizeof buf - 1, &n, NULL);
    CloseHandle(f);
    buf[n] = 0;
    for (char *p = strstr(buf, "\"path\""); p != NULL; p = strstr(p + 1, "\"path\"")) {
        char *q = strchr(p + 6, '"');
        if (q == NULL) break;
        char path[MAX_PATH * 3];
        size_t k = 0;
        for (q++; *q && *q != '"' && k + 1 < sizeof path; q++) {
            if (*q == '\\' && q[1] == '\\') q++;
            path[k++] = *q;
        }
        path[k] = 0;
        wchar_t lib[MAX_PATH];
        if (MultiByteToWideChar(CP_UTF8, 0, path, -1, lib, MAX_PATH) <= 0) continue;
        strip_slash(lib);
        if (try_library(lib, found)) return 1;
    }
    return 0;
}

static int find_game(wchar_t *found)
{
    wchar_t steam[MAX_PATH];
    const struct { HKEY root; const wchar_t *key, *val; } keys[] = {
        { HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath" },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath" },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath" },
    };
    for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (reg_str(keys[i].root, keys[i].key, keys[i].val, steam, MAX_PATH) &&
            (try_library(steam, found) || try_vdf(steam, found))) return 1;
    return try_library(L"C:\\Program Files (x86)\\Steam", found);
}

/* ---------------------------------------------------------------- state */

enum { G_NONE, G_MISSING, G_FILES, G_OK, G_OTHER };
enum { M_NONE, M_OK, M_UPDATE, M_NOBUNDLE, M_NOTHING };

static int g_game = G_NONE, g_mod = M_NOTHING;
static wchar_t g_hashed_for[MAX_PATH];   /* sotes.exe is 64 MB: hash it once per folder */
static char g_game_hash[65];

static void installed_path(wchar_t *out) { join(out, g_game_dir, MOD_EXE); }
static void bundled_path(wchar_t *out) { join(out, g_self_dir, MOD_EXE); }

static void check_state(void)
{
    g_game = G_NONE;
    if (g_game_dir[0]) {
        wchar_t p[MAX_PATH];
        join(p, g_game_dir, GAME_EXE);
        if (!exists(p)) g_game = G_MISSING;
        else {
            int all = 1;
            for (size_t i = 1; i < sizeof GAME_FILES / sizeof GAME_FILES[0]; i++) {
                wchar_t q[MAX_PATH];
                join(q, g_game_dir, GAME_FILES[i]);
                if (!exists(q)) all = 0;
            }
            if (wcscmp(g_hashed_for, g_game_dir) != 0) {
                if (!file_sha256(p, g_game_hash)) g_game_hash[0] = 0;
                wcscpy(g_hashed_for, g_game_dir);
            }
            g_game = !all ? G_FILES : strcmp(g_game_hash, SOTES_SHA256) == 0 ? G_OK : G_OTHER;
        }
    }
    wchar_t inst[MAX_PATH], bund[MAX_PATH];
    bundled_path(bund);
    int have_bundle = exists(bund);
    int have_inst = g_game_dir[0] && (installed_path(inst), exists(inst));
    if (!have_inst) g_mod = have_bundle ? M_NONE : M_NOTHING;
    else if (!have_bundle) g_mod = M_NOBUNDLE;
    else {
        char a[65], b[65];
        g_mod = (file_sha256(inst, a) && file_sha256(bund, b) && strcmp(a, b) == 0) ? M_OK : M_UPDATE;
    }
}

/* ---------------------------------------------------------------- the window */

enum { ID_PATH = 100, ID_BROWSE, ID_FIND, ID_GAME, ID_MOD, ID_INSTALL, ID_UNINSTALL, ID_SOLO, ID_COOP, ID_PADS_L,
       ID_PADS, ID_PADS_NOTE, ID_CONTROLS, ID_FOOTER, ID_DIRLABEL };
#define TIMER_PADS 1

static HWND g_wnd, g_path, g_game_l, g_mod_l, g_install, g_uninstall, g_solo, g_coop, g_pads;
static HFONT g_font, g_font_big, g_font_mono;
static int g_dpi = 96;
static int S(int v) { return MulDiv(v, g_dpi, 96); }

static HWND mk(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id, HFONT font)
{
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h), g_wnd,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}

static void refresh(void)
{
    check_state();
    SetWindowTextW(g_path, g_game_dir);
    static const int gs[] = { S_G_NONE, S_G_MISSING, S_G_FILES, S_G_OK, S_G_OTHER };
    static const int ms[] = { S_M_NONE, S_M_OK, S_M_UPDATE, S_M_NOBUNDLE, S_M_NOTHING };
    SetWindowTextW(g_game_l, TR(gs[g_game]));
    SetWindowTextW(g_mod_l, g_game == G_NONE || g_game == G_MISSING ? L"" : TR(ms[g_mod]));
    int game_ok = g_game == G_OK || g_game == G_OTHER;
    SetWindowTextW(g_install, TR(g_mod == M_UPDATE ? S_UPDATE : S_INSTALL));
    EnableWindow(g_install, game_ok && (g_mod == M_NONE || g_mod == M_UPDATE));
    EnableWindow(g_uninstall, game_ok && (g_mod == M_OK || g_mod == M_UPDATE || g_mod == M_NOBUNDLE));
    int can_play = game_ok && g_mod != M_NOTHING;
    EnableWindow(g_solo, can_play);
    EnableWindow(g_coop, can_play);
}

/* the joysticks, in the order the game's EnumDevices sees them */
typedef struct { wchar_t names[8][MAX_PATH]; int n; } pad_list;

static BOOL CALLBACK pad_cb(LPCDIDEVICEINSTANCEW d, LPVOID ctx)
{
    pad_list *l = ctx;
    if (l->n < 8) wcsncpy(l->names[l->n++], d->tszInstanceName[0] ? d->tszInstanceName : d->tszProductName, MAX_PATH - 1);
    return DIENUM_CONTINUE;
}

static void list_pads(void)
{
    static const GUID IID_DI7W = { 0x9a4cb685, 0x236d, 0x11d3, { 0x8e, 0x9d, 0x00, 0xc0, 0x4f, 0x68, 0x44, 0xae } };
    typedef HRESULT (WINAPI *create_t)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
    static create_t create;
    static IDirectInput7W *di;
    if (di == NULL) {
        HMODULE m = LoadLibraryW(L"dinput.dll");
        if (m != NULL) create = (create_t)(void *)GetProcAddress(m, "DirectInputCreateEx");
        if (create == NULL || FAILED(create(GetModuleHandleW(NULL), 0x0700, &IID_DI7W, (LPVOID *)&di, NULL))) di = NULL;
    }
    pad_list l;
    memset(&l, 0, sizeof l);
    if (di != NULL) IDirectInput7_EnumDevices(di, DIDEVTYPE_JOYSTICK, pad_cb, &l, DIEDFL_ATTACHEDONLY);

    static wchar_t shown[2048];   /* "" = nothing listed yet; "-" = listed "no controller" */
    wchar_t now[2048] = L"-";
    for (int i = 0; i < l.n; i++) wcsncat(now, l.names[i], 2047 - wcslen(now)), wcsncat(now, L"|", 2047 - wcslen(now));
    if (wcscmp(now, shown) == 0) return;
    wcscpy(shown, now);
    SendMessageW(g_pads, LB_RESETCONTENT, 0, 0);
    if (l.n == 0) SendMessageW(g_pads, LB_ADDSTRING, 0, (LPARAM)TR(S_PAD_NONE));
    for (int i = 0; i < l.n; i++) {
        wchar_t row[MAX_PATH + 32];
        if (i < 2) swprintf(row, MAX_PATH + 32, L"P%d:  %ls", i + 1, l.names[i]);
        else swprintf(row, MAX_PATH + 32, L"—   %ls  (%ls)", l.names[i], TR(S_PAD_UNUSED));
        SendMessageW(g_pads, LB_ADDSTRING, 0, (LPARAM)row);
    }
}

static int ask(const wchar_t *text) { return MessageBoxW(g_wnd, text, TR(S_TITLE), MB_YESNO | MB_ICONQUESTION) == IDYES; }
static void info(const wchar_t *text) { MessageBoxW(g_wnd, text, TR(S_TITLE), MB_OK | MB_ICONINFORMATION); }

static void err(int what, DWORD code)
{
    wchar_t sys[512] = L"", msg[2048];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, 0, sys, 512, NULL);
    swprintf(msg, 2048, L"%ls%ls%ls", TR(what), sys, code == ERROR_ACCESS_DENIED ? TR(S_ERR_DENIED) : L"");
    MessageBoxW(g_wnd, msg, TR(S_TITLE), MB_OK | MB_ICONERROR);
}

/* copy (FO_COPY) or recycle (FO_DELETE + allow undo) a file or folder with the shell: handles whole trees */
static int shell_op(UINT op, const wchar_t *from, const wchar_t *to)
{
    wchar_t f[MAX_PATH + 2] = { 0 }, t[MAX_PATH + 2] = { 0 };
    wcsncpy(f, from, MAX_PATH);
    if (to != NULL) wcsncpy(t, to, MAX_PATH);
    SHFILEOPSTRUCTW s;
    memset(&s, 0, sizeof s);
    s.hwnd = g_wnd;
    s.wFunc = op;
    s.pFrom = f;
    s.pTo = to != NULL ? t : NULL;
    s.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT | (op == FO_DELETE ? FOF_ALLOWUNDO : 0);
    return SHFileOperationW(&s) == 0 && !s.fAnyOperationsAborted;
}

/* the first install backs up user\ (saves + settings) to user-backup-<date>; a backup already made is enough */
static int backup_saves(wchar_t *made)
{
    made[0] = 0;
    wchar_t user[MAX_PATH], pat[MAX_PATH];
    join(user, g_game_dir, L"user");
    if (!is_dir(user)) return 1;
    join(pat, g_game_dir, L"user-backup-*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) { FindClose(h); return 1; }
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t name[64];
    swprintf(name, 64, L"user-backup-%04u%02u%02u-%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    join(made, g_game_dir, name);
    if (!shell_op(FO_COPY, user, made)) { made[0] = 0; return 0; }
    return 1;
}

static int confirm_version(void) { return g_game != G_OTHER || ask(TR(S_ASK_OTHER)); }

static int do_install(int quiet)
{
    if (!confirm_version()) return 0;
    wchar_t backup[MAX_PATH];
    backup_saves(backup);
    wchar_t inst[MAX_PATH], bund[MAX_PATH];
    installed_path(inst);
    bundled_path(bund);
    if (_wcsicmp(inst, bund) != 0 && !CopyFileW(bund, inst, FALSE)) {
        err(S_ERR_COPY, GetLastError());
        refresh();
        return 0;
    }
    if (!quiet || backup[0]) {
        wchar_t msg[1024];
        swprintf(msg, 1024, L"%ls%ls%ls", TR(S_DONE_INSTALL), backup[0] ? TR(S_DONE_BACKUP) : L"", backup);
        info(msg);
    }
    refresh();
    return 1;
}

static void do_uninstall(void)
{
    if (!ask(TR(S_ASK_UNINSTALL))) return;
    wchar_t inst[MAX_PATH], bund[MAX_PATH], p[MAX_PATH];
    installed_path(inst);
    bundled_path(bund);
    if (_wcsicmp(inst, bund) != 0 && !DeleteFileW(inst) && GetLastError() != ERROR_FILE_NOT_FOUND) {
        err(S_ERR_COPY, GetLastError());
        refresh();
        return;
    }
    join(p, g_game_dir, L"coop.log");
    DeleteFileW(p);
    join(p, g_game_dir, L"opensummoners-standalone");   /* the mod exe's log + audit folder */
    if (is_dir(p)) shell_op(FO_DELETE, p, NULL);
    info(TR(S_DONE_UNINSTALL));
    refresh();
}

static void play(int coop)
{
    check_state();
    if (g_mod == M_NONE || g_mod == M_UPDATE) {
        if (!ask(TR(S_ASK_INSTALL)) || !do_install(1)) return;
    } else if (!confirm_version()) {
        return;
    }
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 32];
    installed_path(exe);
    swprintf(cmd, MAX_PATH + 32, L"\"%ls\"%ls", exe, coop ? L" --coop" : L"");
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessW(exe, cmd, NULL, NULL, FALSE, 0, NULL, g_game_dir, &si, &pi)) {
        err(S_ERR_START, GetLastError());
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ShowWindow(g_wnd, SW_MINIMIZE);
}

static void set_game_dir(const wchar_t *dir)
{
    wcsncpy(g_game_dir, dir, MAX_PATH - 1);
    strip_slash(g_game_dir);
    settings_save();
    wchar_t ico[MAX_PATH];
    join(ico, g_game_dir, GAME_EXE);
    HICON big = NULL, small = NULL;
    if (ExtractIconExW(ico, 0, &big, &small, 1) > 0) {   /* the user's own sotes.exe icon */
        SendMessageW(g_wnd, WM_SETICON, ICON_BIG, (LPARAM)big);
        SendMessageW(g_wnd, WM_SETICON, ICON_SMALL, (LPARAM)small);
    }
    refresh();
}

static void browse(void)
{
    BROWSEINFOW bi;
    memset(&bi, 0, sizeof bi);
    bi.hwndOwner = g_wnd;
    bi.lpszTitle = TR(S_PICK_FOLDER);
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE id = SHBrowseForFolderW(&bi);
    if (id == NULL) return;
    wchar_t dir[MAX_PATH], exe[MAX_PATH];
    int ok = SHGetPathFromIDListW(id, dir);
    CoTaskMemFree(id);
    if (!ok) return;
    join(exe, dir, GAME_EXE);
    if (!exists(exe)) { MessageBoxW(g_wnd, TR(S_ERR_NOT_GAME), TR(S_TITLE), MB_OK | MB_ICONWARNING); return; }
    set_game_dir(dir);
}

static void find_in_steam(int quiet)
{
    wchar_t dir[MAX_PATH];
    if (find_game(dir)) set_game_dir(dir);
    else if (!quiet) MessageBoxW(g_wnd, TR(S_NOT_FOUND), TR(S_TITLE), MB_OK | MB_ICONWARNING);
}

/* the "Controls..." window: a read-only text box in a fixed-width font */
static LRESULT CALLBACK help_proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_SIZE) {
        HWND e = GetDlgItem(w, 1);
        if (e) MoveWindow(e, 0, 0, LOWORD(lp), HIWORD(lp), TRUE);
        return 0;
    }
    if (m == WM_CTLCOLORSTATIC) {   /* read-only edits paint like statics: keep them white */
        SetBkColor((HDC)wp, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    return DefWindowProcW(w, m, wp, lp);
}

static void show_controls(void)
{
    static HWND h;
    if (h != NULL && IsWindow(h)) { SetForegroundWindow(h); return; }
    wchar_t title[64];   /* the button's label without its "..." */
    wcsncpy(title, TR(S_CONTROLS), 63);
    title[63] = 0;
    for (size_t n = wcslen(title); n > 0 && title[n - 1] == L'.'; ) title[--n] = 0;
    RECT r = { 0, 0, S(720), S(470) };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    h = CreateWindowExW(0, L"CoopSummonersHelp", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                        CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, g_wnd, NULL, GetModuleHandleW(NULL), NULL);
    HWND e = CreateWindowExW(0, L"EDIT", TR(S_CONTROLS_TEXT),
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0,
                             S(720), S(470), h, (HMENU)1, GetModuleHandleW(NULL), NULL);
    SendMessageW(e, WM_SETFONT, (WPARAM)g_font_mono, TRUE);
    SendMessageW(e, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(10), S(10)));
}

static void build_ui(void)
{
    const int X = 16, W = 548;
    int y = 14;
    mk(L"STATIC", TR(S_GAMEDIR), 0, X, y, W, 20, ID_DIRLABEL, g_font);
    y += 22;
    g_path = mk(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, X, y, W - 230, 24, ID_PATH, g_font);
    mk(L"BUTTON", TR(S_BROWSE), BS_PUSHBUTTON, X + W - 222, y - 1, 104, 26, ID_BROWSE, g_font);
    mk(L"BUTTON", TR(S_FIND), BS_PUSHBUTTON, X + W - 112, y - 1, 112, 26, ID_FIND, g_font);
    y += 34;
    g_game_l = mk(L"STATIC", L"", 0, X, y, W, 36, ID_GAME, g_font);
    y += 38;
    g_mod_l = mk(L"STATIC", L"", 0, X, y, W, 36, ID_MOD, g_font);
    y += 40;
    g_install = mk(L"BUTTON", TR(S_INSTALL), BS_PUSHBUTTON, X, y, 150, 28, ID_INSTALL, g_font);
    g_uninstall = mk(L"BUTTON", TR(S_UNINSTALL), BS_PUSHBUTTON, X + 160, y, 130, 28, ID_UNINSTALL, g_font);
    y += 44;
    g_solo = mk(L"BUTTON", TR(S_SOLO), BS_PUSHBUTTON, X, y, (W - 12) / 2, 52, ID_SOLO, g_font_big);
    g_coop = mk(L"BUTTON", TR(S_COOP), BS_DEFPUSHBUTTON, X + (W - 12) / 2 + 12, y, (W - 12) / 2, 52, ID_COOP, g_font_big);
    y += 68;
    mk(L"STATIC", TR(S_PADS), 0, X, y, W, 20, ID_PADS_L, g_font);
    y += 22;
    g_pads = mk(L"LISTBOX", L"", WS_BORDER | LBS_NOSEL | LBS_NOINTEGRALHEIGHT, X, y, W, 70, ID_PADS, g_font);
    y += 74;
    mk(L"STATIC", TR(S_PADS_NOTE), 0, X, y, W, 36, ID_PADS_NOTE, g_font);
    y += 42;
    mk(L"BUTTON", TR(S_CONTROLS), BS_PUSHBUTTON, X, y, 130, 28, ID_CONTROLS, g_font);
    y += 40;
    mk(L"STATIC", TR(S_FOOTER), SS_CENTER, X, y, W, 40, ID_FOOTER, g_font);
}

static LRESULT CALLBACK wnd_proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BROWSE: browse(); break;
        case ID_FIND: find_in_steam(0); break;
        case ID_INSTALL: do_install(0); break;
        case ID_UNINSTALL: do_uninstall(); break;
        case ID_SOLO: play(0); break;
        case ID_COOP: play(1); break;
        case ID_CONTROLS: show_controls(); break;
        }
        return 0;
    case WM_TIMER:
        if (wp == TIMER_PADS) list_pads();
        return 0;
    case WM_ACTIVATE:   /* back from the game (or after files changed): re-read the state */
        if (LOWORD(wp) != WA_INACTIVE && g_game_l != NULL) refresh();
        break;
    case WM_CTLCOLORSTATIC:
        if ((HWND)lp == g_path) break;   /* the read-only path box keeps its own colours */
        SetBkMode((HDC)wp, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show)
{
    (void)prev;
    if ((GetUserDefaultUILanguage() & 0x3ff) == LANG_PORTUGUESE) T = STR_PT;
    if (wcsstr(cmdline, L"--lang en") != NULL) T = STR_EN;   /* override the Windows language */
    if (wcsstr(cmdline, L"--lang pt") != NULL) T = STR_PT;
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    GetModuleFileNameW(NULL, g_self_dir, MAX_PATH);
    wchar_t *slash = wcsrchr(g_self_dir, L'\\');
    if (slash) *slash = 0;
    settings_init();
    settings_load();

    HDC dc = GetDC(NULL);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    NONCLIENTMETRICSW ncm;
    memset(&ncm, 0, sizeof ncm);
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW big = ncm.lfMessageFont;
    big.lfHeight = -S(18);
    big.lfWeight = FW_BOLD;
    g_font_big = CreateFontIndirectW(&big);
    g_font_mono = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                              FIXED_PITCH | FF_MODERN, L"Consolas");

    WNDCLASSW wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"CoopSummonersLauncher";
    RegisterClassW(&wc);
    wc.lpfnWndProc = help_proc;
    wc.lpszClassName = L"CoopSummonersHelp";
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    RegisterClassW(&wc);

    RECT r = { 0, 0, S(580), S(540) };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    g_wnd = CreateWindowExW(0, L"CoopSummonersLauncher", TR(S_TITLE), style,
                            CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    build_ui();

    if (g_game_dir[0] == 0 || !is_dir(g_game_dir)) {
        g_game_dir[0] = 0;
        find_in_steam(1);
    } else {
        set_game_dir(g_game_dir);
    }
    refresh();
    list_pads();
    SetTimer(g_wnd, TIMER_PADS, 2000, NULL);

    ShowWindow(g_wnd, show);
    UpdateWindow(g_wnd);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (IsDialogMessageW(g_wnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return 0;
}
