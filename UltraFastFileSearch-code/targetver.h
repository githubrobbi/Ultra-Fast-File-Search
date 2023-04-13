#pragma once

#ifdef __clang__
#pragma clang diagnostic ignored "-Wc++98-compat-local-type-template-args"
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wmissing-declarations"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wmissing-prototypes"
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wold-style-cast"  // TODO: We may want this for linting our own code...
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wshadow-field-in-constructor"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wunused-macros"  // TODO: We may want this for linting our own code...
#pragma clang diagnostic ignored "-Wunused-template"  // TODO: We may want this for linting our own code...
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

#define _WIN32_WINNT 0x0502
#define _WIN32_IE 0x0600

#ifdef _MSC_VER
// ============= Put these FIRST ============
#if _MSC_VER < 1900
#pragma warning(disable: 4616)  // not a valid compiler warning
#pragma warning(disable: 4619)  // #pragma warning : there is no warning number
#endif
// ==========================================

#ifndef _DEBUG
#pragma warning(disable: 4005)  // macro redefinition  // Only in DDK compilations (which are Release-mode)...
#endif
#pragma warning(disable: 4061)  // enumerator in switch of enum is not explicitly handled by a case label
#pragma warning(disable: 4062)  // enumerator in switch of enum is not handled
#pragma warning(disable: 4100)  // unreferenced formal parameter
#pragma warning(disable: 4159)  // #pragma pack(pop,...): has popped previously pushed identifier
#ifndef _DEBUG
#pragma warning(disable: 4163)  // not available as an intrinsic function  // Only in DDK compilations (which are Release-mode)...
#endif
#pragma warning(disable: 4191)  // 'reinterpret_cast': unsafe conversion
#pragma warning(disable: 4265)  // class has virtual functions, but destructor is not virtual
#pragma warning(disable: 4290)  // C++ exception specification ignored except to indicate a function is not __declspec(nothrow)
#pragma warning(disable: 4324)  // structure was padded due to __declspec(align())
#pragma warning(disable: 4365)  // conversion, signed/unsigned mismatch
#pragma warning(disable: 4371)  // layout of class may have changed from a previous version of the compiler due to better packing of member
#pragma warning(disable: 4435)  // Object layout under /vd2 will change due to virtual base
#pragma warning(disable: 4458)  // declaration of 'field' hides class member
#pragma warning(disable: 4459)  // declaration of 'identifier' hides global declaration
#ifndef _DEBUG
#pragma warning(disable: 4505)  // unreferenced local function has been removed
#endif
#pragma warning(disable: 4510)  // default constructor could not be generated
#pragma warning(disable: 4512)  // assignment operator could not be generated
#pragma warning(disable: 4571)  // Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
#pragma warning(disable: 4610)  // can never be instantiated - user defined constructor required
#pragma warning(disable: 4623)  // default constructor was implicitly defined as deleted
#pragma warning(disable: 4624)  // destructor was implicitly defined as deleted because a base class destructor is inaccessible or deleted
#pragma warning(disable: 4625)  // copy constructor was implicitly defined as deleted
#pragma warning(disable: 4626)  // assignment operator was implicitly defined as deleted
#pragma warning(disable: 4643)  // Forward declaring 'member' in namespace std is not permitted by the C++ Standard.
#pragma warning(disable: 4668)  // 'MACRO' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#pragma warning(disable: 4710)  // function not inlined
#pragma warning(disable: 4711)  // function selected for automatic inline expansion
#pragma warning(disable: 4774)  // format string expected in argument is not a string literal
#pragma warning(disable: 4820)  // 'n' bytes padding added after data member
// #pragma warning(disable: 4838)  // conversion requires a narrowing conversion
#pragma warning(disable: 5026)  // move constructor was implicitly defined as deleted
#pragma warning(disable: 5027)  // move assignment operator was implicitly defined as deleted
#pragma warning(disable: 5039)  // Pointer or reference to potentially throwing function passed to extern C function under -EHc. Undefined behavior may occur if this function throws an exception.
#pragma warning(disable: 5045)  // Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
#endif

#ifdef _DEBUG
#define _ASSERTE(Expr) (void)(Expr)
#define ATLTRACE ATL::CTraceFileAndLineInfo("<redacted-file>", __LINE__)
#define ATLTRACE2 ATLTRACE
#endif

#define _USE_MATH_DEFINES 1
#define _CRT_OBSOLETE_NO_WARNINGS 1
#define _CRT_NONSTDC_NO_WARNINGS 1
#define _CRT_SECURE_NO_WARNINGS 1
#define _SCL_SECURE_NO_WARNINGS 1
#define _CRT_NON_CONFORMING_SWPRINTFS 1
#define _STRALIGN_USE_SECURE_CRT 0
#define _ATL_NO_COM 1
#define _ATL_NO_AUTOMATIC_NAMESPACE 1
#define _ATL_DISABLE_DEPRECATED 1
#define _WTL_NO_AUTOMATIC_NAMESPACE 1
#define _WTL_USE_VSSYM32 1
#define STRICT 1
#define NOMINMAX 1
#define BUILD_WINDOWS 1
#define BOOST_ALL_NO_LIB 1
#define BOOST_ALLOW_DEPRECATED_HEADERS 1
#define BOOST_DISABLE_ASSERTS 1
#define BOOST_EXCEPTION_DISABLE 1
#define BOOST_NO_CXX11_CHAR16_T
#define BOOST_NO_CXX11_CHAR32_T
#define BOOST_NO_SWPRINTF
#define BOOST_REGEX_NO_FILEITER

#ifndef _DEBUG
#define _SECURE_SCL 0
#define _ITERATOR_DEBUG_LEVEL 0
#define __STDC_WANT_SECURE_LIB__ 0
#define _STRALIGN_USE_SECURE_CRT 0
#ifndef __SIZEOF_LONG_LONG__
#define __SIZEOF_LONG_LONG__ (ULLONG_MAX / (UCHAR_MAX + 1U) + 1)
#endif
#ifndef __SIZEOF_WCHAR_T__
#define __SIZEOF_WCHAR_T__ (WCHAR_MAX / (UCHAR_MAX + 1U) + 1)
#endif
#endif

#ifdef __clang__
#ifdef __GNUC__
typedef long long __m64 __attribute__((__vector_size__(8)));
#else
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-declarations"
#pragma clang diagnostic ignored "-Wignored-attributes"
#endif
typedef union __declspec(intrin_type) __declspec(align(8)) __m64;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif
#define _INTEGRAL_MAX_BITS 64
#endif
