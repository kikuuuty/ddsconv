#ifndef PROCESS_H__
#define PROCESS_H__

#include "option_perser.h"

namespace ispc_texcomp {

void show_usage();

struct Spec;

std::shared_ptr<Spec> init_spec_from( const CommandLineOptions& options );

bool compress_and_save( const std::shared_ptr<Spec> spec );

}

#endif
