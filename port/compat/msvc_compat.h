/* Force-included into every Linux translation unit (port/linux.cmake). Maps MSVC keywords and CRT names onto
   GCC/POSIX so the shared PC sources compile. Must stay valid C and C++. Adapted from hit-and-run-linux. */
#ifndef KISAK_MSVC_COMPAT_H
#define KISAK_MSVC_COMPAT_H
#if defined(__linux__)

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>

#define __cdecl
#define __fastcall
#define __int8  char
#define __int16 short
#define __int32 int
#define __int64 long long
#define _int64  long long
#define __forceinline inline
#define __forcenoinline
#define _inline inline
#define __inline inline

#define _isnan(x)   __builtin_isnan(x)
#define _finite(x)  __builtin_isfinite(x)
#define stricmp     strcasecmp
#define _stricmp    strcasecmp
#define strnicmp    strncasecmp
#define _strnicmp   strncasecmp
#define strcmpi     strcasecmp
#define _strcmpi    strcasecmp
#define _snprintf   snprintf
#define _vsnprintf  vsnprintf
#define _strdup     strdup

#ifdef __cplusplus
/* Same linkage as GameSpy's gsPlatform.h, which declares these on _UNIX; defined in GameSpy/common/gsPlatformUtil.c. */
extern "C" char* _strlwr(char* s);
extern "C" char* _strupr(char* s);
#define strlwr _strlwr
#define strupr _strupr
#endif

typedef unsigned int MMRESULT;

#ifndef _MAX_PATH
#define _MAX_PATH 260
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#endif /* __linux__ */
#endif
