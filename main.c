/* JAYJAY'S QUEST: SUN SHARD RUNNER  -  PSP homebrew, hardware GU 3D
 *
 * Race down a neon highway into the sunset. Dodge walls, jump barriers,
 * grab Sun Shards. Every 10 shards = SUN BURST (+1 life).
 *
 * Controls:  LEFT / RIGHT (or analog) = change lane
 *            CROSS / SQUARE = jump      START = begin / pause / retry
 *
 * Needs libs:  -lpspgum -lpspgu -lpspdisplay -lpspge -lpspctrl -lm
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

PSP_MODULE_INFO("JAYJAYQUEST", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

#define SW 480
#define SH 272
#define BW 512
#define FRAME_SIZE (BW * SH * 4)
#define ZBUF_SIZE  (BW * SH * 2)

#define RGBA(r,g,b,a) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(g)<<8)|(uint32_t)(r))
#define RGB(r,g,b) RGBA(r,g,b,255)

typedef struct { uint32_t c; float x, y, z; } V;
#define F3D (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
#define F2D (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

static unsigned int __attribute__((aligned(16))) list[262144];

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

/* ---------- small helpers ---------- */
static uint32_t rng = 12345;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static int rndi(int n) { return (int)(rnd() % (uint32_t)n); }
static float rndf(void) { return (rnd() & 0xFFFF) / 65535.0f; }

static uint32_t shade(uint32_t c, float f) {
    int r = (int)((c & 255) * f), g = (int)(((c >> 8) & 255) * f), b = (int)(((c >> 16) & 255) * f);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return RGBA(r, g, b, c >> 24);
}

/* ---------- 3D drawing ---------- */
static const unsigned char FC[5][4][3] = {
    {{0,1,0},{1,1,0},{1,1,1},{0,1,1}},   /* top   */
    {{0,0,1},{1,0,1},{1,1,1},{0,1,1}},   /* front */
    {{1,0,0},{0,0,0},{0,1,0},{1,1,0}},   /* back  */
    {{0,0,0},{0,0,1},{0,1,1},{0,1,0}},   /* left  */
    {{1,0,1},{1,0,0},{1,1,0},{1,1,1}}    /* right */
};
static const float FSHADE[5] = {1.0f, 0.85f, 0.55f, 0.70f, 0.70f};

static void box(float x, float y, float z, float sx, float sy, float sz, uint32_t c) {
    V *v = (V *)sceGuGetMemory(30 * sizeof(V));
    float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;
    for (int f = 0; f < 5; f++) {
        V q[4];
        uint32_t col = shade(c, FSHADE[f]);
        for (int k = 0; k < 4; k++) {
            q[k].c = col;
            q[k].x = FC[f][k][0] ? x + hx : x - hx;
            q[k].y = FC[f][k][1] ? y + hy : y - hy;
            q[k].z = FC[f][k][2] ? z + hz : z - hz;
        }
        v[f * 6 + 0] = q[0]; v[f * 6 + 1] = q[1]; v[f * 6 + 2] = q[2];
        v[f * 6 + 3] = q[0]; v[f * 6 + 4] = q[2]; v[f * 6 + 5] = q[3];
    }
    sceGumDrawArray(GU_TRIANGLES, F3D, 30, 0, v);
}

/* spinning diamond (octahedron) */
static void diamond(float x, float y, float z, float ang, float s, uint32_t c) {
    float ca = cosf(ang), sa = sinf(ang);
    float bx[6] = {0, 0, s * 0.8f, 0, -s * 0.8f, 0};
    float by[6] = {s * 1.5f, -s * 1.5f, 0, 0, 0, 0};
    float bz[6] = {0, 0, 0, s * 0.8f, 0, -s * 0.8f};
    V p[6];
    for (int i = 0; i < 6; i++) {
        p[i].c = c;
        p[i].x = x + bx[i] * ca + bz[i] * sa;
        p[i].y = y + by[i];
        p[i].z = z - bx[i] * sa + bz[i] * ca;
    }
    V *v = (V *)sceGuGetMemory(24 * sizeof(V));
    int n = 0;
    for (int i = 0; i < 4; i++) {
        int a = 2 + i, b = 2 + (i + 1) % 4;
        uint32_t ct = shade(c, 1.1f - 0.18f * i), cb = shade(c, 0.8f - 0.12f * i);
        v[n] = p[0]; v[n].c = ct; n++;
        v[n] = p[a]; v[n].c = ct; n++;
        v[n] = p[b]; v[n].c = ct; n++;
        v[n] = p[1]; v[n].c = cb; n++;
        v[n] = p[b]; v[n].c = cb; n++;
        v[n] = p[a]; v[n].c = cb; n++;
    }
    sceGumDrawArray(GU_TRIANGLES, F3D, 24, 0, v);
}

/* ---------- 2D drawing ---------- */
static void fill_rect(float x, float y, float w, float h, uint32_t c) {
    V *v = (V *)sceGuGetMemory(2 * sizeof(V));
    v[0].c = c; v[0].x = x;     v[0].y = y;     v[0].z = 0;
    v[1].c = c; v[1].x = x + w; v[1].y = y + h; v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, F2D, 2, 0, v);
}
static void grad_rect(float x, float y, float w, float h, uint32_t top, uint32_t bot) {
    V *v = (V *)sceGuGetMemory(6 * sizeof(V));
    float X[6] = {x, x + w, x + w, x, x + w, x};
    float Y[6] = {y, y, y + h, y, y + h, y + h};
    for (int i = 0; i < 6; i++) {
        v[i].c = (Y[i] > y) ? bot : top; v[i].x = X[i]; v[i].y = Y[i]; v[i].z = 0;
    }
    sceGuDrawArray(GU_TRIANGLES, F2D, 6, 0, v);
}
static void disc(float cx, float cy, float r, uint32_t cin, uint32_t cout) {
    const int N = 32;
    V *v = (V *)sceGuGetMemory((N + 2) * sizeof(V));
    v[0].c = cin; v[0].x = cx; v[0].y = cy; v[0].z = 0;
    for (int i = 0; i <= N; i++) {
        float a = i * 6.2831853f / N;
        v[i + 1].c = cout; v[i + 1].x = cx + cosf(a) * r; v[i + 1].y = cy + sinf(a) * r; v[i + 1].z = 0;
    }
    sceGuDrawArray(GU_TRIANGLE_FAN, F2D, N + 2, 0, v);
}

/* 5x7 font: letters, digits, a few symbols */
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
static const unsigned char DIG[10][7] = {
{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}};
static const unsigned char SYM[5][7] = {
{0x00,0x04,0x00,0x00,0x00,0x04,0x00},{0x04,0x04,0x04,0x04,0x04,0x00,0x04},{0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},{0x04,0x04,0x08,0x00,0x00,0x00,0x00}};

static const unsigned char *glyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= 'A' && c <= 'Z') return FONT[c - 'A'];
    if (c >= '0' && c <= '9') return DIG[c - '0'];
    switch (c) {
        case ':': return SYM[0];
        case '!': return SYM[1];
        case '-': return SYM[2];
        case '.': return SYM[3];
        case '\'': return SYM[4];
    }
    return 0;
}
static int slen(const char *s) { int n = 0; while (*s++) n++; return n; }

static void text(int x, int y, int sc, uint32_t col, const char *s) {
    int n = 0;
    for (const char *p = s; *p; p++) {
        const unsigned char *g = glyph(*p);
        if (!g) continue;
        for (int r = 0; r < 7; r++)
            for (int b = 0; b < 5; b++)
                if (g[r] & (0x10 >> b)) n++;
    }
    if (n == 0) return;
    V *v = (V *)sceGuGetMemory(n * 2 * sizeof(V));
    int k = 0;
    for (const char *p = s; *p; p++, x += 6 * sc) {
        const unsigned char *g = glyph(*p);
        if (!g) continue;
        for (int r = 0; r < 7; r++)
            for (int b = 0; b < 5; b++)
                if (g[r] & (0x10 >> b)) {
                    v[k].c = col; v[k].x = x + b * sc;        v[k].y = y + r * sc;        v[k].z = 0; k++;
                    v[k].c = col; v[k].x = x + b * sc + sc;   v[k].y = y + r * sc + sc;   v[k].z = 0; k++;
                }
    }
    sceGuDrawArray(GU_SPRITES, F2D, n * 2, 0, v);
}
static void text_s(int x, int y, int sc, uint32_t col, const char *s) {
    text(x + sc, y + sc, sc, RGBA(0, 0, 0, 190), s);
    text(x, y, sc, col, s);
}
static void ctext(int y, int sc, uint32_t col, const char *s) {
    text_s((SW - (slen(s) * 6 - 1) * sc) / 2, y, sc, col, s);
}

static const char *HEART[6] = {".xx.xx.", "xxxxxxx", "xxxxxxx", ".xxxxx.", "..xxx..", "...x..."};
static void heart(int x, int y, int sc, uint32_t col) {
    int n = 0;
    for (int j = 0; j < 6; j++) for (int i = 0; i < 7; i++) if (HEART[j][i] == 'x') n++;
    V *v = (V *)sceGuGetMemory(n * 2 * sizeof(V));
    int k = 0;
    for (int j = 0; j < 6; j++)
        for (int i = 0; i < 7; i++)
            if (HEART[j][i] == 'x') {
                v[k].c = col; v[k].x = x + i * sc;        v[k].y = y + j * sc;        v[k].z = 0; k++;
                v[k].c = col; v[k].x = x + i * sc + sc;   v[k].y = y + j * sc + sc;   v[k].z = 0; k++;
            }
    sceGuDrawArray(GU_SPRITES, F2D, n * 2, 0, v);
}

/* ---------- game state ---------- */
enum { TITLE, PLAY, OVER };
enum { K_BARRIER, K_WALL, K_SHARD };

typedef struct { float x, z; int kind, on; } Obj;
typedef struct { float x, y, z, vx, vy, vz, life, max; uint32_t col; } Part;

#define MAXOBJ 48
#define MAXPART 96
static Obj ob[MAXOBJ];
static Part ps[MAXPART];
static int pidx = 0;

static const float LANE_X[3] = {-4.5f, 0.0f, 4.5f};

static int state = TITLE, paused = 0;
static int lane = 1, lives = 3, shards = 0, best = 0;
static float px = 0, py = 0, vy = 0, speed = 26, dist = 0, score = 0;
static float inv = 0, shake = 0, tm = 0, spawn_t = 30, overt = 0, msg_t = 0;
static const char *msg = "";

/* input (edge triggered, filled in main loop) */
static int in_left, in_right, in_jump, in_start;

static void emit(float x, float y, float z, float vx, float vy, float vz, float life, uint32_t col) {
    Part *p = &ps[pidx]; pidx = (pidx + 1) % MAXPART;
    p->x = x; p->y = y; p->z = z; p->vx = vx; p->vy = vy; p->vz = vz;
    p->life = p->max = life; p->col = col;
}
static void burst(float x, float y, float z, int n, uint32_t c1, uint32_t c2) {
    for (int i = 0; i < n; i++)
        emit(x, y, z, (rndf() - 0.5f) * 9.0f, rndf() * 8.0f, (rndf() - 0.3f) * 9.0f + speed * 0.5f,
             0.5f + rndf() * 0.5f, (i & 1) ? c1 : c2);
}

static void reset_game(void) {
    for (int i = 0; i < MAXOBJ; i++) ob[i].on = 0;
    for (int i = 0; i < MAXPART; i++) ps[i].life = 0;
    lane = 1; px = 0; py = 0; vy = 0; lives = 3; shards = 0; score = 0;
    speed = 26; dist = 0; inv = 0; shake = 0; spawn_t = 30; paused = 0; msg_t = 0;
}

static void add_obj(float x, float z, int kind) {
    for (int i = 0; i < MAXOBJ; i++)
        if (!ob[i].on) { ob[i].x = x; ob[i].z = z; ob[i].kind = kind; ob[i].on = 1; return; }
}
static void spawn_row(void) {
    int safe = rndi(3);
    float z = -150.0f;
    for (int l = 0; l < 3; l++) {
        if (l == safe) {
            if (rndi(100) < 75)
                for (int k = 0; k < 3; k++) add_obj(LANE_X[l], z - k * 4.0f, K_SHARD);
        } else {
            int r = rndi(100);
            if (r < 45) add_obj(LANE_X[l], z, K_WALL);
            else if (r < 80) add_obj(LANE_X[l], z, K_BARRIER);
        }
    }
}

static void hurt(void) {
    lives--;
    inv = 1.6f; shake = 1.0f;
    burst(px, py + 0.8f, 0, 22, RGB(255, 80, 40), RGB(255, 220, 90));
    if (lives <= 0) {
        state = OVER; overt = 0;
        if ((int)score + shards * 25 > best) best = (int)score + shards * 25;
    }
}

static void update(float dt) {
    tm += dt;

    /* particles always */
    if (!paused) {
        for (int i = 0; i < MAXPART; i++) {
            Part *p = &ps[i];
            if (p->life <= 0) continue;
            p->life -= dt;
            p->x += p->vx * dt; p->y += p->vy * dt; p->z += p->vz * dt;
            p->vy -= 14.0f * dt;
            if (p->y < 0) { p->y = 0; p->vy *= -0.4f; }
        }
        if (shake > 0) shake -= dt * 1.8f;
        if (msg_t > 0) msg_t -= dt;
    }

    if (state == TITLE) {
        dist += 22.0f * dt;
        if (in_start) { reset_game(); state = PLAY; }
        return;
    }
    if (state == OVER) {
        overt += dt;
        if (in_start && overt > 0.6f) { reset_game(); state = PLAY; }
        return;
    }

    /* ---- PLAY ---- */
    if (in_start) paused = !paused;
    if (paused) return;

    if (in_left && lane > 0) lane--;
    if (in_right && lane < 2) lane++;
    px += (LANE_X[lane] - px) * (dt * 13.0f > 1.0f ? 1.0f : dt * 13.0f);

    if (in_jump && py <= 0.001f) vy = 15.5f;
    vy -= 40.0f * dt;
    py += vy * dt;
    if (py <= 0) { py = 0; if (vy < 0) vy = 0; }

    if (speed < 62.0f) speed += dt * 0.45f;
    dist += speed * dt;
    score += speed * dt * 0.5f;
    if (inv > 0) inv -= dt;

    spawn_t -= speed * dt;
    if (spawn_t <= 0) { spawn_row(); spawn_t = 30.0f + rndi(14); }

    /* engine trail */
    emit(px + (rndf() - 0.5f) * 0.5f, py + 0.5f, 1.3f, 0, rndf() * 1.5f, speed * 0.8f, 0.35f,
         (rnd() & 1) ? RGB(255, 140, 40) : RGB(255, 230, 120));

    for (int i = 0; i < MAXOBJ; i++) {
        Obj *o = &ob[i];
        if (!o->on) continue;
        o->z += speed * dt;
        if (o->z > 14.0f) { o->on = 0; continue; }
        if (fabsf(o->z) < 1.9f && fabsf(o->x - px) < 2.3f) {
            if (o->kind == K_SHARD) {
                if (py < 3.2f) {
                    o->on = 0; shards++;
                    burst(o->x, 1.6f, o->z, 12, RGB(255, 235, 110), RGB(255, 255, 255));
                    if (shards % 10 == 0) {
                        if (lives < 5) lives++;
                        score += 500; msg = "SUN BURST!  +1 LIFE"; msg_t = 1.8f;
                        burst(px, 1.0f, 0, 40, RGB(255, 200, 40), RGB(255, 120, 60));
                    }
                }
            } else if (inv <= 0) {
                int hit = (o->kind == K_WALL) || (py < 1.25f);
                if (hit) { o->on = 0; hurt(); if (state == OVER) return; }
            }
        }
    }
}

/* ---------- rendering ---------- */
static void draw_sky(void) {
    const float HZ = 114.0f;
    grad_rect(0, 0, SW, HZ, RGB(14, 6, 48), RGB(255, 96, 92));
    /* stars */
    {
        V *v = (V *)sceGuGetMemory(2 * 36 * sizeof(V));
        for (int i = 0; i < 36; i++) {
            float x = (float)((((uint32_t)i * 2654435761u) >> 16) % SW), y = (float)((((uint32_t)i * 2246822519u + 977u) >> 16) % 95);
            int a = 120 + (int)(120 * (0.5f + 0.5f * sinf(tm * 2.0f + i)));
            uint32_t c = RGBA(255, 255, 255, a);
            v[i * 2].c = c;     v[i * 2].x = x;         v[i * 2].y = y;         v[i * 2].z = 0;
            v[i * 2 + 1].c = c; v[i * 2 + 1].x = x + 2; v[i * 2 + 1].y = y + 2; v[i * 2 + 1].z = 0;
        }
        sceGuDrawArray(GU_SPRITES, F2D, 72, 0, v);
    }
    disc(240, 100, 58, RGB(255, 245, 160), RGB(255, 120, 70));
    /* ground below horizon */
    grad_rect(0, HZ, SW, SH - HZ, RGB(255, 96, 92), RGB(30, 12, 50));
}

static void draw_world(void) {
    ScePspFVector3 eye, ctr, up = {0, 1, 0};
    float sx = sinf(tm * 90.0f) * shake * 0.25f, sy = cosf(tm * 77.0f) * shake * 0.25f;
    eye.x = px * 0.55f + sx; eye.y = 4.2f + sy; eye.z = 8.5f;
    ctr.x = px * 0.40f;      ctr.y = 1.0f;      ctr.z = -20.0f;

    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity();
    sceGumPerspective(70.0f, 16.0f / 9.0f, 0.5f, 400.0f);
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
    sceGumLookAt(&eye, &ctr, &up);
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();

    /* road + neon rails */
    box(0, -0.3f, -80, 14.4f, 0.6f, 200, RGB(38, 28, 64));
    box(-7.4f, 0.25f, -80, 0.4f, 0.5f, 200, RGB(255, 60, 160));
    box( 7.4f, 0.25f, -80, 0.4f, 0.5f, 200, RGB(40, 220, 255));

    /* lane dashes (scroll with distance) */
    float off = fmodf(dist, 12.0f);
    for (int i = 0; i < 15; i++) {
        float z = -i * 12.0f + off;
        box(-2.25f, 0.0f, z, 0.3f, 0.1f, 5.0f, RGB(210, 210, 240));
        box( 2.25f, 0.0f, z, 0.3f, 0.1f, 5.0f, RGB(210, 210, 240));
    }

    /* crystal towers on both sides */
    float off2 = fmodf(dist, 16.0f);
    int base = (int)(dist / 16.0f);
    for (int i = 0; i < 12; i++) {
        float z = -i * 16.0f + off2;
        int id = i + base;
        for (int s = -1; s <= 1; s += 2) {
            float h = 2.5f + (float)(((id * 37 + (s + 1) * 11) & 7));
            float x = s * (11.5f + (float)((id * 13 + s) & 3));
            uint32_t c = ((id + (s > 0)) & 1) ? RGB(40, 200, 230) : RGB(230, 60, 200);
            box(x, h * 0.5f, z, 2.4f, h, 2.4f, c);
            box(x, h + 0.15f, z, 2.6f, 0.3f, 2.6f, shade(c, 1.4f));
        }
    }

    /* obstacles + shards */
    for (int i = 0; i < MAXOBJ; i++) {
        Obj *o = &ob[i];
        if (!o->on) continue;
        if (o->kind == K_BARRIER) {
            box(o->x, 0.6f, o->z, 3.6f, 1.2f, 1.2f, RGB(255, 140, 30));
            box(o->x, 1.0f, o->z, 3.7f, 0.25f, 1.3f, RGB(255, 245, 200));
        } else if (o->kind == K_WALL) {
            box(o->x, 1.8f, o->z, 3.8f, 3.6f, 1.6f, RGB(150, 40, 190));
            box(o->x, 3.7f, o->z, 4.0f, 0.25f, 1.8f, RGB(255, 80, 220));
        } else {
            diamond(o->x, 1.7f + sinf(tm * 4.0f + o->z) * 0.25f, o->z, tm * 3.0f, 0.55f, RGB(255, 215, 60));
        }
    }

    /* player ship */
    box(px, 0.02f, 0, 1.9f - py * 0.1f, 0.04f, 2.6f, RGB(12, 8, 26));
    if (!(inv > 0 && ((int)(inv * 14) & 1))) {
        float lean = (LANE_X[lane] - px) * 0.08f;
        box(px, py + 0.55f, 0, 1.5f, 0.7f, 2.2f, RGB(40, 180, 255));
        box(px, py + 1.05f, -0.2f, 0.9f, 0.5f, 1.0f, RGB(235, 250, 255));
        box(px - 1.0f + lean, py + 0.45f, 0.3f, 0.6f, 0.25f, 1.4f, RGB(255, 200, 60));
        box(px + 1.0f + lean, py + 0.45f, 0.3f, 0.6f, 0.25f, 1.4f, RGB(255, 200, 60));
        float g = 0.5f + 0.5f * sinf(tm * 40.0f);
        box(px, py + 0.5f, 1.25f, 0.8f, 0.4f, 0.3f, RGB(255, (int)(120 + 100 * g), 40));
    }

    /* particles */
    for (int i = 0; i < MAXPART; i++) {
        Part *p = &ps[i];
        if (p->life <= 0) continue;
        float s = 0.08f + 0.32f * (p->life / p->max);
        box(p->x, p->y, p->z, s, s, s, p->col);
    }
}

static void draw_hud(void) {
    char buf[40];
    fill_rect(0, 0, SW, 40, RGBA(0, 0, 0, 110));
    snprintf(buf, sizeof buf, "SCORE %06d", (int)score + shards * 25);
    text_s(8, 5, 2, RGB(255, 255, 255), buf);
    snprintf(buf, sizeof buf, "SHARDS %02d", shards);
    text_s(190, 5, 2, RGB(255, 215, 60), buf);
    for (int i = 0; i < lives; i++) heart(SW - 14 - 18 * (i + 1) + 4, 6, 2, RGB(255, 70, 90));
    /* sun charge bar */
    fill_rect(8, 25, 104, 10, RGBA(255, 255, 255, 120));
    fill_rect(9, 26, 102, 8, RGBA(20, 10, 40, 220));
    fill_rect(9, 26, (shards % 10) * 10.2f, 8, RGB(255, 200, 40));
    text_s(120, 26, 1, RGB(255, 235, 150), "SUN CHARGE");
    if (msg_t > 0) ctext(70, 3, RGB(255, 235, 120), msg);
}

static void render(void) {
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(RGB(14, 6, 48));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

    sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_FOG);
    draw_sky();

    sceGuEnable(GU_DEPTH_TEST); sceGuEnable(GU_FOG);
    draw_world();

    sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_FOG);
    if (state == PLAY || state == OVER) draw_hud();

    if (state == TITLE) {
        ctext(26, 2, RGB(255, 215, 60), "JAYJAY'S QUEST");
        ctext(52, 5, RGB(255, 255, 255), "SUN SHARD");
        ctext(96, 5, RGB(80, 230, 255), "RUNNER");
        if (((int)(tm * 2.0f)) & 1) ctext(176, 2, RGB(255, 255, 255), "PRESS START");
        ctext(212, 1, RGB(255, 220, 160), "LEFT RIGHT OR STICK: CHANGE LANE   X: JUMP");
        ctext(226, 1, RGB(255, 220, 160), "GRAB SHARDS - 10 SHARDS = EXTRA LIFE");
    } else if (state == OVER) {
        fill_rect(0, 60, SW, 150, RGBA(0, 0, 0, 160));
        char buf[40];
        ctext(72, 5, RGB(255, 90, 90), "GAME OVER");
        snprintf(buf, sizeof buf, "SCORE %d", (int)score + shards * 25);
        ctext(126, 2, RGB(255, 255, 255), buf);
        snprintf(buf, sizeof buf, "BEST %d", best);
        ctext(150, 2, RGB(255, 215, 60), buf);
        if (((int)(tm * 2.0f)) & 1) ctext(180, 2, RGB(255, 255, 255), "PRESS START");
    } else if (paused) {
        fill_rect(0, 90, SW, 80, RGBA(0, 0, 0, 150));
        ctext(105, 5, RGB(255, 255, 255), "PAUSED");
        ctext(150, 1, RGB(255, 220, 160), "PRESS START TO RESUME");
    }

    sceGuFinish();
    sceGuSync(0, 0);
}

static void gfx_init(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BW);
    sceGuDispBuffer(SW, SH, (void *)FRAME_SIZE, BW);
    sceGuDepthBuffer((void *)(FRAME_SIZE * 2), BW);
    sceGuOffset(2048 - (SW / 2), 2048 - (SH / 2));
    sceGuViewport(2048, 2048, SW, SH);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SW, SH);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuFog(50.0f, 190.0f, RGB(255, 96, 92));
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

int main(void) {
    setup_callbacks();
    gfx_init();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    rng = sceKernelGetSystemTimeLow() | 1;
    reset_game();

    SceCtrlData pad;
    unsigned prev = 0;
    int pal = 0, par = 0;
    for (;;) {
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned b = pad.Buttons;
        int al = pad.Lx < 60, ar = pad.Lx > 196;
        in_left  = ((b & PSP_CTRL_LEFT)  && !(prev & PSP_CTRL_LEFT))  || (al && !pal);
        in_right = ((b & PSP_CTRL_RIGHT) && !(prev & PSP_CTRL_RIGHT)) || (ar && !par);
        in_jump  = ((b & (PSP_CTRL_CROSS | PSP_CTRL_SQUARE)) != 0);
        in_start = (b & PSP_CTRL_START) && !(prev & PSP_CTRL_START);
        prev = b; pal = al; par = ar;

        update(1.0f / 60.0f);
        render();
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }
    return 0;
}
