#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "parallel.h"

static FILE *printer_output;
static bool printer_is_pipe;

static bool starts_with(const char *s, const char *prefix)
{
	return strncmp(s, prefix, strlen(prefix)) == 0;
}

static bool openprinter(void)
{
	if (printer_output) {
		return true;
	}
	if (!currprefs.prtname[0] || !_tcsicmp(currprefs.prtname, _T("none"))) {
		return false;
	}

	const char *spec = currprefs.prtname;
	if (!_tcsicmp(currprefs.prtname, _T("default"))) {
		spec = DEFPRTNAME;
	}

	if (spec[0] == '|') {
		const char *command = spec + 1;
		while (*command && isspace((unsigned char)*command)) {
			command++;
		}
		if (!*command) {
			write_log(_T("PARALLEL: empty printer command '%s'\n"), currprefs.prtname);
			return false;
		}
		printer_output = popen(command, "w");
		printer_is_pipe = true;
	} else if (!strcmp(spec, "lpr") || starts_with(spec, "lpr ") || !strcmp(spec, "lp") || starts_with(spec, "lp ")) {
		printer_output = popen(spec, "w");
		printer_is_pipe = true;
	} else {
		TCHAR path[MAX_DPATH];
		target_expand_environment(spec, path, MAX_DPATH);
		printer_output = fopen(path, "ab");
		printer_is_pipe = false;
	}

	if (!printer_output) {
		write_log(_T("PARALLEL: failed to open printer target '%s': %s\n"), currprefs.prtname, strerror(errno));
		return false;
	}
	write_log(_T("PARALLEL: printer output opened: %s\n"), currprefs.prtname);
	return true;
}

int isprinter(void)
{
	return currprefs.prtname[0] != 0 && _tcsicmp(currprefs.prtname, _T("none")) != 0;
}

void doprinter(uae_u8 val)
{
	if (!openprinter()) {
		return;
	}
	if (fputc(val, printer_output) == EOF) {
		write_log(_T("PARALLEL: printer write failed: %s\n"), strerror(errno));
		closeprinter();
	}
}

void flushprinter(void)
{
	if (printer_output) {
		fflush(printer_output);
	}
}

void closeprinter(void)
{
	if (!printer_output) {
		return;
	}
	flushprinter();
	if (printer_is_pipe) {
		pclose(printer_output);
	} else {
		fclose(printer_output);
	}
	printer_output = NULL;
	printer_is_pipe = false;
}

int isprinteropen(void)
{
	return printer_output != NULL;
}

void initparallel(void)
{
	closeprinter();
}

int parallel_direct_write_data(uae_u8, uae_u8)
{
	return 0;
}

int parallel_direct_read_data(uae_u8 *)
{
	return 0;
}

int parallel_direct_write_status(uae_u8, uae_u8)
{
	return 0;
}

int parallel_direct_read_status(uae_u8 *)
{
	return 0;
}
