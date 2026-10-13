/*
 * Unified input: Wiimote (IR pointer, Nunchuk), Classic Controller, GameCube pad,
 * Wii U Pro Controller (libwupc), Wii U GamePad (libwiidrc), DualShock 3
 * (libsicksaxis), USB mouse and USB keyboard all drive one cursor + one button mask.
 */
#include <stdio.h>
#include <unistd.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <ogc/usbmouse.h>
#include <wiikeyboard/usbkeyboard.h>
#ifdef HAVE_WIIDRC
#include <wiidrc/wiidrc.h>
#endif
#ifdef HAVE_WUPC
#include <wupc/wupc.h>
#endif
#ifdef HAVE_SICKSAXIS
#include <sicksaxis.h>
#endif
#include "kidpix.h"

Input in;

static u32 prev_held;
static float best_mag;
static float best_x, best_y;
static const char *best_src;

#ifdef HAVE_SICKSAXIS
static struct ss_device ds3;
static int ds3_initialized;
#endif

/* ---- keyboard (polled on a thread, like GlowWii) ------------------------ */
static lwp_t kbd_thread_hndl = LWP_THREAD_NULL;
static volatile bool kbd_thread_run;
static volatile u8 key_held[256];
static volatile u8 key_queue[32];
static volatile int kq_head, kq_tail;

static void kq_push(u8 code)
{
    const int n = (kq_head + 1) & 31;
    if (n != kq_tail) { key_queue[kq_head] = code; kq_head = n; }
}

static void kb_event(USBKeyboard_event ev)
{
    if (ev.type == USBKEYBOARD_PRESSED) {
        key_held[ev.keyCode] = 1;
        kq_push(ev.keyCode);
    } else if (ev.type == USBKEYBOARD_RELEASED) {
        key_held[ev.keyCode] = 0;
    }
}

static void *kbd_thread(void *arg)
{
    (void)arg;
    while (kbd_thread_run) {
        if (!USBKeyboard_IsConnected()) USBKeyboard_Open(kb_event);
        USBKeyboard_Scan();
        usleep(2000);
    }
    return NULL;
}

static int hid_to_ascii(u8 c, bool shift)
{
    if (c >= 0x04 && c <= 0x1d) return (shift ? 'A' : 'a') + (c - 0x04);
    if (c >= 0x1e && c <= 0x26) {
        static const char sh[] = "!@#$%^&*(";
        return shift ? sh[c - 0x1e] : '1' + (c - 0x1e);
    }
    switch (c) {
    case 0x27: return shift ? ')' : '0';
    case 0x2c: return ' ';
    case 0x2d: return shift ? '_' : '-';
    case 0x2e: return shift ? '+' : '=';
    case 0x33: return shift ? ':' : ';';
    case 0x34: return shift ? '"' : '\'';
    case 0x36: return shift ? '<' : ',';
    case 0x37: return shift ? '>' : '.';
    case 0x38: return shift ? '?' : '/';
    default: return 0;
    }
}

/* ---- helpers ------------------------------------------------------------ */
static void stick(const char *src, float x, float y)
{
    const float m = sqrtf(x * x + y * y);
    if (m > 1.0f) { x /= m; y /= m; }
    const float mm = m > 1.0f ? 1.0f : m;
    if (mm < 0.18f) return;
    if (mm > best_mag) { best_mag = mm; best_x = x; best_y = y; best_src = src; }
}

static u32 map_classic(u32 b)
{
    u32 out = 0;
    if (b & (WPAD_CLASSIC_BUTTON_ZR | WPAD_CLASSIC_BUTTON_PLUS | WPAD_CLASSIC_BUTTON_FULL_R)) out |= IN_PLUS;
    if (b & (WPAD_CLASSIC_BUTTON_ZL | WPAD_CLASSIC_BUTTON_MINUS | WPAD_CLASSIC_BUTTON_FULL_L)) out |= IN_MINUS;
    if (b & WPAD_CLASSIC_BUTTON_A) out |= IN_A;
    if (b & WPAD_CLASSIC_BUTTON_B) out |= IN_B;
    if (b & WPAD_CLASSIC_BUTTON_X) out |= IN_2;
    if (b & WPAD_CLASSIC_BUTTON_Y) out |= IN_1;
    if (b & WPAD_CLASSIC_BUTTON_HOME) out |= IN_HOME;
    if (b & WPAD_CLASSIC_BUTTON_UP) out |= IN_UP;
    if (b & WPAD_CLASSIC_BUTTON_DOWN) out |= IN_DOWN;
    if (b & WPAD_CLASSIC_BUTTON_LEFT) out |= IN_LEFT;
    if (b & WPAD_CLASSIC_BUTTON_RIGHT) out |= IN_RIGHT;
    return out;
}

static u32 map_wiimote(u32 b)
{
    u32 out = 0;
    if (b & WPAD_BUTTON_A) out |= IN_A;
    if (b & WPAD_BUTTON_B) out |= IN_B;
    if (b & WPAD_BUTTON_1) out |= IN_1;
    if (b & WPAD_BUTTON_2) out |= IN_2;
    if (b & WPAD_BUTTON_PLUS) out |= IN_PLUS;
    if (b & WPAD_BUTTON_MINUS) out |= IN_MINUS;
    if (b & WPAD_BUTTON_HOME) out |= IN_HOME;
    if (b & WPAD_BUTTON_UP) out |= IN_UP;
    if (b & WPAD_BUTTON_DOWN) out |= IN_DOWN;
    if (b & WPAD_BUTTON_LEFT) out |= IN_LEFT;
    if (b & WPAD_BUTTON_RIGHT) out |= IN_RIGHT;
    if (b & WPAD_NUNCHUK_BUTTON_Z) out |= IN_B;
    if (b & WPAD_NUNCHUK_BUTTON_C) out |= IN_1;
    return out;
}

#ifdef HAVE_WIIDRC
static u32 map_drc(u32 b)
{
    u32 out = 0;
    if (b & (WIIDRC_BUTTON_R | WIIDRC_BUTTON_ZR | WIIDRC_BUTTON_PLUS)) out |= IN_PLUS;
    if (b & (WIIDRC_BUTTON_L | WIIDRC_BUTTON_ZL | WIIDRC_BUTTON_MINUS)) out |= IN_MINUS;
    if (b & WIIDRC_BUTTON_A) out |= IN_A;
    if (b & WIIDRC_BUTTON_B) out |= IN_B;
    if (b & WIIDRC_BUTTON_X) out |= IN_2;
    if (b & WIIDRC_BUTTON_Y) out |= IN_1;
    if (b & WIIDRC_BUTTON_HOME) out |= IN_HOME;
    if (b & WIIDRC_BUTTON_UP) out |= IN_UP;
    if (b & WIIDRC_BUTTON_DOWN) out |= IN_DOWN;
    if (b & WIIDRC_BUTTON_LEFT) out |= IN_LEFT;
    if (b & WIIDRC_BUTTON_RIGHT) out |= IN_RIGHT;
    return out;
}
#endif

static void joy(const char *src, joystick_t *j)
{
    if (j->mag < 0.01f) return;
    const float a = j->ang * 0.017453292f;
    stick(src, sinf(a) * j->mag, -cosf(a) * j->mag);
}

/* ---- public ------------------------------------------------------------- */
void input_init(void)
{
    PAD_Init();
#ifdef HAVE_WUPC
    WUPC_Init();
#endif
    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_ALL, SCR_W, SCR_H);
#ifdef HAVE_WIIDRC
    WiiDRC_Init();
#endif
    USB_Initialize();
#ifdef HAVE_SICKSAXIS
    if (ss_init() >= 0 && ss_initialize(&ds3) >= 0) {
        ds3_initialized = 1;
        if (ss_open(&ds3) > 0) ss_start_reading(&ds3);
    }
#endif
    MOUSE_Init();
    USBKeyboard_Initialize();
    kbd_thread_run = true;
    LWP_CreateThread(&kbd_thread_hndl, kbd_thread, 0, 0, 0x4000, 0x7f);

    in.x = SCR_W / 2;
    in.y = SCR_H / 2;
    in.source = "";
}

void input_shutdown(void)
{
    kbd_thread_run = false;
    usleep(5000);
    USBKeyboard_Close();
    USBKeyboard_Deinitialize();
    if (kbd_thread_hndl != LWP_THREAD_NULL) LWP_JoinThread(kbd_thread_hndl, 0);
    kbd_thread_hndl = LWP_THREAD_NULL;
    MOUSE_Deinit();
#ifdef HAVE_SICKSAXIS
    if (ds3_initialized) {
        if (ss_is_connected(&ds3)) ss_stop_reading(&ds3);
        ss_close(&ds3);
    }
#endif
#ifdef HAVE_WUPC
    WUPC_Shutdown();
#endif
}

void input_update(void)
{
    static int frame;
    u32 held = 0;
    float mdx = 0, mdy = 0;
    bool mouse_moved = false;
    best_mag = 0;
    best_src = NULL;
    in.wheel = 0;
    in.key = 0;
    in.hotkey = 0;
    frame++;

    /* Wii U Pro Controller (emulated classic) */
#ifdef HAVE_WUPC
    WUPC_UpdateButtonStats();
    for (int ch = 0; ch < 4; ch++) {
        const u32 b = WUPC_ButtonsHeld(ch);
        if (b) { held |= map_classic(b); in.source = "Wii U Pro"; }
        stick("Wii U Pro", WUPC_lStickX(ch) / 900.0f, -WUPC_lStickY(ch) / 900.0f);
        stick("Wii U Pro", WUPC_rStickX(ch) / 900.0f, -WUPC_rStickY(ch) / 900.0f);
    }
#endif

    /* Wiimotes (+ Nunchuk / Classic) */
    WPAD_ScanPads();
    for (int ch = 0; ch < 4; ch++) {
        u32 type;
        if (WPAD_Probe(ch, &type) != WPAD_ERR_NONE) continue;
        const u32 b = WPAD_ButtonsHeld(ch);
        WPADData *d = WPAD_Data(ch);
        held |= map_wiimote(b) | map_classic(b);
        if (b) in.source = d->exp.type == WPAD_EXP_CLASSIC ? "Classic" : "Wiimote";
        if (d->ir.valid) {
            in.x = d->ir.x;
            in.y = d->ir.y;
            in.source = "Wiimote";
            in.pointer_valid = true;
        }
        if (d->exp.type == WPAD_EXP_NUNCHUK) joy("Nunchuk", &d->exp.nunchuk.js);
        else if (d->exp.type == WPAD_EXP_CLASSIC) {
            joy("Classic", &d->exp.classic.ljs);
            joy("Classic", &d->exp.classic.rjs);
        }
    }

    /* Wii U GamePad */
#ifdef HAVE_WIIDRC
    if (WiiDRC_Inited() && WiiDRC_Connected() && WiiDRC_ScanPads()) {
        const u32 b = WiiDRC_ButtonsHeld();
        if (b) { held |= map_drc(b); in.source = "GamePad"; }
        stick("GamePad", WiiDRC_lStickX() / 900.0f, -WiiDRC_lStickY() / 900.0f);
        stick("GamePad", WiiDRC_rStickX() / 900.0f, -WiiDRC_rStickY() / 900.0f);
    }
#endif

    /* DualShock 3 */
#ifdef HAVE_SICKSAXIS
    if (ds3_initialized) {
        if (!ss_is_connected(&ds3)) {
            if (ss_open(&ds3) > 0) ss_start_reading(&ds3);
        } else {
            const struct SS_BUTTONS *b = &ds3.pad.buttons;
            u32 h = 0;
            if (b->start || b->R1 || b->R2) h |= IN_PLUS;
            if (b->select || b->L1 || b->L2) h |= IN_MINUS;
            if (b->cross) h |= IN_A;
            if (b->circle) h |= IN_B;
            if (b->triangle) h |= IN_2;
            if (b->square) h |= IN_1;
            if (b->PS) h |= IN_HOME;
            if (b->up) h |= IN_UP;
            if (b->down) h |= IN_DOWN;
            if (b->left) h |= IN_LEFT;
            if (b->right) h |= IN_RIGHT;
            if (h) { held |= h; in.source = "DualShock 3"; }
            stick("DualShock 3", (ds3.pad.left_analog.x - 128) / 110.0f, (ds3.pad.left_analog.y - 128) / 110.0f);
            stick("DualShock 3", (ds3.pad.right_analog.x - 128) / 110.0f, (ds3.pad.right_analog.y - 128) / 110.0f);
        }
    }
#endif

    /* GameCube pads */
    PAD_ScanPads();
    for (int ch = 0; ch < 4; ch++) {
        const u16 b = PAD_ButtonsHeld(ch);
        u32 h = 0;
        if (b & (PAD_TRIGGER_R | PAD_TRIGGER_Z)) h |= IN_PLUS;
        if (b & PAD_TRIGGER_L) h |= IN_MINUS;
        if (b & PAD_BUTTON_A) h |= IN_A;
        if (b & PAD_BUTTON_B) h |= IN_B;
        if (b & PAD_BUTTON_X) h |= IN_2;
        if (b & PAD_BUTTON_Y) h |= IN_1;
        if (b & PAD_BUTTON_START) h |= IN_HOME;
        if (b & PAD_BUTTON_UP) h |= IN_UP;
        if (b & PAD_BUTTON_DOWN) h |= IN_DOWN;
        if (b & PAD_BUTTON_LEFT) h |= IN_LEFT;
        if (b & PAD_BUTTON_RIGHT) h |= IN_RIGHT;
        if (h) { held |= h; in.source = "GameCube"; }
        stick("GameCube", PAD_StickX(ch) / 70.0f, -PAD_StickY(ch) / 70.0f);
        stick("GameCube", PAD_SubStickX(ch) / 60.0f, -PAD_SubStickY(ch) / 60.0f);
    }

    /* USB mouse */
    if (!MOUSE_IsConnected() && (frame % 180) == 0) MOUSE_Init();
    {
        mouse_event me;
        int guard = 0;
        static u8 mbtn_prev;
        while (guard++ < 64 && MOUSE_GetEvent(&me) > 0) {
            mdx += me.rx;
            mdy += me.ry;
            if (me.rz > 0) in.wheel--;
            if (me.rz < 0) in.wheel++;
            mbtn_prev = me.button;
        }
        if (MOUSE_IsConnected()) {
            if (mbtn_prev & 1) held |= IN_A;
            if (mbtn_prev & 2) held |= IN_B;
            if (mbtn_prev & 4) held |= IN_1;
        }
        if (mdx != 0 || mdy != 0) mouse_moved = true;
    }

    /* USB keyboard */
    {
        bool shift = key_held[0xe1] || key_held[0xe5];
        float kx = 0, ky = 0;
        if (key_held[0x50]) kx -= 1;
        if (key_held[0x4f]) kx += 1;
        if (key_held[0x52]) ky -= 1;
        if (key_held[0x51]) ky += 1;
        if (kx != 0 || ky != 0) stick("Keyboard", kx, ky);
        if (key_held[0x28] || key_held[0x58]) { held |= IN_A; in.source = "Keyboard"; }
        if (shift) held |= IN_B;
        if (key_held[0x2b]) { }
        while (kq_tail != kq_head) {
            const u8 code = key_queue[kq_tail];
            kq_tail = (kq_tail + 1) & 31;
            if (code == 0x29) in.hotkey = HK_MENU;
            else if (code == 0x2a || code == 0x4c) in.hotkey = HK_UNDO;
            else if (code == 0x2b || code == 0x4e) in.hotkey = HK_COLOR_NEXT;
            else if (code == 0x4b) in.hotkey = HK_COLOR_PREV;
            else if (code >= 0x1e && code <= 0x27) in.hotkey = HK_TOOL_BASE + 1 + (code == 0x27 ? 9 : code - 0x1e);
            else if (code == 0x2d) in.hotkey = HK_TOOL_BASE + 11;
            else if (code == 0x2e) in.hotkey = HK_TOOL_BASE + 12;
            else if (code == 0x3a) in.hotkey = HK_TOOL_BASE + 0;
            const int a = hid_to_ascii(code, shift);
            if (a) in.key = a;
            in.source = "Keyboard";
        }
    }

    /* cursor motion from analog sticks / mouse */
    if (best_mag > 0 && best_src) {
        const float sp = 11.0f * powf(best_mag, 1.6f) + 0.6f;
        in.x += best_x * sp;
        in.y += best_y * sp;
        in.source = best_src;
    }
    if (mouse_moved) {
        in.x += mdx * 1.4f;
        in.y += mdy * 1.4f;
        in.source = "Mouse";
    }
    if (in.x < 0) in.x = 0;
    if (in.y < 0) in.y = 0;
    if (in.x > SCR_W - 1) in.x = SCR_W - 1;
    if (in.y > SCR_H - 1) in.y = SCR_H - 1;

    in.held = held;
    in.down = held & ~prev_held;
    in.up = ~held & prev_held;
    prev_held = held;
    if (in.wheel == 0 && (in.down & IN_PLUS)) in.wheel = 0;
}
