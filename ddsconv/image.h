#ifndef IMAGE_H__
#define IMAGE_H__

#include <cstdint>

#include <memory>

namespace util {

class Image {
public:
    Image() noexcept = default;
    Image(size_t w, size_t h, size_t stride, size_t bitsPerPixel);

    void reset();

    void set(void* data, size_t w, size_t h, size_t stride, size_t bitsPerPixel);

    void copy(Image const& src) noexcept;

    bool isValid() const noexcept { return mData != nullptr; }

    size_t getWidth() const noexcept { return mWidth; }

    size_t getHeight() const noexcept { return mHeight; }

    size_t getBytesPerRow() const noexcept { return mBpr; }

    size_t getBytesPerPixel() const noexcept { return mBpp; }

    void* getData() const noexcept { return mData; }

    void* getPixelRef(size_t x, size_t y) const noexcept { return static_cast<uint8_t*>(mData) + y * getBytesPerRow() + x * getBytesPerPixel(); }

private:
    size_t mWidth = 0;
    size_t mHeight = 0;
    size_t mBpr = 0;
    size_t mBpp = 0;
    std::unique_ptr<uint8_t[]> mOwnedData;
    void* mData = nullptr;
};

}

#endif
