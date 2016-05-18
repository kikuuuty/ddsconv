#ifndef OPTION_PERSER_H__
#define OPTION_PERSER_H__

namespace ispc_texcomp {

using CommandLineOptions = std::map<String, StringVector>;

CommandLineOptions perse_options( int32_t argc, const wchar_t* const argv[] );
size_t printf( const wchar_t* fmt, ... );

} 

#endif
