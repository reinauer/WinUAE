#ifndef WINUAE_UNIX_FRAME_CACHE_H
#define WINUAE_UNIX_FRAME_CACHE_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

// Retain only pixels from a successful present. Padding is not visible.
class unix_frame_cache {
    std::vector<uint8_t> pixels;
    int width = 0, height = 0, pixbytes = 0;
    bool valid = false;

    static bool layout(const void *data, int w, int h, int pitch, int bpp,
        size_t &row, size_t &size)
    {
        if (!data || w <= 0 || h <= 0 || pitch <= 0 || (bpp != 2 && bpp != 4))
            return false;
        if (size_t(w) > std::numeric_limits<size_t>::max() / size_t(bpp))
            return false;
        row = size_t(w) * size_t(bpp);
        if (row > size_t(pitch) || size_t(h) > std::numeric_limits<size_t>::max() / size_t(pitch))
            return false;
        size = row * size_t(h);
        return true;
    }

public:
    void invalidate() { valid = false; }

    bool matches(const void *data, int w, int h, int pitch, int bpp) const
    {
        size_t row, size;
        if (!valid || w != width || h != height || bpp != pixbytes ||
            !layout(data, w, h, pitch, bpp, row, size))
            return false;
        const uint8_t *source = static_cast<const uint8_t *>(data);
        for (int y = 0; y < h; y++) {
            if (std::memcmp(pixels.data() + size_t(y) * row,
                source + size_t(y) * size_t(pitch), row))
                return false;
        }
        return true;
    }

    void presented(const void *data, int w, int h, int pitch, int bpp)
    {
        size_t row, size;
        valid = false;
        if (!layout(data, w, h, pitch, bpp, row, size))
            return;
        pixels.resize(size);
        const uint8_t *source = static_cast<const uint8_t *>(data);
        for (int y = 0; y < h; y++)
            std::memcpy(pixels.data() + size_t(y) * row,
                source + size_t(y) * size_t(pitch), row);
        width = w; height = h; pixbytes = bpp; valid = true;
    }
};

#endif
