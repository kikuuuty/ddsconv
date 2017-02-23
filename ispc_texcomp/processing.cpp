#include "DirectXTex.h"
#include "ispc_texcomp.h"
#include "processing.h"

#define ISALIGN(x, a)   (((uintptr_t)(x) & ((a) - 1)) == 0)

namespace {

const wchar_t kHowToUse[] = {
    _T("ISPC Texture Compressor CLI v1.0.0\n")
    _T("\n")
    _T("  Usage:\n")
    _T("    ispc_texcomp [Options] -i <filename> -o <filename>\n")
    _T("\n")
    _T("  Options:\n")
    _T("    -h, --help      このメッセージです\n")
    _T("    -v, --verbose   詳細表示モード\n")
    _T("    -f <format>     圧縮フォーマット\n")
    _T("    -l <level>      圧縮レベル(bc6h,bc7のみ)\n")
    _T("    -rgb            アルファチャンネルを無視する(bc7のみ)\n")
    _T("    -dither         ディザ処理を行う(bc1,bc3のみ)\n")
    _T("\n")
    _T("  <format>\n")
    _T("    bc1,bc3,bc6h,bc7\n")
    _T("\n")
    _T("  <level>\n")
    _T("    ultrafast,veryfast,fast,basic,slow,veryslow\n")
    _T("\n")
};

const wchar_t* kFormats[] = {
    _T("bc1"),
    _T("bc3"),
    _T("bc6h"),
    _T("bc7"),
};

const DXGI_FORMAT kDXGIFormats[] = {
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC3_UNORM,
    DXGI_FORMAT_BC6H_UF16,
    DXGI_FORMAT_BC7_UNORM,
};

const wchar_t* kLevels[] = {
    _T("ultrafast"),
    _T("veryfast"),
    _T("fast"),
    _T("basic"),
    _T("slow"),
    _T("veryslow"),
};

enum class Level
{
    UltraFast,
    VeryFast,
    Fast,
    Basic,
    Slow,
    VerySlow,
};

void init_bc6h_enc_settings(bc6h_enc_settings* settings, Level level)
{
    switch (level) {
    case Level::UltraFast:  GetProfile_bc6h_veryfast(settings); break;
    case Level::VeryFast:   GetProfile_bc6h_veryfast(settings); break;
    case Level::Fast:       GetProfile_bc6h_fast(settings); break;
    case Level::Basic:      GetProfile_bc6h_basic(settings); break;
    case Level::Slow:       GetProfile_bc6h_slow(settings); break;
    case Level::VerySlow:   GetProfile_bc6h_veryslow(settings); break;
    default:
        break;
    }
}

void init_bc7_enc_settings(bc7_enc_settings* settings, Level level, bool rgb_mode)
{
    if (rgb_mode) {
        switch (level) {
        case Level::UltraFast:  GetProfile_ultrafast(settings); break;
        case Level::VeryFast:   GetProfile_veryfast(settings); break;
        case Level::Fast:       GetProfile_fast(settings); break;
        case Level::Basic:      GetProfile_basic(settings); break;
        case Level::Slow:       GetProfile_slow(settings); break;
        case Level::VerySlow:   GetProfile_slow(settings); break;
        default:
            break;
        }
    }
    else {
        switch (level) {
        case Level::UltraFast:  GetProfile_alpha_ultrafast(settings); break;
        case Level::VeryFast:   GetProfile_alpha_veryfast(settings); break;
        case Level::Fast:       GetProfile_alpha_fast(settings); break;
        case Level::Basic:      GetProfile_alpha_basic(settings); break;
        case Level::Slow:       GetProfile_alpha_slow(settings); break;
        case Level::VerySlow:   GetProfile_alpha_slow(settings); break;
        default:
            break;
        }
    }
}

size_t compute_image_size(DXGI_FORMAT format, int32_t width, int32_t height)
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

}

namespace ispc_texcomp {

void show_usage()
{
    printf(kHowToUse);
}

struct Spec
{
    bool verbose;
    bool rgb_mode;
    bool dither;
    DXGI_FORMAT format;
    Level level;
    String input_name;
    String output_name;

    Spec()
        : verbose(false)
        , rgb_mode(false)
        , dither(false)
        , format(DXGI_FORMAT_BC3_UNORM)
        , level(Level::Basic)
    {}
};

static std::unique_ptr<DirectX::ScratchImage> load_image_from_file(const String& name)
{
    DirectX::TexMetadata meta;
    auto image = std::make_unique<DirectX::ScratchImage>();
    HRESULT hr = DirectX::LoadFromTGAFile(name.c_str(), &meta, *image);
    if (FAILED(hr)) {
        hr = DirectX::LoadFromWICFile(name.c_str(), DirectX::WIC_FLAGS_NONE, &meta, *image);
        if (FAILED(hr)) {
            printf(_T("  DirectX::LoadFromXXXFile failed. [%s]\n"), name.c_str());
            return nullptr;
        }
    }
    return image;
}

static std::unique_ptr<DirectX::ScratchImage> compress_with_ispc(std::unique_ptr<DirectX::ScratchImage> image,
                                                                 const std::shared_ptr<Spec> spec)
{
    HRESULT hr;

    auto& meta = image->GetMetadata();
    if (meta.format != DXGI_FORMAT_R8G8B8A8_UNORM) {
        auto temp = std::make_unique<DirectX::ScratchImage>();
        hr = DirectX::Convert(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                              DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0.5f, *temp);
        if (FAILED(hr)) {
            printf(_T("  DirectX::Convert failed. ret = 0x%x\n"), hr);
            return nullptr;
        }
        image = std::move(temp);
    }

    auto width = static_cast<uint32_t>(image->GetMetadata().width);
    auto height = static_cast<uint32_t>(image->GetMetadata().height);

    DirectX::Blob blob;
    hr = blob.Initialize(compute_image_size(spec->format, width, height));
    if (FAILED(hr)) {
        printf(_T("  DirectX::Blob::Initialize failed. ret = 0x%x\n"), hr);
        return nullptr;
    }

    rgba_surface surface;
    surface.ptr = image->GetPixels();
    surface.width = width;
    surface.height = height;
    surface.stride = width * 4; // The format must be DXGI_FORMAT_R8G8B8A8_UNORM.

    switch (spec->format) {
    case DXGI_FORMAT_BC1_UNORM:
        CompressBlocksBC1(&surface, static_cast<uint8_t*>(blob.GetBufferPointer()));
        break;

    case DXGI_FORMAT_BC3_UNORM:
        CompressBlocksBC3(&surface, static_cast<uint8_t*>(blob.GetBufferPointer()));
        break;

    case DXGI_FORMAT_BC6H_UF16:
        {
            bc6h_enc_settings setting;
            init_bc6h_enc_settings(&setting, spec->level);
            CompressBlocksBC6H(&surface, static_cast<uint8_t*>(blob.GetBufferPointer()), &setting);
        }
        break;

    case DXGI_FORMAT_BC7_UNORM:
        {
            bc7_enc_settings setting;
            init_bc7_enc_settings(&setting, spec->level, spec->rgb_mode);
            CompressBlocksBC7(&surface, static_cast<uint8_t*>(blob.GetBufferPointer()), &setting);
        }
        break;
    }

    hr = image->Initialize2D(spec->format, width, height, 1, 1);
    if (FAILED(hr)) {
        printf(_T("  DirectX::ScratchImage::Initialize2D failed. ret = 0x%x\n"), hr);
        return nullptr;
    }

    memcpy(image->GetPixels(), blob.GetBufferPointer(), blob.GetBufferSize());
    return image;
}

static std::unique_ptr<DirectX::ScratchImage> compress_with_dxtex(std::unique_ptr<DirectX::ScratchImage> image,
                                                                  const std::shared_ptr<Spec> spec)
{
    HRESULT hr;

    DWORD compress = DirectX::TEX_COMPRESS_DEFAULT;
    if (spec->dither) {
        compress |= DirectX::TEX_COMPRESS_DITHER;
    }

    auto dst = std::make_unique<DirectX::ScratchImage>();
    hr = DirectX::Compress(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                           spec->format, compress, DirectX::TEX_THRESHOLD_DEFAULT, *dst);
    if (FAILED(hr)) {
        printf(_T("  DirectX::Compress failed. ret = 0x%x\n"), hr);
        return nullptr;
    }
    return dst;
}

std::shared_ptr<Spec> init_spec_from(const CommandLineOptions& options)
{
    std::shared_ptr<Spec> spec;
    if (!options.empty()) {
        spec = std::make_shared<Spec>();
        for (const auto& kv : options) {
            auto& key = kv.first;
            if (key == _T("-h") || key == _T("--help")) {
                return nullptr;
            }
            else if (key == _T("-v") || key == _T("--verbose")) {
                spec->verbose = true;
            }
            else if (key == _T("-f")) {
                auto& args = kv.second;
                if (args.size() >= 1) {
                    auto& format = args[0];
                    for (int32_t i = 0; i < ARRAYSIZE(kFormats); ++i) {
                        if (format == kFormats[i]) {
                            spec->format = kDXGIFormats[i];
                            break;
                        }
                    }
                }
            }
            else if (key == _T("-l")) {
                auto& args = kv.second;
                if (args.size() >= 1) {
                    auto& level = args[0];
                    for (int32_t i = 0; i < ARRAYSIZE(kLevels); ++i) {
                        if (level == kLevels[i]) {
                            spec->level = static_cast<Level>(i);
                            break;
                        }
                    }
                }
            }
            else if (key == _T("-rgb")) {
                spec->rgb_mode = true;
            }
            else if (key == _T("-dither")) {
                spec->dither = true;
            }
            else if (key == _T("-i")) {
                if (kv.second.size() >= 1) {
                    spec->input_name = kv.second[0];
                }
            }
            else if (key == _T("-o")) {
                if (kv.second.size() >= 1) {
                    spec->output_name = kv.second[0];
                }
            }
        }
    }
    return spec;
}

inline bool is_dither_compression(const std::shared_ptr<Spec> spec)
{
    if (spec->format != DXGI_FORMAT_BC1_UNORM && spec->format != DXGI_FORMAT_BC3_UNORM) return false;
    if (!spec->dither) return false;
    return true;
}

bool compress_and_save(const std::shared_ptr<Spec> spec)
{
    HRESULT hr;

    hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        printf(_T("  CoInitializeEx failed. ret = 0x%x\n"), hr);
        return false;
    }

    auto image = load_image_from_file(spec->input_name);
    if (!image) {
        return false;
    }

    bool with_ispc_texcomp = true;
    auto& meta = image->GetMetadata();
    if (!ISALIGN(meta.width, 4) || !ISALIGN(meta.height, 4) || is_dither_compression(spec)) {
        with_ispc_texcomp = false;
    }

    image = with_ispc_texcomp ? compress_with_ispc(std::move(image), spec) :
                                compress_with_dxtex(std::move(image), spec);

    int res = S_FALSE;
    if (image) {
        hr = DirectX::SaveToDDSFile(image->GetImages(), image->GetImageCount(), image->GetMetadata(),
                                    DirectX::DDS_FLAGS_NONE, spec->output_name.c_str());
        if (FAILED(hr)) {
            printf(_T("  DirectX::SaveToDDSFile failed. ret = 0x%x\n"), hr);
        }
        else {
            printf(_T("done.\n"));
            res = S_OK;
        }
    }
    ::CoUninitialize();
    return res == S_OK ? true : false;
}

}
