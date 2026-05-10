#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "traps.h"
#include "inputdevice.h"

int pause_emulation;
int tablet_log;
int key_swap_hack;

static TCHAR friendly[] = _T("Unix placeholder");
static TCHAR unique_name[] = _T("unix.placeholder");
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
static int input_get_num(void) { return 0; }
static TCHAR *input_get_friendlyname(int) { return friendly; }
static TCHAR *input_get_uniquename(int) { return unique_name; }
static int input_get_widget_num(int) { return 0; }
static int input_get_widget_type(int, int, TCHAR *, uae_u32 *) { return IDEV_WIDGET_NONE; }
static int input_get_widget_first(int, int) { return -1; }
static int input_get_flags(int) { return 0; }

inputdevice_functions inputdevicefunc_joystick = {
    input_init, input_close, input_acquire, input_unacquire, input_read,
    input_get_num, input_get_friendlyname, input_get_uniquename,
    input_get_widget_num, input_get_widget_type, input_get_widget_first,
    input_get_flags
};

inputdevice_functions inputdevicefunc_mouse = {
    input_init, input_close, input_acquire, input_unacquire, input_read,
    input_get_num, input_get_friendlyname, input_get_uniquename,
    input_get_widget_num, input_get_widget_type, input_get_widget_first,
    input_get_flags
};

inputdevice_functions inputdevicefunc_keyboard = {
    input_init, input_close, input_acquire, input_unacquire, input_read,
    input_get_num, input_get_friendlyname, input_get_uniquename,
    input_get_widget_num, input_get_widget_type, input_get_widget_first,
    input_get_flags
};

void release_keys(void) {}
int input_get_default_keyboard(int) { return 0; }
int input_get_default_mouse(uae_input_device *, int, int, int, bool, bool, bool) { return 0; }
int input_get_default_lightpen(uae_input_device *, int, int, int, bool, bool, int) { return 0; }
int input_get_default_joystick(uae_input_device *, int, int, int, int, bool, bool, bool) { return 0; }
int input_get_default_joystick_analog(uae_input_device *, int, int, int, bool, bool, bool) { return 0; }
int is_tablet(void) { return 0; }
bool ismouseactive(void) { return false; }
void setmouseactive(int, int) {}
bool target_can_autoswitchdevice(void) { return false; }
void target_inputdevice_acquire(void) {}
void target_inputdevice_unacquire(bool) {}
int getcapslockstate(void) { return 0; }
