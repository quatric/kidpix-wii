/* Display, image/texture helpers and the bitmap text renderer. */
#include <stdio.h>
#include <malloc.h>
#include <png.h>
#include "kidpix.h"

u32 *cv;
char asset_root[256] = "sd:/apps/kidpix/assets";

static GRRLIB_texImg *cv_tex;
static bool cv_dirty = true;

/* GX RGBA8 textures are stored as 4x4 tiles, AR words then GB words. */
static void tile_upload(GRRLIB_texImg *t, const u32 *src, int sw, int sh)
{
    const int tw = t->w, th = t->h;
    u8 *base = (u8 *)t->data;
    const bool fast = (sw == tw && sh == th);

    for (int ty = 0; ty < th; ty += 4) {
        for (int tx = 0; tx < tw; tx += 4) {
            u16 *blk = (u16 *)(base + ((ty << 2) * tw) + (tx << 4));
            for (int r = 0; r < 4; r++) {
                for (int c = 0; c < 4; c++) {
                    const int x = tx + c, y = ty + r;
                    u32 p = 0;
                    if (fast || (x < sw && y < sh)) p = src[y * sw + x];
                    const int i = (r << 2) + c;
                    blk[i] = (u16)(((p & 0xff) << 8) | (p >> 24));
                    blk[16 + i] = (u16)(p >> 8);
                }
            }
        }
    }
    GRRLIB_FlushTex(t);
}

void gfx_init(void)
{
    GRRLIB_Init();
    GRRLIB_SetAntiAliasing(false);
    GRRLIB_SetBlend(GRRLIB_BLEND_ALPHA);

    cv = (u32 *)memalign(32, CV_W * CV_H * 4);
    for (int i = 0; i < CV_W * CV_H; i++) cv[i] = 0xffffffff;
    cv_tex = GRRLIB_CreateEmptyTexture(CV_W, CV_H);
    cv_dirty = true;
}

void canvas_touch(void) { cv_dirty = true; }

void canvas_present(void)
{
    if (cv_dirty) {
        tile_upload(cv_tex, cv, CV_W, CV_H);
        cv_dirty = false;
    }
    GRRLIB_SetBlend(GRRLIB_BLEND_NONE);
    GRRLIB_DrawImg(CV_X, CV_Y, cv_tex, 0, 1, 1, 0xffffffff);
    GRRLIB_SetBlend(GRRLIB_BLEND_ALPHA);
}

/* ---- images ------------------------------------------------------------ */
Image *img_new(int w, int h)
{
    Image *im = (Image *)calloc(1, sizeof(Image));
    im->w = w; im->h = h;
    im->px = (u32 *)calloc((size_t)w * h, 4);
    return im;
}

void img_upload(Image *im)
{
    const int tw = (im->w + 3) & ~3, th = (im->h + 3) & ~3;
    if (im->tex && (im->tex->w != (u32)tw || im->tex->h != (u32)th)) {
        GRRLIB_FreeTexture(im->tex);
        im->tex = NULL;
    }
    if (!im->tex) im->tex = GRRLIB_CreateEmptyTexture(tw, th);
    if (im->tex) tile_upload(im->tex, im->px, im->w, im->h);
}

void img_free(Image *im)
{
    if (!im) return;
    if (im->tex) GRRLIB_FreeTexture(im->tex);
    free(im->px);
    free(im);
}

#define IMG_CACHE 512
static struct { char name[72]; Image *im; } img_cache[IMG_CACHE];
static int img_cache_n;

Image *img_load(const char *rel)
{
    for (int i = 0; i < img_cache_n; i++)
        if (!strcmp(img_cache[i].name, rel)) return img_cache[i].im;

    char path[400];
    snprintf(path, sizeof path, "%s/%s", asset_root, rel);

    png_image pi;
    memset(&pi, 0, sizeof pi);
    pi.version = PNG_IMAGE_VERSION;
    Image *im = NULL;
    if (png_image_begin_read_from_file(&pi, path)) {
        pi.format = PNG_FORMAT_RGBA;
        im = img_new(pi.width, pi.height);
        if (!png_image_finish_read(&pi, NULL, im->px, 0, NULL)) {
            img_free(im);
            im = NULL;
        }
    }
    png_image_free(&pi);
    if (im) img_upload(im);

    if (img_cache_n < IMG_CACHE) {
        strncpy(img_cache[img_cache_n].name, rel, sizeof img_cache[0].name - 1);
        img_cache[img_cache_n].im = im;
        img_cache_n++;
    }
    return im;
}

void img_draw(Image *im, float x, float y, float sx, float sy, u32 tint)
{
    if (!im || !im->tex) return;
    GRRLIB_DrawPart(x, y, 0, 0, im->w, im->h, im->tex, 0, sx, sy, tint);
}

void img_draw_part(Image *im, float x, float y, int px, int py, int pw, int ph,
                   float sx, float sy, u32 tint)
{
    if (!im || !im->tex) return;
    GRRLIB_DrawPart(x, y, px, py, pw, ph, im->tex, 0, sx, sy, tint);
}

bool canvas_save_png(const char *path)
{
    png_image pi;
    memset(&pi, 0, sizeof pi);
    pi.version = PNG_IMAGE_VERSION;
    pi.width = CV_W;
    pi.height = CV_H;
    pi.format = PNG_FORMAT_RGBA;
    const int ok = png_image_write_to_file(&pi, path, 0, cv, 0, NULL);
    png_image_free(&pi);
    return ok != 0;
}

/* ---- rects ------------------------------------------------------------- */
void rect_fill(float x, float y, float w, float h, u32 rgba)
{
    GRRLIB_Rectangle(x, y, w, h, rgba, true);
}

void rect_frame(float x, float y, float w, float h, u32 rgba)
{
    GRRLIB_Rectangle(x, y, w, 1, rgba, true);
    GRRLIB_Rectangle(x, y + h - 1, w, 1, rgba, true);
    GRRLIB_Rectangle(x, y, 1, h, rgba, true);
    GRRLIB_Rectangle(x + w - 1, y, 1, h, rgba, true);
}

/* ---- text -------------------------------------------------------------- */
int text_width(int font, const char *s)
{
    const KpFont *f = &kp_fonts[font];
    int w = 0;
    for (; *s; s++)
        if (*s >= 32 && *s < 127) w += f->g[*s - 32].adv;
    return w;
}

void text_blit(Image *dst, int x, int y, int font, const char *s, u32 rgba)
{
    const KpFont *f = &kp_fonts[font];
    const int cr = rgba >> 24, cg = (rgba >> 16) & 255, cb = (rgba >> 8) & 255, ca = rgba & 255;
    for (; *s; s++) {
        if (*s < 32 || *s >= 127) continue;
        const KpGlyph *g = &f->g[*s - 32];
        const unsigned char *src = f->px + g->off;
        for (int j = 0; j < g->h; j++) {
            for (int i = 0; i < g->w; i++) {
                const int a = (src[j * g->w + i] * ca) / 255;
                if (!a) continue;
                const int dx = x + g->xo + i, dy = y + g->yo + j;
                if (dx < 0 || dy < 0 || dx >= dst->w || dy >= dst->h) continue;
                u32 *d = &dst->px[dy * dst->w + dx];
                const int da = *d & 255;
                if (da == 0) {
                    *d = RGBA(cr, cg, cb, a);
                } else {
                    const int dr = *d >> 24, dg = (*d >> 16) & 255, db = (*d >> 8) & 255;
                    const int r = (cr * a + dr * (255 - a)) / 255;
                    const int gg = (cg * a + dg * (255 - a)) / 255;
                    const int b = (cb * a + db * (255 - a)) / 255;
                    *d = RGBA(r, gg, b, da > a ? da : a);
                }
            }
        }
        x += g->adv;
    }
}

#define TXT_CACHE 64
static struct { char s[64]; int font; u32 color; Image *im; } txt_cache[TXT_CACHE];
static int txt_next;

static Image *text_image(int font, const char *s, u32 rgba)
{
    for (int i = 0; i < TXT_CACHE; i++)
        if (txt_cache[i].im && txt_cache[i].font == font && txt_cache[i].color == rgba &&
            !strncmp(txt_cache[i].s, s, 63))
            return txt_cache[i].im;

    const int w = text_width(font, s) + 4, h = kp_fonts[font].h + 2;
    Image *im = img_new(w, h);
    text_blit(im, 1, 0, font, s, rgba);
    img_upload(im);

    const int slot = txt_next++ % TXT_CACHE;
    if (txt_cache[slot].im) img_free(txt_cache[slot].im);
    strncpy(txt_cache[slot].s, s, 63);
    txt_cache[slot].s[63] = 0;
    txt_cache[slot].font = font;
    txt_cache[slot].color = rgba;
    txt_cache[slot].im = im;
    return im;
}

void text_draw(int x, int y, int font, const char *s, u32 rgba)
{
    img_draw(text_image(font, s, rgba), x - 1, y, 1, 1, 0xffffffff);
}

void text_draw_center(int cx, int y, int font, const char *s, u32 rgba)
{
    text_draw(cx - text_width(font, s) / 2, y, font, s, rgba);
}

/* A glyph centred in a w x h cell, used for the alphabet sub-menu. */
Image *text_glyph_image(int font, int ch, int w, int h, u32 rgba)
{
    Image *im = img_new(w, h);
    char s[2] = { (char)ch, 0 };
    const KpFont *f = &kp_fonts[font];
    const int tw = text_width(font, s);
    text_blit(im, (w - tw) / 2, (h - f->h) / 2, font, s, rgba);
    img_upload(im);
    return im;
}
