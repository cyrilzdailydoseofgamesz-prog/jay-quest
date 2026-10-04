/* JAYJAY'S QUEST: The Sun Shard  -  PSP homebrew (PSPSDK, software rendered)
 * Controls: D-pad / analog = move, CROSS or SQUARE = sword, START = begin/skip
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspge.h>
#include <stdint.h>
#include <stdlib.h>

PSP_MODULE_INFO("JAYJAYQUEST", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define BW 512          /* framebuffer stride in pixels */
#define T 16
#define MW 30
#define MH 17
#define RGB(r,g,b) (0xFF000000u | ((uint32_t)(b)<<16) | ((uint32_t)(g)<<8) | (uint32_t)(r))

enum { TITLE, PLAY, OVER, CREDITS };

static void *dispbuf[2];
static uint32_t *drawbuf[2], *back;
static int cur = 0, frame = 0, state = TITLE, ct = 0;

/* ---------- exit callback (HOME button) ---------- */
static int exit_cb(int a, int b, void *c) { sceKernelExitGame(); return 0; }
static int cb_thread(SceSize args, void *argp) {
    int id = sceKernelCreateCallback("exit_cb", exit_cb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int t = sceKernelCreateThread("cb", cb_thread, 0x11, 0xFA0, 0, NULL);
    if (t >= 0) sceKernelStartThread(t, 0, NULL);
}

/* ---------- drawing ---------- */
static void rect(int x, int y, int w, int h, uint32_t c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SW) w = SW - x;
    if (y + h > SH) h = SH - y;
    if (w <= 0 || h <= 0) return;
    uint32_t *p = back + y * BW + x;
    for (int j = 0; j < h; j++, p += BW)
        for (int i = 0; i < w; i++) p[i] = c;
}
static void disc(int cx, int cy, int r, uint32_t c) {
    for (int j = -r; j <= r; j++) {
        int w = 0;
        while ((w + 1) * (w + 1) + j * j <= r * r) w++;
        if (j * j <= r * r) rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}
static const unsigned char FONT[26][7] = {
{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
{0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
{0x07,0x02,0x02,0x02,0x02,0x12,0x0C},{0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
{0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},{0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
{0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},{0x11,0x11,0x11,0x15,0x15,0x1B,0x11},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
{0x11,0x11,0x0A,0x04,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}};
static void text(int x, int y, const char *s, int sc, uint32_t c) {
    for (; *s; s++, x += 6 * sc) {
        if (*s < 'A' || *s > 'Z') continue;
        const unsigned char *g = FONT[*s - 'A'];
        for (int r = 0; r < 7; r++)
            for (int b = 0; b < 5; b++)
                if (g[r] & (0x10 >> b)) rect(x + b * sc, y + r * sc, sc, sc, c);
    }
}
static int slen(const char *s) { int n = 0; while (*s++) n++; return n; }
static void ctext(int y, const char *s, int sc, uint32_t c) { text((SW - slen(s) * 6 * sc) / 2, y, s, sc, c); }

static const char *HEART[6] = {".xx.xx.","xxxxxxx","xxxxxxx",".xxxxx.","..xxx..","...x..."};
static void heart(int x, int y, uint32_t c) {
    for (int j = 0; j < 6; j++)
        for (int i = 0; i < 7; i++)
            if (HEART[j][i] == 'x') rect(x + i * 2, y + j * 2, 2, 2, c);
}

/* ---------- world ---------- */
static unsigned char map[MH][MW];
static void init_map(void) {
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            map[y][x] = (x == 0 || y == 0 || x == MW - 1 || y == MH - 1);
    for (int y = 1; y < MH - 1; y++) if (y != 7 && y != 8) map[y][14] = 1;
    static const int tr[][2] = {{4,3},{5,3},{4,4},{9,8},{10,8},{19,3},{20,3},{20,4},{8,12},{9,12},{8,13},
        {18,10},{19,10},{25,6},{25,7},{22,13},{23,13},{24,13},{5,13}};
    for (unsigned i = 0; i < sizeof(tr) / sizeof(tr[0]); i++) map[tr[i][1]][tr[i][0]] = 1;
}
static int solid(int x, int y) {
    int tx = x / T, ty = y / T;
    if (x < 0 || y < 0 || tx >= MW || ty >= MH) return 1;
    return map[ty][tx];
}
static int blocked(int x, int y, int w, int h) {
    return solid(x, y) || solid(x + w - 1, y) || solid(x, y + h - 1) || solid(x + w - 1, y + h - 1);
}
static int ov(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

typedef struct { int x, y, w, h, hp, dx, dy, t, stun, boss; } E;
static E en[4];
static int hx, hy, hp, inv, atk, face, bdead;
#define SHX 440
#define SHY 200

static void new_game(void) {
    hx = 32; hy = 48; hp = 5; inv = 0; atk = 0; face = 2; bdead = 0;
    static const int sp[3][2] = {{100,60},{300,120},{380,70}};
    for (int i = 0; i < 3; i++) en[i] = (E){sp[i][0], sp[i][1], 14, 14, 2, 0, 0, 0, 0, 0};
    en[3] = (E){360, 170, 28, 28, 10, 0, 0, 0, 0, 1};
    state = PLAY;
}
static void sword_rect(int *x, int *y, int *w, int *h) {
    switch (face) {
    case 0: *x = hx + 14; *y = hy + 4;  *w = 16; *h = 8;  break;
    case 1: *x = hx - 14; *y = hy + 4;  *w = 16; *h = 8;  break;
    case 2: *x = hx + 4;  *y = hy + 14; *w = 8;  *h = 16; break;
    default:*x = hx + 4;  *y = hy - 14; *w = 8;  *h = 16; break;
    }
}

static void update_play(const SceCtrlData *p, unsigned down) {
    int dx = 0, dy = 0;
    if ((p->Buttons & PSP_CTRL_LEFT)  || p->Lx < 64)  dx = -1;
    if ((p->Buttons & PSP_CTRL_RIGHT) || p->Lx > 192) dx = 1;
    if ((p->Buttons & PSP_CTRL_UP)    || p->Ly < 64)  dy = -1;
    if ((p->Buttons & PSP_CTRL_DOWN)  || p->Ly > 192) dy = 1;
    if (dx) face = dx > 0 ? 0 : 1; else if (dy) face = dy > 0 ? 2 : 3;
    if (atk > 0) atk--; else if (down & (PSP_CTRL_CROSS | PSP_CTRL_SQUARE)) atk = 12;
    if (inv > 0) inv--;
    if (dx && !blocked(hx + dx * 2 + 3, hy + 6, 10, 9)) hx += dx * 2;
    if (dy && !blocked(hx + 3, hy + dy * 2 + 6, 10, 9)) hy += dy * 2;

    int sx, sy, sw, sh; sword_rect(&sx, &sy, &sw, &sh);
    for (int i = 0; i < 4; i++) {
        E *e = &en[i];
        if (e->hp <= 0) continue;
        if (e->stun) { e->stun--; continue; }
        if (e->boss) {
            int s = e->hp <= 4 ? 2 : 1;
            int ddx = hx > e->x ? 1 : hx < e->x ? -1 : 0;
            int ddy = hy > e->y ? 1 : hy < e->y ? -1 : 0;
            for (int k = 0; k < s; k++) {
                if (ddx && !blocked(e->x + ddx, e->y, e->w, e->h)) e->x += ddx;
                if (ddy && !blocked(e->x, e->y + ddy, e->w, e->h)) e->y += ddy;
            }
        } else {
            if (--e->t <= 0) {
                int d = rand() % 5;
                e->dx = d == 0 ? 1 : d == 1 ? -1 : 0;
                e->dy = d == 2 ? 1 : d == 3 ? -1 : 0;
                e->t = 30 + rand() % 60;
            }
            if (frame & 1) {
                int nx = e->x + e->dx, ny = e->y + e->dy;
                if (blocked(nx, ny, e->w, e->h)) e->t = 0; else { e->x = nx; e->y = ny; }
            }
        }
        if (atk > 0 && ov(sx, sy, sw, sh, e->x, e->y, e->w, e->h)) {
            e->hp--; e->stun = 20;
            if (e->hp <= 0) { if (e->boss) bdead = 1; continue; }
        }
        if (!inv && ov(hx + 3, hy + 3, 10, 12, e->x + 2, e->y + 2, e->w - 4, e->h - 4)) { hp--; inv = 60; }
    }
    if (hp <= 0) state = OVER;
    if (bdead && ov(hx, hy, 16, 16, SHX, SHY, 16, 16)) { state = CREDITS; ct = 0; }
}

/* ---------- sprites ---------- */
static void draw_hero(void) {
    if (inv && (inv >> 2) & 1) return;
    int x = hx, y = hy;
    rect(x + 3, y + 14, 10, 2, RGB(30,90,30));
    rect(x + 4, y + 12, 3, 3, RGB(74,48,32)); rect(x + 9, y + 12, 3, 3, RGB(74,48,32));
    rect(x + 3, y + 7, 10, 6, RGB(47,191,154));
    rect(x + 3, y + 2, 10, 6, RGB(242,199,155));
    rect(x + 2, y, 12, 3, RGB(214,58,58));
    if (face == 2) { rect(x + 5, y + 5, 2, 2, RGB(30,30,30)); rect(x + 9, y + 5, 2, 2, RGB(30,30,30)); }
    else if (face == 1) rect(x + 4, y + 5, 2, 2, RGB(30,30,30));
    else if (face == 0) rect(x + 10, y + 5, 2, 2, RGB(30,30,30));
}
static void draw_sword(void) {
    if (atk <= 0) return;
    int x, y, w, h; sword_rect(&x, &y, &w, &h);
    rect(x, y, w, h, RGB(232,232,240));
    if (face < 2) rect(face == 0 ? x : x + w - 2, y - 2, 2, h + 4, RGB(201,162,39));
    else rect(x - 2, face == 2 ? y : y + h - 2, w + 4, 2, RGB(201,162,39));
}
static void draw_slime(const E *e) {
    int x = e->x - 1, y = e->y - 2; uint32_t c = (e->stun && (e->stun >> 1) & 1) ? RGB(255,255,255) : RGB(123,63,184);
    rect(x + 3, y + 3, 10, 4, c); rect(x + 1, y + 6, 14, 8, c);
    rect(x + 1, y + 12, 14, 2, RGB(80,30,130));
    rect(x + 4, y + 7, 3, 3, RGB(255,255,255)); rect(x + 9, y + 7, 3, 3, RGB(255,255,255));
    rect(x + 5, y + 8, 1, 2, RGB(0,0,0)); rect(x + 10, y + 8, 1, 2, RGB(0,0,0));
}
static void draw_boss(const E *e) {
    int x = e->x - 2, y = e->y - 2; int fl = e->stun && (e->stun >> 1) & 1;
    uint32_t red = fl ? RGB(255,255,255) : RGB(190,40,60), dk = fl ? RGB(255,255,255) : RGB(110,15,40);
    rect(x + 8, y + 26, 4, 6, dk); rect(x + 14, y + 27, 4, 5, dk); rect(x + 22, y + 26, 4, 6, dk);
    disc(x + 16, y + 18, 11, red); rect(x + 6, y + 22, 21, 4, dk);
    disc(x + 4, y + 13, 5, red); disc(x + 28, y + 13, 5, red);
    rect(x + 3, y + 8, 2, 4, RGB(0,0,0)); rect(x + 27, y + 8, 2, 4, RGB(0,0,0));
    rect(x + 8, y + 3, 3, 7, RGB(255,224,102)); rect(x + 21, y + 3, 3, 7, RGB(255,224,102));
    disc(x + 11, y + 14, 3, RGB(255,224,102)); disc(x + 21, y + 14, 3, RGB(255,224,102));
    rect(x + 11, y + 14, 2, 3, RGB(0,0,0)); rect(x + 20, y + 14, 2, 3, RGB(0,0,0));
    rect(x + 11, y + 21, 10, 2, RGB(0,0,0));
    for (int i = 11; i < 21; i += 2) rect(x + i, y + 21, 1, 1, RGB(255,255,255));
}
static void draw_shard(int x, int y) {
    int b = (frame / 10) & 1;
    for (int j = -8; j <= 8; j++) {
        int w = 8 - (j < 0 ? -j : j);
        rect(x + 8 - w, y + 8 + j + b, 2 * w + 1, 1, RGB(255,224,102));
        if (w > 3) rect(x + 8 - w / 2, y + 8 + j + b, w, 1, RGB(255,255,230));
    }
}
static void draw_world(void) {
    for (int ty = 0; ty < MH; ty++)
        for (int tx = 0; tx < MW; tx++) {
            int x = tx * T, y = ty * T;
            rect(x, y, T, T, ((tx + ty) & 1) ? RGB(88,169,68) : RGB(79,154,60));
            if ((tx * 7 + ty * 13) % 5 == 0) { rect(x + 4, y + 5, 2, 2, RGB(115,196,92)); rect(x + 10, y + 11, 2, 2, RGB(115,196,92)); }
            if (map[ty][tx]) {
                rect(x + 6, y + 10, 4, 6, RGB(107,68,35));
                rect(x + 1, y + 1, 14, 11, RGB(31,90,43)); rect(x + 3, y + 2, 10, 7, RGB(47,125,58)); rect(x + 4, y + 3, 3, 2, RGB(74,163,82));
            }
        }
    if (bdead) draw_shard(SHX, SHY);
    for (int i = 0; i < 3; i++) if (en[i].hp > 0) draw_slime(&en[i]);
    if (en[3].hp > 0) draw_boss(&en[3]);
    draw_hero(); draw_sword();
    for (int i = 0; i < 5; i++) heart(8 + i * 18, 3, i < hp ? RGB(255,68,102) : RGB(74,36,56));
    if (en[3].hp > 0) { rect(180, 4, 120, 8, RGB(0,0,0)); rect(182, 6, 116 * en[3].hp / 10, 4, RGB(255,68,102)); }
}

/* ---------- screens ---------- */
static const char *CR[] = {"THE SUN SHARD","","A GAME BY","JAYJAY","","GAME DESIGN","JAYJAY","","PROGRAMMING","JAYJAY",
    "","PIXEL ART","JAYJAY","","SOUND","JAYJAY","","THANKS FOR PLAYING","","THE END"};
#define NCR ((int)(sizeof(CR) / sizeof(CR[0])))
static void sky(void) {
    rect(0, 0, SW, SH, RGB(27,20,64));
    for (int i = 0; i < 60; i++) {
        int x = (i * 97) % SW, y = (i * 53) % SH;
        rect(x, y, 1 + (i % 3 > 1), 1 + (i % 3 > 1), ((frame + x) % 60 < 30) ? RGB(255,255,255) : RGB(154,143,216));
    }
}

int main(void) {
    setup_callbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    uint8_t *vb = (uint8_t *)sceGeEdramGetAddr();
    sceDisplaySetMode(0, SW, SH);
    for (int i = 0; i < 2; i++) {
        dispbuf[i] = vb + i * BW * SH * 4;
        drawbuf[i] = (uint32_t *)((uintptr_t)vb + 0x40000000u + (uintptr_t)i * BW * SH * 4);
    }
    sceDisplaySetFrameBuf(dispbuf[0], BW, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
    init_map();
    srand(sceKernelGetSystemTimeLow());

    SceCtrlData pad; unsigned prev = 0;
    for (;;) {
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned down = pad.Buttons & ~prev; prev = pad.Buttons;
        frame++;
        back = drawbuf[cur];

        if (state == TITLE) {
            if (down & PSP_CTRL_START) new_game();
            sky(); draw_shard(232, 24);
            ctext(70, "JAYJAY QUEST", 5, RGB(244,196,48));
            ctext(120, "THE SUN SHARD", 3, RGB(154,143,216));
            ctext(160, "SLAY THE GUARDIAN", 2, RGB(255,255,255));
            ctext(180, "CROSS OR SQUARE SWORD", 2, RGB(255,255,255));
            if ((frame / 30) & 1) ctext(212, "PRESS START", 3, RGB(244,196,48));
            ctext(250, "A GAME BY JAYJAY", 2, RGB(109,99,168));
        } else if (state == PLAY) {
            update_play(&pad, down);
            draw_world();
        } else if (state == OVER) {
            if (down & PSP_CTRL_START) state = TITLE;
            draw_world();
            for (int y = 0; y < SH; y += 2) rect(0, y, SW, 1, RGB(0,0,0));
            ctext(100, "GAME OVER", 6, RGB(255,68,102));
            if ((frame / 30) & 1) ctext(170, "PRESS START", 3, RGB(255,255,255));
        } else {
            ct++;
            if (down & PSP_CTRL_START) state = TITLE;
            sky();
            int y0 = SH - ct / 2, lim = 126 - (NCR - 1) * 34;
            if (y0 < lim) y0 = lim;
            for (int i = 0; i < NCR; i++) {
                int y = y0 + i * 34;
                if (y < -30 || y > SH) continue;
                if (CR[i][0] == 'J' && CR[i][1] == 'A') ctext(y, CR[i], 4, RGB(255,255,255));
                else ctext(y, CR[i], 2, i == NCR - 1 ? RGB(255,224,102) : RGB(154,143,216));
            }
        }

        sceDisplayWaitVblankStart();
        sceDisplaySetFrameBuf(dispbuf[cur], BW, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
        cur ^= 1;
    }
    return 0;
}
