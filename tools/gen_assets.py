#!/usr/bin/env python3
"""Ashfall PSP Chapter 1 procedural town/asset generator.
Usage: python3 tools/gen_assets.py [assets_dir=assets] [icon_dir=.]
"""
import math, os, struct, sys, zlib
from pathlib import Path

OUT = sys.argv[1] if len(sys.argv) > 1 else "assets"
ICO = sys.argv[2] if len(sys.argv) > 2 else "."
os.makedirs(OUT, exist_ok=True); os.makedirs(ICO, exist_ok=True)

class Img:
    def __init__(self,w,h,c=(0,0,0)): self.w=w; self.h=h; self.p=[c]*(w*h)
    def set(self,x,y,c):
        if 0<=x<self.w and 0<=y<self.h: self.p[y*self.w+x]=c
    def rect(self,x0,y0,x1,y1,c):
        for y in range(max(0,int(y0)),min(self.h,int(y1)+1)):
            for x in range(max(0,int(x0)),min(self.w,int(x1)+1)): self.p[y*self.w+x]=c
    def line(self,x0,y0,x1,y1,c):
        x0,y0,x1,y1=map(int,(x0,y0,x1,y1)); dx=abs(x1-x0); dy=abs(y1-y0); sx=1 if x0<x1 else -1; sy=1 if y0<y1 else -1; e=dx-dy
        while True:
            self.set(x0,y0,c)
            if x0==x1 and y0==y1: break
            q=2*e
            if q>-dy: e-=dy; x0+=sx
            if q<dx: e+=dx; y0+=sy
    def save_tga(self,path):
        with open(path,"wb") as f:
            h=bytearray(18); h[2]=2; h[12:14]=struct.pack("<H",self.w); h[14:16]=struct.pack("<H",self.h); h[16]=24; h[17]=0x20; f.write(h)
            for r,g,b in self.p: f.write(bytes((b,g,r)))
    def save_png(self,path):
        def ch(k,d): return struct.pack(">I",len(d))+k+d+struct.pack(">I",zlib.crc32(k+d)&0xffffffff)
        raw=bytearray()
        for y in range(self.h):
            raw.append(0)
            for r,g,b in self.p[y*self.w:(y+1)*self.w]: raw += bytes((r,g,b))
        out=b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",self.w,self.h,8,2,0,0,0))+ch(b"IDAT",zlib.compress(bytes(raw),9))+ch(b"IEND",b"")
        Path(path).write_bytes(out)

def noise(x,y,s=0):
    n=(x*374761393+y*668265263+s*1442695041)&0xffffffff; n^=n>>13; n=(n*1274126177)&0xffffffff; n^=n>>16; return (n&255)-128
def clamp(v): return max(0,min(255,int(v)))

def level_texture():
    im=Img(256,256,(100,80,70))
    R={"stone":(0,0,64,64),"cobble":(64,0,128,64),"plaster":(128,0,192,64),"wood":(192,0,256,64),
       "roof":(0,64,64,128),"door":(64,64,96,128),"window":(96,64,128,128),"grass":(128,64,192,128),
       "dirt":(192,64,256,128),"water":(0,128,64,192),"brick":(64,128,128,192),"plank":(128,128,192,192),
       "dark":(192,128,256,192),"trim":(0,192,64,256),"metal":(64,192,128,256),"sign":(128,192,192,256),
       "path":(192,192,256,256)}
    for name,(x0,y0,x1,y1) in R.items():
        for y in range(y0,y1):
            for x in range(x0,x1):
                n=noise(x,y,len(name))//18
                if name=="stone": c=(65,66,68) if x%16==0 or y%12==0 else (145+n,145+n,140+n)
                elif name=="cobble": c=(65,65,67) if x%16==0 or y%8==0 else (125+n,120+n,113+n)
                elif name=="plaster": c=(202+n,190+n,168+n)
                elif name=="wood": c=(105+n,65+n,40+n)
                elif name=="roof": c=(65,43,39) if y%7==0 or x%18==0 else (124+n,62+n//2,49+n//2)
                elif name=="door": c=(60+n,40+n,28+n)
                elif name=="window": c=(70,54,43) if x%14<2 or y%16<2 else (70,105,125)
                elif name=="grass": c=(62+n,101+n,54+n)
                elif name=="dirt": c=(124+n,91+n,59+n)
                elif name=="water": c=(48,130+(8 if (x+y*2)%17<3 else 0),177)
                elif name=="brick": c=(78,62,55) if y%8==0 or x%16==0 else (150,90,68)
                elif name=="plank": c=(60,42,30) if y%9==0 else (116+n,77+n,43+n)
                elif name=="dark": c=(47+n,48+n,53+n)
                elif name=="trim": c=(72,47,31) if x%13<3 else (126,79,42)
                elif name=="metal": c=(91+n,96+n,101+n)
                elif name=="sign": c=(160,125,68) if (x//6+y//7)%2 else (135,95,50)
                else: c=(150+n,133+n,111+n)
                im.set(x,y,c)
    return im

class Mesh:
    def __init__(self): self.v=[]; self.uv=[]; self.n=[]; self.f=[]
    def tri(self,a,b,c,ta,tb,tc,n):
        base=len(self.v)+1
        for p,t in ((a,ta),(b,tb),(c,tc)): self.v.append(p); self.uv.append(t); self.n.append(n)
        self.f.append((base,base+1,base+2))
    def quad(self,a,b,c,d,t,n):
        self.tri(a,b,c,t[0],t[1],t[2],n); self.tri(a,c,d,t[0],t[2],t[3],n)
    def write(self,path):
        with open(path,"w") as f:
            for x,y,z in self.v:f.write(f"v {x:.4f} {y:.4f} {z:.4f}\n")
            for u,v in self.uv:f.write(f"vt {u:.6f} {v:.6f}\n")
            for x,y,z in self.n:f.write(f"vn {x:.4f} {y:.4f} {z:.4f}\n")
            for q in self.f:f.write("f "+" ".join(f"{i}/{i}/{i}" for i in q)+"\n")

def uv(name):
    R={"stone":(0,0,64,64),"cobble":(64,0,128,64),"plaster":(128,0,192,64),"wood":(192,0,256,64),
       "roof":(0,64,64,128),"door":(64,64,96,128),"window":(96,64,128,128),"grass":(128,64,192,128),
       "dirt":(192,64,256,128),"water":(0,128,64,192),"brick":(64,128,128,192),"plank":(128,128,192,192),
       "dark":(192,128,256,192),"trim":(0,192,64,256),"metal":(64,192,128,256),"sign":(128,192,192,256),"path":(192,192,256,256)}
    x0,y0,x1,y1=R[name]; a,b=(x0+1)/256,(x1-1)/256; c,d=(y0+1)/256,(y1-1)/256
    return [(a,1-c),(b,1-c),(b,1-d),(a,1-d)]

def box(m,x,y,z,hx,hy,hz,t="plaster"):
    X0,X1=x-hx,x+hx; Y0,Y1=y-hy,y+hy; Z0,Z1=z-hz,z+hz; q=uv(t)
    m.quad((X0,Y0,Z0),(X1,Y0,Z0),(X1,Y1,Z0),(X0,Y1,Z0),q,(0,0,-1))
    m.quad((X1,Y0,Z1),(X0,Y0,Z1),(X0,Y1,Z1),(X1,Y1,Z1),q,(0,0,1))
    m.quad((X0,Y0,Z1),(X0,Y0,Z0),(X0,Y1,Z0),(X0,Y1,Z1),q,(-1,0,0))
    m.quad((X1,Y0,Z0),(X1,Y0,Z1),(X1,Y1,Z1),(X1,Y1,Z0),q,(1,0,0))
    m.quad((X0,Y1,Z0),(X1,Y1,Z0),(X1,Y1,Z1),(X0,Y1,Z1),q,(0,1,0))
    m.quad((X0,Y0,Z1),(X1,Y0,Z1),(X1,Y0,Z0),(X0,Y0,Z0),q,(0,-1,0))

def cylinder(m,x,y,z,r,h,seg=8,t="wood"):
    q=uv(t)
    for i in range(seg):
        a=2*math.pi*i/seg; b=2*math.pi*(i+1)/seg
        p0=(x+math.cos(a)*r,y,z+math.sin(a)*r); p1=(x+math.cos(b)*r,y,z+math.sin(b)*r)
        p2=(p1[0],y+h,p1[2]); p3=(p0[0],y+h,p0[2]); n=(math.cos((a+b)/2),0,math.sin((a+b)/2))
        m.quad(p0,p1,p2,p3,q,n)

def cone(m,x,y,z,r,h,seg=8,t="grass"):
    q=uv(t); top=(x,y+h,z)
    for i in range(seg):
        a=2*math.pi*i/seg; b=2*math.pi*(i+1)/seg
        p=(x+math.cos(a)*r,y,z+math.sin(a)*r); p2=(x+math.cos(b)*r,y,z+math.sin(b)*r)
        m.tri(p,p2,top,q[0],q[1],q[2],(math.cos((a+b)/2),r/h,math.sin((a+b)/2)))

def roof(m,x,y,z,hx,hz,h):
    # two sloped roof planes and end caps
    X0,X1=x-hx,x+hx; Z0,Z1=z-hz,z+hz; RY=y+h; q=uv("roof")
    m.quad((X0,y,Z0),(X1,y,Z0),(X1,RY,Z0),(X0,RY,Z0),q,(0,-.6,-.8))
    m.quad((X1,y,Z1),(X0,y,Z1),(X0,RY,Z1),(X1,RY,Z1),q,(0,-.6,.8))
    m.quad((X0,y,Z0),(X0,RY,Z0),(X0,RY,Z1),(X0,y,Z1),q,(-1,.2,0))
    m.quad((X1,y,Z1),(X1,RY,Z1),(X1,RY,Z0),(X1,y,Z0),q,(1,.2,0))

BLD=[(-16,-24,5,4,4),(-4,-25,4,3.5,5.5),(12,-24,6,4,4.5),(-27,-6,3.5,5,4),(27,-4,3.5,6,5),(-14,25,5,3.5,3.5),(14,26,5,3.5,4)]

def building(m,x,z,hx,hz,h,i):
    box(m,x,h/2,z,hx,h/2,hz,"plaster")
    for xx in (x-hx+.16,x+hx-.16): box(m,xx,h/2,z,.13,h/2+.03,hz+.04,"wood")
    for zz in (z-hz+.14,z+hz-.14): box(m,x,h/2,zz,hx,h/2+.03,.13,"wood")
    box(m,x,h*.48,z,hx+.03,.1,hz+.05,"wood")
    roof(m,x,h+.03,z,hx+.55,hz+.55,1.25)
    doorz=z+hz+.05 if z<0 else z-hz-.05
    box(m,x,.95,doorz,.65,.95,.08,"door")
    for dx in (-hx*.48,hx*.48):
        wz=z+(-hz-.06 if z<0 else hz+.06); box(m,x+dx,2.15,wz,.42,.48,.07,"window")
    for side in (-1,1):
        wx=x+side*(hx+.05)
        for dz in (-hz*.45,hz*.45): box(m,wx,2.15,z+dz,.07,.48,.42,"window")
    box(m,x,2.2,doorz-(.1 if z<0 else -.1),.85,.07,.22,"wood")
    if i%2==0: box(m,x+hx*.45,h+1.1,z+hz*.15,.28,.65,.28,"brick")
    box(m,x,.15,z,hx+.15,.15,hz+.15,"dark")

def tree(m,x,z,s=1):
    cylinder(m,x,0,z,.28*s,2.3*s,7,"wood"); cone(m,x,2*s,z,1.15*s,2.4*s,8,"grass"); cone(m,x,3.1*s,z,.78*s,1.5*s,8,"grass")
def lamp(m,x,z):
    cylinder(m,x,0,z,.1,3.25,7,"metal"); box(m,x,3.28,z,.36,.18,.36,"dark"); box(m,x+.22,3.05,z,.28,.07,.07,"metal"); cylinder(m,x+.47,2.93,z,.16,.28,7,"metal")
def fence(m,x0,z0,x1,z1):
    dx=x1-x0; dz=z1-z0; L=max(1,int(math.sqrt(dx*dx+dz*dz)/.9))
    for i in range(L+1):
        t=i/L; cylinder(m,x0+dx*t,0,z0+dz*t,.07,1.05,6,"wood")
    if abs(dx)>abs(dz):
        box(m,(x0+x1)/2,.52,(z0+z1)/2,abs(dx)/2,.06,.07,"wood"); box(m,(x0+x1)/2,.88,(z0+z1)/2,abs(dx)/2,.06,.07,"wood")
    else:
        box(m,(x0+x1)/2,.52,(z0+z1)/2,.07,.06,abs(dz)/2,"wood"); box(m,(x0+x1)/2,.88,(z0+z1)/2,.07,.06,abs(dz)/2,"wood")
def stall(m,x,z):
    box(m,x,.65,z,1.4,.08,.55,"wood"); box(m,x-1.15,1.25,z,.08,1.25,.5,"wood"); box(m,x+1.15,1.25,z,.08,1.25,.5,"wood")
    box(m,x,2.15,z,1.55,.08,.68,"plank"); box(m,x,1.55,z,.95,.18,.38,"sign")

def fountain(m):
    for r,y,tex in ((3,.18,"dark"),(2.55,.32,"stone")):
        seg=16
        for i in range(seg):
            a=2*math.pi*i/seg; x=math.cos(a)*r; z=math.sin(a)*r; box(m,x,y,z,.42,.25,.42,tex)
    cylinder(m,0,.34,0,2.25,.06,16,"water"); cylinder(m,0,.42,0,.42,1.1,10,"stone"); cone(m,0,1.42,0,.72,.45,10,"stone"); cylinder(m,0,1.82,0,.1,.35,8,"water")

def gate(m,x,z):
    box(m,x-3,3,z,.18,3,.18,"brick"); box(m,x+3,3,z,.18,3,.18,"brick"); box(m,x,6,z,3.3,.18,.25,"brick"); box(m,x,2,z,.18,2,.18,"metal")

def build_level():
    m=Mesh(); box(m,0,-.1,0,35,.1,35,"grass"); box(m,0,.01,0,14,.025,12,"stone")
    for args in [(0,.005,-19,13,.02,4,"cobble"),(0,.005,19,13,.02,4,"cobble"),(-21,.005,0,4,.02,15,"cobble"),(21,.005,0,4,.02,15,"cobble")]: box(m,*args)
    for args in [(-8,.006,8,2.2,.025,9,"path"),(8,.006,8,2.2,.025,9,"path"),(-8,.006,-8,2.2,.025,9,"path"),(8,.006,-8,2.2,.025,9,"path")]: box(m,*args)
    for i,b in enumerate(BLD): building(m,*b,i)
    fountain(m)
    for x,z in [(-10,-1),(-10,3),(10,2),(10,-2)]: stall(m,x,z)
    for x,z in [(-12,-5),(-11,-5.8),(-10,-5.4),(12,5),(12.8,5.5),(11.7,6.1),(-22,5),(-22,7),(22,6),(22,8)]: box(m,x,.35,z,.38,.35,.38,"wood")
    for x,z in [(-13,-6.5),(13,6.7),(-24,2),(24,2)]: cylinder(m,x,0,z,.38,.9,8,"wood")
    fence(m,-31,-12,-31,12); fence(m,31,-12,31,12); fence(m,-25,31,25,31); fence(m,-25,-31,25,-31)
    gate(m,0,-31); gate(m,0,31)
    for x,z,s in [(-24,12,.85),(-23,-17,.8),(24,14,.9),(22,-19,.8),(-9,17,.75),(9,19,.8),(-25,20,.9),(25,22,.9),(-29,-20,.75),(-30,20,.85),(29,-16,.8),(28,22,.8)]: tree(m,x,z,s)
    for x,z in [(9,-9),(-9,-9),(-9,9),(9,9),(0,-15),(0,15),(-15,0),(15,0)]: lamp(m,x,z)
    for x,z in [(-6,10),(6,10),(-6,-10),(6,-10)]: box(m,x,.22,z,1,.22,.55,"stone"); box(m,x,.42,z,.75,.18,.4,"dirt")
    for x,z,r,h in [(-33,-20,4,4),(-32,2,5,5),(-33,25,5,5),(33,-20,5,5),(33,4,4,4),(33,24,5,5)]: cone(m,x,0,z,r,h,8,"grass")
    for i in range(7): box(m,-3+i*.9,.14,27,.4,.14,1.8,"plank")
    m.write(os.path.join(OUT,"level.obj")); return len(m.f)

def char_texture(name,shirt,pants,skin,accent):
    im=Img(128,128,(40,40,45)); im.rect(4,4,60,60,shirt); im.rect(66,4,124,60,pants); im.rect(4,66,60,124,skin); im.rect(66,66,124,124,accent)
    for y in range(128):
        for x in range(128):
            if (x+y)%11==0:
                r,g,b=im.p[y*128+x]; im.p[y*128+x]=(clamp(r-8),clamp(g-8),clamp(b-8))
    im.save_tga(os.path.join(OUT,name+".tga"))

def char_parts(prefix):
    a=Mesh(); box(a,0,.36,0,.25,.36,.16,"plaster"); a.write(os.path.join(OUT,prefix+"_torso.obj"))
    a=Mesh(); cylinder(a,0,0,0,.17,.28,8,"plaster"); a.write(os.path.join(OUT,prefix+"_head.obj"))
    a=Mesh(); cylinder(a,0,-.35,0,.065,.70,7,"plaster"); a.write(os.path.join(OUT,prefix+"_arm.obj"))
    a=Mesh(); cylinder(a,0,-.44,0,.085,.88,7,"dark"); a.write(os.path.join(OUT,prefix+"_leg.obj"))

def icons():
    # PSP ICON0.PNG: 144x80. Keep the artwork high-contrast so it is
    # readable in PPSSPP's small game-list tile.
    im=Img(144,80,(18,20,32))

    # dusk gradient bands
    for y in range(80):
        r=18 + y//10
        gg=20 + y//12
        b=32 + y//5
        im.rect(0,y,143,y,(r,gg,b))

    # distant town silhouette
    for x,h in [(5,18),(20,26),(37,21),(54,31),(74,23),(92,29),(112,20),(130,27)]:
        im.rect(x,58-h,x+10,58,(48,40,48))
        im.rect(x+2,55-h,x+8,58-h,(108,62,48))

    # central lantern / flame emblem
    im.rect(61,24,83,27,(196,154,76))
    im.rect(64,28,80,54,(112,72,44))
    im.rect(67,31,77,51,(32,43,57))
    im.rect(69,33,75,48,(216,170,83))
    im.rect(70,35,74,46,(246,204,105))
    im.rect(68,49,76,53,(71,47,34))
    im.rect(68,20,76,24,(142,98,54))
    im.rect(71,16,73,20,(205,157,74))

    # small "A" mark
    im.line(65,63,72,56,(214,171,91))
    im.line(72,56,79,63,(214,171,91))
    im.line(68,61,76,61,(214,171,91))

    # title bars
    im.rect(8,7,136,10,(111,73,56))
    im.rect(8,13,58,15,(193,137,72))
    im.rect(8,17,43,19,(66,75,91))

    im.save_png(os.path.join(ICO,"ICON0.PNG"))

    bg=Img(480,272,(38,34,48)); bg.rect(0,185,479,271,(48,44,51))
    for x,w,h in [(20,70,80),(105,80,100),(205,65,70),(285,90,110),(390,75,85)]:
        bg.rect(x,185-h,x+w,185,(74,57,55)); bg.rect(x+8,173-h,x+w-8,185-h,(104,61,50))
    bg.rect(0,204,479,210,(96,78,64)); bg.line(240,205,240,95,(184,146,82)); bg.line(240,95,180,145,(184,146,82)); bg.line(240,95,300,145,(184,146,82)); bg.rect(218,111,262,152,(58,75,91)); bg.rect(225,103,255,112,(113,76,57)); bg.rect(40,42,440,47,(118,77,57)); bg.save_png(os.path.join(ICO,"PIC1.PNG"))

def main():
    print("Generating detailed Ashfall Chapter 1 town...")
    level_texture().save_tga(os.path.join(OUT,"level.tga"))
    print("level.obj triangles:",build_level())
    char_texture("hero",(70,120,210),(60,60,80),(235,190,160),(110,70,40))
    char_texture("mara",(170,70,70),(70,55,55),(225,185,155),(210,210,215))
    char_texture("shade",(70,50,110),(50,35,80),(90,80,120),(40,30,60))
    for p in ("hero","mara","shade"): char_parts(p)
    icons()
    print("Assets:",os.path.abspath(OUT)); print("Done.")

if __name__=="__main__": main()
