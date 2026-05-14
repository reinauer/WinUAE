#include "sysconfig.h"
#include "sysdeps.h"

#ifdef DRIVESOUND

#include "driveclick.h"
#include "uae.h"
#include "zfile.h"

int driveclick_pcdrivemask;
int driveclick_pcdrivenum;

static const struct {
	const TCHAR *name;
	int slot;
} builtin_samples[] = {
	{ _T("drive_click.wav"), DS_CLICK },
	{ _T("drive_spin.wav"), DS_SPIN },
	{ _T("drive_spinnd.wav"), DS_SPINND },
	{ _T("drive_startup.wav"), DS_START },
	{ _T("drive_snatch.wav"), DS_SNATCH },
	{ NULL, -1 }
};

static bool load_sample_file(const TCHAR *path, struct drvsample *sample)
{
	struct zfile *zf = zfile_fopen(path, _T("rb"), ZFD_NORMAL);
	if (!zf) {
		return false;
	}
	const uae_s64 size = zfile_size(zf);
	if (size <= 0 || size > 16 * 1024 * 1024) {
		zfile_fclose(zf);
		return false;
	}
	uae_u8 *data = xmalloc(uae_u8, (size_t)size);
	if (!data) {
		zfile_fclose(zf);
		return false;
	}
	const size_t got = zfile_fread(data, 1, (size_t)size, zf);
	zfile_fclose(zf);
	if (got != (size_t)size) {
		xfree(data);
		return false;
	}
	int decoded_len = (int)size;
	sample->p = decodewav(data, &decoded_len);
	sample->len = decoded_len;
	xfree(data);
	return sample->p != NULL && sample->len > 0;
}

static bool load_builtin_sample(const TCHAR *name, struct drvsample *sample)
{
	TCHAR path[MAX_DPATH];
	const TCHAR *dirs[] = {
		_T(WINUAE_UNIX_SOURCE_DIR "/od-win32/resources/"),
		_T(WINUAE_UNIX_SOURCE_DIR "/resources/"),
		start_path_data,
		start_path_data_exe,
		NULL
	};

	for (int i = 0; dirs[i]; i++) {
		if (!dirs[i][0]) {
			continue;
		}
		_stprintf(path, _T("%s%s%s"), dirs[i], dirs[i][_tcslen(dirs[i]) - 1] == '/' ? _T("") : _T("/"), name);
		if (load_sample_file(path, sample)) {
			return true;
		}
	}
	return false;
}

int driveclick_loadresource(struct drvsample *sp, int)
{
	bool ok = true;
	for (int i = 0; builtin_samples[i].name; i++) {
		struct drvsample *sample = sp + builtin_samples[i].slot;
		if (!load_builtin_sample(builtin_samples[i].name, sample)) {
			ok = false;
		}
	}
	return ok ? 1 : 0;
}

void driveclick_fdrawcmd_seek(int, int)
{
}

void driveclick_fdrawcmd_motor(int, int)
{
}

void driveclick_fdrawcmd_vsync(void)
{
}

void driveclick_fdrawcmd_close(int)
{
}

int driveclick_fdrawcmd_open(int)
{
	return 0;
}

void driveclick_fdrawcmd_detect(void)
{
	driveclick_pcdrivemask = 0;
	driveclick_pcdrivenum = 0;
}

#endif
