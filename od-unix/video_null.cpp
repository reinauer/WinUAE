#include "sysconfig.h"
#include "sysdeps.h"

#include "video.h"

bool unix_video_setup(void)
{
    return true;
}

bool unix_video_init(int, int, int)
{
    return false;
}

void unix_video_shutdown(void)
{
}

int unix_video_poll(bool *quit_requested)
{
    if (quit_requested) {
        *quit_requested = false;
    }
    return 0;
}

void unix_video_present(const struct unix_video_frame *)
{
}

void unix_video_set_title(const TCHAR *)
{
}

void unix_video_get_desktop(int *dw, int *dh, int *x, int *y, int *w, int *h)
{
    if (dw) {
        *dw = 640;
    }
    if (dh) {
        *dh = 480;
    }
    if (x) {
        *x = 0;
    }
    if (y) {
        *y = 0;
    }
    if (w) {
        *w = 640;
    }
    if (h) {
        *h = 480;
    }
}
