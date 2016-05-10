/*
 * copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file libavutil/internal.h
 * common internal API header
 */

#ifndef AVUTIL_INTERNAL_H
#define AVUTIL_INTERNAL_H

#if !defined(DEBUG) && !defined(NDEBUG)
#    define NDEBUG
#endif

#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
//#include "config.h"
#include "common.h"
#include "mem.h"
#include "timer.h"
#include "avstring.h"

#ifdef __cplusplus
extern "C"{
#endif

#ifndef attribute_align_arg
#if (!defined(__ICC) || __ICC > 1110) && AV_GCC_VERSION_AT_LEAST(4,2)
#    define attribute_align_arg __attribute__((force_align_arg_pointer))
#else
#    define attribute_align_arg
#endif
#endif

#ifndef attribute_used
#if AV_GCC_VERSION_AT_LEAST(3,1)
#    define attribute_used __attribute__((used))
#else
#    define attribute_used
#endif
#endif

#ifndef INT16_MIN
#define INT16_MIN       (-0x7fff - 1)
#endif

#ifndef INT16_MAX
#define INT16_MAX       0x7fff
#endif

#ifndef INT32_MIN
#define INT32_MIN       (-0x7fffffff - 1)
#endif

#ifndef INT32_MAX
#define INT32_MAX       0x7fffffff
#endif

#ifndef UINT32_MAX
#define UINT32_MAX      0xffffffff
#endif

#ifndef INT64_MIN
#define INT64_MIN       (-0x7fffffffffffffffLL - 1)
#endif

#ifndef INT64_MAX
#define INT64_MAX 9223372036854775807I64
#endif

#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFF##UI64
#endif

#ifndef INT_BIT
#    define INT_BIT (CHAR_BIT * sizeof(int))
#endif

#ifndef offsetof
#    define offsetof(T, F) ((unsigned int)((char *)&((T *)0)->F))
#endif

/* Use to export labels from asm. */
#define LABEL_MANGLE(a) EXTERN_PREFIX #a

// Use rip-relative addressing if compiling PIC code on x86-64.
#if ARCH_X86_64 && defined(PIC)
#    define LOCAL_MANGLE(a) #a "(%%rip)"
#elif defined(_MSC_VER)
#    define LOCAL_MANGLE(a) a
#else
#	 define LOCAL_MANGLE(a) #a
#endif

#ifndef  EXTERN_PREFIX
#ifdef _MSC_VER
#define EXTERN_PREFIX 
#else
#define EXTERN_PREFIX "_"
#endif

#endif

#define MANGLE(a) EXTERN_PREFIX LOCAL_MANGLE(a)

/* debug stuff */

/* dprintf macros */
#ifdef DEBUG
#    define dprintf(pctx, ...) av_log(pctx, AV_LOG_DEBUG, __VA_ARGS__)
#else
#    define dprintf(pctx, ...)	(0)
#endif

#define av_abort()      do { av_log(NULL, AV_LOG_ERROR, "Abort at %s:%d\n", __FILE__, __LINE__); abort(); } while (0)

/* math */

extern const uint32_t ff_inverse[257];

#if ARCH_X86
//#    define FASTDIV(a,b) 
int __forceinline FASTDIV(int a, int b)
    {
        int retv;
		__asm{
			mov eax, a;
			lea esi, dword ptr ff_inverse;
			mov edi, b;
			lea esi, [esi+4*edi];
			mov ecx, dword ptr[esi];
            mul ecx;
			mov dword ptr retv, edx;
//             :"=d"(ret), "=a"(dmy)
//             :"1"(a), "g"(ff_inverse[b])
		}
        return retv;
    }
#elif HAVE_ARMV6 && HAVE_INLINE_ASM
static inline av_const int FASTDIV(int a, int b)
{
    int r, t;
    __asm__ volatile("cmp     %3, #2               \n\t"
                     "ldr     %1, [%4, %3, lsl #2] \n\t"
                     "lsrle   %0, %2, #1           \n\t"
                     "smmulgt %0, %1, %2           \n\t"
                     : "=&r"(r), "=&r"(t) : "r"(a), "r"(b), "r"(ff_inverse));
    return r;
}
#elif ARCH_ARM && HAVE_INLINE_ASM
static inline av_const int FASTDIV(int a, int b)
{
    int r, t;
    __asm__ volatile("umull %1, %0, %2, %3"
                     : "=&r"(r), "=&r"(t) : "r"(a), "r"(ff_inverse[b]));
    return r;
}
#elif CONFIG_FASTDIV
#    define FASTDIV(a,b)   ((uint32_t)((((uint64_t)a) * ff_inverse[b]) >> 32))
#else
#    define FASTDIV(a,b)   ((a) / (b))
#endif

extern const uint8_t ff_sqrt_tab[256];

static inline av_const unsigned int ff_sqrt(unsigned int a)
{
    unsigned int b;

    if (a < 255) return (ff_sqrt_tab[a + 1] - 1) >> 4;
    else if (a < (1 << 12)) b = ff_sqrt_tab[a >> 4] >> 2;
#if !CONFIG_SMALL
    else if (a < (1 << 14)) b = ff_sqrt_tab[a >> 6] >> 1;
    else if (a < (1 << 16)) b = ff_sqrt_tab[a >> 8]   ;
#endif
    else {
        int s = av_log2_16bit(a >> 16) >> 1;
        unsigned int c = a >> (s + 2);
        b = ff_sqrt_tab[c >> (s + 8)];
        b = FASTDIV(c,b) + (b << s);
    }

    return b - (a < b * b);
}

#if ARCH_X86
#define MASK_ABS(mask, level)\
	__asm mov eax, level\
	__asm mov edx, mask\
	__asm cdq\
	__asm xor eax, edx\
	__asm sub eax, edx\
	__asm mov level, eax
	///*    : "+a" (level), "=&d" (mask)*/\
//}
#else
#define MASK_ABS(mask, level)\
            mask  = level >> 31;\
            level = (level ^ mask) - mask;
#endif

#if HAVE_CMOV
#define COPY3_IF_LT(x, y, a, b, c, d)\
__asm__ volatile(\
    "cmpl  %0, %3       \n\t"\
    "cmovl %3, %0       \n\t"\
    "cmovl %4, %1       \n\t"\
    "cmovl %5, %2       \n\t"\
    : "+&r" (x), "+&r" (a), "+r" (c)\
    : "r" (y), "r" (b), "r" (d)\
);
#else
#define COPY3_IF_LT(x, y, a, b, c, d)\
if ((y) < (x)) {\
    (x) = (y);\
    (a) = (b);\
    (c) = (d);\
}
#endif

/* avoid usage of dangerous/inappropriate system functions */
#undef  malloc
#define malloc please_use_av_malloc
#undef  free
#define free please_use_av_free
#undef  realloc
#define realloc please_use_av_realloc
#undef calloc
#define calloc please_use_av_mallocz
#undef  time
#define time time_is_forbidden_due_to_security_issues
#undef  rand
#define rand rand_is_forbidden_due_to_state_trashing_use_av_lfg_get
#undef  srand
#define srand srand_is_forbidden_due_to_state_trashing_use_av_lfg_init
#undef  random
#define random random_is_forbidden_due_to_state_trashing_use_av_lfg_get
#undef  sprintf
#define sprintf sprintf_is_forbidden_due_to_security_issues_use_snprintf
#undef  strcat
#define strcat strcat_is_forbidden_due_to_security_issues_use_av_strlcat
#undef  exit
#define exit exit_is_forbidden
#ifndef LIBAVFORMAT_BUILD
#undef  printf
#define printf please_use_av_log_instead_of_printf
#undef  fprintf
#define fprintf please_use_av_log_instead_of_fprintf
#undef  puts
#define puts please_use_av_log_instead_of_puts
#undef  perror
#define perror please_use_av_log_instead_of_perror
#endif

#ifndef FF_ALLOC_OR_GOTO
#define FF_ALLOC_OR_GOTO(ctx, p, size, label)\
{\
    p = av_malloc(size);\
    if (p == NULL && (size) != 0) {\
        av_log(ctx, AV_LOG_ERROR, "Cannot allocate memory.\n");\
        goto label;\
    }\
}
#endif //FF_ALLOC_OR_GOTO

#ifndef FF_ALLOCZ_OR_GOTO
#define FF_ALLOCZ_OR_GOTO(ctx, p, size, label)\
{\
    (*(void**)(&p)) = av_mallocz(size);\
    if (p == NULL && (size) != 0) {\
        av_log(ctx, AV_LOG_ERROR, "Cannot allocate memory.\n");\
        goto label;\
    }\
}
#endif //FF_ALLOCZ_OR_GOTO


double rint(double x);

#if !HAVE_LLRINT
long long llrint(double x);
#endif /* HAVE_LLRINT */

#if !HAVE_LOG2
double log2(double x);
#endif /* HAVE_LOG2 */

#if !HAVE_LRINT
long int lrint(double x);
#endif /* HAVE_LRINT */

#if !HAVE_LRINTF
long int lrintf(float x);
#endif /* HAVE_LRINTF */

#if !HAVE_ROUND
double round(double x);
#endif /* HAVE_ROUND */

#if !HAVE_ROUNDF
float roundf(float x);
#endif /* HAVE_ROUNDF */

#if !HAVE_TRUNCF
float truncf(float x);
#endif /* HAVE_TRUNCF */

/**
 * Returns NULL if CONFIG_SMALL is true, otherwise the argument
 * without modification. Used to disable the definition of strings
 * (for example AVCodec long_names).
 */
#if CONFIG_SMALL
#   define NULL_IF_CONFIG_SMALL(x) NULL
#else
#   define NULL_IF_CONFIG_SMALL(x) x
#endif

#ifdef __cplusplus
}
#endif

#endif /* AVUTIL_INTERNAL_H */
