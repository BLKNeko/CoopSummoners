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
    S_ERR_COPY, S_ERR_DENIED, S_ERR_START, S_ERR_NOT_GAME, S_NOT_FOUND, S_CONTROLS_TEXT,
    S_GAMELANG, S_LANG_EN, S_LANG_PT, S_LANG_PT_NAMES,
    S_WSCALE, S_WS_1, S_WS_AUTO, S_WS_2, S_WS_3, S_ASK_NO_BACKUP, S_WS_NOFIT,
    S_SCREEN, S_SM_BORDERLESS, S_SM_WINDOW, S_SM_FULL, S_SM_ASK,
    S_PLAYER, S_DEV_P1_DEFAULT, S_DEV_KB_ONLY, S_DEV_NONE, S_DEV_KB, S_DEV_PAD, S_DEV_GONE, S_KEYS_BTN, S_KEYS_TITLE,
    S_ACT_0, S_ACT_1, S_ACT_2, S_ACT_3, S_ACT_4, S_ACT_5, S_ACT_6, S_ACT_7, S_ACT_8,
    S_PRESS_KEY, S_PRESS_BTN, S_BUTTON_N, S_PAD_DIRS, S_DEFAULTS, S_OK_BTN, S_ASK_SAME_PAD, S_KEY_NONE, S_N
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
    L"Players and devices (plug the gamepads in before starting the game):",
    L"Player 1 uses the game's own key setup. Players 2 and 3 join by holding their switch key 1 s.",
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
    L"Each player's device is chosen in the launcher (Player 1 / 2 / 3).\r\n"
    L"Player 1 uses the game's own keys (arrows, X, C, Enter...; change\r\n"
    L"them in the game's options). Players 2 and 3: \"Keys...\" next to\r\n"
    L"their device (defaults: player 2 numpad 8/5/4/6, 1 attack, 2 jump,\r\n"
    L"3 use, 0 sheathe, + switch; player 3 I/K/J/L, U attack, O jump,\r\n"
    L"P use, Y sheathe, H switch; gamepads move with the stick / d-pad).\r\n"
    L"\r\n"
    L"Switch key in co-op\r\n"
    L"- Two players tap within 0.5 s: they swap characters (any pair).\r\n"
    L"- One player taps alone: the game's normal switch to a free AI member.\r\n"
    L"- Player 2 / 3 out of the game holds it 1 s: joins (takes a free\r\n"
    L"  member, or waits until one is free).\r\n"
    L"- Player 2 / 3 holds it 1.5 s: leaves (the AI takes over).\r\n"
    L"- Player 1 holds it 5 s: everyone else is dropped out.\r\n"
    L"\r\n"
    L"Players 2 and 3 start out of the game (the AI plays the others).\r\n"
    L"With one free ally, whoever joins first plays it; the other waits.\r\n"
    L"\r\n"
    L"The light (player 2 / 3 with no character: out or waiting)\r\n"
    L"- Any direction / attack / jump / use: a light appears at the leader.\r\n"
    L"  It flies with the directions, through walls, and stays on screen.\r\n"
    L"- It takes the coins it touches. Its number = your\r\n"
    L"  coins this session.\r\n"
    L"- Tap attack: a wave (every 20 s). Allies near it heal 8% of their\r\n"
    L"  max HP; foes take 25% of a plain hit of the leader. The arc round\r\n"
    L"  the light fills while it recharges.\r\n"
    L"- Hold attack: magnet, the coins near it fly to it.\r\n"
    L"- Hold jump: carries the first item it reaches (bring it to the\r\n"
    L"  leader; let go, it falls). Jump with no item near, or use: a ring\r\n"
    L"  where it is, to show the others something.\r\n"
    L"- It goes away after 10 s without input, and when you take a character.\r\n"
    L"\r\n"
    L"Player 1 is always the leader (camera, talking, menus).\r\n"
    L"A fallen player can walk but not attack until healed.\r\n"
    L"Game over only when the whole party is down.\r\n",
    L"Game language:", L"English (original)", L"Português (Brasil)", L"Português, original names",
    L"Window size:", L"Original (640×480)", L"Largest that fits", L"2× (1280×960)", L"3× (1920×1440)",
    L"Could not back up the saves (user\\ to user-backups\\).\n\nPlay anyway?",
    L"too big for this screen",
    L"Screen mode:", L"Borderless full screen (recommended)", L"Window", L"Original full screen (may lag)",
    L"Ask each time (the game's options window)",
    L"Player %d:", L"Keyboard + 1st gamepad (default)", L"Keyboard only", L"Nobody", L"Keyboard", L"Gamepad: ",
    L" (not connected)", L"Keys...", L"Keys — player %d",
    L"Up", L"Down", L"Left", L"Right", L"Attack", L"Jump", L"Use / investigate", L"Sheathe / skill modifier",
    L"Switch character",
    L"Press a key... (Esc cancels)", L"Press a gamepad button... (Esc cancels)", L"Button %d",
    L"Move: left stick or d-pad.", L"Defaults", L"OK",
    L"Two players are set to the same gamepad. Play anyway?", L"(none)",
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
    L"Jogadores e dispositivos (conecte os controles antes de abrir o jogo):",
    L"O jogador 1 usa a configuração de teclas do próprio jogo. Os jogadores 2 e 3 entram segurando a tecla de troca por 1 s.",
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
    L"O dispositivo de cada jogador é escolhido no launcher (Jogador 1 / 2 / 3).\r\n"
    L"O jogador 1 usa as teclas do próprio jogo (setas, X, C, Enter...; mude\r\n"
    L"nas opções do jogo). Jogadores 2 e 3: \"Teclas...\" ao lado do\r\n"
    L"dispositivo (padrão: jogador 2 numérico 8/5/4/6, 1 atacar, 2 pular,\r\n"
    L"3 usar, 0 guardar, + trocar; jogador 3 I/K/J/L, U atacar, O pular,\r\n"
    L"P usar, Y guardar, H trocar; controles andam com analógico / direcional).\r\n"
    L"\r\n"
    L"Tecla de troca no co-op\r\n"
    L"- Dois jogadores apertam em até 0,5 s: trocam de personagem (qualquer par).\r\n"
    L"- Só um aperta: a troca normal do jogo para um membro livre da IA.\r\n"
    L"- Jogador 2 / 3 fora do jogo segura 1 s: entra (pega um membro livre,\r\n"
    L"  ou espera até ter um).\r\n"
    L"- Jogador 2 / 3 segura 1,5 s: sai do jogo (a IA assume).\r\n"
    L"- O jogador 1 segura 5 s: tira todos os outros do jogo.\r\n"
    L"\r\n"
    L"Os jogadores 2 e 3 começam fora do jogo (a IA joga com os outros).\r\n"
    L"Com só um aliado livre, quem entrar primeiro fica com ele; o outro espera.\r\n"
    L"\r\n"
    L"A luz (jogador 2 / 3 sem personagem: fora do jogo ou esperando)\r\n"
    L"- Qualquer direção / atacar / pular / usar: uma luz aparece no líder.\r\n"
    L"  Ela voa com as direções, atravessa paredes e fica dentro da tela.\r\n"
    L"- Pega as moedas em que encostar. O número embaixo dela =\r\n"
    L"  as suas moedas nesta sessão.\r\n"
    L"- Toque atacar: uma onda (a cada 20 s). Aliados perto curam 8% do HP\r\n"
    L"  máximo; inimigos levam 25% de um golpe normal do líder. O arco em\r\n"
    L"  volta da luz enche enquanto ela recarrega.\r\n"
    L"- Segure atacar: ímã, as moedas perto voam até ela.\r\n"
    L"- Segure pular: carrega o primeiro item que alcançar (leve até o\r\n"
    L"  líder; soltou, ele cai). Pular sem item perto, ou usar: um anel\r\n"
    L"  onde ela está, para mostrar algo aos outros.\r\n"
    L"- Some depois de 10 s sem tocar em nada, e quando você pega um personagem.\r\n"
    L"\r\n"
    L"O jogador 1 é sempre o líder (câmera, conversas, menus).\r\n"
    L"Um jogador caído pode andar, mas não atacar, até ser curado.\r\n"
    L"Game over só quando o grupo inteiro cai.\r\n",
    L"Idioma do jogo:", L"Inglês (original)", L"Português (Brasil)", L"Português, nomes originais",
    L"Tamanho da janela:", L"Original (640×480)", L"O maior que couber", L"2× (1280×960)", L"3× (1920×1440)",
    L"Não foi possível fazer o backup dos saves (user\\ para user-backups\\).\n\nJogar mesmo assim?",
    L"não cabe nesta tela",
    L"Modo de tela:", L"Tela cheia sem bordas (recomendado)", L"Janela", L"Tela cheia original (pode travar)",
    L"Perguntar ao abrir (janela de opções do jogo)",
    L"Jogador %d:", L"Teclado + 1º controle (padrão)", L"Só teclado", L"Ninguém", L"Teclado", L"Controle: ",
    L" (desconectado)", L"Teclas...", L"Teclas — jogador %d",
    L"Cima", L"Baixo", L"Esquerda", L"Direita", L"Atacar", L"Pular", L"Usar / investigar", L"Guardar arma / modif. habilidade",
    L"Trocar personagem",
    L"Aperte uma tecla... (Esc cancela)", L"Aperte um botão do controle... (Esc cancela)", L"Botão %d",
    L"Mover: analógico esquerdo ou direcional.", L"Padrão", L"OK",
    L"Dois jogadores estão no mesmo controle. Jogar mesmo assim?", L"(nenhuma)",
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

/* the game's language: 0 English (the game's own text), 1 Portuguese (the translation files, --lang pt-br),
 * 2 Portuguese keeping the item / spell / monster / place names in English (+ --lang-names original);
 * -1 = never chosen: Portuguese on a Portuguese Windows when the translation ships, else English */
static int g_lang_pt = -1;
static const wchar_t *const LANG_INI[3] = { L"en", L"pt-br", L"pt-br-original-names" };
/* the game window's size when "Windowed" is picked in the game's own launcher (--window-scale): the combo's order */
static int g_wscale = 1;
static const wchar_t *const WSCALE_INI[4] = { L"1", L"auto", L"2", L"3" };
/* how the game shows: the launcher passes it (--no-launcher --screen N [--borderless]) so the game's own options
 * window is skipped; "ask" shows that window as before.  The combo's order. */
enum { SCREEN_BORDERLESS, SCREEN_WINDOW, SCREEN_FULL, SCREEN_ASK };
static int g_screen = SCREEN_BORDERLESS;
static const wchar_t *const SCREEN_INI[4] = { L"borderless", L"window", L"fullscreen", L"ask" };

/* the players' devices and keys ([p1] [p2] [p3] in launcher.ini; written for the game as coop-controls.ini at
 * play).  Player 1: the game's own keys, a device choice only; players 2 / 3: keyboard keys (DIK codes) or
 * gamepad buttons for the 9 actions (the order of the mod's COOP_A_*: up down left right attack jump use mod
 * switch; a gamepad moves with its stick / d-pad, its buttons cover attack..switch) */
#define NPLAYERS 3
#define NACT 9
enum { DEV_DEFAULT, DEV_KB, DEV_PAD, DEV_NONE, DEV_UNSET = -1 };
static const wchar_t *const DEV_INI[4] = { L"default", L"keyboard", L"pad", L"none" };
typedef struct {
    int dev;
    wchar_t guid[40];            /* the gamepad's DirectInput instance GUID, "{...}" */
    wchar_t name[MAX_PATH];
    uint8_t keys[NACT];
    uint8_t buttons[NACT];
} player_cfg;
static player_cfg g_pl[NPLAYERS];
static const uint8_t DEF_KEYS[NPLAYERS][NACT] = {
    { 0 }, { 0x48, 0x4c, 0x4b, 0x4d, 0x4f, 0x50, 0x51, 0x52, 0x4e }, { 0x17, 0x25, 0x24, 0x26, 0x16, 0x18, 0x19, 0x15, 0x23 } };
static const uint8_t DEF_BUTTONS[NACT] = { 0, 0, 0, 0, 2, 0, 1, 4, 3 };

static void list_parse(const wchar_t *s, uint8_t *out)
{
    for (int a = 0; a < NACT && *s; a++) {
        out[a] = (uint8_t)wcstoul(s, (wchar_t **)&s, 0);
        while (*s == L',' || *s == L' ') s++;
    }
}
static void list_format(const uint8_t *v, wchar_t *out, size_t n)
{
    out[0] = 0;
    for (int a = 0; a < NACT; a++) swprintf(out + wcslen(out), n - wcslen(out), a ? L",%u" : L"%u", v[a]);
}

static void players_load(void)
{
    for (int p = 0; p < NPLAYERS; p++) {
        player_cfg *c = &g_pl[p];
        wchar_t sec[8], v[128];
        swprintf(sec, 8, L"p%d", p + 1);
        memcpy(c->keys, DEF_KEYS[p], NACT);
        memcpy(c->buttons, DEF_BUTTONS, NACT);
        GetPrivateProfileStringW(sec, L"device", L"", v, 128, g_ini);
        c->dev = DEV_UNSET;
        for (int i = 0; i < 4; i++) if (wcscmp(v, DEV_INI[i]) == 0) c->dev = i;
        if (p == 0 && c->dev == DEV_NONE) c->dev = DEV_DEFAULT;
        if (p > 0 && c->dev == DEV_DEFAULT) c->dev = DEV_UNSET;
        GetPrivateProfileStringW(sec, L"pad_guid", L"", c->guid, 40, g_ini);
        GetPrivateProfileStringW(sec, L"pad_name", L"", c->name, MAX_PATH, g_ini);
        if (GetPrivateProfileStringW(sec, L"keys", L"", v, 128, g_ini)) list_parse(v, c->keys);
        if (GetPrivateProfileStringW(sec, L"buttons", L"", v, 128, g_ini)) list_parse(v, c->buttons);
    }
    if (g_pl[0].dev == DEV_UNSET) g_pl[0].dev = DEV_DEFAULT;
}
static void players_save(void)
{
    for (int p = 0; p < NPLAYERS; p++) {
        const player_cfg *c = &g_pl[p];
        if (c->dev == DEV_UNSET) continue;
        wchar_t sec[8], v[128];
        swprintf(sec, 8, L"p%d", p + 1);
        WritePrivateProfileStringW(sec, L"device", DEV_INI[c->dev], g_ini);
        WritePrivateProfileStringW(sec, L"pad_guid", c->guid, g_ini);
        WritePrivateProfileStringW(sec, L"pad_name", c->name, g_ini);
        if (p > 0) {
            list_format(c->keys, v, 128);
            WritePrivateProfileStringW(sec, L"keys", v, g_ini);
            list_format(c->buttons, v, 128);
            WritePrivateProfileStringW(sec, L"buttons", v, g_ini);
        }
    }
}

static void settings_load(void)
{
    players_load();
    GetPrivateProfileStringW(L"launcher", L"GameDir", L"", g_game_dir, MAX_PATH, g_ini);
    wchar_t l[32];
    GetPrivateProfileStringW(L"launcher", L"GameLang", L"", l, 32, g_ini);
    for (int i = 0; l[0] && i < 3; i++)
        if (wcscmp(l, LANG_INI[i]) == 0) g_lang_pt = i;
    GetPrivateProfileStringW(L"launcher", L"ScreenMode", L"borderless", l, 32, g_ini);
    for (int i = 0; i < 4; i++)
        if (wcscmp(l, SCREEN_INI[i]) == 0) g_screen = i;
    GetPrivateProfileStringW(L"launcher", L"WindowScale", L"auto", l, 32, g_ini);
    for (int i = 0; i < 4; i++)
        if (wcscmp(l, WSCALE_INI[i]) == 0) g_wscale = i;
}
static void settings_save(void)
{
    WritePrivateProfileStringW(L"launcher", L"GameDir", g_game_dir, g_ini);
    if (g_lang_pt >= 0) WritePrivateProfileStringW(L"launcher", L"GameLang", LANG_INI[g_lang_pt], g_ini);
    WritePrivateProfileStringW(L"launcher", L"WindowScale", WSCALE_INI[g_wscale], g_ini);
    WritePrivateProfileStringW(L"launcher", L"ScreenMode", SCREEN_INI[g_screen], g_ini);
    players_save();
}

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
/* the translation files, next to the launcher (shipped in the zip) and in the game folder: the text, the table
 * names (items, spells, monsters, places), and the overlay putting those names back in English in sentences */
#define LANG_FILE L"lang\\pt-br.tsv"
static const wchar_t *const LANG_FILES[] = { LANG_FILE, L"lang\\pt-br.names.tsv", L"lang\\pt-br.en-names.tsv" };
#define N_LANG_FILES (int)(sizeof LANG_FILES / sizeof LANG_FILES[0])
static void lang_installed_k(wchar_t *out, int k) { join(out, g_game_dir, LANG_FILES[k]); }
static void lang_bundled_k(wchar_t *out, int k) { join(out, g_self_dir, LANG_FILES[k]); }
static void lang_installed(wchar_t *out) { lang_installed_k(out, 0); }
static void lang_bundled(wchar_t *out) { lang_bundled_k(out, 0); }

static int same_file(const wchar_t *a, const wchar_t *b)
{
    char x[65], y[65];
    return file_sha256(a, x) && file_sha256(b, y) && strcmp(x, y) == 0;
}

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
        g_mod = same_file(inst, bund) ? M_OK : M_UPDATE;
        for (int k = 0; k < N_LANG_FILES; k++) {   /* a newer translation file is an update too */
            wchar_t li[MAX_PATH], lb[MAX_PATH];
            lang_installed_k(li, k);
            lang_bundled_k(lb, k);
            if (g_mod == M_OK && exists(lb) && _wcsicmp(li, lb) != 0 && !(exists(li) && same_file(li, lb)))
                g_mod = M_UPDATE;
        }
    }
}

/* a translation is available: shipped next to the launcher or already in the game folder */
static int lang_available(void)
{
    wchar_t li[MAX_PATH], lb[MAX_PATH];
    lang_bundled(lb);
    if (exists(lb)) return 1;
    if (!g_game_dir[0]) return 0;
    lang_installed(li);
    return exists(li);
}

/* ---------------------------------------------------------------- the window */

enum { ID_PATH = 100, ID_BROWSE, ID_FIND, ID_GAME, ID_MOD, ID_INSTALL, ID_UNINSTALL, ID_SOLO, ID_COOP, ID_PADS_L,
       ID_PADS, ID_PADS_NOTE, ID_CONTROLS, ID_FOOTER, ID_DIRLABEL, ID_LANG_L, ID_LANG, ID_WSCALE_L, ID_WSCALE,
       ID_SCREEN_L, ID_SCREEN, ID_DEV0 = 300, ID_KEYS0 = 310 };
#define TIMER_PADS 1

static HWND g_wnd, g_path, g_game_l, g_mod_l, g_install, g_uninstall, g_solo, g_coop, g_lang, g_wscale_c, g_screen_c;
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
    int have_lang = lang_available();
    if (g_lang_pt < 0) g_lang_pt = have_lang && T == STR_PT;   /* first run: follow the Windows language */
    SendMessageW(g_lang, CB_SETCURSEL, have_lang && g_lang_pt > 0 ? g_lang_pt : 0, 0);
    EnableWindow(g_lang, have_lang);
}

/* ---------------------------------------------------------------- the players' devices */

/* the joysticks, in the order the game's EnumDevices sees them, with their instance GUIDs */
typedef struct { wchar_t names[8][MAX_PATH]; wchar_t guids[8][40]; GUID g[8]; int n; } pad_list;
static pad_list g_padl;
static IDirectInput7W *g_di;
static HWND g_dev_c[NPLAYERS], g_keys_b[NPLAYERS];
/* each combo item: the device kind and the gamepad (index into g_padl, -2 = the saved one, not connected) */
static struct { int dev, pad; } g_item[NPLAYERS][16];

static BOOL CALLBACK pad_cb(LPCDIDEVICEINSTANCEW d, LPVOID ctx)
{
    pad_list *l = ctx;
    if (l->n < 8) {
        wcsncpy(l->names[l->n], d->tszInstanceName[0] ? d->tszInstanceName : d->tszProductName, MAX_PATH - 1);
        StringFromGUID2(&d->guidInstance, l->guids[l->n], 40);
        l->g[l->n] = d->guidInstance;
        l->n++;
    }
    return DIENUM_CONTINUE;
}

/* player p's device combo, from g_pl[p] and the gamepads connected now */
static void fill_dev_combo(int p)
{
    HWND c = g_dev_c[p];
    player_cfg *pc = &g_pl[p];
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    int n = 0, sel = 0;
    wchar_t row[MAX_PATH + 64];
    if (p == 0) {
        g_item[p][n].dev = DEV_DEFAULT; g_item[p][n++].pad = -1;
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)TR(S_DEV_P1_DEFAULT));
        g_item[p][n].dev = DEV_KB; g_item[p][n++].pad = -1;
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)TR(S_DEV_KB_ONLY));
        if (pc->dev == DEV_KB) sel = 1;
    } else {
        g_item[p][n].dev = DEV_NONE; g_item[p][n++].pad = -1;
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)TR(S_DEV_NONE));
        g_item[p][n].dev = DEV_KB; g_item[p][n++].pad = -1;
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)TR(S_DEV_KB));
        if (pc->dev == DEV_KB) sel = 1;
    }
    int found = 0;
    for (int i = 0; i < g_padl.n && n < 15; i++) {
        swprintf(row, MAX_PATH + 64, L"%ls%ls", TR(S_DEV_PAD), g_padl.names[i]);
        if (pc->dev == DEV_PAD && _wcsicmp(pc->guid, g_padl.guids[i]) == 0) { sel = n; found = 1; }
        g_item[p][n].dev = DEV_PAD; g_item[p][n++].pad = i;
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)row);
    }
    if (pc->dev == DEV_PAD && !found) {   /* the saved gamepad, unplugged: kept */
        swprintf(row, MAX_PATH + 64, L"%ls%ls%ls", TR(S_DEV_PAD), pc->name, TR(S_DEV_GONE));
        sel = n;
        g_item[p][n].dev = DEV_PAD; g_item[p][n++].pad = -2;
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)row);
    }
    SendMessageW(c, CB_SETCURSEL, sel, 0);
    if (g_keys_b[p] != NULL) EnableWindow(g_keys_b[p], pc->dev == DEV_KB || pc->dev == DEV_PAD);
}

static void dev_changed(int p)
{
    int k = (int)SendMessageW(g_dev_c[p], CB_GETCURSEL, 0, 0);
    if (k < 0 || k >= 16) return;
    player_cfg *pc = &g_pl[p];
    pc->dev = g_item[p][k].dev;
    int i = g_item[p][k].pad;
    if (pc->dev == DEV_PAD && i >= 0) {
        wcscpy(pc->guid, g_padl.guids[i]);
        wcsncpy(pc->name, g_padl.names[i], MAX_PATH - 1);
    }
    if (g_keys_b[p] != NULL) EnableWindow(g_keys_b[p], pc->dev == DEV_KB || pc->dev == DEV_PAD);
    settings_save();
}

static void list_pads(void)
{
    static const GUID IID_DI7W = { 0x9a4cb685, 0x236d, 0x11d3, { 0x8e, 0x9d, 0x00, 0xc0, 0x4f, 0x68, 0x44, 0xae } };
    typedef HRESULT (WINAPI *create_t)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
    static create_t create;
    if (g_di == NULL) {
        HMODULE m = LoadLibraryW(L"dinput.dll");
        if (m != NULL) create = (create_t)(void *)GetProcAddress(m, "DirectInputCreateEx");
        if (create == NULL || FAILED(create(GetModuleHandleW(NULL), 0x0700, &IID_DI7W, (LPVOID *)&g_di, NULL))) g_di = NULL;
    }
    pad_list l;
    memset(&l, 0, sizeof l);
    if (g_di != NULL) IDirectInput7_EnumDevices(g_di, DIDEVTYPE_JOYSTICK, pad_cb, &l, DIEDFL_ATTACHEDONLY);

    static int listed;
    if (listed && l.n == g_padl.n && memcmp(l.guids, g_padl.guids, sizeof l.guids) == 0) return;
    g_padl = l;
    if (!listed) {   /* first run without saved devices: player 2 = the 2nd gamepad else the keyboard (numpad),
                      * player 3 = the 3rd gamepad else nobody */
        listed = 1;
        int changed = 0;
        for (int p = 1; p < NPLAYERS; p++) {
            if (g_pl[p].dev != DEV_UNSET) continue;
            changed = 1;
            if (l.n > p) {
                g_pl[p].dev = DEV_PAD;
                wcscpy(g_pl[p].guid, l.guids[p]);
                wcsncpy(g_pl[p].name, l.names[p], MAX_PATH - 1);
            } else g_pl[p].dev = p == 1 ? DEV_KB : DEV_NONE;
        }
        if (changed) settings_save();
    }
    for (int p = 0; p < NPLAYERS; p++) if (g_dev_c[p] != NULL) fill_dev_combo(p);
}

/* coop-controls.ini in the game folder: what the mod reads (rhost's load_controls) */
static void write_controls(void)
{
    wchar_t f[MAX_PATH], v[128], sec[8];
    join(f, g_game_dir, L"coop-controls.ini");
    DeleteFileW(f);
    for (int p = 0; p < NPLAYERS; p++) {
        const player_cfg *c = &g_pl[p];
        int dev = c->dev == DEV_UNSET ? (p == 0 ? DEV_DEFAULT : DEV_NONE) : c->dev;
        swprintf(sec, 8, L"p%d", p + 1);
        WritePrivateProfileStringW(sec, L"keyboard", dev == DEV_DEFAULT || dev == DEV_KB ? L"1" : L"0", f);
        WritePrivateProfileStringW(sec, L"pad", dev == DEV_DEFAULT || dev == DEV_PAD ? L"1" : L"0", f);
        if (dev == DEV_DEFAULT) WritePrivateProfileStringW(sec, L"pad_index", L"0", f);
        if (dev == DEV_PAD) {
            WritePrivateProfileStringW(sec, L"pad_guid", c->guid, f);
            WritePrivateProfileStringW(sec, L"pad_name", c->name, f);
        }
        if (p > 0) {
            list_format(c->keys, v, 128);
            WritePrivateProfileStringW(sec, L"keys", v, f);
            list_format(c->buttons, v, 128);
            WritePrivateProfileStringW(sec, L"buttons", v, f);
        }
    }
}

/* two players on one gamepad (player 1's default = the 1st one connected) */
static int same_pad_twice(void)
{
    const wchar_t *g[NPLAYERS];
    for (int p = 0; p < NPLAYERS; p++) {
        g[p] = NULL;
        if (g_pl[p].dev == DEV_PAD) g[p] = g_pl[p].guid;
        else if (p == 0 && g_pl[p].dev == DEV_DEFAULT && g_padl.n > 0) g[p] = g_padl.guids[0];
        for (int q = 0; q < p; q++) if (g[p] && g[q] && _wcsicmp(g[p], g[q]) == 0) return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- the "Keys..." window (players 2 / 3) */

#define TIMER_CAPTURE 2
static HWND g_kwin, g_kbtn[NACT];
static int g_kp, g_kcap = -1;
static uint8_t g_kdown[256];               /* keys / buttons held when the capture started (ignored) */
static IDirectInputDevice7W *g_kdev;       /* the player's gamepad while the window is open */

static void key_name(uint8_t dik, wchar_t *out, int n)
{
    if (dik == 0) { wcsncpy(out, TR(S_KEY_NONE), (size_t)n); return; }
    LONG lp = (LONG)((dik & 0x7f) << 16) | ((dik & 0x80) ? (1L << 24) : 0);
    if (!GetKeyNameTextW(lp, out, n)) swprintf(out, (size_t)n, L"0x%02X", dik);
}

static void keys_refresh(void)
{
    const player_cfg *c = &g_pl[g_kp];
    for (int a = 0; a < NACT; a++) {
        if (g_kbtn[a] == NULL) continue;
        wchar_t t[64];
        if (a == g_kcap) wcscpy(t, L"...");
        else if (c->dev == DEV_PAD) swprintf(t, 64, TR(S_BUTTON_N), c->buttons[a] + 1);
        else key_name(c->keys[a], t, 64);
        SetWindowTextW(g_kbtn[a], t);
    }
}

static int pad_buttons(uint8_t *b)   /* the open gamepad's 32 buttons now; 0 = not readable */
{
    DIJOYSTATE js;
    if (g_kdev == NULL) return 0;
    IDirectInputDevice7_Poll(g_kdev);
    if (FAILED(IDirectInputDevice7_GetDeviceState(g_kdev, sizeof js, &js))) {
        IDirectInputDevice7_Acquire(g_kdev);
        return 0;
    }
    for (int i = 0; i < 32; i++) b[i] = (js.rgbButtons[i] & 0x80) != 0;
    return 1;
}

static void keys_title(void)
{
    wchar_t t[128];
    if (g_kcap >= 0) wcsncpy(t, TR(g_pl[g_kp].dev == DEV_PAD ? S_PRESS_BTN : S_PRESS_KEY), 127);
    else swprintf(t, 128, TR(S_KEYS_TITLE), g_kp + 1);
    SetWindowTextW(g_kwin, t);
}

static void capture_end(void)
{
    KillTimer(g_kwin, TIMER_CAPTURE);
    g_kcap = -1;
    keys_refresh();
    keys_title();
}

static void capture_poll(void)
{
    player_cfg *c = &g_pl[g_kp];
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { capture_end(); return; }
    if (c->dev == DEV_PAD) {
        uint8_t b[32];
        if (!pad_buttons(b)) return;
        for (int i = 0; i < 32; i++) {
            if (b[i] && !g_kdown[i]) {
                for (int a = 0; a < NACT; a++) if (a != g_kcap && c->buttons[a] == i && a >= 4) c->buttons[a] = c->buttons[g_kcap];
                c->buttons[g_kcap] = (uint8_t)i;
                capture_end();
                settings_save();
                return;
            }
            g_kdown[i] = b[i];
        }
        return;
    }
    for (int vk = 8; vk < 256; vk++) {
        if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_ESCAPE) continue;
        int down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (down && !g_kdown[vk]) {
            UINT sc = MapVirtualKeyW((UINT)vk, 4 /* MAPVK_VK_TO_VSC_EX */);
            if ((sc & 0xff) == 0) continue;
            uint8_t dik = (uint8_t)((sc & 0x7f) | ((sc & 0xff00) == 0xe000 ? 0x80 : 0));
            for (int a = 0; a < NACT; a++) if (a != g_kcap && c->keys[a] == dik) c->keys[a] = c->keys[g_kcap];   /* swap */
            c->keys[g_kcap] = dik;
            capture_end();
            settings_save();
            return;
        }
        g_kdown[vk] = (uint8_t)down;
    }
}

static void capture_start(int a)
{
    g_kcap = a;
    memset(g_kdown, 0, sizeof g_kdown);
    if (g_pl[g_kp].dev == DEV_PAD) pad_buttons(g_kdown);
    else for (int vk = 8; vk < 256; vk++) g_kdown[vk] = (GetAsyncKeyState(vk) & 0x8000) != 0;
    keys_refresh();
    keys_title();
    SetFocus(g_kwin);   /* a key must not press the focused button */
    SetTimer(g_kwin, TIMER_CAPTURE, 30, NULL);
}

static LRESULT CALLBACK keys_proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= 100 && id < 100 + NACT) capture_start(id - 100);
        else if (id == 200) {   /* defaults */
            memcpy(g_pl[g_kp].keys, DEF_KEYS[g_kp], NACT);
            memcpy(g_pl[g_kp].buttons, DEF_BUTTONS, NACT);
            settings_save();
            keys_refresh();
        } else if (id == 201) DestroyWindow(w);
        return 0;
    }
    case WM_TIMER:
        if (wp == TIMER_CAPTURE) capture_poll();
        return 0;
    case WM_CLOSE:
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        KillTimer(w, TIMER_CAPTURE);
        if (g_kdev != NULL) {
            IDirectInputDevice7_Unacquire(g_kdev);
            IDirectInputDevice7_Release(g_kdev);
            g_kdev = NULL;
        }
        EnableWindow(g_wnd, TRUE);
        SetForegroundWindow(g_wnd);
        g_kwin = NULL;
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

static HWND kmk(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h), g_kwin,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}

static void show_keys(int p)
{
    if (g_kwin != NULL) return;
    player_cfg *c = &g_pl[p];
    if (c->dev != DEV_KB && c->dev != DEV_PAD) return;
    g_kp = p;
    g_kcap = -1;
    memset(g_kbtn, 0, sizeof g_kbtn);
    if (c->dev == DEV_PAD && g_di != NULL) {   /* the gamepad, read while the window is open */
        GUID g;
        if (SUCCEEDED(CLSIDFromString(c->guid, &g)) &&
            SUCCEEDED(IDirectInput7_CreateDeviceEx(g_di, &g, &IID_IDirectInputDevice7W, (LPVOID *)&g_kdev, NULL))) {
            IDirectInputDevice7_SetDataFormat(g_kdev, &c_dfDIJoystick);
            IDirectInputDevice7_SetCooperativeLevel(g_kdev, g_wnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
            IDirectInputDevice7_Acquire(g_kdev);
        }
    }
    int first = c->dev == DEV_PAD ? 4 : 0, rows = NACT - first;
    int h = 16 + (c->dev == DEV_PAD ? 26 : 0) + rows * 32 + 56;
    RECT r = { 0, 0, S(420), S(h) };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&r, style, FALSE);
    wchar_t title[64];
    swprintf(title, 64, TR(S_KEYS_TITLE), p + 1);
    RECT mr;
    GetWindowRect(g_wnd, &mr);
    g_kwin = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CoopSummonersKeys", title, style, mr.left + S(60), mr.top + S(80),
                             r.right - r.left, r.bottom - r.top, g_wnd, NULL, GetModuleHandleW(NULL), NULL);
    int y = 14;
    if (c->dev == DEV_PAD) {
        kmk(L"STATIC", TR(S_PAD_DIRS), 0, 16, y, 388, 20, 0);
        y += 26;
    }
    for (int a = first; a < NACT; a++) {
        kmk(L"STATIC", TR(S_ACT_0 + a), 0, 16, y + 5, 210, 20, 0);
        g_kbtn[a] = kmk(L"BUTTON", L"", BS_PUSHBUTTON, 232, y, 172, 26, 100 + a);
        y += 32;
    }
    y += 8;
    kmk(L"STATIC", L"", 0, 16, y - 4, 388, 1, 0);
    kmk(L"BUTTON", TR(S_DEFAULTS), BS_PUSHBUTTON, 16, y + 8, 120, 28, 200);
    kmk(L"BUTTON", TR(S_OK_BTN), BS_DEFPUSHBUTTON, 284, y + 8, 120, 28, 201);
    keys_refresh();
    EnableWindow(g_wnd, FALSE);
    ShowWindow(g_kwin, SW_SHOW);
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
static int shell_op(UINT op, const wchar_t *from, const wchar_t *to, int undo)
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
    s.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_SILENT | (undo ? FOF_ALLOWUNDO : 0);
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
    if (!shell_op(FO_COPY, user, made, 0)) { made[0] = 0; return 0; }
    return 1;
}

/* every play first copies user\ to user-backups\<date-time>\ (unless it equals the newest copy: quitting and
 * restarting must not rotate the real backups away); the newest AUTO_BACKUPS stay, older ones are deleted */
#define AUTO_BACKUPS 5

static int dirs_equal(const wchar_t *a, const wchar_t *b)
{
    wchar_t pat[MAX_PATH], pa[MAX_PATH], pb[MAX_PATH];
    WIN32_FIND_DATAW fd;
    int na = 0, nb = 0, eq = 1;
    swprintf(pat, MAX_PATH, L"%ls\\*", a);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        na++;
        join(pa, a, fd.cFileName);
        join(pb, b, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) eq = is_dir(pb) && dirs_equal(pa, pb);
        else eq = exists(pb) && !is_dir(pb) && same_file(pa, pb);
    } while (eq && FindNextFileW(h, &fd));
    FindClose(h);
    if (!eq) return 0;
    swprintf(pat, MAX_PATH, L"%ls\\*", b);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) nb++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return na == nb;
}

static int name_cmp(const void *a, const void *b) { return wcscmp((const wchar_t *)a, (const wchar_t *)b); }

static int auto_backup(void)
{
    wchar_t user[MAX_PATH], root[MAX_PATH], pat[MAX_PATH], p[MAX_PATH];
    join(user, g_game_dir, L"user");
    if (!is_dir(user)) return 1;   /* nothing saved yet */
    join(root, g_game_dir, L"user-backups");
    CreateDirectoryW(root, NULL);
    static wchar_t names[64][32];
    int n = 0;
    WIN32_FIND_DATAW fd;
    swprintf(pat, MAX_PATH, L"%ls\\2*", root);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcslen(fd.cFileName) < 32 && n < 64)
                wcscpy(names[n++], fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    qsort(names, (size_t)n, sizeof names[0], name_cmp);
    if (n > 0) {
        join(p, root, names[n - 1]);
        if (dirs_equal(user, p)) return 1;   /* unchanged since the newest copy */
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t name[32];
    swprintf(name, 32, L"%04u%02u%02u-%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    join(p, root, name);
    if (!shell_op(FO_COPY, user, p, 0)) return 0;
    for (int i = 0; i + AUTO_BACKUPS < n + 1; i++) {   /* n + 1 copies now: drop the oldest */
        join(p, root, names[i]);
        shell_op(FO_DELETE, p, NULL, 0);
    }
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
    for (int k = 0; k < N_LANG_FILES; k++) {   /* the translation files, when they ship */
        wchar_t li[MAX_PATH], lb[MAX_PATH], ld[MAX_PATH];
        lang_installed_k(li, k);
        lang_bundled_k(lb, k);
        if (exists(lb) && _wcsicmp(li, lb) != 0) {
            join(ld, g_game_dir, L"lang");
            CreateDirectoryW(ld, NULL);
            if (!CopyFileW(lb, li, FALSE)) {
                err(S_ERR_COPY, GetLastError());
                refresh();
                return 0;
            }
        }
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
    join(p, g_game_dir, L"coop-controls.ini");
    DeleteFileW(p);
    join(p, g_game_dir, L"coop.log");
    DeleteFileW(p);
    join(p, g_game_dir, L"lang.log");
    DeleteFileW(p);
    wchar_t li[MAX_PATH], lb[MAX_PATH];
    lang_installed(li);
    lang_bundled(lb);
    if (_wcsicmp(li, lb) != 0) {
        for (int k = 0; k < N_LANG_FILES; k++) {
            lang_installed_k(li, k);
            DeleteFileW(li);
        }
        join(p, g_game_dir, L"lang");
        RemoveDirectoryW(p);   /* only when empty */
    }
    join(p, g_game_dir, L"opensummoners-standalone");   /* the mod exe's log + audit folder */
    if (is_dir(p)) shell_op(FO_DELETE, p, NULL, 1);
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
    if (same_pad_twice() && !ask(TR(S_ASK_SAME_PAD))) return;
    if (!auto_backup() && !ask(TR(S_ASK_NO_BACKUP))) return;
    write_controls();   /* the players' devices and keys, for the mod */
    wchar_t exe[MAX_PATH], cmd[MAX_PATH + 192], ws[80] = L"";
    installed_path(exe);
    int pt = g_lang_pt > 0 && lang_available();
    static const wchar_t *const SCREEN_ARGS[4] = {
        L" --no-launcher --screen 3 --borderless", L" --no-launcher --screen 3", L" --no-launcher --screen 4", L"" };
    wcscpy(ws, SCREEN_ARGS[g_screen]);
    if ((g_screen == SCREEN_WINDOW || g_screen == SCREEN_ASK) && g_wscale > 0)
        swprintf(ws + wcslen(ws), 80 - wcslen(ws), L" --window-scale %ls", WSCALE_INI[g_wscale]);
    swprintf(cmd, MAX_PATH + 192, L"\"%ls\"%ls%ls%ls%ls", exe, coop ? L" --coop --coop-wait" : L"", pt ? L" --lang pt-br" : L"",
             pt && g_lang_pt == 2 ? L" --lang-names original" : L"", ws);
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
    RECT r = { 0, 0, S(720), S(600) };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    h = CreateWindowExW(0, L"CoopSummonersHelp", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                        CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, g_wnd, NULL, GetModuleHandleW(NULL), NULL);
    HWND e = CreateWindowExW(0, L"EDIT", TR(S_CONTROLS_TEXT),
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0,
                             S(720), S(600), h, (HMENU)1, GetModuleHandleW(NULL), NULL);
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
    mk(L"STATIC", TR(S_GAMELANG), SS_RIGHT, X + 296, y + 5, 100, 20, ID_LANG_L, g_font);
    g_lang = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, X + 402, y + 1, 146, 200, ID_LANG, g_font);
    SendMessageW(g_lang, CB_ADDSTRING, 0, (LPARAM)TR(S_LANG_EN));
    SendMessageW(g_lang, CB_ADDSTRING, 0, (LPARAM)TR(S_LANG_PT));
    SendMessageW(g_lang, CB_ADDSTRING, 0, (LPARAM)TR(S_LANG_PT_NAMES));
    y += 44;
    g_solo = mk(L"BUTTON", TR(S_SOLO), BS_PUSHBUTTON, X, y, (W - 12) / 2, 52, ID_SOLO, g_font_big);
    g_coop = mk(L"BUTTON", TR(S_COOP), BS_DEFPUSHBUTTON, X + (W - 12) / 2 + 12, y, (W - 12) / 2, 52, ID_COOP, g_font_big);
    y += 68;
    mk(L"STATIC", TR(S_PADS), 0, X, y, W, 20, ID_PADS_L, g_font);
    y += 24;
    for (int p = 0; p < NPLAYERS; p++) {   /* each player's device (+ keys for players 2 / 3) */
        wchar_t lab[32];
        swprintf(lab, 32, TR(S_PLAYER), p + 1);
        mk(L"STATIC", lab, 0, X, y + 5, 90, 20, 0, g_font);
        g_dev_c[p] = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, X + 96, y + 1, p ? 330 : 452, 300,
                        ID_DEV0 + p, g_font);
        SendMessageW(g_dev_c[p], CB_SETDROPPEDWIDTH, S(452), 0);
        if (p > 0) g_keys_b[p] = mk(L"BUTTON", TR(S_KEYS_BTN), BS_PUSHBUTTON, X + 432, y, 116, 27, ID_KEYS0 + p, g_font);
        fill_dev_combo(p);
        y += 31;
    }
    mk(L"STATIC", TR(S_PADS_NOTE), 0, X, y, W, 36, ID_PADS_NOTE, g_font);
    y += 42;
    mk(L"BUTTON", TR(S_CONTROLS), BS_PUSHBUTTON, X, y, 130, 28, ID_CONTROLS, g_font);
    mk(L"STATIC", TR(S_SCREEN), SS_RIGHT, X + 136, y + 5, 116, 20, ID_SCREEN_L, g_font);
    g_screen_c = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, X + 258, y + 1, 290, 200, ID_SCREEN, g_font);
    for (int i = S_SM_BORDERLESS; i <= S_SM_ASK; i++) SendMessageW(g_screen_c, CB_ADDSTRING, 0, (LPARAM)TR(i));
    SendMessageW(g_screen_c, CB_SETCURSEL, g_screen, 0);
    y += 34;
    mk(L"STATIC", TR(S_WSCALE), SS_RIGHT, X + 96, y + 5, 156, 20, ID_WSCALE_L, g_font);
    g_wscale_c = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, X + 258, y + 1, 290, 200, ID_WSCALE, g_font);
    {   /* the largest scale whose game window (retail's windowed style) fits the main screen's work area: what
         * "Largest that fits" picks, and the sizes that won't fit say so (the game then uses the largest fit) */
        RECT wa;
        if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) SetRect(&wa, 0, 0, 1280, 1000);
        int fit = 1;
        for (int k = 2; k <= 8; k++) {
            RECT r = { 0, 0, 640 * k, 480 * k };
            AdjustWindowRectEx(&r, 0x80CA0000u, FALSE, 0);
            if (r.right - r.left > wa.right - wa.left || r.bottom - r.top > wa.bottom - wa.top) break;
            fit = k;
        }
        for (int i = S_WS_1; i <= S_WS_3; i++) {
            wchar_t item[128];
            int k = i == S_WS_2 ? 2 : i == S_WS_3 ? 3 : 0;
            if (i == S_WS_AUTO) swprintf(item, 128, L"%ls (%d×)", TR(i), fit);
            else if (k > fit) swprintf(item, 128, L"%ls — %ls", TR(i), TR(S_WS_NOFIT));
            else swprintf(item, 128, L"%ls", TR(i));
            SendMessageW(g_wscale_c, CB_ADDSTRING, 0, (LPARAM)item);
        }
        SendMessageW(g_wscale_c, CB_SETDROPPEDWIDTH, S(300), 0);
    }
    SendMessageW(g_wscale_c, CB_SETCURSEL, g_wscale, 0);
    EnableWindow(g_wscale_c, g_screen == SCREEN_WINDOW || g_screen == SCREEN_ASK);   /* only a window has a size */
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
        case ID_LANG:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                g_lang_pt = (int)SendMessageW(g_lang, CB_GETCURSEL, 0, 0);
                settings_save();
            }
            break;
        case ID_DEV0: case ID_DEV0 + 1: case ID_DEV0 + 2:
            if (HIWORD(wp) == CBN_SELCHANGE) dev_changed(LOWORD(wp) - ID_DEV0);
            break;
        case ID_KEYS0 + 1: case ID_KEYS0 + 2:
            show_keys(LOWORD(wp) - ID_KEYS0);
            break;
        case ID_SCREEN:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                g_screen = (int)SendMessageW(g_screen_c, CB_GETCURSEL, 0, 0);
                if (g_screen < 0 || g_screen > 3) g_screen = SCREEN_BORDERLESS;
                EnableWindow(g_wscale_c, g_screen == SCREEN_WINDOW || g_screen == SCREEN_ASK);
                settings_save();
            }
            break;
        case ID_WSCALE:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                g_wscale = (int)SendMessageW(g_wscale_c, CB_GETCURSEL, 0, 0);
                if (g_wscale < 0 || g_wscale > 3) g_wscale = 1;
                settings_save();
            }
            break;
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
    wc.lpfnWndProc = keys_proc;
    wc.lpszClassName = L"CoopSummonersKeys";
    RegisterClassW(&wc);
    wc.lpfnWndProc = help_proc;
    wc.lpszClassName = L"CoopSummonersHelp";
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    RegisterClassW(&wc);

    RECT r = { 0, 0, S(580), S(592) };
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
