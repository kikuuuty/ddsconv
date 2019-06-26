#ifndef IMAGE_H__
#define IMAGE_H__

#include <cstdint>

#include <memory>

namespace util
{
    class Image
    {
    public:
        Image();
        Image(size_t w, size_t h, size_t stride, size_t bitsPerPixel);

        void reset();

        void set(void* data, size_t w, size_t h, size_t stride, size_t bitsPerPixel);

        void copy(Image const& src);

        bool isValid() const { return mData != nullptr; }

        size_t getWidth() const { return mWidth; }

        size_t getHeight() const { return mHeight; }

        size_t getBytesPerRow() const { return mBpr; }

        size_t getBytesPerPixel() const { return mBpp; }

        void* getData() const { return mData; }

        void* getPixelRef(size_t x, size_t y) const { return static_cast<uint8_t*>(mData) + y * getBytesPerRow() + x * getBytesPerPixel(); }

    private:
        size_t mWidth;
        size_t mHeight;
        size_t mBpr;
        size_t mBpp;
        std::unique_ptr<uint8_t[]> mOwnedData;
        void* mData;
    };
}

#endif
