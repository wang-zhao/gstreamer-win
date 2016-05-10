#ifdef _STL70_
#include "_70_yvals.h"
#else


/* yvals.h values header for Microsoft C/C++ */
#ifndef _YVALS
#define _YVALS
#include <use_ansi.h>

/* Define _CRTIMP2 */
#ifndef _CRTIMP2
#if defined(_DLL) && !defined(_STATIC_CPPLIB)
#define _CRTIMP2 __declspec(dllimport)
#else	/* ndef _DLL */
#define _CRTIMP2
#endif	/* _DLL */
#endif	/* _CRTIMP2 */

#if !defined(_CRTDATA2)
#define _CRTDATA2 _CRTIMP2
#endif

#if !defined(_SCL_INSECURE_DEPRECATE)
#define _SCL_INSECURE_DEPRECATE
#endif

#if !defined(_SCL_CHECKED_ALGORITHM_WARN)
#define _SCL_CHECKED_ALGORITHM_WARN
#endif

#ifndef _MRTIMP2_NPURE_NCEEPURE
#if defined(_M_CEE_PURE)
#define _MRTIMP2_NPURE_NCEEPURE
#else
#define _MRTIMP2_NPURE_NCEEPURE _MRTIMP2_NPURE
#endif
#endif

#ifndef __CLRCALL_PURE_OR_CDECL
#if defined(_M_CEE_PURE)
#define __CLRCALL_PURE_OR_CDECL __clrcall
#else
#define __CLRCALL_PURE_OR_CDECL __cdecl
#endif
#endif

#if __STDC_WANT_SECURE_LIB__
#define _CRT_SECURE_MEMCPY(dest, destsize, source, count) ::memcpy_s((dest), (destsize), (source), (count))
#define _CRT_SECURE_MEMMOVE(dest, destsize, source, count) ::memmove_s((dest), (destsize), (source), (count))
#define _CRT_SECURE_WMEMCPY(dest, destsize, source, count) ::wmemcpy_s((dest), (destsize), (source), (count))
#define _CRT_SECURE_WMEMMOVE(dest, destsize, source, count) ::wmemmove_s((dest), (destsize), (source), (count))
#else
#define _CRT_SECURE_MEMCPY(dest, destsize, source, count) ::memcpy((dest), (source), (count))
#define _CRT_SECURE_MEMMOVE(dest, destsize, source, count) ::memmove((dest), (source), (count))
#define _CRT_SECURE_WMEMCPY(dest, destsize, source, count) ::wmemcpy((dest), (source), (count))
#define _CRT_SECURE_WMEMMOVE(dest, destsize, source, count) ::wmemmove((dest), (source), (count))
#endif


#ifdef  _MSC_VER
#pragma pack(push,8)
#endif  /* _MSC_VER */

#pragma warning(disable: 4018 4114 4146 4244 4245)
#pragma warning(disable: 4663 4664 4665)
#pragma warning(disable: 4237 4284 4290 4514)
/* NAMESPACE */
#if defined(__cplusplus)
#define _STD			std::
#define _STD_BEGIN	namespace std {
#define _STD_END		};
#define _STD_USING 

/*
We use the stdext (standard extension) namespace to contain extensions that are not part of the current standard
*/
#define _STDEXT_BEGIN	    namespace stdext {
#define _STDEXT_END		}
#define _STDEXT	        ::stdext::

#ifdef _STD_USING
#define _C_STD_BEGIN	namespace std {	/* only if *.c compiled as C++ */
#define _C_STD_END	}
#define _CSTD	::std::

#else /* _STD_USING */
/* #define _GLOBAL_USING	*.h in global namespace, c* imports to std */

#define _C_STD_BEGIN
#define _C_STD_END
#define _CSTD	::
#endif /* _STD_USING */

#define _C_LIB_DECL		extern "C" {	/* C has extern "C" linkage */
#define _END_C_LIB_DECL	}
#define _EXTERN_C			extern "C" {
#define _END_EXTERN_C		}

#else
#define _STD			::
#define _STD_BEGIN
#define _STD_END

#define _STD_BEGIN
#define _STD_END
#define _STD

#define _C_STD_BEGIN
#define _C_STD_END
#define _CSTD

#define _C_LIB_DECL
#define _END_C_LIB_DECL
#define _EXTERN_C
#define _END_EXTERN_C
#endif /* __cplusplus */
_STD_BEGIN
/* TYPE bool */
#if defined(__cplusplus)
typedef bool _Bool;
#endif /* __cplusplus */
/* INTEGER PROPERTIES */
#define _MAX_EXP_DIG	8	/* for parsing numerics */
#define _MAX_INT_DIG	32
#define _MAX_SIG_DIG	36
/* STDIO PROPERTIES */
#define _Filet _iobuf

#ifndef _FPOS_T_DEFINED
#define _FPOSOFF(fp)	((long)(fp))
#endif /* _FPOS_T_DEFINED */

/* NAMING PROPERTIES */
#if defined(__cplusplus)
#define _C_LIB_DECL extern "C" {
#define _END_C_LIB_DECL }
#else
#define _C_LIB_DECL
#define _END_C_LIB_DECL
#endif /* __cplusplus */
#define _CDECL
// CLASS _Lockit
#if defined(__cplusplus)
class _CRTIMP2 _Lockit
{	// lock while object in existence
public:
#ifdef _MT
#define _LOCKIT(x)	lockit x
  _Lockit();
  ~_Lockit();
#else
#define _LOCKIT(x)
  _Lockit()
  {}
  ~_Lockit()
  {}
#endif /* _MT */
};
#endif /* __cplusplus */
/* MISCELLANEOUS MACROS */
#define _L(c)	L##c
#define _Mbstinit(x)	mbstate_t x = {0}
#define _MAX	_cpp_max
#define _MIN	_cpp_min
#if defined(_DLL) && !defined(_MANAGED)
#define _DLL_CPPLIB_STDHPP64
#endif

#ifdef _MSC_VER
#define _EXTERN_TEMPLATE	template
#define _THROW_BAD_ALLOC	_THROW1(...)
#endif  /* _MSC_VER */


_STD_END
#ifdef  _MSC_VER
#pragma pack(pop)
#endif  /* _MSC_VER */


#endif /* _YVALS */

/*
* Copyright (c) 1996 by P.J. Plauger.  ALL RIGHTS RESERVED. 
* Consult your license regarding permissions and restrictions.
*/

#endif	// _STL70_
/* 88bf0570-3001-4e78-a5f2-be5765546192 */ 

