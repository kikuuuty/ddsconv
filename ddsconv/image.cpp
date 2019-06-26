#include "image.h"

#include <algorithm>

namespace util
{
    Image::Image()
        : mWidth(0)
        , mHeight(0)
        , mBpr(0)
        , mBpp(0)
        , mData(nullptr)
    {
    }

    Image::Image(size_t w, size_t h, size_t stride, size_t bitsPerPixel)
        : mWidth(w)
        , mHeight(h)
        , mBpr(stride)
        , mBpp(bitsPerPixel >> 3)
        , mOwnedData(new uint8_t[mBpr * h])
        , mData(mOwnedData.get())
    {
    }

    void Image::reset()
    {
        mOwnedData.release();
        mWidth = 0;
        mHeight = 0;
        mBpr = 0;
        mBpp = 0;
        mData = nullptr;
    }

    void Image::set(void* data, size_t w, size_t h, size_t stride, size_t bitsPerPixel)
    {
        mOwnedData.release();
        mWidth = w;
        mHeight = h;
        mBpr = stride;
        mBpp = bitsPerPixel >> 3;
        mData = data;
    }

    void Image::copy(Image const& src)
    {
        size_t width = std::min(getWidth(), src.getWidth());
        size_t height = std::min(getHeight(), src.getHeight());
        size_t stride = std::min(getBytesPerRow(), src.getBytesPerRow());

        static const size_t uSrc[] = {0, 0, 0, 1};

        for (size_t y = 0; y < height; ++y) {
            memcpy(getPixelRef(0, y), src.getPixelRef(0, y), stride);

            size_t base = getWidth() - 4;
            for (size_t x = width, count = getWidth(); x < count; ++x) {
                size_t sx = base + uSrc[x & 3];
                memcpy(getPixelRef(x, y), getPixelRef(sx, y), getBytesPerPixel());
            }
        }

        size_t base = getHeight() - 4;
        for (size_t y = height, count = getHeight(); y < count; ++y) {
            size_t sy = base + uSrc[y & 3];
            memcpy(getPixelRef(0, y), getPixelRef(0, sy), getBytesPerRow());
        }
    }
}
