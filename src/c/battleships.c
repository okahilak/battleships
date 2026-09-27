/*
 * BATTLESHIPS - real-time two player naval duel, top view.
 *
 *   Player 1 (yellow, starts left):  joystick in port 2
 *   Player 2 (red, starts right):    joystick in port 1
 *
 *   joystick           move your crosshair (the ship keeps its course)
 *   fire + left/right  turn the ship (10-degree steps)
 *   fire + up          engines ahead (ships are heavy: speed builds up
 *                      slowly, and nothing but the engines slows them down)
 *   fire + down        engines astern / brake
 *   The ship always moves the way its bow points.
 *   tap fire           fire a shell; it flies in an arc and lands on the
 *                      crosshair after a delay, hitting any ship there
 *
 * Each ship takes 5 hits. A ship that runs aground on an island is wrecked
 * at once. The screen edges just stop it.
 *
 * Build with -DAUTOPLAY to let two simple bots play (used for testing).
 */
#include <c64.h>
#include <string.h>

#include "ship_sprites.h"         /* generated: ship frames and heading tables */

/* The VIC-II looks at bank 2 ($8000-$BFFF): screen at $8000, sprite graphics
   from $8400, character ROM visible at $9000. That leaves $0801-$7FFF for the
   program; main() checks it ends below $8000. */
#define VIC_BANK    0x8000
#define SCREEN      ((unsigned char *)0x8000)
#define COLORRAM    ((unsigned char *)0xD800)
#define SPRPTR      ((unsigned char *)0x83F8)
#define SPRPOS      ((unsigned char *)0xD000)      /* x,y pairs */
#define SPRCOLOR    (&VIC.spr0_color)

#define SPRDATA     ((unsigned char *)0x8400)
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

#define ISLAND_CHAR 160                 /* reverse space */
#define WAVE_CHAR   100                 /* thin line at bottom of cell */

#define JOY_UP      0x01
#define JOY_DOWN    0x02
#define JOY_LEFT    0x04
#define JOY_RIGHT   0x08
#define JOY_FIRE    0x10
#define JOY_DIRS    (JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT)

#define MAX_HP      5
/* Ship physics. Velocity is in 1/256 pixel per frame. */
#define MAX_SPEED   192                 /* ahead: 0.75 pixels per frame */
#define MAX_REVERSE 64                  /* astern */
#define SPEED_STEP  1                   /* engine change per frame: 0 to full in ~4 s */
#define TURN_DELAY  5                   /* frames per 10-degree heading step */
#define RELOAD      30                  /* frames between shots */
#define SHELLS_PER  2                   /* shells in flight per player */
#define AIM_SPEED   2                   /* crosshair pixels per frame */
#define AIM_AHEAD   64                  /* crosshair start distance from ship */
#define HIT_RANGE   10                  /* pixels from ship centre that count as a hit */
#define SPLASH_TIME 20                  /* frames the impact stays visible */

typedef struct {
    int x, y;                   /* centre, 1/16 pixel */
    int speed;                  /* along the heading, 1/256 pixel per frame */
    int vx, vy;                 /* resulting velocity, derived each frame */
    int rx, ry;                 /* sub-step remainder, 0..15 */
    unsigned char dir;
    unsigned char turn_wait;
    unsigned char reload;
    unsigned char hp;
    unsigned char flash;
    unsigned char color;
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
} shell_t;

typedef struct { unsigned char c, r, w, h; } island_t;

/* Left half of the map; each island is mirrored onto the right half. */
static const island_t islands[] = {
    {  9,  4, 2, 2 },
    { 14, 15, 2, 2 },
    {  6, 18, 2, 2 },
    { 19, 11, 2, 3 },               /* centre: mirrors onto itself */
};

static ship_t ships[2];
static shell_t shells[2 * SHELLS_PER];  /* player p owns p*SHELLS_PER.. */
static unsigned char wins[2];
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

static void draw_island(unsigned char c, unsigned char r, unsigned char w, unsigned char h)
{
    unsigned char x, y;
    unsigned int o;

    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            /* round off the corners of bigger islands */
            if (w > 2 && h > 2 && (x == 0 || x == w - 1) && (y == 0 || y == h - 1)) {
                continue;
            }
            o = (r + y) * 40 + c + x;
            SCREEN[o] = ISLAND_CHAR;
            COLORRAM[o] = COLOR_GREEN;
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

static void draw_map(void)
{
    unsigned char i;
    const island_t *is;

    draw_sea();
    for (i = 0; i < sizeof(islands) / sizeof(islands[0]); ++i) {
        is = &islands[i];
        draw_island(is->c, is->r, is->w, is->h);
        draw_island(40 - is->c - is->w, is->r, is->w, is->h);
    }
}

static void put_number(unsigned char col, unsigned char n, unsigned char color)
{
    SCREEN[col] = '0' + n / 10;
    SCREEN[col + 1] = '0' + n % 10;
    COLORRAM[col] = color;
    COLORRAM[col + 1] = color;
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
        SCREEN[32 + i] = 83;
        COLORRAM[32 + i] = i < ships[1].hp ? COLOR_LIGHTRED : COLOR_GRAY1;
    }
    put_number(17, wins[0], COLOR_YELLOW);
    print(19, 0, "-", COLOR_WHITE);
    put_number(20, wins[1], COLOR_LIGHTRED);
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
            SPRCOLOR[n] = (sh->flash & 2) ? COLOR_WHITE : sh->color;
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
void sound_start(void);
void __fastcall__ sfx_play(unsigned char effect);

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

    if (s->reload == 0 && (frame & 31) == me * 16) {
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
    if (diff != 0) {
        joy |= diff <= HEADINGS / 2 ? JOY_RIGHT : JOY_LEFT;
    }
    /* cruise at about half speed, brake when close */
    if (ax + ay > 60 && s->speed < 100) {
        joy |= JOY_UP;
    } else if (ax + ay <= 60 && s->speed > 0) {
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

/* Only for points inside the sea area (check off_sea first). */
static unsigned char land_at(int px, int py)
{
    return SCREEN[(py >> 3) * 40 + (px >> 3)] == ISLAND_CHAR;
}

static unsigned char solid_at(int px, int py)
{
    return off_sea(px, py) || land_at(px, py);
}

enum { CONTACT_NONE, CONTACT_EDGE, CONTACT_LAND };

/* What the ship's centre, bow and stern touch at position (x,y). */
static unsigned char ship_contact(int x, int y, unsigned char d)
{
    int px = x >> 4;
    int py = y >> 4;
    int bx = bow_x[d];
    int by = bow_y[d];

    if (off_sea(px, py) || off_sea(px + bx, py + by) || off_sea(px - bx, py - by)) {
        return CONTACT_EDGE;
    }
    if (land_at(px, py) || land_at(px + bx, py + by) || land_at(px - bx, py - by)) {
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

/* Launches a shell from the bow toward the crosshair. */
static void fire(unsigned char p)
{
    ship_t *s = &ships[p];
    shell_t *b;
    unsigned char i;
    int sx, sy, ax, ay;

    if (s->reload) return;
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
            s->reload = RELOAD;
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
    unsigned char contact;
    int nx, ny;

    if (s->reload) --s->reload;
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

    if (joy & (JOY_LEFT | JOY_RIGHT)) {
        if (s->turn_wait == 0) {
            s->dir = (s->dir + ((joy & JOY_RIGHT) ? 1 : HEADINGS - 1)) % HEADINGS;
            s->turn_wait = TURN_DELAY;
        } else {
            --s->turn_wait;
        }
    } else {
        s->turn_wait = 0;               /* taps turn immediately */
    }

    /* engines change the speed slowly; nothing else slows the ship down */
    if (joy & JOY_UP) {
        s->speed += SPEED_STEP;
        if (s->speed > MAX_SPEED) s->speed = MAX_SPEED;
    } else if (joy & JOY_DOWN) {
        s->speed -= SPEED_STEP;
        if (s->speed < -MAX_REVERSE) s->speed = -MAX_REVERSE;
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

static void hit(unsigned char p)
{
    ship_t *s = &ships[p];

    if (s->hp) --s->hp;
    s->flash = 16;
    s->speed /= 2;
    sfx_play(SFX_HIT);
    draw_hud();
}

/* The shell comes down at its target: hits any ship there, else splashes. */
static void land(shell_t *b)
{
    unsigned char k;
    ship_t *t;

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

static void ships_collide(int ox0, int oy0, int ox1, int oy1)
{
    int dx = (ships[0].x >> 4) - (ships[1].x >> 4);
    int dy = (ships[0].y >> 4) - (ships[1].y >> 4);

    if (dx > -14 && dx < 14 && dy > -14 && dy < 14) {
        ships[0].x = ox0; ships[0].y = oy0; ships[0].speed = 0;
        ships[1].x = ox1; ships[1].y = oy1; ships[1].speed = 0;
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
    ships[0].color = COLOR_YELLOW;
    ships[1].color = COLOR_LIGHTRED;
    ships[0].aim_x = 32 + AIM_AHEAD;  ships[0].aim_y = 104;
    ships[1].aim_x = 287 - AIM_AHEAD; ships[1].aim_y = 104;
    /* fire may still be held from the previous screen: ignore that press */
    ships[0].fire_held = ships[1].fire_held = 1;
    ships[0].stick_used = ships[1].stick_used = 1;
    for (i = 0; i < 2; ++i) {
        SPRCOLOR[SPR_AIM(i)] = ships[i].color;
    }

    draw_map();
    draw_hud();
    update_sprites();
}

#define DRAW 2

/* Plays one round, returns the winner (0 or 1) or DRAW if both ships sank. */
static unsigned char play_round(void)
{
    int ox0, oy0, ox1, oy1;
    unsigned char k, t, lost = 0;

    new_round();
    while (ships[0].hp && ships[1].hp) {
        wait_frame();
        update_sprites();
        ox0 = ships[0].x; oy0 = ships[0].y;
        ox1 = ships[1].x; oy1 = ships[1].y;
        move_ship(0);
        move_ship(1);
        ships_collide(ox0, oy0, ox1, oy1);
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
    print_centered(14, "fire + left/right   turn", COLOR_WHITE);
    print_centered(15, "fire + up/down   engines", COLOR_WHITE);
    print_centered(16, "tap fire   shoot", COLOR_WHITE);
    print_centered(18, "shells land on your cross", COLOR_CYAN);
    print_centered(19, "five hits sinks a ship", COLOR_CYAN);
    print_centered(20, "running aground wrecks it", COLOR_CYAN);
    print_centered(22, "press fire to start", COLOR_YELLOW);
}

extern char _BSS_RUN__[], _BSS_SIZE__[];

int main(void)
{
    unsigned char w;

    CIA2.pra = (CIA2.pra & 0xFC) | 0x01; /* VIC bank 2 ($8000-$BFFF) */
    VIC.addr = 0x04;                    /* screen +$0000, uppercase/graphics charset +$1000 */
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
    sound_start();
    SPRPTR[SPR_AIM(0)] = SPRPTR[SPR_AIM(1)] = BLK_AIM;

    title();
    wait_fire();

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
