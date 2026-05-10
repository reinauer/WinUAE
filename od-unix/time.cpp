#include "sysconfig.h"
#include "sysdeps.h"

#include <mach/mach_time.h>

#include "uae/time.h"

static uint64_t epoch_ns;
static mach_timebase_info_data_t timebase;

static uint64_t monotonic_ns(void)
{
    if (!timebase.denom) {
        mach_timebase_info(&timebase);
    }
    uint64_t t = mach_absolute_time();
    return t * timebase.numer / timebase.denom;
}

void uae_time_init(void)
{
    epoch_ns = monotonic_ns();
    syncbase = 1000000;
}

void uae_time_calibrate(void)
{
    syncbase = 1000000;
}

uae_time_t uae_time(void)
{
    if (!epoch_ns) {
        uae_time_init();
    }
    return (uae_time_t)((monotonic_ns() - epoch_ns) / 1000);
}

int64_t uae_time_us(void)
{
    if (!epoch_ns) {
        uae_time_init();
    }
    return (int64_t)((monotonic_ns() - epoch_ns) / 1000);
}

int64_t uae_time_ns(void)
{
    if (!epoch_ns) {
        uae_time_init();
    }
    return (int64_t)(monotonic_ns() - epoch_ns);
}

void uae_time_use_rdtsc(bool)
{
}
