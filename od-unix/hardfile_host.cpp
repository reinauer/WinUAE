#include "sysconfig.h"
#include "sysdeps.h"

#include <unistd.h>

#include "options.h"
#include "filesys.h"
#include "uae.h"
#include "uae/io.h"

struct hardfilehandle {
    FILE *file;
};

int hdf_init_target(void)
{
    return 1;
}

int hdf_open_target(struct hardfiledata *hfd, const TCHAR *name)
{
    if (!hfd || !name) {
        return 0;
    }
    hdf_close_target(hfd);
    hfd->handle = xcalloc(hardfilehandle, 1);
    hfd->cache = xcalloc(uae_u8, 16384);
    hfd->cache_valid = 0;
    hfd->handle->file = uae_tfopen(name, hfd->ci.readonly ? _T("rb") : _T("r+b"));
    if (!hfd->handle->file && !hfd->ci.readonly) {
        hfd->ci.readonly = true;
        hfd->handle->file = uae_tfopen(name, _T("rb"));
    }
    if (!hfd->handle->file) {
        hdf_close_target(hfd);
        return 0;
    }
    fseeko(hfd->handle->file, 0, SEEK_END);
    hfd->physsize = hfd->virtsize = (uae_u64)ftello(hfd->handle->file);
    fseeko(hfd->handle->file, 0, SEEK_SET);
    hfd->offset = 0;
    hfd->handle_valid = 1;
    if (hfd->ci.blocksize <= 0) {
        hfd->ci.blocksize = 512;
    }
    return 1;
}

int hdf_dup_target(struct hardfiledata *dhfd, const struct hardfiledata *shfd)
{
    if (!dhfd || !shfd) {
        return 0;
    }
    *dhfd = *shfd;
    dhfd->handle = NULL;
    dhfd->cache = NULL;
    return hdf_open_target(dhfd, shfd->ci.rootdir);
}

void hdf_close_target(struct hardfiledata *hfd)
{
    if (!hfd) {
        return;
    }
    if (hfd->handle) {
        if (hfd->handle->file) {
            fclose(hfd->handle->file);
        }
        xfree(hfd->handle);
        hfd->handle = NULL;
    }
    xfree(hfd->cache);
    hfd->cache = NULL;
    hfd->cache_valid = 0;
    hfd->handle_valid = 0;
}

int hdf_read_target(struct hardfiledata *hfd, void *buffer, uae_u64 offset, int len, uae_u32 *error)
{
    if (error) {
        *error = 0;
    }
    if (!hfd || !hfd->handle || !hfd->handle->file || fseeko(hfd->handle->file, (off_t)(offset + hfd->offset), SEEK_SET) != 0) {
        if (error) {
            *error = errno;
        }
        return 0;
    }
    return (int)fread(buffer, 1, len, hfd->handle->file);
}

int hdf_write_target(struct hardfiledata *hfd, void *buffer, uae_u64 offset, int len, uae_u32 *error)
{
    if (error) {
        *error = 0;
    }
    if (!hfd || hfd->ci.readonly || !hfd->handle || !hfd->handle->file || fseeko(hfd->handle->file, (off_t)(offset + hfd->offset), SEEK_SET) != 0) {
        if (error) {
            *error = errno ? errno : EACCES;
        }
        return 0;
    }
    return (int)fwrite(buffer, 1, len, hfd->handle->file);
}

int hdf_resize_target(struct hardfiledata *hfd, uae_u64 newsize)
{
    if (!hfd || !hfd->handle || !hfd->handle->file) {
        return 0;
    }
    int fd = fileno(hfd->handle->file);
    if (ftruncate(fd, (off_t)newsize) != 0) {
        return 0;
    }
    hfd->physsize = hfd->virtsize = newsize;
    return 1;
}

int get_guid_target(uae_u8 *out)
{
    if (!out) {
        return 0;
    }
    for (int i = 0; i < 16; i++) {
        out[i] = (uae_u8)uaerand();
    }
    return 1;
}
