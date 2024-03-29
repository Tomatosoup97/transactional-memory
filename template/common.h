#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdlib.h>

#define DEBUG 0
#define DEBUG1 0
#define VERBOSE 0
#define VERBOSE_V2 0
#define OPT 1

/* Define a proposition as likely true */
#undef likely
#ifdef __GNUC__
#define likely(prop) __builtin_expect((prop) ? 1 : 0, 1)
#else
#define likely(prop) (prop)
#endif

/* Define a proposition as likely false */
#undef unlikely
#ifdef __GNUC__
#define unlikely(prop) __builtin_expect((prop) ? 1 : 0, 0)
#else
#define unlikely(prop) (prop)
#endif

/* Define one or several attributes */
#undef as
#ifdef __GNUC__
#define as(type...) __attribute__((type))
#else
#define as(type...)
#warning This compiler has no support for GCC attributes
#endif

#define CANARY_ADDR 0xDEADC0DE

#define IS_SEG_VALID(segment) (segment->canary == CANARY_ADDR)
#define SEG_CANARY_CHECK(segment) (OPT ? 0 : assert(IS_SEG_VALID((segment))))
#define SET_SEG_CANARY(segment) (segment->canary = CANARY_ADDR)

#define CAS __sync_bool_compare_and_swap

inline int pow2_exp(size_t x) { return (64 - __builtin_clzl(x - 1)); }

inline size_t pow2(int exp) { return 1 << exp; }

inline size_t next_pow2(size_t x) { return pow2(pow2_exp(x)); }

#endif
