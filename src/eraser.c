/* Eraser sub-menu: plain erasers plus the animated erasing effects. */
#include <stdio.h>
#include "tools_int.h"

enum {
    E_SQ20, E_CIRCLE, E_SQ10, E_SQ2, E_TNT, E_HIDDEN, E_WHITECIRC, E_DOORBELL,
    E_LETTERS, E_FADE, E_BLACKHOLE, E_COUNTDOWN, E_SWEEP
};

static const char *icons[ERASER_COUNT] = {
    "img/tool-submenu-eraser-178.png", "img/tool-submenu-eraser-179.png",
    "img/tool-submenu-eraser-180.png", "img/tool-submenu-eraser-181.png",
    "img/tool-submenu-eraser-182.png", "img/tool-submenu-eraser-183.png",
    "img/tool-submenu-eraser-184.png", "img/tool-submenu-eraser-185.png",
    "img/tool-submenu-eraser-186.png", "img/tool-submenu-eraser-187.png",
    "img/tool-submenu-eraser-189.png", "img/tool-submenu-eraser-190.png",
    "img/tool-submenu-eraser-191.png"
};
const char *eraser_icon(int e) { return icons[e]; }

static const char *hidden_names[12] = {
    "bear", "bison", "corn", "eye", "fox", "horse", "hummingbird", "ladybug", "lion", "magnet", "moth", "octopus"
};
static Image *hidden;

void eraser_select(int e)
{
    if (e != E_HIDDEN) return;
    char n[64];
    snprintf(n, sizeof n, "img/kp-h-%s.png", hidden_names[rnd(12)]);
    hidden = img_load(n);
}

static int esize(void)
{
    switch (st.eraser) {
    case E_SQ20: return 20;
    case E_CIRCLE: return 10;
    case E_SQ10: return 10;
    case E_SQ2: return 2;
    case E_HIDDEN: return 32;
    default: return 20;
    }
}

static const u32 *src(void) { return undo_peek(); }

static void erase_at(int x, int y)
{
    const int sz = esize() * SCALE();
    if (st.eraser == E_CIRCLE) {
        Paint p = { 0xffffffff, 1 };
        p_disc(&p, x, y, sz * 2);
    } else if (st.eraser == E_HIDDEN) {
        if (!hidden) eraser_select(E_HIDDEN);
        if (!hidden) return;
        for (int j = y - sz / 2; j < y + sz / 2; j++)
            for (int i = x - sz / 2; i < x + sz / 2; i++)
                cv_put(i, j, hidden->px[(((j % hidden->h) + hidden->h) % hidden->h) * hidden->w +
                                         (((i % hidden->w) + hidden->w) % hidden->w)] | 0xff);
    } else {
        for (int j = y - sz / 2; j < y + (sz + 1) / 2; j++)
            for (int i = x - sz / 2; i < x + (sz + 1) / 2; i++) cv_put(i, j, 0xffffffff);
    }
}

/* ---- animations ---------------------------------------------------------- */
static void big_glyph(int ch, int cx, int cy, int scale, u32 c)
{
    const KpFont *f = &kp_fonts[FONT_BIG];
    if (ch < 32 || ch > 126) return;
    const KpGlyph *g = &f->g[ch - 32];
    const unsigned char *px = f->px + g->off;
    const int ox = cx - (g->w * scale) / 2, oy = cy - (g->h * scale) / 2;
    for (int j = 0; j < g->h; j++)
        for (int i = 0; i < g->w; i++) {
            if (px[j * g->w + i] < 110) continue;
            for (int b = 0; b < scale; b++)
                for (int a = 0; a < scale; a++) cv_put(ox + i * scale + a, oy + j * scale + b, c);
        }
}

static inline u32 mix(u32 a, u32 b, int al)
{
    const int ia = 256 - al;
    return RGBA((((a >> 24) & 255) * ia + ((b >> 24) & 255) * al) >> 8,
                (((a >> 16) & 255) * ia + ((b >> 16) & 255) * al) >> 8,
                (((a >> 8) & 255) * ia + ((b >> 8) & 255) * al) >> 8, 255);
}

static void a_fade(int f)
{
    const u32 *s = src();
    const int al = 256 * (f + 1) / 20;
    for (int i = 0; i < CV_W * CV_H; i++) cv[i] = mix(s[i], 0xffffffff, al);
    if (f == 0) snd_play("eraser-tool-fade-2");
}

static void a_doorbell(int f)
{
    const u32 *s = src();
    const int off = f * f / 2 + f * 3;
    for (int j = 0; j < CV_H; j++)
        for (int i = 0; i < CV_W; i++) cv[j * CV_W + i] = (i - off >= 0) ? s[j * CV_W + i - off] : 0xffffffff;
    if (f == 0) snd_play("kidpix-eraser-doorbell-ding-dong");
    if (f == 8) snd_play("kidpix-eraser-doorbell-door-creak");
    if (f == 20) snd_play("kidpix-eraser-doorbell-wwoooowwww");
}

static void a_blackhole(int f)
{
    const u32 *s = src();
    const float sc = 1.0f - (f + 1) / 26.0f;
    const int cx = S.sx, cy = S.sy;
    cv_clear(0xffffffff);
    if (sc > 0.02f) {
        for (int y = 0; y < CV_H; y++)
            for (int x = 0; x < CV_W; x++) {
                const int sx = cx + (int)((x - cx) / sc), sy = cy + (int)((y - cy) / sc);
                if ((unsigned)sx < CV_W && (unsigned)sy < CV_H) cv[y * CV_W + x] = s[sy * CV_W + sx];
            }
    }
    Paint p = { 0x000000ff, 1 };
    p_ellipse(&p, cx, cy, 6 + f, 6 + f);
    if (f == 25) { cv_clear(0xffffffff); }
    if (f == 0) snd_play("kidpix-submenu-brush-zoom");
}

static void a_countdown(int f)
{
    const u32 *s = src();
    const int n = 3 - f / 25;
    memcpy(cv, s, CV_W * CV_H * 4);
    if (n > 0) {
        Paint w = { 0xffffffff, 1 };
        p_ellipse(&w, CV_W / 2, CV_H / 2, 70, 80);
        s_ellipse(0x000000ff, CV_W / 2, CV_H / 2, 70, 80, 3);
        big_glyph('0' + n, CV_W / 2, CV_H / 2, 3, 0xd00000ff);
        if (f % 25 == 0) snd_play("kidpix-menu-click-main-tools");
    } else {
        for (int i = 0; i < CV_W * CV_H; i++) cv[i] = 0xffffffff;
        const int r = (f - 75) * 30;
        Paint p = { 0xffd020ff, 1 };
        p_ellipse(&p, CV_W / 2, CV_H / 2, r > 0 ? r : 1, r > 0 ? r : 1);
        if (f == 75) snd_play("kidpix-tool-eraser-tnt-explosion");
        if (f >= 78) cv_clear(0xffffffff);
    }
}

static void a_letters(int f)
{
    if (f == 0) snd_play("western-gun-shot-twirl");
    static const char sym[] = "#$%!*&@?";
    for (int i = 0; i < 9; i++)
        big_glyph(sym[rnd(8)], rnd(CV_W), rnd(CV_H), 1 + rnd(3), hue_color(rnd(360)));
    if (f == 34) cv_clear(0xffffffff);
}

static void a_sweep(int f)
{
    const u32 *s = src();
    const int edge = (f + 1) * CV_W / 24;
    for (int j = 0; j < CV_H; j++)
        for (int i = 0; i < CV_W; i++) cv[j * CV_W + i] = i < edge ? 0xffffffff : s[j * CV_W + i];
    if (f < 23) for (int j = 0; j < CV_H; j += 3) { cv_put(edge + 2, j, 0x805020ff); cv_put(edge + 3, j, 0x805020ff); cv_put(edge + 4, j, 0x805020ff); }
    if (f == 0) snd_play("kidpix-truck-skid");
}

static void a_tnt(int f)
{
    const u32 *s = src();
    memcpy(cv, s, CV_W * CV_H * 4);
    const int r = (f + 1) * 6;
    if (f < 14) {
        const u32 cols[4] = { 0xffff00ff, 0xff8000ff, 0xff0000ff, 0xffffffff };
        Paint p = { cols[f & 3], 1 };
        p_ellipse(&p, S.sx, S.sy, r, r);
        for (int i = 0; i < 16; i++) {
            const float a = i * 0.3927f;
            s_line(cols[(f + i) & 3], S.sx, S.sy, S.sx + (int)(cosf(a) * (r + 18)), S.sy + (int)(sinf(a) * (r + 18)), 3);
        }
    } else {
        Paint w = { 0xffffffff, 1 };
        p_ellipse(&w, S.sx, S.sy, 84, 84);
    }
    if (f == 0) snd_play("kidpix-tool-eraser-tnt-explosion");
}

/* ---- tool entry points --------------------------------------------------- */
void eraser_down(int x, int y)
{
    switch (st.eraser) {
    case E_TNT: anim_start(a_tnt, 16); break;
    case E_DOORBELL: anim_start(a_doorbell, 32); break;
    case E_LETTERS: anim_start(a_letters, 36); break;
    case E_FADE: anim_start(a_fade, 20); break;
    case E_BLACKHOLE: anim_start(a_blackhole, 26); break;
    case E_COUNTDOWN: anim_start(a_countdown, 80); break;
    case E_SWEEP: anim_start(a_sweep, 24); break;
    case E_WHITECIRC:
        snd_play("bubble-pop-4");
        break;
    default:
        erase_at(x, y);
        break;
    }
    S.px = x; S.py = y;
}

void eraser_move(int x, int y)
{
    switch (st.eraser) {
    case E_SQ20: case E_CIRCLE: case E_SQ10: case E_SQ2: case E_HIDDEN: {
        const int n = abs(x - S.px) > abs(y - S.py) ? abs(x - S.px) : abs(y - S.py);
        const int step = esize() * SCALE() / 3 + 1;
        for (int i = 0; i <= n; i += step)
            erase_at(n ? S.px + (x - S.px) * i / n : x, n ? S.py + (y - S.py) * i / n : y);
        erase_at(x, y);
        break; }
    case E_WHITECIRC: {
        Paint p = { 0xffffffff, 1 };
        p_disc(&p, x, y, 18 + rnd(30) * SCALE());
        snd_play_idle("bubble-pop-3");
        break; }
    default: break;
    }
    S.px = x; S.py = y;
}

void eraser_up(int x, int y) { (void)x; (void)y; }
void eraser_frame(void) {}

void eraser_preview(int x, int y)
{
    int sz = esize() * SCALE();
    switch (st.eraser) {
    case E_SQ20: case E_SQ10: case E_SQ2: case E_HIDDEN:
        rect_fill(CV_X + x - sz / 2, y - sz / 2, sz, sz, 0xffffffff);
        rect_frame(CV_X + x - sz / 2, y - sz / 2, sz, sz, 0x000000ff);
        break;
    case E_CIRCLE:
        rect_fill(CV_X + x - sz, y - sz, sz * 2, sz * 2, 0xffffff80);
        rect_frame(CV_X + x - sz, y - sz, sz * 2, sz * 2, 0x000000ff);
        break;
    default: break;
    }
}
