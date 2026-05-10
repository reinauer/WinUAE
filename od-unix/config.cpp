#include "sysconfig.h"
#include "sysdeps.h"

#include <libgen.h>
#include <limits.h>
#include <unistd.h>

#include "options.h"
#include "uae/string.h"
#include "uae.h"
#include "zfile.h"

TCHAR start_path_data[MAX_DPATH];
TCHAR start_path_data_exe[MAX_DPATH];
TCHAR start_path_plugins[MAX_DPATH];
int saveimageoriginalpath;

int target_cfgfile_load(struct uae_prefs *p, const TCHAR *filename, int type, int isdefault)
{
    int loaded_type = type;
    return cfgfile_load(p, filename, &loaded_type, 0, !isdefault);
}

int target_parse_option(struct uae_prefs *, const TCHAR *, const TCHAR *, int)
{
    return 0;
}

void target_save_options(struct zfile*, struct uae_prefs*)
{
}

void target_default_options(struct uae_prefs*, int)
{
}

void target_fixup_options(struct uae_prefs*)
{
}

void target_multipath_modified(struct uae_prefs*)
{
}

bool target_isrelativemode(void)
{
    return false;
}

bool get_plugin_path(TCHAR *out, int size, const TCHAR *path)
{
    uae_tcslcpy(out, path, size);
    return true;
}

void stripslashes(TCHAR *p)
{
    while (*p) {
        if (*p == '\\') {
            *p = '/';
        }
        p++;
    }
}

void fixtrailing(TCHAR *p)
{
    int len = _tcslen(p);
    if (len > 0 && p[len - 1] != '/') {
        _tcscat(p, "/");
    }
}

void fullpath(TCHAR *path, int size)
{
    fullpath(path, size, false);
}

void fullpath(TCHAR *path, int size, bool)
{
    char tmp[PATH_MAX];
    if (realpath(path, tmp)) {
        uae_tcslcpy(path, tmp, size);
    }
}

void getpathpart(TCHAR *outpath, int size, const TCHAR *inpath)
{
    uae_tcslcpy(outpath, inpath, size);
    TCHAR *slash = _tcsrchr(outpath, '/');
    if (slash) {
        slash[1] = 0;
    } else {
        outpath[0] = 0;
    }
}

void getfilepart(TCHAR *out, int size, const TCHAR *path)
{
    const TCHAR *slash = _tcsrchr(path, '/');
    uae_tcslcpy(out, slash ? slash + 1 : path, size);
}

bool samepath(const TCHAR *p1, const TCHAR *p2)
{
    return _tcscmp(p1, p2) == 0;
}

static void fetch_home_path(TCHAR *out, int size)
{
    const char *home = getenv("HOME");
    uae_tcslcpy(out, home ? home : ".", size);
    fixtrailing(out);
}

void fetch_saveimagepath(TCHAR *out, int size, int) { fetch_home_path(out, size); }
void fetch_configurationpath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_nvrampath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_luapath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_screenshotpath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_ripperpath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_statefilepath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_inputfilepath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_datapath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_rompath(TCHAR *out, int size) { fetch_home_path(out, size); }
void fetch_videopath(TCHAR *out, int size) { fetch_home_path(out, size); }

void target_getdate(int *y, int *m, int *d)
{
    *y = 2026;
    *m = 5;
    *d = 10;
}
