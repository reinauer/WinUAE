#ifndef WINUAE_OD_UNIX_VIDEO_H
#define WINUAE_OD_UNIX_VIDEO_H

#include "sysdeps.h"

struct unix_video_frame
{
    const uae_u8 *pixels;
    int width;
    int height;
    int rowbytes;
    int pixbytes;
};

bool unix_video_setup(void);
bool unix_video_init(int width, int height, int pixbytes);
void unix_video_shutdown(void);
int unix_video_poll(bool *quit_requested);
void unix_video_present(const struct unix_video_frame *frame);
void unix_video_set_title(const TCHAR *title);
void unix_video_get_desktop(int *dw, int *dh, int *x, int *y, int *w, int *h);

#endif /* WINUAE_OD_UNIX_VIDEO_H */
