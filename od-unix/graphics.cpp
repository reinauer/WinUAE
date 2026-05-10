#include "sysconfig.h"
#include "sysdeps.h"

#include "picasso96.h"
#include "xwin.h"

uae_u32 p96_rgbx16[65536];
bool gfx_hdr;
int flashscreen;
struct picasso96_state_struct picasso96_state[MAX_AMIGAMONITORS];
struct picasso_vidbuf_description picasso_vidinfo[MAX_AMIGAMONITORS];

int graphics_setup(void) { return 1; }
int graphics_init(bool) { return 1; }
void graphics_leave(void) {}
void graphics_reset(bool) {}
bool handle_events(void) { return false; }
int handle_msgpump(bool) { return 0; }
void setup_brkhandler(void) {}
int isfullscreen(void) { return 0; }
void toggle_fullscreen(int, int) {}
bool toggle_rtg(int, int) { return false; }
void close_rtg(int, bool) {}
void toggle_mousegrab(void) {}
void setmouseactivexy(int, int, int, int) {}
void desktop_coords(int, int *dw, int *dh, int *x, int *y, int *w, int *h)
{
    if (dw) *dw = 640;
    if (dh) *dh = 480;
    if (x) *x = 0;
    if (y) *y = 0;
    if (w) *w = 640;
    if (h) *h = 480;
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
void show_screen(int, int) {}
bool show_screen_maybe(int, bool) { return true; }
int lockscr(struct vidbuffer*, bool, bool) { return 1; }
void unlockscr(struct vidbuffer*, int, int) {}
bool target_graphics_buffer_update(int, bool) { return true; }
float target_adjust_vblank_hz(int, float hz) { return hz; }
int target_get_display_scanline(int) { return 0; }
void target_spin(int) {}
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
void refreshtitle(void) {}
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
