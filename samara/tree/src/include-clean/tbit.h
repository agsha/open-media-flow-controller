/*
 *
 * tbit.h
 *
 *
 *
 */

#ifndef __TBIT_H_
#define __TBIT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file tbit.h Bitstring manuipulation.  The bitstrings are allocated by
 * the caller, and so can be very long.  Set, clear and test are available.
 * There is also a size allocation helper suitable for static or dynamic
 * use.
 * \ingroup lc
 */

#include "common.h"

/* 
 * Helpers to compute the byte number inside the whole bit string, and the
 * bit inside the byte.
 */

#define _tbit_byte_num(bit_num)                         \
    ( ((unsigned long)(bit_num)) >> 3 )
#define _tbit_per_byte_mask(bit_num)                    \
    ( 1 << (((unsigned int) (bit_num)) & 0x07) )

/* How many bytes do we need to hold a bit string this many bits long */
#define TBIT_SIZE(num_bits)                     \
    ( (((unsigned long) (num_bits)) + 7) >> 3 )

/** Set a bit in a bit field */
#define TBIT_SET(name, bit_num)                                 \
    ( ((unsigned char *) (name))[_tbit_byte_num(bit_num)] |=    \
      _tbit_per_byte_mask(bit_num) )

/** Clear a bit in a bit field */
#define TBIT_CLEAR(name, bit_num)                               \
    ( ((unsigned char *) (name))[_tbit_byte_num(bit_num)] &=    \
      (~ _tbit_per_byte_mask(bit_num)) )

/** Test the value of a bit in a bit field */
#define TBIT_TEST(name, bit_num)                                \
    ( (((unsigned char *) (name))[_tbit_byte_num(bit_num)] &    \
       _tbit_per_byte_mask(bit_num)) != 0 )

#ifdef __cplusplus
}
#endif

#endif /* __TBIT_H_ */
