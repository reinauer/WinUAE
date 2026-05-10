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
static bool mouse_active;

static uae_input_device_kbr_default empty_keytrans[] = {
    { -1, { { 0, 0 } } }
};
static uae_input_device_kbr_default *keytrans[] = {
    empty_keytrans,
    empty_keytrans,
    empty_keytrans
};
static int empty_keymap[] = { -1 };
static int *keymaps[] = {
    empty_keymap, empty_keymap, empty_keymap,
    empty_keymap, empty_keymap, empty_keymap,
    empty_keymap, empty_keymap, empty_keymap,
    empty_keymap, empty_keymap
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
    empty_get_num, empty_get_friendlyname, empty_get_uniquename,
    empty_get_widget_num, empty_get_widget_type, empty_get_widget_first,
    empty_get_flags
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

void release_keys(void) {}
int input_get_default_keyboard(int) { return 0; }
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
