#include "sysconfig.h"
#include "sysdeps.h"
#include "uae/api.h"
#include "uae/dlopen.h"
#include "uae/log.h"

#ifdef _WIN32
#include "windows.h"
#else
#include <dlfcn.h>
#if defined(UAE_HOST_DARWIN)
#include <mach-o/dyld.h>
#elif defined(UAE_HOST_LINUX)
#include <unistd.h>
#endif
#endif
#ifdef WINUAE
#include "od-win32/win32.h"
#endif

UAE_DLHANDLE uae_dlopen(const TCHAR *path)
{
	UAE_DLHANDLE result;
	if (path == NULL || path[0] == _T('\0')) {
		write_log(_T("DLOPEN: No path given\n"));
		return NULL;
	}
#ifdef _WIN32
	result = LoadLibrary(path);
#else
	result = dlopen(path, RTLD_NOW);
	const char *error = dlerror();
	if (error != NULL)  {
		write_log("DLOPEN: %s\n", error);
	}
#endif
	if (result == NULL) {
		write_log("DLOPEN: Failed to open %s\n", path);
	}
	return result;
}

void *uae_dlsym(UAE_DLHANDLE handle, const char *name)
{
#if 0
	if (handle == NULL) {
		return NULL;
	}
#endif
#ifdef _WIN32
	return (void *) GetProcAddress(handle, name);
#else
	return dlsym(handle, name);
#endif
}

void uae_dlclose(UAE_DLHANDLE handle)
{
#ifdef _WIN32
	FreeLibrary (handle);
#else
	dlclose(handle);
#endif
}

#if defined(UAE_TARGET_UNIX) && !defined(_WIN32)
static bool get_executable_dir(TCHAR *out, size_t out_size)
{
#if defined(UAE_HOST_DARWIN)
	uint32_t size = out_size;
	if (_NSGetExecutablePath(out, &size) != 0) {
		return false;
	}
#elif defined(UAE_HOST_LINUX)
	ssize_t len = readlink("/proc/self/exe", out, out_size - 1);
	if (len <= 0 || (size_t)len >= out_size) {
		return false;
	}
	out[len] = 0;
#else
	return false;
#endif
	TCHAR *slash = _tcsrchr(out, FSDB_DIR_SEPARATOR);
	if (!slash) {
		return false;
	}
	*slash = 0;
	return true;
}

static bool append_plugin_path(TCHAR *path, size_t path_size,
	const TCHAR *prefix, const TCHAR *name, const TCHAR *suffix)
{
	if (_tcslen(prefix) + _tcslen(name) + _tcslen(suffix) >= path_size) {
		return false;
	}
	_tcscpy(path, prefix);
	_tcscat(path, name);
	_tcscat(path, suffix);
	return true;
}

static UAE_DLHANDLE try_unix_plugin_path(const TCHAR *path,
	const char **last_error)
{
	dlerror();
	UAE_DLHANDLE handle = dlopen(path, RTLD_NOW);
	if (!handle && last_error) {
		*last_error = dlerror();
	}
	return handle;
}

static UAE_DLHANDLE uae_dlopen_plugin_unix(const TCHAR *name, TCHAR *loaded_path)
{
	const char *last_error = NULL;
	TCHAR executable_dir[MAX_DPATH] = { 0 };
	bool have_executable_dir = get_executable_dir(executable_dir,
		MAX_DPATH);

#if defined(UAE_HOST_DARWIN)
	const TCHAR *suffixes[] = { _T(".so"), LT_MODULE_EXT, NULL };
	const TCHAR *prefixes[] = {
		_T("@executable_path/../PlugIns/"),
		_T("@executable_path/"),
		_T("./plugins/"),
		_T("./"),
		_T(""),
		NULL
	};
#else
	const TCHAR *suffixes[] = { LT_MODULE_EXT, NULL };
	const TCHAR *prefixes[] = {
		_T("./plugins/"),
		_T("./"),
		_T(""),
		NULL
	};
#endif

	for (int s = 0; suffixes[s]; s++) {
		for (int p = 0; prefixes[p]; p++) {
			TCHAR path[MAX_DPATH];
			if (!append_plugin_path(path, MAX_DPATH, prefixes[p], name,
				suffixes[s])) {
				continue;
			}
			UAE_DLHANDLE handle = try_unix_plugin_path(path, &last_error);
			if (handle) {
				_tcscpy(loaded_path, path);
				return handle;
			}
		}
		if (have_executable_dir) {
			TCHAR prefix[MAX_DPATH];
			if (_tcslen(executable_dir) + 2 < MAX_DPATH) {
				_tcscpy(prefix, executable_dir);
				_tcscat(prefix, _T("/"));
				TCHAR path[MAX_DPATH];
				if (append_plugin_path(path, MAX_DPATH, prefix, name,
					suffixes[s])) {
					UAE_DLHANDLE handle = try_unix_plugin_path(path,
						&last_error);
					if (handle) {
						_tcscpy(loaded_path, path);
						return handle;
					}
				}
			}
			if (_tcslen(executable_dir) + 10 < MAX_DPATH) {
				_tcscpy(prefix, executable_dir);
				_tcscat(prefix, _T("/plugins/"));
				TCHAR path[MAX_DPATH];
				if (append_plugin_path(path, MAX_DPATH, prefix, name,
					suffixes[s])) {
					UAE_DLHANDLE handle = try_unix_plugin_path(path,
						&last_error);
					if (handle) {
						_tcscpy(loaded_path, path);
						return handle;
					}
				}
			}
		}
	}

	if (last_error) {
		write_log("DLOPEN: %s\n", last_error);
	}
	write_log(_T("DLOPEN: Could not find plugin \"%s\"\n"), name);
	return NULL;
}
#endif

UAE_DLHANDLE uae_dlopen_plugin(const TCHAR *name)
{
#if defined(FSUAE)
	const TCHAR *path = NULL;
	if (plugin_lookup) {
		path = plugin_lookup(name);
	}
	if (path == NULL or path[0] == _T('\0')) {
		write_log(_T("DLOPEN: Could not find plugin \"%s\"\n"), name);
		return NULL;
	}
	UAE_DLHANDLE handle = uae_dlopen(path);
#elif defined(WINUAE)
	TCHAR path[MAX_DPATH];
	_tcscpy(path, name);
	_tcscat(path, LT_MODULE_EXT);
	UAE_DLHANDLE handle = WIN32_LoadLibrary(path);
#elif defined(UAE_TARGET_UNIX)
	TCHAR path[MAX_DPATH] = { 0 };
	UAE_DLHANDLE handle = uae_dlopen_plugin_unix(name, path);
#else
	TCHAR path[MAX_DPATH];
	_tcscpy(path, name);
#ifdef _WIN64
	_tcscat(path, _T("_x64"));
#endif
	_tcscat(path, LT_MODULE_EXT);
	UAE_DLHANDLE handle = uae_dlopen(path);
#endif
	if (handle) {
		write_log(_T("DLOPEN: Loaded plugin %s\n"), path);
		uae_dlopen_patch_common(handle);
	}
	return handle;
}

void uae_dlopen_patch_common(UAE_DLHANDLE handle)
{
	void *ptr;
	ptr = uae_dlsym(handle, "uae_log");
	if (ptr) {
		write_log(_T("DLOPEN: Patching common functions\n"));
		*((uae_log_function *)ptr) = &uae_log;
	}
}
