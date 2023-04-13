#pragma once

#include "targetver.h"

#pragma warning(push)
#pragma warning(disable: 4595)  // '...': non-member operator new or delete functions may not be declared inline
#pragma warning(disable: 4571)  // Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
#include "WinDDKFixes.hpp"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#pragma clang diagnostic ignored "-Wc99-extensions"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wcomment"
#pragma clang diagnostic ignored "-Wdelete-incomplete"
#pragma clang diagnostic ignored "-Wdeprecated-dynamic-exception-spec"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
#pragma clang diagnostic ignored "-Winvalid-noreturn"
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wkeyword-macro"
#pragma clang diagnostic ignored "-Wmicrosoft-cast"
#pragma clang diagnostic ignored "-Wmicrosoft-comment-paste"
#pragma clang diagnostic ignored "-Wmicrosoft-enum-value"
#pragma clang diagnostic ignored "-Wmicrosoft-template"
#pragma clang diagnostic ignored "-Wmissing-declarations"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-value"
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wvarargs"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#include <process.h>
#include <stddef.h>
#pragma warning(push)
#pragma warning(disable: 4548)  // expression before comma has no effect; expected expression with side-effect
#include <malloc.h>  // we include it just to suppress its warning...
#pragma warning(pop)
#ifndef assert
#include <assert.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
#endif
#include <tchar.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#include <time.h>

#ifdef __cplusplus
#pragma warning(push)
#pragma warning(disable: 4574)  // '...' is defined to be '0': did you mean to use '...'
#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>
#pragma warning(pop)
#endif
#pragma warning(pop)

#if defined(__STDC_WANT_SECURE_LIB__) && !__STDC_WANT_SECURE_LIB__
#if !defined(_WCHAR_T_DEFINED) && !defined(_NATIVE_WCHAR_T_DEFINED)
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif
#if !(defined(_MSC_VER) && _MSC_VER <= 1400)

#ifdef __cplusplus
inline int __cdecl sprintf_s(char *buffer, size_t, const char *format, ...) { int r; va_list args; va_start(args, format); r = vsprintf(buffer, format, args); va_end(args); return r; }
inline int __cdecl swprintf_s(wchar_t *buffer, size_t, const wchar_t *format, ...) { int r; va_list args; va_start(args, format); r = vswprintf(buffer, format, args); va_end(args); return r; }
inline int __cdecl my_swprintf(wchar_t *buffer, const wchar_t *format, ...) { int r; va_list args; va_start(args, format); r = vswprintf(buffer, format, args); va_end(args); return r; }
inline int __cdecl my_swprintf(wchar_t *buffer, size_t, const wchar_t *format, ...) { int r; va_list args; va_start(args, format); r = vswprintf(buffer, format, args); va_end(args); return r; }
namespace std { using ::my_swprintf; }
#define swprintf my_swprintf
#define sprintf_s(buf, size, ...) sprintf(buf, __VA_ARGS__)
#define swprintf_s(buf, size, ...) swprintf(buf, size, __VA_ARGS__)
#endif

#define wcstok_s(tok, delim, ctx) wcstok(tok, delim)
#define wcstok_s(tok, delim, ctx) wcstok(tok, delim)
static int __cdecl wcscpy_s(wchar_t *strDestination, size_t numberOfElements, const wchar_t *strSource) { wcscpy(strDestination, strSource); return 0; }
#define wcscpy_s(a, b, c) wcscpy(a, c)
#define wcscat_s(a, b, c) wcscat(a, c)
#define memcpy_s(a, b, c, d) memcpy(a, c, d)
#define memmove_s(a, b, c, d) memmove(a, c, d)
#define wmemcpy_s(a, b, c, d) wmemcpy(a, c, d)
#define wmemmove_s(a, b, c, d) wmemmove(a, c, d)
#define strcpy_s(a, b, c) strcpy(a, c)
#define strncpy_s(a, b, c, d) strncpy(a, c, d)
#define wcsncpy_s(a, b, c, d) wcsncpy(a, c, d)
#define vswprintf_s(a, b, c, d) _vstprintf(a, c, d)
#define strcat_s(a, b, c) strcat(a, c)
#define ATL_CRT_ERRORCHECK(A) ((A), 0)
#endif
#endif

#ifdef __cplusplus
namespace std
{
	// I don't get why these are necessary??
#define X(T) template<> inline T const &(min)(T const &a, T const &b) { return b < a ? b : a; } template<> inline T const &(max)(T const &a, T const &b) { return b < a ? a : b; }
	X(int); X(unsigned int);
	X(long); X(unsigned long);
	X(long long); X(unsigned long long);
#undef  X
}
#endif

#ifdef __cplusplus
namespace WTL { using std::min; using std::max; }
#endif

#ifdef __clang__
struct __declspec(uuid("00000001-0000-0000-C000-000000000046")) IClassFactory;
struct __declspec(uuid("000214e6-0000-0000-c000-000000000046")) IShellFolder;
struct __declspec(uuid("00020412-0000-0000-C000-000000000046")) ITypeInfo2;
struct __declspec(uuid("00000000-0000-0000-C000-000000000046")) IUnknown;
#endif

#ifndef _CPPLIB_VER
#define __movsb __movsb_
#define __movsd __movsd_
#define __movsw __movsw_
#define __movsq __movsq_
#endif
#ifdef __clang__
#define _interlockedbittestandset _interlockedbittestandset_old
#define _interlockedbittestandreset _interlockedbittestandreset_old
#define _interlockedbittestandset64 _interlockedbittestandset64_old
#define _interlockedbittestandreset64 _interlockedbittestandreset64_old
#endif
#include <Windows.h>
#ifdef __clang__
#undef  _interlockedbittestandreset64
#undef  _interlockedbittestandset64
#undef  _interlockedbittestandreset
#undef  _interlockedbittestandset
#endif
#ifndef _CPPLIB_VER
#undef __movsq
#undef __movsw
#undef __movsd
#undef __movsb
#endif

#ifdef __cplusplus
#pragma warning(push)
#pragma warning(disable: 4091)  // 'typedef ': ignored on left of '' when no variable is declared
#pragma warning(disable: 4191)  // 'type cast': unsafe conversion
#pragma warning(disable: 4265)  // class has virtual functions, but destructor is not virtual
#pragma warning(disable: 4302)
#pragma warning(disable: 4365)
#pragma warning(disable: 4456)  // declaration hides previous local declaration
#pragma warning(disable: 4457)  // declaration hides function parameter
#pragma warning(disable: 4555)  // expression has no effect; expected expression with side-effect
#pragma warning(disable: 4619)  // there is no warning number ''
#pragma warning(disable: 4640)  // construction of local static object is not thread-safe
#pragma warning(disable: 4768)  // __declspec attributes before linkage specification are ignored
#pragma warning(disable: 4838)  // conversion requires a narrowing conversion
#pragma warning(disable: 4917)  // a GUID can only be associated with a class, interface or namespace
#pragma warning(disable: 4986)  // exception specification does not match previous declaration
#pragma warning(disable: 4987)  // nonstandard extension used: 'throw (...)'
#pragma warning(disable: 5038)  // data member will be initialized after data member
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>
extern WTL::CAppModule _Module;
#include <atlwin.h>
#include <atldlgs.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atltheme.h>
#pragma warning(pop)
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#if defined(__STDC_WANT_SECURE_LIB__) && !__STDC_WANT_SECURE_LIB__
#if !(defined(_MSC_VER) && _MSC_VER <= 1400)
#undef sprintf_s
#undef swprintf_s
#undef wcstok_s
#undef wcstok_s
#undef wcscpy_s
#undef wcscat_s
#undef memcpy_s
#undef memmove_s
#undef wmemcpy_s
#undef wmemmove_s
#undef strcpy_s
#undef strncpy_s
#undef wcsncpy_s
#undef vswprintf_s
#undef strcat_s
#undef ATL_CRT_ERRORCHECK
#endif
#endif
