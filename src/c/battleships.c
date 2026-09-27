/*
 * BATTLESHIPS - real-time two player naval duel, top view.
 *
 *   Player 1 (yellow, starts left):  joystick in port 2
 *   Player 2 (red, starts right):    joystick in port 1
 *
 *   joystick           move your crosshair (the ship keeps its course)
 *   fire + left/right  put the rudder hard left / hard right. It stays
 *                      there; the ship keeps turning (10-degree steps).
 *   fire + up          with the rudder over: centre it (straight ahead).
 *                      With the rudder already straight: raise the engine
 *                      setting (speed setpoint).
 *   fire + down        lower the engine setting / astern.
 *                      The slider on the bottom row shows the setting; the
 *                      ship's speed follows it with a delay, and nothing
 *                      but the engines slows it down.
 *   The ship always moves the way its bow points.
 *   tap fire           fire a shell; it flies in an arc and lands on the
 *                      crosshair after a delay, hitting any ship there
 *
 * Every round has a new random map with a random amount of land.
 * Each map hides 1-2 icebergs: they only show up when a ship is within one
 * tile, and ramming one costs a heart (the iceberg breaks up).
 * Each ship takes 3 hits. Ramming the other ship costs both a heart. A ship that runs aground on an island is wrecked
 * at once. The screen edges just stop it.
 *
 * Build with -DAUTOPLAY to let two simple bots play (used for testing).
 */
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include "ship_sprites.h"         /* generated: ship frames and heading tables */
#include "charset.h"              /* generated: custom character set */

/* The VIC-II looks at bank 2 ($8000-$BFFF): screen at $8000, sprite graphics
   from $8400, custom character set at $A000 (RAM under BASIC ROM: the CPU
   writes it, the VIC reads it). That leaves $0801-$7FFF for the
   program; main() checks it ends below $8000. */
#define VIC_BANK    0x8000
#define SCREEN      ((unsigned char *)0x8000)
#define COLORRAM    ((unsigned char *)0xD800)
#define SPRPTR      ((unsigned char *)0x83F8)
#define SPRPOS      ((unsigned char *)0xD000)      /* x,y pairs */
#define SPRCOLOR    (&VIC.spr0_color)

#define SPRDATA     ((unsigned char *)0x8400)
#define CHARSET     ((unsigned char *)0xA000)
#define BLK_SHIP    ((0x8400 - VIC_BANK) / 64)  /* HEADINGS frames */
#define BLK_SHELL   (BLK_SHIP + HEADINGS)       /* 4 frames, small to large */
#define SHELL_SIZES 4
#define BLK_BOOM    (BLK_SHELL + SHELL_SIZES)
#define BLK_AIM     (BLK_BOOM + 1)
#define BLK_SPLASH  (BLK_AIM + 1)
#define SPR_BLOCKS  (BLK_SPLASH + 1 - BLK_SHIP)
#define BLOCK_DATA(b) (SPRDATA + ((b) - BLK_SHIP) * 64)

/* Sprite numbers; lower numbers are drawn on top. */
#define SPR_AIM(p)   (p)
#define SPR_SHIP(p)  (2 + (p))
#define SPR_SHELL(i) (4 + (i))

/* Island tiles are ISLAND_BASE + neighbour mask (charset.h): coast shapes. */
#define IS_ISLAND(c) (((c) & 0xF0) == ISLAND_BASE)
#define WAVE_CHAR   100                 /* thin line at bottom of cell */
#define MARK_CHAR   87                  /* ring: where a shell will land */
#define MARK_LAND   (MARK_CHAR | 0x80)  /* the same on an island square */

#define JOY_UP      0x01
#define JOY_DOWN    0x02
#define JOY_LEFT    0x04
#define JOY_RIGHT   0x08
#define JOY_FIRE    0x10
#define JOY_DIRS    (JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT)

#define MAX_HP      3
/* Ship physics. Velocity is in 1/256 pixel per frame. */
#define MAX_SPEED   192                 /* ahead: 0.75 pixels per frame */
#define MAX_REVERSE 64                  /* astern */
#define SPEED_STEP  3                   /* max speed change per frame toward the setpoint */
#define SET_STEP    18                  /* setpoint change per frame while fire+up/down */

/* Engine slider on the bottom row: 17 cells from full astern to full ahead. */
#define SLIDER_ROW   24
#define SLIDER_COL0  0
#define SLIDER_COL1  23
#define SLIDER_CELLS 17
#define SLIDER_UNIT  ((MAX_SPEED + MAX_REVERSE) / (SLIDER_CELLS - 1))
#define SLIDER_ZERO  (MAX_REVERSE / SLIDER_UNIT)
#define SL_TRACK     64                 /* horizontal line */
#define SL_ZERO      91                 /* cross: stop */
#define SL_KNOB      90                 /* diamond: setpoint */
/* Turning: TURN_RATE / TURN_COST heading steps per frame, i.e. one
   10-degree step every 6.25 frames. */
#define TURN_RATE   4
#define TURN_COST   25
#define FPS         50                  /* PAL frames per second */
#define RELOAD      30                  /* frames between shots */
#define MAGAZINE    3                   /* shots before a cooldown */
#define COOLDOWN    (5 * FPS / 2)       /* frames to reload the magazine (2.5 s) */
#define AMMO_COL0   9                   /* HUD columns of the ammo display, */
#define AMMO_COL1   26                  /* 5 cells each */
#define AMMO_CELLS  5
#define COOL_CELL   (COOLDOWN / AMMO_CELLS)  /* reload frames per bar block */
#define SHELLS_PER  2                   /* shells in flight per player */
#define AIM_SPEED   2                   /* crosshair pixels per frame */
#define AIM_AHEAD   64                  /* crosshair start distance from ship */
#define HIT_RANGE   10                  /* pixels from ship centre that count as a hit */
#define SPLASH_TIME 20                  /* frames the impact stays visible */

typedef struct {
    int x, y;                   /* centre, 1/16 pixel */
    int speed;                  /* along the heading, 1/256 pixel per frame */
    int setpoint;               /* engine setting the speed follows */
    unsigned char knob_cell;    /* slider as last drawn */
    unsigned char fill_cell;
    int vx, vy;                 /* resulting velocity, derived each frame */
    int rx, ry;                 /* sub-step remainder, 0..15 */
    unsigned char dir;
    unsigned char turn_acc;     /* builds up to TURN_COST for the next step */
    signed char rudder;         /* -1 left, 0 straight, 1 right */
    unsigned char prev_dirs;    /* joystick directions last frame, for press edges */
    unsigned char up_centred;   /* this up press centred the rudder: no speed change */
    unsigned char reload;
    unsigned char shots;        /* left in the magazine */
    unsigned char cooldown;     /* frames until the magazine is full again */
    unsigned char hp;
    unsigned char flash;
    unsigned char color;
    unsigned char cool_color;   /* hull colour while the magazine reloads */
    int aim_x, aim_y;           /* crosshair, pixels */
    unsigned char fire_held;
    unsigned char stick_used;   /* joystick moved during this press: no shot */
} ship_t;

enum { SHELL_FREE, SHELL_FLYING, SHELL_SPLASH };

typedef struct {
    unsigned char state;
    int x, y;                   /* ground position under the shell, 1/16 pixel */
    int dx, dy;                 /* step per frame, 1/16 pixel */
    int tx, ty;                 /* landing point, pixels */
    unsigned char flight;       /* total flight time, frames */
    unsigned char t;            /* frames flown, or splash countdown */
    unsigned char block, color; /* impact sprite */
    unsigned int cell;          /* screen cell of the landing mark */
    unsigned char under_ch;     /* what the mark covers */
    unsigned char under_col;
} shell_t;


static ship_t ships[2];
static shell_t shells[2 * SHELLS_PER];  /* player p owns p*SHELLS_PER.. */
static unsigned char wins[2];

/* Icebergs: hidden until a ship comes within one tile. */
#define ICE_CHAR    112
#define MAX_ICE     2
typedef struct {
    unsigned char col, row;
    unsigned char alive;
    unsigned char shown;
} iceberg_t;
static iceberg_t icebergs[MAX_ICE];
static unsigned char ice_count;
static unsigned char frame;

/* ------------------------------------------------------------------ */
/* Screen helpers                                                     */

static void print(unsigned char col, unsigned char row, const char *s, unsigned char color)
{
    unsigned int o = row * 40 + col;
    unsigned char ch;

    /* Write strings in lowercase: cc65 turns them into PETSCII $41-$5A,
       which map to screen codes $01-$1A (uppercase letters). */
    while ((ch = *s++)) {
        if (ch >= 0x41 && ch <= 0x5A) {
            ch -= 0x40;
        }
        SCREEN[o] = ch;
        COLORRAM[o] = color;
        ++o;
    }
}

static void print_centered(unsigned char row, const char *s, unsigned char color)
{
    print((unsigned char)(20 - strlen(s) / 2), row, s, color);
}

/* Fills a w x h block of land (corners cut on bigger blocks); returns how
   many cells became land. */
static unsigned char draw_island(unsigned char c, unsigned char r, unsigned char w, unsigned char h)
{
    unsigned char x, y, added = 0;
    unsigned int o;

    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            /* round off the corners of bigger islands */
            if (w > 2 && h > 2 && (x == 0 || x == w - 1) && (y == 0 || y == h - 1)) {
                continue;
            }
            o = (r + y) * 40 + c + x;
            if (!IS_ISLAND(SCREEN[o])) ++added;
            SCREEN[o] = ISLAND_BASE;    /* shaped by shape_coast() */
            COLORRAM[o] = COLOR_GREEN;
        }
    }
    return added;
}

/* Picks the coast tile for every land cell from its land neighbours
   (N=1 E=2 S=4 W=8), so islands get rounded, irregular shorelines. */
static void shape_coast(void)
{
    unsigned char x, y, m;
    unsigned char *p = SCREEN + 40;

    for (y = 1; y < 24; ++y) {
        for (x = 0; x < 40; ++x, ++p) {
            if (!IS_ISLAND(*p)) continue;
            m = 0;
            if (y > 1 && IS_ISLAND(p[-40])) m |= 1;
            if (x < 39 && IS_ISLAND(p[1])) m |= 2;
            if (y < 23 && IS_ISLAND(p[40])) m |= 4;
            if (x > 0 && IS_ISLAND(p[-1])) m |= 8;
            *p = ISLAND_BASE | m;
        }
    }
}

static void draw_sea(void)
{
    unsigned char x, y, m;
    unsigned char *scr = SCREEN;

    memset(SCREEN, ' ', 40);            /* HUD row */
    memset(COLORRAM, COLOR_LIGHTBLUE, 1000);
    /* scattered waves where (x*7 + y*13) % 17 == 0, stepped incrementally */
    for (y = 1; y < 25; ++y) {
        scr += 40;
        m = (y * 13) % 17;
        for (x = 0; x < 40; ++x) {
            scr[x] = m == 0 ? WAVE_CHAR : ' ';
            m += 7;
            if (m >= 17) m -= 17;
        }
    }
}

/* A new random map every round: islands of 2-4 x 2-4 cells, which may
   merge, until a random amount of land (about 1-10% of the sea) is reached.
   Each island is mirrored left/right so both sides are equal, and the
   start areas (columns 0-9 / 30-39, rows 10-16) stay clear. */
#define LAND_MIN    12
#define LAND_RANGE  90
static void draw_map(void)
{
    unsigned char tries, c, r, w, h, land, target;

    draw_sea();
    target = LAND_MIN + rand() % LAND_RANGE;
    land = 0;
    for (tries = 0; tries < 60 && land < target; ++tries) {
        w = 2 + rand() % 3;
        h = 2 + rand() % 3;
        c = 1 + rand() % (20 - w);              /* left half: ends by column 19 */
        r = 2 + rand() % (21 - h);              /* rows 2..21, clear of HUD and slider */
        if (c < 10 && r <= 16 && r + h > 10) continue;
        land += draw_island(c, r, w, h);
        land += draw_island(40 - c - w, r, w, h);
    }
    shape_coast();
}

static void put_number(unsigned char col, unsigned char n, unsigned char color)
{
    SCREEN[col] = '0' + n / 10;
    SCREEN[col + 1] = '0' + n % 10;
    COLORRAM[col] = color;
    COLORRAM[col + 1] = color;
}

/* Ammo status in the HUD: one shell icon per shot (grey when used), or
   while reloading a bar that loses one block every COOL_CELL frames. */
static void draw_ammo(unsigned char p)
{
    ship_t *s = &ships[p];
    unsigned char *scr = SCREEN + (p ? AMMO_COL1 : AMMO_COL0);
    unsigned char *col = COLORRAM + (p ? AMMO_COL1 : AMMO_COL0);
    unsigned char i, n;

    if (s->cooldown) {
        n = (s->cooldown + COOL_CELL - 1) / COOL_CELL;  /* blocks left, rounded up */
        for (i = 0; i < AMMO_CELLS; ++i) {
            scr[i] = i < n ? 98 : ' ';          /* lower half block */
            col[i] = COLOR_GRAY2;
        }
    } else {
        for (i = 0; i < AMMO_CELLS; ++i) {
            scr[i] = i < MAGAZINE ? 81 : ' ';   /* filled circle */
            col[i] = i < s->shots ? s->color : COLOR_GRAY1;
        }
    }
}

static unsigned char slider_cell(int v)
{
    return (unsigned char)((v + MAX_REVERSE + SLIDER_UNIT / 2) / SLIDER_UNIT);
}

/* Engine slider: knob at the setpoint, track lit from stop to the actual
   speed, so the delay between the two is visible. */
static void draw_slider(unsigned char p)
{
    ship_t *s = &ships[p];
    unsigned int o = SLIDER_ROW * 40 + (p ? SLIDER_COL1 : SLIDER_COL0);
    unsigned char i, lo, hi;

    s->knob_cell = slider_cell(s->setpoint);
    s->fill_cell = slider_cell(s->speed);
    lo = s->fill_cell < SLIDER_ZERO ? s->fill_cell : SLIDER_ZERO;
    hi = s->fill_cell > SLIDER_ZERO ? s->fill_cell : SLIDER_ZERO;
    for (i = 0; i < SLIDER_CELLS; ++i, ++o) {
        if (i == s->knob_cell) {
            SCREEN[o] = SL_KNOB;
            COLORRAM[o] = COLOR_WHITE;
        } else {
            SCREEN[o] = i == SLIDER_ZERO ? SL_ZERO : SL_TRACK;
            COLORRAM[o] = (i >= lo && i <= hi) ? s->color : COLOR_GRAY1;
        }
    }
}

static void draw_hud(void)
{
    unsigned char i;

    for (i = 0; i < 40; ++i) {
        SCREEN[i] = ' ';
    }
    print(0, 0, "p1", COLOR_YELLOW);
    print(38, 0, "p2", COLOR_LIGHTRED);
    for (i = 0; i < MAX_HP; ++i) {
        SCREEN[3 + i] = 83;                 /* heart */
        COLORRAM[3 + i] = i < ships[0].hp ? COLOR_YELLOW : COLOR_GRAY1;
        SCREEN[37 - MAX_HP + i] = 83;       /* right-aligned next to "p2" */
        COLORRAM[37 - MAX_HP + i] = i < ships[1].hp ? COLOR_LIGHTRED : COLOR_GRAY1;
    }
    put_number(17, wins[0], COLOR_YELLOW);
    print(19, 0, "-", COLOR_WHITE);
    put_number(20, wins[1], COLOR_LIGHTRED);
    draw_ammo(0);
    draw_ammo(1);
    memset(SCREEN + SLIDER_ROW * 40, ' ', 40);
    draw_slider(0);
    draw_slider(1);
}

/* ------------------------------------------------------------------ */
/* Sprites                                                            */

static void make_sprites(void)
{
    unsigned char d, x, y;
    int c, u, v;
    unsigned char *p;

    memset(SPRDATA, 0, 64 * SPR_BLOCKS);

    /* ship headings, pre-rotated by tools/gen_ship_sprites.py */
    for (d = 0; d < HEADINGS; ++d) {
        memcpy(SPRDATA + d * 64, ship_frames + d * 63, 63);
    }

    /* shell: discs of radius 1..4 around (5,5); bigger = higher up */
    for (d = 0; d < SHELL_SIZES; ++d) {
        p = BLOCK_DATA(BLK_SHELL + d);
        c = (d + 1) * (d + 1) + d;      /* radius squared, slightly rounded */
        for (y = 0; y < 11; ++y) {
            for (x = 0; x < 11; ++x) {
                u = (int)x - 5;
                v = (int)y - 5;
                if (u * u + v * v <= c) {
                    p[y * 3 + (x >> 3)] |= 0x80 >> (x & 7);
                }
            }
        }
    }

    /* explosion: speckled disc */
    p = BLOCK_DATA(BLK_BOOM);
    for (y = 0; y < 21; ++y) {
        for (x = 0; x < 21; ++x) {
            u = (int)x - 10;
            v = (int)y - 10;
            if (u * u + v * v < 100 && (x * 7 + y * 13) % 5 < 3) {
                p[y * 3 + (x >> 3)] |= 0x80 >> (x & 7);
            }
        }
    }

    /* crosshair: 7x7 X, centre (3,3) */
    p = BLOCK_DATA(BLK_AIM);
    p[0] = 0x82; p[3] = 0x44; p[6] = 0x28; p[9] = 0x10;
    p[12] = 0x28; p[15] = 0x44; p[18] = 0x82;

    /* splash: ring, centre (5,5) */
    p = BLOCK_DATA(BLK_SPLASH);
    for (y = 0; y < 11; ++y) {
        for (x = 0; x < 11; ++x) {
            u = (int)x - 5;
            v = (int)y - 5;
            c = u * u + v * v;
            if (c >= 9 && c <= 25) {
                p[y * 3 + (x >> 3)] |= 0x80 >> (x & 7);
            }
        }
    }
}

/* Place sprite n with its top-left corner at screen pixel (x,y). */
static void place_sprite(unsigned char n, int x, int y)
{
    unsigned int sx = x + 24;
    unsigned char bit = 1 << n;

    SPRPOS[n * 2] = (unsigned char)sx;
    SPRPOS[n * 2 + 1] = (unsigned char)(y + 50);
    if (sx & 0x100) {
        VIC.spr_hi_x |= bit;
    } else {
        VIC.spr_hi_x &= ~bit;
    }
}

static void update_sprites(void)
{
    unsigned char i, n, arc, size, ena = 0;
    ship_t *sh;
    shell_t *b;

    for (i = 0; i < 2; ++i) {
        sh = &ships[i];
        n = SPR_SHIP(i);
        if (sh->hp) {
            SPRPTR[n] = BLK_SHIP + sh->dir;
            SPRCOLOR[n] = (sh->flash & 2) ? COLOR_WHITE
                        : sh->cooldown ? sh->cool_color : sh->color;
            place_sprite(SPR_AIM(i), sh->aim_x - 3, sh->aim_y - 3);
            ena |= 1 << SPR_AIM(i);
        }
        place_sprite(n, (sh->x >> 4) - 10, (sh->y >> 4) - 10);
        ena |= 1 << n;
    }
    for (i = 0; i < 2 * SHELLS_PER; ++i) {
        b = &shells[i];
        n = SPR_SHELL(i);
        if (b->state == SHELL_FLYING) {
            /* parabolic arc: height peaks at flight/8 pixels halfway */
            arc = (unsigned char)((unsigned)b->t * (b->flight - b->t) / (b->flight * 2));
            /* size follows height: arc / (peak / SHELL_SIZES) */
            size = arc * SHELL_SIZES / (b->flight / 8 + 1);
            if (size >= SHELL_SIZES) size = SHELL_SIZES - 1;
            SPRPTR[n] = BLK_SHELL + size;
            SPRCOLOR[n] = COLOR_WHITE;
            place_sprite(n, (b->x >> 4) - 5, (b->y >> 4) - 5 - arc);
            ena |= 1 << n;
        } else if (b->state == SHELL_SPLASH) {
            SPRPTR[n] = b->block;
            SPRCOLOR[n] = (b->t & 4) ? COLOR_WHITE : b->color;
            if (b->block == BLK_BOOM) {
                place_sprite(n, b->tx - 10, b->ty - 10);
            } else {
                place_sprite(n, b->tx - 5, b->ty - 5);
            }
            ena |= 1 << n;
        }
    }
    VIC.spr_ena = ena;
}

/* ------------------------------------------------------------------ */
/* Sound                                                              */

/* Music and effects run from a raster interrupt (src/music/sound.s).
   Effects borrow one SID voice; a higher number has priority. */
void __fastcall__ sound_start(unsigned char song);
void __fastcall__ music_select(unsigned char song);
void __fastcall__ sfx_play(unsigned char effect);

/* Music choices on the title screen; the last one is "off". */
#define SONGS 3
static const char *const song_names[SONGS] = { "neon tide", "moonlit harbour", "off" };
static unsigned char song;

#define SFX_SPLASH  0
#define SFX_SHOT    1
#define SFX_HIT     2
#define SFX_SINK    3

/* ------------------------------------------------------------------ */
/* Input                                                              */

static void wait_frame(void)
{
    while (VIC.rasterline != 251) ;
    while (VIC.rasterline == 251) ;
    ++frame;
}

#ifdef AUTOPLAY
/* Test bot: sail toward the other ship; now and then aim at where it will be
   when the shell lands, then tap fire. */
static unsigned char bot_phase[2];

static unsigned char bot(unsigned char me)
{
    ship_t *s = &ships[me];
    ship_t *t = &ships[me ^ 1];
    int dx = (t->x >> 4) - (s->x >> 4);
    int dy = (t->y >> 4) - (s->y >> 4);
    int ax = dx < 0 ? -dx : dx;
    int ay = dy < 0 ? -dy : dy;
    unsigned char d, want, diff, joy = 0;
    signed char rudder;
    int ex, ey, lead, dot, best;

    switch (bot_phase[me]) {
    case 1:                             /* fire released: move crosshair onto target */
        lead = 25 + (ax > ay ? ax : ay) / 5;
        ex = (t->x >> 4) + t->vx / 16 * lead / 16;
        ey = (t->y >> 4) + t->vy / 16 * lead / 16;
        if (s->aim_x < ex - 2) joy |= JOY_RIGHT;
        else if (s->aim_x > ex + 2) joy |= JOY_LEFT;
        if (s->aim_y < ey - 2) joy |= JOY_DOWN;
        else if (s->aim_y > ey + 2) joy |= JOY_UP;
        if (joy == 0) {
            bot_phase[me] = 2;
        }
        return joy;
    case 2:
        bot_phase[me] = 3;
        return JOY_FIRE;                /* tap ... */
    case 3:
        bot_phase[me] = 0;
        return 0;                       /* ... and release: shot */
    }

    if (s->reload == 0 && s->shots && (frame & 31) == me * 16) {
        bot_phase[me] = 1;
    }
    /* heading that points most toward the target */
    want = 0;
    best = -32767;
    for (d = 0; d < HEADINGS; ++d) {
        dot = head_x[d] * dx + head_y[d] * dy;
        if (dot > best) {
            best = dot;
            want = d;
        }
    }
    if (((frame + me * 128) & 255) >= 160) {
        want = (want + HEADINGS / 4 + me * HEADINGS / 2) % HEADINGS;  /* wander off */
    }
    diff = (want + HEADINGS - s->dir) % HEADINGS;
    rudder = diff == 0 ? 0 : diff <= HEADINGS / 2 ? 1 : -1;
    if (s->rudder != rudder) {
        /* one rudder press at a time, released in between */
        d = rudder > 0 ? JOY_RIGHT : rudder < 0 ? JOY_LEFT : JOY_UP;
        return (s->prev_dirs & d) ? JOY_FIRE : (d | JOY_FIRE);
    }
    /* cruise at about half speed, stop when close */
    if (ax + ay > 60 && s->setpoint < 100) {
        joy |= JOY_UP;
    } else if (ax + ay <= 60 && s->setpoint > 0) {
        joy |= JOY_DOWN;
    }
    return joy ? joy | JOY_FIRE : 0;    /* steering needs fire held */
}
#endif

/* Returns joystick bits for player 0 (port 2) and player 1 (port 1). */
static unsigned char read_joy(unsigned char player)
{
#ifdef AUTOPLAY
    return bot(player);
#else
    return (unsigned char)(~(player ? CIA1.prb : CIA1.pra) & 0x1F);
#endif
}

#ifndef AUTOPLAY
static unsigned char any_fire(void)
{
    return (read_joy(0) | read_joy(1)) & JOY_FIRE;
}
#endif

static void wait_fire(void)
{
#ifdef AUTOPLAY
    unsigned char t;
    for (t = 0; t < 100; ++t) wait_frame();
#else
    while (any_fire()) wait_frame();    /* wait for release first */
    while (!any_fire()) wait_frame();
#endif
}

/* ------------------------------------------------------------------ */
/* Game logic                                                         */

static unsigned char off_sea(int px, int py)
{
    return px < 2 || px > 317 || py < 10 || py > 197;
}

/* Inside the screen below the HUD row, where land_at() may look. */
static unsigned char on_screen(int px, int py)
{
    return px >= 0 && px < 320 && py >= 8 && py < 200;
}

/* Only for points inside the sea area (check off_sea first). */
/* Pixel-precise: water in the rounded coast tiles doesn't count as land. */
static unsigned char land_at(int px, int py)
{
    unsigned char c = SCREEN[(py >> 3) * 40 + (px >> 3)];

    if (c == MARK_LAND) return 1;
    if (!IS_ISLAND(c)) return 0;
    return charset_data[c * 8 + (py & 7)] & (0x80 >> (px & 7));
}

static unsigned char solid_at(int px, int py)
{
    return off_sea(px, py) || land_at(px, py);
}

enum { CONTACT_NONE, CONTACT_EDGE, CONTACT_LAND };

/* What the ship touches at position (x,y). Only the centre is stopped by the
   screen edge, so bow and stern can slide under the border; all three points
   run aground on land. */
static unsigned char ship_contact(int x, int y, unsigned char d)
{
    int px = x >> 4;
    int py = y >> 4;
    int bx = px + bow_x[d];
    int by = py + bow_y[d];
    int sx = px - bow_x[d];
    int sy = py - bow_y[d];

    if (off_sea(px, py)) {
        return CONTACT_EDGE;
    }
    if (land_at(px, py)
        || (on_screen(bx, by) && land_at(bx, by))
        || (on_screen(sx, sy) && land_at(sx, sy))) {
        return CONTACT_LAND;
    }
    return CONTACT_NONE;
}

static int iabs(int v)
{
    return v < 0 ? -v : v;
}

/* Signed division by a small positive number. cc65 divides unsigned when the
   divisor is an unsigned char, which turns negative steps into huge ones. */
static int sdiv(int n, unsigned char d)
{
    return n < 0 ? -(int)((unsigned)-n / d) : (int)((unsigned)n / d);
}

/* Another shell in flight whose landing mark is on the same cell, or 0. */
static shell_t *mark_shared(shell_t *b)
{
    unsigned char i;
    shell_t *o;

    for (i = 0; i < 2 * SHELLS_PER; ++i) {
        o = &shells[i];
        if (o != b && o->state == SHELL_FLYING && o->cell == b->cell) {
            return o;
        }
    }
    return 0;
}

/* Marks the landing point with a ring in the shooter's colour. */
static void mark_target(shell_t *b, unsigned char color)
{
    shell_t *o;

    b->cell = (b->ty >> 3) * 40 + (b->tx >> 3);
    if ((o = mark_shared(b)) != 0) {
        b->under_ch = o->under_ch;      /* cell already shows a mark */
        b->under_col = o->under_col;
    } else {
        b->under_ch = SCREEN[b->cell];
        b->under_col = COLORRAM[b->cell] & 0x0F;
    }
    SCREEN[b->cell] = IS_ISLAND(b->under_ch) ? MARK_LAND : MARK_CHAR;
    COLORRAM[b->cell] = color;
}

static void unmark_target(shell_t *b)
{
    if (!mark_shared(b)) {
        SCREEN[b->cell] = b->under_ch;
        COLORRAM[b->cell] = b->under_col;
    }
}

/* Launches a shell from the bow toward the crosshair. */
static void fire(unsigned char p)
{
    ship_t *s = &ships[p];
    shell_t *b;
    unsigned char i;
    int sx, sy, ax, ay;

    if (s->reload || s->shots == 0) return;
    for (i = p * SHELLS_PER; i < (p + 1) * SHELLS_PER; ++i) {
        b = &shells[i];
        if (b->state == SHELL_FREE) {
            sx = (s->x >> 4) + bow_x[s->dir];
            sy = (s->y >> 4) + bow_y[s->dir];
            ax = iabs(s->aim_x - sx);
            ay = iabs(s->aim_y - sy);
            /* flight time grows with distance (approximate length) */
            b->flight = 25 + (ax > ay ? ax + ay / 2 : ay + ax / 2) / 5;
            b->t = 0;
            b->x = sx << 4;
            b->y = sy << 4;
            b->tx = s->aim_x;
            b->ty = s->aim_y;
            b->dx = sdiv((s->aim_x - sx) << 4, b->flight);
            b->dy = sdiv((s->aim_y - sy) << 4, b->flight);
            b->state = SHELL_FLYING;
            mark_target(b, s->color);
            s->reload = RELOAD;
            if (--s->shots == 0) {
                s->cooldown = COOLDOWN;
            }
            draw_ammo(p);
            sfx_play(SFX_SHOT);
            return;
        }
    }
}

static void move_aim(ship_t *s, unsigned char joy)
{
    if (joy & JOY_LEFT)  s->aim_x -= AIM_SPEED;
    if (joy & JOY_RIGHT) s->aim_x += AIM_SPEED;
    if (joy & JOY_UP)    s->aim_y -= AIM_SPEED;
    if (joy & JOY_DOWN)  s->aim_y += AIM_SPEED;
    if (s->aim_x < 4)   s->aim_x = 4;
    if (s->aim_x > 315) s->aim_x = 315;
    if (s->aim_y < 12)  s->aim_y = 12;
    if (s->aim_y > 195) s->aim_y = 195;
}

/* Converts velocity (1/256 px) into this frame's move in position units
   (1/16 px), carrying the remainder so slow drifts still add up. */
static int step(int *rem, int v)
{
    int t = *rem + v;
    int move = t >= 0 ? t >> 4 : -((15 - t) >> 4);   /* floor(t / 16) */

    *rem = t - move * 16;
    return move;
}

static void move_ship(unsigned char p)
{
    ship_t *s = &ships[p];
    unsigned char joy = read_joy(p);
    unsigned char contact, pressed;
    int nx, ny, d;

    if (s->reload) --s->reload;
    if (s->cooldown) {
        if (--s->cooldown == 0) {
            s->shots = MAGAZINE;
        }
        if (s->cooldown % COOL_CELL == 0) {
            draw_ammo(p);               /* each bar block, and when reloaded */
        }
    }
    if (s->flash) --s->flash;

    if (joy & JOY_FIRE) {
        /* fire held: the joystick steers the ship */
        if (joy & JOY_DIRS) {
            s->stick_used = 1;
        }
        s->fire_held = 1;
    } else {
        if (s->fire_held) {
            /* released: a plain tap fires, a steering press does not */
            if (!s->stick_used) {
                fire(p);
            }
            s->fire_held = 0;
            s->stick_used = 0;
        }
        /* fire not held: the joystick moves the crosshair, the ship keeps
           its course */
        move_aim(s, joy);
        joy = 0;
    }

    /* rudder: left/right presses put it over, an up press centres it */
    pressed = joy & JOY_DIRS & ~s->prev_dirs;
    s->prev_dirs = joy & JOY_DIRS;
    if (pressed & JOY_RIGHT) s->rudder = 1;
    if (pressed & JOY_LEFT) s->rudder = -1;
    if ((pressed & JOY_UP) && s->rudder) {
        s->rudder = 0;
        s->up_centred = 1;              /* this press doesn't touch the speed */
    }
    if (!(joy & JOY_UP)) {
        s->up_centred = 0;
    }

    if (s->rudder) {
        s->turn_acc += TURN_RATE;
        if (s->turn_acc >= TURN_COST) {
            s->turn_acc -= TURN_COST;
            s->dir = (s->dir + (s->rudder > 0 ? 1 : HEADINGS - 1)) % HEADINGS;
        }
    } else {
        s->turn_acc = TURN_COST - TURN_RATE;    /* next rudder turns immediately */
    }

    /* fire + up/down moves the engine setting */
    if ((joy & JOY_UP) && !s->up_centred) {
        s->setpoint += SET_STEP;
        if (s->setpoint > MAX_SPEED) s->setpoint = MAX_SPEED;
    } else if (joy & JOY_DOWN) {
        s->setpoint -= SET_STEP;
        if (s->setpoint < -MAX_REVERSE) s->setpoint = -MAX_REVERSE;
    }

    /* the speed follows the setting with a delay: an eighth of the gap per
       frame, at least 1 and at most SPEED_STEP; nothing else slows it down */
    d = (s->setpoint - s->speed) / 8;
    if (d == 0 && s->setpoint != s->speed) d = s->setpoint > s->speed ? 1 : -1;
    if (d > SPEED_STEP) d = SPEED_STEP;
    if (d < -SPEED_STEP) d = -SPEED_STEP;
    s->speed += d;

    if (slider_cell(s->setpoint) != s->knob_cell || slider_cell(s->speed) != s->fill_cell) {
        draw_slider(p);
    }

    /* the ship moves the way its bow points (head_x/y are scaled by 32) */
    s->vx = head_x[s->dir] * s->speed / 32;
    s->vy = head_y[s->dir] * s->speed / 32;

    nx = s->x + step(&s->rx, s->vx);
    ny = s->y + step(&s->ry, s->vy);
    contact = ship_contact(nx, ny, s->dir);
    if (contact == CONTACT_EDGE) {
        s->speed = 0;
    } else {
        s->x = nx;
        s->y = ny;
        if (contact == CONTACT_LAND) {
            /* ran aground: wrecked on the spot */
            s->hp = 0;
            s->speed = 0;
            draw_hud();
        }
    }
}

/* One heart lost: flash, sound, HUD. */
static void damage(unsigned char p)
{
    ship_t *s = &ships[p];

    if (s->hp) --s->hp;
    s->flash = 16;
    sfx_play(SFX_HIT);
    draw_hud();
}

/* Shell or iceberg: a heart, and the blow takes half the speed. */
static void hit(unsigned char p)
{
    damage(p);
    ships[p].speed /= 2;
}

/* ------------------------------------------------------------------ */
/* Icebergs                                                           */

static unsigned char iceberg_near(iceberg_t *ice, unsigned char col, unsigned char row, unsigned char dist);

static unsigned char sea_char(unsigned char col, unsigned char row)
{
    return (col * 7 + row * 13) % 17 == 0 ? WAVE_CHAR : ' ';
}

/* Open sea around (col,row), nothing but water or waves in the 3x3 block. */
static unsigned char open_sea(unsigned char col, unsigned char row)
{
    unsigned char x, y, c;

    for (y = row - 1; y <= row + 1; ++y) {
        for (x = col - 1; x <= col + 1; ++x) {
            c = SCREEN[y * 40 + x];
            if (c != ' ' && c != WAVE_CHAR) return 0;
        }
    }
    return 1;
}

/* 1 or 2 icebergs on open sea, away from the start positions (row 13,
   columns 4 and 35). */
static void place_icebergs(void)
{
    unsigned char i, tries, col, row;
    iceberg_t *ice;

    ice_count = 1 + (rand() & 1);
    for (i = 0; i < ice_count; ++i) {
        ice = &icebergs[i];
        ice->alive = 0;
        ice->shown = 0;
        for (tries = 0; tries < 100; ++tries) {
            col = 3 + rand() % 34;
            row = 3 + rand() % 19;
            if ((col < 10 || col > 29) && row > 9 && row < 17) continue;
            if (!open_sea(col, row)) continue;
            if (i && iceberg_near(&icebergs[0], col, row, 3)) continue;
            ice->col = col;
            ice->row = row;
            ice->alive = 1;
            break;
        }
    }
}

static unsigned char iceberg_near(iceberg_t *ice, unsigned char col, unsigned char row, unsigned char dist)
{
    return (unsigned char)(ice->col - col + dist) <= 2 * dist
        && (unsigned char)(ice->row - row + dist) <= 2 * dist;
}

static void show_iceberg(iceberg_t *ice, unsigned char show)
{
    unsigned int o = ice->row * 40 + ice->col;
    unsigned char c = SCREEN[o];

    if (c == MARK_CHAR || c == MARK_LAND) return;  /* a landing mark is on top: later */
    SCREEN[o] = show ? ICE_CHAR : sea_char(ice->col, ice->row);
    COLORRAM[o] = show ? COLOR_WHITE : COLOR_LIGHTBLUE;
    ice->shown = show;
}

/* Reveals icebergs next to a ship, hides them again when it leaves, and
   makes a ship that touches one (centre, bow or stern) pay a heart. */
static void update_icebergs(void)
{
    unsigned char i, k, nearby, cx, cy;
    int px, py;
    iceberg_t *ice;
    ship_t *s;

    for (i = 0; i < ice_count; ++i) {
        ice = &icebergs[i];
        if (!ice->alive) continue;
        nearby = 0;
        for (k = 0; k < 2; ++k) {
            s = &ships[k];
            if (!s->hp) continue;
            px = s->x >> 4;
            py = s->y >> 4;
            cx = (unsigned char)(px >> 3);
            cy = (unsigned char)(py >> 3);
            if (iceberg_near(ice, cx, cy, 1)) nearby = 1;
            if ((cx == ice->col && cy == ice->row)
                || (((px + bow_x[s->dir]) >> 3) == ice->col && ((py + bow_y[s->dir]) >> 3) == ice->row)
                || (((px - bow_x[s->dir]) >> 3) == ice->col && ((py - bow_y[s->dir]) >> 3) == ice->row)) {
                hit(k);
                ice->alive = 0;
                show_iceberg(ice, 0);
                ice->shown = 0;
                nearby = 0;
                break;
            }
        }
        if (ice->alive && nearby != ice->shown) {
            show_iceberg(ice, nearby);
        }
    }
}

/* The shell comes down at its target: hits any ship there, else splashes. */
static void land(shell_t *b)
{
    unsigned char k;
    ship_t *t;

    unmark_target(b);
    b->state = SHELL_SPLASH;
    b->t = SPLASH_TIME;
    b->block = BLK_SPLASH;
    b->color = solid_at(b->tx, b->ty) ? COLOR_BROWN : COLOR_LIGHTBLUE;
    for (k = 0; k < 2; ++k) {
        t = &ships[k];
        if (t->hp && iabs(b->tx - (t->x >> 4)) < HIT_RANGE && iabs(b->ty - (t->y >> 4)) < HIT_RANGE) {
            b->block = BLK_BOOM;
            b->color = COLOR_ORANGE;
            hit(k);
            return;
        }
    }
    sfx_play(SFX_SPLASH);
}

static void move_shells(void)
{
    unsigned char i;
    shell_t *b;

    for (i = 0; i < 2 * SHELLS_PER; ++i) {
        b = &shells[i];
        if (b->state == SHELL_FLYING) {
            b->x += b->dx;
            b->y += b->dy;
            if (++b->t >= b->flight) {
                land(b);
            }
        } else if (b->state == SHELL_SPLASH) {
            if (--b->t == 0) {
                b->state = SHELL_FREE;
            }
        }
    }
}

static unsigned char ships_touching;

/* Ramming: when the ships first touch, each loses a heart; they keep their
   course and speed and can sail on through each other. */
static void ships_collide(void)
{
    int dx = (ships[0].x >> 4) - (ships[1].x >> 4);
    int dy = (ships[0].y >> 4) - (ships[1].y >> 4);

    if (dx > -14 && dx < 14 && dy > -14 && dy < 14) {
        if (!ships_touching) {
            damage(0);
            damage(1);
            ships_touching = 1;
        }
    } else {
        ships_touching = 0;
    }
}

static void new_round(void)
{
    unsigned char i;

    memset(ships, 0, sizeof(ships));
    memset(shells, 0, sizeof(shells));

    ships[0].x = 32 * 16;  ships[0].y = 104 * 16; ships[0].dir = 0;
    ships[1].x = 287 * 16; ships[1].y = 104 * 16; ships[1].dir = HEADINGS / 2;
    ships[0].hp = ships[1].hp = MAX_HP;
    ships[0].shots = ships[1].shots = MAGAZINE;
    ships[0].color = COLOR_YELLOW;
    ships[1].color = COLOR_LIGHTRED;
    /* nearby hue, similar brightness: shows the reload at a glance */
    ships[0].cool_color = COLOR_LIGHTGREEN;
    ships[1].cool_color = COLOR_VIOLET;
    ships[0].aim_x = 32 + AIM_AHEAD;  ships[0].aim_y = 104;
    ships[1].aim_x = 287 - AIM_AHEAD; ships[1].aim_y = 104;
    /* fire may still be held from the previous screen: ignore that press */
    ships[0].fire_held = ships[1].fire_held = 1;
    ships[0].stick_used = ships[1].stick_used = 1;
    for (i = 0; i < 2; ++i) {
        SPRCOLOR[SPR_AIM(i)] = ships[i].color;
    }

    draw_map();
    place_icebergs();
    draw_hud();
    update_sprites();
}

#define DRAW 2

/* Plays one round, returns the winner (0 or 1) or DRAW if both ships sank. */
static unsigned char play_round(void)
{
    unsigned char k, t, lost = 0;

    new_round();
    ships_touching = 0;
    while (ships[0].hp && ships[1].hp) {
        wait_frame();
        update_sprites();
        move_ship(0);
        move_ship(1);
        ships_collide();
        update_icebergs();
        move_shells();
    }

    /* both can go down in the same frame */
    for (k = 0; k < 2; ++k) {
        if (!ships[k].hp) {
            lost |= 1 << SPR_SHIP(k);
            SPRPTR[SPR_SHIP(k)] = BLK_BOOM;
        }
    }
    sfx_play(SFX_SINK);
    VIC.spr_ena = (1 << SPR_SHIP(0)) | (1 << SPR_SHIP(1));
    for (t = 0; t < 100; ++t) {
        wait_frame();
        for (k = 0; k < 2; ++k) {
            if (!ships[k].hp) {
                SPRCOLOR[SPR_SHIP(k)] = (t & 4) ? COLOR_YELLOW : COLOR_ORANGE;
            }
        }
        if (t == 70) {
            VIC.spr_ena &= ~lost;
        }
    }
    if (!ships[0].hp && !ships[1].hp) {
        return DRAW;
    }
    return ships[0].hp ? 0 : 1;
}

static void title(void)
{
    draw_sea();
    VIC.spr_ena = 0;
    print_centered(4, "b a t t l e s h i p s", COLOR_WHITE);
    print_centered(8, "player 1  joystick port 2", COLOR_YELLOW);
    print_centered(10, "player 2  joystick port 1", COLOR_LIGHTRED);
    print_centered(13, "joystick   aim", COLOR_WHITE);
    print_centered(14, "fire + left/right/up   rudder", COLOR_WHITE);
    print_centered(15, "fire + up/down   engine setting", COLOR_WHITE);
    print_centered(16, "tap fire   shoot", COLOR_WHITE);
    print_centered(18, "shells land on your cross", COLOR_CYAN);
    print_centered(19, "3 shots, then a 2.5 s reload", COLOR_CYAN);
    print_centered(20, "three hits sink a ship", COLOR_CYAN);
    print_centered(21, "running aground wrecks it", COLOR_CYAN);
    print_centered(22, "new sea every round, beware icebergs", COLOR_WHITE);
    print_centered(23, "press fire to start", COLOR_YELLOW);
}

static void draw_music_choice(void)
{
    char line[32];
    unsigned char x;

    for (x = 0; x < 40; ++x) {
        SCREEN[6 * 40 + x] = sea_char(x, 6);
        COLORRAM[6 * 40 + x] = COLOR_LIGHTBLUE;
    }
    strcpy(line, "< music: ");
    strcat(line, song_names[song]);
    strcat(line, " >");
    print_centered(6, line, COLOR_LIGHTGREEN);
}

/* Title screen: left/right changes the music (it plays straight away),
   fire starts the game. */
static void title_menu(void)
{
#ifndef AUTOPLAY
    unsigned char joy, pressed, prev = 0xFF;    /* ignore anything already held */
#endif

    title();
    draw_music_choice();
#ifdef AUTOPLAY
    wait_fire();
#else
    for (;;) {
        wait_frame();
        joy = read_joy(0) | read_joy(1);
        pressed = joy & ~prev;
        prev = joy;
        if (pressed & (JOY_LEFT | JOY_RIGHT)) {
            song = (song + ((pressed & JOY_RIGHT) ? 1 : SONGS - 1)) % SONGS;
            music_select(song);
            draw_music_choice();
        }
        if (pressed & JOY_FIRE) {
            break;
        }
    }
#endif
}

extern char _BSS_RUN__[], _BSS_SIZE__[];

int main(void)
{
    unsigned char w;

    CIA2.pra = (CIA2.pra & 0xFC) | 0x01; /* VIC bank 2 ($8000-$BFFF) */
    memcpy(CHARSET, charset_data, sizeof(charset_data));
    VIC.addr = 0x08;                    /* screen +$0000, charset +$2000 ($A000) */
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0 = COLOR_BLUE;
    draw_sea();                         /* the new screen RAM holds garbage */

    if ((unsigned)_BSS_RUN__ + (unsigned)_BSS_SIZE__ > VIC_BANK) {
        print(0, 0, "program overlaps screen memory", COLOR_WHITE);
        return 1;
    }

    /* Joysticks are read through CIA1 ports A/B with no keyboard column
       selected. sound_start() turns off the KERNAL keyboard scan, which
       would otherwise change $DC00 under us. */
    CIA1.ddra = 0xFF;
    CIA1.ddrb = 0x00;
    CIA1.pra = 0xFF;

    make_sprites();
    sound_start(song);
    SPRPTR[SPR_AIM(0)] = SPRPTR[SPR_AIM(1)] = BLK_AIM;

    title_menu();
    srand(((unsigned)frame << 8) | VIC.rasterline);   /* players' timing seeds the maps */

    for (;;) {
        w = play_round();
        if (w == DRAW) {
            draw_hud();
            print_centered(11, " both ships lost! ", COLOR_WHITE);
        } else {
            ++wins[w];
            if (wins[w] > 99) wins[w] = 99;
            draw_hud();
            print_centered(11, w ? " player 2 wins! " : " player 1 wins! ",
                           w ? COLOR_LIGHTRED : COLOR_YELLOW);
        }
        print_centered(13, " press fire ", COLOR_WHITE);
        wait_fire();
    }
}
