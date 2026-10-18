/*
 * Kid Pix for Wii - a C / GRRLIB port of the jskidpix Kid Pix 1.0 recreation.
 * Controls are described in README.md.
 */
#include <stdio.h>
#include <stdarg.h>
#include <fat.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <input_wii.h>
#include <wiiupdate.h>
#include "tools_int.h"

#define SUB_Y   CV_H
#define COL_Y   (CV_H + 36)
#define PER_PAGE 14

static int sub_page;
static int last_tool = -1;
static bool menu_open;
static int menu_sel;
static const char *menu_items[4] = { "Resume", "New picture", "Save picture", "Exit" };
static char toast[96];
static int toast_t;
static bool drag_active;
static int last_cx = -1, last_cy = -1;
static float prev_px, prev_py;

void ui_toast(const char *s)
{
    snprintf(toast, sizeof toast, "%s", s);
    toast_t = 150;
}

/* ---- layout helpers ------------------------------------------------------- */
static int pages_for(int tool)
{
    const int n = sub_count(tool);
    return n ? (n + PER_PAGE - 1) / PER_PAGE : 0;
}

static void sync_page(void)
{
    sub_page = 0;
    const int n = sub_count(st.tool);
    for (int i = 0; i < n; i++)
        if (sub_selected(st.tool, i)) { sub_page = i / PER_PAGE; break; }
}

static bool in_rect(float x, float y, float rx, float ry, float rw, float rh)
{
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static void sub_nav(int d)                         /* previous / next page */
{
    if (st.tool == T_STAMP) { stamp_page_move(d); return; }
    const int p = pages_for(st.tool);
    if (p > 1) {
        sub_page = (sub_page + d + p) % p;
        snd_play("kidpix-menu-click-submenu-options");
    }
}

static void step_option(int d)
{
    const int n = sub_count(st.tool);
    if (!n) return;
    int cur = 0;
    for (int i = 0; i < n; i++) if (sub_selected(st.tool, i)) cur = i;
    cur = (cur + d + n) % n;
    sub_select(st.tool, cur);
    sub_page = cur / PER_PAGE;
}

static void step_tool(int d)
{
    int t = st.tool;
    do { t = (t + d + T_COUNT) % T_COUNT; } while (t == T_SAVE || t == T_UNDO);
    tool_select(t);
}

/* ---- UI click handling ----------------------------------------------------------- */
static void ui_click(float x, float y)
{
    if (x < TB_W) {                                             /* tools */
        const int t = ((int)y - 6) / 36;
        if (y >= 6 && t >= 0 && t < T_COUNT) tool_select(t);
        return;
    }
    if (y >= SUB_Y && y < COL_Y) {                              /* sub-menu */
        const int slot = ((int)x - TB_W) / 36;
        const bool arrows = pages_for(st.tool) > 1 || st.tool == T_STAMP;
        if (arrows && slot == 0) { sub_nav(-1); return; }
        if (arrows && slot == 15) { sub_nav(1); return; }
        const int i = sub_page * PER_PAGE + (slot - (arrows ? 1 : 0));
        if (i >= 0 && i < sub_count(st.tool) && slot < (arrows ? 15 : 16)) sub_select(st.tool, i);
        return;
    }
    if (y >= COL_Y) {                                           /* colours */
        if (x >= 584) {
            if (y < COL_Y + 18) palette_next(x < 612 ? -1 : 1);
            return;
        }
        const int col = ((int)x - TB_W) / 34, row = ((int)y - COL_Y) / 18;
        if (col >= 0 && col < 16 && row >= 0 && row < 2) {
            color_set(row * 16 + col);
            snd_play("kidpix-menu-click-submenu-color");
        }
    }
}

/* ---- drawing the UI --------------------------------------------------------------- */
static void draw_arrow_label(int slot, const char *s)
{
    rect_fill(TB_W + slot * 36 + 2, SUB_Y + 4, 32, 28, 0xa8b8e0ff);
    rect_frame(TB_W + slot * 36 + 2, SUB_Y + 4, 32, 28, 0x303060ff);
    text_draw_center(TB_W + slot * 36 + 18, SUB_Y + 11, FONT_UI, s, 0x101040ff);
}

static void draw_ui(float mx, float my)
{
    /* tool column */
    rect_fill(0, 0, TB_W, SCR_H, 0xd0d4f0ff);
    for (int t = 0; t < T_COUNT; t++) {
        const int y = 6 + t * 36;
        const bool sel = st.tool == t;
        const bool hov = in_rect(mx, my, 0, y, TB_W, 36);
        if (sel) rect_fill(1, y, TB_W - 2, 36, 0xffe060ff);
        else if (hov) rect_fill(1, y, TB_W - 2, 36, 0xeeeeffff);
        char n[48];
        snprintf(n, sizeof n, "img/kp-m_%d.png", 27 + t);
        img_draw(img_load(n), 3, y + 2, 1, 1, 0xffffffff);
        if (sel) rect_frame(1, y, TB_W - 2, 36, 0x000000ff);
    }

    /* sub-menu strip */
    rect_fill(TB_W, SUB_Y, SCR_W - TB_W, 36, 0xc4d4f4ff);
    const int n = sub_count(st.tool);
    const bool arrows = pages_for(st.tool) > 1 || st.tool == T_STAMP;
    if (arrows) {
        draw_arrow_label(0, "<");
        draw_arrow_label(15, ">");
    }
    const int first = arrows ? 1 : 0;
    for (int s = first; s < (arrows ? 15 : 16); s++) {
        const int i = sub_page * PER_PAGE + (s - first);
        if (i >= n) break;
        const int x = TB_W + s * 36;
        const bool sel = sub_selected(st.tool, i);
        const bool hov = in_rect(mx, my, x, SUB_Y, 36, 36);
        if (sel) rect_fill(x, SUB_Y, 36, 36, 0xffe060ff);
        else if (hov) rect_fill(x, SUB_Y, 36, 36, 0xeeeeffff);
        if (st.tool == T_STAMP) {
            int row;
            Image *sh = stamp_sheet(&row, NULL);
            img_draw_part(sh, x + 2, SUB_Y + 2, i * 32, row * 32, 32, 32, 1, 1, 0xffffffff);
        } else {
            img_draw(sub_icon(st.tool, i), x, SUB_Y, 1, 1, 0xffffffff);
        }
        if (sel) rect_frame(x, SUB_Y, 36, 36, 0x000000ff);
    }
    if (st.tool == T_STAMP) {
        char b[24];
        snprintf(b, sizeof b, "%d/80", st.stamp_page + 1);
        text_draw_center(TB_W + 8 * 36, SUB_Y - 14, FONT_UI, b, 0x404040ff);
    }

    /* colour strip */
    rect_fill(TB_W, COL_Y, SCR_W - TB_W, 36, 0xe8e8f8ff);
    for (int i = 0; i < 32; i++) {
        const int x = TB_W + (i % 16) * 34, y = COL_Y + (i / 16) * 18;
        rect_fill(x, y, 33, 17, kp_palettes[st.palette][i]);
        if (i == st.color_idx) {
            rect_frame(x, y, 33, 17, 0xffffffff);
            rect_frame(x + 1, y + 1, 31, 15, 0x000000ff);
        }
    }
    rect_fill(584, COL_Y, 56, 36, 0xd0d4f0ff);
    rect_fill(586, COL_Y + 1, 26, 16, 0xa8b8e0ff);
    rect_fill(614, COL_Y + 1, 24, 16, 0xa8b8e0ff);
    text_draw_center(599, COL_Y + 2, FONT_UI, "<", 0x101040ff);
    text_draw_center(626, COL_Y + 2, FONT_UI, ">", 0x101040ff);
    rect_fill(586, COL_Y + 19, 52, 16, st.color);
    rect_frame(586, COL_Y + 19, 52, 16, 0x000000ff);
}

static void draw_cursor(float mx, float my)
{
    const bool over = mx >= CV_X && my < CV_H && !menu_open;
    int hx = 0, hy = 0;
    Image *ci = over ? cursor_image(&hx, &hy) : NULL;
    if (ci) {
        img_draw(ci, mx - hx, my - hy, 1, 1, 0xffffffff);
        return;
    }
    if (over) {
        rect_fill(mx - 8, my - 1, 17, 3, 0xffffffff);
        rect_fill(mx - 1, my - 8, 3, 17, 0xffffffff);
        rect_fill(mx - 7, my, 15, 1, 0x000000ff);
        rect_fill(mx, my - 7, 1, 15, 0x000000ff);
        return;
    }
    for (int r = 0; r < 14; r++) {                              /* arrow */
        rect_fill(mx, my + r, 1 + r * 0.6f + 2, 1, 0x000000ff);
        rect_fill(mx + 1, my + r + 1, r * 0.6f, 1, 0xffffffff);
    }
}

static void draw_menu(void)
{
    rect_fill(0, 0, SCR_W, SCR_H, 0x00000090);
    rect_fill(150, 90, 340, 300, 0xf0f0ffff);
    rect_frame(150, 90, 340, 300, 0x000000ff);
    rect_frame(151, 91, 338, 298, 0x6060c0ff);
    text_draw_center(320, 100, FONT_BIG, "Kid Pix", 0x4040c0ff);
    for (int i = 0; i < 4; i++) {
        const int y = 160 + i * 36;
        if (i == menu_sel) rect_fill(170, y, 300, 30, 0xffe060ff);
        rect_frame(170, y, 300, 30, 0x303060ff);
        text_draw_center(320, y + 8, FONT_UI, menu_items[i], 0x101040ff);
    }
    text_draw_center(320, 316, FONT_UI, "A draw   B bigger   2 alt   1 undo", 0x303030ff);
    text_draw_center(320, 332, FONT_UI, "+/- colour   < > tool   ^ v option", 0x303030ff);
    char b[80];
    snprintf(b, sizeof b, "Pointer: %s   Saves: %s", in.source[0] ? in.source : "-", save_dir);
    text_draw_center(320, 356, FONT_UI, b, 0x505050ff);
}

static void menu_activate(int i)
{
    switch (i) {
    case 0: break;
    case 1: file_new(); break;
    case 2: tool_select(T_SAVE); break;
    case 3: exit(0);
    }
    menu_open = false;
}

/* ---- start-up ---------------------------------------------------------------------- */
static bool have_assets(const char *root)
{
    char p[300];
    snprintf(p, sizeof p, "%s/img/kp-m_27.png", root);
    FILE *f = fopen(p, "rb");
    if (f) fclose(f);
    return f != NULL;
}

static void find_assets(const char *argv0)
{
    char cand[300];
    if (argv0 && strchr(argv0, '/')) {
        snprintf(cand, sizeof cand, "%s", argv0);
        char *s = strrchr(cand, '/');
        if (s) *s = 0;
        strncat(cand, "/assets", sizeof cand - strlen(cand) - 1);
        if (have_assets(cand)) { snprintf(asset_root, sizeof asset_root, "%s", cand); return; }
    }
    static const char *list[] = { "sd:/apps/kidpix/assets", "usb:/apps/kidpix/assets", "assets", "./assets" };
    for (unsigned i = 0; i < sizeof list / sizeof *list; i++)
        if (have_assets(list[i])) { snprintf(asset_root, sizeof asset_root, "%s", list[i]); return; }
}

/* Update popup: shown when a newer release is out; it has the controls while it is up. */
static wiiupdate_popup upd;
static int upd_open;

static void draw_update_popup(void)
{
    const int w = 500, h = upd.progress >= 0 || upd.detail[0] ? 176 : 150;
    const int x = (640 - w) / 2, y = (480 - h) / 2;
    rect_fill(0, 0, 640, 480, 0x00000070);
    rect_fill(x - 3, y - 3, w + 6, h + 6, 0x303030ff);
    rect_fill(x, y, w, h, 0xfffcf4ff);
    text_draw_center(320, y + 12, FONT_BIG, upd.title, 0x202020ff);
    text_draw_center(320, y + 76, FONT_UI, upd.text, 0x303030ff);
    int row = y + 104;
    if (upd.progress >= 0) {
        rect_fill(x + 50, row + 4, w - 100, 12, 0xc8c8c8ff);
        rect_fill(x + 50, row + 4, (w - 100) * upd.progress / 100.0f, 12, 0x2f9e44ff);
        row += 26;
    } else if (upd.detail[0]) {
        text_draw_center(320, row, FONT_UI, upd.detail, 0x1c6fc8ff);
        row += 26;
    }
    text_draw_center(320, row + 8, FONT_UI, upd.hint, 0x606060ff);
}

int main(int argc, char **argv)
{
    gfx_init();
    fatInitDefault();
    find_assets(argv && argv[0] ? argv[0] : NULL);
    input_init();
    atexit(input_shutdown);

    if (!have_assets(asset_root)) {
        for (;;) {
            input_update();
            GRRLIB_FillScreen(0x300000ff);
            text_draw(40, 180, FONT_BIG, "Kid Pix: assets not found", 0xffffffff);
            text_draw(40, 250, FONT_UI, "Copy the 'assets' folder next to boot.dol", 0xffffffff);
            text_draw(40, 270, FONT_UI, "(SD or USB: /apps/kidpix/assets). Press HOME to exit.", 0xffffffff);
            GRRLIB_Render();
            if (in.down & IN_HOME) exit(0);
        }
    }

    snd_init();
    tools_init();
    sync_page();
    ui_toast("Welcome to Kid Pix!");
    WIIUPDATE_SetAppPath(argc > 0 ? argv[0] : NULL);
    WIIUPDATE_Start("quatric", "kidpix-wii", APP_VERSION);

    for (;;) {
        input_update();
        {
            const int was_open = upd_open;
            const int ust = WIIUPDATE_Popup(INPUT_ButtonsDown(), &upd);
            if (ust == WIIUPDATE_POPUP_EXIT) exit(0);
            upd_open = ust == WIIUPDATE_POPUP_OPEN;
            if (upd_open || was_open) {
                in.down = in.held = in.up = 0;
                in.hotkey = 0;
                in.key = 0;
                in.wheel = 0;
            }
        }
        const float mx = in.x, my = in.y;
        const bool over = mx >= CV_X && my < CV_H;
        const int cx = (int)mx - CV_X, cy = (int)my - CV_Y;
        shift_mod = (in.held & IN_B) != 0;
        alt_mod = (in.held & IN_2) != 0;

        if (st.tool != last_tool) { last_tool = st.tool; sync_page(); }

        /* ---- menu ---- */
        if (in.down & IN_HOME || in.hotkey == HK_MENU) {
            menu_open = !menu_open;
            menu_sel = 0;
            if (drag_active) { tool_up(cx, cy); drag_active = false; }
        }
        if (menu_open) {
            if (in.down & IN_UP) menu_sel = (menu_sel + 3) % 4;
            if (in.down & IN_DOWN) menu_sel = (menu_sel + 1) % 4;
            if (mx != prev_px || my != prev_py)
                for (int i = 0; i < 4; i++)
                    if (in_rect(mx, my, 170, 160 + i * 36, 300, 30)) menu_sel = i;
            if (in.down & IN_A) {
                if (in_rect(mx, my, 170, 160, 300, 4 * 36) || !in.pointer_valid) menu_activate(menu_sel);
                else if (!in_rect(mx, my, 150, 90, 340, 300)) menu_open = false;
            }
            prev_px = mx; prev_py = my;
        } else {
            /* ---- shortcuts ---- */
            if (in.hotkey >= HK_TOOL_BASE && in.hotkey < HK_TOOL_BASE + T_COUNT && !(tool_is_text() && in.key))
                tool_select(in.hotkey - HK_TOOL_BASE);
            if (in.hotkey == HK_UNDO) undo_do();
            if (in.hotkey == HK_COLOR_NEXT) color_next(1);
            if (in.hotkey == HK_COLOR_PREV) color_next(-1);
            if (in.key && tool_is_text()) tool_text_input(in.key);

            if (in.down & IN_1) undo_do();
            if (in.down & IN_PLUS) { if (alt_mod) palette_next(1); else color_next(1); }
            if (in.down & IN_MINUS) { if (alt_mod) palette_next(-1); else color_next(-1); }
            if (in.wheel > 0) color_next(1);
            if (in.wheel < 0) color_next(-1);
            if (in.down & IN_LEFT) step_tool(-1);
            if (in.down & IN_RIGHT) step_tool(1);
            if (in.down & IN_UP) step_option(-1);
            if (in.down & IN_DOWN) step_option(1);

            /* ---- pointer / drawing ---- */
            if (in.down & IN_A) {
                if (over && !tool_busy()) {
                    tool_down(cx, cy);
                    drag_active = true;
                    last_cx = cx; last_cy = cy;
                } else if (!over) {
                    ui_click(mx, my);
                }
            }
            if (drag_active) {
                int dx = cx, dy = cy;
                if (dx < 0) dx = 0;
                if (dy < 0) dy = 0;
                if (dx >= CV_W) dx = CV_W - 1;
                if (dy >= CV_H) dy = CV_H - 1;
                if ((in.held & IN_A) && (dx != last_cx || dy != last_cy)) {
                    tool_move(dx, dy);
                    last_cx = dx; last_cy = dy;
                }
                if (in.up & IN_A) {
                    tool_up(dx, dy);
                    drag_active = false;
                }
            }
            tool_frame((int)mx, (int)my, over);
            if (!tool_busy() && drag_active && !S.down && !(in.held & IN_A)) drag_active = false;
        }

        /* ---- draw ---- */
        GRRLIB_FillScreen(0xf4f0e8ff);
        canvas_present();
        if (!menu_open) tool_draw_overlay((int)mx, (int)my, over);
        draw_ui(mx, my);
        if (toast_t > 0) {
            toast_t--;
            const int w = text_width(FONT_UI, toast) + 24;
            rect_fill(CV_X + (CV_W - w) / 2, 8, w, 24, 0x000000b0);
            text_draw_center(CV_X + CV_W / 2, 13, FONT_UI, toast, 0xffffffff);
        }
        if (menu_open) draw_menu();
        if (upd_open) draw_update_popup();
        draw_cursor(mx, my);
        GRRLIB_Render();
    }
    return 0;
}
