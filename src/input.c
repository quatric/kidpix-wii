/*
 * Unified input: Wiimote (IR pointer, Nunchuk), Classic Controller, GameCube pad,
 * Wii U Pro Controller, Wii U GamePad, DualShock 3, USB mouse and USB keyboard
 * all drive one cursor + one button mask. The devices come from libinput-wii.
 */
#include <stdio.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <input_wii.h>
#include "kidpix.h"

Input in;

static u32 prev_held;
static float best_mag;
static float best_x, best_y;
static const char *best_src;

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

static u32 map_buttons(u32 b)
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
    if (b & INPUT_BUTTON_Z) out |= IN_B;
    if (b & INPUT_BUTTON_C) out |= IN_1;
    return out;
}

/* Both sticks of every device; screen y grows downward. */
static void lib_stick(int which)
{
    float x, y;
    const char *src;
    if (INPUT_Stick(which, &x, &y, &src) > 0)
        stick(src, x, -y);
}

/* ---- public ------------------------------------------------------------- */
void input_init(void)
{
    INPUT_Init();
    INPUT_SetScreenSize(SCR_W, SCR_H);

    /* Keyboard: Enter = A, Shift = B; arrows move the cursor (below). */
    INPUT_KeyboardMapClear();
    INPUT_KeyboardMapSet(0x28, WPAD_BUTTON_A);
    INPUT_KeyboardMapSet(0x58, WPAD_BUTTON_A);
    INPUT_KeyboardMapSet(0xE1, WPAD_BUTTON_B);
    INPUT_KeyboardMapSet(0xE5, WPAD_BUTTON_B);

    in.x = SCR_W / 2;
    in.y = SCR_H / 2;
    in.source = "";
}

void input_shutdown(void)
{
    INPUT_Shutdown();
}

void input_update(void)
{
    u32 held;
    float px, py;
    int mdx, mdy, mwheel;
    best_mag = 0;
    best_src = NULL;
    in.wheel = 0;
    in.key = 0;
    in.hotkey = 0;

    INPUT_Update();

    held = map_buttons(INPUT_ButtonsHeld());
    if (INPUT_Source()[0]) in.source = INPUT_Source();

    if (INPUT_Pointer(&px, &py)) {
        in.x = px;
        in.y = py;
        in.source = "Wiimote";
        in.pointer_valid = true;
    }
    lib_stick(INPUT_STICK_LEFT);
    lib_stick(INPUT_STICK_RIGHT);

    /* USB mouse: left = A, right = B, middle = 1 */
    INPUT_MouseDelta(&mdx, &mdy, &mwheel);
    if (mwheel > 0) in.wheel = -1;
    if (mwheel < 0) in.wheel = 1;
    {
        const u8 mb = INPUT_MouseButtonsHeld();
        if (mb & INPUT_MOUSE_LEFT) held |= IN_A;
        if (mb & INPUT_MOUSE_RIGHT) held |= IN_B;
        if (mb & INPUT_MOUSE_MIDDLE) held |= IN_1;
    }

    /* USB keyboard */
    {
        bool shift = (INPUT_KeyModifiers() & INPUT_MOD_SHIFT) != 0;
        float kx = 0, ky = 0;
        input_key_event ev;
        if (INPUT_KeyHeld(0x50)) kx -= 1;
        if (INPUT_KeyHeld(0x4f)) kx += 1;
        if (INPUT_KeyHeld(0x52)) ky -= 1;
        if (INPUT_KeyHeld(0x51)) ky += 1;
        if (kx != 0 || ky != 0) stick("Keyboard", kx, ky);
        while (INPUT_KeyPop(&ev)) {
            const u8 code = ev.code;
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
    if (mdx != 0 || mdy != 0) {
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
}
