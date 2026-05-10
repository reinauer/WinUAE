#include "sysconfig.h"
#include "sysdeps.h"

#include <stdarg.h>
#include "gui.h"

unsigned int gui_ledstate;

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
