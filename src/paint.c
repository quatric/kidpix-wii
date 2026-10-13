/* Software rasteriser for the drawing canvas. */
#include <stdio.h>
#include "kidpix.h"

int rnd(int n) { return n <= 0 ? 0 : (int)(((unsigned)rand() >> 4) % (unsigned)n); }

void cv_put(int x, int y, u32 c)
{
    if ((unsigned)x < CV_W && (unsigned)y < CV_H) cv[y * CV_W + x] = c;
}

u32 cv_get(int x, int y)
{
    if ((unsigned)x < CV_W && (unsigned)y < CV_H) return cv[y * CV_W + x];
    return 0xffffffff;
}

void cv_blend(int x, int y, u32 c, int a)
{
    if ((unsigned)x >= CV_W || (unsigned)y >= CV_H) return;
    u32 *d = &cv[y * CV_W + x];
    const int ia = 256 - a;
    const int r = (((c >> 24) & 255) * a + ((*d >> 24) & 255) * ia) >> 8;
    const int g = (((c >> 16) & 255) * a + ((*d >> 16) & 255) * ia) >> 8;
    const int b = (((c >> 8) & 255) * a + ((*d >> 8) & 255) * ia) >> 8;
    *d = RGBA(r, g, b, 255);
}

void cv_clear(u32 c)
{
    for (int i = 0; i < CV_W * CV_H; i++) cv[i] = c;
}

u32 hue_color(int h)
{
    h = ((h % 360) + 360) % 360;
    const int s = h / 60, f = (h % 60) * 255 / 60;
    int r = 0, g = 0, b = 0;
    switch (s) {
    case 0: r = 255; g = f; break;
    case 1: r = 255 - f; g = 255; break;
    case 2: g = 255; b = f; break;
    case 3: g = 255 - f; b = 255; break;
    case 4: r = f; b = 255; break;
    default: r = 255; b = 255 - f; break;
    }
    return RGBA(r, g, b, 255);
}

static const u32 rainbow_rows[7] = {
    0xff0000ff, 0xff8000ff, 0xffff00ff, 0x00c000ff, 0x0000ffff, 0x8000c0ff, 0xff00ffff
};

bool paint_pixel(const Paint *p, int x, int y, u32 *out)
{
    if (p->tex == 0) return false;
    if (p->tex == TEX_RAINBOW) {
        *out = rainbow_rows[((y >> 2) % 7 + 7) % 7];
        return true;
    }
    const KpTexture *t = &kp_textures[p->tex];
    if (!t->mask) return false;
    if (!t->mask[(((y % t->h) + t->h) % t->h) * t->w + (((x % t->w) + t->w) % t->w)]) return false;
    *out = p->color;
    return true;
}

static inline void ppix(const Paint *p, int x, int y)
{
    u32 c;
    if ((unsigned)x < CV_W && (unsigned)y < CV_H && paint_pixel(p, x, y, &c))
        cv[y * CV_W + x] = c;
}

void p_disc(const Paint *p, int cx, int cy, int size)
{
    if (size <= 1) { ppix(p, cx, cy); return; }
    const int x0 = cx - size / 2, y0 = cy - size / 2;
    const float r = size / 2.0f, r2 = r * r + 0.25f;
    for (int j = 0; j < size; j++) {
        const float fy = j + 0.5f - r;
        for (int i = 0; i < size; i++) {
            const float fx = i + 0.5f - r;
            if (fx * fx + fy * fy <= r2) ppix(p, x0 + i, y0 + j);
        }
    }
}

void p_line(const Paint *p, int x0, int y0, int x1, int y1, int size)
{
    const int dx = abs(x1 - x0), dy = abs(y1 - y0);
    const int n = dx > dy ? dx : dy;
    int step = size / 4;
    if (step < 1) step = 1;
    if (n == 0) { p_disc(p, x0, y0, size); return; }
    for (int i = 0; i <= n; i += step) {
        p_disc(p, x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, size);
    }
    p_disc(p, x1, y1, size);
}

void p_rect(const Paint *p, int x0, int y0, int x1, int y1)
{
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= CV_W) x1 = CV_W - 1;
    if (y1 >= CV_H) y1 = CV_H - 1;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) ppix(p, x, y);
}

void p_ellipse(const Paint *p, int cx, int cy, int rx, int ry)
{
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    for (int y = -ry; y <= ry; y++) {
        const float t = 1.0f - (float)(y * y) / (float)(ry * ry);
        if (t < 0) continue;
        const int hw = (int)(rx * sqrtf(t) + 0.5f);
        for (int x = -hw; x <= hw; x++) ppix(p, cx + x, cy + y);
    }
}

void s_line(u32 color, int x0, int y0, int x1, int y1, int size)
{
    Paint p = { color, 1 };
    p_line(&p, x0, y0, x1, y1, size);
}

void s_rect(u32 color, int x0, int y0, int x1, int y1, int w)
{
    s_line(color, x0, y0, x1, y0, w);
    s_line(color, x1, y0, x1, y1, w);
    s_line(color, x1, y1, x0, y1, w);
    s_line(color, x0, y1, x0, y0, w);
}

void s_ellipse(u32 color, int cx, int cy, int rx, int ry, int w)
{
    const int n = (rx + ry) * 2 + 16;
    int px = cx + rx, py = cy;
    for (int i = 1; i <= n; i++) {
        const float a = 6.2831853f * i / n;
        const int x = cx + (int)(cosf(a) * rx + (cosf(a) >= 0 ? 0.5f : -0.5f));
        const int y = cy + (int)(sinf(a) * ry + (sinf(a) >= 0 ? 0.5f : -0.5f));
        s_line(color, px, py, x, y, w);
        px = x; py = y;
    }
}

void s_circle(u32 color, int cx, int cy, int r, int w) { s_ellipse(color, cx, cy, r, r, w); }

void s_fill_poly(u32 color, const int *xy, int n)
{
    int miny = 1 << 30, maxy = -(1 << 30);
    for (int i = 0; i < n; i++) {
        if (xy[i * 2 + 1] < miny) miny = xy[i * 2 + 1];
        if (xy[i * 2 + 1] > maxy) maxy = xy[i * 2 + 1];
    }
    if (miny < 0) miny = 0;
    if (maxy >= CV_H) maxy = CV_H - 1;
    for (int y = miny; y <= maxy; y++) {
        int xs[32], k = 0;
        for (int i = 0; i < n && k < 30; i++) {
            const int j = (i + 1) % n;
            int ax = xy[i * 2], ay = xy[i * 2 + 1], bx = xy[j * 2], by = xy[j * 2 + 1];
            if (ay == by) continue;
            if (ay > by) { int t = ax; ax = bx; bx = t; t = ay; ay = by; by = t; }
            if (y < ay || y >= by) continue;
            xs[k++] = ax + (bx - ax) * (y - ay) / (by - ay);
        }
        for (int a = 1; a < k; a++) {                 /* insertion sort */
            const int v = xs[a];
            int b = a - 1;
            while (b >= 0 && xs[b] > v) { xs[b + 1] = xs[b]; b--; }
            xs[b + 1] = v;
        }
        for (int a = 0; a + 1 < k; a += 2)
            for (int x = xs[a]; x <= xs[a + 1]; x++) cv_put(x, y, color);
    }
}

/* ---- flood fill -------------------------------------------------------- */
typedef struct { short x, y; } Pt;

void flood_fill(const Paint *p, int sx, int sy)
{
    if ((unsigned)sx >= CV_W || (unsigned)sy >= CV_H) return;
    const u32 target = cv[sy * CV_W + sx];
    u8 *seen = (u8 *)calloc(CV_W * CV_H, 1);
    const int cap = 40000;
    Pt *stack = (Pt *)malloc(sizeof(Pt) * cap);
    if (!seen || !stack) { free(seen); free(stack); return; }
    int sp = 0;
    stack[sp++] = (Pt){ sx, sy };
    while (sp > 0) {
        Pt s = stack[--sp];
        int x = s.x;
        const int y = s.y;
        if (seen[y * CV_W + x] || cv[y * CV_W + x] != target) continue;
        int l = x, r = x;
        while (l > 0 && !seen[y * CV_W + l - 1] && cv[y * CV_W + l - 1] == target) l--;
        while (r < CV_W - 1 && !seen[y * CV_W + r + 1] && cv[y * CV_W + r + 1] == target) r++;
        for (x = l; x <= r; x++) seen[y * CV_W + x] = 1;
        for (int dy = -1; dy <= 1; dy += 2) {
            const int ny = y + dy;
            if (ny < 0 || ny >= CV_H) continue;
            bool run = false;
            for (x = l; x <= r; x++) {
                const bool ok = !seen[ny * CV_W + x] && cv[ny * CV_W + x] == target;
                if (ok && !run && sp < cap) stack[sp++] = (Pt){ x, ny };
                run = ok;
            }
        }
    }
    /* seen[] marks the region; paint after so pattern pixels can't re-trigger */
    for (int y = 0; y < CV_H; y++)
        for (int x = 0; x < CV_W; x++)
            if (seen[y * CV_W + x]) ppix(p, x, y);
    free(seen);
    free(stack);
}

/* 36x36 sample swatch of a texture for the sub-menus */
void tex_icon(Image *im, int tex, u32 color)
{
    const u32 frame = 0x303030ff;
    for (int i = 0; i < im->w * im->h; i++) im->px[i] = 0;
    Paint p = { color, tex };
    const int m = 4;
    for (int y = m; y < im->h - m; y++) {
        for (int x = m; x < im->w - m; x++) {
            u32 c = 0xffffffff;
            if (x == m || y == m || x == im->w - m - 1 || y == im->h - m - 1) c = frame;
            else {
                u32 o;
                if (paint_pixel(&p, x, y, &o)) c = o;
            }
            im->px[y * im->w + x] = c;
        }
    }
}
