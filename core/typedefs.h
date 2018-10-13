#pragma once

#include "int_types.h"
#include <stddef.h>

//should always inline no matter what
#ifndef _ALWAYS_INLINE_

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define _ALWAYS_INLINE_ __attribute__((always_inline)) inline
#elif defined(__llvm__)
#define _ALWAYS_INLINE_ __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define _ALWAYS_INLINE_ __forceinline
#else
#define _ALWAYS_INLINE_ inline
#endif

#endif

//should always inline, except in some cases because it makes debugging harder
#ifndef _FORCE_INLINE_

#ifdef DISABLE_FORCED_INLINE
#define _FORCE_INLINE_ inline
#else
#define _FORCE_INLINE_ _ALWAYS_INLINE_
#endif

#endif

/** Generic ABS function, for math uses please use Math::abs */

#ifndef ABS
#define ABS(m_v) (((m_v) < 0) ? (-(m_v)) : (m_v))
#endif

#ifndef SGN
#define SGN(m_v) (((m_v) < 0) ? (-1.0) : (+1.0))
#endif

#ifndef MIN
#define MIN(m_a, m_b) (((m_a) < (m_b)) ? (m_a) : (m_b))
#endif

#ifndef MAX
#define MAX(m_a, m_b) (((m_a) > (m_b)) ? (m_a) : (m_b))
#endif

#ifndef CLAMP
#define CLAMP(m_a, m_min, m_max) (((m_a) < (m_min)) ? (m_min) : (((m_a) > (m_max)) ? m_max : m_a))
#endif

/** Generic swap template */
#ifndef SWAP

#define SWAP(m_x, m_y) __swap_tmpl(m_x, m_y)
template <class T>
inline void __swap_tmpl(T &x, T &y) {

	T aux = x;
	x = y;
	y = aux;
}

#endif //swap

/* clang-format off */
#define HEX2CHR(m_hex) \
	((m_hex >= '0' && m_hex <= '9') ? (m_hex - '0') : \
	((m_hex >= 'A' && m_hex <= 'F') ? (10 + m_hex - 'A') : \
	((m_hex >= 'a' && m_hex <= 'f') ? (10 + m_hex - 'a') : 0)))
/* clang-format on */

#define __STRX(m_index) #m_index
#define __STR(m_index) __STRX(m_index)

#ifdef __GNUC__
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) x
#define unlikely(x) x
#endif
