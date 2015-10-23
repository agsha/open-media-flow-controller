/*
 *
 * typed_arrays.h
 *
 *
 *
 */

#ifndef __TYPED_ARRAYS_H_
#define __TYPED_ARRAYS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "typed_array_direct_tmpl.h"
#include "typed_array_tmpl.h"
#include "tstr_array.h"

/**
 * \file typed_arrays.h Declarations of common type-specific arrays
 * \ingroup lc_ds
 */

/* ========================================================================= */
/** Array of uint16 elements
 */
TYPED_ARRAY_DIRECT_HEADER_NEW_NONE(uint16, uint16);

/**
 * Allocate a new uint16 array
 */
int uint16_array_new(uint16_array **ret_arr);

/* ========================================================================= */
/** Array of uint32 elements
 */
TYPED_ARRAY_DIRECT_HEADER_NEW_NONE(uint32, uint32);

/**
 * Allocate a new uint32 array
 */
int uint32_array_new(uint32_array **ret_arr);

int uint32_array_new_size(uint32_array **ret_arr, uint32 initial_size);

/**
 * Find the minimum value that occurs in the array.
 *
 * \param arr Array to check
 *
 * \retval ret_min The minimum value that occurs in the array, or
 * UINT32_MAX if the array is empty.
 *
 * \return \c lc_err_not_found if the array is empty, otherwise zero for
 * success.
 */
int uint32_array_get_min(const uint32_array *arr, uint32 *ret_min);

/**
 * Find the maximum value that occurs in the array.
 *
 * \param arr Array to check
 *
 * \retval ret_max The maximum value that occurs in the array, or 0 if
 * the array is empty.
 *
 * \return \c lc_err_not_found if the array is empty, otherwise zero for
 * success.
 */
int uint32_array_get_max(const uint32_array *arr, uint32 *ret_max);

/**
 * Find the first number that does not occur in the array.
 * Note: requires that the array be sorted.
 */
int uint32_array_get_first_absent(const uint32_array *arr, uint32 *ret_first);

/**
 * Given a string that is a comma separated list of numbers,
 * Get the list into a number array and in sorted order.
 */
int uint32_array_new_from_list(uint32_array **ret_arr, uint32 *ret_num_elems,
                           const char *comma_list);

/**
 * Create a list out of a set of numbers.  The parameters following
 * ret_arr are all read as int32, so values greater than INT32_MAX
 * cannot be included.  Anything less than zero, or greater than
 * INT32_MAX will terminate the list; by convention, we suggest using -1.
 */
int uint32_array_new_va(uint32_array **ret_arr, ...);

/**
 * Given a tstr_array whose elements are string representations of 
 * uint32s, create a uint32 array that has the native representations
 * of those numbers.
 *
 * Any entries in the string array which are not pure integers, or
 * are out of range for a uint32, are ignored.
 */
int uint32_array_new_from_tstr_array(uint32_array **ret_arr,
                                     const tstr_array *tsa);

/**
 * Give a uint32_array, produce a string representing the array as
 * a comma separated list of the numbers
 */
int uint32_array_to_list(const uint32_array *num_arr, char **ret_comma_list);


/* ========================================================================= */
/** Array of uint64 pointer elements
 */
TYPED_ARRAY_HEADER_NEW_NONE(uint64, uint64 *);

int uint64_array_new(uint64_array **ret_arr);

/* ========================================================================= */
/** Array of int64 pointer elements
 */
TYPED_ARRAY_HEADER_NEW_NONE(int64, int64 *);

int int64_array_new(int64_array **ret_arr);


/* ========================================================================= */
/** Array of float32 elements
 */
TYPED_ARRAY_DIRECT_HEADER_NEW_NONE(float32, float32);

int float32_array_new(float32_array **ret_arr);

/* ========================================================================= */
/** Array of tbool elements
 */
TYPED_ARRAY_DIRECT_HEADER_NEW_NONE(tbool, tbool);

int tbool_array_new(tbool_array **ret_arr);

/* ========================================================================= */
/** Array of tstr_array pointer elements
 */
TYPED_ARRAY_HEADER_NEW_NONE(tstr_array, tstr_array *);

int tstr_array_array_new(tstr_array_array **ret_arr);

#ifdef __cplusplus
}
#endif

#endif /* __TYPED_ARRAYS_H_ */
