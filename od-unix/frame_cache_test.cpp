#include "frame_cache.h"
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char *what)
{
    if (!ok) { std::fprintf(stderr, "%s\n", what); std::exit(1); }
}

int main()
{
    unix_frame_cache cache;
    uint8_t frame[48] = {}, other[48] = {};
    check(!cache.matches(frame, 3, 3, 16, 4), "first frame must present");
    cache.presented(frame, 3, 3, 16, 4);
    check(cache.matches(other, 3, 3, 16, 4), "same pixels at a different address");
    other[12] = 42;
    check(cache.matches(other, 3, 3, 16, 4), "ignore row padding");
    other[43] = 1;
    check(!cache.matches(other, 3, 3, 16, 4), "detect last visible pixel");
    check(!cache.matches(other, 3, 3, 16, 4), "failed present must retry");
    cache.presented(other, 3, 3, 16, 4);
    check(cache.matches(other, 3, 3, 16, 4), "remember successful present");
    cache.invalidate();
    check(!cache.matches(other, 3, 3, 16, 4), "exposure must repaint");
    cache.presented(other, 3, 3, 16, 4);
    check(!cache.matches(other, 6, 3, 16, 2), "same bytes with new pixel format");
    check(!cache.matches(other, 3, 2, 16, 4), "height change must repaint");
    cache.presented(frame, 3, 3, 16, 2);
    check(cache.matches(frame, 3, 3, 16, 2), "16-bit frame");
    cache.presented(nullptr, 3, 3, 16, 2);
    check(!cache.matches(frame, 3, 3, 16, 2), "invalid capture invalidates");
    cache.presented(frame, 3, 3, 16, 2);
    check(!cache.matches(frame, 3, 3, 5, 2), "reject truncated pitch");
    unsigned presents = 0;
    cache.invalidate();
    for (unsigned i = 0; i < 300; i++) {
        if (i == 100 || i == 200) frame[0]++;
        if (!cache.matches(frame, 3, 3, 16, 2)) {
            cache.presented(frame, 3, 3, 16, 2); presents++;
        }
    }
    check(presents == 3, "static frames skip; changed frames present immediately");
    std::puts("frame cache: pixel changes, repaint, format, pitch and retries passed");
}
