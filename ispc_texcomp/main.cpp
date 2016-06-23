// ispc_texcomp.cpp : �R���\�[�� �A�v���P�[�V�����̃G���g�� �|�C���g���`���܂��B
//

#include "option_perser.h"
#include "processing.h"

int wmain(int argc, wchar_t** argv)
{
    auto options = ispc_texcomp::perse_options(argc, argv);
    auto spec = ispc_texcomp::init_spec_from(options);
    if (!spec) {
        ispc_texcomp::show_usage();
    }
    else if (!ispc_texcomp::compress_and_save(spec)) {
        return 1;
    }
    return 0;
}
