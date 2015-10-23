/*
 *
 * ttypes.h
 *
 *
 *
 */

#ifndef __TTYPES_H_
#define __TTYPES_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* As we do not include common.h, check if this include is prohibited */
#if defined(NON_REDIST_HEADERS_PROHIBITED)
#error Non-redistributable header file included when prohibited
#endif

#include <stdint.h>
#include <sys/types.h>

/* ----------------------------------------------------------------------------
 * Atomic data types
 */

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float    float32;
typedef double   float64;

typedef intptr_t  int_ptr;
typedef uintptr_t uint_ptr;

/* Another approach to the code below would be to do: include <stdbool.h> */

#if defined __cplusplus || defined __bool_true_false_are_defined
typedef signed int tbool;
#else
typedef enum {false = 0, true = 1} tbool;
#endif

/*
 * Just use 32-bit quantities as indicators of whether these are already
 * defined, so we don't have to check all of them.
 */

/* Minimum of signed integral types.  */
#ifndef INT32_MIN
# define INT8_MIN               (-128)
# define INT16_MIN              (-32767-1)
# define INT32_MIN              (-2147483647-1)
# define INT64_MIN              (-__INT64_C(9223372036854775807)-1)
#endif

/* Maximum of signed integral types.  */
#ifndef INT32_MAX
# define INT8_MAX               (127)
# define INT16_MAX              (32767)
# define INT32_MAX              (2147483647)
# define INT64_MAX              (__INT64_C(9223372036854775807))
#endif

/* Maximum of unsigned integral types.  */
#ifndef UINT32_MAX
# define UINT8_MAX              (255)
# define UINT16_MAX             (65535)
# define UINT32_MAX             (4294967295U)
# define UINT64_MAX             (__UINT64_C(18446744073709551615))
#endif


#ifdef __cplusplus
}
#endif

#endif /* __TTYPES_H_ */
