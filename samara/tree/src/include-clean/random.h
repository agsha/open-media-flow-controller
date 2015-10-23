/*
 *
 * random.h
 *
 *
 *
 */

#ifndef __RANDOM_H_
#define __RANDOM_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file random.h Random number functions
 * \ingroup lc
 */

#include "common.h"

/**
 * Seed the random number generator.  Note that in extreme situations,
 * this could block.
 * \return 0 on success, error code on failure.
 */
int lc_random_seed(void);

/**
 * Seed the random number generator, potentially in a slightly less random
 * or secure way than lc_random_seed(), but without the possibility of
 * blocking.  Do not use for cryptography purposes.
 * \return 0 on success, error code on failure.
 */
int lc_random_seed_nonblocking(void);

/**
 * Seed the random number generator, if it has not already been seeded
 * in this process.  Note that in extreme situations, this could block.
 * \return 0 on success, error code on failure.
 */
int lc_random_seed_once(void);

/**
 * Seed the random number generator, if it has not already been seeded
 * in this process.  This is done in potentially in a slightly less
 * random or secure way than lc_random_seed(), but without the
 * possibility of blocking.  Do not use for cryptography purposes.
 * \return 0 on success, error code on failure.
 */
int lc_random_seed_nonblocking_once(void);

/**
 * Obtain an 8 bit random number.
 * \return uint8.
 */
uint8 lc_random_get_uint8(void);

/**
 * Obtain a 16 bit random number.
 * \return uint16.
 */
uint16 lc_random_get_uint16(void);

/**
 * Obtain a 32 bit random number.
 * \return uint32.
 */
uint32 lc_random_get_uint32(void);

/**
 * Obtain a 64 bit random number.
 * \return uint64.
 */
uint64 lc_random_get_uint64(void);

/**
 * Obtain a random number in [0..1).
 * \return double.
 */
double lc_random_get_fraction(void);

/**
 * Obtain an n-byte sequence of random bytes.
 * \param n the number of bytes to fetch.
 * \param ret_bytes the buffer to store the bytes in (must be >= n size).
 * \return 0 on success, error code on failure.
 */
int lc_random_get_bytes(uint16 n, uint8 *ret_bytes);

/**
 * Obtain an n-byte sequence of random bytes.
 * \param n the number of bytes to fetch.
 * \param ret_bytes the bytes.
 * \return 0 on success, error code on failure.
 */
int lc_random_get_bytes_new(uint16 n, uint8 **ret_bytes);

/**
 * Obtain an n-char length string of random characters (A-Z,a-z,0-9).
 * \param n size of the desired string (not including the NUL terminator).
 * The NUL terminator will be added automatically.
 * \param ret_string the buffer to store the bytes in (must be >= n + 1 size).
 * \return 0 on success, error code on failure.
 */
int lc_random_get_string(uint16 n, char *ret_string);

/**
 * Obtain an n-char length string of random characters (A-Z,a-z,0-9).
 * \param n the number of bytes to fetch.
 * \param ret_string the string.
 * \return 0 on success, error code on failure.
 */
int lc_random_get_string_new(uint16 n, char **ret_string);


#ifdef __cplusplus
}
#endif

#endif /* __RANDOM_H_ */
