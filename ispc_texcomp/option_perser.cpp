#include "option_perser.h"
#include <cstdarg>

namespace {

StringVector split(const String& str, const String& delims)
{
    StringVector ret;
    size_t start = 0, pos;
    do {
        pos = str.find_first_of(delims, start);
        if (pos == start) {
            start = pos + 1;
        }
        else if (pos == String::npos) {
            ret.push_back(str.substr(start));
        }
        else {
            ret.push_back(str.substr(start, pos - start));
            start = pos + 1;
        }

        start = str.find_first_not_of(delims, start);

    } while (pos != String::npos);
    return ret;
}

wchar_t* set_locale(const wchar_t* locale)
{
    auto temp = _wsetlocale(LC_CTYPE, nullptr);
    auto length = wcslen(temp);
    auto prev_locale = new wchar_t[length + 1];
    memcpy(prev_locale, temp, sizeof(wchar_t) * length);
    prev_locale[length] = 0;
    _wsetlocale(LC_CTYPE, locale);
    return prev_locale;
}

}

namespace ispc_texcomp {

CommandLineOptions perse_options(int32_t argc, const wchar_t* const argv[])
{
    CommandLineOptions options;
    String op;
    for (int32_t i = 1; i < argc; ++i) {
        auto* args = argv[i];
        if (args[0] == '-') {
            if (args[1] == '-') {
                // --option=param,...
                auto v = split(args, _T("="));
                size_t size = v.size();
                if ( size == 1 ) {
                    options[v[0]].clear();
                }
                else if ( v.size() == 2 ) {
                    options[v[0]] = split(v[1], _T(","));
                }
            }
            else {
                op = String(args);
                options[op].clear();
            }
        }
        else {
            options[op].push_back(argv[i]);
        }
    }
    return options;
}

size_t printf(const wchar_t* format, ...)
{
    va_list arg;
    va_start(arg, format);

    size_t length = ::_vscwprintf(format, arg);

    auto s = new wchar_t[length + 1];
    ::_vsnwprintf_s(s, length + 1, _TRUNCATE, format, arg);

    va_end(arg);

    auto prev_locale = set_locale(_T("japanese"));

    ::OutputDebugStringW(s);
    length = wprintf(_T("%s"), s);

    _wsetlocale(LC_CTYPE, prev_locale);

    delete [] prev_locale;

    delete [] s;
    return length;
}

}
