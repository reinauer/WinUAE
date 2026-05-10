#include "sysconfig.h"
#include "sysdeps.h"

#include <stdarg.h>
#include <stdio.h>

int console_logging;
int always_flush_log;
TCHAR *conlogfile;
FILE *debugfile;

static void vlog_to_stderr(const char *format, va_list ap)
{
    vfprintf(stderr, format, ap);
    fflush(stderr);
}

void write_log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vlog_to_stderr(format, ap);
    va_end(ap);
}

void write_logx(const TCHAR *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vlog_to_stderr(format, ap);
    va_end(ap);
}

void write_dlog(const TCHAR *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vlog_to_stderr(format, ap);
    va_end(ap);
}

int read_log(void)
{
    return 0;
}

void flush_log(void)
{
    fflush(stderr);
}

void logging_init(void)
{
}

FILE *log_open(const TCHAR *name, int append, int, TCHAR*)
{
    return fopen(name, append ? "ab" : "wb");
}

void log_close(FILE *f)
{
    if (f) {
        fclose(f);
    }
}

TCHAR *setconsolemode(TCHAR *buffer, int)
{
    return buffer;
}

void close_console(void) {}
void open_console(void) {}
bool is_interactive_console(void) { return true; }
void reopen_console(void) {}
void activate_console(void) {}
void deactivate_console(void) {}
void set_console_input_mode(int) {}
bool is_console_open(void) { return true; }
void console_out(const TCHAR *s) { fputs(s, stderr); }
void console_out_f(const TCHAR *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vlog_to_stderr(format, ap);
    va_end(ap);
}
void console_flush(void) { fflush(stderr); }
int console_get(TCHAR *, int) { return 0; }
bool console_isch(void) { return false; }
TCHAR console_getch(void) { return 0; }
void f_out(void *, const TCHAR *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vlog_to_stderr(format, ap);
    va_end(ap);
}
TCHAR* buf_out(TCHAR *buffer, int *bufsize, const TCHAR *format, ...)
{
    va_list ap;
    va_start(ap, format);
    int n = vsnprintf(buffer, *bufsize, format, ap);
    va_end(ap);
    if (n >= 0 && n < *bufsize) {
        *bufsize -= n;
        return buffer + n;
    }
    return buffer;
}
TCHAR *write_log_get_ts(void)
{
    static TCHAR ts[1] = { 0 };
    return ts;
}
