#include "sysconfig.h"
#include "sysdeps.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "options.h"
#include "traps.h"
#include "custom.h"
#include "inputdevice.h"
#include "gui.h"
#include "gui_unix.h"
#include "sounddep/sound.h"

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
    const int action = runWinUaeQtLauncherForPrefs(unix_gui_argc, unix_gui_argv, &changed_prefs, 0);
    if (action == WINUAE_QT_LAUNCHER_START) {
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

#ifdef WINUAE_UNIX_WITH_INTEGRATED_QT_UI
static bool write_runtime_config_snapshot(TCHAR *path, size_t path_len)
{
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !tmpdir[0]) {
        tmpdir = "/tmp";
    }
    const int written = snprintf(path, path_len, "%s/winuae-runtime-%ld.uae", tmpdir, (long)getpid());
    if (written < 0 || size_t(written) >= path_len) {
        write_log("Unix Qt runtime UI: temporary config path is too long\n");
        return false;
    }
    if (!cfgfile_save(&changed_prefs, path, 0)) {
        write_log("Unix Qt runtime UI: failed to write temporary config '%s'\n", path);
        return false;
    }
    return true;
}
#endif

void gui_display(int shortcut)
{
#ifdef WINUAE_UNIX_WITH_INTEGRATED_QT_UI
    static bool active;
    if (active) {
        return;
    }
    if (shortcut != -1) {
        write_log("Unix Qt runtime UI: shortcut %d is not implemented yet\n", shortcut);
        return;
    }

    active = true;

    TCHAR snapshot_path[MAX_DPATH];
    snapshot_path[0] = 0;
    const bool have_snapshot = write_runtime_config_snapshot(snapshot_path, sizeof snapshot_path / sizeof snapshot_path[0]);

    const int old_pause = pause_emulation;
    pause_emulation = 1;
    setsystime();
    inputdevice_unacquire();
    pause_sound();

    int exit_code = 0;
    const int action = runWinUaeQtLauncherForPrefsWithConfig(
        unix_gui_argc,
        unix_gui_argv,
        &changed_prefs,
        have_snapshot ? snapshot_path : nullptr,
        &exit_code);

    if (have_snapshot) {
        unlink(snapshot_path);
    }

    if (action == WINUAE_QT_LAUNCHER_START) {
        fixup_prefs(&changed_prefs, true);
        reset_sound();
        inputdevice_copyconfig(&changed_prefs, &currprefs);
        inputdevice_config_change_test();
        set_config_changed();
    } else if (action == WINUAE_QT_LAUNCHER_ERROR) {
        write_log("Unix Qt runtime UI exited with error code %d\n", exit_code);
    }

    pause_emulation = old_pause;
    setsystime();
    resume_sound();
    inputdevice_acquire(TRUE);
    fpscounter_reset();

    active = false;
#else
    write_log("Unix Qt runtime UI is not enabled in this build\n");
#endif
}
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
