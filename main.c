/* ASHFALL - PSP 3D story adventure, VERTICAL SLICE (pspdev SDK / PPSSPP)
 * One location, 1 NPC, 1 enemy type (3 shades), 1 mission, melee combat, dialogue + choice,
 * health, inventory/key item, collectibles, checkpoint save/load, pause menu, objectives.
 * Smooth textured meshes with baked vertex lighting, fog, distance culling, 30 FPS lock.
 * Placeholders are generated in code. Drop real assets in assets/ (see bottom of file).
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

PSP_MODULE_INFO("Ashfall Slice", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(12 * 1024);

#define BW 512
#define SW 480
#define SH 272
#define FS (BW * SH * 4)
#define PI 3.14159265f
#define DT (1.0f / 30.0f)
#define RGB(r,g,b) (0xff000000u | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))
#define RGBA(r,g,b,a) (((unsigned)(a) << 24) | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))
#define SKY RGB(70,60,95)

static unsigned int __attribute__((aligned(16))) list[0x20000];
static int running = 1;
static int exitCb(int a, int b, void *c) { (void)a; (void)b; (void)c; running = 0; sceKernelExitGame(); return 0; }
static int cbThread(SceSize a, void *b) { (void)a; (void)b; sceKernelRegisterExitCallback(sceKernelCreateCallback("exit", exitCb, NULL)); sceKernelSleepThreadCB(); return 0; }

/* ------------------------------------------------------------ meshes */
typedef struct { float u, v; unsigned c; float x, y, z; } TV;
#define TVT (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)
typedef struct { TV *v; int n; } M;
typedef struct { unsigned *d; int w, h; } Tex;

#define ARENA 90000
static TV *arena; static int arenaN;
static TV *tvAlloc(int n) { if (arenaN + n > ARENA) return NULL; TV *p = arena + arenaN; arenaN += n; return p; }

static unsigned sh(unsigned c, int p) {
    unsigned r = (c & 255) * p / 100, g = ((c >> 8) & 255) * p / 100, b = ((c >> 16) & 255) * p / 100;
    if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
    return 0xff000000u | r | (g << 8) | (b << 16);
}
/* baked lighting: one fixed sun + ambient, computed once at build time */
static unsigned bake(unsigned c, float nx, float ny, float nz) {
    float l = sqrtf(nx * nx + ny * ny + nz * nz);
    if (l > 0) { nx /= l; ny /= l; nz /= l; }
    float d = nx * -.4f + ny * .8f + nz * -.45f; if (d < 0) d = 0;
    return sh(c, (int)((.5f + .65f * d) * 100));
}

static void ringN(const float *pr, const float *py, int n, int i, float *nr, float *ny) {
    int a = i > 0 ? i - 1 : i, b = i < n - 1 ? i + 1 : i;
    float dr = pr[b] - pr[a], dy = py[b] - py[a], l = sqrtf(dr * dr + dy * dy);
    if (l < 1e-5f) { *nr = 1; *ny = 0; return; }
    *nr = dy / l; *ny = -dr / l;
}
static void pv(TV *v, float r, float y, float a, float nr, float ny, float u, float vv, unsigned col) {
    float c = cosf(a), s = sinf(a);
    v->u = u; v->v = vv; v->c = bake(col, nr * c, ny, nr * s); v->x = r * c; v->y = y; v->z = r * s;
}
/* surface of revolution: smooth organic shapes (bodies, heads, trees, lamps, fountain) */
static M lathe(const float *pr, const float *py, int n, int seg, float ut, float vt, unsigned col) {
    M m; m.n = (n - 1) * seg * 6; m.v = tvAlloc(m.n); int k = 0;
    if (!m.v) { m.n = 0; return m; }
    for (int i = 0; i < n - 1; i++) {
        float nr0, ny0, nr1, ny1; ringN(pr, py, n, i, &nr0, &ny0); ringN(pr, py, n, i + 1, &nr1, &ny1);
        for (int s = 0; s < seg; s++) {
            float a0 = s * 2 * PI / seg, a1 = (s + 1) * 2 * PI / seg, u0 = ut * s / seg, u1 = ut * (s + 1) / seg;
            float v0 = vt * i / (n - 1), v1 = vt * (i + 1) / (n - 1);
            TV A, B, C, D;
            pv(&A, pr[i], py[i], a0, nr0, ny0, u0, v0, col); pv(&B, pr[i], py[i], a1, nr0, ny0, u1, v0, col);
            pv(&C, pr[i + 1], py[i + 1], a0, nr1, ny1, u0, v1, col); pv(&D, pr[i + 1], py[i + 1], a1, nr1, ny1, u1, v1, col);
            m.v[k++] = A; m.v[k++] = C; m.v[k++] = B; m.v[k++] = B; m.v[k++] = C; m.v[k++] = D;
        }
    }
    return m;
}
static M sphere(float r, float sy, int seg, unsigned col) {
    float pr[7], py[7];
    for (int i = 0; i < 7; i++) { float t = -PI / 2 + PI * i / 6; pr[i] = r * cosf(t); py[i] = r * sy * sinf(t); }
    return lathe(pr, py, 7, seg, 2, 1, col);
}
static M boxM(float hx, float hy, float hz, unsigned col, float tu) {
    static const unsigned char F[6][4] = {{0,1,3,2},{4,5,7,6},{0,2,6,4},{1,3,7,5},{0,1,5,4},{2,3,7,6}};
    static const float FN[6][3] = {{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}};
    static const unsigned char T[6] = {0,1,2,0,2,3};
    M m; m.n = 36; m.v = tvAlloc(36); int k = 0;
    if (!m.v) { m.n = 0; return m; }
    for (int f = 0; f < 6; f++) {
        unsigned c = bake(col, FN[f][0], FN[f][1], FN[f][2]);
        for (int t = 0; t < 6; t++) {
            int j = T[t], i = F[f][j];
            m.v[k].c = c; m.v[k].u = (j == 1 || j == 2) ? tu : 0; m.v[k].v = (j >= 2) ? tu : 0;
            m.v[k].x = (i & 1) ? hx : -hx; m.v[k].y = (i & 2) ? hy * 2 : 0; m.v[k].z = (i & 4) ? hz : -hz; k++;
        }
    }
    return m;
}
static M groundM(float half, float rep, float y, unsigned col) {
    M m; m.n = 6; m.v = tvAlloc(6); if (!m.v) { m.n = 0; return m; }
    static const float cx[6] = {-1,1,-1,-1,1,1}, cz[6] = {-1,-1,1,1,-1,1};
    for (int i = 0; i < 6; i++) { m.v[i].c = bake(col, 0, 1, 0); m.v[i].u = (cx[i] + 1) * .5f * rep; m.v[i].v = (cz[i] + 1) * .5f * rep; m.v[i].x = cx[i] * half; m.v[i].y = y; m.v[i].z = cz[i] * half; }
    return m;
}

/* ---- OBJ / TGA loaders: how real art gets in (assets/*.obj, assets/*.tga) ---- */
static int corner(char **s, int *a, int *b, int *c) {
    char *e; while (**s == ' ' || **s == '\t') (*s)++;
    if (!**s || **s == '\n' || **s == '\r') return 0;
    *a = (int)strtol(*s, &e, 10); *b = *c = 0; *s = e;
    if (**s == '/') { (*s)++; if (**s != '/') { *b = (int)strtol(*s, &e, 10); *s = e; } if (**s == '/') { (*s)++; *c = (int)strtol(*s, &e, 10); *s = e; } }
    return 1;
}
static M loadOBJ(const char *path) {
    M m = {0, 0}; FILE *f = fopen(path, "rb"); if (!f) return m;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1); if (!buf) { fclose(f); return m; }
    if (fread(buf, 1, sz, f) != (size_t)sz) { free(buf); fclose(f); return m; }
    buf[sz] = 0; fclose(f);
    int nv = 0, nt = 0, nn = 0, tris = 0, a, b, c;
    for (char *p = buf; *p;) {
        if (p[0] == 'v' && p[1] == ' ') nv++; else if (p[0] == 'v' && p[1] == 't') nt++; else if (p[0] == 'v' && p[1] == 'n') nn++;
        else if (p[0] == 'f' && p[1] == ' ') { char *q = p + 1; int k = 0; while (corner(&q, &a, &b, &c)) k++; if (k >= 3) tris += k - 2; }
        while (*p && *p != '\n') p++; if (*p) p++;
    }
    float *V = malloc((nv + 1) * 12), *T = malloc((nt + 1) * 8), *N = malloc((nn + 1) * 12);
    m.v = tvAlloc(tris * 3);
    if (!V || !T || !N || !m.v) { free(V); free(T); free(N); free(buf); m.n = 0; m.v = 0; return m; }
    int iv = 0, it = 0, in = 0, k = 0;
    for (char *p = buf; *p;) {
        if (p[0] == 'v' && p[1] == ' ') { sscanf(p + 2, "%f %f %f", &V[iv * 3], &V[iv * 3 + 1], &V[iv * 3 + 2]); iv++; }
        else if (p[0] == 'v' && p[1] == 't') { sscanf(p + 3, "%f %f", &T[it * 2], &T[it * 2 + 1]); it++; }
        else if (p[0] == 'v' && p[1] == 'n') { sscanf(p + 3, "%f %f %f", &N[in * 3], &N[in * 3 + 1], &N[in * 3 + 2]); in++; }
        else if (p[0] == 'f' && p[1] == ' ') {
            int ia[16], ib[16], ic[16], kk = 0; char *q = p + 1;
            while (kk < 16 && corner(&q, &ia[kk], &ib[kk], &ic[kk])) kk++;
            for (int t = 1; t + 1 < kk; t++) {
                int id[3] = {0, t, t + 1};
                float fn[3] = {0, 1, 0};
                if (!ic[0] || ic[0] > nn) {
                    int i0 = ia[0] - 1, i1 = ia[t] - 1, i2 = ia[t + 1] - 1;
                    if (i0 >= 0 && i1 >= 0 && i2 >= 0 && i0 < iv && i1 < iv && i2 < iv) {
                        float ux = V[i1*3]-V[i0*3], uy = V[i1*3+1]-V[i0*3+1], uz = V[i1*3+2]-V[i0*3+2], wx = V[i2*3]-V[i0*3], wy = V[i2*3+1]-V[i0*3+1], wz = V[i2*3+2]-V[i0*3+2];
                        fn[0] = uy*wz - uz*wy; fn[1] = uz*wx - ux*wz; fn[2] = ux*wy - uy*wx;
                    }
                }
                for (int j = 0; j < 3; j++) {
                    int pi = ia[id[j]] - 1, ti = ib[id[j]] - 1, ni = ic[id[j]] - 1;
                    TV *o = &m.v[k++];
                    if (pi < 0 || pi >= iv) pi = 0;
                    o->x = V[pi*3]; o->y = V[pi*3+1]; o->z = V[pi*3+2];
                    o->u = (ti >= 0 && ti < it) ? T[ti*2] : 0; o->v = (ti >= 0 && ti < it) ? 1.0f - T[ti*2+1] : 0;
                    if (ni >= 0 && ni < in) o->c = bake(RGB(255,255,255), N[ni*3], N[ni*3+1], N[ni*3+2]); else o->c = bake(RGB(255,255,255), fn[0], fn[1], fn[2]);
                }
            }
        }
        while (*p && *p != '\n') p++; if (*p) p++;
    }
    m.n = k; free(V); free(T); free(N); free(buf);
    return m;
}
static Tex loadTGA(const char *path) {
    Tex t = {0, 0, 0}; FILE *f = fopen(path, "rb"); if (!f) return t;
    unsigned char h[18]; if (fread(h, 1, 18, f) != 18) { fclose(f); return t; }
    int w = h[12] | (h[13] << 8), hh = h[14] | (h[15] << 8), bpp = h[16];
    if (h[2] != 2 || (bpp != 24 && bpp != 32) || w < 4 || hh < 1 || w > 256 || hh > 256 || (w & (w - 1)) || (hh & (hh - 1))) { fclose(f); return t; }
    fseek(f, h[0], SEEK_CUR);
    int bp = bpp / 8; unsigned char *px = malloc(w * hh * bp); unsigned *d = memalign(16, w * hh * 4);
    if (!px || !d || fread(px, 1, w * hh * bp, f) != (size_t)(w * hh * bp)) { free(px); free(d); fclose(f); return t; }
    fclose(f);
    for (int y = 0; y < hh; y++) {
        int sy = (h[17] & 0x20) ? y : hh - 1 - y;
        for (int x = 0; x < w; x++) { unsigned char *s = px + (sy * w + x) * bp; d[y * w + x] = 0xff000000u | (s[2]) | (s[1] << 8) | ((unsigned)s[0] << 16); }
    }
    free(px); t.d = d; t.w = w; t.h = hh;
    return t;
}

/* ---- procedural placeholder textures (64x64) ---- */
static unsigned hsh(int x, int y) { unsigned h = x * 374761393u + y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }
static Tex mkTex(int kind) {
    Tex t; t.w = t.h = 64; t.d = memalign(16, 64 * 64 * 4);
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        int n = (int)(hsh(x, y) & 31) - 16, r, g, b;
        if (kind == 0) { r = 70 + n; g = 115 + n * 2; b = 55 + n; }                                   /* grass */
        else if (kind == 1) { int e = (x % 32 < 2) || (y % 32 < 2); r = g = b = e ? 70 : 150 + n; b += 5; } /* flagstone */
        else if (kind == 2) { int e = (y % 16 < 2) || (((x + (y / 16) * 16) % 32) < 2); r = e ? 120 : 205 + n; g = e ? 110 : 190 + n; b = e ? 100 : 160 + n; } /* plaster+brick */
        else { int wv = ((x ^ y) & 4) ? 6 : -6; r = g = b = 205 + n + wv; }                              /* cloth */
        t.d[y * 64 + x] = 0xff000000u | (r < 0 ? 0 : r > 255 ? 255 : r) | ((g < 0 ? 0 : g > 255 ? 255 : g) << 8) | ((unsigned)(b < 0 ? 0 : b > 255 ? 255 : b) << 16);
    }
    return t;
}
static void bind(const Tex *t) { sceGuTexImage(0, t->w, t->h, t->w, t->d); }

/* ------------------------------------------------------------ humanoid rig (rigid parts, procedural animation) */
typedef struct { M torso, head, hair, arm, leg, eye; Tex tex; } Hum;
static Tex texCloth, texGrass, texStone, texWall, texLevel;
static M tryObj(const char *pre, const char *part, M fb) { char p[96]; snprintf(p, sizeof p, "assets/%s_%s.obj", pre, part); M m = loadOBJ(p); return m.n ? m : fb; }

static Hum mkHum(const char *pre, unsigned shirt, unsigned pants, unsigned skin, unsigned hair, unsigned eye) {
    static const float tp[6] = {.2f,.26f,.28f,.33f,.2f,.09f}, ty[6] = {-.05f,.1f,.3f,.55f,.66f,.7f};
    static const float ap[5] = {.07f,.08f,.065f,.055f,.05f}, ay[5] = {0,-.2f,-.4f,-.6f,-.7f};
    static const float lp[5] = {.12f,.12f,.09f,.08f,.09f}, ly[5] = {0,-.3f,-.6f,-.82f,-.88f};
    float hp[5], hy[5]; Hum h; char p[96];
    for (int i = 0; i < 5; i++) { float t = .15f + (PI / 2 - .15f) * i / 4; hp[i] = .19f * cosf(t); hy[i] = .22f * sinf(t); }
    h.torso = tryObj(pre, "torso", lathe(tp, ty, 6, 10, 2, 1, shirt));
    h.head = tryObj(pre, "head", sphere(.17f, 1.2f, 10, skin));
    h.hair = lathe(hp, hy, 5, 10, 2, 1, hair);
    h.arm = tryObj(pre, "arm", lathe(ap, ay, 5, 8, 1, 1, shirt));
    h.leg = tryObj(pre, "leg", lathe(lp, ly, 5, 8, 1, 1, pants));
    h.eye = sphere(.03f, 1, 6, eye);
    snprintf(p, sizeof p, "assets/%s.tga", pre); h.tex = loadTGA(p); if (!h.tex.d) h.tex = texCloth;
    return h;
}
enum { A_IDLE, A_WALK, A_ATTACK, A_HURT, A_DIE, A_TALK };
static void tr(float x, float y, float z) { ScePspFVector3 v = {x, y, z}; sceGumTranslate(&v); }
static void drawM(const M *m) { if (m->n) sceGumDrawArray(GU_TRIANGLES, TVT, m->n, 0, m->v); }
static void drawHum(const Hum *h, float x, float y, float z, float yaw, float ph, float spd, int an, float at) {
    float sw = sinf(ph) * .8f * spd, bob = fabsf(cosf(ph)) * .04f * spd, ra = -sw, la = sw;
    bind(&h->tex);
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
    tr(x, y + .9f + bob, z); sceGumRotateY(yaw);
    if (an == A_HURT) sceGumRotateX(-.5f * sinf(at * PI));
    if (an == A_DIE) { sceGumRotateX(-1.5f * (at > 1 ? 1 : at)); }
    if (an == A_ATTACK) { float p = at > 1 ? 1 : at; ra = -2.8f + p * 2.2f; }
    if (an == A_TALK) ra = -1.1f + sinf(at * 6) * .3f;
    if (an == A_IDLE) { bob = sinf(at * 2) * .01f; }
    drawM(&h->torso);
    sceGumPushMatrix(); tr(0, .88f, 0); drawM(&h->head); drawM(&h->hair);
    sceGumPushMatrix(); tr(-.07f, .03f, .17f); drawM(&h->eye); sceGumPopMatrix();
    sceGumPushMatrix(); tr(.07f, .03f, .17f); drawM(&h->eye); sceGumPopMatrix(); sceGumPopMatrix();
    sceGumPushMatrix(); tr(.37f, .58f, 0); sceGumRotateX(ra); drawM(&h->arm); sceGumPopMatrix();
    sceGumPushMatrix(); tr(-.37f, .58f, 0); sceGumRotateX(la); drawM(&h->arm); sceGumPopMatrix();
    sceGumPushMatrix(); tr(.13f, 0, 0); sceGumRotateX(la); drawM(&h->leg); sceGumPopMatrix();
    sceGumPushMatrix(); tr(-.13f, 0, 0); sceGumRotateX(-la); drawM(&h->leg); sceGumPopMatrix();
}

/* ------------------------------------------------------------ level: Ashfall town square */
typedef struct { float x, z, hx, hz, h; unsigned c; } Bd;
typedef struct { float x, z, r; } Ci;
static const Bd BLD[] = {
    {-16,-24,5,4,4,RGB(225,205,170)}, {-4,-25,4,3.5f,5.5f,RGB(200,185,170)}, {12,-24,6,4,4.5f,RGB(215,190,160)},
    {-27,-6,3.5f,5,4,RGB(210,200,180)}, {27,-4,3.5f,6,5,RGB(220,195,165)},
    {-14,25,5,3.5f,3.5f,RGB(205,190,165)}, {14,26,5,3.5f,4,RGB(225,200,170)} };
#define NB ((int)(sizeof(BLD) / sizeof(BLD[0])))
static const Ci TREE[] = {{-24,12,.6f},{-23,-17,.6f},{24,14,.6f},{22,-19,.6f},{-9,17,.6f},{9,19,.6f},{-25,20,.6f},{25,22,.6f}};
#define NT ((int)(sizeof(TREE) / sizeof(TREE[0])))
static const float LAMP[4][2] = {{9,-9},{-9,-9},{-9,9},{9,9}};   /* lamp 0 = checkpoint beside Mara */
static const float ORB[5][2] = {{-20,18},{21,18},{-21,-14},{20,-14},{0,-18}};
static const float HOME[3][2] = {{-15,6},{17,8},{-5,-13}};
#define BOUND 28.0f

static M mBld[NB], mRoof[NB], mTrunk, mLeaf, mPole, mBulb, mFount, mOrb, mGrass, mPlaza, mLevel;

static void buildLevel(void) {
    static const float trp[3] = {.25f,.17f,.14f}, trh[3] = {0,1.2f,2.6f};
    static const float lfp[5] = {.02f,.9f,1.5f,1.8f,.3f}, lfy[5] = {5.6f,4.6f,3.5f,2.5f,2.3f};
    static const float pop[5] = {.22f,.1f,.08f,.08f,.12f}, poy[5] = {0,.4f,1.5f,3.3f,3.45f};
    static const float fp[8] = {.01f,2.6f,2.6f,2.35f,2.35f,.5f,.35f,.9f}, fy[8] = {.05f,.05f,.75f,.75f,.35f,.35f,1.5f,1.6f};
    for (int i = 0; i < NB; i++) { mBld[i] = boxM(BLD[i].hx, BLD[i].h / 2, BLD[i].hz, BLD[i].c, 3); mRoof[i] = boxM(BLD[i].hx + .6f, .3f, BLD[i].hz + .6f, RGB(120,60,50), 2); }
    mTrunk = lathe(trp, trh, 3, 8, 1, 2, RGB(120,85,60)); mLeaf = lathe(lfp, lfy, 5, 9, 3, 2, RGB(70,120,70));
    mPole = lathe(pop, poy, 5, 8, 1, 3, RGB(80,80,95)); mBulb = sphere(.3f, 1.1f, 8, RGB(255,220,120));
    mFount = lathe(fp, fy, 8, 14, 4, 2, RGB(150,150,160)); mOrb = sphere(.28f, 1.2f, 8, RGB(150,230,255));
    mGrass = groundM(70, 30, 0, RGB(255,255,255)); mPlaza = groundM(15, 6, .03f, RGB(255,255,255));
    mLevel = loadOBJ("assets/level.obj"); texLevel = loadTGA("assets/level.tga");
}

/* ------------------------------------------------------------ game state */
typedef struct { float x, z, yaw, ph, t, cd, hurt; int hp, st, alive, active; } En;
enum { M_TITLE, M_PLAY, M_SAY, M_CHOOSE, M_PAUSE, M_DEAD };
enum { Q_TALK, Q_LAMP, Q_JOREN, Q_LINA, Q_KILL, Q_RETURN, Q_DONE };
static int mode = M_TITLE, quest, trust, orbs, hasKey, php = 5, orbGot[5], menuSel, sel;
static float px, pz, pyaw, pph, pspd, inv, pAtk = -1, hurtT, talkT, camYaw, fadeT, ckx = 7.5f, ckz = -5.5f;
static int hitDone;
static En en[3];
static Hum hHero, hMara, hShade;
static unsigned frames, playF;
static char msg[64]; static int msgT;
static float marX = 6.5f, marZ = -7.0f;
static float jorenX = -7.0f, jorenZ = -7.5f;
static float linaX = 8.5f, linaZ = 7.0f;

static void note(const char *s) { snprintf(msg, sizeof msg, "%s", s); msgT = 90; }
static int kills(void) { int k = 0; for (int i = 0; i < 3; i++) if (!en[i].alive) k++; return k; }

static void collide(float *x, float *z, float r) {
    if (*x > BOUND) *x = BOUND; if (*x < -BOUND) *x = -BOUND; if (*z > BOUND) *z = BOUND; if (*z < -BOUND) *z = -BOUND;
    for (int i = 0; i < NB; i++) {
        float cx = *x < BLD[i].x - BLD[i].hx ? BLD[i].x - BLD[i].hx : *x > BLD[i].x + BLD[i].hx ? BLD[i].x + BLD[i].hx : *x;
        float cz = *z < BLD[i].z - BLD[i].hz ? BLD[i].z - BLD[i].hz : *z > BLD[i].z + BLD[i].hz ? BLD[i].z + BLD[i].hz : *z;
        float dx = *x - cx, dz = *z - cz, d = sqrtf(dx * dx + dz * dz);
        if (d < r) { if (d > 1e-4f) { *x = cx + dx / d * r; *z = cz + dz / d * r; } else *x += r; }
    }
    for (int i = 0; i < NT; i++) { float dx = *x - TREE[i].x, dz = *z - TREE[i].z, d = sqrtf(dx * dx + dz * dz), m = TREE[i].r + r; if (d < m && d > 1e-4f) { *x = TREE[i].x + dx / d * m; *z = TREE[i].z + dz / d * m; } }
    { float d = sqrtf(*x * *x + *z * *z); if (d < 2.9f + r && d > 1e-4f) { *x = *x / d * (2.9f + r); *z = *z / d * (2.9f + r); } }
}

/* ---- save / load (checkpoint) ---- */
typedef struct { int magic, quest, hp, orbs, key, trust; float x, z; int orbGot[5], dead[3]; } SV;
static int saveGame(void) {
    SV s; memset(&s, 0, sizeof s); s.magic = 0xA5F1; s.quest = quest; s.hp = php; s.orbs = orbs; s.key = hasKey; s.trust = trust; s.x = ckx; s.z = ckz + 1.5f;
    for (int i = 0; i < 5; i++) s.orbGot[i] = orbGot[i];
    for (int i = 0; i < 3; i++) s.dead[i] = !en[i].alive;
    FILE *f = fopen("save.bin", "wb"); if (!f) return 0; fwrite(&s, sizeof s, 1, f); fclose(f); return 1;
}
static void resetEnemies(const int *dead) {
    for (int i = 0; i < 3; i++) {
        en[i].x = HOME[i][0]; en[i].z = HOME[i][1]; en[i].yaw = 0; en[i].ph = 0;
        en[i].t = 0; en[i].cd = 0; en[i].hurt = 0; en[i].hp = 3; en[i].st = 0;
        en[i].alive = dead ? !dead[i] : 1; en[i].active = 0;
    }
}
static int loadGame(void) {
    SV s; FILE *f = fopen("save.bin", "rb"); if (!f) return 0;
    int ok = fread(&s, sizeof s, 1, f) == 1 && s.magic == 0xA5F1; fclose(f); if (!ok) return 0;
    quest = s.quest; php = s.hp; orbs = s.orbs; hasKey = s.key; trust = s.trust; px = s.x; pz = s.z; pyaw = 3.14f; inv = 0; pAtk = -1;
    for (int i = 0; i < 5; i++) orbGot[i] = s.orbGot[i];
    resetEnemies(s.dead); for (int i=0;i<3;i++) en[i].active=(quest>=Q_KILL && quest!=Q_DONE); return 1;
}
static void newGame(void) {
    quest=Q_TALK; trust=0; orbs=0; hasKey=0; php=5; px=0; pz=14; pyaw=3.14f; camYaw=0;
    inv=0; pAtk=-1; playF=0; memset(orbGot,0,sizeof orbGot); resetEnemies(0);
}


/* ---- dialogue / story ---- */
typedef struct { const char *s, *t; } L;
#define E {0,0}
static const L d_intro[] = {
 {"", "Ashfall was once a quiet mining town. Tonight, its lamps are going dark."},
 {"Kai", "Mara's letter said to meet her in the square. I should find her."},
 {"", "Move with the analog stick. X talks and attacks. Square dodges. Circle runs. START pauses."}, E};
static const L d_mara1[] = {
 {"Mara", "Kai. You made it. Something is wrong with the town."},
 {"Kai", "Your letter mentioned the lamps. What happened?"},
 {"Mara", "The eastern lamp went dark first. People have heard whispers near the old mine."},
 {"Mara", "Inspect the broken lamp near the east side of the square."}, E};
static const L d_joren[] = {
 {"Joren", "That lamp wasn't broken by weather. Look at the black marks around the base."},
 {"Kai", "So something did this?"},
 {"Joren", "Something came from the mine. I saw a shadow between the houses."},
 {"Joren", "Lina keeps records of the old mine. She may know what we're dealing with."}, E};
static const L d_lina[] = {
 {"Lina", "The old records call them Shades. They appear when the mine's deepest lamp goes out."},
 {"Kai", "How do we stop them?"},
 {"Lina", "Wait for a Shade to commit to an attack, then strike while it is open."},
 {"Lina", "Mara should know what to do next. Go back to her."}, E};
static const L d_mara2[] = {
 {"Mara", "Joren and Lina were right. The Shades are here."},
 {"Mara", "Three of them entered the square."},
 {"Kai", "And after that?"},
 {"Mara", "Come back to me when the square is safe."}, E};
static const L d_mara3[] = {
 {"Mara", "The square is quiet again."},
 {"Kai", "For now."},
 {"Mara", "Take this Lantern Key. It belonged to your father. The old mine is waiting."},
 {"", "CHAPTER 1 COMPLETE - THE DARKENED LAMP"}, E};
static const L d_after[] = {{"Mara", "The mine is beyond the northern road. We will go there next."}, E};
static const L *dl,*cur; static int di,after; static float dchars,talkT;
/* Choice labels are kept here so the optional M_CHOOSE renderer always has
 * valid strings. Chapter 1 currently uses linear dialogue; these can be
 * replaced by real choices later without changing the renderer. */
static const char *optA = "Continue";
static const char *optB = "Leave";
static void load(void);
static void startDlg(const L *a,int aft){dl=a;di=0;after=aft;sel=0;load();}
static void finishD(void){
 int a=after; after=0; mode=M_PLAY;
 if(a==10){quest=Q_LAMP;note("Objective: inspect the broken lamp");saveGame();}
 else if(a==11){quest=Q_JOREN;note("Objective: talk to Joren");saveGame();}
 else if(a==12){quest=Q_LINA;note("Objective: talk to Lina");saveGame();}
 else if(a==13){quest=Q_KILL;for(int i=0;i<3;i++)en[i].active=1;note("The Shades have appeared");saveGame();}
 else if(a==15){quest=Q_DONE;hasKey=1;note("Received: Lantern Key");saveGame();}
}
static void load(void){cur=&dl[di++];if(!cur->s){finishD();return;}dchars=0;talkT=0;mode=M_SAY;}
static const char *objective(void){
 switch(quest){case Q_TALK:return "Talk to Mara by the fountain.";case Q_LAMP:return "Inspect the broken eastern lamp.";case Q_JOREN:return "Talk to Joren about the lamp.";case Q_LINA:return "Talk to Lina about the mine.";case Q_KILL:{static char b[48];snprintf(b,sizeof b,"Defeat the Shades (%d/3).",kills());return b;}case Q_RETURN:return "Return to Mara after the battle.";default:return "Chapter 1 complete. The mine awaits.";}
}
/* ------------------------------------------------------------ update */
static float dist(float ax, float az, float bx, float bz) { float a = ax - bx, b = az - bz; return sqrtf(a * a + b * b); }

static void hurtPlayer(float fx, float fz) {
    if (inv > 0 || php <= 0) return;
    php--; inv = 1.4f; hurtT = .4f;
    float d = dist(px, pz, fx, fz); if (d > .01f) { px += (px - fx) / d * 1.2f; pz += (pz - fz) / d * 1.2f; collide(&px, &pz, .5f); }
    if (php <= 0) { mode = M_DEAD; fadeT = 0; }
}

static void updateEnemies(void){
 if(quest!=Q_KILL&&quest!=Q_RETURN)return;
 for(int i=0;i<3;i++){En *e=&en[i];if(!e->active)continue;float d=dist(e->x,e->z,px,pz),sp=0;e->t+=DT;if(!e->alive)continue;
  if(e->hurt>0){e->hurt-=DT;continue;}if(e->cd>0)e->cd-=DT;
  if(e->st==2){e->yaw=atan2f(px-e->x,pz-e->z);if(e->t>.55f){if(d<2.0f)hurtPlayer(e->x,e->z);e->st=0;e->cd=1.15f;e->t=0;}continue;}
  if(d<10&&php>0){e->st=1;e->yaw=atan2f(px-e->x,pz-e->z);sp=2.25f;if(d<1.8f){sp=0;if(e->cd<=0){e->st=2;e->t=0;}}}
  else{e->st=0;float hx=HOME[i][0]-e->x,hz=HOME[i][1]-e->z,hd=sqrtf(hx*hx+hz*hz);if(hd>1.5f){e->yaw=atan2f(hx,hz);sp=.8f;}}
  e->x+=sinf(e->yaw)*sp*DT;e->z+=cosf(e->yaw)*sp*DT;collide(&e->x,&e->z,.5f);e->ph+=sp*DT*3;
 }
}

static void playerAttackHit(void){
 for(int i=0;i<3;i++){En *e=&en[i];if(!e->active||!e->alive)continue;float d=dist(px,pz,e->x,e->z);if(d>2.35f)continue;float ang=atan2f(e->x-px,e->z-pz)-pyaw;while(ang>PI)ang-=2*PI;while(ang<-PI)ang+=2*PI;if(fabsf(ang)>1.0f)continue;
  e->hp--;e->hurt=.38f;e->t=0;e->st=0;e->cd=.55f;if(d>.01f){e->x+=(e->x-px)/d*.7f;e->z+=(e->z-pz)/d*.7f;collide(&e->x,&e->z,.5f);}if(e->hp<=0){e->alive=0;e->t=0;note("Shade defeated");}else note("Hit! Shade staggered");}
}

static void update(const SceCtrlData *p,unsigned pr){
 frames++;if(msgT>0)msgT--;
 if(mode==M_TITLE){camYaw+=.01f;if(pr&(PSP_CTRL_UP|PSP_CTRL_DOWN))menuSel=!menuSel;if(pr&PSP_CTRL_CROSS){if(menuSel==1&&loadGame())mode=M_PLAY;else{newGame();startDlg(d_intro,0);}}return;}
 if(mode==M_DEAD){fadeT+=DT;if(fadeT>1.2f&&(pr&PSP_CTRL_CROSS)){if(!loadGame())newGame();mode=M_PLAY;}return;}
 if(mode==M_SAY){int len=(int)strlen(cur->t);dchars+=1.8f;talkT+=DT;if(pr&PSP_CTRL_CROSS){if(dchars<len)dchars=(float)len;else load();}return;}
 if(mode==M_CHOOSE){if(pr&PSP_CTRL_UP)sel=0;if(pr&PSP_CTRL_DOWN)sel=1;if(pr&PSP_CTRL_CROSS){if(sel==0)load();else{after=0;mode=M_PLAY;}}return;}
 if(mode==M_PAUSE){if(pr&PSP_CTRL_UP)menuSel=(menuSel+3)%4;if(pr&PSP_CTRL_DOWN)menuSel=(menuSel+1)%4;if(pr&PSP_CTRL_START)mode=M_PLAY;if(pr&PSP_CTRL_CROSS){if(menuSel==0)mode=M_PLAY;else if(menuSel==1)note(saveGame()?"Game saved":"Save failed");else if(menuSel==2){if(loadGame()){mode=M_PLAY;note("Game loaded");}else note("No save found");}else{mode=M_TITLE;menuSel=0;}}return;}
 playF++;if(pr&PSP_CTRL_START){mode=M_PAUSE;menuSel=0;return;}if(inv>0)inv-=DT;if(hurtT>0)hurtT-=DT;
 float dx=(p->Lx-128)/128.0f,dz=(p->Ly-128)/128.0f;if(fabsf(dx)<.25f)dx=0;if(fabsf(dz)<.25f)dz=0;if(p->Buttons&PSP_CTRL_LEFT)dx=-1;if(p->Buttons&PSP_CTRL_RIGHT)dx=1;if(p->Buttons&PSP_CTRL_UP)dz=-1;if(p->Buttons&PSP_CTRL_DOWN)dz=1;if(p->Buttons&PSP_CTRL_LTRIGGER)camYaw-=1.6f*DT;if(p->Buttons&PSP_CTRL_RTRIGGER)camYaw+=1.6f*DT;
 float fx=-sinf(camYaw),fz=-cosf(camYaw),rx=cosf(camYaw),rz=-sinf(camYaw),mx=-fx*dz+rx*dx,mz=-fz*dz+rz*dx,m=sqrtf(mx*mx+mz*mz);pspd=0;
 if(m>.01f&&pAtk<0){if(m>1){mx/=m;mz/=m;m=1;}float sp=((p->Buttons&PSP_CTRL_CIRCLE)?7:4.2f)*m*DT;px+=mx*sp;pz+=mz*sp;pyaw=atan2f(mx,mz);pspd=(p->Buttons&PSP_CTRL_CIRCLE)?1.4f:1;pph+=sp*2.4f;}collide(&px,&pz,.5f);
 if(pr&PSP_CTRL_SQUARE&&pAtk<0&&inv<=0){inv=.55f;px+=sinf(pyaw);pz+=cosf(pyaw);collide(&px,&pz,.5f);note("Dodge");}
 for(int i=0;i<5;i++)if(!orbGot[i]&&dist(px,pz,ORB[i][0],ORB[i][1])<1.4f){orbGot[i]=1;orbs++;char b[40];snprintf(b,sizeof b,"Memory orb %d/5",orbs);note(b);}
 if(pAtk>=0){pAtk+=DT;if(pAtk>.14f&&!hitDone){hitDone=1;playerAttackHit();}if(pAtk>.48f)pAtk=-1;}
 int nearMara=dist(px,pz,marX,marZ)<2.7f,nearJoren=dist(px,pz,jorenX,jorenZ)<2.7f,nearLina=dist(px,pz,linaX,linaZ)<2.7f,nearLamp=dist(px,pz,ckx,ckz)<2.6f;
 if(pr&PSP_CTRL_CROSS){
  if(nearMara){if(quest==Q_TALK)startDlg(d_mara1,10);else if(quest==Q_KILL)startDlg(d_mara2,13);else if(quest==Q_RETURN)startDlg(d_mara3,15);else startDlg(d_after,0);return;}
  if(quest==Q_JOREN&&nearJoren){startDlg(d_joren,11);return;}if(quest==Q_LINA&&nearLina){startDlg(d_lina,12);return;}
  if(quest==Q_LAMP&&nearLamp){note("The lamp is cold. Something attacked it.");quest=Q_JOREN;saveGame();return;}if(nearLamp&&quest!=Q_LAMP){php=5;note(saveGame()?"Checkpoint saved. Health restored.":"Save failed");return;}if(pAtk<0){pAtk=0;hitDone=0;}
 }
 updateEnemies();if(quest==Q_KILL&&kills()>=3){quest=Q_RETURN;note("All Shades defeated - return to Mara");saveGame();}
}

/* ------------------------------------------------------------ render */
static int drawOff = 0; /* current render-buffer VRAM offset */
typedef struct { unsigned c; float x, y, z; } CV;
static void rect(int x, int y, int w, int h, unsigned c) {
    CV *v = sceGuGetMemory(2 * sizeof(CV));
    v[0].c = c; v[0].x = (float)x; v[0].y = (float)y; v[0].z = 0; v[1].c = c; v[1].x = (float)(x + w); v[1].y = (float)(y + h); v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
}
static void ptxt(int c, int r, unsigned col, const char *s) { pspDebugScreenSetTextColor(col); pspDebugScreenSetXY(c, r); pspDebugScreenPuts(s); }
static void wrapPrint(const char *t, int shown, int row, unsigned col) {
    char line[64]; int pos = 0, len = (int)strlen(t);
    while (pos < len && row < 29) {
        int end = pos + 54; if (end >= len) end = len; else { while (end > pos && t[end] != ' ') end--; if (end == pos) end = pos + 54; }
        int n = end - pos, show = shown - pos; if (show > n) show = n; if (show < 0) show = 0;
        memcpy(line, t + pos, show); line[show] = 0; ptxt(2, row, col, line);
        pos = end; while (pos < len && t[pos] == ' ') pos++; row++;
    }
}
static void place(float x, float y, float z, float yaw) { sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity(); tr(x, y, z); if (yaw) sceGumRotateY(yaw); }

static void enemyAnim(const En *e, int *an, float *at) {
    if (!e->alive) { *an = A_DIE; *at = e->t / .8f; return; }
    if (e->hurt > 0) { *an = A_HURT; *at = 1 - e->hurt / .35f; return; }
    if (e->st == 2) { *an = A_ATTACK; *at = e->t / .6f; return; }
    *an = A_WALK; *at = e->t;
}

static void world(float t) {
    float far2 = 70.0f * 70.0f;
    sceGuEnable(GU_TEXTURE_2D);
    bind(&texGrass); place(0, 0, 0, 0); drawM(&mGrass);
    if (mLevel.n) { bind(texLevel.d ? &texLevel : &texWall); place(0, 0, 0, 0); drawM(&mLevel); }
    else {
        bind(&texStone); place(0, 0, 0, 0); drawM(&mPlaza); drawM(&mFount);
        for (int i = 0; i < NB; i++) {
            if (dist(px, pz, BLD[i].x, BLD[i].z) * 1 > 70) continue;
            bind(&texWall); place(BLD[i].x, 0, BLD[i].z, 0); drawM(&mBld[i]);
            bind(&texCloth); place(BLD[i].x, BLD[i].h, BLD[i].z, 0); drawM(&mRoof[i]);
        }
    }
    bind(&texCloth);
    for (int i = 0; i < NT; i++) { if (dist(px, pz, TREE[i].x, TREE[i].z) > 60) continue; place(TREE[i].x, 0, TREE[i].z, i * 1.3f); drawM(&mTrunk); drawM(&mLeaf); }
    for (int i = 0; i < 4; i++) { place(LAMP[i][0], 0, LAMP[i][1], 0); drawM(&mPole); sceGumPushMatrix(); tr(0, 3.65f, 0); drawM(&mBulb); sceGumPopMatrix(); }
    for (int i = 0; i < 5; i++) if (!orbGot[i]) { place(ORB[i][0], 1.0f + sinf(t * 2 + i) * .2f, ORB[i][1], t * 2); drawM(&mOrb); }
    (void)far2;
    place(0, 0, 0, 0);
    drawHum(&hMara, marX, 0, marZ, atan2f(px - marX, pz - marZ), 0, 0, (mode == M_SAY || mode == M_CHOOSE) ? A_TALK : A_IDLE, (mode == M_SAY) ? talkT : t);
    drawHum(&hMara, jorenX, 0, jorenZ, atan2f(px - jorenX, pz - jorenZ), 0, 0, A_IDLE, t);
    drawHum(&hHero, linaX, 0, linaZ, atan2f(px - linaX, pz - linaZ), 0, 0, A_IDLE, t);
    for (int i = 0; i < 3; i++) {
        const En *e = &en[i]; int an; float at; if (!e->active) continue; if (!e->alive && e->t > 3) continue;
        enemyAnim(e, &an, &at); drawHum(&hShade, e->x, 0, e->z, e->yaw, e->ph, e->st == 1 ? 1.2f : (e->st == 0 ? .6f : 0), an, at);
    }
    if (mode == M_TITLE) drawHum(&hHero, 0, 0, 8, t, 0, 0, A_IDLE, t);
    else if (inv <= 0 || ((int)(t * 12) & 1)) {
        int an = A_IDLE; float at = t;
        if (pAtk >= 0) { an = A_ATTACK; at = pAtk / .5f; } else if (hurtT > 0) { an = A_HURT; at = 1 - hurtT / .4f; } else if (pspd > 0) { an = A_WALK; at = t; }
        drawHum(&hHero, px, 0, pz, pyaw, pph, pspd, an, at);
    }
}

static void render(float t) {
    char buf[96];
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(SKY); sceGuClearDepth(0); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuEnable(GU_DEPTH_TEST); sceGuDisable(GU_BLEND); sceGuEnable(GU_FOG); sceGuFog(14.0f, 58.0f, SKY);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity(); sceGumPerspective(55.0f, 16.0f / 9.0f, 0.5f, 90.0f);
    float tx = mode == M_TITLE ? 0 : px, tz = mode == M_TITLE ? 8 : pz, dist_ = mode == M_TITLE ? 5.5f : 8.0f;
    ScePspFVector3 eye = {tx + sinf(camYaw) * dist_, mode == M_TITLE ? 2.2f : 4.6f, tz + cosf(camYaw) * dist_}, ctr = {tx, 1.5f, tz}, up = {0, 1, 0};
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity(); sceGumLookAt(&eye, &ctr, &up);
    world(t);

    sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_FOG); sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    int nearMara = 0, nearJoren = 0, nearLina = 0, nearLamp = 0;
    if (mode == M_PLAY) { nearMara=dist(px,pz,marX,marZ)<2.7f; nearJoren=dist(px,pz,jorenX,jorenZ)<2.7f; nearLina=dist(px,pz,linaX,linaZ)<2.7f; nearLamp=dist(px,pz,ckx,ckz)<2.6f; }
    if (mode != M_TITLE) {
        rect(0, 0, 480, 34, RGBA(0,0,0,150));
        for (int i = 0; i < 5; i++) rect(8 + i * 16, 20, 12, 10, i < php ? RGBA(230,60,70,255) : RGBA(80,40,50,255));
    } else rect(110, 150, 260, 60, RGBA(0,0,0,170));
    if (mode == M_SAY || mode == M_CHOOSE) {
        rect(6, 172, 468, 94, RGBA(10,10,30,215)); rect(6, 172, 468, 2, RGBA(255,255,255,200));
        if (cur->s[0] && cur->s[0] != '*') rect(10, 158, (int)strlen(cur->s) * 8 + 12, 16, RGBA(40,40,90,230));
    }
    if (mode == M_PAUSE) rect(100, 40, 280, 190, RGBA(10,10,30,225));
    if (mode == M_DEAD) rect(0, 0, 480, 272, RGBA(0,0,0, fadeT > 1 ? 220 : (int)(fadeT * 220)));
    if (mode == M_PLAY && (nearMara || nearJoren || nearLina || nearLamp)) rect(80,236,320,16,RGBA(0,0,0,170));
    pspDebugScreenSetOffset(drawOff);
    if (mode == M_TITLE) {
        ptxt(17, 3, 0xffffffff, "A S H F A L L"); ptxt(11, 5, 0xff88ddff, "PSP 3D story adventure - Chapter 1");
        ptxt(16, 20, menuSel == 0 ? 0xff55ffff : 0xff999999, "New Game"); ptxt(16, 22, menuSel == 1 ? 0xff55ffff : 0xff999999, "Continue");
    } else if (mode != M_DEAD) {
        ptxt(1, 0, 0xff88ddff, objective());
        ptxt(1, 2, 0xffaaaaaa, "X attack/talk  Square dodge  Circle run  START pause");
        snprintf(buf, sizeof buf, "Orbs %d/5%s", orbs, hasKey ? "  Key: Lantern" : ""); ptxt(1, 1, 0xffffffff, buf);
        if (msgT > 0) ptxt(2, 5, 0xff66ffff, msg);
        if (nearMara) ptxt(15,30,0xff66ffff,"X: Talk to Mara"); else if (nearJoren) ptxt(15,30,0xff66ffff,"X: Talk to Joren"); else if (nearLina) ptxt(15,30,0xff66ffff,"X: Talk to Lina"); else if (nearLamp && quest==Q_LAMP) ptxt(10,30,0xff66ffff,"X: Inspect broken lamp"); else if (nearLamp) ptxt(13,30,0xff66ffff,"X: Rest at lamp (save)");
        if (mode == M_SAY || mode == M_CHOOSE) {
            const char *sp = cur->s;
            if (mode == M_CHOOSE) {
                ptxt(2, 23, 0xffcccccc, "Choose:");
                ptxt(2, 25, sel == 0 ? 0xff55ffff : 0xff999999, sel == 0 ? "> " : "  "); ptxt(4, 25, sel == 0 ? 0xff55ffff : 0xff999999, optA);
                ptxt(2, 27, sel == 1 ? 0xff55ffff : 0xff999999, sel == 1 ? "> " : "  "); ptxt(4, 27, sel == 1 ? 0xff55ffff : 0xff999999, optB);
            } else {
                if (sp[0]) ptxt(2, 20, !strcmp(sp, "Kai") ? 0xffffd296 : 0xff96dcff, sp);
                wrapPrint(cur->t, (int)dchars, 23, sp[0] ? 0xffffffff : 0xffe0e0e0);
                if (dchars >= (float)strlen(cur->t)) ptxt(56, 31, 0xffaaaaaa, "[X]");
            }
        }
        if (mode == M_PAUSE) {
            ptxt(25, 7, 0xffffffff, "PAUSED");
            static const char *it[4] = {"Resume", "Save", "Load", "Quit to title"};
            for (int i = 0; i < 4; i++) ptxt(16, 9 + i, menuSel == i ? 0xff55ffff : 0xff999999, it[i]);
            snprintf(buf, sizeof buf, "Objective: %s", objective()); ptxt(14, 15, 0xff88ddff, buf);
            snprintf(buf, sizeof buf, "Health %d/5   Orbs %d/5", php, orbs); ptxt(14, 17, 0xffffffff, buf);
            ptxt(14, 19, 0xffffffff, "Inventory:"); ptxt(16, 20, 0xffcccccc, hasKey ? "- Lantern Key" : "- (empty)");
            snprintf(buf, sizeof buf, "Play time: %u min", playF / 1800); ptxt(14, 23, 0xff999999, buf);
        }
    } else {
        ptxt(20, 14, 0xff6666ff, "YOU FELL"); if (fadeT > 1.5f) ptxt(14, 17, 0xffcccccc, "X: return to last checkpoint");
    }
    /* Finish this frame before presenting it. sceGuSwapBuffers() returns
     * the new draw-buffer offset; keep the debug console on that same
     * buffer so text and GU rendering are never split across buffers. */
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    drawOff = (int)sceGuSwapBuffers();
    sceDisplayWaitVblankStart();   /* lock to 30 FPS */
}

static void initGu(void) {
    sceGuInit(); sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BW); sceGuDispBuffer(SW, SH, (void *)FS, BW); sceGuDepthBuffer((void *)(FS * 2), BW);
    sceGuOffset(2048 - (SW / 2), 2048 - (SH / 2)); sceGuViewport(2048, 2048, SW, SH); sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SW, SH); sceGuEnable(GU_SCISSOR_TEST); sceGuDepthFunc(GU_GEQUAL); sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW); sceGuShadeModel(GU_SMOOTH); sceGuDisable(GU_CULL_FACE);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB); sceGuTexFilter(GU_LINEAR, GU_LINEAR); sceGuTexWrap(GU_REPEAT, GU_REPEAT);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

int main(void) {
    int th = sceKernelCreateThread("cb", cbThread, 0x11, 0xFA0, 0, 0); if (th >= 0) sceKernelStartThread(th, 0, 0);
    pspDebugScreenInit(); pspDebugScreenEnableBackColor(0);
    sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    arena = memalign(16, ARENA * sizeof(TV));
    texCloth = mkTex(3); texGrass = mkTex(0); texStone = mkTex(1); texWall = mkTex(2);
    buildLevel();
    hHero = mkHum("hero", RGB(70,120,210), RGB(60,60,80), RGB(235,190,160), RGB(110,70,40), RGB(20,20,20));
    hMara = mkHum("mara", RGB(170,70,70), RGB(70,55,55), RGB(225,185,155), RGB(210,210,215), RGB(20,20,20));
    hShade = mkHum("shade", RGB(70,50,110), RGB(50,35,80), RGB(90,80,120), RGB(40,30,60), RGB(255,60,60));
    sceKernelDcacheWritebackAll();
    initGu(); newGame(); mode = M_TITLE;
    SceCtrlData pad; unsigned old = 0;
    while (running) {
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~old; old = pad.Buttons;
        update(&pad, pressed); render(frames * DT);
    }
    sceGuTerm(); sceKernelExitGame();
    return 0;
}

/* ASSETS (optional, picked up automatically; power-of-two TGA <= 256, uncompressed 24/32-bit):
 *   assets/level.obj + assets/level.tga           whole location as one mesh (replaces buildings/plaza)
 *   assets/hero_torso.obj / _head / _arm / _leg   per-character rigid parts (same for mara_*, shade_*)
 *   assets/hero.tga, mara.tga, shade.tga          character textures
 * Export OBJ with normals + UVs, triangulated, ~2-4k tris per location chunk, ~500-800 per character.
 */
