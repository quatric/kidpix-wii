#ifndef KIDPIX_H
#define KIDPIX_H

#include <gccore.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ogc/lwp_watchdog.h>
#include <grrlib.h>

/* ---- screen layout (640x480) ------------------------------------------ */
#define SCR_W 640
#define SCR_H 480
#define TB_W  40                 /* left tool column */
#define BAR_H 72                 /* bottom sub-menu + colour bars */
#define CV_X  TB_W
#define CV_Y  0
#define CV_W  (SCR_W - TB_W)     /* 600 */
#define CV_H  (SCR_H - BAR_H)    /* 408 */

/* ---- generated data --------------------------------------------------- */
typedef struct { int w, h; const unsigned char *mask; } KpTexture;
#define KP_TEXTURE_COUNT 21      /* 0 = none, 1 = solid ... 20 */
#define TEX_RAINBOW      21      /* dynamic */
#define TEX_NUM          22
extern const KpTexture kp_textures[KP_TEXTURE_COUNT];

#define KP_PALETTE_COUNT 8
extern const u32 kp_palettes[KP_PALETTE_COUNT][32];

typedef struct { unsigned short w, h; short xo, yo, adv; unsigned int off; } KpGlyph;
typedef struct { int h, asc; const KpGlyph *g; const unsigned char *px; } KpFont;
extern const KpFont kp_fonts[2];
#define FONT_UI  0
#define FONT_BIG 1

/* ---- images ------------------------------------------------------------ */
typedef struct Image {
    int w, h;
    u32 *px;                     /* 0xRRGGBBAA, linear */
    GRRLIB_texImg *tex;          /* lazily created GX texture */
} Image;

/* ---- gfx.c ------------------------------------------------------------- */
extern u32 *cv;                  /* the drawing, CV_W x CV_H */
extern char asset_root[256];

void   gfx_init(void);
Image *img_new(int w, int h);
Image *img_load(const char *rel);               /* asset png, cached */
void   img_upload(Image *im);
void   img_free(Image *im);
void   img_draw(Image *im, float x, float y, float sx, float sy, u32 tint);
void   img_draw_part(Image *im, float x, float y, int px, int py, int pw, int ph,
                     float sx, float sy, u32 tint);
void   canvas_touch(void);
void   canvas_present(void);
bool   canvas_save_png(const char *path);
int    text_width(int font, const char *s);
void   text_blit(Image *dst, int x, int y, int font, const char *s, u32 rgba);
void   text_draw(int x, int y, int font, const char *s, u32 rgba);   /* cached */
void   text_draw_center(int cx, int y, int font, const char *s, u32 rgba);
Image *text_glyph_image(int font, int ch, int w, int h, u32 rgba);
void   rect_fill(float x, float y, float w, float h, u32 rgba);
void   rect_frame(float x, float y, float w, float h, u32 rgba);

/* ---- paint primitives (paint.c) ---------------------------------------- */
typedef struct { u32 color; int tex; } Paint;
#ifndef RGBA
#define RGBA(r,g,b,a) (((u32)(r)<<24)|((u32)(g)<<16)|((u32)(b)<<8)|(u32)(a))
#endif
bool  paint_pixel(const Paint *p, int x, int y, u32 *out);
void  cv_put(int x, int y, u32 c);
u32   cv_get(int x, int y);
void  cv_blend(int x, int y, u32 c, int alpha256);
void  cv_clear(u32 c);
void  p_disc(const Paint *p, int cx, int cy, int size);
void  p_line(const Paint *p, int x0, int y0, int x1, int y1, int size);
void  p_rect(const Paint *p, int x0, int y0, int x1, int y1);
void  p_ellipse(const Paint *p, int cx, int cy, int rx, int ry);
void  s_line(u32 color, int x0, int y0, int x1, int y1, int size);
void  s_circle(u32 color, int cx, int cy, int r, int w);
void  s_rect(u32 color, int x0, int y0, int x1, int y1, int w);
void  s_ellipse(u32 color, int cx, int cy, int rx, int ry, int w);
void  s_fill_poly(u32 color, const int *xy, int n);
void  flood_fill(const Paint *p, int x, int y);
u32   hue_color(int hue_deg);
int   rnd(int n);
void  tex_icon(Image *im, int tex, u32 color);

/* ---- sound.c ------------------------------------------------------------ */
void snd_init(void);
void snd_play(const char *prefix);
void snd_play_idle(const char *prefix);          /* only if not already sounding */
void snd_oops(void);

/* ---- input.c ------------------------------------------------------------ */
enum {
    IN_A = 1 << 0, IN_B = 1 << 1, IN_1 = 1 << 2, IN_2 = 1 << 3,
    IN_PLUS = 1 << 4, IN_MINUS = 1 << 5, IN_HOME = 1 << 6,
    IN_UP = 1 << 7, IN_DOWN = 1 << 8, IN_LEFT = 1 << 9, IN_RIGHT = 1 << 10,
};
typedef struct {
    float x, y;
    u32 held, down, up;
    int wheel;                   /* +1 / -1 pulses */
    int key;                     /* typed ascii (0 = none) */
    int hotkey;                  /* keyboard shortcut id, see input.c */
    const char *source;          /* name of the device driving the cursor */
    bool pointer_valid;
} Input;
extern Input in;
void input_init(void);
void input_update(void);
void input_shutdown(void);
#define HK_TOOL_BASE 100         /* HK_TOOL_BASE + tool index */
#define HK_UNDO 200
#define HK_MENU 201
#define HK_COLOR_NEXT 202
#define HK_COLOR_PREV 203

/* ---- tools.c ------------------------------------------------------------ */
enum { T_SAVE, T_PENCIL, T_LINE, T_RECT, T_OVAL, T_BRUSH, T_MIXER, T_FLOOD,
       T_ERASER, T_ALPHA, T_STAMP, T_TRUCK, T_UNDO, T_COUNT };

typedef struct {
    int tool;
    u32 color;
    int palette, color_idx;
    int scale;                   /* 1, or 2 while "enlarge" is held */
    int pencil_size, pencil_tex;
    int line_size, line_tex;
    int shape_tex;
    int brush, mixer, flood_tex, eraser, alpha_ch, stamp_page, stamp_i, truck;
    bool dragging;
} KpState;
extern KpState st;

int  sub_count(int tool);
Image *sub_icon(int tool, int i);
bool sub_selected(int tool, int i);
void sub_select(int tool, int i);
const char *sub_name(int tool);

void tools_init(void);
void tool_select(int t);
void tool_down(int x, int y);
void tool_move(int x, int y);
void tool_up(int x, int y);
void tool_frame(int mx, int my, bool over_canvas);   /* per-frame animation + hover */
void tool_draw_overlay(int mx, int my, bool over_canvas); /* previews, drawn over canvas */
bool tool_busy(void);                                 /* animation in progress */
void color_set(int idx);
void color_next(int d);
void palette_next(int d);
void undo_push(void);
void undo_do(void);
void file_new(void);
bool file_save(char *outname, int outlen);
Image *cursor_image(int *hx, int *hy);
void ui_toast(const char *s);
void stamp_page_move(int d);
extern char save_dir[300];
const char *tool_name(int tool);
void tool_text_input(int ch);
bool tool_is_text(void);
Image *stamp_sheet(int *px_row, int *page);

#endif
