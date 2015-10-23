/*
 *
 * typed_array_direct_tmpl.h
 *
 *
 *
 */

#ifndef __TYPED_ARRAY_DIRECT_TMPL_H_
#define __TYPED_ARRAY_DIRECT_TMPL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "array.h"

/* ========================================================================= */
/** \file typed_array_direct_tmpl.h Macros to create type-specific arrays.
 * \ingroup lc_ds
 *
 * This file contains macros that can be used to declare new array
 * types that hold a particular data type. The array direct arrays
 * differ from generic array and other template arrays in that they
 * directly store data in the array element (instead of a pointer to
 * their data). Since the generic array has element space for a 'void *',
 * the TYPE of a direct array must be of a size that is smaller than
 * the size of a 'void *'. The 'void *' size is equivalent to the size
 * of the type 'int_ptr'.
 * Unlike the generic array, the API generated will only accept arrays
 * and elements of the correct type, and does not require elements to
 * be typecast when added or removed.
 *
 * NOTE: Direct arrays inherently does not work with larger types because
 * the elements are cast as void *s to be stored in the array.
 * XXX If the sizeof(TYPE) is larger than the sizeof(int_ptr)
 * then it won't. Currently we bail if this is detected.
 *
 * There are three sets of macros available are as listed below.  For
 * normal header declarations:
 * \li TYPED_ARRAY_DIRECT_HEADER_NEW_DEFAULTS(name, type)
 * \li TYPED_ARRAY_DIRECT_HEADER_NEW_FULL(name, type)
 * \li TYPED_ARRAY_DIRECT_HEADER_NEW_NONE(name, type)
 *
 * For header declarations where you need to split up the type declaration 
 * and the function prototypes:
 * \li TYPED_ARRAY_DIRECT_PRE_DECL(name)
 * \li TYPED_ARRAY_DIRECT_HEADER_BODY_NEW_DEFAULTS(name, type)
 * \li TYPED_ARRAY_DIRECT_HEADER_BODY_NEW_FULL(name, type)
 * \li TYPED_ARRAY_DIRECT_HEADER_BODY_NEW_NONE(name, type)
 *
 * For the implementation:
 * \li TYPED_ARRAY_DIRECT_IMPL_NEW_DEFAULTS(name, type, empty value)
 * \li TYPED_ARRAY_DIRECT_IMPL_NEW_FULL(name, type, empty value)
 * \li TYPED_ARRAY_DIRECT_IMPL_NEW_NONE(name, type, empty value)
 *
 * To declare a new array type:
 *
 * \li Call one of the TYPED_ARRAY_DIRECT_HEADER... variants from your header
 * file.  For example, TYPED_ARRAY_DIRECT_HEADER_NEW_NONE(str, char *) was
 * used to declare the str_array type whose elements are of type char *,
 * and which declares its own new function to supply customized
 * array options.
 *
 * \li Call one of the TYPED_ARRAY_DIRECT_IMPL... variants from your .c file.
 * The empty value parameter specifies what should be used as an empty
 * element as the return value for ..._array_get_quick() if there is
 * an error.  Generally it will be 0 or -1 for integers, and NULL for
 * pointers.
 *
 * \li Have your .c file include \c stdlib.h.  The macro can't do this
 * for you because preprocessor directives like \#include cannot be
 * nested inside macro definitions.
 *
 * If you need to pre-declare the type, separately from listing the
 * prototypes for all of its functions (e.g. tstr_array needs this to
 * resolve a mutual dependency with the tstring data type), use
 * TYPED_ARRAY_DIRECT_PRE_DECL for that, and then use a
 * TYPED_ARRAY_DIRECT_HEADER_BODY... variant for the remainder of the header.
 *
 * Except for array creation, the API generated will be the same as
 * for the base array type, with the names of the functions (and the
 * foreach callback function type) prefaced with the name given when
 * you declared the type, and an underscore.  What array creation
 * functions are made available is determined by which of the macro
 * variants is used:
 *
 * \li ..._NEW_DEFAULTS declares ..._array_new() to take no options,
 * and accept the baseline array's default options by passing NULL to
 * array_new().  It also declares ..._array_full() to take an
 * array_options pointer.  Use this when most/all instances of this
 * typed array are going to want the standard array default options.
 *
 * \li ..._NEW_FULL declares only ..._array_new() to take an array
 * options structure.  Use this if you need maximum flexibility for
 * creating all of your arrays (or before you have gotten around to
 * writing your own type-specific new function and don't want to add
 * "_full" to the end of your function calls)
 *
 * \li ..._NEW_NONE declares only ..._array_full() to take an
 * array_options pointer.  Use this when you want to write your own
 * custom ..._array_new() function which uses its own special set of
 * array options.
 *
 * Note: the array types declared are structures containing an array,
 * rather than typedef'd to be an array, because the latter would make
 * them all type-compatible and the compiler wouldn't catch you if you
 * were feeding it the wrong type of the array.
 */


/* ------------------------------------------------------------------------- */
/* Header                                                                    */
/* ------------------------------------------------------------------------- */

#define TYPED_ARRAY_DIRECT_PRE_DECL(NAME)                                     \
                                                                              \
typedef struct NAME ## _array NAME ## _array;

#define TYPED_ARRAY_DIRECT_NEW_FULL_H(NAME)                                   \
                                                                              \
    int NAME ## _array_new(NAME ## _array **ret_arr,                          \
                           const array_options *opts);

#define TYPED_ARRAY_DIRECT_NEW_DEFAULTS_H(NAME)                               \
                                                                              \
    int NAME ## _array_new(NAME ## _array **ret_arr);                         \
                                                                              \
    int NAME ## _array_new_full(NAME ## _array **ret_arr,                     \
                                const array_options *opts);

#define TYPED_ARRAY_DIRECT_NEW_NONE_H(NAME)                                   \
                                                                              \
    int NAME ## _array_new_full(NAME ## _array **ret_arr,                     \
                                const array_options *opts);

#define TYPED_ARRAY_DIRECT_HEADER_BODY(NAME, TYPE)                            \
                                                                              \
struct NAME ## _array {                                                       \
    array * NAME ## _arr;                                                     \
};                                                                            \
                                                                              \
int NAME ## _array_options_get(NAME ## _array *arr,                           \
                               array_options *ret_options);                   \
                                                                              \
int NAME ## _array_options_set(NAME ## _array *arr,                           \
                               const array_options *new_options,              \
                               array_options *ret_old_options);               \
                                                                              \
int NAME ## _array_free(NAME ## _array **inout_arr);                          \
                                                                              \
int NAME ## _array_empty(NAME ## _array *arr);                                \
                                                                              \
int NAME ## _array_compact(NAME ## _array *arr);                              \
                                                                              \
int NAME ## _array_compact_empty_slots(NAME ## _array *arr);                  \
                                                                              \
int NAME ## _array_delete_duplicates(NAME ## _array *arr, uint32 flags);      \
                                                                              \
int NAME ## _array_ensure_size_min_exact(NAME ## _array *arr,                 \
                                         uint32 num_elems);                   \
                                                                              \
int NAME ## _array_duplicate(const NAME ## _array *src,                       \
                             NAME ## _array **ret_dest);                      \
                                                                              \
int NAME ## _array_concat(NAME ## _array *base,                               \
                          NAME ## _array **inout_extra);                      \
                                                                              \
int NAME ## _array_append_array(NAME ## _array *base,                         \
                                const NAME ## _array *extra);                 \
                                                                              \
int NAME ## _array_length(const NAME ## _array *arr, uint32 *ret_length);     \
                                                                              \
uint32 NAME ## _array_length_quick(const NAME ## _array *arr);                \
                                                                              \
int NAME ## _array_equal(const NAME ## _array *arr1,                          \
                         const NAME ## _array *arr2, tbool *ret_equal);       \
                                                                              \
int NAME ## _array_get(const NAME ## _array *arr, uint32 idx,                 \
                       TYPE *ret_elem);                                       \
                                                                              \
TYPE NAME ## _array_get_quick(const NAME ## _array *arr, uint32 idx);         \
                                                                              \
int NAME ## _array_set(NAME ## _array *arr, uint32 idx, const TYPE elem);     \
                                                                              \
int NAME ## _array_append(NAME ## _array *arr, const TYPE elem);              \
                                                                              \
int NAME ## _array_insert(NAME ## _array *arr, uint32 idx, const TYPE elem);  \
                                                                              \
int NAME ## _array_insert_sorted(NAME ## _array *arr, const TYPE elem);       \
                                                                              \
int NAME ## _array_insert_sorted_hint(NAME ## _array *arr, const TYPE elem,   \
                                      uint32 hint_index);                     \
                                                                              \
int NAME ## _array_find_insertion_point(const NAME ## _array *arr,            \
                                        const TYPE elem, uint32 *ret_index);  \
                                                                              \
int NAME ## _array_find_insertion_point_hint(const NAME ## _array *arr,       \
                                             const void *elem,                \
                                             uint32 hint_index,               \
                                             uint32 *ret_index);              \
                                                                              \
int NAME ## _array_delete(NAME ## _array *arr, uint32 idx);                   \
                                                                              \
int NAME ## _array_delete_no_shift(NAME ## _array *arr, uint32 idx);          \
                                                                              \
int NAME ## _array_remove(NAME ## _array *arr, uint32 idx, TYPE *ret_elem);   \
                                                                              \
int NAME ## _array_remove_no_shift(NAME ## _array *arr, uint32 idx,           \
                                   TYPE *ret_elem);                           \
                                                                              \
int NAME ## _array_delete_by_value(NAME ## _array *arr, TYPE elem,            \
                                   tbool allow_multiple);                     \
                                                                              \
int NAME ## _array_remove_by_value(NAME ## _array *arr, TYPE elem,            \
                                   TYPE *ret_elem);                           \
                                                                              \
int NAME ## _array_delete_range(NAME ## _array *arr, uint32 idx,              \
                                int32 num_elems, tbool shift_to_cover);       \
                                                                              \
int NAME ## _array_push(NAME ## _array *arr, const TYPE elem);                \
                                                                              \
int NAME ## _array_pop(NAME ## _array *arr, TYPE *ret_elem);                  \
                                                                              \
int NAME ## _array_peek(const NAME ## _array *arr, TYPE *ret_elem);           \
                                                                              \
int NAME ## _array_sort(NAME ## _array *arr);                                 \
                                                                              \
int NAME ## _array_is_sorted(const NAME ## _array *arr, tbool *ret_is_sorted);\
                                                                              \
int NAME ## _array_update_sorted(NAME ## _array *arr);                        \
                                                                              \
int NAME ## _array_sort_custom(NAME ## _array *arr,                           \
                               array_elem_compare_func custom_compare_func);  \
                                                                              \
int NAME ## _array_binary_search(const NAME ## _array *arr,                   \
                                 const TYPE model_elem,                       \
                                 uint32 *ret_found_index);                    \
                                                                              \
int NAME ## _array_binary_search_hint(const NAME ## _array *arr,              \
                                      const TYPE model_elem,                  \
                                      uint32 hint_index,                      \
                                      uint32 *ret_found_index);               \
                                                                              \
int NAME ## _array_linear_search(const NAME ## _array *arr,                   \
                                 const TYPE model_elem,                       \
                                 uint32 start_index,                          \
                                 uint32 *ret_found_index);                    \
                                                                              \
int NAME ## _array_linear_search_custom(const NAME ## _array *arr,            \
                                 const TYPE model_elem,                       \
                                 array_elem_compare_func custom_compare_func, \
                                 uint32 start_index,                          \
                                 uint32 *ret_found_index);                    \
                                                                              \
int NAME ## _array_best_search(const NAME ## _array *arr,                     \
                               const TYPE model_elem,                         \
                               uint32 *ret_found_index);                      \
                                                                              \
typedef int (* NAME ## _array_foreach_func)                                   \
(const NAME ## _array *arr, uint32 idx, TYPE elem, void *data);               \
                                                                              \
typedef struct NAME ## _array_foreach_shim_args                               \
    NAME ## _array_foreach_shim_args;                                         \
                                                                              \
struct NAME ## _array_foreach_shim_args {                                     \
    const NAME ## _array *afsa_arr;                                           \
    NAME ## _array_foreach_func afsa_func;                                    \
    void *afsa_data;                                                          \
};                                                                            \
                                                                              \
int NAME ## _array_foreach_shim(const array *arr, uint32 idx,                 \
                                void *elem, void *data);                      \
                                                                              \
int NAME ## _array_foreach(const NAME ## _array *arr,                         \
                           NAME ## _array_foreach_func foreach_func,          \
                           void *foreach_data);                               \
                                                                              \
int NAME ## _array_foreach_delete(NAME ## _array *arr,                        \
                                  NAME ## _array_foreach_func foreach_func,   \
                                  void *foreach_data); 

#define TYPED_ARRAY_DIRECT_HEADER_BODY_NEW_DEFAULTS(NAME, TYPE)               \
        TYPED_ARRAY_DIRECT_NEW_DEFAULTS_H(NAME)                               \
        TYPED_ARRAY_DIRECT_HEADER_BODY(NAME, TYPE)

#define TYPED_ARRAY_DIRECT_HEADER_BODY_NEW_FULL(NAME, TYPE)                   \
        TYPED_ARRAY_DIRECT_NEW_FULL_H(NAME)                                   \
        TYPED_ARRAY_DIRECT_HEADER_BODY(NAME, TYPE)

#define TYPED_ARRAY_DIRECT_HEADER_BODY_NEW_NONE(NAME, TYPE)                   \
        TYPED_ARRAY_DIRECT_NEW_NONE_H(NAME)                                   \
        TYPED_ARRAY_DIRECT_HEADER_BODY(NAME, TYPE)

#define TYPED_ARRAY_DIRECT_HEADER_NEW_DEFAULTS(NAME, TYPE)                    \
        TYPED_ARRAY_DIRECT_PRE_DECL(NAME)                                     \
        TYPED_ARRAY_DIRECT_HEADER_BODY_NEW_DEFAULTS(NAME, TYPE)

#define TYPED_ARRAY_DIRECT_HEADER_NEW_FULL(NAME, TYPE)                        \
        TYPED_ARRAY_DIRECT_PRE_DECL(NAME)                                     \
        TYPED_ARRAY_DIRECT_HEADER_BODY_NEW_FULL(NAME, TYPE)

#define TYPED_ARRAY_DIRECT_HEADER_NEW_NONE(NAME, TYPE)                        \
        TYPED_ARRAY_DIRECT_PRE_DECL(NAME)                                     \
        TYPED_ARRAY_DIRECT_HEADER_BODY_NEW_NONE(NAME, TYPE)


/* ------------------------------------------------------------------------- */
/* Internal use helpers                                                      */
/* ------------------------------------------------------------------------- */

/*
 * Test if X is NULL; log an error and return error code if it is,
 * and do CMD if is isn't.
 */
#define IF_NON_NULL(X, CMD)                                                   \
    if (X == NULL) {                                                          \
        lc_log_debug(LOG_ERR, "Unexpected NULL");                             \
        bail_debug();                                                         \
        return(lc_err_unexpected_null);                                       \
    }                                                                         \
    else {                                                                    \
        CMD ;                                                                 \
    }

#define IF_EITHER_NON_NULL(X, Y, CMD)                                         \
    if ((X == NULL) || (Y == NULL)) {                                         \
        lc_log_debug(LOG_ERR, "Unexpected NULL");                             \
        bail_debug();                                                         \
        return(lc_err_unexpected_null);                                       \
    }                                                                         \
    else {                                                                    \
        CMD ;                                                                 \
    }

/*
 * Same as IF_NON_NULL, except return RET instead of an error code if
 * test fails.
 */
#define IF_NON_NULL_SPECIAL(X, RET, CMD)                                      \
    if (X == NULL) {                                                          \
        lc_log_debug(LOG_ERR, "Unexpected NULL");                             \
        bail_debug();                                                         \
        return(RET);                                                          \
    }                                                                         \
    else {                                                                    \
        CMD ;                                                                 \
    }

/*
 * Same as IF_NON_NULL_SPECIAL, except don't log an error if X is NULL,
 * as the caller is allowed to pass a NULL to this function.
 */
#define IF_NON_NULL_SPECIAL_QUIET(X, RET, CMD)                                \
    if (X == NULL) {                                                          \
        return(RET);                                                          \
    }                                                                         \
    else {                                                                    \
        CMD ;                                                                 \
    }


/* ------------------------------------------------------------------------- */
/* Implementation                                                            */
/* ------------------------------------------------------------------------- */

#define TYPED_ARRAY_DIRECT_NEW_FULL_C(NAME, TYPE)                             \
                                                                              \
int NAME ## _array_new_full(NAME ## _array **ret_arr,                         \
                            const array_options *opts)                        \
{                                                                             \
    int err = 0;                                                              \
    uint32 size_needed = 0;                                                   \
                                                                              \
    bail_null(ret_arr);                                                       \
    bail_require(sizeof(TYPE) <= sizeof(int_ptr));                            \
                                                                              \
    size_needed = array_get_prealloc_size_quick() +                           \
        sizeof(NAME ## _array);                                               \
    *ret_arr = (NAME ## _array *) malloc(size_needed);                        \
    bail_null(*ret_arr);                                                      \
                                                                              \
    err = array_new_prealloc(&((*ret_arr)->NAME ## _arr),                     \
                             opts,                                            \
                             false,                                           \
                             NULL,                                            \
                             (char *) *ret_arr + sizeof(NAME ## _array),      \
                             size_needed - sizeof(NAME ## _array));           \
    bail_error(err);                                                          \
                                                                              \
bail:                                                                         \
    return(err);                                                              \
}

#define TYPED_ARRAY_DIRECT_NEW_OPTS_C(NAME, TYPE)                             \
                                                                              \
int NAME ## _array_new(NAME ## _array **ret_arr,                              \
                       const array_options *opts)                             \
{                                                                             \
    int err = 0;                                                              \
    uint32 size_needed = 0;                                                   \
                                                                              \
    bail_null(ret_arr);                                                       \
    bail_require(sizeof(TYPE) <= sizeof(int_ptr));                            \
                                                                              \
    size_needed = array_get_prealloc_size_quick() +                           \
        sizeof(NAME ## _array);                                               \
    *ret_arr = (NAME ## _array *) malloc(size_needed);                        \
    bail_null(*ret_arr);                                                      \
                                                                              \
    err = array_new_prealloc(&((*ret_arr)->NAME ## _arr),                     \
                             opts,                                            \
                             false,                                           \
                             NULL,                                            \
                             (char *) *ret_arr + sizeof(NAME ## _array),      \
                             size_needed - sizeof(NAME ## _array));           \
    bail_error(err);                                                          \
                                                                              \
bail:                                                                         \
    return(err);                                                              \
}

#define TYPED_ARRAY_DIRECT_NEW_DEFAULTS_C(NAME, TYPE)                         \
                                                                              \
int NAME ## _array_new(NAME ## _array **ret_arr)                              \
{                                                                             \
    int err = 0;                                                              \
                                                                              \
    bail_require(sizeof(TYPE) <= sizeof(int_ptr));                            \
    err = NAME ## _array_new_full(ret_arr, NULL);                             \
                                                                              \
bail:                                                                         \
    return(err);                                                              \
}

#define TYPED_ARRAY_DIRECT_IMPL(NAME, TYPE, EMPTY_VALUE)                      \
                                                                              \
int NAME ## _array_options_get(NAME ## _array *arr,                           \
                               array_options *ret_options)                    \
{ IF_NON_NULL(arr, return(array_options_get(arr-> NAME ## _arr,               \
                                            ret_options))); }                 \
                                                                              \
int NAME ## _array_options_set(NAME ## _array *arr,                           \
                               const array_options *new_options,              \
                               array_options *ret_old_options)                \
{ IF_NON_NULL(arr, return(array_options_set(arr-> NAME ## _arr,               \
                                            new_options,                      \
                                            ret_old_options))); }             \
                                                                              \
int NAME ## _array_free(NAME ## _array **inout_arr)                           \
{                                                                             \
    int err = 0;                                                              \
                                                                              \
    bail_null(inout_arr);                                                     \
    if (*inout_arr) {                                                         \
        array_free(&((*inout_arr)-> NAME ## _arr));                           \
        safe_free(*inout_arr);                                                \
    }                                                                         \
                                                                              \
 bail:                                                                        \
    return(err);                                                              \
}                                                                             \
                                                                              \
int NAME ## _array_empty(NAME ## _array *arr)                                 \
{ IF_NON_NULL(arr, return(array_empty(arr-> NAME ## _arr))) }                 \
                                                                              \
int NAME ## _array_compact(NAME ## _array *arr)                               \
{ IF_NON_NULL(arr, return(array_compact(arr-> NAME ## _arr))) }               \
                                                                              \
int NAME ## _array_compact_empty_slots(NAME ## _array *arr)                   \
{ IF_NON_NULL(arr, return(array_compact_empty_slots(arr-> NAME ## _arr))) }   \
                                                                              \
int NAME ## _array_delete_duplicates(NAME ## _array *arr, uint32 flags)       \
{ IF_NON_NULL(arr, return(array_delete_duplicates(arr-> NAME ## _arr,         \
              flags))) }                                                      \
                                                                              \
int NAME ## _array_ensure_size_min_exact(NAME ## _array *arr,                 \
                                         uint32 num_elems)                    \
{ IF_NON_NULL(arr, return(array_ensure_size_min_exact(arr-> NAME ## _arr,     \
                                                      num_elems))) }          \
                                                                              \
int NAME ## _array_duplicate(const NAME ## _array *src,                       \
                             NAME ## _array **ret_dest)                       \
{                                                                             \
    int err = 0;                                                              \
    uint32 size_needed = 0;                                                   \
                                                                              \
    bail_null(ret_dest);                                                      \
    *ret_dest = NULL;                                                         \
                                                                              \
    if (!src) {                                                               \
        goto bail;                                                            \
    }                                                                         \
                                                                              \
    size_needed = array_get_prealloc_size_quick() +                           \
        sizeof(NAME ## _array);                                               \
    *ret_dest = (NAME ## _array *) malloc(size_needed);                       \
    bail_null(*ret_dest);                                                     \
                                                                              \
    err = array_duplicate_prealloc(src-> NAME ## _arr,                        \
                                   &((*ret_dest)->NAME ## _arr),              \
                                   false,                                     \
                                   NULL,                                      \
                                   (char *) *ret_dest +                       \
                                   sizeof(NAME ## _array),                    \
                                   size_needed - sizeof(NAME ## _array));     \
   bail_error(err);                                                          \
                                                                              \
 bail:                                                                        \
    return(err);                                                              \
}                                                                             \
                                                                              \
int NAME ## _array_concat(NAME ## _array *base,                               \
                          NAME ## _array **inout_extra)                       \
{                                                                             \
    int err = 0;                                                              \
    NAME ## _array *extra = NULL;                                             \
                                                                              \
    bail_null(base);                                                          \
    bail_null(inout_extra);                                                   \
    extra = *inout_extra;                                                     \
    bail_null(extra);                                                         \
                                                                              \
    err = array_concat(base-> NAME ## _arr, &(extra-> NAME ## _arr));         \
    bail_error(err);                                                          \
                                                                              \
    safe_free(*inout_extra);                                                  \
                                                                              \
bail:                                                                         \
 return(err);                                                                 \
}                                                                             \
                                                                              \
int NAME ## _array_append_array(NAME ## _array *base,                         \
                                const NAME ## _array *extra)                  \
{                                                                             \
    int err = 0;                                                              \
                                                                              \
    bail_null(base);                                                          \
    bail_null(extra);                                                         \
                                                                              \
    err = array_append_array(base-> NAME ## _arr, extra-> NAME ## _arr);      \
    bail_error(err);                                                          \
                                                                              \
bail:                                                                         \
 return(err);                                                                 \
}                                                                             \
                                                                              \
int NAME ## _array_length(const NAME ## _array *arr, uint32 *ret_length)      \
{ IF_NON_NULL(arr, return(array_length(arr-> NAME ## _arr, ret_length))) }    \
                                                                              \
uint32 NAME ## _array_length_quick(const NAME ## _array *arr)                 \
{ IF_NON_NULL_SPECIAL_QUIET(arr, 0, return                                    \
  (array_length_quick(arr-> NAME ## _arr))) }                                 \
                                                                              \
int NAME ## _array_equal(const NAME ## _array *arr1,                          \
                         const NAME ## _array *arr2, tbool *ret_equal)        \
{ IF_EITHER_NON_NULL(arr1, arr2,                                              \
  return(array_equal(arr1-> NAME ## _arr,                                     \
                     arr2-> NAME ## _arr,  ret_equal))); }                    \
                                                                              \
int NAME ## _array_get                                                        \
    (const NAME ## _array *arr, uint32 idx, TYPE *ret_elem)                   \
{                                                                             \
    int err = 0;                                                              \
    int_ptr local_elem;                                                       \
                                                                              \
    bail_null(arr);                                                           \
    err = array_get(arr-> NAME ## _arr, idx, (void **) &local_elem);          \
    bail_error(err);                                                          \
    *ret_elem = local_elem;                                                   \
                                                                              \
 bail:                                                                        \
    return(err);                                                              \
}                                                                             \
                                                                              \
TYPE NAME ## _array_get_quick(const NAME ## _array *arr, uint32 idx)          \
{                                                                             \
    void *val_v;                                                              \
    TYPE val_t;                                                               \
                                                                              \
    if (arr == NULL) {                                                        \
        lc_log_debug(LOG_ERR, "Unexpected NULL");                             \
        bail_debug();                                                         \
        return(EMPTY_VALUE);                                                  \
    }                                                                         \
    else {                                                                    \
        val_v = array_get_quick(arr-> NAME ## _arr, idx);                     \
        val_t = (TYPE) (int_ptr) val_v;                                       \
        return(val_t);                                                        \
    }                                                                         \
}                                                                             \
                                                                              \
int NAME ## _array_set(NAME ## _array *arr, uint32 idx, const TYPE elem)      \
{ IF_NON_NULL(arr, return(array_set(arr-> NAME ## _arr, idx,                  \
              (void *) (int_ptr) elem))) }                                    \
                                                                              \
int NAME ## _array_append(NAME ## _array *arr, const TYPE elem)               \
{ IF_NON_NULL(arr, return(array_append(arr-> NAME ## _arr,                    \
                                       (void *) (int_ptr) elem))) }           \
                                                                              \
int NAME ## _array_insert(NAME ## _array *arr, uint32 idx, const TYPE elem)   \
{ IF_NON_NULL(arr, return(array_insert(arr-> NAME ## _arr, idx,               \
              (void *) (int_ptr) elem))) }                                    \
                                                                              \
int NAME ## _array_insert_sorted(NAME ## _array *arr, const TYPE elem)        \
{ IF_NON_NULL(arr, return(array_insert_sorted(arr-> NAME ## _arr,             \
              (void *) (int_ptr) elem))) }                                    \
                                                                              \
int NAME ## _array_insert_sorted_hint(NAME ## _array *arr, const TYPE elem,   \
                                      uint32 hint_index)                      \
{ IF_NON_NULL(arr, return(array_insert_sorted_hint                            \
              (arr-> NAME ## _arr, (void *) (int_ptr) elem, hint_index))) }   \
                                                                              \
int NAME ## _array_find_insertion_point                                       \
    (const NAME ## _array *arr, const TYPE elem, uint32 *ret_index)           \
{ IF_NON_NULL(arr, return(array_find_insertion_point(arr-> NAME ## _arr,      \
              (void *) (int_ptr) elem, ret_index))) }                         \
                                                                              \
int NAME ## _array_find_insertion_point_hint                                  \
    (const NAME ## _array *arr, const void *elem,                             \
     uint32 hint_index, uint32 *ret_index)                                    \
{ IF_NON_NULL(arr, return(array_find_insertion_point_hint                     \
    (arr-> NAME ## _arr, elem, hint_index, ret_index))) }                     \
                                                                              \
int NAME ## _array_delete(NAME ## _array *arr, uint32 idx)                    \
{ IF_NON_NULL(arr, return(array_delete(arr-> NAME ## _arr, idx))) }           \
                                                                              \
int NAME ## _array_delete_no_shift(NAME ## _array *arr, uint32 idx)           \
{ IF_NON_NULL(arr, return(array_delete_no_shift(arr-> NAME ## _arr, idx))) }  \
                                                                              \
int NAME ## _array_remove(NAME ## _array *arr, uint32 idx, TYPE *ret_elem)    \
{                                                                             \
    int err = 0;                                                              \
    int_ptr local_elem;                                                       \
                                                                              \
    bail_null(arr);                                                           \
    err = array_remove(arr-> NAME ## _arr, idx, (void **) &local_elem);       \
    bail_error(err);                                                          \
    *ret_elem = local_elem;                                                   \
                                                                              \
 bail:                                                                        \
    return(err);                                                              \
}                                                                             \
                                                                              \
int NAME ## _array_remove_no_shift(NAME ## _array *arr, uint32 idx,           \
                                   TYPE *ret_elem)                            \
{                                                                             \
    int err = 0;                                                              \
    int_ptr local_elem;                                                       \
                                                                              \
    bail_null(arr);                                                           \
    err = array_remove_no_shift(arr-> NAME ## _arr, idx,                      \
                               (void **) &local_elem);                        \
    bail_error(err);                                                          \
    *ret_elem = local_elem;                                                   \
                                                                              \
 bail:                                                                        \
    return(err);                                                              \
}                                                                             \
                                                                              \
int NAME ## _array_delete_by_value(NAME ## _array *arr, TYPE elem,            \
                                   tbool allow_multiple)                      \
{ IF_NON_NULL(arr, return(array_delete_by_value                               \
              (arr-> NAME ## _arr, (void *) (int_ptr) elem,                   \
               allow_multiple))) }                                            \
                                                                              \
int NAME ## _array_remove_by_value(NAME ## _array *arr, TYPE elem,            \
                                   TYPE *ret_elem)                            \
{                                                                             \
    int err = 0;                                                              \
    int_ptr local_elem;                                                       \
                                                                              \
    bail_null(arr);                                                           \
    err = array_remove_by_value(arr-> NAME ## _arr, (void *) (int_ptr) elem,  \
                                (void **) &local_elem);                       \
    bail_error(err);                                                          \
    *ret_elem = local_elem;                                                   \
                                                                              \
 bail:                                                                        \
    return(err);                                                              \
}                                                                             \
                                                                              \
int NAME ## _array_delete_range(NAME ## _array *arr, uint32 idx,              \
                                int32 num_elems, tbool shift_to_cover)        \
{ IF_NON_NULL(arr, return(array_delete_range(arr-> NAME ## _arr, idx,         \
                                             num_elems, shift_to_cover))) }   \
                                                                              \
int NAME ## _array_push(NAME ## _array *arr, const TYPE elem)                 \
{ IF_NON_NULL(arr, return(array_push(arr-> NAME ## _arr,                      \
                                     (void *) (int_ptr) elem))) }             \
                                                                              \
int NAME ## _array_pop(NAME ## _array *arr, TYPE *ret_elem)                   \
{                                                                             \
    int err = 0;                                                              \
    int_ptr local_elem;                                                       \
                                                                              \
    bail_null(arr);                                                           \
    err = array_pop(arr-> NAME ## _arr, (void **) &local_elem);               \
    bail_error(err);                                                          \
    *ret_elem = local_elem;                                                   \
                                                                              \
 bail:                                                                        \
    return(err);                                                              \
}                                                                             \
                                                                              \
int NAME ## _array_peek(const NAME ## _array *arr, TYPE *ret_elem)            \
{                                                                             \
    int err = 0;                                                              \
    int_ptr local_elem;                                                       \
                                                                              \
    bail_null(arr);                                                           \
    err = array_peek(arr-> NAME ## _arr, (void **) &local_elem);              \
    bail_error(err);                                                          \
    *ret_elem = local_elem;                                                   \
                                                                              \
 bail:                                                                        \
    return(err);                                                              \
}                                                                             \
                                                                              \
int NAME ## _array_sort(NAME ## _array *arr)                                  \
{ IF_NON_NULL(arr, return(array_sort(arr-> NAME ## _arr))) }                  \
                                                                              \
int NAME ## _array_is_sorted(const NAME ## _array *arr, tbool *ret_is_sorted) \
{ IF_NON_NULL(arr, return(array_is_sorted(arr-> NAME ## _arr,                 \
                                          ret_is_sorted))) }                  \
                                                                              \
int NAME ## _array_update_sorted(NAME ## _array *arr)                         \
{ IF_NON_NULL(arr, return(array_update_sorted(arr-> NAME ## _arr))) }         \
                                                                              \
int NAME ## _array_sort_custom(NAME ## _array *arr,                           \
                               array_elem_compare_func custom_compare_func)   \
{ IF_NON_NULL(arr, return(array_sort_custom(arr-> NAME ## _arr,               \
                          custom_compare_func))) }                            \
                                                                              \
int NAME ## _array_binary_search(const NAME ## _array *arr,                   \
                                 const TYPE model_elem,                       \
                                 uint32 *ret_found_index)                     \
{ IF_NON_NULL(arr, return(array_binary_search(arr-> NAME ## _arr,             \
                          (void *) (int_ptr) model_elem, ret_found_index))) } \
                                                                              \
int NAME ## _array_binary_search_hint                                         \
    (const NAME ## _array *arr, const TYPE model_elem,                        \
     uint32 hint_index, uint32 *ret_found_index)                              \
{ IF_NON_NULL(arr, return(array_binary_search_hint(arr-> NAME ## _arr,        \
    (void *) (int_ptr) model_elem, hint_index, ret_found_index))) }           \
                                                                              \
int NAME ## _array_linear_search                                              \
    (const NAME ## _array *arr, const TYPE model_elem,                        \
     uint32 start_index, uint32 *ret_found_index)                             \
{ IF_NON_NULL(arr, return(array_linear_search(arr-> NAME ## _arr,             \
     (void *) (int_ptr) model_elem, start_index, ret_found_index))) }         \
                                                                              \
int NAME ## _array_linear_search_custom                                       \
    (const NAME ## _array *arr, const TYPE model_elem,                        \
     array_elem_compare_func custom_compare_func,                             \
     uint32 start_index, uint32 *ret_found_index)                             \
{ IF_NON_NULL(arr, return(array_linear_search_custom(arr-> NAME ## _arr,      \
     (void *) (int_ptr) model_elem, custom_compare_func, start_index,         \
     ret_found_index))) }                                                     \
                                                                              \
int NAME ## _array_best_search(const NAME ## _array *arr,                     \
                               const TYPE model_elem,                         \
                               uint32 *ret_found_index)                       \
{ IF_NON_NULL(arr, return(array_best_search(arr-> NAME ## _arr,               \
                          (void *) (int_ptr) model_elem, ret_found_index))) } \
                                                                              \
int NAME ## _array_foreach(const NAME ## _array *arr,                         \
                           NAME ## _array_foreach_func foreach_func,          \
                           void *foreach_data)                                \
{                                                                             \
    NAME ## _array_foreach_shim_args fe_args;                                 \
    fe_args.afsa_arr = arr;                                                   \
    fe_args.afsa_func = foreach_func;                                         \
    fe_args.afsa_data = foreach_data;                                         \
                                                                              \
    IF_NON_NULL(arr, return(array_foreach(arr-> NAME ## _arr,                 \
                            NAME ## _array_foreach_shim, &fe_args)))          \
}                                                                             \
                                                                              \
int NAME ## _array_foreach_delete(NAME ## _array *arr,                        \
                                  NAME ## _array_foreach_func foreach_func,   \
                                  void *foreach_data)                         \
{                                                                             \
    NAME ## _array_foreach_shim_args fe_args;                                 \
    fe_args.afsa_arr = arr;                                                   \
    fe_args.afsa_func = foreach_func;                                         \
    fe_args.afsa_data = foreach_data;                                         \
                                                                              \
    IF_NON_NULL(arr, return(array_foreach_delete(                             \
                            arr-> NAME ## _arr,                               \
                            NAME ## _array_foreach_shim, &fe_args)))          \
}                                                                             \
                                                                              \
int NAME ## _array_foreach_shim(const array *arr, uint32 idx,                 \
                                void *elem, void *data)                       \
{                                                                             \
    int err = 0;                                                              \
    NAME ## _array_foreach_shim_args *fe_args;                                \
                                                                              \
    bail_null(data);                                                          \
    fe_args = (NAME ## _array_foreach_shim_args *) data;                      \
    /* OK to cast elem to a (TYPE) based on elem size check in                \
     * _array_new functions (which checks if sizeof(int_ptr) is  r            \
     * greater than or equal to sizeof(TYPE).                                 \
     */                                                                       \
    bail_require(fe_args->afsa_arr-> NAME ## _arr == arr);                    \
    err = (fe_args->afsa_func)(fe_args->afsa_arr, idx,                        \
                               (TYPE) (int_ptr) elem, fe_args->afsa_data);    \
bail:                                                                         \
    return(err);                                                              \
}

#define TYPED_ARRAY_DIRECT_IMPL_NEW_DEFAULTS(NAME, TYPE, EMPTY_VALUE)         \
        TYPED_ARRAY_DIRECT_NEW_DEFAULTS_C(NAME, TYPE)                         \
        TYPED_ARRAY_DIRECT_NEW_FULL_C(NAME, TYPE)                             \
        TYPED_ARRAY_DIRECT_IMPL(NAME, TYPE, EMPTY_VALUE)

#define TYPED_ARRAY_DIRECT_IMPL_NEW_FULL(NAME, TYPE, EMPTY_VALUE)             \
        TYPED_ARRAY_DIRECT_NEW_OPTS_C(NAME, TYPE)                             \
        TYPED_ARRAY_DIRECT_IMPL(NAME, TYPE, EMPTY_VALUE)

#define TYPED_ARRAY_DIRECT_IMPL_NEW_NONE(NAME, TYPE, EMPTY_VALUE)             \
        TYPED_ARRAY_DIRECT_NEW_FULL_C(NAME, TYPE)                             \
        TYPED_ARRAY_DIRECT_IMPL(NAME, TYPE, EMPTY_VALUE)

#ifdef __cplusplus
}
#endif

#endif /* __TYPED_ARRAY_DIRECT_TMPL_H_ */
