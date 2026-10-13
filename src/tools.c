/* Tool state, undo, sub-menus and the simple drawing tools. */
#include <stdio.h>
#include <sys/stat.h>
#include "tools_int.h"

KpState st;
Stroke S;
bool shift_mod, alt_mod;
char save_dir[300];

static const char *tool_names[T_COUNT] = {
    "Save", "Wacky Pencil", "Line", "Rectangle", "Oval", "Wacky Brush", "Electric Mixer",
    "Paint Can", "Eraser", "Alphabet", "Rubber Stamps", "Moving Van", "Undo Guy"
};
const char *tool_name(int t) { return tool_names[t]; }

/* ---- undo / redo ----------------------------------------------------------- */
#define UNDO_N 6
typedef struct { u32 *buf[UNDO_N]; int n; } Stack;
static Stack undo_s, redo_s;
static bool last_was_undo;

static void stack_push(Stack *s)
{
    u32 *b;
    if (s->n == UNDO_N) {
        b = s->buf[0];
        memmove(&s->buf[0], &s->buf[1], sizeof(u32 *) * (UNDO_N - 1));
        s->buf[UNDO_N - 1] = b;
    } else {
        if (!s->buf[s->n]) s->buf[s->n] = (u32 *)malloc(CV_W * CV_H * 4);
        b = s->buf[s->n++];
    }
    if (b) memcpy(b, cv, CV_W * CV_H * 4);
}

static void stack_pop(Stack *s)
{
    if (s->n <= 0) return;
    s->n--;
    if (s->buf[s->n]) memcpy(cv, s->buf[s->n], CV_W * CV_H * 4);
}

u32 *undo_peek(void) { return undo_s.n > 0 ? undo_s.buf[undo_s.n - 1] : NULL; }

void undo_restore_base(void)
{
    u32 *b = undo_peek();
    if (b) memcpy(cv, b, CV_W * CV_H * 4);
}

void undo_push(void)
{
    stack_push(&undo_s);
    redo_s.n = 0;
    last_was_undo = false;
}

void undo_do(void)
{
    if (last_was_undo && redo_s.n > 0) {
        stack_push(&undo_s);
        stack_pop(&redo_s);
        last_was_undo = false;
        ui_toast("Redo");
    } else if (undo_s.n > 0) {
        stack_push(&redo_s);
        stack_pop(&undo_s);
        last_was_undo = true;
        snd_oops();
        ui_toast("Oops!");
    } else {
        ui_toast("Nothing to undo");
    }
    canvas_touch();
}

/* ---- animation driver ------------------------------------------------------- */
static void (*anim_fn)(int);
static int anim_f, anim_n;

void anim_start(void (*fn)(int), int n) { anim_fn = fn; anim_f = 0; anim_n = n; }
bool anim_running(void) { return anim_fn != NULL; }
bool tool_busy(void) { return anim_fn != NULL; }

Paint cur_paint(int tex) { Paint p = { st.color, tex }; return p; }

/* ---- colours --------------------------------------------------------------- */
void color_set(int idx)
{
    st.color_idx = ((idx % 32) + 32) % 32;
    st.color = kp_palettes[st.palette][st.color_idx];
}
void color_next(int d) { color_set(st.color_idx + d); snd_play("kidpix-menu-click-submenu-color"); }
void palette_next(int d)
{
    st.palette = ((st.palette + d) % KP_PALETTE_COUNT + KP_PALETTE_COUNT) % KP_PALETTE_COUNT;
    color_set(st.color_idx);
    snd_play("kidpix-menu-click-submenu-color");
}

/* ---- files ------------------------------------------------------------------ */
void file_new(void)
{
    undo_push();
    cv_clear(0xffffffff);
    canvas_touch();
    snd_play("kidpix-menu-click-main-tools");
    ui_toast("New picture");
}

bool file_save(char *outname, int outlen)
{
    mkdir(save_dir, 0777);                    /* ok if it already exists */
    char path[360];
    for (int n = 1; n < 10000; n++) {
        snprintf(path, sizeof path, "%s/kidpix-%03d.png", save_dir, n);
        FILE *f = fopen(path, "rb");
        if (f) { fclose(f); continue; }
        if (!canvas_save_png(path)) return false;
        snprintf(outname, outlen, "kidpix-%03d.png", n);
        return true;
    }
    return false;
}

static void do_save(void)
{
    char name[64], msg[96];
    if (file_save(name, sizeof name)) {
        snprintf(msg, sizeof msg, "Saved %s", name);
        snd_play("chord");
    } else {
        snprintf(msg, sizeof msg, "Could not save - no writable SD/USB");
        snd_oops();
    }
    ui_toast(msg);
}

/* ---- sub-menu model --------------------------------------------------------- */
static const int pencil_sizes[6] = { 1, 5, 9, 13, 17, 25 };

static const char *truck_icons[13] = {
    "192", "193", "194", "195", "196", "197", "198", "199", "200", "201", "202", "203", "204"
};
static const int truck_dims[13][2] = {
    { 50, 50 }, { 25, 25 }, { 10, 10 }, { 5, 5 }, { 1, 1 }, { 50, 25 }, { 25, 10 },
    { 10, 5 }, { 5, 2 }, { 25, 50 }, { 10, 25 }, { 5, 10 }, { 2, 5 }
};
static const char alpha_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!?0123456789+-<>$%^&@*()~|";

int sub_count(int tool)
{
    switch (tool) {
    case T_PENCIL: return 6 + 21;
    case T_LINE: return 6 + 21;
    case T_RECT: case T_OVAL: return TEX_NUM;
    case T_BRUSH: return BRUSH_COUNT;
    case T_MIXER: return MIXER_COUNT;
    case T_FLOOD: return 21;
    case T_ERASER: return ERASER_COUNT;
    case T_ALPHA: return 52;
    case T_STAMP: return 14;
    case T_TRUCK: return 13;
    default: return 0;
    }
}

bool sub_selected(int tool, int i)
{
    switch (tool) {
    case T_PENCIL: return i < 6 ? st.pencil_size == i : st.pencil_tex == i - 5;
    case T_LINE: return i < 6 ? st.line_size == i : st.line_tex == i - 5;
    case T_RECT: case T_OVAL: return st.shape_tex == i;
    case T_BRUSH: return st.brush == i;
    case T_MIXER: return st.mixer == i;
    case T_FLOOD: return st.flood_tex == i + 1;
    case T_ERASER: return st.eraser == i;
    case T_ALPHA: return st.alpha_ch == i;
    case T_STAMP: return st.stamp_i == i;
    case T_TRUCK: return st.truck == i;
    default: return false;
    }
}

void sub_select(int tool, int i)
{
    switch (tool) {
    case T_PENCIL: if (i < 6) st.pencil_size = i; else st.pencil_tex = i - 5; break;
    case T_LINE: if (i < 6) st.line_size = i; else st.line_tex = i - 5; break;
    case T_RECT: case T_OVAL: st.shape_tex = i; break;
    case T_BRUSH: st.brush = i; brush_select(i); break;
    case T_MIXER: st.mixer = i; break;
    case T_FLOOD: st.flood_tex = i + 1; break;
    case T_ERASER: st.eraser = i; eraser_select(i); break;
    case T_ALPHA: st.alpha_ch = i; break;
    case T_STAMP: st.stamp_i = i; break;
    case T_TRUCK: st.truck = i; break;
    default: break;
    }
    snd_play("kidpix-menu-click-submenu-options");
}

/* generated sub-menu icons */
static Image *icon_cache[T_COUNT][64];
static Image *tex_icons[TEX_NUM];

static void ipix(Image *im, int x, int y, u32 c)
{
    if ((unsigned)x < (unsigned)im->w && (unsigned)y < (unsigned)im->h) im->px[y * im->w + x] = c;
}
static void idisc(Image *im, int cx, int cy, int r, u32 c)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) ipix(im, cx + x, cy + y, c);
}

static Image *gen_icon(int kind)
{
    Image *im = img_new(36, 36);
    for (int i = 0; i < 36 * 36; i++) im->px[i] = 0xffffffff;
    for (int i = 0; i < 36; i++) {
        ipix(im, i, 0, 0x303030ff); ipix(im, i, 35, 0x303030ff);
        ipix(im, 0, i, 0x303030ff); ipix(im, 35, i, 0x303030ff);
    }
    if (kind == 0) {                       /* paw */
        idisc(im, 18, 23, 7, 0x402010ff);
        idisc(im, 9, 14, 3, 0x402010ff); idisc(im, 15, 8, 3, 0x402010ff);
        idisc(im, 22, 8, 3, 0x402010ff); idisc(im, 28, 14, 3, 0x402010ff);
    } else if (kind == 1) {                /* swirl lollipop */
        for (int i = 0; i < 160; i++) {
            const float a = i * 0.16f, r = 2 + i * 0.1f;
            idisc(im, 18 + (int)(cosf(a) * r), 16 + (int)(sinf(a) * r), 1, hue_color(i * 4));
        }
        for (int y = 26; y < 34; y++) ipix(im, 18, y, 0x806040ff);
    } else {                               /* pancakes */
        for (int k = 0; k < 3; k++)
            for (int y = -4; y <= 4; y++)
                for (int x = -13; x <= 13; x++)
                    if (x * x * 16 + y * y * 169 <= 169 * 16) ipix(im, 18 + x, 12 + k * 7 + y, k == 0 ? 0xe0a040ff : 0xc88820ff);
    }
    img_upload(im);
    return im;
}

static Image *letter_icon(int ch)
{
    Image *im = img_new(36, 36);
    for (int i = 0; i < 36 * 36; i++) im->px[i] = 0xffffffff;
    char s[2] = { (char)ch, 0 };
    Image *tmp = img_new(20, 20);
    text_blit(tmp, (20 - text_width(FONT_UI, s)) / 2, 2, FONT_UI, s, 0x000000ff);
    for (int y = 0; y < 36; y++)
        for (int x = 0; x < 36; x++) {
            const int sx = x * 20 / 36, sy = y * 20 / 36;
            const int a = tmp->px[sy * 20 + sx] & 255;
            if (a > 100) im->px[y * 36 + x] = 0x203080ff;
        }
    img_free(tmp);
    for (int i = 0; i < 36; i++) {
        ipix(im, i, 0, 0x303030ff); ipix(im, i, 35, 0x303030ff);
        ipix(im, 0, i, 0x303030ff); ipix(im, 35, i, 0x303030ff);
    }
    img_upload(im);
    return im;
}

static Image *texture_icon(int tex)
{
    if (!tex_icons[tex]) {
        tex_icons[tex] = img_new(36, 36);
        tex_icon(tex_icons[tex], tex, 0x000000ff);
        img_upload(tex_icons[tex]);
    }
    return tex_icons[tex];
}

Image *sub_icon(int tool, int i)
{
    if (i < 0 || i >= 64) return NULL;
    if (tool == T_STAMP) return NULL;
    Image **slot = &icon_cache[tool][i];
    if (*slot) return *slot;
    char n[96];
    switch (tool) {
    case T_PENCIL:
        if (i < 6) { snprintf(n, sizeof n, "img/tool-submenu-pencil-size-%d.png", i + 1); *slot = img_load(n); }
        else *slot = texture_icon(i - 5);
        break;
    case T_LINE:
        if (i < 6) { snprintf(n, sizeof n, "img/pw%d.png", i + 1); *slot = img_load(n); }
        else *slot = texture_icon(i - 5);
        break;
    case T_RECT: case T_OVAL: *slot = texture_icon(i); break;
    case T_FLOOD: *slot = texture_icon(i + 1); break;
    case T_BRUSH: {
        const char *f = brush_icon(i);
        *slot = f ? img_load(f) : gen_icon(0);
        break; }
    case T_MIXER: {
        const char *f = mixer_icon(i);
        *slot = f ? img_load(f) : gen_icon(i == 13 ? 1 : 2);
        break; }
    case T_ERASER: *slot = img_load(eraser_icon(i)); break;
    case T_TRUCK:
        snprintf(n, sizeof n, "img/tool-submenu-truck-%s.png", truck_icons[i]);
        *slot = img_load(n);
        break;
    case T_ALPHA: *slot = letter_icon(alpha_chars[i]); break;
    default: break;
    }
    return *slot;
}

/* ---- stamps --------------------------------------------------------------- */
static const char *sheet_names[10] = {
    "img/kidpix-spritesheet-0.png", "img/kidpix-spritesheet-0b.png", "img/kidpix-spritesheet-1.png",
    "img/kidpix-spritesheet-2.png", "img/kidpix-spritesheet-3.png", "img/kidpix-spritesheet-4.png",
    "img/kidpix-spritesheet-5.png", "img/kidpix-spritesheet-6.png", "img/kidpix-spritesheet-7.png",
    "img/kidpix-spritesheet-8.png"
};

Image *stamp_sheet(int *row, int *page)
{
    if (row) *row = st.stamp_page % 8;
    if (page) *page = st.stamp_page;
    return img_load(sheet_names[st.stamp_page / 8]);
}

void stamp_page_move(int d)
{
    st.stamp_page = ((st.stamp_page + d) % 80 + 80) % 80;
    snd_play("kidpix-menu-click-submenu-options");
}

static void stamp_place(int cx, int cy)
{
    int row;
    Image *sh = stamp_sheet(&row, NULL);
    if (!sh) return;
    const int z = 2 * SCALE(), size = 32 * z;
    const int px0 = st.stamp_i * 32, py0 = row * 32;
    for (int j = 0; j < size; j++)
        for (int i = 0; i < size; i++) {
            const u32 c = sh->px[(py0 + j / z) * sh->w + px0 + i / z];
            const int a = c & 255;
            if (a) cv_blend(cx - size / 2 + i, cy - size / 2 + j, c, a + (a >> 7));
        }
}

/* ---- alphabet ---------------------------------------------------------------- */
static int text_x = -1, text_y;

static const char *alpha_sound(int ch, char *buf, int n)
{
    if (ch >= 'A' && ch <= 'Z') snprintf(buf, n, "alpha-%c", ch + 32);
    else if (ch >= 'a' && ch <= 'z') snprintf(buf, n, "alpha-%c", ch);
    else if (ch >= '0' && ch <= '9') snprintf(buf, n, "number-%c", ch);
    else if (ch == '!') snprintf(buf, n, "number-eclamation");
    else if (ch == '?') snprintf(buf, n, "number-question-mark");
    else if (ch == '+') snprintf(buf, n, "number-plus");
    else if (ch == '-') snprintf(buf, n, "number-minus");
    else if (ch == '&') snprintf(buf, n, "number-ampersand");
    else return NULL;
    return buf;
}

static int draw_letter(int ch, int x, int y)       /* x = left edge, y = vertical centre */
{
    const KpFont *f = &kp_fonts[FONT_BIG];
    if (ch < 32 || ch > 126) return 0;
    const KpGlyph *g = &f->g[ch - 32];
    const unsigned char *px = f->px + g->off;
    const float z = 1.6f * SCALE();
    const int dw = (int)(g->w * z), dh = (int)(g->h * z);
    const int oy = y - (int)(f->asc * z) / 2 + (int)(g->yo * z);
    for (int j = 0; j < dh; j++)
        for (int i = 0; i < dw; i++) {
            const int a = px[(int)(j / z) * g->w + (int)(i / z)];
            if (a > 8) cv_blend(x + (int)(g->xo * z) + i, oy + j, st.color, a + (a >> 7));
        }
    return (int)(g->adv * z);
}

bool tool_is_text(void) { return st.tool == T_ALPHA; }

void tool_text_input(int ch)
{
    if (st.tool != T_ALPHA || ch < 32 || ch > 126) return;
    if (text_x < 0) { text_x = 20; text_y = CV_H / 2; }
    undo_push();
    const int adv = draw_letter(ch, text_x, text_y);
    text_x += adv ? adv : 10;
    if (text_x > CV_W - 30) { text_x = 20; text_y += 70; }
    char b[32];
    const char *s = alpha_sound(ch, b, sizeof b);
    if (s) snd_play(s);
    canvas_touch();
}

/* ---- moving van --------------------------------------------------------------- */
static u32 *truck_buf;
static int truck_w, truck_h, truck_ox, truck_oy;
static bool truck_copy;

static void truck_dims_get(int *hx, int *hy)
{
    *hx = truck_dims[st.truck][0] * SCALE();
    *hy = truck_dims[st.truck][1] * SCALE();
    if (*hx > 100) *hx = 100;
    if (*hy > 100) *hy = 100;
}

static void truck_draw(int x, int y)
{
    undo_restore_base();
    if (!truck_copy)
        for (int j = 0; j < truck_h; j++)
            for (int i = 0; i < truck_w; i++) cv_put(truck_ox + i, truck_oy + j, 0xffffffff);
    for (int j = 0; j < truck_h; j++)
        for (int i = 0; i < truck_w; i++)
            cv_put(x - truck_w / 2 + i, y - truck_h / 2 + j, truck_buf[j * truck_w + i]);
}

/* ---- tool dispatch --------------------------------------------------------------- */
void tools_init(void)
{
    memset(&st, 0, sizeof st);
    st.tool = T_PENCIL;
    st.palette = 0;
    st.pencil_size = 1;
    st.pencil_tex = 1;
    st.line_size = 1;
    st.line_tex = 1;
    st.shape_tex = 0;
    st.flood_tex = 1;
    st.eraser = 0;
    st.alpha_ch = 0;
    color_set(0);
    snprintf(save_dir, sizeof save_dir, "%s", asset_root);
    char *slash = strrchr(save_dir, '/');
    if (slash) *slash = 0;                              /* strip /assets */
    strncat(save_dir, "/saves", sizeof save_dir - strlen(save_dir) - 1);
    truck_buf = (u32 *)malloc(200 * 200 * 4);
    srand(1234567u ^ (unsigned)gettick());
}

void tool_select(int t)
{
    if (t < 0 || t >= T_COUNT) return;
    snd_play("kidpix-menu-click-main-tools");
    if (t == T_SAVE) { do_save(); return; }
    if (t == T_UNDO) { undo_do(); return; }
    st.tool = t;
    S.down = false;
    ui_toast(tool_names[t]);
}

void tool_down(int x, int y)
{
    if (anim_running()) return;
    S.down = true;
    S.sx = S.px = S.cx = x;
    S.sy = S.py = S.cy = y;
    S.step = 0;
    S.dist = 0;
    Paint p;

    switch (st.tool) {
    case T_PENCIL:
        undo_push();
        p = cur_paint(st.pencil_tex);
        p_disc(&p, x, y, pencil_sizes[st.pencil_size] * SCALE());
        snd_play_idle("kidpix-tool-pencil");
        break;
    case T_LINE:
        undo_push();
        snd_play("kidpix-tool-line-start");
        break;
    case T_RECT:
    case T_OVAL:
        undo_push();
        break;
    case T_BRUSH: undo_push(); brush_down(x, y); break;
    case T_MIXER: undo_push(); mixer_down(x, y); break;
    case T_ERASER:
        undo_push();
        eraser_down(x, y);
        if (anim_running()) S.down = false;
        break;
    case T_FLOOD:
        undo_push();
        p = cur_paint(st.flood_tex);
        flood_fill(&p, x, y);
        snd_play("flood0");
        S.down = false;
        break;
    case T_ALPHA: {
        undo_push();
        text_x = x - 20;
        text_y = y;
        const int ch = alpha_chars[st.alpha_ch];
        text_x += draw_letter(ch, text_x, text_y);
        char b[32];
        const char *s = alpha_sound(ch, b, sizeof b);
        if (s) snd_play(s);
        break; }
    case T_STAMP:
        undo_push();
        stamp_place(x, y);
        snd_play("stamp0");
        break;
    case T_TRUCK: {
        undo_push();
        int hx, hy;
        truck_dims_get(&hx, &hy);
        truck_w = hx * 2;
        truck_h = hy * 2;
        truck_ox = x - hx;
        truck_oy = y - hy;
        truck_copy = alt_mod;
        for (int j = 0; j < truck_h; j++)
            for (int i = 0; i < truck_w; i++) truck_buf[j * truck_w + i] = cv_get(truck_ox + i, truck_oy + j);
        snd_play("kidpix-truck-skid");
        truck_draw(x, y);
        break; }
    default: S.down = false; break;
    }
    canvas_touch();
}

void tool_move(int x, int y)
{
    if (!S.down) return;
    Paint p;
    S.cx = x;
    S.cy = y;
    S.step++;
    switch (st.tool) {
    case T_PENCIL:
        p = cur_paint(st.pencil_tex);
        p_line(&p, S.px, S.py, x, y, pencil_sizes[st.pencil_size] * SCALE());
        snd_play_idle("kidpix-tool-pencil");
        break;
    case T_LINE:
        undo_restore_base();
        p = cur_paint(st.line_tex);
        p_line(&p, S.sx, S.sy, x, y, pencil_sizes[st.line_size] * SCALE());
        snd_play_idle("kidpix-tool-line-during");
        break;
    case T_RECT: {
        undo_restore_base();
        int ex = x, ey = y;
        if (shift_mod) {
            int dx = x - S.sx, dy = y - S.sy;
            const int m = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
            ex = S.sx + (dx < 0 ? -m : m);
            ey = S.sy + (dy < 0 ? -m : m);
        }
        p = cur_paint(st.shape_tex);
        p_rect(&p, S.sx, S.sy, ex, ey);
        if (!alt_mod) s_rect(st.color, S.sx, S.sy, ex, ey, 2);
        snd_play_idle("kidpix-tool-box-during-approx");
        break; }
    case T_OVAL: {
        undo_restore_base();
        const int dx = x - S.sx, dy = y - S.sy;
        p = cur_paint(st.shape_tex);
        int cx, cy, rx, ry;
        if (shift_mod) {
            cx = (x + S.sx) / 2; cy = (y + S.sy) / 2; rx = abs(dx) / 2; ry = abs(dy) / 2;
        } else {
            cx = (x + S.sx) / 2; cy = (y + S.sy) / 2;
            rx = ry = (int)(sqrtf((float)(dx * dx + dy * dy)) / 2);
        }
        p_ellipse(&p, cx, cy, rx, ry);
        if (!alt_mod) s_ellipse(st.color, cx, cy, rx, ry, 2);
        snd_play_idle("kidpix-tool-circle-during-approx");
        break; }
    case T_BRUSH: brush_move(x, y); break;
    case T_MIXER: mixer_move(x, y); break;
    case T_ERASER: eraser_move(x, y); break;
    case T_STAMP: {
        const int dx = x - S.px, dy = y - S.py;
        S.dist += sqrtf((float)(dx * dx + dy * dy));
        if (S.dist >= 32 * 2 * SCALE()) { S.dist = 0; stamp_place(x, y); snd_play("stamp0"); }
        break; }
    case T_TRUCK:
        truck_draw(x, y);
        snd_play_idle("kidpix-truck-truckin");
        break;
    default: break;
    }
    S.px = x;
    S.py = y;
    canvas_touch();
}

void tool_up(int x, int y)
{
    if (!S.down) return;
    switch (st.tool) {
    case T_LINE: snd_play("kidpix-tool-line-end"); break;
    case T_BRUSH: brush_up(x, y); break;
    case T_MIXER: mixer_up(x, y); break;
    case T_ERASER: eraser_up(x, y); break;
    case T_TRUCK: snd_play("kidpix-truck-truckin-go"); break;
    default: break;
    }
    S.down = false;
    canvas_touch();
}

void tool_frame(int mx, int my, bool over_canvas)
{
    (void)mx; (void)my; (void)over_canvas;
    if (anim_fn) {
        anim_fn(anim_f++);
        canvas_touch();
        if (anim_f >= anim_n) anim_fn = NULL;
        return;
    }
    if (S.down) {
        if (st.tool == T_BRUSH) brush_frame();
        else if (st.tool == T_MIXER) mixer_frame();
        else if (st.tool == T_ERASER) eraser_frame();
        canvas_touch();
    }
}

void tool_draw_overlay(int mx, int my, bool over)
{
    if (!over || S.down || anim_running()) return;
    const int cx = mx - CV_X, cy = my - CV_Y;
    switch (st.tool) {
    case T_STAMP: {
        int row;
        Image *sh = stamp_sheet(&row, NULL);
        const int z = 2 * SCALE();
        img_draw_part(sh, mx - 16 * z, my - 16 * z, st.stamp_i * 32, row * 32, 32, 32, z, z, 0xffffff90);
        break; }
    case T_TRUCK: {
        int hx, hy;
        truck_dims_get(&hx, &hy);
        rect_frame(mx - hx, my - hy, hx * 2, hy * 2, 0xffffffff);
        rect_frame(mx - hx - 1, my - hy - 1, hx * 2 + 2, hy * 2 + 2, 0x000000ff);
        break; }
    case T_ERASER: eraser_preview(cx, cy); break;
    default: break;
    }
}

Image *cursor_image(int *hx, int *hy)
{
    const char *f = NULL;
    switch (st.tool) {
    case T_PENCIL: f = "img/cursor-pencil.png"; *hx = 1; *hy = 15; break;
    case T_BRUSH: f = "img/cursor-paint-brush.png"; *hx = 2; *hy = 14; break;
    case T_FLOOD: f = "img/cursor-fill-bucket.png"; *hx = 2; *hy = 14; break;
    case T_MIXER: f = S.down ? "img/cursor-guy-wow.png" : "img/cursor-guy-smile.png"; *hx = 8; *hy = 8; break;
    case T_ERASER:
        if (st.eraser == 4) { f = "img/cursor-tnt-0.png"; *hx = 4; *hy = 7; }
        else if (st.eraser >= 7) { f = "img/cursor-guy-smile.png"; *hx = 8; *hy = 8; }
        break;
    default: break;
    }
    return f ? img_load(f) : NULL;
}
