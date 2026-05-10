#include "sysconfig.h"
#include "sysdeps.h"

#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "options.h"
#include "memory.h"
#include "picasso96.h"
#include "uae.h"
#include "video.h"
#include "host.h"

#include <stdlib.h>

extern int pause_emulation;

uae_u32 p96_rgbx16[65536];
bool gfx_hdr;
int flashscreen;
struct picasso96_state_struct picasso96_state[MAX_AMIGAMONITORS];
struct picasso_vidbuf_description picasso_vidinfo[MAX_AMIGAMONITORS];

static bool unix_graphics_initialized;
static bool unix_video_debug;

enum {
    UNIX_PICASSO_STATE_SETDISPLAY = 1,
    UNIX_PICASSO_STATE_SETPANNING = 2,
    UNIX_PICASSO_STATE_SETGC = 4,
    UNIX_PICASSO_STATE_SETDAC = 8,
    UNIX_PICASSO_STATE_SETSWITCH = 16
};

static int unix_picasso_bytes_per_pixel(RGBFTYPE rgbfmt)
{
    switch (rgbfmt) {
    case RGBFB_CLUT:
    case RGBFB_Y4U1V1:
        return 1;
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_B5G6R5PC:
    case RGBFB_B5G5R5PC:
    case RGBFB_Y4U2V2:
        return 2;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        return 3;
    case RGBFB_A8R8G8B8:
    case RGBFB_A8B8G8R8:
    case RGBFB_R8G8B8A8:
    case RGBFB_B8G8R8A8:
        return 4;
    default:
        return 0;
    }
}

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
    unix_video_debug = getenv("WINUAE_UNIX_VIDEO_DEBUG") != NULL;
    unix_init_colors();
    if (unix_video_debug) {
        write_log(_T("Unix video colors: direct_rgb=%d black=%08x white=%08x r255=%08x g255=%08x b255=%08x\n"),
            direct_rgb ? 1 : 0, xcolors[0], xcolors[0xfff], xredcolors[255], xgreencolors[255], xbluecolors[255]);
    }
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
bool toggle_rtg(int monid, int)
{
    return monid >= 0 && monid < MAX_AMIGAMONITORS && currprefs.rtgboards[0].rtgmem_size > 0;
}
void close_rtg(int, bool) {}
void toggle_mousegrab(void) { unix_video_toggle_mouse_grab(); }
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
bool render_screen(int, int, bool)
{
    set_custom_limits(-1, -1, -1, -1, false);
    return true;
}

static void unix_log_video_frame(const struct vidbuffer *vb)
{
    static int frames;

    if (!unix_video_debug || !vb || !vb->bufmem || vb->pixbytes != 4) {
        return;
    }

    frames++;
    if (frames > 1 && (frames % 50) != 0) {
        return;
    }

    int nonblack = 0;
    int firstx = -1;
    int firsty = -1;
    int lastx = -1;
    int lasty = -1;
    uae_u32 first = 0;
    uae_u32 last = 0;

    int scan_width = vb->width_allocated > 0 ? vb->width_allocated : vb->outwidth;
    int scan_height = vb->height_allocated > 0 ? vb->height_allocated : vb->outheight;
    for (int y = 0; y < scan_height; y++) {
        const uae_u32 *row = (const uae_u32 *)(vb->bufmem + y * vb->rowbytes);
        for (int x = 0; x < scan_width; x++) {
            uae_u32 pixel = row[x];
            if ((pixel & 0x00ffffff) != 0) {
                if (!nonblack) {
                    firstx = x;
                    firsty = y;
                    first = pixel;
                }
                lastx = x;
                lasty = y;
                last = pixel;
                nonblack++;
            }
        }
    }

    write_log(_T("Unix video frame %d: out=%dx%d alloc=%dx%d pitch=%d xoff=%d yoff=%d nonblack=%d first=%d,%d:%08x last=%d,%d:%08x\n"),
        frames, vb->outwidth, vb->outheight, vb->width_allocated, vb->height_allocated, vb->rowbytes, vb->xoffset, vb->yoffset,
        nonblack, firstx, firsty, first, lastx, lasty, last);
}

void show_screen(int monid, int)
{
    if (!unix_graphics_initialized || monid < 0 || monid >= MAX_AMIGADISPLAYS) {
        return;
    }

    struct vidbuf_description *vidinfo = &adisplays[monid].gfxvidinfo;
    struct vidbuffer *vb = vidinfo->inbuffer ? vidinfo->inbuffer : &vidinfo->drawbuffer;
    if (!vb->bufmem || vb->outwidth <= 0 || vb->outheight <= 0) {
        return;
    }

    struct unix_video_frame frame;
    frame.pixels = vb->bufmem;
    frame.width = vb->outwidth;
    frame.height = vb->outheight;
    frame.rowbytes = vb->rowbytes;
    frame.pixbytes = vb->pixbytes;
    unix_log_video_frame(vb);
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

void InitPicasso96(int monid)
{
    if (monid < 0 || monid >= MAX_AMIGAMONITORS) {
        return;
    }

    memset(&picasso96_state[monid], 0, sizeof picasso96_state[monid]);
    memset(&picasso_vidinfo[monid], 0, sizeof picasso_vidinfo[monid]);
    picasso_vidinfo[monid].rgbformat = RGBFB_B8G8R8A8;
    picasso_vidinfo[monid].selected_rgbformat = RGBFB_B8G8R8A8;
    picasso_vidinfo[monid].host_mode = RGBFB_B8G8R8A8;
    picasso_vidinfo[monid].pixbytes = 4;
    for (int i = 0; i < 256; i++) {
        picasso96_state[monid].CLUT[i].Red = i;
        picasso96_state[monid].CLUT[i].Green = i;
        picasso96_state[monid].CLUT[i].Blue = i;
        picasso_vidinfo[monid].clut[i] = 0xff000000 | (i << 16) | (i << 8) | i;
    }
}

static bool unix_picasso_ensure_buffer(int monid)
{
    struct picasso96_state_struct *state = &picasso96_state[monid];
    struct picasso_vidbuf_description *pvidinfo = &picasso_vidinfo[monid];
    struct vidbuf_description *vidinfo = &adisplays[monid].gfxvidinfo;
    int width = state->Width > 0 ? state->Width : 640;
    int height = state->Height > 0 ? state->Height : 480;

    unix_alloc_buffer(monid, &vidinfo->drawbuffer, width, height);
    vidinfo->drawbuffer.outwidth = width;
    vidinfo->drawbuffer.outheight = height;
    vidinfo->drawbuffer.pixbytes = 4;
    vidinfo->drawbuffer.monitor_id = monid;
    vidinfo->inbuffer = &vidinfo->drawbuffer;
    vidinfo->outbuffer = &vidinfo->drawbuffer;

    pvidinfo->width = width;
    pvidinfo->height = height;
    pvidinfo->depth = state->GC_Depth ? (state->GC_Depth + 7) / 8 : 0;
    pvidinfo->pixbytes = 4;
    pvidinfo->rowbytes = vidinfo->drawbuffer.rowbytes;
    pvidinfo->rgbformat = state->RGBFormat;
    pvidinfo->selected_rgbformat = state->RGBFormat;
    pvidinfo->host_mode = RGBFB_B8G8R8A8;

    return vidinfo->drawbuffer.bufmem != NULL;
}

static uae_u32 unix_picasso_convert_pixel(const uae_u8 *src, RGBFTYPE fmt, const uae_u32 *clut)
{
    switch (fmt) {
    case RGBFB_CLUT:
        return clut[src[0]];
    case RGBFB_R8G8B8:
        return 0xff000000 | ((uae_u32)src[0] << 16) | ((uae_u32)src[1] << 8) | src[2];
    case RGBFB_B8G8R8:
        return 0xff000000 | ((uae_u32)src[2] << 16) | ((uae_u32)src[1] << 8) | src[0];
    case RGBFB_R8G8B8A8:
        return ((uae_u32)src[3] << 24) | ((uae_u32)src[0] << 16) | ((uae_u32)src[1] << 8) | src[2];
    case RGBFB_B8G8R8A8:
        return ((uae_u32)src[3] << 24) | ((uae_u32)src[2] << 16) | ((uae_u32)src[1] << 8) | src[0];
    case RGBFB_A8R8G8B8:
        return ((uae_u32)src[0] << 24) | ((uae_u32)src[1] << 16) | ((uae_u32)src[2] << 8) | src[3];
    case RGBFB_A8B8G8R8:
        return ((uae_u32)src[0] << 24) | ((uae_u32)src[3] << 16) | ((uae_u32)src[2] << 8) | src[1];
    case RGBFB_R5G6B5:
    case RGBFB_R5G5B5:
    case RGBFB_R5G6B5PC:
    case RGBFB_R5G5B5PC:
    case RGBFB_B5G6R5PC:
    case RGBFB_B5G5R5PC:
        return p96_rgbx16[do_get_mem_word((uae_u16 *)src)];
    default:
        return 0xff000000;
    }
}

static void unix_picasso_render(int monid)
{
    if (monid < 0 || monid >= MAX_AMIGAMONITORS || !unix_picasso_ensure_buffer(monid)) {
        return;
    }

    struct picasso96_state_struct *state = &picasso96_state[monid];
    struct picasso_vidbuf_description *pvidinfo = &picasso_vidinfo[monid];
    struct vidbuffer *vb = &adisplays[monid].gfxvidinfo.drawbuffer;
    addrbank *bank = gfxmem_banks[0];
    int srcpixbytes = unix_picasso_bytes_per_pixel((RGBFTYPE)state->RGBFormat);

    if (!bank || !bank->baseaddr || !srcpixbytes || !state->Address || !state->BytesPerRow ||
        !state->Width || !state->Height) {
        return;
    }
    if ((uae_u32)state->XYOffset < bank->start) {
        return;
    }

    uae_u32 offset = (uae_u32)state->XYOffset - bank->start;
    uae_u32 needed = offset + (state->Height - 1) * state->BytesPerRow + state->Width * srcpixbytes;
    if (needed > bank->allocated_size) {
        return;
    }

    alloc_colors_picasso(8, 8, 8, 16, 8, 0, (RGBFTYPE)state->RGBFormat, p96_rgbx16);
    const uae_u8 *srcbase = bank->baseaddr + offset;
    for (int y = 0; y < state->Height; y++) {
        const uae_u8 *src = srcbase + y * state->BytesPerRow;
        uae_u32 *dst = (uae_u32 *)(vb->bufmem + y * vb->rowbytes);
        for (int x = 0; x < state->Width; x++) {
            dst[x] = unix_picasso_convert_pixel(src + x * srcpixbytes, (RGBFTYPE)state->RGBFormat, pvidinfo->clut);
        }
    }
}

void picasso_enablescreen(int monid, int on)
{
    if (monid < 0 || monid >= MAX_AMIGAMONITORS) {
        return;
    }
    picasso_vidinfo[monid].picasso_active = on != 0;
    if (on) {
        picasso_refresh(monid);
    }
}

void picasso_refresh(int monid)
{
    if (monid < 0 || monid >= MAX_AMIGAMONITORS) {
        return;
    }
    unix_picasso_render(monid);
    if (adisplays[monid].picasso_on || picasso_vidinfo[monid].picasso_active) {
        show_screen(monid, 0);
    }
}
void init_hz_p96(int) {}

void gfx_set_picasso_modeinfo(int monid, RGBFTYPE rgbfmt)
{
    if (monid < 0 || monid >= MAX_AMIGAMONITORS) {
        return;
    }
    struct picasso96_state_struct *state = &picasso96_state[monid];
    picasso_vidinfo[monid].rgbformat = rgbfmt;
    picasso_vidinfo[monid].selected_rgbformat = rgbfmt;
    picasso_vidinfo[monid].depth = state->GC_Depth ? (state->GC_Depth + 7) / 8 : 0;
    picasso_vidinfo[monid].picasso_changed = true;
    unix_picasso_ensure_buffer(monid);
}

void gfx_set_picasso_colors(int monid, RGBFTYPE rgbfmt)
{
    if (monid >= 0 && monid < MAX_AMIGAMONITORS) {
        picasso_vidinfo[monid].rgbformat = rgbfmt;
    }
}

void gfx_set_picasso_state(int monid, int on)
{
    if (monid >= 0 && monid < MAX_AMIGAMONITORS) {
        picasso_vidinfo[monid].picasso_active = on != 0;
    }
}

uae_u8 *gfx_lock_picasso(int monid, bool)
{
    if (monid < 0 || monid >= MAX_AMIGAMONITORS || !unix_picasso_ensure_buffer(monid)) {
        return NULL;
    }
    struct vidbuffer *vb = &adisplays[monid].gfxvidinfo.drawbuffer;
    vb->locked = true;
    return vb->bufmem;
}

void gfx_unlock_picasso(int monid, bool dorender)
{
    if (monid < 0 || monid >= MAX_AMIGAMONITORS) {
        return;
    }
    struct vidbuffer *vb = &adisplays[monid].gfxvidinfo.drawbuffer;
    vb->locked = false;
    if (dorender) {
        show_screen(monid, 0);
    }
}
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

void picasso_handle_vsync(void)
{
    for (int monid = 0; monid < MAX_AMIGAMONITORS; monid++) {
        struct amigadisplay *ad = &adisplays[monid];
        struct picasso_vidbuf_description *vidinfo = &picasso_vidinfo[monid];
        int state = vidinfo->picasso_state_change;

        if (state) {
            atomic_and(&vidinfo->picasso_state_change, ~state);
            if (state & UNIX_PICASSO_STATE_SETGC) {
                gfx_set_picasso_modeinfo(monid, (RGBFTYPE)picasso96_state[monid].RGBFormat);
            }
            if (state & UNIX_PICASSO_STATE_SETSWITCH) {
                vidinfo->picasso_active = ad->picasso_requested_on;
            }
            if (state & (UNIX_PICASSO_STATE_SETGC | UNIX_PICASSO_STATE_SETPANNING |
                UNIX_PICASSO_STATE_SETDAC | UNIX_PICASSO_STATE_SETDISPLAY)) {
                vidinfo->full_refresh = 1;
            }
        }
        if (ad->picasso_on || vidinfo->picasso_active) {
            picasso_refresh(monid);
        }
    }
}

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
