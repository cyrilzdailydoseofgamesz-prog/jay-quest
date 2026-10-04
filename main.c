/* ECHOES OF HOLLOW BAY - PSP story adventure (pspdev SDK, runs on PPSSPP)
 * 3D blocky characters via sceGu, dialogue engine, choices, 5 chapters, 3 endings.
 * Controls: stick/D-pad move, L/R camera, O run, X talk/advance, START title.
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

PSP_MODULE_INFO("Echoes of Hollow Bay", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define BW 512
#define SW 480
#define SH 272
#define FS (BW * SH * 4)
#define RGB(r,g,b) (0xff000000u | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))
#define RGBA(r,g,b,a) (((unsigned)(a) << 24) | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))

static unsigned int __attribute__((aligned(16))) list[0x40000];
static int running = 1;

static int exitCb(int a, int b, void *c) { (void)a; (void)b; (void)c; running = 0; sceKernelExitGame(); return 0; }
static int cbThread(SceSize a, void *b) {
    (void)a; (void)b;
    int id = sceKernelCreateCallback("exit", exitCb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}

/* ---------------------------------------------------------------- 3D */
typedef struct { unsigned int c; float x, y, z; } V;
#define VT (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D)

static unsigned seed;
static float rnd(void) { seed = seed * 1664525u + 1013904223u; return ((seed >> 8) & 0xffff) / 65535.0f; }

static unsigned sh(unsigned c, int p) {
    unsigned r = (c & 255) * p / 100, g = ((c >> 8) & 255) * p / 100, b = ((c >> 16) & 255) * p / 100;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return 0xff000000u | r | (g << 8) | (b << 16);
}

static float gx, gy, gz, gs, gc, gk = 1;
static void setT(float x, float y, float z, float yaw, float k) { gx = x; gy = y; gz = z; gs = sinf(yaw); gc = cosf(yaw); gk = k; }

static void box(float ox, float oy, float oz, float hx, float hy, float hz, unsigned col) {
    static const unsigned char F[6][4] = {{0,1,3,2},{4,5,7,6},{0,2,6,4},{1,3,7,5},{0,1,5,4},{2,3,7,6}};
    static const unsigned char T[6] = {0,1,2,0,2,3};
    static const int SHD[6] = {65,65,80,80,50,100};
    float X[8], Y[8], Z[8];
    int i, f, k = 0;
    ox *= gk; oy *= gk; oz *= gk; hx *= gk; hy *= gk; hz *= gk;
    for (i = 0; i < 8; i++) {
        float lx = ox + ((i & 1) ? hx : -hx), ly = oy + ((i & 2) ? hy : -hy), lz = oz + ((i & 4) ? hz : -hz);
        X[i] = gx + lx * gc + lz * gs; Y[i] = gy + ly; Z[i] = gz - lx * gs + lz * gc;
    }
    V *v = sceGuGetMemory(36 * sizeof(V));
    for (f = 0; f < 6; f++) {
        unsigned c = sh(col, SHD[f]);
        for (i = 0; i < 6; i++) { int j = F[f][T[i]]; v[k].c = c; v[k].x = X[j]; v[k].y = Y[j]; v[k].z = Z[j]; k++; }
    }
    sceGumDrawArray(GU_TRIANGLES, VT, 36, 0, v);
}

static void person(float x, float y, float z, float yaw, unsigned sk, unsigned sht, unsigned pa, unsigned ha, unsigned ey, float w, float k) {
    float s = sinf(w) * .35f;
    setT(x, y, z, yaw, k);
    box(-.2f, .45f, s * .5f, .15f, .45f, .15f, pa); box(.2f, .45f, -s * .5f, .15f, .45f, .15f, pa);
    box(0, 1.25f, 0, .38f, .4f, .2f, sht);
    box(-.54f, 1.25f, -s * .5f, .1f, .36f, .1f, sht); box(.54f, 1.25f, s * .5f, .1f, .36f, .1f, sht);
    box(0, 1.95f, 0, .28f, .28f, .28f, sk);
    box(0, 2.2f, -.02f, .3f, .1f, .3f, ha); box(0, 1.95f, -.27f, .3f, .3f, .05f, ha);
    box(-.1f, 2.0f, .28f, .04f, .05f, .02f, ey); box(.1f, 2.0f, .28f, .04f, .05f, .02f, ey);
}

static void fox(float x, float z, float yaw, float t) {
    unsigned o = RGB(230,120,40), w = RGB(250,250,250), d = RGB(60,40,30);
    setT(x, 0, z, yaw, 1);
    box(0, .5f, 0, .22f, .2f, .5f, o); box(0, .75f, .6f, .18f, .17f, .18f, o); box(0, .7f, .82f, .07f, .06f, .06f, d);
    box(-.1f, .98f, .55f, .05f, .1f, .04f, o); box(.1f, .98f, .55f, .05f, .1f, .04f, o);
    box(0, .55f + sinf(t * 3) * .05f, -.75f, .1f, .1f, .35f, o); box(0, .55f, -1.12f, .1f, .1f, .08f, w);
    box(-.15f, .15f, .35f, .05f, .15f, .05f, d); box(.15f, .15f, .35f, .05f, .15f, .05f, d);
    box(-.15f, .15f, -.35f, .05f, .15f, .05f, d); box(.15f, .15f, -.35f, .05f, .15f, .05f, d);
}

static void shardObj(float x, float z, unsigned col, float t) {
    float b = sinf(t * 2) * .15f;
    setT(x, 0, z, t * 1.5f, 1);
    box(0, 1.3f + b, 0, .22f, .55f, .22f, col); box(0, 1.3f + b, 0, .4f, .2f, .1f, col);
    box(0, .1f, 0, .5f, .1f, .5f, RGB(80,80,100));
}
static void bookObj(float x, float z, unsigned col) {
    setT(x, 0, z, .6f, 1);
    box(0, .5f, 0, .1f, .5f, .1f, RGB(90,70,50)); box(0, 1.1f, 0, .45f, .06f, .35f, col); box(0, 1.17f, 0, .4f, .02f, .3f, RGB(240,235,210));
}
static void blob(float x, float z, float yaw, unsigned col, float t) {
    setT(x, 0, z, yaw, 1);
    float b = .1f + sinf(t * 2) * .08f;
    box(0, .5f + b, 0, .5f, .4f, .5f, col); box(-.18f, .75f + b, .5f, .07f, .07f, .03f, RGB(10,10,10)); box(.18f, .75f + b, .5f, .07f, .07f, .03f, RGB(10,10,10));
}
static void marker(float x, float y, float z, float t) {
    setT(x, y + sinf(t * 3) * .15f, z, t * 2, 1);
    box(0, 0, 0, .18f, .25f, .18f, RGB(255,230,60));
}

/* ---------------------------------------------------------------- scenes */
typedef struct { float x, z, r, v; int t; } P;
typedef struct { const char *n; unsigned sky, gr; float b, fog; } S;
static const S SC[5] = {
    {"Hollow Bay",      RGB(150,170,190), RGB(90,120,80),   30, 70},
    {"Whispering Woods",RGB(40,70,60),    RGB(30,70,40),    32, 50},
    {"Lighthouse Point",RGB(120,130,160), RGB(110,110,90),  30, 75},
    {"The Tidecaves",   RGB(8,12,30),     RGB(25,30,50),    28, 34},
    {"Point at Dawn",   RGB(235,170,140), RGB(110,130,85),  30, 90}
};

/* ---------------------------------------------------------------- story data */
typedef struct { const char *s, *t; } L;
#define E {0,0}

static const L d_intro1[] = {
{"","Hollow Bay. A fishing town at the edge of the map, and the only place Dad's last letter ever mentioned."},
{"Kai","Seven nights since the lighthouse went dark. The letter said come before the light fails. I'm late."},
{"","Fog crawls through the streets. Somewhere a bell rings, though no one is pulling the rope."},
{"","Stick: walk. L/R: camera. O: run. X: talk or advance. Look for the yellow markers."},E};
static const L d_mara[] = {
{"Mara","You have Elias's eyes. You're his kid, aren't you?"},
{"Kai","You knew my father?"},
{"Mara","Everyone did. Every autumn he climbed the tower and tended the lamp. This year he went up and never came down."},
{"Mara","Then people started vanishing. The Hendry twins, half the harbor crew. Only the fog stayed."},
{"*","I'll find them.|Not my problem, sorry."},
{"Mara","That's what he said too. Take the room upstairs if you need it. And trust the little ones, Kai. They notice things."},
{"Mara","Hm. Then why come at all? The door's open, whatever you decide."},
{"Mara","Talk to Old Tom on the pier. He saw the light die."},E};
static const L d_tom[] = {
{"Tom","Careful, lad. The fog bites after dusk."},
{"Tom","I saw it happen. The beam swung round three times, slow as a heartbeat, then burst into three pieces and scattered."},
{"Kai","Burst? A light can't burst."},
{"Tom","This one was ground from a star that fell in the bay. Three lens shards, flung far."},
{"Tom","One into the Whispering Woods. One back inside the tower. One down into the sea caves."},
{"Tom","Gather them, set them in the lamp, and maybe the bay wakes up."},
{"Kai","And if it doesn't?"},
{"Tom","Then at least you tried. That's more than most of us did."},E};
static const L d_pip[] = {
{"Pip","Are you the new person? You smell like a bus!"},
{"Kai","It WAS a bus. What are you doing out in the fog alone?"},
{"Pip","I'm not alone. Biscuit is with me."},
{"Kai","...Biscuit?"},
{"Pip","He's a ghost dog. Only I can see him. He says the woods are singing again."},
{"Pip","Biscuit says you're brave. He also says you're slow."},
{"Kai","Fair. Stay close, both of you."},E};
static const L d_bertie[] = {
{"Bertie","Counting gulls. Forty-one today. Last spring it was three hundred."},
{"Bertie","They all flew inland, toward the woods. Every last one."},
{"Bertie","Gulls know when a place is dying. Or about to be born. Same flight pattern, really."},E};
static const L d_out1[] = {
{"","By dusk Kai sets out along the old coast road, lantern swinging, a small shadow trailing behind."},
{"Kai","Woods first, then the tower, then whatever's underneath."},
{"Pip","Biscuit says the last part is a terrible idea."},
{"","Behind you, the town's bell rings once and falls silent."},E};

static const L d_intro2[] = {
{"","The Whispering Woods hum with one low note, like a wet finger on the rim of a glass."},
{"Kai","Why do I feel like the trees are talking about me?"},
{"Pip","(from far behind) Only the polite ones!"},E};
static const L d_elda[] = {
{"Elda","Visitors. How rare. And you carry the keeper's stubbornness on your shoulders."},
{"Kai","Everyone keeps saying that."},
{"Elda","The woods remember him. He came here to hide the first shard from something."},
{"Kai","From what?"},
{"Elda","The Tide Warden. It guards the caves, and it believes the light is a cage."},
{"Elda","Long ago the lighthouse did not guide ships. It held something down."},
{"Kai","Dad never told me any of this."},
{"Elda","He hoped you would never need to know. Speak with Ember. The fox holds the shard's trail."},E};
static const L d_ember[] = {
{"Ember","Sniff, sniff. Lighthouse blood. Fine, I will talk."},
{"Kai","You can speak?"},
{"Ember","I have always spoken. Humans seldom listen."},
{"Ember","The shard sleeps in the hollow oak. First, an honest answer. What matters most?"},
{"*","Saving the people.|Restoring the light."},
{"Ember","A rare answer. People first. The light will follow if the heart is right."},
{"Ember","Practical. Light first, then. Practical hearts break less, though they bend less too."},
{"Ember","The hollow oak is the one that glows. Do not dawdle."},E};
static const L d_shard1[] = {
{"","Inside the hollow oak, a shard of pale glass hums, warm as a heartbeat."},
{"Kai","One down."},
{"","SHARD 1 OF 3 RECOVERED."},E};
static const L d_stump[] = {
{"Moss","Psst. Over here. I'm a stump."},
{"Kai","I can see that."},
{"Moss","Five hundred years standing here and nobody asks how I'm doing."},
{"Kai","...How are you doing?"},
{"Moss","Damp. Thank you for asking."},E};
static const L d_out2[] = {
{"","The humming softens as you leave, almost grateful."},
{"Elda","Go carefully. The tower remembers what was done there."},
{"Kai","I'm starting to think everything here remembers more than I do."},E};

static const L d_intro3[] = {
{"","Lighthouse Point. The tower looms over the cliffs, its great lamp dark, its door hanging open."},
{"Pip","Biscuit won't go in. He says it's rude to walk into a grave."},
{"Kai","It isn't a grave."},
{"Pip","Okay. He says 'yet'."},E};
static const L d_keeper[] = {
{"Ansel","You are late, young Elias. No... no, you are his child."},
{"Kai","Who are you?"},
{"Ansel","Ansel. Keeper before your father. I stayed when the light went out."},
{"Ansel","The lamp never guided ships, Kai. It sealed the cave mouth below. Every night for four hundred years."},
{"Kai","And now it's broken."},
{"Ansel","Your father broke it. On purpose."},
{"Kai","Why would he do that?"},
{"Ansel","Because the Warden is no monster. It is the last of the bay's old guardians, and we have starved it of the sea."},
{"Ansel","The vanished townsfolk are with it, safe but sleeping. Read Elias's logbook."},E};
static const L d_log[] = {
{"","Elias's logbook, the last page, ink smudged by salt."},
{"Elias","Kai, if you are reading this, I did not make it back."},
{"Elias","I opened the seal because I could no longer live with what we did to the Warden."},
{"Elias","It will ask you a question. Answer with your own heart, not mine."},
{"Elias","Tell Mara I'm sorry about the debt. Tell yourself none of this was your fault. Love, Dad."},
{"Kai","...Dad."},
{"","Somewhere behind you, a small hand slips into yours."},E};
static const L d_shard2[] = {
{"","The second shard glows in the lamp housing, wedged between brass gears."},
{"Kai","Two."},
{"","SHARD 2 OF 3 RECOVERED."},E};
static const L d_pip3[] = {
{"Pip","Kai? Biscuit says your dad isn't dead."},
{"Pip","He says the sea took him gently. He's down there with the others."},
{"Kai","Can he really tell?"},
{"Pip","He's a dog. Dogs always know."},
{"Kai","Then let's go and get them back."},E};
static const L d_crab[] = {
{"Crab","Click. Click. I am the true keeper of this lighthouse."},
{"Kai","Of course you are."},
{"Crab","Rent is due."},
{"Kai","I am completely out of rent."},
{"Crab","Click."},E};
static const L d_out3[] = {
{"","Behind the tower, a spiral stair drops into the cliffside, wet and glowing a faint blue."},
{"Kai","That's the way down."},
{"Pip","Biscuit says he'll wait up here. Very firmly."},E};

static const L d_intro4[] = {
{"Kai","I told Pip to wait with Biscuit. This is no place for a kid."},
{"","The caves breathe. Every few seconds the walls glow, then dim, in time with something vast and slow."},
{"Kai","That sounds like a heartbeat. Great."},E};
static const L d_wisp[] = {
{"Wisp","...ssss... Hollow... Bay... was... a... harbor... for... spirits..."},
{"Kai","A harbor for spirits?"},
{"Wisp","We came... to rest... men built... the light... to keep us... from leaving..."},
{"Wisp","Warden... keeps... the door... Warden... is... tired..."},
{"Kai","Then I'll listen to it before I decide anything."},E};
static const L d_warden[] = {
{"Warden","ELIAS'S CHILD. THE LAST SHARD'S SCENT CLINGS TO YOU."},
{"Kai","Where are the townsfolk? Where is my father?"},
{"Warden","ASLEEP IN MY TIDE. SAFE. THEY CANNOT WAKE WHILE THE LIGHT STAYS BROKEN."},
{"Warden","FOUR CENTURIES YOUR PEOPLE CHAINED THE SEA TO THIS ROCK. I AM ALL THAT REMAINS OF WHAT YOU CALLED MONSTERS."},
{"*","I'll listen to you.|Give them back. Now."},
{"Warden","ELIAS DID NOT LISTEN. YOU WILL HAVE MORE TIME THAN HE DID."},
{"Warden","ANGER. HE ARRIVED WITH ANGER TOO. SEE WHERE IT LED HIM."},
{"Warden","TAKE THE THIRD SHARD FROM THE PEARL BED. THEN DECIDE WHAT THE LIGHT IS FOR."},E};
static const L d_shard3[] = {
{"","The last shard rests in a drift of glowing pearls, warm as a held breath."},
{"Kai","Three."},
{"","SHARD 3 OF 3 RECOVERED. The caves tremble, then go still."},E};
static const L d_echo[] = {
{"Echo","Echo... echo..."},
{"Kai","Hello?"},
{"Echo","Hello? Hello? Who's there? Who's there?"},
{"Kai","That is not funny."},
{"Echo","Funny. Funny. Funny."},E};
static const L d_out4[] = {
{"","You climb back to the surface as the tide rises, three shards humming in your pack."},
{"Warden","(far below) THE SEA WILL WAIT. IT HAS WAITED LONGER."},
{"Kai","No pressure, then."},E};

static const L d_intro5[] = {
{"","Dawn is a thin silver line on the water. The villagers have gathered at the tower, lanterns raised."},
{"Kai","Everyone's here. Mara, Tom, Pip... and every light in town."},E};
static const L d_mara5[] = {
{"Mara","Kai! You're alive. Whatever you did down there, the fog is thinning."},
{"Kai","The light isn't mended yet."},
{"Mara","Then mend it. We'll hold the lanterns."},
{"Mara","Your father owed me nothing, you know. I only wanted him home."},E};
static const L d_tom5[] = {
{"Tom","Forty years I've fished these waters. Never saw the tide look at me before."},
{"Tom","Whatever you choose, lad, the bay will remember it."},E};
static const L d_pip5[] = {
{"Pip","Biscuit says whatever you pick, he still thinks you're brave. And slow."},
{"Pip","...And that he's proud of you."},E};
static const L d_lens[] = {
{"","At the lamp's heart, the three shards slide together and hum as one."},
{"Kai","A seal, or a door. Dad wanted me to choose with my own heart."},
{"Ansel","Whatever you choose, it will outlast you both."},
{"*","Seal the cave again.|Open the way home."},
{"","The lamp ignites, white and cold. Far below, something vast sighs and sleeps."},
{"","The lamp ignites warm and gold, and the whole sea answers."},E};
static const L d_enda[] = {
{"","The sleepers wake by morning, blinking, bewildered, safe. Elias stands among them, older and weeping."},
{"Elias","Kai. I'm so sorry I left you the weight of it."},
{"Kai","You're here. That's all I wanted."},
{"","The bay is whole again. But on still nights Kai hears the tide asking a question no one answers."},
{"","ENDING 1 OF 3 - THE SEAL"},E};
static const L d_endb[] = {
{"","The Warden rises from the bay in a column of silver water and bows to Kai, once."},
{"Warden","THE LIGHT WILL GUIDE. IT WILL NOT CAGE. THIS IS A FAIR TIDE."},
{"","The sleepers wake on the shore. Elias runs up the beach, and Mara runs after him, furious and laughing."},
{"","The lighthouse still shines, but now it welcomes the sea instead of chaining it. The gulls return by noon."},
{"","ENDING 3 OF 3 - THE HARBOR (TRUE ENDING)"},E};
static const L d_endc[] = {
{"","The Warden slips into the open sea, grateful but wild, and the tide reclaims the harbor wall in a single night."},
{"","The sleepers wake, soaked and alive. The town must learn to live beside something it once feared."},
{"Kai","I should have listened more. Next time, I will."},
{"","ENDING 2 OF 3 - THE TIDE RETURNS"},E};

enum { D_INTRO1, D_MARA, D_TOM, D_PIP, D_BERTIE, D_OUT1,
       D_INTRO2, D_ELDA, D_EMBER, D_SHARD1, D_STUMP, D_OUT2,
       D_INTRO3, D_KEEPER, D_LOG, D_SHARD2, D_PIP3, D_CRAB, D_OUT3,
       D_INTRO4, D_WISP, D_WARDEN, D_SHARD3, D_ECHO, D_OUT4,
       D_INTRO5, D_MARA5, D_TOM5, D_PIP5, D_LENS, D_ENDA, D_ENDB, D_ENDC, D_N };

static const L *const DL[D_N] = {
    d_intro1, d_mara, d_tom, d_pip, d_bertie, d_out1,
    d_intro2, d_elda, d_ember, d_shard1, d_stump, d_out2,
    d_intro3, d_keeper, d_log, d_shard2, d_pip3, d_crab, d_out3,
    d_intro4, d_wisp, d_warden, d_shard3, d_echo, d_out4,
    d_intro5, d_mara5, d_tom5, d_pip5, d_lens, d_enda, d_endb, d_endc };

typedef struct { const char *t, *goal; int mask, intro, outro; } C;
static const C CH[5] = {
    {"Ch.1 Arrival",        "Talk to Mara, Tom and Pip. Find 3 lanterns.", 7,  D_INTRO1, D_OUT1},
    {"Ch.2 Whispering Woods","Speak to Elda and Ember. Take the shard.",   7,  D_INTRO2, D_OUT2},
    {"Ch.3 Lighthouse",     "Learn Elias's fate. Take the 2nd shard.",     15, D_INTRO3, D_OUT3},
    {"Ch.4 The Tidecaves",  "Face the Warden. Take the last shard.",       7,  D_INTRO4, D_OUT4},
    {"Ch.5 Dawn",           "Gather the village. Mend the great lens.",    15, D_INTRO5, -1}
};

enum { K_P, K_SHARD, K_BOOK, K_FOX, K_BLOB, K_GIANT };
typedef struct {
    const char *n; int ch; float x, z; unsigned sk, sht, pa, ha;
    int kind, dlg, bit, req, act; const char *rep;
} N;
#define SKA RGB(240,200,170)
#define SKB RGB(200,150,110)
#define SKC RGB(140,95,65)
static N NP[] = {
 {"Mara",0,-7,-5,SKA,RGB(180,60,60),RGB(60,50,50),RGB(90,40,30),K_P,D_MARA,0,0,0,"Talk to Tom on the pier. He saw the light die."},
 {"Old Tom",0,9,-10,SKB,RGB(60,90,140),RGB(70,70,60),RGB(210,210,210),K_P,D_TOM,1,0,0,"Mind the fog, lad. It bites."},
 {"Pip",0,-13,9,SKC,RGB(240,200,60),RGB(70,70,130),RGB(30,20,20),K_P,D_PIP,2,0,0,"Biscuit says hi! He's licking your boot."},
 {"Bertie",0,15,10,SKA,RGB(90,140,90),RGB(100,80,60),RGB(150,110,50),K_P,D_BERTIE,-1,0,0,"Forty-one gulls. Forty-one."},
 {"Elda",1,-10,-8,SKB,RGB(110,60,140),RGB(60,40,70),RGB(190,190,200),K_P,D_ELDA,0,0,0,"Speak with Ember. The fox holds the trail."},
 {"Ember",1,9,-5,0,0,0,0,K_FOX,D_EMBER,1,0,0,"The oak glows. Follow the light."},
 {"Hollow Oak",1,2,16,RGB(170,240,255),RGB(170,240,255),0,0,K_SHARD,D_SHARD1,2,2,1,"Only an empty hollow remains."},
 {"Moss",1,-16,10,0,RGB(95,65,40),0,0,K_BLOB,D_STUMP,-1,0,0,"Still damp."},
 {"Ansel",2,0,-16.5f,RGB(200,220,255),RGB(150,180,230),RGB(120,140,200),RGB(240,240,250),K_P,D_KEEPER,0,0,0,"Read Elias's logbook. Please."},
 {"Logbook",2,10,-10,0,RGB(120,80,50),0,0,K_BOOK,D_LOG,1,1,0,"You have read it twice already."},
 {"Lamp Housing",2,-12,-12,RGB(180,255,230),RGB(180,255,230),0,0,K_SHARD,D_SHARD2,2,2,1,"Gears, brass, and an empty socket."},
 {"Pip",2,6,6,SKC,RGB(240,200,60),RGB(70,70,130),RGB(30,20,20),K_P,D_PIP3,3,0,0,"Biscuit says be brave. And slow."},
 {"Crab",2,-9,8,0,RGB(200,60,50),0,0,K_BLOB,D_CRAB,-1,0,0,"Click. Rent."},
 {"Wisp",3,-8,-6,0,RGB(150,200,255),0,0,K_BLOB,D_WISP,0,0,0,"...tired... so tired..."},
 {"Tide Warden",3,0,-21,RGB(30,60,100),RGB(20,40,80),RGB(15,30,60),RGB(40,200,220),K_GIANT,D_WARDEN,1,0,0,"THE THIRD SHARD WAITS IN THE PEARLS."},
 {"Pearl Bed",3,11,-12,RGB(255,220,255),RGB(255,220,255),0,0,K_SHARD,D_SHARD3,2,2,1,"Only pearls remain."},
 {"Echo",3,-12,10,0,RGB(200,200,255),0,0,K_BLOB,D_ECHO,-1,0,0,"Echo... echo..."},
 {"Mara",4,-8,-3,SKA,RGB(180,60,60),RGB(60,50,50),RGB(90,40,30),K_P,D_MARA5,0,0,0,"We'll hold the lanterns."},
 {"Old Tom",4,8,-4,SKB,RGB(60,90,140),RGB(70,70,60),RGB(210,210,210),K_P,D_TOM5,1,0,0,"The bay will remember."},
 {"Pip",4,0,-6,SKC,RGB(240,200,60),RGB(70,70,130),RGB(30,20,20),K_P,D_PIP5,2,0,0,"Biscuit is proud of you."},
 {"Great Lens",4,0,-14,RGB(255,225,110),RGB(255,225,110),0,0,K_SHARD,D_LENS,3,7,0,"The lens waits."},
};
#define NNP ((int)(sizeof(NP) / sizeof(NP[0])))

/* ---------------------------------------------------------------- state */
enum { M_TITLE, M_PLAY, M_SAY, M_CHOOSE, M_FADE, M_CREDITS };
static int mode = M_TITLE, ch, flags, shards, trust, lensChoice, seen[32], got[5][5];
static float px, pz, pyaw, walk, cam;
static unsigned playF;

static P pr[64]; static int np; static float lanX[5], lanZ[5];

static int nearNpc(int sc, float x, float z, float d) {
    for (int i = 0; i < NNP; i++) if (NP[i].ch == sc) { float a = NP[i].x - x, b = NP[i].z - z; if (a * a + b * b < d * d) return 1; }
    return 0;
}

static void gen(int sc) {
    float b = SC[sc].b;
    seed = sc * 7919u + 13; np = 0;
    if (sc == 2 || sc == 4) { pr[np].x = 0; pr[np].z = -24; pr[np].r = 4.6f; pr[np].v = 0; pr[np].t = 3; np++; }
    for (int tries = 0; tries < 500 && np < 48; tries++) {
        float x = (rnd() * 2 - 1) * (b - 2), z = (rnd() * 2 - 1) * (b - 2), q = rnd(), r; int t;
        if (x * x + z * z < 36 || nearNpc(sc, x, z, 5.5f)) continue;
        if (sc == 0) { if (q < .3f) { t = 1; r = 3.2f; } else if (q < .8f) { t = 0; r = 1; } else { t = 2; r = 1.2f; } }
        else if (sc == 1) { if (q < .8f) { t = 0; r = 1; } else { t = 2; r = 1.2f; } }
        else if (sc == 3) { if (q < .55f) { t = 4; r = 1.2f; } else { t = 2; r = 1.4f; } }
        else { if (q < .2f) { t = 0; r = 1; } else { t = 2; r = 1.3f; } }
        int ok = 1;
        for (int i = 0; i < np; i++) { float a = pr[i].x - x, c = pr[i].z - z, m = pr[i].r + r + .5f; if (a * a + c * c < m * m) { ok = 0; break; } }
        if (!ok) continue;
        pr[np].x = x; pr[np].z = z; pr[np].r = r; pr[np].v = rnd(); pr[np].t = t; np++;
    }
    for (int i = 0; i < 16; i++) { pr[np].x = (rnd() * 2 - 1) * b; pr[np].z = (rnd() * 2 - 1) * b; pr[np].r = 1.5f + rnd() * 2.5f; pr[np].v = rnd(); pr[np].t = 5; np++; }
    for (int k = 0; k < 5; k++) {
        for (int tries = 0; tries < 200; tries++) {
            float x = (rnd() * 2 - 1) * (b - 3), z = (rnd() * 2 - 1) * (b - 3); int ok = 1;
            if (x * x + z * z < 16 || nearNpc(sc, x, z, 3)) continue;
            for (int i = 0; i < np; i++) if (pr[i].t != 5) { float a = pr[i].x - x, c = pr[i].z - z, m = pr[i].r + 1.6f; if (a * a + c * c < m * m) { ok = 0; break; } }
            if (ok) { lanX[k] = x; lanZ[k] = z; break; }
        }
    }
}

static int lantCount(int sc) { int n = 0; for (int i = 0; i < 5; i++) n += got[sc][i]; return n; }
static int lantTotal(void) { int n = 0; for (int s = 0; s < 5; s++) n += lantCount(s); return n; }

static void prop(const P *p) {
    unsigned wall;
    switch (p->t) {
    case 0: { float h = 1.6f + p->v; unsigned lc = RGB(30 + (int)(p->v * 60), 100 + (int)(p->v * 80), 40);
        if (ch == 1) lc = RGB(25 + (int)(p->v * 30), 80 + (int)(p->v * 50), 70);
        setT(p->x, 0, p->z, p->v * 6, 1);
        box(0, h * .5f, 0, .3f, h * .5f, .3f, RGB(95,65,40)); box(0, h + .8f, 0, 1.1f, .9f, 1.1f, lc); box(0, h + 1.9f, 0, .7f, .5f, .7f, lc); break; }
    case 1: wall = sh(RGB(210,190,150), 80 + (int)(p->v * 40));
        setT(p->x, 0, p->z, (int)(p->v * 4) * 1.5708f, 1);
        box(0, 1.4f, 0, 2.2f, 1.4f, 1.8f, wall); box(0, 3.2f, 0, 2.6f, .4f, 2.2f, RGB(150,50,40)); box(0, 3.9f, 0, 1.8f, .4f, 1.6f, RGB(150,50,40));
        box(0, .9f, 1.8f, .4f, .9f, .05f, RGB(70,45,30)); box(1.3f, 1.6f, 1.8f, .35f, .35f, .05f, RGB(255,230,150)); break;
    case 2: { float s = .6f + p->v * .8f; setT(p->x, 0, p->z, p->v * 6, 1);
        box(0, s * .6f, 0, s, s * .6f, s * .8f, ch == 3 ? RGB(60,65,85) : RGB(110,110,115)); box(.4f * s, s * 1.3f, 0, s * .5f, s * .4f, s * .5f, ch == 3 ? RGB(75,80,100) : RGB(130,130,135)); break; }
    case 3: setT(p->x, 0, p->z, 0, 1);
        box(0, 5, 0, 3, 5, 3, RGB(235,235,240)); box(0, 11, 0, 2.5f, 1.6f, 2.5f, RGB(200,50,50)); box(0, 14.5f, 0, 2.1f, 1.9f, 2.1f, RGB(235,235,240));
        box(0, 17, 0, 2.5f, .4f, 2.5f, RGB(60,60,70)); box(0, 18.4f, 0, 1.4f, 1.0f, 1.4f, ch == 4 ? RGB(255,235,140) : RGB(30,30,40)); box(0, 19.8f, 0, 1.9f, .4f, 1.9f, RGB(60,60,70)); break;
    case 4: setT(p->x, 0, p->z, p->v * 6, 1);
        box(0, 1.6f, 0, .4f, 1.6f, .4f, RGB(70,160,230)); box(.5f, .8f, .2f, .25f, .8f, .25f, RGB(90,190,255)); box(-.4f, .6f, -.3f, .2f, .6f, .2f, RGB(60,130,210)); break;
    case 5: setT(p->x, 0, p->z, 0, 1); box(0, .02f, 0, p->r, .02f, p->r, sh(SC[ch].gr, 80 + (int)(p->v * 45))); break;
    }
}

static void drawNPC(const N *n, float t) {
    float yaw = atan2f(px - n->x, pz - n->z);
    switch (n->kind) {
    case K_P: person(n->x, 0, n->z, yaw, n->sk, n->sht, n->pa, n->ha, RGB(20,20,20), 0, 1); break;
    case K_GIANT: person(n->x, 0, n->z, yaw, n->sk, n->sht, n->pa, n->ha, RGB(150,255,255), t, 3.2f); break;
    case K_FOX: fox(n->x, n->z, yaw, t); break;
    case K_SHARD: shardObj(n->x, n->z, n->sht, t); break;
    case K_BOOK: bookObj(n->x, n->z, n->sht); break;
    case K_BLOB: blob(n->x, n->z, yaw, n->sht, t); break;
    }
}

static int npcOpen(int i) { const N *n = &NP[i]; return !seen[i] && (flags & n->req) == n->req; }

static void world(float t) {
    const S *s = &SC[ch];
    setT(0, 0, 0, 0, 1);
    box(0, -.5f, 0, s->b + 14, .5f, s->b + 14, s->gr);
    if (ch == 0 || ch == 2 || ch == 4) { setT(0, 0, 0, 0, 1); box(0, -.7f, -(s->b + 14) - 40, 150, .5f, 40, RGB(40,90,150)); }
    float far2 = (s->fog + 8) * (s->fog + 8);
    for (int i = 0; i < np; i++) { float a = pr[i].x - px, b = pr[i].z - pz; if (a * a + b * b < far2 + 600) prop(&pr[i]); }
    for (int i = 0; i < 5; i++) if (!got[ch][i]) {
        setT(lanX[i], 0, lanZ[i], t * 2, 1);
        box(0, .9f + sinf(t * 2 + i) * .12f, 0, .2f, .25f, .2f, RGB(255,200,60)); box(0, 1.25f + sinf(t * 2 + i) * .12f, 0, .1f, .06f, .1f, RGB(90,70,40));
    }
    for (int i = 0; i < NNP; i++) if (NP[i].ch == ch) {
        drawNPC(&NP[i], t);
        if (mode == M_PLAY && npcOpen(i) && NP[i].bit >= 0) marker(NP[i].x, NP[i].kind == K_GIANT ? 7.5f : 3.0f, NP[i].z, t);
    }
    if (mode != M_TITLE) person(px, 0, pz, pyaw, SKA, RGB(60,110,200), RGB(50,50,70), RGB(100,60,30), RGB(20,20,20), walk, 1);
    else person(0, 0, 4, t, SKA, RGB(60,110,200), RGB(50,50,70), RGB(100,60,30), RGB(20,20,20), t * 3, 1);
}

/* ---------------------------------------------------------------- 2D / text */
static void rect(int x, int y, int w, int h, unsigned c) {
    V *v = sceGuGetMemory(2 * sizeof(V));
    v[0].c = c; v[0].x = (float)x; v[0].y = (float)y; v[0].z = 0;
    v[1].c = c; v[1].x = (float)(x + w); v[1].y = (float)(y + h); v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
}
static void ptxt(int c, int r, unsigned col, const char *s) {
    pspDebugScreenSetTextColor(col); pspDebugScreenSetXY(c, r); pspDebugScreenPuts(s);
}
static void wrapPrint(const char *t, int shown, int row, unsigned col) {
    char line[64]; int pos = 0, len = (int)strlen(t);
    while (pos < len && row < 29) {
        int end = pos + 54;
        if (end >= len) end = len; else { while (end > pos && t[end] != ' ') end--; if (end == pos) end = pos + 54; }
        int n = end - pos, show = shown - pos;
        if (show > n) show = n;
        if (show < 0) show = 0;
        memcpy(line, t + pos, show); line[show] = 0;
        ptxt(2, row, col, line);
        pos = end; while (pos < len && t[pos] == ' ') pos++;
        row++;
    }
}

/* ---------------------------------------------------------------- dialogue engine */
static const L *dl, *cur; static int di, dn = -1, after, sel; static float dchars;
static L tmp[2]; static char optA[64], optB[64];
static int fadeA, fadeDir;
static const char *endTitle = "";

static void load(void);
static void startDlg(const L *a, int npc, int aft) { dl = a; di = 0; dn = npc; after = aft; load(); }
static void one(const char *spk, const char *txt) { tmp[0].s = spk; tmp[0].t = txt; tmp[1].s = 0; tmp[1].t = 0; startDlg(tmp, -1, 0); }

static void finishD(void) {
    int a = after, n = dn;
    mode = M_PLAY; after = 0; dn = -1;
    if (n >= 0 && !seen[n]) { seen[n] = 1; if (NP[n].bit >= 0) flags |= 1 << NP[n].bit; if (NP[n].act == 1) shards++; }
    if (a == 1) { mode = M_FADE; fadeA = 0; fadeDir = 1; }
    else if (a == 2) {
        if (lensChoice == 0) { endTitle = "THE SEAL"; startDlg(DL[D_ENDA], -1, 3); }
        else if (trust >= 2) { endTitle = "THE HARBOR - TRUE ENDING"; startDlg(DL[D_ENDB], -1, 3); }
        else { endTitle = "THE TIDE RETURNS"; startDlg(DL[D_ENDC], -1, 3); }
    }
    else if (a == 3) mode = M_CREDITS;
}

static void load(void) {
    cur = &dl[di++];
    if (!cur->s) { finishD(); return; }
    if (cur->s[0] == '*') {
        const char *bar = strchr(cur->t, '|');
        int la = (int)(bar - cur->t);
        memcpy(optA, cur->t, la); optA[la] = 0;
        strncpy(optB, bar + 1, 63); optB[63] = 0;
        sel = 0; mode = M_CHOOSE;
    } else { dchars = 0; mode = M_SAY; }
}

static void talk(int i) {
    N *n = &NP[i];
    if ((flags & n->req) != n->req) one("", "Something tells you there is more to learn here first.");
    else if (seen[i]) one(n->n, n->rep);
    else startDlg(DL[n->dlg], i, n->dlg == D_LENS ? 2 : 0);
}

static void newGame(void) {
    memset(seen, 0, sizeof(seen)); memset(got, 0, sizeof(got));
    ch = 0; flags = 0; shards = 0; trust = 0; lensChoice = 0; playF = 0;
    px = 0; pz = 6; pyaw = 3.14f; cam = 0; gen(0);
    startDlg(DL[CH[0].intro], -1, 0);
}

static void nextChapter(void) {
    ch++; flags = 0; px = 0; pz = 8; pyaw = 3.14f; cam = 0; gen(ch);
}

static void update(const SceCtrlData *p, unsigned pr_) {
    if (mode == M_TITLE) { if (pr_ & (PSP_CTRL_CROSS | PSP_CTRL_START)) newGame(); return; }
    if (mode == M_CREDITS) { if (pr_ & PSP_CTRL_START) { mode = M_TITLE; ch = 0; gen(0); } return; }
    if (mode == M_FADE) {
        fadeA += 6 * fadeDir;
        if (fadeA >= 255) { fadeA = 255; fadeDir = -1; nextChapter(); }
        if (fadeA <= 0 && fadeDir < 0) { fadeA = 0; mode = M_PLAY; startDlg(DL[CH[ch].intro], -1, 0); }
        return;
    }
    playF++;
    if (mode == M_SAY) {
        int len = (int)strlen(cur->t);
        dchars += .8f;
        if (pr_ & PSP_CTRL_CROSS) { if (dchars < len) dchars = (float)len; else load(); }
        return;
    }
    if (mode == M_CHOOSE) {
        if (pr_ & (PSP_CTRL_UP | PSP_CTRL_DOWN)) sel = !sel;
        if (pr_ & PSP_CTRL_CROSS) {
            const L *base = cur; int ch2 = sel;
            if (dl == d_lens) lensChoice = sel; else if (sel == 0) trust++;
            cur = base + 1 + ch2;
            di += 2; dchars = 0; mode = M_SAY;
        }
        return;
    }
    /* M_PLAY */
    float dx = (p->Lx - 128) / 128.0f, dz = (p->Ly - 128) / 128.0f;
    if (fabsf(dx) < .25f) dx = 0;
    if (fabsf(dz) < .25f) dz = 0;
    if (p->Buttons & PSP_CTRL_LEFT) dx = -1;
    if (p->Buttons & PSP_CTRL_RIGHT) dx = 1;
    if (p->Buttons & PSP_CTRL_UP) dz = -1;
    if (p->Buttons & PSP_CTRL_DOWN) dz = 1;
    if (p->Buttons & PSP_CTRL_LTRIGGER) cam -= .04f;
    if (p->Buttons & PSP_CTRL_RTRIGGER) cam += .04f;
    float fx = -sinf(cam), fz = -cosf(cam), rx = cosf(cam), rz = -sinf(cam);
    float mx = -fx * dz + rx * dx, mz = -fz * dz + rz * dx, m = sqrtf(mx * mx + mz * mz);
    if (m > .01f) {
        float sp = (p->Buttons & PSP_CTRL_CIRCLE) ? .22f : .12f;
        if (m > 1) { mx /= m; mz /= m; }
        px += mx * sp; pz += mz * sp; pyaw = atan2f(mx, mz); walk += sp * 3;
    }
    float b = SC[ch].b; if (px > b) px = b; if (px < -b) px = -b; if (pz > b) pz = b; if (pz < -b) pz = -b;
    for (int i = 0; i < np; i++) if (pr[i].t != 5) {
        float a = px - pr[i].x, c = pz - pr[i].z, d = sqrtf(a * a + c * c), mm = pr[i].r + .6f;
        if (d < mm && d > .001f) { px = pr[i].x + a / d * mm; pz = pr[i].z + c / d * mm; }
    }
    int near = -1; float best = 3.0f;
    for (int i = 0; i < NNP; i++) if (NP[i].ch == ch) {
        float a = px - NP[i].x, c = pz - NP[i].z, d = sqrtf(a * a + c * c), rad = NP[i].kind == K_GIANT ? 2.6f : .8f;
        if (d < rad && d > .001f) { px = NP[i].x + a / d * rad; pz = NP[i].z + c / d * rad; }
        if (NP[i].kind == K_GIANT) d -= 2.2f;
        if (d < best) { best = d; near = i; }
    }
    for (int i = 0; i < 5; i++) if (!got[ch][i]) { float a = px - lanX[i], c = pz - lanZ[i]; if (a * a + c * c < 2.0f) got[ch][i] = 1; }
    if (near >= 0 && (pr_ & PSP_CTRL_CROSS)) { talk(near); return; }
    if (ch < 4 && (flags & CH[ch].mask) == CH[ch].mask && lantCount(ch) >= 3) startDlg(DL[CH[ch].outro], -1, 1);
}

static int nearIdx(void) {
    int near = -1; float best = 3.0f;
    for (int i = 0; i < NNP; i++) if (NP[i].ch == ch) {
        float a = px - NP[i].x, c = pz - NP[i].z, d = sqrtf(a * a + c * c);
        if (NP[i].kind == K_GIANT) d -= 2.2f;
        if (d < best) { best = d; near = i; }
    }
    return near;
}

/* ---------------------------------------------------------------- render */
static int drawOff = 0;

static void render(float t) {
    const S *s = &SC[ch];
    char buf[96];
    sceGuStart(GU_DIRECT, list);
    sceGuClearColor(mode == M_CREDITS ? 0xff000000u : s->sky); sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuEnable(GU_DEPTH_TEST); sceGuDisable(GU_BLEND); sceGuEnable(GU_FOG); sceGuFog(8.0f, s->fog, s->sky);
    sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity(); sceGumPerspective(55.0f, 16.0f / 9.0f, 0.5f, 140.0f);
    float ca = (mode == M_TITLE) ? t * .3f : cam, dist = (mode == M_TITLE) ? 11.0f : 9.0f, tx = (mode == M_TITLE) ? 0 : px, tz = (mode == M_TITLE) ? 4 : pz;
    ScePspFVector3 eye = { tx + sinf(ca) * dist, 5.5f, tz + cosf(ca) * dist }, ctr = { tx, 1.6f, tz }, up = { 0, 1, 0 };
    sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity(); sceGumLookAt(&eye, &ctr, &up);
    sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity();
    if (mode != M_CREDITS) world(t);

    sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_FOG); sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    if (mode == M_TITLE) rect(50, 70, 380, 100, RGBA(0,0,0,170));
    if (mode == M_PLAY || mode == M_SAY || mode == M_CHOOSE) rect(0, 0, 480, 18, RGBA(0,0,0,150));
    if (mode == M_SAY || mode == M_CHOOSE) {
        rect(6, 172, 468, 94, RGBA(10,10,30,215)); rect(6, 172, 468, 2, RGBA(255,255,255,200));
        if (cur->s[0] && cur->s[0] != '*') rect(10, 158, (int)strlen(cur->s) * 8 + 12, 16, RGBA(40,40,90,230));
    }
    int ni = (mode == M_PLAY) ? nearIdx() : -1;
    if (ni >= 0) rect(120, 236, 240, 16, RGBA(0,0,0,170));
    if (mode == M_FADE) rect(0, 0, 480, 272, RGBA(0,0,0,fadeA));
    sceGuFinish(); sceGuSync(0, 0);

    pspDebugScreenSetOffset(drawOff);
    if (mode == M_TITLE) {
        ptxt(10, 10, 0xffffffff, "E C H O E S   O F   H O L L O W   B A Y");
        ptxt(14, 12, 0xff88ddff, "A story adventure for PSP");
        ptxt(13, 14, 0xffcccccc, "Press X or START to begin");
        ptxt(8, 16, 0xff999999, "Walk, talk, find lanterns. Your choices matter.");
    } else if (mode == M_CREDITS) {
        snprintf(buf, sizeof(buf), "ENDING: %s", endTitle); ptxt(6, 8, 0xff88ddff, buf);
        snprintf(buf, sizeof(buf), "Play time: %u min", playF / 3600); ptxt(6, 11, 0xffffffff, buf);
        snprintf(buf, sizeof(buf), "Memory lanterns found: %d / 25", lantTotal()); ptxt(6, 13, 0xffffffff, buf);
        snprintf(buf, sizeof(buf), "Kindness: %d / 3", trust); ptxt(6, 15, 0xffffffff, buf);
        ptxt(6, 18, 0xffcccccc, "Thanks for playing ECHOES OF HOLLOW BAY.");
        ptxt(6, 20, 0xff999999, "Press START to return to the title.");
    } else if (mode != M_FADE) {
        snprintf(buf, sizeof(buf), "%s  Shards %d/3  Lanterns %d/5%s", CH[ch].t, shards, lantCount(ch), ch < 4 ? " (need 3)" : "");
        ptxt(1, 0, 0xffffffff, buf); ptxt(1, 1, 0xff88ddff, CH[ch].goal);
        if (ni >= 0) {
            snprintf(buf, sizeof(buf), "X: %s %s", (NP[ni].kind == K_SHARD || NP[ni].kind == K_BOOK) ? "Examine" : "Talk to", NP[ni].n);
            ptxt(16, 30, 0xff66ffff, buf);
        }
        if (mode == M_SAY || mode == M_CHOOSE) {
            unsigned c = 0xffe6e6e6; const char *sp = cur->s;
            if (mode == M_CHOOSE) {
                ptxt(2, 23, 0xffcccccc, "Choose:");
                ptxt(2, 25, sel == 0 ? 0xff55ffff : 0xff999999, sel == 0 ? "> " : "  "); ptxt(4, 25, sel == 0 ? 0xff55ffff : 0xff999999, optA);
                ptxt(2, 27, sel == 1 ? 0xff55ffff : 0xff999999, sel == 1 ? "> " : "  "); ptxt(4, 27, sel == 1 ? 0xff55ffff : 0xff999999, optB);
            } else {
                if (sp[0]) {
                    ptxt(2, 20, !strcmp(sp, "Kai") ? 0xffffd296 : !strcmp(sp, "Warden") ? 0xffffff78 : 0xff96dcff, sp);
                    c = 0xffffffff;
                }
                wrapPrint(cur->t, (int)dchars, 23, c);
                if (dchars >= (float)strlen(cur->t)) ptxt(56, 31, 0xffaaaaaa, "[X]");
            }
        }
    }
    drawOff = drawOff ? 0 : FS;
    sceGuSwapBuffers();
    sceDisplayWaitVblankStart();
}

static void initGu(void) {
    sceGuInit(); sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BW);
    sceGuDispBuffer(SW, SH, (void *)FS, BW);
    sceGuDepthBuffer((void *)(FS * 2), BW);
    sceGuOffset(2048 - (SW / 2), 2048 - (SH / 2));
    sceGuViewport(2048, 2048, SW, SH);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SW, SH); sceGuEnable(GU_SCISSOR_TEST);
    sceGuDepthFunc(GU_GEQUAL); sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW); sceGuShadeModel(GU_SMOOTH); sceGuDisable(GU_CULL_FACE); sceGuDisable(GU_TEXTURE_2D);
    sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); sceGuDisplay(GU_TRUE);
}

int main(void) {
    int th = sceKernelCreateThread("cb", cbThread, 0x11, 0xFA0, 0, 0);
    if (th >= 0) sceKernelStartThread(th, 0, 0);
    pspDebugScreenInit();
    pspDebugScreenEnableBackColor(0);
    sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    initGu(); gen(0);
    SceCtrlData pad; unsigned old = 0, frame = 0;
    while (running) {
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned pressed = pad.Buttons & ~old; old = pad.Buttons; frame++;
        update(&pad, pressed);
        render(frame / 60.0f);
    }
    sceGuTerm(); sceKernelExitGame();
    return 0;
}
