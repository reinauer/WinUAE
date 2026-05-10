#include "sysconfig.h"
#include "sysdeps.h"

#include <stdarg.h>
#include "options.h"
#include "gui.h"
#include "gui_unix.h"
#include "startup_config.h"

#ifdef WINUAE_UNIX_WITH_INTEGRATED_QT_UI
#include "qt/launcher_bridge.h"
#endif

unsigned int gui_ledstate;

static int unix_gui_argc;
static TCHAR **unix_gui_argv;

void unix_gui_set_main_args(int argc, TCHAR **argv)
{
    unix_gui_argc = argc;
    unix_gui_argv = argv;
}

int gui_init(void)
{
#ifdef WINUAE_UNIX_WITH_INTEGRATED_QT_UI
    const int action = runWinUaeQtLauncherForStartupConfig(unix_gui_argc, unix_gui_argv, 0);
    if (action == WINUAE_QT_LAUNCHER_START) {
        unix_startup_config_apply(&changed_prefs);
        return 1;
    }
    if (action == WINUAE_QT_LAUNCHER_ERROR) {
        return -1;
    }
    return -2;
#else
    return 0;
#endif
}
int gui_update(void) { return 0; }
void gui_exit(void) {}
void gui_led(int, int, int) {}
void gui_filename(int, const TCHAR*) {}
void gui_fps(int, int, bool, int, int) {}
void gui_lock(void) {}
void gui_unlock(void) {}
void gui_flicker_led(int, int, int) {}
void gui_disk_image_change(int, const TCHAR*, bool) {}
void gui_display(int) {}
void gui_gameport_button_change(int, int, int) {}
void gui_gameport_axis_change(int, int, int, int) {}
void notify_user(int msg) { write_log("notify_user: %d\n", msg); }
void notify_user_parms(int msg, const TCHAR*, ...) { write_log("notify_user: %d\n", msg); }
int translate_message(int, TCHAR *out) { if (out) out[0] = 0; return 0; }
void gui_message(const TCHAR *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fputc('\n', stderr);
    va_end(ap);
}
