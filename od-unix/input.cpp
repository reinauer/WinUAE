#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "traps.h"
#include "inputdevice.h"
#include "input.h"

int pause_emulation;
int tablet_log;
int key_swap_hack;

static TCHAR empty_friendly[] = _T("Unix placeholder");
static TCHAR empty_unique_name[] = _T("unix.placeholder");
static TCHAR mouse_friendly[] = _T("Unix Mouse");
static TCHAR mouse_unique_name[] = _T("unix.mouse");
static TCHAR mouse_axis_names[][16] = {
    _T("X Axis"),
    _T("Y Axis"),
    _T("Wheel")
};
static TCHAR mouse_button_names[][16] = {
    _T("Button 1"),
    _T("Button 2"),
    _T("Button 3")
};
static TCHAR keyboard_friendly[] = _T("Unix Keyboard");
static TCHAR keyboard_unique_name[] = _T("unix.keyboard");
static bool mouse_active;
static bool keyboard_state[512];

enum {
    UKEY_A = 4,
    UKEY_B = 5,
    UKEY_C = 6,
    UKEY_D = 7,
    UKEY_E = 8,
    UKEY_F = 9,
    UKEY_G = 10,
    UKEY_H = 11,
    UKEY_I = 12,
    UKEY_J = 13,
    UKEY_K = 14,
    UKEY_L = 15,
    UKEY_M = 16,
    UKEY_N = 17,
    UKEY_O = 18,
    UKEY_P = 19,
    UKEY_Q = 20,
    UKEY_R = 21,
    UKEY_S = 22,
    UKEY_T = 23,
    UKEY_U = 24,
    UKEY_V = 25,
    UKEY_W = 26,
    UKEY_X = 27,
    UKEY_Y = 28,
    UKEY_Z = 29,
    UKEY_1 = 30,
    UKEY_2 = 31,
    UKEY_3 = 32,
    UKEY_4 = 33,
    UKEY_5 = 34,
    UKEY_6 = 35,
    UKEY_7 = 36,
    UKEY_8 = 37,
    UKEY_9 = 38,
    UKEY_0 = 39,
    UKEY_RETURN = 40,
    UKEY_ESCAPE = 41,
    UKEY_BACKSPACE = 42,
    UKEY_TAB = 43,
    UKEY_SPACE = 44,
    UKEY_MINUS = 45,
    UKEY_EQUALS = 46,
    UKEY_LEFTBRACKET = 47,
    UKEY_RIGHTBRACKET = 48,
    UKEY_BACKSLASH = 49,
    UKEY_NONUSHASH = 50,
    UKEY_SEMICOLON = 51,
    UKEY_APOSTROPHE = 52,
    UKEY_GRAVE = 53,
    UKEY_COMMA = 54,
    UKEY_PERIOD = 55,
    UKEY_SLASH = 56,
    UKEY_CAPSLOCK = 57,
    UKEY_F1 = 58,
    UKEY_F2 = 59,
    UKEY_F3 = 60,
    UKEY_F4 = 61,
    UKEY_F5 = 62,
    UKEY_F6 = 63,
    UKEY_F7 = 64,
    UKEY_F8 = 65,
    UKEY_F9 = 66,
    UKEY_F10 = 67,
    UKEY_F11 = 68,
    UKEY_F12 = 69,
    UKEY_PRINTSCREEN = 70,
    UKEY_SCROLLLOCK = 71,
    UKEY_PAUSE = 72,
    UKEY_INSERT = 73,
    UKEY_HOME = 74,
    UKEY_PAGEUP = 75,
    UKEY_DELETE = 76,
    UKEY_END = 77,
    UKEY_PAGEDOWN = 78,
    UKEY_RIGHT = 79,
    UKEY_LEFT = 80,
    UKEY_DOWN = 81,
    UKEY_UP = 82,
    UKEY_NUMLOCKCLEAR = 83,
    UKEY_KP_DIVIDE = 84,
    UKEY_KP_MULTIPLY = 85,
    UKEY_KP_MINUS = 86,
    UKEY_KP_PLUS = 87,
    UKEY_KP_ENTER = 88,
    UKEY_KP_1 = 89,
    UKEY_KP_2 = 90,
    UKEY_KP_3 = 91,
    UKEY_KP_4 = 92,
    UKEY_KP_5 = 93,
    UKEY_KP_6 = 94,
    UKEY_KP_7 = 95,
    UKEY_KP_8 = 96,
    UKEY_KP_9 = 97,
    UKEY_KP_0 = 98,
    UKEY_KP_PERIOD = 99,
    UKEY_NONUSBACKSLASH = 100,
    UKEY_APPLICATION = 101,
    UKEY_F13 = 104,
    UKEY_F14 = 105,
    UKEY_F15 = 106,
    UKEY_AUDIOSTOP = 260,
    UKEY_AUDIOPLAY = 261,
    UKEY_AUDIOPREV = 259,
    UKEY_AUDIONEXT = 258,
    UKEY_LCTRL = 224,
    UKEY_LSHIFT = 225,
    UKEY_LALT = 226,
    UKEY_LGUI = 227,
    UKEY_RCTRL = 228,
    UKEY_RSHIFT = 229,
    UKEY_RALT = 230,
    UKEY_RGUI = 231
};

#define K(scancode, event) { scancode, { { event, 0 } } }
#define KF(scancode, event, flags) { scancode, { { event, flags } } }
#define K2(scancode, event1, flags1, event2, flags2) { scancode, { { event1, flags1 }, { event2, flags2 } } }
#define K3(scancode, event1, flags1, event2, flags2, event3, flags3) { scancode, { { event1, flags1 }, { event2, flags2 }, { event3, flags3 } } }
#define K4(scancode, event1, flags1, event2, flags2, event3, flags3, event4, flags4) { scancode, { { event1, flags1 }, { event2, flags2 }, { event3, flags3 }, { event4, flags4 } } }

static uae_input_device_kbr_default keytrans_amiga[] = {
    K(UKEY_ESCAPE, INPUTEVENT_KEY_ESC),

    K3(UKEY_F1, INPUTEVENT_KEY_F1, 0, INPUTEVENT_SPC_FLOPPY0, ID_FLAG_QUALIFIER_SPECIAL, INPUTEVENT_SPC_EFLOPPY0, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SHIFT),
    K3(UKEY_F2, INPUTEVENT_KEY_F2, 0, INPUTEVENT_SPC_FLOPPY1, ID_FLAG_QUALIFIER_SPECIAL, INPUTEVENT_SPC_EFLOPPY1, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SHIFT),
    K3(UKEY_F3, INPUTEVENT_KEY_F3, 0, INPUTEVENT_SPC_FLOPPY2, ID_FLAG_QUALIFIER_SPECIAL, INPUTEVENT_SPC_EFLOPPY2, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SHIFT),
    K3(UKEY_F4, INPUTEVENT_KEY_F4, 0, INPUTEVENT_SPC_FLOPPY3, ID_FLAG_QUALIFIER_SPECIAL, INPUTEVENT_SPC_EFLOPPY3, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SHIFT),
    K3(UKEY_F5, INPUTEVENT_KEY_F5, 0, INPUTEVENT_SPC_CD0, ID_FLAG_QUALIFIER_SPECIAL, INPUTEVENT_SPC_ECD0, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SHIFT),
    K3(UKEY_F6, INPUTEVENT_KEY_F6, 0, INPUTEVENT_SPC_STATERESTOREDIALOG, ID_FLAG_QUALIFIER_SPECIAL, INPUTEVENT_SPC_STATESAVEDIALOG, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SHIFT),
    K(UKEY_F7, INPUTEVENT_KEY_F7),
    K(UKEY_F8, INPUTEVENT_KEY_F8),
    K2(UKEY_F9, INPUTEVENT_KEY_F9, 0, INPUTEVENT_SPC_TOGGLERTG, ID_FLAG_QUALIFIER_SPECIAL),
    K(UKEY_F10, INPUTEVENT_KEY_F10),

    K(UKEY_1, INPUTEVENT_KEY_1),
    K(UKEY_2, INPUTEVENT_KEY_2),
    K(UKEY_3, INPUTEVENT_KEY_3),
    K(UKEY_4, INPUTEVENT_KEY_4),
    K(UKEY_5, INPUTEVENT_KEY_5),
    K(UKEY_6, INPUTEVENT_KEY_6),
    K(UKEY_7, INPUTEVENT_KEY_7),
    K(UKEY_8, INPUTEVENT_KEY_8),
    K(UKEY_9, INPUTEVENT_KEY_9),
    K(UKEY_0, INPUTEVENT_KEY_0),

    K(UKEY_TAB, INPUTEVENT_KEY_TAB),
    K(UKEY_A, INPUTEVENT_KEY_A),
    K(UKEY_B, INPUTEVENT_KEY_B),
    K(UKEY_C, INPUTEVENT_KEY_C),
    K(UKEY_D, INPUTEVENT_KEY_D),
    K(UKEY_E, INPUTEVENT_KEY_E),
    K(UKEY_F, INPUTEVENT_KEY_F),
    K(UKEY_G, INPUTEVENT_KEY_G),
    K(UKEY_H, INPUTEVENT_KEY_H),
    K(UKEY_I, INPUTEVENT_KEY_I),
    K2(UKEY_J, INPUTEVENT_KEY_J, 0, INPUTEVENT_SPC_SWAPJOYPORTS, ID_FLAG_QUALIFIER_SPECIAL),
    K(UKEY_K, INPUTEVENT_KEY_K),
    K(UKEY_L, INPUTEVENT_KEY_L),
    K(UKEY_M, INPUTEVENT_KEY_M),
    K(UKEY_N, INPUTEVENT_KEY_N),
    K(UKEY_O, INPUTEVENT_KEY_O),
    K(UKEY_P, INPUTEVENT_KEY_P),
    K(UKEY_Q, INPUTEVENT_KEY_Q),
    K(UKEY_R, INPUTEVENT_KEY_R),
    K(UKEY_S, INPUTEVENT_KEY_S),
    K(UKEY_T, INPUTEVENT_KEY_T),
    K(UKEY_U, INPUTEVENT_KEY_U),
    K(UKEY_V, INPUTEVENT_KEY_V),
    K(UKEY_W, INPUTEVENT_KEY_W),
    K(UKEY_X, INPUTEVENT_KEY_X),
    K(UKEY_Y, INPUTEVENT_KEY_Y),
    K(UKEY_Z, INPUTEVENT_KEY_Z),

    KF(UKEY_CAPSLOCK, INPUTEVENT_KEY_CAPS_LOCK, ID_FLAG_TOGGLE),

    K(UKEY_KP_1, INPUTEVENT_KEY_NP_1),
    K(UKEY_KP_2, INPUTEVENT_KEY_NP_2),
    K(UKEY_KP_3, INPUTEVENT_KEY_NP_3),
    K(UKEY_KP_4, INPUTEVENT_KEY_NP_4),
    K(UKEY_KP_5, INPUTEVENT_KEY_NP_5),
    K(UKEY_KP_6, INPUTEVENT_KEY_NP_6),
    K(UKEY_KP_7, INPUTEVENT_KEY_NP_7),
    K(UKEY_KP_8, INPUTEVENT_KEY_NP_8),
    K(UKEY_KP_9, INPUTEVENT_KEY_NP_9),
    K(UKEY_KP_0, INPUTEVENT_KEY_NP_0),
    K(UKEY_KP_PERIOD, INPUTEVENT_KEY_NP_PERIOD),
    K4(UKEY_KP_PLUS, INPUTEVENT_KEY_NP_ADD, 0, INPUTEVENT_SPC_VOLUME_UP, ID_FLAG_QUALIFIER_SPECIAL, INPUTEVENT_SPC_MASTER_VOLUME_UP, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_CONTROL, INPUTEVENT_SPC_INCREASE_REFRESHRATE, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SHIFT),
    K4(UKEY_KP_MINUS, INPUTEVENT_KEY_NP_SUB, 0, INPUTEVENT_SPC_VOLUME_DOWN, ID_FLAG_QUALIFIER_SPECIAL, INPUTEVENT_SPC_MASTER_VOLUME_DOWN, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_CONTROL, INPUTEVENT_SPC_DECREASE_REFRESHRATE, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SHIFT),
    K3(UKEY_KP_MULTIPLY, INPUTEVENT_KEY_NP_MUL, 0, INPUTEVENT_SPC_VOLUME_MUTE, ID_FLAG_QUALIFIER_SPECIAL, INPUTEVENT_SPC_MASTER_VOLUME_MUTE, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_CONTROL),
    K2(UKEY_KP_DIVIDE, INPUTEVENT_KEY_NP_DIV, 0, INPUTEVENT_SPC_STATEREWIND, ID_FLAG_QUALIFIER_SPECIAL),
    K(UKEY_KP_ENTER, INPUTEVENT_KEY_ENTER),

    K(UKEY_MINUS, INPUTEVENT_KEY_SUB),
    K(UKEY_EQUALS, INPUTEVENT_KEY_EQUALS),
    K(UKEY_BACKSPACE, INPUTEVENT_KEY_BACKSPACE),
    K(UKEY_RETURN, INPUTEVENT_KEY_RETURN),
    K(UKEY_SPACE, INPUTEVENT_KEY_SPACE),

    K2(UKEY_LSHIFT, INPUTEVENT_KEY_SHIFT_LEFT, 0, INPUTEVENT_SPC_QUALIFIER_SHIFT, 0),
    K2(UKEY_LCTRL, INPUTEVENT_KEY_CTRL, 0, INPUTEVENT_SPC_QUALIFIER_CONTROL, 0),
    K2(UKEY_LGUI, INPUTEVENT_KEY_AMIGA_LEFT, 0, INPUTEVENT_SPC_QUALIFIER_WIN, 0),
    K2(UKEY_LALT, INPUTEVENT_KEY_ALT_LEFT, 0, INPUTEVENT_SPC_QUALIFIER_ALT, 0),
    K2(UKEY_RALT, INPUTEVENT_KEY_ALT_RIGHT, 0, INPUTEVENT_SPC_QUALIFIER_ALT, 0),
    K2(UKEY_RGUI, INPUTEVENT_KEY_AMIGA_RIGHT, 0, INPUTEVENT_SPC_QUALIFIER_WIN, 0),
    K2(UKEY_APPLICATION, INPUTEVENT_KEY_AMIGA_RIGHT, 0, INPUTEVENT_SPC_QUALIFIER_WIN, 0),
    K2(UKEY_RCTRL, INPUTEVENT_KEY_CTRL, 0, INPUTEVENT_SPC_QUALIFIER_CONTROL, 0),
    K2(UKEY_RSHIFT, INPUTEVENT_KEY_SHIFT_RIGHT, 0, INPUTEVENT_SPC_QUALIFIER_SHIFT, 0),

    K(UKEY_UP, INPUTEVENT_KEY_CURSOR_UP),
    K(UKEY_DOWN, INPUTEVENT_KEY_CURSOR_DOWN),
    K2(UKEY_LEFT, INPUTEVENT_KEY_CURSOR_LEFT, 0, INPUTEVENT_SPC_PAUSE, ID_FLAG_QUALIFIER_SPECIAL),
    K2(UKEY_RIGHT, INPUTEVENT_KEY_CURSOR_RIGHT, 0, INPUTEVENT_SPC_WARP, ID_FLAG_QUALIFIER_SPECIAL),

    K2(UKEY_INSERT, INPUTEVENT_KEY_AMIGA_LEFT, 0, INPUTEVENT_SPC_PASTE, ID_FLAG_QUALIFIER_SPECIAL),
    K(UKEY_DELETE, INPUTEVENT_KEY_DEL),
    K(UKEY_HOME, INPUTEVENT_KEY_AMIGA_RIGHT),
    K(UKEY_PAGEDOWN, INPUTEVENT_KEY_HELP),
    K(UKEY_PAGEUP, INPUTEVENT_SPC_FREEZEBUTTON),

    K(UKEY_LEFTBRACKET, INPUTEVENT_KEY_LEFTBRACKET),
    K(UKEY_RIGHTBRACKET, INPUTEVENT_KEY_RIGHTBRACKET),
    K(UKEY_SEMICOLON, INPUTEVENT_KEY_SEMICOLON),
    K(UKEY_APOSTROPHE, INPUTEVENT_KEY_SINGLEQUOTE),
    K(UKEY_GRAVE, INPUTEVENT_KEY_BACKQUOTE),
    K(UKEY_BACKSLASH, INPUTEVENT_KEY_NUMBERSIGN),
    K(UKEY_NONUSHASH, INPUTEVENT_KEY_BACKSLASH),
    K(UKEY_NONUSBACKSLASH, INPUTEVENT_KEY_30),
    K(UKEY_COMMA, INPUTEVENT_KEY_COMMA),
    K(UKEY_PERIOD, INPUTEVENT_KEY_PERIOD),
    K(UKEY_SLASH, INPUTEVENT_KEY_DIV),

    K(UKEY_F11, INPUTEVENT_KEY_BACKSLASH),
    K(UKEY_F13, INPUTEVENT_KEY_BACKSLASH),
    K(UKEY_F14, INPUTEVENT_KEY_NP_LPAREN),
    K(UKEY_F15, INPUTEVENT_KEY_NP_RPAREN),
    K2(UKEY_PRINTSCREEN, INPUTEVENT_SPC_SCREENSHOT_CLIPBOARD, 0, INPUTEVENT_SPC_SCREENSHOT, ID_FLAG_QUALIFIER_SPECIAL),
    K(UKEY_END, INPUTEVENT_SPC_QUALIFIER_SPECIAL),
    K4(UKEY_PAUSE, INPUTEVENT_SPC_PAUSE, 0, INPUTEVENT_SPC_SINGLESTEP, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_CONTROL, INPUTEVENT_SPC_IRQ7, ID_FLAG_QUALIFIER_SPECIAL | ID_FLAG_QUALIFIER_SHIFT, INPUTEVENT_SPC_WARP, ID_FLAG_QUALIFIER_SPECIAL),
    K4(UKEY_F12, INPUTEVENT_SPC_ENTERGUI, 0, INPUTEVENT_SPC_ENTERDEBUGGER, ID_FLAG_QUALIFIER_SPECIAL, INPUTEVENT_SPC_ENTERDEBUGGER, ID_FLAG_QUALIFIER_SHIFT, INPUTEVENT_SPC_TOGGLEDEFAULTSCREEN, ID_FLAG_QUALIFIER_CONTROL),

    K(UKEY_AUDIOSTOP, INPUTEVENT_KEY_CDTV_STOP),
    K(UKEY_AUDIOPLAY, INPUTEVENT_KEY_CDTV_PLAYPAUSE),
    K(UKEY_AUDIOPREV, INPUTEVENT_KEY_CDTV_PREV),
    K(UKEY_AUDIONEXT, INPUTEVENT_KEY_CDTV_NEXT),

    { -1, { { 0, 0 } } }
};

static uae_input_device_kbr_default keytrans_pc[] = {
    K(UKEY_ESCAPE, INPUTEVENT_KEY_ESC),

    K(UKEY_F1, INPUTEVENT_KEY_F1),
    K(UKEY_F2, INPUTEVENT_KEY_F2),
    K(UKEY_F3, INPUTEVENT_KEY_F3),
    K(UKEY_F4, INPUTEVENT_KEY_F4),
    K(UKEY_F5, INPUTEVENT_KEY_F5),
    K(UKEY_F6, INPUTEVENT_KEY_F6),
    K(UKEY_F7, INPUTEVENT_KEY_F7),
    K(UKEY_F8, INPUTEVENT_KEY_F8),
    K(UKEY_F9, INPUTEVENT_KEY_F9),
    K(UKEY_F10, INPUTEVENT_KEY_F10),
    K(UKEY_F11, INPUTEVENT_KEY_F11),
    K(UKEY_F12, INPUTEVENT_KEY_F12),

    K(UKEY_1, INPUTEVENT_KEY_1),
    K(UKEY_2, INPUTEVENT_KEY_2),
    K(UKEY_3, INPUTEVENT_KEY_3),
    K(UKEY_4, INPUTEVENT_KEY_4),
    K(UKEY_5, INPUTEVENT_KEY_5),
    K(UKEY_6, INPUTEVENT_KEY_6),
    K(UKEY_7, INPUTEVENT_KEY_7),
    K(UKEY_8, INPUTEVENT_KEY_8),
    K(UKEY_9, INPUTEVENT_KEY_9),
    K(UKEY_0, INPUTEVENT_KEY_0),

    K(UKEY_TAB, INPUTEVENT_KEY_TAB),
    K(UKEY_A, INPUTEVENT_KEY_A),
    K(UKEY_B, INPUTEVENT_KEY_B),
    K(UKEY_C, INPUTEVENT_KEY_C),
    K(UKEY_D, INPUTEVENT_KEY_D),
    K(UKEY_E, INPUTEVENT_KEY_E),
    K(UKEY_F, INPUTEVENT_KEY_F),
    K(UKEY_G, INPUTEVENT_KEY_G),
    K(UKEY_H, INPUTEVENT_KEY_H),
    K(UKEY_I, INPUTEVENT_KEY_I),
    K(UKEY_J, INPUTEVENT_KEY_J),
    K(UKEY_K, INPUTEVENT_KEY_K),
    K(UKEY_L, INPUTEVENT_KEY_L),
    K(UKEY_M, INPUTEVENT_KEY_M),
    K(UKEY_N, INPUTEVENT_KEY_N),
    K(UKEY_O, INPUTEVENT_KEY_O),
    K(UKEY_P, INPUTEVENT_KEY_P),
    K(UKEY_Q, INPUTEVENT_KEY_Q),
    K(UKEY_R, INPUTEVENT_KEY_R),
    K(UKEY_S, INPUTEVENT_KEY_S),
    K(UKEY_T, INPUTEVENT_KEY_T),
    K(UKEY_U, INPUTEVENT_KEY_U),
    K(UKEY_V, INPUTEVENT_KEY_V),
    K(UKEY_W, INPUTEVENT_KEY_W),
    K(UKEY_X, INPUTEVENT_KEY_X),
    K(UKEY_Y, INPUTEVENT_KEY_Y),
    K(UKEY_Z, INPUTEVENT_KEY_Z),

    KF(UKEY_CAPSLOCK, INPUTEVENT_KEY_CAPS_LOCK, ID_FLAG_TOGGLE),

    K(UKEY_KP_1, INPUTEVENT_KEY_NP_1),
    K(UKEY_KP_2, INPUTEVENT_KEY_NP_2),
    K(UKEY_KP_3, INPUTEVENT_KEY_NP_3),
    K(UKEY_KP_4, INPUTEVENT_KEY_NP_4),
    K(UKEY_KP_5, INPUTEVENT_KEY_NP_5),
    K(UKEY_KP_6, INPUTEVENT_KEY_NP_6),
    K(UKEY_KP_7, INPUTEVENT_KEY_NP_7),
    K(UKEY_KP_8, INPUTEVENT_KEY_NP_8),
    K(UKEY_KP_9, INPUTEVENT_KEY_NP_9),
    K(UKEY_KP_0, INPUTEVENT_KEY_NP_0),
    K(UKEY_KP_PERIOD, INPUTEVENT_KEY_NP_PERIOD),
    K(UKEY_KP_PLUS, INPUTEVENT_KEY_NP_ADD),
    K(UKEY_KP_MINUS, INPUTEVENT_KEY_NP_SUB),
    K(UKEY_KP_MULTIPLY, INPUTEVENT_KEY_NP_MUL),
    K(UKEY_KP_DIVIDE, INPUTEVENT_KEY_NP_DIV),
    K(UKEY_KP_ENTER, INPUTEVENT_KEY_ENTER),

    K(UKEY_MINUS, INPUTEVENT_KEY_SUB),
    K(UKEY_EQUALS, INPUTEVENT_KEY_EQUALS),
    K(UKEY_BACKSPACE, INPUTEVENT_KEY_BACKSPACE),
    K(UKEY_RETURN, INPUTEVENT_KEY_RETURN),
    K(UKEY_SPACE, INPUTEVENT_KEY_SPACE),

    K(UKEY_LSHIFT, INPUTEVENT_KEY_SHIFT_LEFT),
    K(UKEY_LCTRL, INPUTEVENT_KEY_CTRL),
    K(UKEY_LGUI, INPUTEVENT_KEY_AMIGA_LEFT),
    K(UKEY_LALT, INPUTEVENT_KEY_ALT_LEFT),
    K(UKEY_RALT, INPUTEVENT_KEY_ALT_RIGHT),
    K(UKEY_RGUI, INPUTEVENT_KEY_AMIGA_RIGHT),
    K(UKEY_APPLICATION, INPUTEVENT_KEY_APPS),
    K(UKEY_RCTRL, INPUTEVENT_KEY_CTRL),
    K(UKEY_RSHIFT, INPUTEVENT_KEY_SHIFT_RIGHT),

    K(UKEY_UP, INPUTEVENT_KEY_CURSOR_UP),
    K(UKEY_DOWN, INPUTEVENT_KEY_CURSOR_DOWN),
    K(UKEY_LEFT, INPUTEVENT_KEY_CURSOR_LEFT),
    K(UKEY_RIGHT, INPUTEVENT_KEY_CURSOR_RIGHT),

    K(UKEY_LEFTBRACKET, INPUTEVENT_KEY_LEFTBRACKET),
    K(UKEY_RIGHTBRACKET, INPUTEVENT_KEY_RIGHTBRACKET),
    K(UKEY_SEMICOLON, INPUTEVENT_KEY_SEMICOLON),
    K(UKEY_APOSTROPHE, INPUTEVENT_KEY_SINGLEQUOTE),
    K(UKEY_GRAVE, INPUTEVENT_KEY_BACKQUOTE),
    K(UKEY_BACKSLASH, INPUTEVENT_KEY_2B),
    K(UKEY_NONUSHASH, INPUTEVENT_KEY_BACKSLASH),
    K(UKEY_NONUSBACKSLASH, INPUTEVENT_KEY_30),
    K(UKEY_COMMA, INPUTEVENT_KEY_COMMA),
    K(UKEY_PERIOD, INPUTEVENT_KEY_PERIOD),
    K(UKEY_SLASH, INPUTEVENT_KEY_DIV),

    K(UKEY_INSERT, INPUTEVENT_KEY_INSERT),
    K(UKEY_DELETE, INPUTEVENT_KEY_DEL),
    K(UKEY_HOME, INPUTEVENT_KEY_HOME),
    K(UKEY_END, INPUTEVENT_KEY_END),
    K(UKEY_PAGEUP, INPUTEVENT_KEY_PAGEUP),
    K(UKEY_PAGEDOWN, INPUTEVENT_KEY_PAGEDOWN),
    K(UKEY_SCROLLLOCK, INPUTEVENT_KEY_HELP),
    K(UKEY_PRINTSCREEN, INPUTEVENT_KEY_SYSRQ),
    K(UKEY_PAUSE, INPUTEVENT_KEY_PAUSE),

    K(UKEY_AUDIOSTOP, INPUTEVENT_KEY_CDTV_STOP),
    K(UKEY_AUDIOPLAY, INPUTEVENT_KEY_CDTV_PLAYPAUSE),
    K(UKEY_AUDIOPREV, INPUTEVENT_KEY_CDTV_PREV),
    K(UKEY_AUDIONEXT, INPUTEVENT_KEY_CDTV_NEXT),

    { -1, { { 0, 0 } } }
};

#undef K4
#undef K3
#undef K2
#undef KF
#undef K

static uae_input_device_kbr_default *keytrans[] = {
    keytrans_amiga,
    keytrans_pc,
    keytrans_pc
};
static int kb_np[] = { UKEY_KP_4, -1, UKEY_KP_6, -1, UKEY_KP_8, -1, UKEY_KP_2, -1, UKEY_KP_0, UKEY_KP_5, -1, UKEY_KP_PERIOD, -1, UKEY_KP_ENTER, -1, -1 };
static int kb_ck[] = { UKEY_LEFT, -1, UKEY_RIGHT, -1, UKEY_UP, -1, UKEY_DOWN, -1, UKEY_RCTRL, UKEY_RALT, -1, UKEY_RSHIFT, -1, -1 };
static int kb_se[] = { UKEY_A, -1, UKEY_D, -1, UKEY_W, -1, UKEY_S, -1, UKEY_LALT, -1, UKEY_LSHIFT, -1, -1 };
static int kb_np3[] = { UKEY_KP_4, -1, UKEY_KP_6, -1, UKEY_KP_8, -1, UKEY_KP_2, -1, UKEY_KP_0, UKEY_KP_5, -1, UKEY_KP_PERIOD, -1, UKEY_KP_ENTER, -1, -1 };
static int kb_ck3[] = { UKEY_LEFT, -1, UKEY_RIGHT, -1, UKEY_UP, -1, UKEY_DOWN, -1, UKEY_RCTRL, -1, UKEY_RSHIFT, -1, UKEY_RALT, -1, -1 };
static int kb_se3[] = { UKEY_A, -1, UKEY_D, -1, UKEY_W, -1, UKEY_S, -1, UKEY_LALT, -1, UKEY_LSHIFT, -1, UKEY_LCTRL, -1, -1 };
static int kb_cd32_np[] = { UKEY_KP_4, -1, UKEY_KP_6, -1, UKEY_KP_8, -1, UKEY_KP_2, -1, UKEY_KP_0, UKEY_KP_5, UKEY_KP_1, -1, UKEY_KP_PERIOD, UKEY_KP_3, -1, UKEY_KP_7, -1, UKEY_KP_9, -1, UKEY_KP_DIVIDE, -1, UKEY_KP_MINUS, -1, UKEY_KP_MULTIPLY, -1, -1 };
static int kb_cd32_ck[] = { UKEY_LEFT, -1, UKEY_RIGHT, -1, UKEY_UP, -1, UKEY_DOWN, -1, UKEY_RCTRL, UKEY_RALT, UKEY_RSHIFT, -1, UKEY_KP_7, -1, UKEY_KP_9, -1, UKEY_KP_DIVIDE, -1, UKEY_KP_MINUS, -1, UKEY_KP_MULTIPLY, -1, -1 };
static int kb_cd32_se[] = { UKEY_A, -1, UKEY_D, -1, UKEY_W, -1, UKEY_S, -1, -1, UKEY_LALT, -1, UKEY_LSHIFT, -1, UKEY_KP_7, -1, UKEY_KP_9, -1, UKEY_KP_DIVIDE, -1, UKEY_KP_MINUS, -1, UKEY_KP_MULTIPLY, -1, -1 };
static int kb_arcadia[] = { UKEY_F2, -1, UKEY_1, -1, UKEY_2, -1, UKEY_5, -1, UKEY_6, -1, -1 };
static int kb_cdtv[] = { UKEY_KP_1, -1, UKEY_KP_3, -1, UKEY_KP_7, -1, UKEY_KP_9, -1, -1 };
static int *keymaps[] = {
    kb_np, kb_ck, kb_se, kb_np3, kb_ck3, kb_se3,
    kb_cd32_np, kb_cd32_ck, kb_cd32_se,
    kb_arcadia, kb_cdtv
};

static const int keyboard_keycodes[] = {
    UKEY_ESCAPE,
    UKEY_F1, UKEY_F2, UKEY_F3, UKEY_F4, UKEY_F5, UKEY_F6, UKEY_F7, UKEY_F8, UKEY_F9, UKEY_F10, UKEY_F11, UKEY_F12,
    UKEY_1, UKEY_2, UKEY_3, UKEY_4, UKEY_5, UKEY_6, UKEY_7, UKEY_8, UKEY_9, UKEY_0,
    UKEY_TAB,
    UKEY_A, UKEY_B, UKEY_C, UKEY_D, UKEY_E, UKEY_F, UKEY_G, UKEY_H, UKEY_I, UKEY_J, UKEY_K, UKEY_L, UKEY_M,
    UKEY_N, UKEY_O, UKEY_P, UKEY_Q, UKEY_R, UKEY_S, UKEY_T, UKEY_U, UKEY_V, UKEY_W, UKEY_X, UKEY_Y, UKEY_Z,
    UKEY_CAPSLOCK,
    UKEY_KP_1, UKEY_KP_2, UKEY_KP_3, UKEY_KP_4, UKEY_KP_5, UKEY_KP_6, UKEY_KP_7, UKEY_KP_8, UKEY_KP_9, UKEY_KP_0,
    UKEY_KP_PERIOD, UKEY_KP_PLUS, UKEY_KP_MINUS, UKEY_KP_MULTIPLY, UKEY_KP_DIVIDE, UKEY_KP_ENTER,
    UKEY_MINUS, UKEY_EQUALS, UKEY_BACKSPACE, UKEY_RETURN, UKEY_SPACE,
    UKEY_LSHIFT, UKEY_LCTRL, UKEY_LGUI, UKEY_LALT, UKEY_RALT, UKEY_RGUI, UKEY_APPLICATION, UKEY_RCTRL, UKEY_RSHIFT,
    UKEY_UP, UKEY_DOWN, UKEY_LEFT, UKEY_RIGHT,
    UKEY_INSERT, UKEY_DELETE, UKEY_HOME, UKEY_END, UKEY_PAGEUP, UKEY_PAGEDOWN, UKEY_SCROLLLOCK, UKEY_PRINTSCREEN, UKEY_PAUSE,
    UKEY_LEFTBRACKET, UKEY_RIGHTBRACKET, UKEY_SEMICOLON, UKEY_APOSTROPHE, UKEY_GRAVE, UKEY_BACKSLASH, UKEY_NONUSHASH, UKEY_NONUSBACKSLASH,
    UKEY_COMMA, UKEY_PERIOD, UKEY_SLASH,
    UKEY_F13, UKEY_F14, UKEY_F15,
    UKEY_AUDIOSTOP, UKEY_AUDIOPLAY, UKEY_AUDIOPREV, UKEY_AUDIONEXT
};

static int input_init(void)
{
    inputdevice_setkeytranslation(keytrans, keymaps);
    return 1;
}
static void input_close(void) {}
static int input_acquire(int, int) { return 1; }
static void input_unacquire(int) {}
static void input_read(void) {}
static int empty_get_num(void) { return 0; }
static TCHAR *empty_get_friendlyname(int) { return empty_friendly; }
static TCHAR *empty_get_uniquename(int) { return empty_unique_name; }
static int empty_get_widget_num(int) { return 0; }
static int empty_get_widget_type(int, int, TCHAR *, uae_u32 *) { return IDEV_WIDGET_NONE; }
static int empty_get_widget_first(int, int) { return -1; }
static int empty_get_flags(int) { return 0; }

static int mouse_get_num(void) { return 1; }
static TCHAR *mouse_get_friendlyname(int) { return mouse_friendly; }
static TCHAR *mouse_get_uniquename(int) { return mouse_unique_name; }
static int mouse_get_widget_num(int) { return 6; }
static int mouse_get_widget_type(int, int widget, TCHAR *name, uae_u32 *code)
{
    if (code) {
        *code = widget;
    }
    if (widget >= 0 && widget < 3) {
        if (name) {
            _tcscpy(name, mouse_axis_names[widget]);
        }
        return IDEV_WIDGET_AXIS;
    }
    if (widget >= 3 && widget < 6) {
        if (name) {
            _tcscpy(name, mouse_button_names[widget - 3]);
        }
        return IDEV_WIDGET_BUTTON;
    }
    return IDEV_WIDGET_NONE;
}
static int mouse_get_widget_first(int, int type)
{
    if (type == IDEV_WIDGET_AXIS) {
        return 0;
    }
    if (type == IDEV_WIDGET_BUTTON) {
        return 3;
    }
    return -1;
}
static int mouse_get_flags(int) { return 0; }

static int keyboard_get_num(void) { return 1; }
static TCHAR *keyboard_get_friendlyname(int) { return keyboard_friendly; }
static TCHAR *keyboard_get_uniquename(int) { return keyboard_unique_name; }
static int keyboard_get_widget_num(int) { return sizeof keyboard_keycodes / sizeof keyboard_keycodes[0]; }
static int keyboard_get_widget_type(int, int widget, TCHAR *name, uae_u32 *code)
{
    if (widget < 0 || widget >= keyboard_get_widget_num(0)) {
        return IDEV_WIDGET_NONE;
    }
    int scancode = keyboard_keycodes[widget];
    if (name) {
        _sntprintf(name, 64, _T("Key %d"), scancode);
        name[63] = 0;
    }
    if (code) {
        *code = scancode;
    }
    return IDEV_WIDGET_KEY;
}
static int keyboard_get_widget_first(int, int type)
{
    return type == IDEV_WIDGET_KEY ? 0 : -1;
}
static int keyboard_get_flags(int) { return 0; }

inputdevice_functions inputdevicefunc_joystick = {
    input_init, input_close, input_acquire, input_unacquire, input_read,
    empty_get_num, empty_get_friendlyname, empty_get_uniquename,
    empty_get_widget_num, empty_get_widget_type, empty_get_widget_first,
    empty_get_flags
};

inputdevice_functions inputdevicefunc_mouse = {
    input_init, input_close, input_acquire, input_unacquire, input_read,
    mouse_get_num, mouse_get_friendlyname, mouse_get_uniquename,
    mouse_get_widget_num, mouse_get_widget_type, mouse_get_widget_first,
    mouse_get_flags
};

inputdevice_functions inputdevicefunc_keyboard = {
    input_init, input_close, input_acquire, input_unacquire, input_read,
    keyboard_get_num, keyboard_get_friendlyname, keyboard_get_uniquename,
    keyboard_get_widget_num, keyboard_get_widget_type, keyboard_get_widget_first,
    keyboard_get_flags
};

static int nextsub(struct uae_input_device *uid, int dev, int slot, int sub)
{
    if (currprefs.input_advancedmultiinput) {
        while (uid[dev].eventid[slot][sub] > 0) {
            sub++;
            if (sub >= MAX_INPUT_SUB_EVENT) {
                return -1;
            }
        }
    }
    return sub;
}

static void setid(struct uae_input_device *uid, int dev, int slot, int sub, int port, int evt, bool gp)
{
    sub = nextsub(uid, dev, slot, sub);
    if (sub < 0 || evt <= 0) {
        return;
    }
    if (gp && sub == 0) {
        inputdevice_sparecopy(&uid[dev], slot, sub);
    }
    uid[dev].eventid[slot][sub] = evt;
    uid[dev].port[slot][sub] = port + 1;
}

static void setid(struct uae_input_device *uid, int dev, int slot, int sub, int port, int evt, int af, bool gp)
{
    sub = nextsub(uid, dev, slot, sub);
    if (sub < 0) {
        return;
    }
    setid(uid, dev, slot, sub, port, evt, gp);
    uid[dev].flags[slot][sub] &= ~ID_FLAG_AUTOFIRE_MASK;
    if (af >= JPORT_AF_NORMAL) {
        uid[dev].flags[slot][sub] |= ID_FLAG_AUTOFIRE;
    }
    if (af == JPORT_AF_TOGGLE) {
        uid[dev].flags[slot][sub] |= ID_FLAG_TOGGLE;
    }
    if (af == JPORT_AF_ALWAYS) {
        uid[dev].flags[slot][sub] |= ID_FLAG_INVERTTOGGLE;
    }
    if (af == JPORT_AF_TOGGLENOAF) {
        uid[dev].flags[slot][sub] |= ID_FLAG_INVERT;
    }
}

void unix_input_mouse_motion(int dx, int dy)
{
    if (dx) {
        setmousestate(0, 0, dx, 0);
    }
    if (dy) {
        setmousestate(0, 1, dy, 0);
    }
}

void unix_input_mouse_button(int button, bool pressed)
{
    if (button >= 0 && button < 3) {
        setmousebuttonstate(0, button, pressed ? 1 : 0);
    }
}

void unix_input_mouse_wheel(int, int y)
{
    if (y) {
        setmousestate(0, 2, y * 120, 0);
    }
}

void unix_input_set_mouse_active(bool active)
{
    mouse_active = active;
}

bool unix_input_get_mouse_active(void)
{
    return mouse_active;
}

void unix_input_keyboard_key(int scancode, bool pressed)
{
    if (scancode <= 0 || scancode >= (int)(sizeof keyboard_state / sizeof keyboard_state[0])) {
        return;
    }
    if (keyboard_state[scancode] == pressed) {
        return;
    }

    keyboard_state[scancode] = pressed;
    inputdevice_translatekeycode(0, scancode, pressed ? 1 : 0, false);
}

void unix_input_release_keys(void)
{
    for (int scancode = 0; scancode < (int)(sizeof keyboard_state / sizeof keyboard_state[0]); scancode++) {
        if (keyboard_state[scancode]) {
            keyboard_state[scancode] = false;
            inputdevice_translatekeycode(0, scancode, 0, true);
        }
    }
    setmousebuttonstateall(0, 0, 7);
}

void release_keys(void) { unix_input_release_keys(); }
int input_get_default_keyboard(int num)
{
    if (num < 0) {
        return 0;
    }
    return num == 0 ? 1 : 0;
}
int input_get_default_mouse(uae_input_device *uid, int dev, int port, int af, bool gp, bool wheel, bool joymouseswap)
{
    if (joymouseswap || dev != 0) {
        return 0;
    }

    setid(uid, dev, ID_AXIS_OFFSET + 0, 0, port, port ? INPUTEVENT_MOUSE2_HORIZ : INPUTEVENT_MOUSE1_HORIZ, gp);
    setid(uid, dev, ID_AXIS_OFFSET + 1, 0, port, port ? INPUTEVENT_MOUSE2_VERT : INPUTEVENT_MOUSE1_VERT, gp);
    if (wheel && port == 0) {
        setid(uid, dev, ID_AXIS_OFFSET + 2, 0, port, INPUTEVENT_MOUSE1_WHEEL, gp);
    }
    setid(uid, dev, ID_BUTTON_OFFSET + 0, 0, port, port ? INPUTEVENT_JOY2_FIRE_BUTTON : INPUTEVENT_JOY1_FIRE_BUTTON, af, gp);
    setid(uid, dev, ID_BUTTON_OFFSET + 1, 0, port, port ? INPUTEVENT_JOY2_2ND_BUTTON : INPUTEVENT_JOY1_2ND_BUTTON, gp);
    setid(uid, dev, ID_BUTTON_OFFSET + 2, 0, port, port ? INPUTEVENT_JOY2_3RD_BUTTON : INPUTEVENT_JOY1_3RD_BUTTON, gp);

    return 1;
}
int input_get_default_lightpen(uae_input_device *, int, int, int, bool, bool, int) { return 0; }
int input_get_default_joystick(uae_input_device *, int, int, int, int, bool, bool, bool) { return 0; }
int input_get_default_joystick_analog(uae_input_device *, int, int, int, bool, bool, bool) { return 0; }
int is_tablet(void) { return 0; }
bool ismouseactive(void) { return unix_input_get_mouse_active(); }
void setmouseactive(int, int active) { unix_input_set_mouse_active(active != 0); }
bool target_can_autoswitchdevice(void) { return false; }
void target_inputdevice_acquire(void) {}
void target_inputdevice_unacquire(bool) {}
int getcapslockstate(void) { return 0; }
