/* Electric Mixer: whole-picture effects. The pre-effect picture is the undo top. */
#include <stdio.h>
#include "tools_int.h"

enum {
    M_INVERT, M_RAIN, M_CHECKER, M_WALLPAPER, M_VENETIAN, M_OUTLINER, M_SHADOW,
    M_ZOOM, M_GLASS, M_PIP, M_HILITE, M_PATTERN, M_WRAP, M_SWIRL, M_PANCAKE
};

static const char *icons[MIXER_COUNT] = {
    "img/tool-submenu-wacky-mixer-164.png", "img/tool-submenu-wacky-mixer-165.png",
    "img/tool-submenu-wacky-mixer-166.png", "img/tool-submenu-wacky-mixer-167.png",
    "img/tool-submenu-wacky-mixer-168.png", "img/tool-submenu-wacky-mixer-169.png",
    "img/tool-submenu-wacky-mixer-170.png", "img/tool-submenu-wacky-mixer-171.png",
    "img/tool-submenu-wacky-mixer-172.png", "img/tool-submenu-wacky-mixer-173.png",
    "img/tool-submenu-wacky-mixer-174.png", "img/tool-submenu-wacky-mixer-175.png",
    "img/tool-submenu-wacky-mixer-176.png", NULL, NULL
};
const char *mixer_icon(int m) { return icons[m]; }

static u8 *mask;

static inline u32 sget(const u32 *s, int x, int y)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= CV_W) x = CV_W - 1;
    if (y >= CV_H) y = CV_H - 1;
    return s[y * CV_W + x];
}

static inline u32 blend2(u32 a, u32 b, int al)   /* al 0..256 of b */
{
    const int ia = 256 - al;
    const int r = (((a >> 24) & 255) * ia + ((b >> 24) & 255) * al) >> 8;
    const int g = (((a >> 16) & 255) * ia + ((b >> 16) & 255) * al) >> 8;
    const int bl = (((a >> 8) & 255) * ia + ((b >> 8) & 255) * al) >> 8;
    return RGBA(r, g, bl, 255);
}

static inline int lum(u32 c)
{
    return (((c >> 24) & 255) * 3 + ((c >> 16) & 255) * 6 + ((c >> 8) & 255)) / 10;
}

static void swirl(const u32 *s, int cx, int cy, int radius, float angle)
{
    memcpy(cv, s, CV_W * CV_H * 4);
    if (radius < 4) return;
    for (int y = cy - radius; y <= cy + radius; y++)
        for (int x = cx - radius; x <= cx + radius; x++) {
            if ((unsigned)x >= CV_W || (unsigned)y >= CV_H) continue;
            const float dx = x - cx, dy = y - cy;
            const float d = sqrtf(dx * dx + dy * dy);
            if (d >= radius) continue;
            const float t = 1.0f - d / radius;
            const float a = angle * t * t;
            const float ca = cosf(a), sa = sinf(a);
            cv[y * CV_W + x] = sget(s, cx + (int)(dx * ca - dy * sa), cy + (int)(dx * sa + dy * ca));
        }
}

static void pancake(const u32 *s, int dx, int dy, int dist)
{
    int n = 2 + dist / 64;
    if (n > 6) n = 6;
    const int inc = alt_mod ? 4 : 16;
    const int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
    memcpy(cv, s, CV_W * CV_H * 4);
    for (int i = 1; i < n; i++) {
        const int ox = (n * inc - i * inc) * sx, oy = (n * inc - i * inc) * sy;
        const int al = 256 * i / n;
        for (int y = 0; y < CV_H; y++)
            for (int x = 0; x < CV_W; x++) {
                const int xx = x - ox, yy = y - oy;
                if ((unsigned)xx >= CV_W || (unsigned)yy >= CV_H) continue;
                cv[y * CV_W + x] = blend2(cv[y * CV_W + x], s[yy * CV_W + xx], al);
            }
    }
}

static void wrap(const u32 *s, int dx, int dy)
{
    for (int y = 0; y < CV_H; y++)
        for (int x = 0; x < CV_W; x++) {
            int xx = (x - dx) % CV_W, yy = (y - dy) % CV_H;
            if (xx < 0) xx += CV_W;
            if (yy < 0) yy += CV_H;
            cv[y * CV_W + x] = s[yy * CV_W + xx];
        }
}

static void highlight_at(const u32 *s, int cx, int cy)
{
    for (int j = -7; j <= 7; j++)
        for (int i = -14; i <= 14; i++) {
            const int x = cx + i, y = cy + j;
            if ((unsigned)x >= CV_W || (unsigned)y >= CV_H) continue;
            if (mask[y * CV_W + x]) continue;
            mask[y * CV_W + x] = 1;
            cv[y * CV_W + x] = blend2(s[y * CV_W + x], 0xffff00ff, 120);
        }
}

static void one_shot(const u32 *s, int x, int y)
{
    switch (st.mixer) {
    case M_INVERT:
        for (int i = 0; i < CV_W * CV_H; i++) cv[i] = s[i] ^ 0xffffff00;
        snd_play("electric-mixer-inverter");
        break;
    case M_CHECKER:
        for (int j = 0; j < CV_H; j++)
            for (int i = 0; i < CV_W; i++)
                cv[j * CV_W + i] = (((i / 24) + (j / 24)) & 1) ? (s[j * CV_W + i] ^ 0xffffff00) : s[j * CV_W + i];
        snd_play("electric-mixer-shadow");
        break;
    case M_WALLPAPER: {
        const int n = alt_mod ? 3 : 4;
        for (int j = 0; j < CV_H; j++)
            for (int i = 0; i < CV_W; i++) cv[j * CV_W + i] = s[((j * n) % CV_H) * CV_W + (i * n) % CV_W];
        snd_play("electric-mixer-wallpaper");
        break; }
    case M_VENETIAN:
        for (int j = 0; j < CV_H; j++) {
            const int band = j / 18, r = j % 18;
            for (int i = 0; i < CV_W; i++) {
                u32 c = s[j * CV_W + i];
                if (band & 1) c = s[j * CV_W + (i + 24) % CV_W];
                if (r >= 15) c = 0x303030ff;
                cv[j * CV_W + i] = c;
            }
        }
        snd_play("electric-mixer-venetian");
        break;
    case M_OUTLINER:
        for (int j = 0; j < CV_H; j++)
            for (int i = 0; i < CV_W; i++) {
                const int gx = lum(sget(s, i + 1, j)) - lum(sget(s, i - 1, j));
                const int gy = lum(sget(s, i, j + 1)) - lum(sget(s, i, j - 1));
                int v = 255 - (abs(gx) + abs(gy)) * 4;
                if (v < 0) v = 0;
                cv[j * CV_W + i] = RGBA(v, v, v, 255);
            }
        snd_play("electric-mixer-inverter");
        break;
    case M_SHADOW:
        for (int j = 0; j < CV_H; j++)
            for (int i = 0; i < CV_W; i++) {
                const u32 c = s[j * CV_W + i];
                const bool sh = (i % 48) >= 38 || (j % 48) >= 38;
                cv[j * CV_W + i] = sh ? blend2(c, 0x000000ff, 150) : c;
            }
        snd_play("electric-mixer-shadow");
        break;
    case M_ZOOM:
        for (int j = 0; j < CV_H; j++)
            for (int i = 0; i < CV_W; i++) cv[j * CV_W + i] = sget(s, x + (i - x) / 2, y + (j - y) / 2);
        snd_play("kidpix-submenu-brush-zoom");
        break;
    case M_GLASS: {
        const int cw = 75, ch = 68;
        for (int cy = 0; cy * ch < CV_H; cy++)
            for (int cx = 0; cx * cw < CV_W; cx++) {
                const int ox = rnd(15) - 7, oy = rnd(15) - 7;
                for (int j = cy * ch; j < (cy + 1) * ch && j < CV_H; j++)
                    for (int i = cx * cw; i < (cx + 1) * cw && i < CV_W; i++) {
                        const bool crack = (i - cx * cw) < 2 || (j - cy * ch) < 2 || ((i - cx * cw) + (j - cy * ch)) % 70 == 0;
                        cv[j * CV_W + i] = crack ? 0xf0f0f0ff : sget(s, i + ox, j + oy);
                    }
            }
        snd_play("bubble-pop-4");
        break; }
    case M_PIP: {
        memcpy(cv, s, CV_W * CV_H * 4);
        const int w = CV_W / 3, h = CV_H / 3;
        const int x0 = x - w / 2, y0 = y - h / 2;
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                cv_put(x0 + i + 4, y0 + j + 4, 0x000000ff);
            }
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) cv_put(x0 + i, y0 + j, s[(j * 3) * CV_W + i * 3]);
        s_rect(0x000000ff, x0, y0, x0 + w, y0 + h, 2);
        snd_play("electric-mixer-pip");
        break; }
    case M_PATTERN: {
        char name[64];
        snprintf(name, sizeof name, "img/kidpix-mixer-pattern-%d.png", 206 + rnd(25));
        Image *im = img_load(name);
        if (!im) break;
        memcpy(cv, s, CV_W * CV_H * 4);
        for (int j = 0; j < CV_H; j++)
            for (int i = 0; i < CV_W; i++) {
                const u32 p = im->px[((j / 4) % im->h) * im->w + (i / 4) % im->w];
                const int a = p & 255;
                if (a) cv[j * CV_W + i] = blend2(cv[j * CV_W + i], p, a + (a >> 7));
            }
        snd_play("electric-mixer-shadow");
        break; }
    default: break;
    }
}

void mixer_down(int x, int y)
{
    const u32 *s = undo_peek();
    if (!s) return;
    if (!mask) mask = (u8 *)malloc(CV_W * CV_H);
    memset(mask, 0, CV_W * CV_H);
    S.t = 0;
    switch (st.mixer) {
    case M_RAIN: S.step = 0; break;
    case M_HILITE: highlight_at(s, x, y); snd_play("bubble-pop-3"); break;
    case M_WRAP:
    case M_SWIRL:
    case M_PANCAKE:
        snd_play("electric-mixer-wallpaper");
        break;
    default: one_shot(s, x, y); break;
    }
    S.px = x; S.py = y;
}

void mixer_move(int x, int y)
{
    const u32 *s = undo_peek();
    if (!s) return;
    S.cx = x; S.cy = y;
    const int dx = x - S.sx, dy = y - S.sy;
    const int dist = (int)sqrtf((float)(dx * dx + dy * dy));
    switch (st.mixer) {
    case M_HILITE: {
        const int n = abs(x - S.px) > abs(y - S.py) ? abs(x - S.px) : abs(y - S.py);
        for (int i = 0; i <= n; i += 3) highlight_at(s, S.px + (n ? (x - S.px) * i / n : 0), S.py + (n ? (y - S.py) * i / n : 0));
        break; }
    case M_WRAP: wrap(s, dx, dy); break;
    case M_SWIRL: {
        int r = abs(dx);
        float ang = (float)dy / 300.0f * 6.2831853f;
        if (ang > 6.2831853f) ang = 6.2831853f;
        if (ang < -6.2831853f) ang = -6.2831853f;
        swirl(s, S.sx, S.sy, r, ang);
        break; }
    case M_PANCAKE: pancake(s, dx, dy, dist); break;
    default: break;
    }
    S.px = x; S.py = y;
}

void mixer_up(int x, int y) { (void)x; (void)y; }

void mixer_frame(void)
{
    if (!S.down || st.mixer != M_RAIN) return;
    for (int i = 0; i < 2; i++) {
        const int x = S.cx + rnd(81) - 40, y = S.cy + rnd(81) - 40, r = 5 + rnd(14);
        const u32 c = kp_palettes[st.palette][rnd(32)];
        Paint p = { c, 1 };
        p_ellipse(&p, x, y, r, r);
        s_circle(0x000000ff, x, y, r, 1);
        cv_put(x - r / 3, y - r / 3, 0xffffffff);
        cv_put(x - r / 3 + 1, y - r / 3, 0xffffffff);
    }
    snd_play_idle("bubble-pop-2");
}
