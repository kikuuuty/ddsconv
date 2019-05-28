#include <cassert>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <map>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "DirectXTex.h"
#include "ispc_texcomp.h"

#define VERSION "1.0.0"

#define ABORT(msg) { puts(msg); return 1; }
#define ARG_CASE(s) if (kv.first == s)
#define ARG_CASE2(s1, s2) if (kv.first == s1 || kv.first == s2)
#define CHECK_NUM_ARGS(p) if (kv.second.size() < p) continue;

namespace
{
    const char helpText[] = 
        "\n"
        "ddsconv v" VERSION "\n"
        "---------------------------------------------------------------------\n"
        "  Usage: ddsconv <input specification> <options>\n"
        "\n"
        "INPUT SPECIFICATION\n"
        "  -i, --input <filename>\n"
            "\t入力ファイルパスを指定します。\n"
        "\n"
        "OPTIONS\n"
        "  -f, --format <format>\n"
            "\t圧縮フォーマットを指定します。\n"
            "\tbc1  - RGB画像、またはRGBA画像(1bitアルファ)。\n"
            "\tbc3  - RGBA画像(多階調アルファ)。\n"
            "\tbc5  - 2成分のデータ(法線マップなど)。DX10以降。\n"
            "\tbc6h - HDRのRGB画像。DX10以降。\n"
            "\tbc7  - RGB画像、またはRGBA画像(多階調アルファ)。DX10以降。\n"
        "  -h, --help\n"
            "\tこれを表示します。\n"
        "  -l, --level <level>\n"
            "\tBC7/BC6Hの圧縮速度と品質を指定します。初期値は\"ultrafast\"です。\n"
            "\tultrafast - 最高速度。\n"
            "\tveryfast  - 非常に高速。\n"
            "\tfast      - 高速。\n"
            "\tbasic     - 中間。\n"
            "\tslow      - 高品質。\n"
            "\tveryslow  - 最高品質。\n"
        "  -m, --miplevels <levels>\n"
            "\t元画像を含むミップマップレベルを指定します。\n"
            "\t0を指定した場合は1x1までのミップマップを出力します。\n"
        "  -o, --output <filename>\n"
            "\t出力ファイルパスを指定します。初期値は\"<input>.dds\"です。\n"
        "  -s, --srgb\n"
            "\t入力をsRGB色空間、出力をリニア色空間として処理します。\n"
        "  --forceRgb\n"
            "\tBC7の圧縮時にアルファチャンネルを無視します。\n"
            "\tわずかに圧縮速度が向上しますが、サイズには影響しません。\n"
        "  -v, --verbose\n"
            "\t詳細な出力を行います。\n"
        "\n";

    struct Level
    {
        enum Type
        {
            UltraFast,
            VeryFast,
            Fast,
            Basic,
            Slow,
            VerySlow,
        };
    };

    struct Spec
    {
        std::wstring source;
        std::wstring output;
        DXGI_FORMAT format = DXGI_FORMAT_BC7_UNORM;
        Level::Type level = Level::UltraFast;
        uint32_t mipLevels = 0;
        bool forceRgbSpecified = false;
        bool mipmapSpecified = false;
        bool srgbSpecified = false;
        bool verboseSpecified = false;
    };

    std::map<std::string, std::vector<std::string>> parseOptions(int argc, char* argv[])
    {
        std::map<std::string, std::vector<std::string>> options;
        int argPos = 1;
        bool suggestHelp = false;
        while (argPos < argc) {
            const char* arg = argv[argPos];
            if (arg[0] != '-') {
                printf("Unknown setting or insufficient parameters: %s\n", arg);
                suggestHelp = true;
            }
            else {
                int pos = argPos;
                while (pos+1 < argc) {
                    if (argv[pos+1][0] == '-') break;
                    ++pos;
                }
                int count = pos - argPos;
                if (count <= 0) {
                    options[arg].clear();
                    ++argPos;
                }
                else {
                    for (int i = argPos+1, end = i+count; i < end; ++i)
                        options[arg].push_back(argv[i]);
                    argPos += 1 + count;
                }
            }
        }
        if (suggestHelp) printf("Use --help for more information.\n");
        return options;
    }

    std::wstring utf8ToUtf16(const std::string& u8str)
    {
        int u16strLen = ::MultiByteToWideChar(CP_UTF8, 0, u8str.c_str(), -1, NULL, 0);
        if (u16strLen <= 0) return std::wstring();
        std::wstring u16str;
        u16str.resize(u16strLen-1);
        ::MultiByteToWideChar(CP_UTF8, 0, u8str.c_str(), -1, &u16str[0], u16strLen);
        return u16str;
    }

    std::string utf16ToUtf8(const std::wstring& u16str)
    {
        int u8strLen = ::WideCharToMultiByte(CP_UTF8, 0, u16str.c_str(), -1, NULL, 0, NULL, NULL);
        if (u8strLen <= 0) return std::string();
        std::string u8str;
        u8str.resize(u8strLen - 1);
        ::WideCharToMultiByte(CP_UTF8, 0, u16str.c_str(), -1, &u8str[0], u8strLen, NULL, NULL);
        return u8str;
    }

    int parseArguments(Spec& spec, int argc, char* argv[])
    {
        auto options = parseOptions(argc, argv);
        bool inputSpecified = false;
        bool outputSpecified = false;
        for (auto& kv : options) {
            ARG_CASE2("-f", "--format") {
                CHECK_NUM_ARGS(1);
                auto format = kv.second[0];
                if (format == "bc1")  spec.format = DXGI_FORMAT_BC1_UNORM;
                if (format == "bc3")  spec.format = DXGI_FORMAT_BC3_UNORM;
                if (format == "bc5")  spec.format = DXGI_FORMAT_BC5_UNORM;
                if (format == "bc6h") spec.format = DXGI_FORMAT_BC6H_UF16;
                if (format == "bc7")  spec.format = DXGI_FORMAT_BC7_UNORM;
                continue;
            }
            ARG_CASE2("-i", "--input") {
                CHECK_NUM_ARGS(1);
                inputSpecified = true;
                spec.source = utf8ToUtf16(kv.second[0]);
                continue;
            }
            ARG_CASE2("-l", "--level") {
                CHECK_NUM_ARGS(1);
                auto level = kv.second[0];
                if (level == "ultrafast") spec.level = Level::UltraFast;
                if (level == "veryfast")  spec.level = Level::VeryFast;
                if (level == "fast")      spec.level = Level::Fast;
                if (level == "basic")     spec.level = Level::Basic;
                if (level == "slow")      spec.level = Level::Slow;
                if (level == "veryslow")  spec.level = Level::VerySlow;
                continue;
            }
            ARG_CASE2("-m", "--mipLevels") {
                spec.mipmapSpecified = true;
                if (kv.second.size() == 1)
                    spec.mipLevels = std::max(std::stoi(kv.second[0]), 0);
                continue;
            }
            ARG_CASE2("-o", "--output") {
                CHECK_NUM_ARGS(1);
                outputSpecified = true;
                spec.output = utf8ToUtf16(kv.second[0]);
                continue;
            }
            ARG_CASE2("-s", "--srgb") {
                spec.srgbSpecified = true;
                continue;
            }
            ARG_CASE2("--forceRgb", "--forcergb") {
                spec.forceRgbSpecified = true;
                continue;
            }
            ARG_CASE2("-v", "--verbose") {
                spec.verboseSpecified = true;
                continue;
            }
            ARG_CASE2("-h", "--help") {
                ABORT(helpText);
            }
        }
        if (!inputSpecified) ABORT("No input source specified! Use --input <filename/folder>, or see --help");
        if (!outputSpecified) {
            auto i = spec.source.find_first_of('.');
            if (i == std::string::npos) {
                spec.output = spec.source + L".dds";
            }
            else {
                spec.output = spec.source.substr(0, i) + L".dds";
            }
        }
        return 0;
    }

    std::unique_ptr<DirectX::ScratchImage> loadImageFromFile(const std::wstring& filename)
    {
        auto images = std::make_unique<DirectX::ScratchImage>();
        DirectX::TexMetadata meta;
        HRESULT hr = DirectX::LoadFromTGAFile(filename.c_str(), &meta, *images);
        if (FAILED(hr)) {
            hr = DirectX::LoadFromWICFile(filename.c_str(), DirectX::WIC_FLAGS_NONE, &meta, *images);
            if (FAILED(hr))
                return nullptr;
        }
        return images;
    }

    bool shouldConvertImage(const Spec& spec, const DirectX::TexMetadata& meta)
    {
        return spec.srgbSpecified || meta.format != DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    std::unique_ptr<DirectX::ScratchImage> convertImage(std::unique_ptr<DirectX::ScratchImage> images, bool srgbSpecified)
    {
        uint32_t filter = DirectX::TEX_FILTER_DEFAULT;
        if (srgbSpecified)
            filter |= DirectX::TEX_FILTER_SRGB_IN;
        auto result = std::make_unique<DirectX::ScratchImage>();
        HRESULT hr = DirectX::Convert(images->GetImages(), images->GetImageCount(), images->GetMetadata(),
                                      DXGI_FORMAT_R8G8B8A8_UNORM, filter, DirectX::TEX_THRESHOLD_DEFAULT, *result);
        if (FAILED(hr))
            return nullptr;
        return result;
    }

    std::unique_ptr<DirectX::ScratchImage> generateMipmaps(std::unique_ptr<DirectX::ScratchImage> images, uint32_t mipLevels)
    {
        auto& meta = images->GetMetadata();
        auto mipChain = std::make_unique<DirectX::ScratchImage>();
        if (meta.dimension != DirectX::TEX_DIMENSION_TEXTURE3D) {
            if (FAILED(DirectX::GenerateMipMaps(images->GetImages(), images->GetImageCount(), meta, DirectX::TEX_FILTER_DEFAULT, mipLevels, *mipChain))) {
                return nullptr;
            }
        }
        else {
            if (FAILED(DirectX::GenerateMipMaps3D(images->GetImages(), images->GetImageCount(), meta, DirectX::TEX_FILTER_DEFAULT, mipLevels, *mipChain))) {
                return nullptr;
            }
        }
        return mipChain;
    }

    size_t computeImageSize(DXGI_FORMAT format, size_t width, size_t height)
    {
        switch (format) {
        case DXGI_FORMAT_BC1_UNORM:
            return (((width + 3) / 4) * ((height + 3) / 4)) * 8;
        case DXGI_FORMAT_BC3_UNORM:
            return (((width + 3) / 4) * ((height + 3) / 4)) * 16;
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC7_UNORM:
            return (((width + 3) / 4) * ((height + 3) / 4)) * 16;
        default:
            break;
        }
        return 0;
    }

    void initBC6HProfile(bc6h_enc_settings* settings, Level::Type level)
    {
        switch (level) {
        case Level::UltraFast: GetProfile_bc6h_veryfast(settings); break;
        case Level::VeryFast:  GetProfile_bc6h_veryfast(settings); break;
        case Level::Fast:      GetProfile_bc6h_fast(settings); break;
        case Level::Basic:     GetProfile_bc6h_basic(settings); break;
        case Level::Slow:      GetProfile_bc6h_slow(settings); break;
        case Level::VerySlow:  GetProfile_bc6h_veryslow(settings); break;
        default:
            break;
        }
    }

    void initBC7Profile(bc7_enc_settings* settings, Level::Type level, bool forceRgbSpecified)
    {
        if (forceRgbSpecified) {
            switch (level) {
            case Level::UltraFast: GetProfile_ultrafast(settings); break;
            case Level::VeryFast:  GetProfile_veryfast(settings); break;
            case Level::Fast:      GetProfile_fast(settings); break;
            case Level::Basic:     GetProfile_basic(settings); break;
            case Level::Slow:      GetProfile_slow(settings); break;
            case Level::VerySlow:  GetProfile_slow(settings); break;
            default:
                break;
            }
        }
        else {
            switch (level) {
            case Level::UltraFast: GetProfile_alpha_ultrafast(settings); break;
            case Level::VeryFast:  GetProfile_alpha_veryfast(settings); break;
            case Level::Fast:      GetProfile_alpha_fast(settings); break;
            case Level::Basic:     GetProfile_alpha_basic(settings); break;
            case Level::Slow:      GetProfile_alpha_slow(settings); break;
            case Level::VerySlow:  GetProfile_alpha_slow(settings); break;
            default:
                break;
            }
        }
    }

    std::unique_ptr<DirectX::ScratchImage> compressImages(std::unique_ptr<DirectX::ScratchImage> images, const Spec& spec)
    {
        auto& meta = images->GetMetadata();
        auto newImages = std::make_unique<DirectX::ScratchImage>();
        newImages->Initialize2D(spec.format, meta.width, meta.height, meta.arraySize, meta.mipLevels);

        size_t bufferSize = computeImageSize(spec.format, meta.width, meta.height);
        DirectX::Blob blob;
        blob.Initialize(bufferSize);

        for (uint32_t mip = 0; mip < meta.mipLevels; ++mip) {
            auto src = images->GetImage(mip, 0, 0);
            auto dst = newImages->GetImage(mip, 0, 0);

            rgba_surface surface;
            surface.ptr = src->pixels;
            surface.width = (int32_t)src->width;
            surface.height = (int32_t)src->height;
            surface.stride = (int32_t)src->rowPitch;

            switch (spec.format) {
            case DXGI_FORMAT_BC1_UNORM:
                CompressBlocksBC1(&surface, (uint8_t*)blob.GetBufferPointer());
                break;
            case DXGI_FORMAT_BC3_UNORM:
                CompressBlocksBC3(&surface, (uint8_t*)blob.GetBufferPointer());
                break;
            case DXGI_FORMAT_BC6H_UF16:
                {
                    bc6h_enc_settings settings;
                    initBC6HProfile(&settings, spec.level);
                    CompressBlocksBC6H(&surface, (uint8_t*)blob.GetBufferPointer(), &settings);
                }
                break;
            case DXGI_FORMAT_BC7_UNORM:
                {
                    bc7_enc_settings settings;
                    initBC7Profile(&settings, spec.level, spec.forceRgbSpecified);
                    CompressBlocksBC7(&surface, (uint8_t*)blob.GetBufferPointer(), &settings);
                }
                break;
            }

            bufferSize = computeImageSize(spec.format, surface.width, surface.height);
            memcpy(dst->pixels, blob.GetBufferPointer(), bufferSize);
        }

        return newImages;
    }

    std::unique_ptr<DirectX::ScratchImage> compressNormalMaps(std::unique_ptr<DirectX::ScratchImage> images, const Spec& spec)
    {
        auto newImages = std::make_unique<DirectX::ScratchImage>();
        if (FAILED(DirectX::Compress(images->GetImages(), images->GetImageCount(), images->GetMetadata(),
                                     spec.format, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT,
                                     *newImages)))
        {
            return nullptr;
        }
        return newImages;
    }
}

int main(int argc, char* argv[])
{
    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
        return 1;

    Spec spec;
    if (parseArguments(spec, argc, argv) != 0)
        return 1;

    auto images = loadImageFromFile(spec.source);
    if (!images)
        ABORT("DirectX::LoadFromXXXFile failed.");

    if (shouldConvertImage(spec, images->GetMetadata()))
    {
        images = convertImage(std::move(images), spec.srgbSpecified);
        if (!images)
            ABORT("DirectX::Convert failed.");
    }

    if (spec.mipmapSpecified)
    {
        images = generateMipmaps(std::move(images), spec.mipLevels);
        if (!images)
            ABORT("DirectX::GenerateMipMaps failed.");
    }

    if (spec.format != DXGI_FORMAT_BC5_UNORM)
    {
        images = compressImages(std::move(images), spec);
    }
    else
    {
        images = compressNormalMaps(std::move(images), spec);
        if (!images)
            ABORT("DirectX::Compress failed.");
    }

    if (FAILED(DirectX::SaveToDDSFile(images->GetImages(), images->GetImageCount(), images->GetMetadata(),
                                      DirectX::DDS_FLAGS_NONE, spec.output.c_str())))
    {
        ABORT("DirectX::SaveToDDSFile failed.");
    }

    CoUninitialize();
    return 0;
}
