#include "sysconfig.h"
#include "sysdeps.h"

#include <stdarg.h>
#include "gui.h"
#include "gui_unix.h"

#ifdef WINUAE_UNIX_WITH_INTEGRATED_QT_UI
#include "qt/launcher_bridge.h"
#endif

unsigned int gui_ledstate;

static bool is_qt_ui_option(const TCHAR *arg)
{
    return _tcscmp(arg, _T("--qt-ui")) == 0 || _tcscmp(arg, _T("-qt-ui")) == 0;
}

int unix_gui_handle_early_options(int argc, TCHAR **argv, int *exit_code)
{
    for (int i = 1; i < argc; i++) {
        if (!is_qt_ui_option(argv[i])) {
            continue;
        }

#ifdef WINUAE_UNIX_WITH_INTEGRATED_QT_UI
        const int action = runWinUaeQtLauncherForStartupConfig(argc, argv, exit_code);
        if (action == WINUAE_QT_LAUNCHER_START) {
            return UNIX_GUI_EARLY_START;
        }
        if (action == WINUAE_QT_LAUNCHER_EXIT) {
            return UNIX_GUI_EARLY_EXIT;
        }
        return UNIX_GUI_EARLY_EXIT;
#else
        write_log("Unix Qt UI was not enabled in this build.\n");
        if (exit_code) {
            *exit_code = 1;
        }
        return UNIX_GUI_EARLY_EXIT;
#endif
    }
    return UNIX_GUI_EARLY_NONE;
}

int gui_init(void) { return 0; }
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
