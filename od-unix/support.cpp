#include "sysconfig.h"
#include "sysdeps.h"

#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "options.h"
#include "uae.h"

uae_u8 *natmem_offset;
uae_u8 *natmem_reserved;
uae_u32 natmem_reserved_size;

int machdep_init(void)
{
    return 1;
}

int sleep_resolution;

int sleep_millis(int ms)
{
    if (ms <= 0) {
        return 0;
    }
    usleep((useconds_t)ms * 1000);
    return 0;
}

int sleep_millis_main(int ms)
{
    return sleep_millis(ms);
}

int sleep_millis_amiga(int ms)
{
    return sleep_millis(ms);
}

void sleep_cpu_wakeup(void)
{
}

int target_sleep_nanos(int ns)
{
    if (ns <= 0) {
        return 0;
    }
    struct timespec ts;
    ts.tv_sec = ns / 1000000000;
    ts.tv_nsec = ns % 1000000000;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
    }
    return 0;
}

uae_atomic atomic_and(volatile uae_atomic *p, uae_u32 v)
{
    return __sync_and_and_fetch(p, (uae_atomic)v);
}

uae_atomic atomic_or(volatile uae_atomic *p, uae_u32 v)
{
    return __sync_or_and_fetch(p, (uae_atomic)v);
}

uae_atomic atomic_inc(volatile uae_atomic *p)
{
    return __sync_add_and_fetch(p, 1);
}

uae_atomic atomic_dec(volatile uae_atomic *p)
{
    return __sync_sub_and_fetch(p, 1);
}

uae_u32 atomic_bit_test_and_reset(volatile uae_atomic *p, uae_u32 v)
{
    uae_atomic mask = (uae_atomic)1 << v;
    uae_atomic old = __sync_fetch_and_and(p, ~mask);
    return (old & mask) != 0;
}

uae_u32 getlocaltime(void)
{
    return (uae_u32)time(NULL);
}

void target_run(void) {}
void target_quit(void) {}
void target_restart(void) {}
void target_reset(void) {}
void target_cpu_speed(void) {}
void target_addtorecent(const TCHAR*, int) {}
void target_setdefaultstatefilename(const TCHAR*) {}
bool target_osd_keyboard(int) { return false; }
void target_osk_control(int, int, int, int) {}
bool isguiactive(void) { return false; }
bool is_mainthread(void) { return true; }
void fpux_save(int*) {}
void fpux_restore(int*) {}
