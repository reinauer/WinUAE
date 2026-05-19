#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "xwin.h"
#include "fsdb.h"

#include <errno.h>
#include <vector>

#ifdef WINUAE_UNIX_WITH_LIBPNG
#include <png.h>
#endif

static void unix_tcslcpy(TCHAR *dst, const TCHAR *src, size_t size)
{
    if (!dst || !size) {
        return;
    }
    if (!src) {
        src = _T("");
    }
    _tcsncpy(dst, src, size - 1);
    dst[size - 1] = 0;
}

static void unix_bmp_put_word(FILE *fp, uae_u16 value)
{
    fputc(value & 0xff, fp);
    fputc((value >> 8) & 0xff, fp);
}

static void unix_bmp_put_long(FILE *fp, uae_u32 value)
{
    fputc(value & 0xff, fp);
    fputc((value >> 8) & 0xff, fp);
    fputc((value >> 16) & 0xff, fp);
    fputc((value >> 24) & 0xff, fp);
}

static bool unix_ensure_directory(const TCHAR *path)
{
    TCHAR tmp[MAX_DPATH];

    if (!path || !path[0]) {
        return false;
    }
    if (my_existsdir(path)) {
        return true;
    }

    unix_tcslcpy(tmp, path, sizeof tmp / sizeof(TCHAR));
    int len = (int)_tcslen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[--len] = 0;
    }

    for (TCHAR *p = tmp + 1; *p; p++) {
        if (*p != '/') {
            continue;
        }
        *p = 0;
        if (tmp[0] && !my_existsdir(tmp) && my_mkdir(tmp) < 0 && errno != EEXIST) {
            *p = '/';
            return false;
        }
        *p = '/';
    }

    return my_existsdir(tmp) || my_mkdir(tmp) == 0 || errno == EEXIST;
}

static void unix_screenshot_base_name(TCHAR *out, int out_size)
{
    const TCHAR *name = NULL;

    if (currprefs.floppyslots[0].dfxtype >= 0 && currprefs.floppyslots[0].df[0]) {
        name = currprefs.floppyslots[0].df;
    } else if (currprefs.cdslots[0].inuse && currprefs.cdslots[0].name[0]) {
        name = currprefs.cdslots[0].name;
    }

    if (name) {
        getfilepart(out, out_size, name);
        TCHAR *dot = _tcsrchr(out, '.');
        if (dot) {
            *dot = 0;
        }
    } else {
        unix_tcslcpy(out, _T("WinUAE"), out_size);
    }

    if (!out[0]) {
        unix_tcslcpy(out, _T("WinUAE"), out_size);
    }
    for (TCHAR *p = out; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '?' || *p == '*') {
            *p = '_';
        }
    }
}

static bool unix_write_bmp(const TCHAR *filename, const struct vidbuffer *vb)
{
    if (!filename || !vb || !vb->bufmem || vb->outwidth <= 0 || vb->outheight <= 0 || vb->rowbytes <= 0) {
        return false;
    }
    if (vb->pixbytes != 4 && vb->pixbytes != 2) {
        write_log(_T("Unix screenshot: unsupported pixel size %d\n"), vb->pixbytes);
        return false;
    }

    const int width = vb->outwidth;
    const int height = vb->outheight;
    const int bmp_rowbytes = (width * 3 + 3) & ~3;
    const uae_u32 image_size = (uae_u32)bmp_rowbytes * (uae_u32)height;
    const uae_u32 file_size = 14 + 40 + image_size;

    FILE *fp = _tfopen(filename, _T("wb"));
    if (!fp) {
        write_log(_T("Unix screenshot: can't open '%s'\n"), filename);
        return false;
    }

    unix_bmp_put_word(fp, 0x4d42);
    unix_bmp_put_long(fp, file_size);
    unix_bmp_put_word(fp, 0);
    unix_bmp_put_word(fp, 0);
    unix_bmp_put_long(fp, 14 + 40);
    unix_bmp_put_long(fp, 40);
    unix_bmp_put_long(fp, (uae_u32)width);
    unix_bmp_put_long(fp, (uae_u32)height);
    unix_bmp_put_word(fp, 1);
    unix_bmp_put_word(fp, 24);
    unix_bmp_put_long(fp, 0);
    unix_bmp_put_long(fp, image_size);
    unix_bmp_put_long(fp, 0);
    unix_bmp_put_long(fp, 0);
    unix_bmp_put_long(fp, 0);
    unix_bmp_put_long(fp, 0);

    std::vector<uae_u8> row((size_t)bmp_rowbytes);
    for (int y = height - 1; y >= 0; y--) {
        const uae_u8 *src = vb->bufmem + (size_t)y * (size_t)vb->rowbytes;
        memset(row.data(), 0, row.size());
        for (int x = 0; x < width; x++) {
            uae_u8 r, g, b;
            if (vb->pixbytes == 4) {
                const uae_u32 pixel = ((const uae_u32 *)src)[x];
                b = pixel & 0xff;
                g = (pixel >> 8) & 0xff;
                r = (pixel >> 16) & 0xff;
            } else {
                const uae_u16 pixel = (uae_u16)src[x * 2] | ((uae_u16)src[x * 2 + 1] << 8);
                r = (uae_u8)((((pixel >> 11) & 0x1f) * 255) / 31);
                g = (uae_u8)((((pixel >> 5) & 0x3f) * 255) / 63);
                b = (uae_u8)(((pixel & 0x1f) * 255) / 31);
            }
            row[(size_t)x * 3 + 0] = b;
            row[(size_t)x * 3 + 1] = g;
            row[(size_t)x * 3 + 2] = r;
        }
        if (fwrite(row.data(), 1, row.size(), fp) != row.size()) {
            fclose(fp);
            _tunlink(filename);
            write_log(_T("Unix screenshot: failed writing '%s'\n"), filename);
            return false;
        }
    }

    fclose(fp);
    return true;
}

#ifdef WINUAE_UNIX_WITH_LIBPNG
static bool unix_write_png(const TCHAR *filename, const struct vidbuffer *vb)
{
    if (!filename || !vb || !vb->bufmem || vb->outwidth <= 0 || vb->outheight <= 0 || vb->rowbytes <= 0) {
        return false;
    }
    if (vb->pixbytes != 4 && vb->pixbytes != 2) {
        write_log(_T("Unix screenshot: unsupported pixel size %d\n"), vb->pixbytes);
        return false;
    }

    FILE *fp = _tfopen(filename, _T("wb"));
    if (!fp) {
        write_log(_T("Unix screenshot: can't open '%s'\n"), filename);
        return false;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return false;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return false;
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        _tunlink(filename);
        write_log(_T("Unix screenshot: failed writing '%s'\n"), filename);
        return false;
    }

    const int width = vb->outwidth;
    const int height = vb->outheight;
    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    std::vector<uae_u8> row((size_t)width * 3);
    for (int y = 0; y < height; y++) {
        const uae_u8 *src = vb->bufmem + (size_t)y * (size_t)vb->rowbytes;
        for (int x = 0; x < width; x++) {
            uae_u8 r, g, b;
            if (vb->pixbytes == 4) {
                const uae_u32 pixel = ((const uae_u32 *)src)[x];
                b = pixel & 0xff;
                g = (pixel >> 8) & 0xff;
                r = (pixel >> 16) & 0xff;
            } else {
                const uae_u16 pixel = (uae_u16)src[x * 2] | ((uae_u16)src[x * 2 + 1] << 8);
                r = (uae_u8)((((pixel >> 11) & 0x1f) * 255) / 31);
                g = (uae_u8)((((pixel >> 5) & 0x3f) * 255) / 63);
                b = (uae_u8)(((pixel & 0x1f) * 255) / 31);
            }
            row[(size_t)x * 3 + 0] = r;
            row[(size_t)x * 3 + 1] = g;
            row[(size_t)x * 3 + 2] = b;
        }
        png_write_row(png_ptr, row.data());
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return true;
}
#endif

static bool unix_save_screenshot_file(int monid)
{
    TCHAR path[MAX_DPATH];
    TCHAR base[MAX_DPATH];
    TCHAR filename[MAX_DPATH];

    if (monid < 0 || monid >= MAX_AMIGADISPLAYS) {
        monid = 0;
    }
    struct vidbuf_description *vidinfo = &adisplays[monid].gfxvidinfo;
    struct vidbuffer *vb = vidinfo->inbuffer ? vidinfo->inbuffer : &vidinfo->drawbuffer;
    if (!vb || !vb->bufmem || vb->outwidth <= 0 || vb->outheight <= 0) {
        write_log(_T("Unix screenshot: no active video buffer\n"));
        return false;
    }

    fetch_screenshotpath(path, sizeof path / sizeof(TCHAR));
    if (!unix_ensure_directory(path)) {
        write_log(_T("Unix screenshot: can't create screenshot directory '%s'\n"), path);
        return false;
    }
    unix_screenshot_base_name(base, sizeof base / sizeof(TCHAR));

    for (int i = 1; i < 100000; i++) {
#ifdef WINUAE_UNIX_WITH_LIBPNG
        _sntprintf(filename, sizeof filename / sizeof(TCHAR), _T("%s%s_%05d.png"), path, base, i);
#else
        _sntprintf(filename, sizeof filename / sizeof(TCHAR), _T("%s%s_%05d.bmp"), path, base, i);
#endif
        FILE *existing = _tfopen(filename, _T("rb"));
        if (existing) {
            fclose(existing);
            continue;
        }
#ifdef WINUAE_UNIX_WITH_LIBPNG
        if (!unix_write_png(filename, vb)) {
            return false;
        }
#else
        if (!unix_write_bmp(filename, vb)) {
            return false;
        }
#endif
        write_log(_T("Screenshot saved as \"%s\"\n"), filename);
        return true;
    }

    write_log(_T("Unix screenshot: no free filename in '%s'\n"), path);
    return false;
}

void screenshot(int monid, int mode, int)
{
    if (mode == 0) {
        write_log(_T("Unix screenshot: clipboard screenshots are not implemented yet\n"));
        return;
    }
    if (mode == 2 || mode == 3) {
        write_log(_T("Unix screenshot: continuous screenshots are not implemented yet\n"));
        return;
    }
    if (mode == 4) {
        return;
    }
    unix_save_screenshot_file(monid);
}
