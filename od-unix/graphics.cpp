#include "sysconfig.h"
#include "sysdeps.h"

#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "options.h"
#include "picasso96.h"
#include "uae.h"
#include "video.h"
#include "host.h"

extern int pause_emulation;

uae_u32 p96_rgbx16[65536];
bool gfx_hdr;
int flashscreen;
struct picasso96_state_struct picasso96_state[MAX_AMIGAMONITORS];
struct picasso_vidbuf_description picasso_vidinfo[MAX_AMIGAMONITORS];

static bool unix_graphics_initialized;

static void unix_init_colors(void)
{
    alloc_colors64k(0, 8, 8, 8, 16, 8, 0, 8, 24, 1, 0);
    notice_new_xcolors();
    alloc_colors_picasso(8, 8, 8, 16, 8, 0, RGBFB_R8G8B8A8, p96_rgbx16);
}

static void unix_alloc_buffer(int monid, struct vidbuffer *buffer, int width, int height)
{
    if (buffer->realbufmem && buffer->width_allocated >= width && buffer->height_allocated >= height &&
        buffer->pixbytes == 4) {
        return;
    }

    freevidbuffer(monid, buffer);
    allocvidbuffer(monid, buffer, width, height, 32);
    buffer->initialized = true;
}

static void unix_init_display_buffers(void)
{
    struct vidbuf_description *vidinfo = &adisplays[0].gfxvidinfo;

    vidinfo->gfx_resolution_reserved = RES_MAX;
    vidinfo->gfx_vresolution_reserved = VRES_MAX;
    vidinfo->xchange = 1;
    vidinfo->ychange = 1;

    unix_alloc_buffer(0, &vidinfo->drawbuffer, 1920, 1280);
    unix_alloc_buffer(0, &vidinfo->tempbuffer, 2048, 2048);

    vidinfo->drawbuffer.monitor_id = 0;
    vidinfo->tempbuffer.monitor_id = 0;
    vidinfo->outbuffer = &vidinfo->drawbuffer;
    vidinfo->inbuffer = &vidinfo->drawbuffer;
}

int graphics_setup(void)
{
    unix_init_colors();
    InitPicasso96(0);
    return 1;
}

int graphics_init(bool)
{
    unix_init_display_buffers();
    struct vidbuffer *vb = &adisplays[0].gfxvidinfo.drawbuffer;
    if (!unix_video_init(vb->outwidth, vb->outheight, vb->pixbytes)) {
        write_log(_T("Unix video: no window presenter available, continuing headless\n"));
    }
    unix_graphics_initialized = true;
    return 1;
}

void graphics_leave(void)
{
    struct vidbuf_description *vidinfo = &adisplays[0].gfxvidinfo;
    freevidbuffer(0, &vidinfo->drawbuffer);
    freevidbuffer(0, &vidinfo->tempbuffer);
    unix_video_shutdown();
    unix_graphics_initialized = false;
}

void graphics_reset(bool) {}

bool handle_events(void)
{
    handle_msgpump(false);
    return pause_emulation != 0;
}

int handle_msgpump(bool)
{
    unix_host_check_quit();
    bool quit_requested = false;
    int got = unix_video_poll(&quit_requested);
    if (quit_requested) {
        uae_quit();
    }
    unix_host_check_quit();
    return got;
}

int isfullscreen(void) { return 0; }
void toggle_fullscreen(int, int) {}
bool toggle_rtg(int, int) { return false; }
void close_rtg(int, bool) {}
void toggle_mousegrab(void) {}
void setmouseactivexy(int, int, int, int) {}

void desktop_coords(int, int *dw, int *dh, int *x, int *y, int *w, int *h)
{
    unix_video_get_desktop(dw, dh, x, y, w, h);
}

bool vsync_switchmode(int, int) { return false; }
void vsync_clear(void) {}
int vsync_isdone(frame_time_t*) { return 1; }
void doflashscreen(void) {}
void updatedisplayarea(int) {}
void flush_line(struct vidbuffer*, int) {}
void flush_block(struct vidbuffer*, int, int) {}
void flush_screen(struct vidbuffer*, int, int) {}
void flush_clear_screen(struct vidbuffer*) {}
bool render_screen(int, int, bool) { return true; }

void show_screen(int monid, int)
{
    if (!unix_graphics_initialized || monid < 0 || monid >= MAX_AMIGADISPLAYS) {
        return;
    }

    struct vidbuf_description *vidinfo = &adisplays[monid].gfxvidinfo;
    struct vidbuffer *vb = vidinfo->outbuffer ? vidinfo->outbuffer : &vidinfo->drawbuffer;
    if (!vb->bufmem || vb->outwidth <= 0 || vb->outheight <= 0) {
        return;
    }

    struct unix_video_frame frame;
    frame.pixels = vb->bufmem;
    frame.width = vb->outwidth;
    frame.height = vb->outheight;
    frame.rowbytes = vb->rowbytes;
    frame.pixbytes = vb->pixbytes;
    unix_video_present(&frame);
}

bool show_screen_maybe(int monid, bool)
{
    show_screen(monid, 0);
    return true;
}

int lockscr(struct vidbuffer *vb, bool, bool)
{
    if (!vb || !vb->bufmem) {
        return 0;
    }
    vb->locked = true;
    return 1;
}

void unlockscr(struct vidbuffer *vb, int, int)
{
    if (vb) {
        vb->locked = false;
    }
}

bool target_graphics_buffer_update(int, bool) { return true; }
float target_adjust_vblank_hz(int, float hz) { return hz; }
int target_get_display_scanline(int) { return 0; }
void target_spin(int)
{
    static int spin_counter;
    if ((spin_counter++ & 31) == 0 || unix_host_quit_requested()) {
        handle_msgpump(false);
    }
}

void getgfxoffset(int, float *dxp, float *dyp, float *mxp, float *myp)
{
    if (dxp) *dxp = 0;
    if (dyp) *dyp = 0;
    if (mxp) *mxp = 1;
    if (myp) *myp = 1;
}

float target_getcurrentvblankrate(int) { return 60.0f; }
int debuggable(void) { return 0; }
void screenshot(int, int, int) {}

void refreshtitle(void)
{
    unix_video_set_title(_T("WinUAE Unix"));
}

void InitPicasso96(int) {}
void picasso_enablescreen(int, int) {}
void picasso_refresh(int) {}
void init_hz_p96(int) {}

void gfx_set_picasso_modeinfo(int monid, RGBFTYPE rgbfmt)
{
    if (monid < 0 || monid >= MAX_AMIGAMONITORS) {
        return;
    }
    picasso_vidinfo[monid].rgbformat = rgbfmt;
    picasso_vidinfo[monid].selected_rgbformat = rgbfmt;
}

void gfx_set_picasso_colors(int, RGBFTYPE) {}
void gfx_set_picasso_state(int, int) {}
uae_u8 *gfx_lock_picasso(int, bool) { return NULL; }
void gfx_unlock_picasso(int, bool) {}
void picasso_allocatewritewatch(int, int) {}

int picasso_getwritewatch(int, int, uae_u8 ***gwwbufp, uae_u8 **startp)
{
    if (gwwbufp) {
        *gwwbufp = NULL;
    }
    if (startp) {
        *startp = NULL;
    }
    return 0;
}

bool picasso_is_vram_dirty(int, uaecptr, int) { return true; }
void picasso_invalidate(int, int, int, int, int) {}

void fb_copyrow(int monid, uae_u8 *src, uae_u8 *dst, int x, int, int width, int srcpixbytes, int dy)
{
    if (!src || !dst || monid < 0 || monid >= MAX_AMIGAMONITORS) {
        return;
    }
    int rowbytes = picasso_vidinfo[monid].rowbytes;
    int pixbytes = picasso_vidinfo[monid].pixbytes ? picasso_vidinfo[monid].pixbytes : srcpixbytes;
    if (rowbytes <= 0 || pixbytes <= 0 || width <= 0) {
        return;
    }
    memcpy(dst + dy * rowbytes + x * pixbytes, src, (size_t)width * (size_t)srcpixbytes);
}
