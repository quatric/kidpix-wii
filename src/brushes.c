/* Wacky Brush: the 28 brushes of the Wacky Brush sub-menu. */
#include <stdio.h>
#include "tools_int.h"

enum {
    B_LEAKY, B_ZIGZAG, B_DOTS, B_BUBBLY, B_PIES, B_ECHOES, B_NORTHERN, B_FUZZER,
    B_MAGNIFY, B_SPRAY, B_PINES, B_3D, B_KALEIDO, B_CONNECT, B_SWIRL, B_ROTDOTS,
    B_INVERTER, B_GEOMETRY, B_XY, B_TREE, B_SPLATTER, B_STARBURST, B_LOOPER,
    B_GALAXY, B_XOS, B_CARDS, B_SHAPES, B_PAW
};

static const char *icons[BRUSH_COUNT] = {
    "img/tool-menu-wacky-brush-70.png", "img/tool-menu-wacky-brush-71.png",
    "img/tool-menu-wacky-brush-72.png", "img/tool-menu-wacky-brush-73.png",
    "img/tool-menu-wacky-brush-74.png", "img/tool-menu-wacky-brush-75.png",
    "img/tool-menu-wacky-brush-76.png", "img/tool-menu-wacky-brush-77.png",
    "img/tool-menu-wacky-brush-78.png", "img/tool-menu-wacky-brush-79.png",
    "img/tool-menu-wacky-brush-80.png", "img/tool-menu-wacky-brush-81.png",
    "img/tool-menu-wacky-brush-82.png", "img/tool-menu-wacky-brush-84.png",
    "img/tool-menu-wacky-brush-86.png", "img/br12.png",
    "img/tool-menu-wacky-brush-87.png", "img/tool-menu-wacky-brush-88.png",
    "img/br16.png", "img/tool-menu-wacky-brush-89.png",
    "img/tool-menu-wacky-brush-91.png", "img/br-starburst.png",
    "img/tool-menu-wacky-brush-92.png", "img/tool-menu-wacky-brush-94.png",
    "img/tool-menu-wacky-brush-95.png", "img/tool-menu-wacky-brush-96.png",
    "img/tool-menu-wacky-brush-97.png", NULL /* paw: drawn */
};

static const char *sounds[BRUSH_COUNT] = {
    "kidpix-submenu-brush-leaky-pen", "kidpix-submenu-brush-zigzag", "kidpix-submenu-brush-dots",
    "kidpix-submenu-brush-bubbly", "kidpix-submenu-brush-pies", "kidpix-submenu-brush-owl",
    "kidpix-submenu-brush-northern", "kidpix-submenu-brush-fuzzer", "kidpix-submenu-brush-zoom",
    "kidpix-submenu-brush-spraypaint", "kidpix-submenu-brush-pines", "kidpix-tool-pencil",
    "kidpix-submenu-brush-kaliediscope", "kidpix-submenu-brush-twirly", "kidpix-submenu-brush-twirly",
    "kidpix-submenu-brush-rollingdots", "kidpix-submenu-brush-inverter", "kidpix-submenu-brush-guilloche",
    "kidpix-submenu-brush-xy-during", "kidpix-submenu-brush-tree", "kidpix-submenu-brush-bubbly",
    "kidpix-tool-line-during", "kidpix-submenu-brush-rollingdots", "kidpix-submenu-brush-stars",
    "kidpix-submenu-brush-xos", "kidpix-submenu-brush-cards", "kidpix-submenu-brush-shapes",
    "kidpix-submenu-brush-prints"
};

const char *brush_icon(int b) { return icons[b]; }

typedef struct { int x, y, len, max; bool on; } Drip;
static Drip drips[48];
static int shape_i, side;
static float loop_t, swirl_t;
static int loop_px, loop_py;
static bool loop_have;
static int zz_x, zz_y;
static bool zz_have;

void brush_select(int b) { (void)b; }

static Paint solid(void) { Paint p = { st.color, 1 }; return p; }
static void sfx(void) { snd_play_idle(sounds[st.brush]); }

/* ---- shape helpers ------------------------------------------------------ */
static void star(int cx, int cy, int r, int pts, bool filled, u32 c)
{
    int v[32 * 2];
    const int n = pts * 2;
    for (int i = 0; i < n; i++) {
        const float a = -1.5707963f + i * 3.1415926f / pts;
        const float rr = (i & 1) ? r * 0.45f : (float)r;
        v[i * 2] = cx + (int)(cosf(a) * rr);
        v[i * 2 + 1] = cy + (int)(sinf(a) * rr);
    }
    if (filled) s_fill_poly(c, v, n);
    else for (int i = 0; i < n; i++) {
        const int j = (i + 1) % n;
        s_line(c, v[i * 2], v[i * 2 + 1], v[j * 2], v[j * 2 + 1], 1);
    }
}

static void ngon(int cx, int cy, int r, int n, float rot, bool filled, u32 c)
{
    int v[24 * 2];
    for (int i = 0; i < n; i++) {
        const float a = rot + i * 6.2831853f / n;
        v[i * 2] = cx + (int)(cosf(a) * r);
        v[i * 2 + 1] = cy + (int)(sinf(a) * r);
    }
    if (filled) s_fill_poly(c, v, n);
    else for (int i = 0; i < n; i++) {
        const int j = (i + 1) % n;
        s_line(c, v[i * 2], v[i * 2 + 1], v[j * 2], v[j * 2 + 1], 2);
    }
}

static void disc(int cx, int cy, int size, u32 c) { Paint p = { c, 1 }; p_disc(&p, cx, cy, size); }

static void suit(int cx, int cy, int r, int kind, u32 c)
{
    switch (kind & 3) {
    case 0: { /* spade */
        disc(cx - r / 2, cy + r / 6, r, c);
        disc(cx + r / 2, cy + r / 6, r, c);
        int t[6] = { cx - r + 1, cy + r / 6, cx + r - 1, cy + r / 6, cx, cy - r };
        s_fill_poly(c, t, 3);
        int s[6] = { cx, cy + r / 6, cx - r / 3, cy + r, cx + r / 3, cy + r };
        s_fill_poly(c, s, 3);
        break; }
    case 1: { /* club */
        disc(cx, cy - r / 2, r, c);
        disc(cx - r / 2, cy + r / 4, r, c);
        disc(cx + r / 2, cy + r / 4, r, c);
        int s[6] = { cx, cy, cx - r / 3, cy + r, cx + r / 3, cy + r };
        s_fill_poly(c, s, 3);
        break; }
    case 2: { /* heart */
        disc(cx - r / 2, cy - r / 4, r, c);
        disc(cx + r / 2, cy - r / 4, r, c);
        int t[6] = { cx - r + 1, cy - r / 8, cx + r - 1, cy - r / 8, cx, cy + r };
        s_fill_poly(c, t, 3);
        break; }
    default: { /* diamond */
        int d[8] = { cx, cy - r, cx + r * 2 / 3, cy, cx, cy + r, cx - r * 2 / 3, cy };
        s_fill_poly(c, d, 4);
        break; }
    }
}

static void paw(int cx, int cy, float a, int r, u32 c)
{
    const float ca = cosf(a), sa = sinf(a);
#define PT(lx, ly) (cx + (int)((lx) * ca - (ly) * sa)), (cy + (int)((lx) * sa + (ly) * ca))
    disc(PT(-r * 0.15f, 0), (int)(r * 1.5f), c);
    disc(PT(r * 0.9f, -r * 0.75f), (int)(r * 0.7f), c);
    disc(PT(r * 1.25f, -r * 0.25f), (int)(r * 0.7f), c);
    disc(PT(r * 1.25f, r * 0.25f), (int)(r * 0.7f), c);
    disc(PT(r * 0.9f, r * 0.75f), (int)(r * 0.7f), c);
#undef PT
}

/* ---- spaced stamps ------------------------------------------------------ */
typedef void (*StampFn)(int, int, float);

static void along(int x, int y, float spacing, StampFn fn)
{
    const float dx = x - S.px, dy = y - S.py;
    const float d = sqrtf(dx * dx + dy * dy);
    if (d < 0.01f) return;
    const float ux = dx / d, uy = dy / d, ang = atan2f(dy, dx);
    S.dist += d;
    while (S.dist >= spacing) {
        S.dist -= spacing;
        const float t = d - S.dist;
        fn((int)(S.px + ux * t), (int)(S.py + uy * t), ang);
    }
}

static void st_dots(int x, int y, float a) { (void)a; disc(x, y, 16 * SCALE(), st.color); sfx(); }

static void st_pies(int x, int y, float a)
{
    (void)a;
    const int r = 15 * SCALE();
    const float a0 = rnd(360) * 0.01745f, span = (60 + rnd(150)) * 0.01745f;
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            if (i * i + j * j > r * r) continue;
            float an = atan2f((float)j, (float)i) - a0;
            while (an < 0) an += 6.2831853f;
            cv_put(x + i, y + j, an < span ? 0xffffffff : st.color);
        }
    s_circle(0x000000ff, x, y, r, 1);
    sfx();
}

static void st_stars(int x, int y, float a)
{
    (void)a;
    const int r = (8 + rnd(10)) * SCALE();
    star(x, y, r, 4 + rnd(3), (shape_i++ & 1) == 0, st.color);
    sfx();
}

static void st_xos(int x, int y, float a)
{
    (void)a;
    const int r = 11 * SCALE();
    if (shape_i++ & 1) s_circle(st.color, x, y, r, 3);
    else { s_line(st.color, x - r, y - r, x + r, y + r, 3); s_line(st.color, x + r, y - r, x - r, y + r, 3); }
    sfx();
}

static void st_cards(int x, int y, float a)
{
    (void)a;
    const int k = shape_i++;
    suit(x, y, 12 * SCALE(), k, (k & 3) >= 2 ? 0xff0000ff : st.color);
    sfx();
}

static void st_shapes(int x, int y, float a)
{
    (void)a;
    const int r = 13 * SCALE(), k = shape_i++ % 8;
    switch (k) {
    case 0: ngon(x, y, r, 3, -1.5707963f, true, st.color); break;
    case 1: ngon(x, y, r, 4, 0.7853982f, false, st.color); break;
    case 2: ngon(x, y, r, 5, -1.5707963f, true, st.color); break;
    case 3: s_circle(st.color, x, y, r, 3); break;
    case 4: ngon(x, y, r, 6, 0, false, st.color); break;
    case 5: ngon(x, y, r, 4, 0, true, st.color); break;
    case 6: ngon(x, y, r, 3, 1.5707963f, false, st.color); break;
    default: disc(x, y, r * 2, st.color); break;
    }
    sfx();
}

static void st_paw(int x, int y, float a)
{
    side = -side;
    const int r = 6 * SCALE();
    const float nx = -sinf(a), ny = cosf(a);
    paw(x + (int)(nx * r * 1.2f * side), y + (int)(ny * r * 1.2f * side), a, r, st.color);
    sfx();
}

static void st_connect(int x, int y, float a)
{
    (void)a;
    if (S.lx >= 0) s_line(st.color, S.lx, S.ly, x, y, 1);
    disc(x, y, 6, st.color);
    S.lx = x; S.ly = y;
    sfx();
}

static void st_zig(int x, int y, float a)
{
    const float nx = -sinf(a), ny = cosf(a);
    const int amp = 11 * SCALE();
    side = -side;
    const int vx = x + (int)(nx * amp * side), vy = y + (int)(ny * amp * side);
    if (zz_have) s_line(st.color, zz_x, zz_y, vx, vy, 3);
    zz_x = vx; zz_y = vy; zz_have = true;
    sfx();
}

/* ---- composite helpers ---------------------------------------------------- */
static void tree(int x, int y, float ang, float len, int depth)
{
    if (depth == 0) {
        disc(x, y, 6 + rnd(5), RGBA(30, 140 + rnd(80), 40, 255));
        return;
    }
    const int x2 = x + (int)(cosf(ang) * len), y2 = y + (int)(sinf(ang) * len);
    s_line(depth > 3 ? RGBA(110, 70, 30, 255) : RGBA(40, 130, 40, 255), x, y, x2, y2, depth > 4 ? depth - 2 : 2);
    tree(x2, y2, ang - (0.25f + rnd(30) * 0.01f), len * 0.74f, depth - 1);
    tree(x2, y2, ang + (0.25f + rnd(30) * 0.01f), len * 0.74f, depth - 1);
}

static void lens(int cx, int cy)
{
    const u32 *src = undo_peek();
    const int R = 52;
    for (int j = -R; j <= R; j++)
        for (int i = -R; i <= R; i++) {
            const int d2 = i * i + j * j;
            if (d2 > R * R) continue;
            const int sx = cx + i / 2, sy = cy + j / 2;
            if ((unsigned)sx < CV_W && (unsigned)sy < CV_H)
                cv_put(cx + i, cy + j, src[sy * CV_W + sx]);
        }
    s_circle(0x000000ff, cx, cy, R, 3);
}

static void invert_disc(int cx, int cy, int r)
{
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            if (i * i + j * j > r * r) continue;
            const int x = cx + i, y = cy + j;
            if ((unsigned)x < CV_W && (unsigned)y < CV_H) cv[y * CV_W + x] ^= 0xffffff00;
        }
}

static void guilloche(int cx, int cy, int dist)
{
    if (dist < 6) return;
    const int k = 3 + (dist / 50) % 5;
    const float d = 0.85f, sc = dist / (k - 1 + d);
    float px = 0, py = 0;
    for (int i = 0; i <= 720; i++) {
        const float t = i * 6.2831853f / 720.0f;
        const float x = cx + sc * ((k - 1) * cosf(t) + d * cosf((k - 1) * t));
        const float y = cy + sc * ((k - 1) * sinf(t) - d * sinf((k - 1) * t));
        if (i > 0) s_line(hue_color(i / 2), (int)px, (int)py, (int)x, (int)y, 1);
        px = x; py = y;
    }
}

static void xy2xy(int x0, int y0, int x1, int y1)
{
    const int n = 24;
    for (int i = 0; i <= n; i++) {
        const int ax = x0 + (x1 - x0) * i / n, by = y0 + (y1 - y0) * i / n;
        s_line(st.color, ax, y0, x0, by, 1);
        s_line(st.color, ax, y1, x1, by, 1);
        s_line(st.color, ax, y0, x1, by, 1);
        s_line(st.color, ax, y1, x0, by, 1);
    }
}

static void mirror4(int x0, int y0, int x1, int y1, int size)
{
    Paint p = solid();
    p_line(&p, x0, y0, x1, y1, size);
    p_line(&p, CV_W - 1 - x0, y0, CV_W - 1 - x1, y1, size);
    p_line(&p, x0, CV_H - 1 - y0, x1, CV_H - 1 - y1, size);
    p_line(&p, CV_W - 1 - x0, CV_H - 1 - y0, CV_W - 1 - x1, CV_H - 1 - y1, size);
}

static void northern(int x0, int x1)
{
    const int a = x0 < x1 ? x0 : x1, b = x0 < x1 ? x1 : x0;
    for (int x = a; x <= b; x++) {
        const u32 c = hue_color((x * 3 + (int)S.t) % 360);
        for (int k = -60; k <= 60; k++) {
            const int al = 90 - abs(k) * 90 / 60;
            if (al > 0) cv_blend(x, S.cy + k, c, al / 3 + 1);
        }
    }
}

/* ---- public -------------------------------------------------------------- */
void brush_down(int x, int y)
{
    S.lx = S.ly = -1;
    S.dist = 0;
    S.t = 0;
    S.step = 0;
    zz_have = false;
    loop_have = false;
    side = 1;
    memset(drips, 0, sizeof drips);
    Paint p = solid();
    switch (st.brush) {
    case B_DOTS: st_dots(x, y, 0); break;
    case B_PIES: st_pies(x, y, 0); break;
    case B_GALAXY: st_stars(x, y, 0); break;
    case B_XOS: st_xos(x, y, 0); break;
    case B_CARDS: st_cards(x, y, 0); break;
    case B_SHAPES: st_shapes(x, y, 0); break;
    case B_PAW: st_paw(x, y, -1.5707963f); break;
    case B_CONNECT: st_connect(x, y, 0); break;
    case B_TREE: tree(x, y, -1.5707963f, 48.0f * SCALE(), 8); sfx(); break;
    case B_PINES:
        for (int i = 0; i < 14; i++) {
            const float a = -1.5707963f + (rnd(140) - 70) * 0.01745f;
            const int l = 8 + rnd(18) * SCALE();
            s_line(st.color, x, y, x + (int)(cosf(a) * l), y + (int)(sinf(a) * l), 1);
        }
        sfx();
        break;
    case B_INVERTER: invert_disc(x, y, 14 * SCALE()); sfx(); break;
    case B_KALEIDO: mirror4(x, y, x, y, 3 * SCALE()); sfx(); break;
    case B_LEAKY:
    case B_3D:
    case B_ROTDOTS:
    case B_ZIGZAG:
    case B_SWIRL:
    case B_ECHOES:
        p_disc(&p, x, y, 3);
        break;
    default: break;
    }
    S.px = x; S.py = y; S.cx = x; S.cy = y;
}

void brush_move(int x, int y)
{
    Paint p = solid();
    S.cx = x; S.cy = y;
    S.step++;
    switch (st.brush) {
    case B_LEAKY:
        p_line(&p, S.px, S.py, x, y, 4 * SCALE());
        if (rnd(7) == 0)
            for (int i = 0; i < 48; i++)
                if (!drips[i].on) { drips[i] = (Drip){ x + rnd(5) - 2, y + 2, 0, 12 + rnd(55), true }; break; }
        sfx();
        break;
    case B_ZIGZAG: along(x, y, 12.0f * SCALE(), st_zig); break;
    case B_DOTS: along(x, y, 22.0f * SCALE(), st_dots); break;
    case B_PIES: along(x, y, 40.0f * SCALE(), st_pies); break;
    case B_ECHOES: {
        const int r = (S.step % 18 + 1) * 2 * SCALE();
        s_circle(st.color, x, y, r, 1);
        sfx();
        break; }
    case B_NORTHERN:
        S.t += 6;
        northern(S.px, x);
        sfx();
        break;
    case B_PINES:
        for (int i = 0; i < 6; i++) {
            const float a = -1.5707963f + (rnd(160) - 80) * 0.01745f;
            const int l = 8 + rnd(16) * SCALE();
            s_line(st.color, x, y, x + (int)(cosf(a) * l), y + (int)(sinf(a) * l), 1);
        }
        sfx();
        break;
    case B_3D: {
        const int w = 8 * SCALE();
        const u32 dark = RGBA(((st.color >> 24) & 255) / 3, ((st.color >> 16) & 255) / 3, ((st.color >> 8) & 255) / 3, 255);
        const u32 lite = RGBA(((st.color >> 24) & 255) / 2 + 128, ((st.color >> 16) & 255) / 2 + 128, ((st.color >> 8) & 255) / 2 + 128, 255);
        s_line(dark, S.px + 3, S.py + 3, x + 3, y + 3, w);
        s_line(st.color, S.px, S.py, x, y, w);
        s_line(lite, S.px - 1, S.py - 1, x - 1, y - 1, w / 3 + 1);
        sfx();
        break; }
    case B_KALEIDO: mirror4(S.px, S.py, x, y, 3 * SCALE()); sfx(); break;
    case B_CONNECT: along(x, y, 25.0f * SCALE(), st_connect); break;
    case B_SWIRL: {
        swirl_t += 0.55f;
        const float r = 6 + (S.step % 40) * 0.5f * SCALE();
        disc(x + (int)(cosf(swirl_t) * r), y + (int)(sinf(swirl_t) * r), 4, S.step & 1 ? st.color : hue_color(S.step * 9));
        sfx();
        break; }
    case B_ROTDOTS: {
        loop_t += 0.45f;
        const int off = (int)(sinf(loop_t) * 13 * SCALE());
        const float dx = x - S.px, dy = y - S.py;
        const float d = sqrtf(dx * dx + dy * dy) + 0.001f;
        disc(x + (int)(-dy / d * off), y + (int)(dx / d * off), 6, alt_mod ? hue_color(S.step * 10) : st.color);
        sfx();
        break; }
    case B_INVERTER: {
        const int n = abs(x - S.px) > abs(y - S.py) ? abs(x - S.px) : abs(y - S.py);
        for (int i = 0; i <= n; i += 4)
            invert_disc(n ? S.px + (x - S.px) * i / n : x, n ? S.py + (y - S.py) * i / n : y, 14 * SCALE());
        sfx();
        break; }
    case B_LOOPER: {
        S.t += 0.7f;
        const int lx = x + (int)(cosf(S.t) * 16 * SCALE()), ly = y + (int)(sinf(S.t) * 16 * SCALE());
        if (loop_have) s_line(st.color, loop_px, loop_py, lx, ly, 2);
        loop_px = lx; loop_py = ly; loop_have = true;
        sfx();
        break; }
    case B_GALAXY: along(x, y, 36.0f * SCALE(), st_stars); break;
    case B_XOS: along(x, y, 36.0f * SCALE(), st_xos); break;
    case B_CARDS: along(x, y, 36.0f * SCALE(), st_cards); break;
    case B_SHAPES: along(x, y, 36.0f * SCALE(), st_shapes); break;
    case B_PAW: along(x, y, 30.0f * SCALE(), st_paw); break;
    case B_MAGNIFY:
        undo_restore_base();
        lens(x, y);
        sfx();
        break;
    case B_GEOMETRY:
        undo_restore_base();
        guilloche(S.sx, S.sy, (int)sqrtf((float)((x - S.sx) * (x - S.sx) + (y - S.sy) * (y - S.sy))));
        sfx();
        break;
    case B_XY:
        undo_restore_base();
        xy2xy(S.sx, S.sy, x, y);
        sfx();
        break;
    case B_STARBURST:
        s_line(st.color, S.sx, S.sy, x, y, 2);
        sfx();
        break;
    default: break;
    }
    S.px = x; S.py = y;
}

void brush_up(int x, int y)
{
    (void)x; (void)y;
    if (st.brush == B_KALEIDO || st.brush == B_ECHOES) return;
}

/* animated / held brushes */
void brush_frame(void)
{
    if (!S.down) return;
    const int x = S.cx, y = S.cy, sc = SCALE();
    Paint p = solid();
    switch (st.brush) {
    case B_LEAKY:
        for (int i = 0; i < 48; i++)
            if (drips[i].on) {
                Drip *d = &drips[i];
                d->len++;
                p_disc(&p, d->x, d->y + d->len, 3);
                if (d->len >= d->max) d->on = false;
            }
        break;
    case B_BUBBLY:
        for (int i = 0; i < 2; i++)
            s_circle(st.color, x + rnd(41) - 20, y + rnd(41) - 20, 3 + rnd(11 * sc), 1);
        sfx();
        break;
    case B_FUZZER:
        for (int i = 0; i < 14; i++) {
            const float a = rnd(360) * 0.01745f, r = rnd(14 * sc);
            cv_put(x + (int)(cosf(a) * r), y + (int)(sinf(a) * r), st.color);
            cv_put(x + (int)(cosf(a) * r) + 1, y + (int)(sinf(a) * r), st.color);
        }
        sfx();
        break;
    case B_SPRAY:
        for (int i = 0; i < 22; i++) {
            const float a = rnd(360) * 0.01745f, r = sqrtf((float)rnd(1000) / 1000.0f) * 17 * sc;
            cv_put(x + (int)(cosf(a) * r), y + (int)(sinf(a) * r), st.color);
        }
        sfx();
        break;
    case B_SPLATTER:
        for (int i = 0; i < 3; i++) {
            const int sx = x + rnd(61) - 30, sy = y + rnd(61) - 30;
            const u32 c = hue_color(rnd(360));
            const int sz = 2 + rnd(6 * sc);
            disc(sx, sy, sz, c);
            if (rnd(4) == 0) for (int k = 0; k < 4; k++) disc(sx + rnd(17) - 8, sy + rnd(17) - 8, 2, c);
        }
        sfx();
        break;
    default: break;
    }
}

void brush_preview(int x, int y)
{
    (void)x; (void)y;
}
