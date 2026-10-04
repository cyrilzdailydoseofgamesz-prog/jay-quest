#!/usr/bin/env python3
"""Ashfall asset generator (Python 3 standard library only).

Usage: python3 tools/gen_assets.py [assets_dir=assets] [icon_dir=.]

Writes, for main.c to pick up at runtime:
  assets/level.obj  + level.tga      town square (plaza, houses, roofs, fountain, props)
  assets/hero_{torso,head,arm,leg}.obj + hero.tga     (same for mara_*, shade_*)
and ICON0.PNG / PIC1.PNG for the EBOOT.
Everything is procedural, so edit the numbers below and re-run to restyle.
"""
import math, os, struct, sys, zlib

OUT = sys.argv[1] if len(sys.argv) > 1 else "assets"
ICO = sys.argv[2] if len(sys.argv) > 2 else "."
os.makedirs(OUT, exist_ok=True)
os.makedirs(ICO, exist_ok=True)
TAU = math.tau


def clamp(v, a=0, b=255):
    return a if v < a else b if v > b else v


# ------------------------------------------------------------------ images
class Img:
    def __init__(s, w, h, fill=(0, 0, 0)):
        s.w, s.h = w, h
        s.p = [[fill] * w for _ in range(h)]

    def paint(s, x0, y0, w, h, fn):
        for v in range(h):
            row = s.p[y0 + v]
            for u in range(w):
                r, g, b = fn(u, v)
                row[x0 + u] = (clamp(int(r)), clamp(int(g)), clamp(int(b)))

    def tga(s, path):  # uncompressed 24-bit, bottom-left origin (what main.c's loader expects)
        with open(path, "wb") as f:
            f.write(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, s.w, s.h, 24, 0))
            for y in range(s.h - 1, -1, -1):
                f.write(b"".join(bytes((b, g, r)) for r, g, b in s.p[y]))

    def png(s, path):
        raw = b"".join(b"\x00" + b"".join(bytes(px) for px in row) for row in s.p)

        def ch(t, d):
            return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

        with open(path, "wb") as f:
            f.write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", s.w, s.h, 8, 2, 0, 0, 0))
                    + ch(b"IDAT", zlib.compress(raw, 9)) + ch(b"IEND", b""))


# ------------------------------------------------------------------ tileable noise
def hsh(*a):
    h = 2166136261
    for x in a:
        h = ((h ^ (x & 0xFFFFFFFF)) * 16777619) & 0xFFFFFFFF
    h ^= h >> 13
    h = (h * 1274126177) & 0xFFFFFFFF
    h ^= h >> 16
    return h / 4294967295.0


def tnoise(x, y, px, py, seed):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    sx, sy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = hsh(ix % px, iy % py, seed), hsh((ix + 1) % px, iy % py, seed)
    c, d = hsh(ix % px, (iy + 1) % py, seed), hsh((ix + 1) % px, (iy + 1) % py, seed)
    top = a + (b - a) * sx
    return top + ((c + (d - c) * sx) - top) * sy


def fbm(u, v, w, h, seed, base=4, octs=3):
    s = tot = 0.0
    amp, f = .5, base
    for o in range(octs):
        s += amp * tnoise(u / w * f, v / h * f, f, f, seed + o)
        tot += amp
        amp *= .5
        f *= 2
    return s / tot


# ------------------------------------------------------------------ level texture painters
def p_stone(u, v):
    row = v // 32
    uu = (u + (32 if row % 2 else 0)) % 128
    bx, lx, ly = uu // 64, uu % 64, v % 32
    e = min(lx, 63 - lx, ly, 31 - ly)
    c = (112 + hsh(bx, row, 5) * 58) * (.78 + .45 * fbm(u, v, 128, 128, 3, 8, 3))
    if e < 2:
        c = 52 + fbm(u, v, 128, 128, 4, 8, 2) * 18
    elif e < 4:
        c *= .84 + .04 * e
    return (c * 1.02, c * .99, c * .93)


def p_cobble(u, v):
    cx, cy = u // 16, v // 16
    lx = u % 16 - 8 + hsh(cx, cy, 2) * 3 - 1.5
    ly = v % 16 - 8 + hsh(cx, cy, 3) * 3 - 1.5
    d, n = math.hypot(lx * .9, ly), fbm(u, v, 128, 48, 5, 8, 3)
    if d > 7.2:
        return (40 + n * 20, 38 + n * 18, 36 + n * 15)
    c = (105 + hsh(cx, cy, 7) * 60) * (1.1 - d / 14) * (.8 + .4 * n)
    return (c, c * .97, c * .9)


def p_water(u, v):
    a = math.sin(TAU * (u / 128 * 3 + math.sin(TAU * v / 40 * 2) * .15))
    b = math.sin(TAU * (v / 40 * 2 + u / 128 * 2))
    k, n = .5 + .25 * a + .25 * b, fbm(u, v, 128, 40, 9, 4, 2)
    return (35 + 60 * k + 20 * n, 95 + 70 * k + 20 * n, 130 + 70 * k + 20 * n)


def p_planks(u, v):
    pl, lx = u // 16, u % 16
    n = fbm(u, v, 128, 40, 11, 6, 3)
    g = math.sin((lx * .7 + hsh(pl, 1, 1) * 6 + v * .12) * 2) * .5 + .5
    c = (120 + hsh(pl, 2, 2) * 40) * (.75 + .25 * g + .2 * n)
    if lx == 0 or lx == 15:
        return (30, 22, 15)
    if (v < 3 or v > 36) and lx in (3, 12):
        return (70, 70, 76)
    return (c, c * .7, c * .45)


def mk_plaster(base, beam, seed):
    def f(u, v):
        c = .82 + .3 * fbm(u, v, 128, 64, seed, 6, 4)
        c -= max(0, fbm(u, v, 128, 64, seed + 7, 3, 2) - .62) * .9
        e = min(u, 127 - u, v, 63 - v)
        if e < 5:
            g = (.8 + .3 * math.sin(u * .9 + v * .5 + seed)) * (.55 if e == 4 else 1)
            return (beam[0] * g, beam[1] * g, beam[2] * g)
        return (base[0] * c, base[1] * c, base[2] * c)
    return f


def p_roof(u, v):
    r = v // 16
    uu = (u + (8 if r % 2 else 0)) % 128
    tx, ty = uu % 16, v % 16
    n, t = fbm(u, v, 128, 64, 13, 8, 3), hsh(uu // 16, r, 4)
    sh = (1.0 - .35 * (ty / 15.0) ** 2) * (.45 if ty >= 14 else 1) * (.6 if tx == 0 else 1)
    c = (.8 + .25 * n) * sh
    col = [(150 + t * 40) * c, (62 + t * 20) * c, (42 + t * 14) * c]
    if n > .68:
        m = min(1, (n - .68) * 6)
        col = [col[0] * (1 - m) + 70 * m * sh, col[1] * (1 - m) + 95 * m * sh, col[2] * (1 - m) + 50 * m * sh]
    return tuple(col)


def p_door(u, v):
    if min(u, 63 - u, v, 63 - v) < 4:
        return (62, 40, 26)
    pl, lx = (u - 4) // 19, (u - 4) % 19
    n, g = fbm(u, v, 64, 64, 17, 6, 3), math.sin(v * .35 + hsh(pl, 3, 3) * 8) * .5 + .5
    c = (110 + hsh(pl, 1, 1) * 35) * (.75 + .2 * g + .25 * n)
    if lx == 0:
        return (30, 20, 12)
    if 13 <= v <= 17 or 45 <= v <= 49:
        return (55, 55, 62)
    if (u - 50) ** 2 + (v - 34) ** 2 <= 9:
        return (175, 150, 70)
    return (c, c * .66, c * .4)


def p_window(u, v):
    if u < 11 or u >= 53:
        sl = (v // 5) % 2
        return (60 + sl * 20, 92 + sl * 16, 74 + sl * 12) if (v % 5) else (30, 40, 34)
    if v < 4:
        return (70, 46, 30)
    if v >= 56:
        return (150, 145, 135)
    if u in (31, 32) or v in (29, 30):
        return (62, 42, 28)
    glow = max(0, 1 - math.hypot((u - 32) / 22, (v - 30) / 26))
    return (40 + 215 * glow ** 1.2, 48 + 150 * glow ** 1.6, 70 + 30 * glow)


REG = {  # name: (x, y, w, h) in a 256x256 atlas, y from the top
    "stone": (0, 0, 128, 128), "cobble": (0, 128, 128, 48), "water": (0, 176, 128, 40), "planks": (0, 216, 128, 40),
    "plasterA": (128, 0, 128, 64), "plasterB": (128, 64, 128, 64), "roof": (128, 128, 128, 64),
    "door": (128, 192, 64, 64), "window": (192, 192, 64, 64),
}


def uvr(img, name, ins=1.0):
    x, y, w, h = REG[name]
    return ((x + ins) / img.w, 1 - (y + h - ins) / img.h, (x + w - ins) / img.w, 1 - (y + ins) / img.h)  # u0,v0,u1,v1 (v up)


def level_texture():
    im = Img(256, 256, (90, 90, 90))
    for name, fn in (("stone", p_stone), ("cobble", p_cobble), ("water", p_water), ("planks", p_planks),
                     ("plasterA", mk_plaster((226, 208, 172), (92, 62, 40), 31)),
                     ("plasterB", mk_plaster((212, 168, 118), (84, 58, 40), 47)),
                     ("roof", p_roof), ("door", p_door), ("window", p_window)):
        x, y, w, h = REG[name]
        im.paint(x, y, w, h, fn)
    return im


# ------------------------------------------------------------------ mesh builder
def norm(v):
    l = math.sqrt(v[0] ** 2 + v[1] ** 2 + v[2] ** 2) or 1.0
    return (v[0] / l, v[1] / l, v[2] / l)


class Mesh:
    def __init__(s):
        s.vl, s.tl, s.nl, s.vd, s.td, s.nd, s.f = [], [], [], {}, {}, {}, []

    @staticmethod
    def _i(d, l, k):
        if k not in d:
            l.append(k)
            d[k] = len(l)
        return d[k]

    def tri(s, p, t, n):
        idx = []
        for k in range(3):
            pk = tuple(round(c, 4) for c in p[k])
            tk = (round(t[k][0], 4), round(t[k][1], 4))
            nk = tuple(round(c, 3) for c in norm(n[k]))
            idx.append((s._i(s.vd, s.vl, pk), s._i(s.td, s.tl, tk), s._i(s.nd, s.nl, nk)))
        s.f.append(idx)

    def quad(s, p, t, n):
        if isinstance(n[0], (int, float)):
            n = [n] * 4
        s.tri((p[0], p[1], p[2]), (t[0], t[1], t[2]), (n[0], n[1], n[2]))
        s.tri((p[0], p[2], p[3]), (t[0], t[2], t[3]), (n[0], n[2], n[3]))

    def grid(s, o, du, dv, nu, nv, reg, n):
        u0, v0, u1, v1 = reg
        for j in range(nv):
            for i in range(nu):
                a, b, c, d = i / nu, (i + 1) / nu, j / nv, (j + 1) / nv
                P = lambda x, y: tuple(o[k] + du[k] * x + dv[k] * y for k in range(3))
                s.quad([P(a, c), P(b, c), P(b, d), P(a, d)], [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], n)

    def save(s, path, name):
        with open(path, "w") as f:
            f.write("# Ashfall generated mesh\no %s\n" % name)
            for v in s.vl: f.write("v %.4f %.4f %.4f\n" % v)
            for t in s.tl: f.write("vt %.4f %.4f\n" % t)
            for n in s.nl: f.write("vn %.3f %.3f %.3f\n" % n)
            for tri in s.f: f.write("f " + " ".join("%d/%d/%d" % c for c in tri) + "\n")
        return len(s.f)


def lathe(m, prof, seg, reg, i0=0, i1=None, sz=1.0, off=(0, 0, 0), vflip=False):
    """Surface of revolution around +Y. prof=[(radius,y),...]. Rings i0..i1+1 are emitted, normals use the full profile."""
    n = len(prof)
    i1 = n - 2 if i1 is None else i1
    u0, v0, u1, v1 = reg
    nrm = []
    for i in range(n):
        a, b = max(i - 1, 0), min(i + 1, n - 1)
        dr, dy = prof[b][0] - prof[a][0], prof[b][1] - prof[a][1]
        l = math.hypot(dr, dy) or 1
        nrm.append((dy / l, -dr / l))
    cnt = i1 + 1 - i0

    def pt(i, a):
        r, y = prof[i]
        return (r * math.cos(a) + off[0], y + off[1], r * math.sin(a) * sz + off[2])

    def nm(i, a):
        nr, ny = nrm[i]
        return norm((nr * math.cos(a), ny, nr * math.sin(a) / sz))

    for i in range(i0, i1 + 1):
        va, vb = v0 + (v1 - v0) * (i - i0) / cnt, v0 + (v1 - v0) * (i + 1 - i0) / cnt
        if vflip:
            va, vb = vb, va
        for k in range(seg):
            a0, a1 = TAU * k / seg, TAU * (k + 1) / seg
            ua, ub = u0 + (u1 - u0) * k / seg, u0 + (u1 - u0) * (k + 1) / seg
            m.quad([pt(i, a0), pt(i, a1), pt(i + 1, a1), pt(i + 1, a0)],
                   [(ua, va), (ub, va), (ub, vb), (ua, vb)],
                   [nm(i, a0), nm(i, a1), nm(i + 1, a1), nm(i + 1, a0)])


def ellipsoid(m, c, rx, ry, rz, seg, rings, reg):
    prof = [(rx * math.cos(-math.pi / 2 + math.pi * i / rings) + 1e-4, ry * math.sin(-math.pi / 2 + math.pi * i / rings)) for i in range(rings + 1)]
    lathe(m, prof, seg, reg, sz=rz / rx, off=c)


# ------------------------------------------------------------------ level geometry
BLD = [(-16, -24, 5, 4, 4), (-4, -25, 4, 3.5, 5.5), (12, -24, 6, 4, 4.5), (-27, -6, 3.5, 5, 4),
       (27, -4, 3.5, 6, 5), (-14, 25, 5, 3.5, 3.5), (14, 26, 5, 3.5, 4)]   # must match BLD[] in main.c


def wall(m, A, B, h, reg, center):
    du, dv = (B[0] - A[0], 0, B[2] - A[2]), (0, h, 0)
    n = norm((du[2], 0, -du[0]))
    if n[0] * (A[0] - center[0]) + n[2] * (A[2] - center[1]) < 0:
        n = (-n[0], 0, -n[2])
    ln = math.hypot(du[0], du[2])
    m.grid(A, du, dv, max(1, round(ln / 2.5)), max(1, round(h / 2.0)), reg, n)
    return n


def decal(m, A, B, f0, f1, y0, y1, off, n, reg):
    P = lambda f, y: (A[0] + (B[0] - A[0]) * f + n[0] * off, y, A[2] + (B[2] - A[2]) * f + n[2] * off)
    u0, v0, u1, v1 = reg
    m.quad([P(f0, y0), P(f1, y0), P(f1, y1), P(f0, y1)], [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], n)


def box(m, cx, cz, hx, hz, y0, y1, reg):
    for (A, B) in (((cx - hx, cz - hz), (cx + hx, cz - hz)), ((cx + hx, cz - hz), (cx + hx, cz + hz)),
                   ((cx + hx, cz + hz), (cx - hx, cz + hz)), ((cx - hx, cz + hz), (cx - hx, cz - hz))):
        wall(m, (A[0], y0, A[1]), (B[0], y0, B[1]), y1 - y0, reg, (cx, cz))
        # (wall() sizes by height from y0; geometry starts at y0 because A,B carry y0)
    u0, v0, u1, v1 = reg
    m.quad([(cx - hx, y1, cz - hz), (cx + hx, y1, cz - hz), (cx + hx, y1, cz + hz), (cx - hx, y1, cz + hz)],
           [(u0, v0), (u1, v0), (u1, v1), (u0, v1)], (0, 1, 0))


def building(m, img, b, k):
    x, z, hx, hz, h = b
    plaster = uvr(img, "plasterA" if k % 2 == 0 else "plasterB")
    stone, cob, win, door, roof = uvr(img, "stone"), uvr(img, "cobble"), uvr(img, "window"), uvr(img, "door"), uvr(img, "roof")
    c = [(x - hx, z - hz), (x + hx, z - hz), (x + hx, z + hz), (x - hx, z + hz)]
    facing_x = abs(x) > abs(z)
    face = None
    for i in range(4):
        A, B = c[i], c[(i + 1) % 4]
        n = wall(m, (A[0], 0, A[1]), (B[0], 0, B[1]), h, plaster, (x, z))
        wall_len = math.hypot(B[0] - A[0], B[1] - A[1])
        # stone base course
        decal(m, (A[0], 0, A[1]), (B[0], 0, B[1]), 0, 1, 0, .7, .05, n, cob)
        toward = n[0] * -x + n[2] * -z  # >0 when the wall faces the plaza centre
        is_front = (abs(n[0]) > .5) == facing_x and toward > 0
        if is_front:
            face = (A, B, n)
            decal(m, (A[0], 0, A[1]), (B[0], 0, B[1]), .5 - .75 / wall_len, .5 + .75 / wall_len, 0, 2.3, .06, n, door)
            if wall_len >= 7:
                for cf in (.18, .82):
                    decal(m, (A[0], 0, A[1]), (B[0], 0, B[1]), cf - .6 / wall_len, cf + .6 / wall_len, h * .5, h * .5 + 1.3, .06, n, win)
        else:
            decal(m, (A[0], 0, A[1]), (B[0], 0, B[1]), .5 - .6 / wall_len, .5 + .6 / wall_len, h * .5, h * .5 + 1.3, .06, n, win)
    # gable roof, ridge along the longer side
    ridge_x = hx >= hz
    ha, hb = (hx, hz) if ridge_x else (hz, hx)
    rise = min(hx, hz) * .75
    W = (lambda a, y, bb: (x + a, y, z + bb)) if ridge_x else (lambda a, y, bb: (x + bb, y, z + a))
    dz, dy = hb + .55, rise + .15
    for sb in (1, -1):
        eave, top = h - .15, h + rise
        P = [W(-(ha + .55), eave, sb * dz), W(ha + .55, eave, sb * dz), W(ha + .55, top, 0), W(-(ha + .55), top, 0)]
        nrm = (0, dz, sb * dy) if ridge_x else (sb * dy, dz, 0)
        nu = max(1, round((2 * ha + 1.1) / 2.5))
        m.grid(P[0], tuple(P[1][i] - P[0][i] for i in range(3)), tuple(P[3][i] - P[0][i] for i in range(3)), nu, 2, roof, nrm)
    pl = plaster
    for sa in (1, -1):  # gable end triangles
        n = (sa, 0, 0) if ridge_x else (0, 0, sa)
        m.tri((W(sa * ha, h, -hb), W(sa * ha, h, hb), W(sa * ha, h + rise, 0)),
              ((pl[0], pl[1]), (pl[2], pl[1]), ((pl[0] + pl[2]) / 2, pl[3])), (n, n, n))
    # chimney
    ca, cb = ha * .45, hb * .25
    cx, cz = W(ca, 0, cb)[0], W(ca, 0, cb)[2]
    box(m, cx, cz, .42, .42, h + rise * .45, h + rise + 1.3, stone)
    # door step
    if face:
        A, B, n = face
        mx, mz = (A[0] + B[0]) / 2 + n[0] * .45, (A[1] + B[1]) / 2 + n[2] * .45
        box(m, mx, mz, .9 if not facing_x else .45, .45 if not facing_x else .9, 0, .2, cob)
    return face


def barrel(m, img, x, z):
    prof = [(.0, 0), (.30, 0), (.38, .25), (.40, .45), (.38, .65), (.31, .85), (.0, .85)]
    lathe(m, prof, 10, uvr(img, "planks"), off=(x, 0, z))


def fountain(m, img):
    cob, stone, water = uvr(img, "cobble"), uvr(img, "stone"), uvr(img, "water")
    basin = [(2.7, .03), (2.72, .4), (2.7, .72), (2.5, .82), (2.3, .74), (2.3, .5)]
    lathe(m, basin, 28, cob)
    lathe(m, [(2.3, .55), (1.2, .55), (.0, .55)], 28, water)
    col = [(.6, .5), (.46, .9), (.3, 1.5), (.55, 1.78), (.7, 2.0), (.2, 2.22)]
    lathe(m, col, 14, stone)
    lathe(m, [(1.1, 1.62), (1.0, 1.55), (.35, 1.5)], 18, cob)
    lathe(m, [(.0, 2.2), (.22, 2.38), (.0, 2.62)], 10, water)


def build_level():
    img = level_texture()
    m = Mesh()
    stone, cob = uvr(img, "stone"), uvr(img, "cobble")
    m.grid((-15, .03, -15), (30, 0, 0), (0, 0, 30), 12, 12, stone, (0, 1, 0))      # plaza flagstones
    for (cx, cz, hx, hz) in ((0, -15.3, 15.6, .3), (0, 15.3, 15.6, .3), (-15.3, 0, .3, 15.6), (15.3, 0, .3, 15.6)):
        box(m, cx, cz, hx, hz, 0, .22, cob)                                         # curb
    fountain(m, img)
    for k, b in enumerate(BLD):
        building(m, img, b, k)
    for (bx, bz) in ((-12.4, -20.3), (-11.5, -20.2), (9.0, -20.2), (-23.5, -9.0), (-23.4, -2.2), (23.6, 2.5), (-9.5, 22.1)):
        barrel(m, img, bx, bz)
    n = m.save(os.path.join(OUT, "level.obj"), "ashfall_level")
    img.tga(os.path.join(OUT, "level.tga"))
    return n


# ------------------------------------------------------------------ characters
CREG = {"shirt": (0, 0, 64, 64), "pants": (64, 0, 64, 64), "skin": (0, 64, 64, 64), "accent": (64, 64, 64, 64)}


def cuv(name, ins=1.0):
    x, y, w, h = CREG[name]
    return ((x + ins) / 128, 1 - (y + h - ins) / 128, (x + w - ins) / 128, 1 - (y + ins) / 128)


def weave(base, k=14, seed=1):
    def f(u, v):
        n = fbm(u, v, 64, 64, seed, 8, 3)
        w = 1 + (.07 if ((u // 2 + v // 2) % 2) else -.05) + (.04 if u % 16 == 0 else 0)
        return (base[0] * (.85 + .3 * n) * w, base[1] * (.85 + .3 * n) * w, base[2] * (.85 + .3 * n) * w)
    return f


def face_skin(base, old=False, ghost=False):
    def f(u, v):
        n = fbm(u, v, 64, 64, 21, 6, 3)
        c = [base[i] * (.94 + .12 * n) for i in range(3)]
        blob = lambda cx, cy, rx, ry: max(0, 1 - (((u - cx) / rx) ** 2 + ((v - cy) / ry) ** 2))
        if not ghost:
            k = blob(18, 38, 6, 5) + blob(46, 38, 6, 5)
            c = [c[0] + 22 * k, c[1] - 7 * k, c[2] - 6 * k]
        for sx in (-11, 11):
            k = blob(32 + sx, 28, 7.5, 5.5)
            c = [x * (1 - (.9 if ghost and k > .25 else .26 * k)) for x in c]
        k = blob(32, 36, 3, 6)
        c = [x * (1 - .22 * k) for x in c]
        if ghost:
            jag = 1 if (int(u) % 4 < 2) else .5
            k = blob(32, 44, 9, 3 * jag)
            c = [x * (1 - .95 * (k > .2)) for x in c]
        else:
            k = blob(32, 43, 7, 1.5)
            c = [c[0] * (1 - .35 * k), c[1] * (1 - .6 * k), c[2] * (1 - .55 * k)]
        if old:
            for ex in (-1, 1):
                k = blob(32 + ex * 17, 31, 2, 6) + blob(32 + ex * 9, 46, 1.5, 6)
                c = [x * (1 - .18 * k) for x in c]
        if u < 5 or u > 58:      # plain border: ears, nose and the back of the head sample here
            c = [base[i] * (.94 + .12 * n) * (.82 if ghost else 1) for i in range(3)]
        return tuple(c)
    return f


def leather(base):
    def f(u, v):
        n = fbm(u, v, 64, 64, 33, 8, 3)
        s = .35 if (v % 16 == 0 or (u % 16 == 0 and v % 16 > 12)) else 1
        return (base[0] * (.75 + .5 * n) * s, base[1] * (.75 + .5 * n) * s, base[2] * (.75 + .5 * n) * s)
    return f


def smoke(base, vein):
    def f(u, v):
        n = fbm(u, v, 64, 64, 41, 4, 4)
        w = abs(math.sin((n * 7 + u / 64 * 3 + v / 64) * math.pi))
        g = max(0, 1 - w * 7) * .9
        return (base[0] * (.5 + n) + vein[0] * g, base[1] * (.5 + n) + vein[1] * g, base[2] * (.5 + n) + vein[2] * g)
    return f


def char_texture(shirt, pants, skin, accent):
    im = Img(128, 128, (100, 100, 100))
    for name, fn in (("shirt", shirt), ("pants", pants), ("skin", skin), ("accent", accent)):
        x, y, w, h = CREG[name]
        im.paint(x, y, w, h, fn)
    return im


def head_mesh(m, rx, ry, rz, reg, seg=14, rings=10):
    """Ellipsoid head with planar face projection (front) and plain skin (back), plus nose and ears."""
    u0, v0, u1, v1 = reg
    pr = [(rx * math.cos(-math.pi / 2 + math.pi * i / rings) + 1e-4, ry * math.sin(-math.pi / 2 + math.pi * i / rings)) for i in range(rings + 1)]

    def P(r, y, a):
        return (r * math.cos(a), y, r * math.sin(a) * rz / rx)

    def UV(p):
        u = (p[0] / .2 + 1) / 2 if p[2] > -.01 else .04
        v = p[1] / .48 + .5
        return (u0 + (u1 - u0) * u, v0 + (v1 - v0) * v)

    def N(p):
        return norm((p[0] / rx ** 2, p[1] / ry ** 2, p[2] / rz ** 2))

    for i in range(rings):
        for k in range(seg):
            a0, a1 = TAU * k / seg, TAU * (k + 1) / seg
            q = [P(*pr[i], a0), P(*pr[i], a1), P(*pr[i + 1], a1), P(*pr[i + 1], a0)]
            m.quad(q, [UV(p) for p in q], [N(p) for p in q])
    plain = (u0 + (u1 - u0) * .04, v0 + (v1 - v0) * .5)
    ellipsoid(m, (0, -.02, rz * .97), .026, .04, .034, 8, 5, (plain[0], plain[1], plain[0] + .001, plain[1] + .001))
    for sx in (-1, 1):
        ellipsoid(m, (sx * rx * .98, 0, -.01), .022, .05, .04, 8, 5, (plain[0], plain[1], plain[0] + .001, plain[1] + .001))


def arm_mesh(m, r0, length, sleeve, skin_reg, cuff_reg, claws=False):
    ys = [0, -.2, -.4, -length * .8, -length * .8 - .03]
    sl = [(r0, 0), (r0 * 1.08, -.18), (r0 * .92, -.4), (r0 * .8, -length * .78), (r0 * .84, -length * .8)]
    lathe(m, sl, 9, sleeve)
    hand = [(r0 * .78, -length * .8), (r0 * .7, -length * .86), (r0 * .74, -length * .93), (r0 * .6, -length * .98), (.0, -length)]
    if claws:
        hand = [(r0 * .6, -length * .8), (r0 * .45, -length * .9), (.0, -length * 1.12)]
    lathe(m, hand, 8, skin_reg)


def leg_mesh(m, r0, pants, boot, wisp=False):
    if wisp:
        lathe(m, [(r0, 0), (r0 * .9, -.3), (r0 * .55, -.6), (.01, -.88)], 8, pants)
        return
    prof = [(r0, 0), (r0 * 1.04, -.25), (r0 * .78, -.55), (r0 * .74, -.64)]
    lathe(m, prof, 9, pants)
    bp = [(r0 * .86, -.62), (r0 * .98, -.7), (r0 * 1.06, -.84), (.0, -.88)]
    lathe(m, bp, 9, boot)
    ellipsoid(m, (0, -.8, .075), r0 * .85, .06, .12, 8, 5, boot)


def make_char(name, kind):
    A = lambda: Mesh()
    pants_r, shirt_r, skin_r, acc_r = cuv("pants"), cuv("shirt"), cuv("skin"), cuv("accent")
    t = A()
    if kind == "hero":
        prof = [(.27, -.06), (.25, .04), (.26, .2), (.29, .4), (.325, .56), (.2, .65), (.1, .68), (.085, .74)]
        lathe(t, prof, 14, shirt_r, sz=.72)
        lathe(t, [(.265, .1), (.275, .14), (.275, .18), (.265, .22)], 14, acc_r, sz=.72)  # belt
        lathe(t, [(.12, .66), (.14, .7), (.12, .75)], 10, acc_r)                                # scarf
    elif kind == "mara":
        prof = [(.42, -.55), (.36, -.3), (.28, -.02), (.26, .2), (.29, .42), (.35, .58), (.14, .66), (.09, .73)]
        lathe(t, prof, 14, pants_r, i0=0, i1=2, sz=.8)
        lathe(t, prof, 14, shirt_r, i0=3, i1=6, sz=.8)
        lathe(t, [(.3, .2), (.32, .24), (.32, .27), (.3, .3)], 14, acc_r, sz=.8)
    else:
        prof = [(.14, -.62), (.2, -.3), (.24, .0), (.27, .3), (.33, .52), (.3, .6), (.1, .66), (.07, .74)]
        lathe(t, prof, 12, shirt_r, sz=.7)
        for k in range(12):  # tattered hem
            a = TAU * k / 12
            r0 = .15
            for d in (0, .5):
                b0, b1 = a + TAU / 24 * d, a + TAU / 24 * (d + .5)
                tip = (math.cos(a + .26) * .1, -1.0 - .1 * (k % 3), math.sin(a + .26) * .1 * .7)
                p0 = (math.cos(b0) * r0, -.6, math.sin(b0) * r0 * .7)
                p1 = (math.cos(b1) * r0, -.6, math.sin(b1) * r0 * .7)
                nn = (math.cos(a), 0, math.sin(a))
                t.tri((p0, p1, tip), ((shirt_r[0], shirt_r[1]), (shirt_r[2], shirt_r[1]), ((shirt_r[0] + shirt_r[2]) / 2, shirt_r[3])), (nn, nn, nn))
    t.save(os.path.join(OUT, name + "_torso.obj"), name + "_torso")
    h = A()
    if kind == "shade":
        head_mesh(h, .15, .25, .15, skin_r)
    elif kind == "mara":
        head_mesh(h, .165, .2, .16, skin_r)
    else:
        head_mesh(h, .17, .204, .17, skin_r)
    h.save(os.path.join(OUT, name + "_head.obj"), name + "_head")
    a = A()
    if kind == "hero":
        arm_mesh(a, .075, .7, shirt_r, skin_r, acc_r)
    elif kind == "mara":
        arm_mesh(a, .068, .7, shirt_r, skin_r, acc_r)
    else:
        arm_mesh(a, .05, .9, shirt_r, acc_r, acc_r, claws=True)
    a.save(os.path.join(OUT, name + "_arm.obj"), name + "_arm")
    l = A()
    leg_mesh(l, .12 if kind != "mara" else .1, pants_r, acc_r, wisp=(kind == "shade"))
    l.save(os.path.join(OUT, name + "_leg.obj"), name + "_leg")


def build_chars():
    char_texture(weave((62, 108, 196), seed=1), weave((50, 56, 82), seed=2), face_skin((236, 192, 162)), leather((110, 72, 44))).tga(os.path.join(OUT, "hero.tga"))
    char_texture(weave((176, 66, 70), seed=3), weave((92, 70, 60), seed=4), face_skin((228, 190, 166), old=True), leather((70, 52, 40))).tga(os.path.join(OUT, "mara.tga"))
    char_texture(smoke((70, 50, 110), (120, 200, 255)), smoke((40, 30, 70), (150, 90, 255)), face_skin((150, 140, 185), ghost=True), smoke((60, 60, 80), (200, 120, 255))).tga(os.path.join(OUT, "shade.tga"))
    for nm, kd in (("hero", "hero"), ("mara", "mara"), ("shade", "shade")):
        make_char(nm, kd)


# ------------------------------------------------------------------ EBOOT icons
FONT = {"A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"], "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
        "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"], "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
        "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"]}


def text(im, s, x, y, sc, col):
    for ch in s:
        for r, row in enumerate(FONT[ch]):
            for c, bit in enumerate(row):
                if bit == "1":
                    for dy in range(sc):
                        for dx in range(sc):
                            px, py = x + c * sc + dx, y + r * sc + dy
                            if 0 <= px < im.w and 0 <= py < im.h:
                                im.p[py][px] = col
        x += 6 * sc


def scene(w, h, title_scale, title_y):
    im = Img(w, h)
    for y in range(h):
        k = y / h
        for x in range(w):
            im.p[y][x] = (int(30 + 95 * k * k + 12 * (hsh(x, y, 1) - .5)), int(24 + 55 * k * k), int(60 + 70 * k))
    mx, my, mr = int(w * .8), int(h * .22), int(h * .09)
    for y in range(h):
        for x in range(w):
            d = math.hypot(x - mx, y - my)
            if d < mr:
                im.p[y][x] = (235, 228, 200)
            elif d < mr * 2.4:
                g = (1 - (d - mr) / (mr * 1.4)) * .35
                r, gg, b = im.p[y][x]
                im.p[y][x] = (int(r + (235 - r) * g), int(gg + (225 - gg) * g), int(b + (190 - b) * g))
    base = int(h * .78)
    for x in range(w):  # town silhouette
        top = base - int(h * .1 * (hsh(x // max(8, w // 14), 9) + .35))
        for y in range(top, h):
            im.p[y][x] = (18, 14, 30)
        if hsh(x // 9, 4) > .9 and hsh(x // 9, 5) > .3:
            for y in range(top + 4, top + 8):
                for xx in range(x, x + 3):
                    if xx < w: im.p[y][xx] = (255, 200, 110)
    for lx in (int(w * .3), int(w * .62)):  # lamp glow
        ly = base - int(h * .04)
        for y in range(h):
            for x in range(w):
                d = math.hypot(x - lx, y - ly)
                if d < h * .12:
                    g = (1 - d / (h * .12)) ** 2
                    r, gg, b = im.p[y][x]
                    im.p[y][x] = (int(r + (255 - r) * g * .7), int(gg + (190 - gg) * g * .7), int(b + (90 - b) * g * .5))
    sc = title_scale
    tw = 7 * 6 * sc
    text(im, "ASHFALL", (w - tw) // 2 + 2, title_y + 2, sc, (10, 8, 20))
    text(im, "ASHFALL", (w - tw) // 2, title_y, sc, (255, 236, 190))
    return im


def build_icons():
    scene(144, 80, 3, 28).png(os.path.join(ICO, "ICON0.PNG"))
    scene(480, 272, 8, 70).png(os.path.join(ICO, "PIC1.PNG"))


if __name__ == "__main__":
    n = build_level()
    print("level.obj: %d triangles" % n)
    build_chars()
    build_icons()
    print("assets written to", OUT, "| icons to", ICO)
