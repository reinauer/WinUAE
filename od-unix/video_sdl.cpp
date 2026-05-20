#include "sysconfig.h"
#include "sysdeps.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <vector>

#include "statusline.h"
#include "disk.h"
#include "gui.h"
#include "input.h"
#include "options.h"
#include "uae.h"
#include "video.h"

extern int pause_emulation;
extern void pausemode(int mode);

static SDL_Window *s_window;
static SDL_Renderer *s_renderer;
static SDL_Texture *s_texture;
static SDL_Texture *s_status_texture;
static bool s_setup_done;
static bool s_available;
static bool s_mouse_grabbed;
static enum unix_video_window_mode s_requested_window_mode = UNIX_VIDEO_WINDOWED;
static enum unix_video_window_mode s_active_window_mode = UNIX_VIDEO_WINDOWED;
static int s_requested_display_index;
static int s_active_display_index = -1;
static int s_requested_fullscreen_width;
static int s_requested_fullscreen_height;
static int s_requested_fullscreen_refresh;
static int s_active_fullscreen_width = -1;
static int s_active_fullscreen_height = -1;
static int s_active_fullscreen_refresh = -1;
static int s_texture_width;
static int s_texture_height;
static int s_texture_pixbytes;
static int s_status_width;
static int s_status_height;
static std::vector<uae_u32> s_status_pixels;
static uae_u32 s_status_rc[256];
static uae_u32 s_status_gc[256];
static uae_u32 s_status_bc[256];
static bool s_status_colors_ready;
static Uint8 s_status_click_button;

static constexpr int UnixStatusScale = 2;
static TCHAR s_display_name[MAX_DPATH];

static int clamp_window_dimension(int value, int fallback, int maxvalue)
{
    if (value <= 0) {
        value = fallback;
    }
    if (value > maxvalue) {
        value = maxvalue;
    }
    return value;
}

static SDL_PixelFormat texture_format_for_pixbytes(int pixbytes)
{
    if (pixbytes == 2) {
        return SDL_PIXELFORMAT_RGB565;
    }
    return SDL_PIXELFORMAT_ARGB8888;
}

static SDL_DisplayID *get_sdl_displays(int *count)
{
    if (count) {
        *count = 0;
    }
    if (!unix_video_setup()) {
        return NULL;
    }
    return SDL_GetDisplays(count);
}

static SDL_DisplayID get_sdl_display_for_index(int display_index)
{
    int count = 0;
    SDL_DisplayID *displays = get_sdl_displays(&count);
    SDL_DisplayID display = 0;

    if (displays && count > 0) {
        if (display_index > 0 && display_index <= count) {
            display = displays[display_index - 1];
        } else {
            display = displays[0];
        }
    }
    if (displays) {
        SDL_free(displays);
    }
    if (!display) {
        display = SDL_GetPrimaryDisplay();
    }
    return display;
}

static void center_window_on_display(SDL_DisplayID display)
{
    if (!s_window || !display) {
        return;
    }
    SDL_SetWindowPosition(s_window, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display));
}

static float refresh_rate_from_mode(const SDL_DisplayMode *mode)
{
    if (!mode) {
        return 0.0f;
    }
    if (mode->refresh_rate_numerator > 0 && mode->refresh_rate_denominator > 0) {
        return (float)mode->refresh_rate_numerator / (float)mode->refresh_rate_denominator;
    }
    return mode->refresh_rate;
}

static bool copy_sdl_display_name(int index, bool friendlyname, TCHAR *dst, size_t dstsize)
{
    int count = 0;
    SDL_DisplayID *displays = get_sdl_displays(&count);
    if (!displays) {
        return false;
    }

    bool found = false;
    if (index >= 0 && index < count) {
        if (friendlyname) {
            const char *name = SDL_GetDisplayName(displays[index]);
            if (name && name[0]) {
                snprintf(dst, dstsize, "%s", name);
                found = true;
            }
        } else {
            snprintf(dst, dstsize, "SDL:%u", (unsigned int)displays[index]);
            found = true;
        }
    }

    SDL_free(displays);
    return found;
}

int target_get_display(const TCHAR *name)
{
    if (!name || !name[0]) {
        return -1;
    }

    int count = 0;
    SDL_DisplayID *displays = get_sdl_displays(&count);
    if (!displays) {
        return -1;
    }

    int found = -1;
    unsigned int displayid = 0;
    if (sscanf(name, "SDL:%u", &displayid) == 1) {
        for (int i = 0; i < count; i++) {
            if (displays[i] == (SDL_DisplayID)displayid) {
                found = i + 1;
                break;
            }
        }
    } else {
        for (int i = 0; i < count; i++) {
            const char *displayname = SDL_GetDisplayName(displays[i]);
            if (displayname && !_tcsicmp(displayname, name)) {
                found = i + 1;
                break;
            }
        }
    }

    SDL_free(displays);
    return found;
}

const TCHAR *target_get_display_name(int num, bool friendlyname)
{
    if (num <= 0) {
        return NULL;
    }
    if (!copy_sdl_display_name(num - 1, friendlyname, s_display_name, sizeof s_display_name / sizeof s_display_name[0])) {
        return NULL;
    }
    return s_display_name;
}

static int unix_input_lock_state_from_sdl(SDL_Keymod mod)
{
    int lockstate = 0;
    if (mod & SDL_KMOD_CAPS) {
        lockstate |= UNIX_INPUT_LOCK_CAPS;
    }
    if (mod & SDL_KMOD_NUM) {
        lockstate |= UNIX_INPUT_LOCK_NUM;
    }
    if (mod & SDL_KMOD_SCROLL) {
        lockstate |= UNIX_INPUT_LOCK_SCROLL;
    }
    return lockstate;
}

static int statusbar_source_height(void)
{
    return TD_TOTAL_HEIGHT;
}

static int statusbar_display_height(void)
{
    return statusbar_source_height() * UnixStatusScale;
}

static void init_status_colors(void)
{
    if (s_status_colors_ready) {
        return;
    }
    for (int i = 0; i < 256; i++) {
        s_status_rc[i] = 0xff000000u | (uae_u32(i) << 16);
        s_status_gc[i] = uae_u32(i) << 8;
        s_status_bc[i] = uae_u32(i);
    }
    s_status_colors_ready = true;
}

static bool ensure_texture(int width, int height, int pixbytes)
{
    if (!s_renderer || width <= 0 || height <= 0) {
        return false;
    }
    if (s_texture && s_texture_width == width && s_texture_height == height && s_texture_pixbytes == pixbytes) {
        return true;
    }

    if (s_texture) {
        SDL_DestroyTexture(s_texture);
        s_texture = NULL;
    }

    s_texture = SDL_CreateTexture(s_renderer, texture_format_for_pixbytes(pixbytes), SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!s_texture) {
        write_log(_T("SDL3: failed to create %dx%d texture: %s\n"), width, height, SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(s_texture, SDL_BLENDMODE_NONE);
    s_texture_width = width;
    s_texture_height = height;
    s_texture_pixbytes = pixbytes;
    return true;
}

static bool ensure_status_texture(int width)
{
    const int height = statusbar_source_height();
    if (!s_renderer || width <= 0 || height <= 0) {
        return false;
    }
    if (s_status_texture && s_status_width == width && s_status_height == height) {
        return true;
    }

    if (s_status_texture) {
        SDL_DestroyTexture(s_status_texture);
        s_status_texture = NULL;
    }
    s_status_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!s_status_texture) {
        write_log(_T("SDL3: failed to create %dx%d status texture: %s\n"), width, height, SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(s_status_texture, SDL_BLENDMODE_NONE);
    s_status_width = width;
    s_status_height = height;
    s_status_pixels.resize(size_t(width) * size_t(height));
    return true;
}

static bool update_status_texture(int width)
{
    if (!ensure_status_texture(width)) {
        return false;
    }

    init_status_colors();
    std::fill(s_status_pixels.begin(), s_status_pixels.end(), 0xffd4d0c8u);
    statusline_set_multiplier(0, width, statusbar_source_height());
    for (int y = 0; y < statusbar_source_height(); y++) {
        draw_status_line_single(
            0,
            reinterpret_cast<uae_u8 *>(s_status_pixels.data() + size_t(y) * size_t(width)),
            y,
            width,
            s_status_rc,
            s_status_gc,
            s_status_bc,
            NULL);
    }

    SDL_UpdateTexture(s_status_texture, NULL, s_status_pixels.data(), width * int(sizeof(uae_u32)));
    return true;
}

static int unix_mouse_button_from_sdl(Uint8 button)
{
    switch (button) {
    case SDL_BUTTON_LEFT:
        return 0;
    case SDL_BUTTON_RIGHT:
        return 1;
    case SDL_BUTTON_MIDDLE:
        return 2;
    default:
        return -1;
    }
}

static bool statusbar_logical_position(int window_x, int window_y, int *logical_x, int *logical_y)
{
    if (!s_window || s_texture_width <= 0 || s_texture_height <= 0) {
        return false;
    }

    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(s_window, &window_width, &window_height);
    if (window_width <= 0 || window_height <= 0) {
        return false;
    }

    const int logical_width = s_texture_width;
    const int logical_height = s_texture_height + statusbar_display_height();
    const int lx = window_x * logical_width / window_width;
    const int ly = window_y * logical_height / window_height;
    if (ly < s_texture_height || ly >= logical_height) {
        return false;
    }
    if (logical_x) {
        *logical_x = lx;
    }
    if (logical_y) {
        *logical_y = ly;
    }
    return true;
}

static int statusbar_hit_slot(int logical_x)
{
    if (s_status_width <= 0) {
        return -1;
    }

    int mult = statusline_get_multiplier(0) / 100;
    if (mult < 1) {
        mult = 1;
    }
    const int x_start = (td_numbers_pos & TD_RIGHT)
        ? s_status_width - (td_numbers_padx + VISIBLE_LEDS * td_width) * mult
        : td_numbers_padx * mult;
    const int slot_width = td_width * mult;
    if (slot_width <= 0 || logical_x < x_start) {
        return -1;
    }
    const int slot = (logical_x - x_start) / slot_width;
    return slot >= 0 && slot < VISIBLE_LEDS ? slot : -1;
}

static bool handle_statusbar_click(int window_x, int window_y, Uint8 button)
{
    int logical_x = 0;
    if (!statusbar_logical_position(window_x, window_y, &logical_x, NULL)) {
        return false;
    }

    const int slot = statusbar_hit_slot(logical_x);
    if (slot < 0) {
        return true;
    }

    const bool right_click = button == SDL_BUTTON_RIGHT;
    if (slot >= 8 && slot <= 11) {
        const int drive = slot - 8;
        if (right_click) {
            disk_eject(drive);
        } else if (changed_prefs.floppyslots[drive].dfxtype >= 0) {
            gui_display(drive);
        }
        return true;
    }
    if (slot == 6) {
        if (right_click) {
            changed_prefs.cdslots[0].name[0] = 0;
            changed_prefs.cdslots[0].inuse = false;
            set_config_changed();
        } else {
            gui_display(6);
        }
        return true;
    }
    if (slot == 3) {
        if (right_click) {
            uae_reset(0, 1);
        } else {
            gui_display(-1);
        }
        return true;
    }
    if (slot == 2 && !right_click && pause_emulation) {
        pausemode(0);
        return true;
    }

    return true;
}

bool unix_video_setup(void)
{
    if (s_setup_done) {
        return s_available;
    }

    SDL_SetMainReady();
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        write_log(_T("SDL3: video unavailable: %s\n"), SDL_GetError());
        s_setup_done = true;
        s_available = false;
        return false;
    }

    s_setup_done = true;
    s_available = true;
    return true;
}

static bool apply_window_mode(void)
{
    if (!s_window || !s_setup_done || !s_available) {
        return false;
    }

    if (s_requested_window_mode == s_active_window_mode &&
        s_requested_display_index == s_active_display_index &&
        s_requested_fullscreen_width == s_active_fullscreen_width &&
        s_requested_fullscreen_height == s_active_fullscreen_height &&
        s_requested_fullscreen_refresh == s_active_fullscreen_refresh) {
        return true;
    }

    SDL_DisplayID display = get_sdl_display_for_index(s_requested_display_index);
    center_window_on_display(display);

    if (s_requested_window_mode == UNIX_VIDEO_WINDOWED) {
        if (!SDL_SetWindowFullscreen(s_window, false)) {
            write_log(_T("SDL3: failed to leave fullscreen: %s\n"), SDL_GetError());
            return false;
        }
        s_active_window_mode = UNIX_VIDEO_WINDOWED;
        s_active_display_index = s_requested_display_index;
        s_active_fullscreen_width = s_requested_fullscreen_width;
        s_active_fullscreen_height = s_requested_fullscreen_height;
        s_active_fullscreen_refresh = s_requested_fullscreen_refresh;
        return true;
    }

    SDL_DisplayMode fullscreen_mode;
    const SDL_DisplayMode *mode = NULL;
    if (s_requested_window_mode == UNIX_VIDEO_FULLSCREEN) {
        if (s_requested_fullscreen_width > 0 && s_requested_fullscreen_height > 0) {
            float refresh = s_requested_fullscreen_refresh > 0 ? (float)s_requested_fullscreen_refresh : 0.0f;
            if (SDL_GetClosestFullscreenDisplayMode(display, s_requested_fullscreen_width,
                s_requested_fullscreen_height, refresh, true, &fullscreen_mode)) {
                mode = &fullscreen_mode;
            } else {
                write_log(_T("SDL3: no exclusive fullscreen mode %dx%d@%d on display %d, using desktop fullscreen\n"),
                    s_requested_fullscreen_width, s_requested_fullscreen_height,
                    s_requested_fullscreen_refresh, s_requested_display_index);
            }
        } else {
            const SDL_DisplayMode *desktop = SDL_GetDesktopDisplayMode(display);
            if (desktop) {
                fullscreen_mode = *desktop;
                mode = &fullscreen_mode;
            }
        }
    }

    if (!SDL_SetWindowFullscreenMode(s_window, mode)) {
        write_log(_T("SDL3: failed to set fullscreen mode: %s\n"), SDL_GetError());
        return false;
    }
    if (!SDL_SetWindowFullscreen(s_window, true)) {
        write_log(_T("SDL3: failed to enter fullscreen: %s\n"), SDL_GetError());
        return false;
    }

    s_active_window_mode = s_requested_window_mode;
    s_active_display_index = s_requested_display_index;
    s_active_fullscreen_width = s_requested_fullscreen_width;
    s_active_fullscreen_height = s_requested_fullscreen_height;
    s_active_fullscreen_refresh = s_requested_fullscreen_refresh;
    return true;
}

bool unix_video_init(int width, int height, int pixbytes)
{
    if (!unix_video_setup()) {
        return false;
    }

    width = width > 0 ? width : 768;
    height = height > 0 ? height : 576;
    pixbytes = pixbytes == 2 ? 2 : 4;

    if (!s_window) {
        int window_width = clamp_window_dimension(width, 768, 960);
        int window_height = clamp_window_dimension(height + statusbar_display_height(), 576 + statusbar_display_height(), 720 + statusbar_display_height());
        s_window = SDL_CreateWindow("WinUAE Unix", window_width, window_height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!s_window) {
            write_log(_T("SDL3: failed to create window: %s\n"), SDL_GetError());
            s_available = false;
            return false;
        }
        center_window_on_display(get_sdl_display_for_index(s_requested_display_index));
    }

    if (!s_renderer) {
        s_renderer = SDL_CreateRenderer(s_window, NULL);
        if (!s_renderer) {
            s_renderer = SDL_CreateRenderer(s_window, "software");
        }
        if (!s_renderer) {
            write_log(_T("SDL3: failed to create renderer: %s\n"), SDL_GetError());
            s_available = false;
            return false;
        }
        SDL_SetRenderVSync(s_renderer, 1);
        SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255);
        SDL_RenderClear(s_renderer);
        SDL_RenderPresent(s_renderer);
    }

    apply_window_mode();

    return ensure_texture(width, height, pixbytes);
}

void unix_video_shutdown(void)
{
    unix_input_release_keys();
    unix_video_set_mouse_grab(false);

    if (s_texture) {
        SDL_DestroyTexture(s_texture);
        s_texture = NULL;
    }
    if (s_status_texture) {
        SDL_DestroyTexture(s_status_texture);
        s_status_texture = NULL;
    }
    s_status_pixels.clear();
    if (s_renderer) {
        SDL_DestroyRenderer(s_renderer);
        s_renderer = NULL;
    }
    if (s_window) {
        SDL_DestroyWindow(s_window);
        s_window = NULL;
    }
    s_texture_width = 0;
    s_texture_height = 0;
    s_texture_pixbytes = 0;
    s_status_width = 0;
    s_status_height = 0;

    if (s_setup_done && s_available) {
        SDL_QuitSubSystem(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
    }
    s_setup_done = false;
    s_available = false;
}

int unix_video_poll(bool *quit_requested)
{
    SDL_Event event;
    int got = 0;

    if (quit_requested) {
        *quit_requested = false;
    }
    if (!s_setup_done || !s_available) {
        return 0;
    }

    while (SDL_PollEvent(&event)) {
        got = 1;
        switch (event.type) {
        case SDL_EVENT_QUIT:
            if (quit_requested) {
                *quit_requested = true;
            }
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (quit_requested) {
                *quit_requested = true;
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            unix_input_release_keys();
            unix_video_set_mouse_grab(false);
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (event.key.repeat) {
                break;
            }
            if (event.key.key == SDLK_Q && (event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
                if (quit_requested) {
                    *quit_requested = true;
                }
                break;
            }
            if (event.key.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_G &&
                (event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI))) {
                unix_video_set_mouse_grab(false);
                break;
            }
            if (event.key.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE && s_mouse_grabbed) {
                unix_video_set_mouse_grab(false);
            }
            unix_input_keyboard_key((int)event.key.scancode, event.key.type == SDL_EVENT_KEY_DOWN,
                unix_input_lock_state_from_sdl(event.key.mod));
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (s_mouse_grabbed) {
                unix_input_mouse_motion((int)event.motion.xrel, (int)event.motion.yrel);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            int button = unix_mouse_button_from_sdl(event.button.button);
            if (button >= 0) {
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && handle_statusbar_click((int)event.button.x, (int)event.button.y, event.button.button)) {
                    s_status_click_button = event.button.button;
                    break;
                }
                if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && s_status_click_button == event.button.button) {
                    s_status_click_button = 0;
                    break;
                }
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !s_mouse_grabbed) {
                    unix_video_set_mouse_grab(true);
                }
                unix_input_mouse_button(button, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            if (s_mouse_grabbed) {
                unix_input_mouse_wheel(event.wheel.integer_x, event.wheel.integer_y);
            }
            break;
        case SDL_EVENT_JOYSTICK_ADDED:
        case SDL_EVENT_JOYSTICK_REMOVED:
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
        case SDL_EVENT_GAMEPAD_REMAPPED:
            unix_input_joystick_device_changed();
            break;
        default:
            break;
        }
    }

    return got;
}

void unix_video_present(const struct unix_video_frame *frame)
{
    if (!frame || !frame->pixels || frame->width <= 0 || frame->height <= 0 || frame->rowbytes <= 0) {
        return;
    }
    if (!unix_video_init(frame->width, frame->height, frame->pixbytes)) {
        return;
    }
    if (!ensure_texture(frame->width, frame->height, frame->pixbytes)) {
        return;
    }

    SDL_UpdateTexture(s_texture, NULL, frame->pixels, frame->rowbytes);
    SDL_SetRenderLogicalPresentation(s_renderer, frame->width, frame->height + statusbar_display_height(), SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_RenderClear(s_renderer);
    SDL_FRect frame_dst = { 0.0f, 0.0f, (float)frame->width, (float)frame->height };
    SDL_RenderTexture(s_renderer, s_texture, NULL, &frame_dst);
    if (update_status_texture(frame->width)) {
        SDL_FRect status_dst = { 0.0f, (float)frame->height, (float)frame->width, (float)statusbar_display_height() };
        SDL_RenderTexture(s_renderer, s_status_texture, NULL, &status_dst);
    }
    SDL_RenderPresent(s_renderer);
}

void unix_video_set_title(const TCHAR *title)
{
    if (s_window && title) {
        SDL_SetWindowTitle(s_window, title);
    }
}

bool unix_video_set_window_mode(enum unix_video_window_mode mode, int display_index, int width, int height, int refresh_rate)
{
    s_requested_window_mode = mode;
    s_requested_display_index = display_index;
    s_requested_fullscreen_width = width;
    s_requested_fullscreen_height = height;
    s_requested_fullscreen_refresh = refresh_rate;

    if (!s_window) {
        return true;
    }
    return apply_window_mode();
}

enum unix_video_window_mode unix_video_get_window_mode(void)
{
    return s_active_window_mode;
}

float unix_video_get_display_refresh_rate(int display_index)
{
    if (!unix_video_setup()) {
        return 0.0f;
    }

    SDL_DisplayID display = 0;
    if (display_index < 0 && s_window) {
        display = SDL_GetDisplayForWindow(s_window);
    }
    if (!display) {
        display = get_sdl_display_for_index(display_index);
    }

    const SDL_DisplayMode *mode = display ? SDL_GetCurrentDisplayMode(display) : NULL;
    float refresh = refresh_rate_from_mode(mode);
    if (refresh <= 0.0f && display) {
        refresh = refresh_rate_from_mode(SDL_GetDesktopDisplayMode(display));
    }
    return refresh;
}

void unix_video_set_mouse_grab(bool grab)
{
    s_mouse_grabbed = grab;
    unix_input_set_mouse_active(grab);

    if (!s_setup_done || !s_available) {
        return;
    }

    if (s_window) {
        SDL_SetWindowRelativeMouseMode(s_window, grab);
        SDL_SetWindowMouseGrab(s_window, grab);
        SDL_CaptureMouse(grab);
    }
}

bool unix_video_get_mouse_grab(void)
{
    return s_mouse_grabbed;
}

void unix_video_toggle_mouse_grab(void)
{
    unix_video_set_mouse_grab(!unix_video_get_mouse_grab());
}

void unix_video_get_desktop(int *dw, int *dh, int *x, int *y, int *w, int *h)
{
    SDL_Rect usable;
    SDL_DisplayID display = SDL_GetPrimaryDisplay();

    if (x) {
        *x = 0;
    }
    if (y) {
        *y = 0;
    }
    const SDL_DisplayMode *mode = display ? SDL_GetCurrentDisplayMode(display) : NULL;
    if (mode) {
        if (dw) {
            *dw = mode->w;
        }
        if (dh) {
            *dh = mode->h;
        }
    } else {
        if (dw) {
            *dw = 640;
        }
        if (dh) {
            *dh = 480;
        }
    }

    if (display && SDL_GetDisplayUsableBounds(display, &usable)) {
        if (x) {
            *x = usable.x;
        }
        if (y) {
            *y = usable.y;
        }
        if (w) {
            *w = usable.w;
        }
        if (h) {
            *h = usable.h;
        }
    } else {
        if (w) {
            *w = dw ? *dw : 640;
        }
        if (h) {
            *h = dh ? *dh : 480;
        }
    }
}
