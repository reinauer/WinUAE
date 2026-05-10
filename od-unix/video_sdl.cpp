#include "sysconfig.h"
#include "sysdeps.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "input.h"
#include "video.h"

static SDL_Window *s_window;
static SDL_Renderer *s_renderer;
static SDL_Texture *s_texture;
static bool s_setup_done;
static bool s_available;
static bool s_mouse_grabbed;
static int s_texture_width;
static int s_texture_height;
static int s_texture_pixbytes;

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

static Uint32 texture_format_for_pixbytes(int pixbytes)
{
    if (pixbytes == 2) {
        return SDL_PIXELFORMAT_RGB565;
    }
    return SDL_PIXELFORMAT_ARGB8888;
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
        write_log(_T("SDL2: failed to create %dx%d texture: %s\n"), width, height, SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(s_texture, SDL_BLENDMODE_NONE);
    SDL_RenderSetLogicalSize(s_renderer, width, height);
    s_texture_width = width;
    s_texture_height = height;
    s_texture_pixbytes = pixbytes;
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

bool unix_video_setup(void)
{
    if (s_setup_done) {
        return s_available;
    }

    SDL_SetMainReady();
    if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        write_log(_T("SDL2: video unavailable: %s\n"), SDL_GetError());
        s_setup_done = true;
        s_available = false;
        return false;
    }

    s_setup_done = true;
    s_available = true;
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
        int window_height = clamp_window_dimension(height, 576, 720);
        s_window = SDL_CreateWindow("WinUAE Unix", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            window_width, window_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!s_window) {
            write_log(_T("SDL2: failed to create window: %s\n"), SDL_GetError());
            s_available = false;
            return false;
        }
    }

    if (!s_renderer) {
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!s_renderer) {
            s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
        }
        if (!s_renderer) {
            write_log(_T("SDL2: failed to create renderer: %s\n"), SDL_GetError());
            s_available = false;
            return false;
        }
        SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255);
        SDL_RenderClear(s_renderer);
        SDL_RenderPresent(s_renderer);
    }

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
        case SDL_QUIT:
            if (quit_requested) {
                *quit_requested = true;
            }
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE && quit_requested) {
                *quit_requested = true;
            }
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                unix_input_release_keys();
                unix_video_set_mouse_grab(false);
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (event.key.repeat) {
                break;
            }
            if (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod & (KMOD_CTRL | KMOD_GUI))) {
                if (quit_requested) {
                    *quit_requested = true;
                }
                break;
            }
            if (event.key.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_g &&
                (event.key.keysym.mod & (KMOD_CTRL | KMOD_GUI))) {
                unix_video_set_mouse_grab(false);
                break;
            }
            if (event.key.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && s_mouse_grabbed) {
                unix_video_set_mouse_grab(false);
            }
            unix_input_keyboard_key((int)event.key.keysym.scancode, event.key.type == SDL_KEYDOWN);
            break;
        case SDL_MOUSEMOTION:
            if (s_mouse_grabbed) {
                unix_input_mouse_motion(event.motion.xrel, event.motion.yrel);
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            int button = unix_mouse_button_from_sdl(event.button.button);
            if (button >= 0) {
                if (event.type == SDL_MOUSEBUTTONDOWN && !s_mouse_grabbed) {
                    unix_video_set_mouse_grab(true);
                }
                unix_input_mouse_button(button, event.type == SDL_MOUSEBUTTONDOWN);
            }
            break;
        }
        case SDL_MOUSEWHEEL:
            if (s_mouse_grabbed) {
                unix_input_mouse_wheel(event.wheel.x, event.wheel.y);
            }
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
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
}

void unix_video_set_title(const TCHAR *title)
{
    if (s_window && title) {
        SDL_SetWindowTitle(s_window, title);
    }
}

void unix_video_set_mouse_grab(bool grab)
{
    s_mouse_grabbed = grab;
    unix_input_set_mouse_active(grab);

    if (!s_setup_done || !s_available) {
        return;
    }

    SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE);
    if (s_window) {
        SDL_SetWindowGrab(s_window, grab ? SDL_TRUE : SDL_FALSE);
        SDL_CaptureMouse(grab ? SDL_TRUE : SDL_FALSE);
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
    SDL_DisplayMode mode;
    SDL_Rect usable;
    int display = 0;

    if (x) {
        *x = 0;
    }
    if (y) {
        *y = 0;
    }
    if (SDL_GetCurrentDisplayMode(display, &mode) == 0) {
        if (dw) {
            *dw = mode.w;
        }
        if (dh) {
            *dh = mode.h;
        }
    } else {
        if (dw) {
            *dw = 640;
        }
        if (dh) {
            *dh = 480;
        }
    }

    if (SDL_GetDisplayUsableBounds(display, &usable) == 0) {
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
