/*
 *
 * array.h
 *
 *
 *
 */

#ifndef __ARRAY_H_
#define __ARRAY_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/** \defgroup lc_ds LibCommon: data structures */

/* ------------------------------------------------------------------------- */
/** \file array.h Generic array data type to store 'void *' elements.
 * \ingroup lc_ds
 *
 * Index numbers of array elements start
 * counting from zero.  Does not support "sparse" arrays; one element
 * is stored for every slot from zero up to the highest used index.
 *
 * All functions return an int error code: zero for success, and
 * nonzero for any other failure, unless otherwise noted.
 */

#include "common.h"

typedef struct array array;

/* ------------------------------------------------------------------------- */
/** @name Callback function types.
 *
 * The following four function type declarations define functions that
 * may be passed to array_new() when an array is created.  They allow
 * the caller to override the default behavior for how array elements
 * are handled in certain contexts.
 */
/*@{*/

/* ------------------------------------------------------------------------- */
/** Function to compare two array elements.  This is used for 
 * sorting the array, as well as for binary and linear searches.
 *
 * \param elem1 Pointer to the first element to be compared.
 * It should be cast as a (void **) and then dereferenced to 
 * produce the element to be compared.  This is done, rather
 * than passing the element directly, for compatibility with
 * qsort() and bsearch().
 *
 * \param elem1 Pointer to the second element to be compared.
 *
 * \return A negative number if *elem1 is less than *elem2; zero if
 * *elem1 is equal to *elem2; a positive number if *elem1 is greater
 * than *elem2.
 *
 * The default comparison function simply compares *elem1 and *elem2
 * themselves as if they were integers.
 */
typedef int (*array_elem_compare_func)(const void *elem1, const void *elem2);

/* ------------------------------------------------------------------------- */
/** Function to duplicate an array element.  This is called by
 * array_duplicate() on all of the non-NULL elements in the source
 * array.  It is also called by array_set(), array_append(), and
 * array_insert() to duplicate the element passed in, and the duplicate
 * is the one added to the array.  It is legal to pass NULL for the 
 * duplicate function, but then any calls to these APIs mentioned
 * above will fail.
 *
 * \param src The element to be duplicated
 *
 * \retval ret_dest Newly-created copy of the element
 *
 * There is no default duplicate function.
 */
typedef int (*array_elem_dup_func)(const void *src, void **ret_dest);

/* ------------------------------------------------------------------------- */
/** Function to be called when an element is set in the array with a
 * "takeover" variant.  This is called by array_set_takeover(),
 * array_append_takeover(), array_insert_takeover(), and
 * array_insert_sorted_takeover() when adding elements to the array.
 * This function intends to take over ownership of the passed in memory
 * (referred to by 'src').  It is not called if a NULL is being set in
 * the array.
 *
 * \param src The element passed to the library to be added to the array.
 * The array now owns this element. All array routines that call
 * this member function will null out the caller's passed in reference
 * to indicate the transfer of ownership
 *
 * \retval ret_dest The pointer to be actually added to the array.
 * In most cases since you are to passing ownership of the element
 * to the array as it is added this should just return the pointer
 * given. Any user supplied takeover function must adhere to this
 * intended use.
 *
 * The default takeover function just returns the pointer given and
 * NULLs out the callers source reference.
 */
typedef int (*array_elem_takeover_func)(void **inout_src, void **ret_dest);

/* ------------------------------------------------------------------------- */
/** Function to free an array element.  This is called by array_free(),
 * array_clear(), and array_delete().  It is also potentially called
 * by array_set() if there was already an element at the index where the
 * new one is to be written.  It is not called on NULL elements.
 *
 * Note that generally there should be a free function if and only if
 * there is a duplicate function.
 *
 * \param elem The element to be freed;
 *
 * Does not return a value, for function type compatibility with
 * free(), which will be a commonly used free function.
 *
 * The default free function does not do anything; it assumes that the
 * array does not own the memory associated with the element.
 */
typedef void (*array_elem_free_func)(void *elem);

/*@}*/

/* ------------------------------------------------------------------------- */
/** Specify behavior when the caller tries to insert a node that
 * matches an existing node, according to the comparison function.
 *
 * NOTE: this option is only heeded when the element is added using
 * array_append(), array_insert_sorted(), or array_insert_sorted_hint(),
 * or when arrays are merged using array_concat().  It does not have
 * any impact if array_set() or array_insert() are used (i.e. behavior
 * is equivalent to adp_add_new in these cases).
 *
 * The default behavior, if not overridden in the options structure,
 * is adp_add_new.
 */
typedef enum {
    /**
     * No policy has been specified.
     */
    adp_none = 0,

    /**
     * The nodes should be allowed to coexist peacefully; no checking
     * for duplicates needs to be performed.
     */
    adp_add_new = 1,

    /**
     * The node already in the array should be deleted first, then
     * the new one added.
     */
    adp_delete_old = 2,

    /**
     * The new element being added should be deleted, and the old one
     * left where it is.  This can be useful in cases where elements
     * are being appended, rather than inserted in sorted order, and
     * their ordering is important.
     */
    adp_delete_new = 3,

    /**
     * The requested operation should not be performed, the array
     * should remain unmodified, and the error code lc_err_exists
     * should be returned.
     */
    adp_fail_new = 4,
    adp_last
} array_dup_policy;

/* ------------------------------------------------------------------------- */
/** Options for a newly-created array.  These are passed in a
 * structure, rather than directly as parameters to array_new(), to
 * allow new options to be added without a change to the API.
 */
typedef struct array_options {
    /** Initial array size */
    uint32 ao_initial_size;

    /** Function to compare two elements */
    array_elem_compare_func  ao_elem_compare_func;

    /** Function to duplicate an element */
    array_elem_dup_func      ao_elem_dup_func;

    /** Function to prepare an element to be added to the array */
    array_elem_takeover_func      ao_elem_takeover_func;

    /** Function to deallocate an element */
    array_elem_free_func     ao_elem_free_func;

    /** Policy on handling insertion of duplicate elements */
    array_dup_policy         ao_dup_policy;

    /** 
     * Should the array library enforce constraints on sorted arrays?
     *
     * An array keeps track of whether or not its elements are sorted,
     * as far as it can tell.  An array begins life sorted.  The sorting
     * bit is cleared by array_set(), array_append(), array_insert(),
     * and array_push().  The sorting bit is set by array_clear() and
     * array_sort().
     *
     * This boolean controls whether we use this bit to try to keep the
     * caller from doing incorrect things.  If it is true an error will
     * be returned by array_insert_sorted() or array_binary_search() if
     * the sorting bit is not currently set.  Otherwise, this is not
     * enforced.  A caller might not want this enforcement if he is
     * manually making sure to add elements in sorted order.
     *
     * The default setting is true.
     */
    tbool                     ao_enforce_sorting_constraints;
} array_options;

/* ------------------------------------------------------------------------- */
/** In some cases the pointer that references an array element
 * is sufficient to hold the element data (i.e. some size of ints).
 * These cases are not takeovers and there is no default dup
 * function. The array creator in these cases can set the dup
 * function to 'array_std_assign_func' to simply copy in an
 * element that will fit in a sizeof(void *) space. This function
 * is provided to save callers from replicating this small routine
 * in many places. This routine is *not* the default for any
 * of the elem function pointers.
 *
 * \param src The source of the element
 *
 * \param ret_dest where to copy to
 */
int array_std_assign_func(const void *src, void **ret_dest);

/* ------------------------------------------------------------------------- */
/** Populate an options structure with the default values.  If you do
 * not want only the default values, call this first and then
 * overwrite whichever values you want to change.
 *
 * \retval ret_options The structure provided is filled in with
 * default options.
 */
int array_options_get_defaults(array_options *ret_options);

/* ------------------------------------------------------------------------- */
/** Create a new array.  The array initially has no elements and zero
 * length.
 *
 * \param options Array behavior options.  If all of the default
 * options are acceptable, NULL may be passed.  Otherwise, a
 * fully-populated options structure must be passed.  DO NOT fill in
 * an empty options structure if you want to override all of the
 * defaults.  If more options are added, these will then be passed in
 * unset, potentially causing mysterious failures and defeating the
 * purpose of the option structure.  ALWAYS call
 * array_options_get_defaults() and overwrite whatever you want to,
 * unless you want only the defaults.
 * \n\n
 * The compare and set functions are required; setting them to NULL
 * will result in the array_new() call failing.  The duplicate
 * function may be NULL; in this case, a call to array_duplicate()
 * will fail but everything else will function normally.  The free
 * function is may be NULL; in this case, no action is taken to free
 * an element when it is deleted from the array.
 *
 * \retval ret_arr The newly-created array.
 */
int array_new(array **ret_arr, const array_options *options);

/* ------------------------------------------------------------------------- */
/** Retrieve the options currently set on an array.  Most useful when you
 * want to change only one of them using array_options_set().  The caller 
 * provides a structure which this function fills in.
 *
 * \param arr An array.
 *
 * \retval ret_options The structure provided is filled in with the
 * options of the specified array.
 */ 
int array_options_get(array *arr, array_options *ret_options);

/* ------------------------------------------------------------------------- */
/** Change the options on an already-created array.  In general the
 * only valid reason to use this is to change the compare function, if
 * the caller wants to re-sort the array in a different order;
 * changing the other options risks memory leaks or other such errors.
 * Calling this function clears the array's sorted bit.
 *
 * \param arr An array.
 *
 * \param new_options The new options to set on the array.  Passing NULL
 * causes the defaults to be used for all options.
 *
 * \retval ret_old_options If provided, this structure is populated
 * with the options that were set on the array before this call.
 * May be NULL if you don't care.
 */
int array_options_set(array *arr, const array_options *new_options,
                      array_options *ret_old_options);


/**
 * There are a number of memory allocation forms for arrays.  They are:
 *
 * 1) Normal, what array_new() gives you.  The 'array' the 'elems' and
 *    are each separate, and the memory is owned by the array.
 *
 * 2) Embedded, what array_new_embedded() gives you.  The 'elems' element
 *    array is embedded in the 'array' structure allocation, immediately
 *    after the array, so that there is a single memory allocation for
 *    both.  This does mean that the maximum size of the array cannot be
 *    grown, although values can be added and removed (and have their
 *    values chaned) as long as they fit.
 *
 *    Arrays with embedded 'elems' never automatically shrink or grow in
 *    size, and cannot be array_compact()'d.  Attemps to insert/append
 *    with no room left will return lc_err_no_buffer_space .
 *
 * 3) Prealloc, what array_new_prealloc() gives you.  The memory for the
 *    array itself (and optional for the 'elems' if embedded elems is
 *    selected) is provided by the caller.  This may be useful if the
 *    array is part of some larger structure, as happens with our typed
 *    arrays.
 *
 * When duplicating any of these arrays, by default a "normal" array is
 * returned.  array_duplicate_embedded() and array_duplicate_prealloc()
 * may be used to duplicate in other ways.
 *
 * On array_free(), if the 'elems' is not owned by the array it is
 * simply NULL'd out.  Likewise in the prealloc case, the array pointer
 * itself is simply NULL'd out.
 */

/**
 * Like array_new(), except that the memory of the array's element array
 * is embedded in the new array.  This means that there is one
 * allocation, instead of two, and that the number of elements in the
 * array cannot grow.  This can save a bit of memory (and potentially
 * time), which may be important if there are many such arrays.
 */
int array_new_embedded(array **ret_arr,
                       const array_options *options);

/**
 * Like array_new(), except that the memory needed for the array
 * structure is provided by the caller.  This may be useful if the array
 * is part of some larger structure which has already been allocated, as
 * happens with typed arrays.  Optionally, (if embedded_elems is true)
 * the provided memory is also used for the array's element array; see
 * array_new_embedded() for notes on the implications of this.  Note
 * that in no case is memory for the array values allocated or affected
 * by this.  However a caller with carefully considered compare, dup,
 * takeover and free functions could set array element values to be out
 * of the same block of memory, and thereby get the array, the elements
 * array, and all the element values in a single block of memory.
 *
 * Callers may use array_get_prealloc_elems_size_quick() to figure out
 * how big a buffer is needed.
 *
 */
int array_new_prealloc(array **ret_arr,
                       const array_options *options,
                       tbool embedded_elems,
                       uint32 *ret_bytes_used,
                       void *buf,
                       uint32 buf_avail);

/* ------------------------------------------------------------------------- */
/** Tell us if the array uses embedded elems.
 */
int array_is_embedded(const array *arr, tbool *ret_emb);

/* ------------------------------------------------------------------------- */
/** Tell us if the array uses preallocated memory.
 */
int array_is_prealloc(const array *arr, tbool *ret_pre);

/**
 * For use with callers of array_new_prealloc() , in order to size the
 * chunk of memory that will be required.  Callers can pass -1 if there
 * are no embedded elements.
 */
uint32 array_get_prealloc_elems_size_quick(int32 embedded_elems);

/**
 * The same as array_get_prealloc_elems_size_quick(-1) .
 */
uint32 array_get_prealloc_size_quick(void);

/**
 * Like array_get_prealloc_elems_size_quick(), but sized based on the
 * given array option initial size.
 */
uint32 array_get_prealloc_opts_size_quick(tbool embedded_elems,
                                          const array_options *options);

/**
 * Like array_get_prealloc_elems_size_quick(), but sized based on the
 * given array's current allocated size.
 */
uint32 array_get_prealloc_alloced_size_quick(tbool embedded_elems,
                                             const array *arr);


/* ------------------------------------------------------------------------- */
/** Free an array.  Free all the array elements, free all memory allocated
 * by the array library, and then set *arr to NULL.
 *
 * \param arr Array to be freed.
 */
int array_free(array **arr);

/* ------------------------------------------------------------------------- */
/** Free all array elements, then remove all elements from the array.
 *
 * \param arr Array to be emptied.
 */
int array_empty(array *arr);

/* ------------------------------------------------------------------------- */
/** Compact the array data structure so as to only take up as much
 * memory as is necessary to hold the elements within it.  Note that
 * this does NOT compact the elements in the array (e.g. by removing
 * NULLs); the post-compacted array will appear in every way identical
 * to the pre-compacted array, but may take up less memory.  This is
 * because the array is grown in progressively larger chunks as
 * elements are added, so at any given time its allocated size is
 * probably bigger than necessary to hold all of the elements.
 *
 * \param arr Array to be compacted.
 */
int array_compact(array *arr);

/* ------------------------------------------------------------------------- */
/** Compact the array data structure by shifting down elements as
 * necessary to cover any NULL slots.  This does NOT change the
 * allocated size of the array as array_compact() does.
 * e.g. {p1, p2, NULL, p3, NULL, NULL, p4} -->
 *      {p1, p2, p3, p4}
 *
 * \param arr Array to be compacted.
 */
int array_compact_empty_slots(array *arr);

/* ------------------------------------------------------------------------- */
/** Option flags for array_delete_duplicates().
 */
typedef enum {

    addf_none = 0,

    /**
     * Use an alternate algorithm for duplicate removal, which could
     * run faster in cases of few duplicates.  We make a copy of the
     * array, quicksort it, and then use the sorted copy to find
     * duplicates.  (This is instead of sorting the original, whose
     * order we want to preserve.)  When a duplicate element is found,
     * all but one instance of it are removed from the original array.
     * So this has higher fixed costs (the duplication, and sort), but
     * in the best case (few duplicates) ends up faster.  Basically,
     * it is O(nlogn) on the number of elements (for the sort), plus
     * O(n*d) where d is the number of duplicates.
     */
    addf_use_sorted_copy = 1 << 0,

} array_delete_duplicates_flags;

/** Bit field of ::array_delete_duplicates_flags ORed together */
typedef uint32 array_delete_duplicates_flags_bf;

/* ------------------------------------------------------------------------- */
/** Delete duplicate entries from the array.  The first instance of
 * any given value will be kept, and any later matches will be
 * deleted.  Later elements will be shifted down to cover holes left
 * by the deletions.  The registered compare function is used to
 * determine matches.  NULL elements are ignored (and are not
 * considered duplicates of anything, including each other).
 *
 * Performance note: by default, this function takes the brute force
 * approach, and is therefore O(n^2) in the best and worst cases.  
 * It assumes the array is not sorted, and so does a linear search to
 * find duplicates of each element, and compacts elements one at a
 * time.
 *
 * XXX/EMT: we could do better if the array is already sorted.
 *
 * See the ::addf_use_sorted_copy option flag for an alternate
 * algorithm which can speed up the process in the case of few or no
 * duplicates.
 *
 * \param arr Array whose duplicates are to be removed.
 * \param flags Option flags.  Pass zero to accept defaults.
 */
int array_delete_duplicates(array *arr, 
                            array_delete_duplicates_flags_bf flags);

/* ------------------------------------------------------------------------- */
/** Ensure that the array has at least the specified number of slots
 * allocated to hold elements.  If the array already has this many or
 * more, do nothing.  If it has fewer, increase it to exactly this
 * size.
 */
int array_ensure_size_min_exact(array *arr, uint32 num_elems);


/* ------------------------------------------------------------------------- */
/** Make a duplicate copy of the specified array.  The src array is not
 * modified.  The duplicate function is used to duplicate each
 * non-NULL element in the array.  A pointer to the new copy of the
 * array is returned in dest.
 *
 * \param src Array to be duplicated
 *
 * \retval ret_dest A newly-created duplicate copy of the array provided.
 */
int array_duplicate(const array *src, array **ret_dest);

/**
 * Like array_duplicate(), except that the memory of the array's element
 * array is embedded in the new array.  This means that there is one
 * allocation, instead of two, and that the number of elements in the
 * array cannot grow.  This can save a bit of memory (and potentially
 * time), which may be important if there are many such arrays.
 */

int array_duplicate_embedded(const array *src, array **ret_dest);

/**
 * Like array_duplicate(), except that the memory needed for the array
 * structure, and optionally the array's element array, are provided by
 * the caller.  See array_new_prealloc() for more information.
 */
int array_duplicate_prealloc(const array *src, array **ret_dest,
                             tbool embedded_elems,
                             uint32 *ret_bytes_used,
                             void *buf,
                             uint32 buf_avail);

/* ------------------------------------------------------------------------- */
/** Concatenate one array onto the end of the other.  This involves
 * removing all of the elements from one array and appending them to
 * the other array in the order in which they were found.
 *
 * \param base The array onto which to concatenate the extra array.
 * 
 * \param inout_extra The array to concatenate onto the end of the
 * base array.  After all of the elements are removed from this array,
 * the array itself, now empty, is deleted, and the pointer set to NULL.
 *
 * Note that no elements are copied or freed during this process, so
 * none of the provided callbacks are called.
 */
int array_concat(array *base, array **inout_extra);

/* ------------------------------------------------------------------------- */
/** Concatenate one array onto the end of the other, without
 * disturbing the former.  This involves copying all of the elements
 * from one array and appending them to the other array in the order
 * in which they were found.  This is just like array_concat(), except
 * that the elements from the array being appended are copied rather
 * than moved.
 *
 * NOTE: the duplicate function in the base array is the one used
 * to duplicate the nodes, not the one in the extra array.
 *
 * \param base The array onto which to concatenate the extra array.
 * 
 * \param extra The array to concatenate onto the end of the
 * base array.  This array is not modified.
 */
int array_append_array(array *base, const array *extra);

/* ------------------------------------------------------------------------- */
/** Return the number of elements in the array.  Note that this is
 * determined by the index of the last valid element, and does not
 * necessarily reflect the number of non-NULL elements, or elements
 * explicitly added by the caller.  See array_set() for details.
 *
 * \param arr An array.
 *
 * \retval ret_length The number of elements in the specified array.
 */
int array_length(const array *arr, uint32 *ret_length);

/* ------------------------------------------------------------------------- */
/** Return the number of elements in the array. Same as array_length,
 * but returns the length directly, for greater code compactness.
 *
 * \param arr An array.
 *
 * \return The number of elements in the specified array, or 0 if it
 * was NULL.
 */
uint32 array_length_quick(const array *arr);

/* ------------------------------------------------------------------------- */
/** Determine whether the contents of two arrays are exactly
 * identical.  This requires that they have the same number of
 * elements, the same compare function, and that each element at a
 * given position is identical to the element in the other array at
 * the same position, according to their common compare function.
 * Other aspects, such as the number of slots allocated and the other
 * options besides the compare function, are not compared.
 */
int array_equal(const array *arr1, const array *arr2, tbool *ret_equal);

/* ------------------------------------------------------------------------- */
/** Like array_equal(), except skips over NULL members of the array
 * during the comparison.  e.g. so the following two would match:
 *     {NULL, a,    b,    NULL, c}
 *     {a,    NULL, NULL, NULL, b,   c,   NULL}
 */
int array_equal_sparse(const array *arr1, const array *arr2, tbool *ret_equal);

/* ------------------------------------------------------------------------- */
/** Retrieve the value of a specified array element.  If the index is
 * out of range, lc_err_not_found is returned.
 *
 * \param arr An array.
 *
 * \param idx Index of the element to retrieve.
 *
 * \retval ret_elem Value of the specified element.
 */
int array_get(const array *arr, uint32 idx, void **ret_elem);

/* ------------------------------------------------------------------------- */
/** Retrieve the value of a specified array element.  Same as
 * array_get(), but returns the value of the element directly, for
 * greater code compactness.
 *
 * \param arr An array.
 *
 * \param idx Index of the element to retrieve.
 *
 * \return Value of the specified element, or NULL if the index was
 * out of range.
 */
void *array_get_quick(const array *arr, uint32 idx);

/* ------------------------------------------------------------------------- */
/** Place the specified element in the array.  A dup function is required
 * as the passed in element must be copied into new memory owned by
 * the array. This new element is added to the array instead of the element
 * passed to this function.
 *
 * If the index is within range of the array, the element already
 * there is freed (if it was non-NULL) and then overwritten.  If the
 * index is out of range, the array is automatically extended to
 * accomodate the new element.  All elements between the previous end
 * of the array and the element being set, if any, are set to NULL,
 * and are thereafter indistinguishable from elements that were set
 * directly.  e.g. if a fresh, empty array has an element set at index
 * 10, a subsequent call to array_length() would return 11 as the
 * length.
 *
 * \param arr An array.
 *
 * \param idx The index at which the element should be placed in the
 * array.
 *
 * \param elem The element to add to the array.
 */
int array_set(array *arr, uint32 idx, const void *elem);

/* ------------------------------------------------------------------------- */
/** Analog of array_set, except a takeover function is required and the
 * array assumes ownership of the element passed in.
 *
 * \param arr An array.
 *
 * \param idx The index at which the element should be placed in the
 * array.
 *
 * \param inout_elem The element to add to the array and assume ownership.
 */
int array_set_takeover(array *arr, uint32 idx, void **inout_elem);

/* ------------------------------------------------------------------------- */
/** Append the specified element to the end of the array.  As with
 * array_set(), a dup function is required to add a copy of the element.
 *
 * \param arr An array.
 *
 * \param elem The element to append to the array.
 */
int array_append(array *arr, const void *elem);

/* ------------------------------------------------------------------------- */
/** Append the specified element to the end of the array.  As with
 * array_set_takeover(), a takeover function is required to assume ownership
 * of the passed in element.
 *
 * \param arr An array.
 *
 * \param inout_elem The element to append to the array and assume ownership.
 */
int array_append_takeover(array *arr, void **inout_elem);

/* ------------------------------------------------------------------------- */
/** Insert the specified element into the array.  All elements at or
 * beyond the specified index are shifted down by one slot to make
 * room.  If the specified index is beyond the current range of
 * indices, this behaves identically to array_set().  As with
 * array_set(), the dup function is required to add a copy of the element.
 *
 * \param arr An array.
 *
 * \param idx The index at which the element should be inserted in the
 * array.
 *
 * \param elem The element to add to the array.
 */
int array_insert(array *arr, uint32 idx, const void *elem);

/* ------------------------------------------------------------------------- */
/** Insert the specified element into the array.  All elements at or
 * beyond the specified index are shifted down by one slot to make
 * room.  If the specified index is beyond the current range of
 * indices, this behaves identically to array_set().  As with
 * array_set_takeover(), a takeover function is required to assume
 * ownership of the passed in element.
 *
 * \param arr An array.
 *
 * \param idx The index at which the element should be inserted in the
 * array.
 *
 * \param inout_elem The element to insert to the array and assume ownership.
 */
int array_insert_takeover(array *arr, uint32 idx, void **inout_elem);

/* ------------------------------------------------------------------------- */
/** Insert the specified element into the array in sorted order.
 * Requires that the array currently be sorted, as a binary search is
 * used to find the insertion point.  If the array is not currently
 * sorted, and enforcement of sorting constraints was requested, an
 * error is returned.  As with array_set(), the dup function is
 * required to add a copy of the element.
 *
 * If there are multiple elements that match according to the
 * comparison function, and the duplicate policy is adp_add_new, the
 * element is inserted after all of the others that it matches.
 *
 * \param arr An array.
 *
 * \param elem The element to add to the array.
 */
int array_insert_sorted(array *arr, const void *elem);

/* ------------------------------------------------------------------------- */
/** Variant of array_insert_sorted() which returns the array
 * index at which the element was inserted.
 */
int array_insert_sorted_idx(array *arr, const void *elem,
                            uint32 *ret_insert_idx);

/* ------------------------------------------------------------------------- */
/** Insert the specified element into the array in sorted order.
 * Requires that the array currently be sorted, as a binary search is
 * used to find the insertion point.  If the array is not currently
 * sorted, and enforcement of sorting constraints was requested, an
 * error is returned.  As with array_set(), a takeover function is
 * required to assume ownership of the passed in element.
 *
 * If there are multiple elements that match according to the
 * comparison function, and the duplicate policy is adp_add_new, the
 * element is inserted after all of the others that it matches.
 *
 * \param arr An array.
 *
 * \param inout_elem The element to add to the array and assume ownership.
 */
int array_insert_sorted_takeover(array *arr, void **inout_elem);

/* ------------------------------------------------------------------------- */
/** Insert the specified element into the array in sorted order.
 * Instead of beginning the binary search at index n/2, start at
 * the specified index.  If the specified index is correct, the
 * search completes in O(1) time (though the insertion itself still
 * takes O(n)).  If the specified index is not correct, the binary
 * search proceeds from there, having eliminated indices 0 through
 * index, or index through n, depending on the circumstances.
 *
 * \param arr An array.
 *
 * \param elem The element to add to the array.
 *
 * \param hint_index The index at which the caller thinks the element
 * is most likely to go.
 */
int array_insert_sorted_hint(array *arr, const void *elem, uint32 hint_index);

/* ------------------------------------------------------------------------- */
/** Variant of array_insert_sorted_hint() which returns the array
 * index at which the element was inserted.
 */
int array_insert_sorted_hint_idx(array *arr, const void *elem,
                                 uint32 hint_index, uint32 *ret_insert_idx);

/* ------------------------------------------------------------------------- */
/** Like array_insert_sorted_hint(), except ownership of the passed in
 * element is assumed by the array (as with all takeover variants).
 *
 * \param arr An array.
 *
 * \param inout_elem The element to add to the array and assume ownership.
 *
 * \param hint_index The index at which the caller thinks the element
 * is most likely to go.
 */
int array_insert_sorted_hint_takeover(array *arr, void **inout_elem,
                                      uint32 hint_index);

/* ------------------------------------------------------------------------- */
/** Find the index at which the specified element would have been
 * inserted by array_insert_sorted().  Instead of inserting it, just
 * return the index.
 *
 * If there are multiple elements that match according to the
 * comparison function, the insertion point returned is the first
 * non-matching element after the matching ones.  Note that this is
 * not affected by the duplicate policy.
 *
 * \param arr An array.
 *
 * \param elem The element whose insertion point to find.
 *
 * \retval ret_index The index at which the element would need to be
 * inserted to keep the array sorted.
 */
int array_find_insertion_point(const array *arr, const void *elem,
                               uint32 *ret_index);

/* ------------------------------------------------------------------------- */
/** Find the index at which the specified element would have been
 * inserted by array_insert_sorted_hint().  Instead of inserting it,
 * just return the index.
 *
 * \param arr An array.
 *
 * \param elem The element whose insertion point to find.
 *
 * \param hint_index The index at which the caller thinks the element
 * is most likely to go.
 *
 * \retval ret_index The index at which the element would need to be
 * inserted to keep the array sorted.
 */
int array_find_insertion_point_hint(const array *arr, const void *elem,
                                    uint32 hint_index, uint32 *ret_index);

/* ------------------------------------------------------------------------- */
/** Free the specified element then shift all elements after this one
 * up by one slot, overwriting the specified element.  If the index is
 * out of range, an error is returned.
 *
 * \param arr An array.
 *
 * \param idx The index of the element to be deleted.
 */
int array_delete(array *arr, uint32 idx);

/* ------------------------------------------------------------------------- */
/** Free the specified element and set it to NULL.  Just like
 * array_delete() except that it does not shift all of the other
 * elements down to fill the vacated spot.  It is equivalent to
 * array_set(arr, idx, NULL).
 *
 * \param arr An array.
 *
 * \param idx The index of the element to be deleted.
 */
int array_delete_no_shift(array *arr, uint32 idx);

/* ------------------------------------------------------------------------- */
/** Remove the specified element from the array, shifting elements
 * after this one up by one slot to fill the hole, and pass ownership
 * of it back to the caller.  The differences between this and
 * deleting the element are (a) the free function is not called, and
 * (b) a pointer to the element is returned to the caller.
 *
 * \param arr An array.
 *
 * \param idx The index of the element to be removed.
 *
 * \param ret_elem The element removed from the array.
 */
int array_remove(array *arr, uint32 idx, void **ret_elem);

/* ------------------------------------------------------------------------- */
/** Remove the specified element from the array and pass ownership of
 * it back to the caller.  Just like array_remove() except that the
 * elements after this one are not shifted down.  The element removed
 * is set to NULL in the array.
 *
 * \param arr An array.
 *
 * \param idx The index of the element to be removed.
 *
 * \param ret_elem The element removed from the array.
 */
int array_remove_no_shift(array *arr, uint32 idx, void **ret_elem);

/* ------------------------------------------------------------------------- */
/** A variant on array_delete() which specifies which element(s) to
 * delete by their value rather than by their index.  If no matching
 * elements are found, lc_err_not_found is returned.
 *
 * NOTE: may leave the pointer passed in for elem dangling, if the
 * element that matches is in fact the same pointer.
 *
 * \param arr An array.
 *
 * \param elem A model of the element(s) to be deleted.  Elements
 * matching this element according to the comparison function (which
 * could include the model itself, if it is in the array) are eligible
 * for deletion.
 * 
 * \param allow_multiple If true, all matching elements will be
 * deleted.  If false, if there is more than one matching element no
 * elements are deleted, and lc_err_multiple is returned.
 */
int array_delete_by_value(array *arr, void *elem, tbool allow_multiple);

/* ------------------------------------------------------------------------- */
/** A variant on array_remove() which specifies which element to
 * remove by its value rather than by its index.  If no matching
 * elements are found, lc_err_not_found is returned.  If there is more
 * than one matching element no elements are removed, and
 * lc_err_multiple is returned.
 *
 * \param arr An array.
 *
 * \param elem A model of the element to be removed.  Elements
 * matching this element according to the comparison function (which
 * could include the model itself, if it is in the array) are eligible
 * for removal.
 *
 * \retval ret_elem The element removed from the array.  Note that
 * this may match elem if the model was in fact the actual element in
 * the array.
 */ 
int array_remove_by_value(array *arr, void *elem, void **ret_elem);

/* ------------------------------------------------------------------------- */
/** A variant on array_delete() which deletes multiple elements
 * simultaneously.  Besides being more convenient than writing your 
 * own loop, with 'no_shift == false' this is more efficient than n
 * calls to array_delete() because it shifts everything down at once.
 *
 * \param arr An array.
 *
 * \param idx The index of the first element to be deleted.
 *
 * \param num_elems The number of elements to be deleted, or -1 to delete
 * all remaining elements after 'idx'.
 *
 * \param shift_to_cover Pass 'false' to just delete elements; 'true' to
 * also shift down any elements past those deleted (if any) to cover the
 * vacated space.
 */
int array_delete_range(array *arr, uint32 idx, int32 num_elems,
                       tbool shift_to_cover);

/* ------------------------------------------------------------------------- */
/** @name Stack simulation wrappers.
 *
 * These definitions allow an array to be used as a stack.
 * These are just thin wrappers around other functionality, intended
 * mainly for code readability.
 *
 * array_push() is identical to array_append().
 * array_pop() is identical to array_remove() on the last element.
 * array_peek() is identical array_get() on the last element.
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** Push an element onto an array being used as a stack.  Identical to
 *  array_append().
 */
#define array_push array_append

/* ------------------------------------------------------------------------- */
/** Push an element onto an array being used as a stack and assume
 *  memory ownership.  Identical to array_append_takeover().
 */
#define array_push_takeover array_append_takeover

/* ------------------------------------------------------------------------- */
/** Pop an element off an array being used as a stack.  Identical to
 * array_remove() of the last element.
 *
 * \param arr An array.
 *
 * \retval ret_elem The element popped off the stack.
 */
int array_pop(array *arr, void **ret_elem);

/* ------------------------------------------------------------------------- */
/** Retrieve the top element from an array being used as a stack, but
 * do not remove the element.  Identical to array_get() of the last
 * element.
 *
 * \param arr An array.
 *
 * \retval ret_elem The element currently on the top of the stack.
 */
int array_peek(const array *arr, void **ret_elem);

/*@}*/

/* ------------------------------------------------------------------------- */
/** Sort the array using the compare function previously set up with
 * this array.
 *
 * \param arr The array to be sorted.
 */
int array_sort(array *arr);

/* ------------------------------------------------------------------------- */
/** Tell us if the array is currently sorted.
 */
int array_is_sorted(const array *arr, tbool *ret_is_sorted);

/* ------------------------------------------------------------------------- */
/** Compare the elements of the array, and set the array 'sorted' flag
 * accordingly.  This does not sort or modify the array elements.
 */
int array_update_sorted(array *arr);

/* ------------------------------------------------------------------------- */
/** Sort the array using the compare function specified in parameters
 * to this function.  The previously-registered compare function is
 * restored to the array after the sort is complete.  This does *not*
 * set the 'sorted' flag on the array, since the array probably won't
 * end up sorted according to the registered compare function, which is
 * what would be used for binary search, etc.
 *
 * \param arr The array to be sorted.
 *
 * \param compare_func The compare function with which to sort the array.
 */
int array_sort_custom(array *arr, array_elem_compare_func compare_func);

/* ------------------------------------------------------------------------- */
/** Find the first element in the array that matches the specified
 * element according to the comparison function, using a binary
 * search.  If the array is not sorted, and enforcement of sorting
 * constraints was requested, an error is returned.  If the element is
 * not found, lc_err_not_found is returned.
 *
 * Note that often the caller may not have an element he wants to
 * match, but rather only has the criteria which identify the element
 * he wants to match.  In this case, the caller has to make up a dummy
 * element which will appear to the comparison function identical to
 * the element to be matched.
 *
 * \param arr An array.
 *
 * \param model_elem The model element for which to find a match.
 *
 * \retval ret_found_index The index at which a matching element
 * occurs in the array.  If multiple elements match, the one with
 * the lowest index is returned.
 */
int array_binary_search(const array *arr, const void *model_elem, 
                        uint32 *ret_found_index);

/* ------------------------------------------------------------------------- */
/** Same as array_binary_search(), except that it starts looking at the
 * specified index, instead of at index n/2.
 */
int array_binary_search_hint(const array *arr, const void *model_elem,
                             uint32 hint_index, uint32 *ret_found_index);

/* ------------------------------------------------------------------------- */
/** Find the first element in the array that matches the specified
 * element according to the comparison function, using a linear
 * search.  Only elements at or beyond the start_index are considered.
 *
 * As with binary search, the caller may need to construct a dummy
 * element if all he has initially are a separate set of criteria.
 *
 * If the element is not found, lc_err_not_found is returned.
 *
 * \param arr An array.
 *
 * \param model_elem The model element for which to find a match.
 *
 * \param start_index The index of the element at which to start
 * searching.  Elements prior to this one in the array will not be
 * checked.
 *
 * \retval ret_found_index The index at which a matching element
 * occurs in the array.  If multiple elements match, the one with
 * the lowest index is returned.
 */
int array_linear_search(const array *arr, const void *model_elem,
                        uint32 start_index, uint32 *ret_found_index);

/* ------------------------------------------------------------------------- */
/** Find the first element in the array that matches the specified
 * element according to the comparison function, using a linear
 * search.  Only elements at or beyond the start_index are considered.
 *
 * As with binary search, the caller may need to construct a dummy
 * element if all he has initially are a separate set of criteria.
 *
 * If the element is not found, lc_err_not_found is returned.
 *
 * \param arr An array.
 *
 * \param model_elem The model element for which to find a match.
 *
 * \param custom_compare_func A one-time compare function, presumably
 * different than the array's default compare function.  This is useful if
 * you have an array sorted by one of a number of keys, but sometimes want
 * to do a lookup on another key.
 *
 * \param start_index The index of the element at which to start
 * searching.  Elements prior to this one in the array will not be
 * checked.
 *
 * \retval ret_found_index The index at which a matching element
 * occurs in the array.  If multiple elements match, the one with
 * the lowest index is returned.
 */
int array_linear_search_custom(const array *arr, const void *model_elem,
                               array_elem_compare_func custom_compare_func,
                               uint32 start_index, uint32 *ret_found_index);

/* ------------------------------------------------------------------------- */
/** Find the first element in the array matching the model element,
 * using the best search possible.  If the array is sorted, a binary
 * search will be used; otherwise, a linear search.  This is intended
 * for places where the caller may simply be passing on an array that
 * was given to it, and it doesn't know if the array is sorted or not.
 *
 * If the element is not found, lc_err_not_found is returned.
 *
 * \param arr An array.
 *
 * \param model_elem The model element for which to find a match.
 *
 * \retval ret_found_index The index at which a matching element
 * occurs in the array.  If multiple elements match, the one with
 * the lowest index is returned.
 */
int array_best_search(const array *arr, const void *model_elem,
                      uint32 *ret_found_index);

/* ------------------------------------------------------------------------- */
/** Function to be called on every element in an array.
 *
 * \param arr The array being iterated over.
 *
 * \param idx The index of the element being passed.
 *
 * \param elem One element in the array.
 *
 * \param data The data passed by the caller to array_foreach().
 *
 * \return May return any of the following:
 * \li lc_err_no_error
 * \li lc_err_foreach_delete (only valid if called from
 * array_foreach_delete(); if returned from array_foreach(), 
 * treated as lc_err_foreach_halt_err)
 * \li lc_err_foreach_halt_err
 * \li lc_err_foreach_halt_ok
 * \li anything else: a nonfatal error.  Iteration continues, but an error
 * is returned from array_foreach() when it is finished.
 *
 * NOTE: this function must not modify the array (which would involve
 * casting the array pointer to non-const).  Particularly, it is
 * important that no elements be added, deleted, or moved.
 */
typedef int (*array_foreach_func)(const array *arr, uint32 idx, void *elem,
                                  void *data);

/* ------------------------------------------------------------------------- */
/** Call the specified function once for every element in the array.
 * It is guaranteed that the calls to the function will be in the same
 * order as the elements appear in the array, with index 0 being called
 * first.
 *
 * \param arr The array to iterate over.
 *
 * \param foreach_func The function to call for each element.
 *
 * \param foreach_data A void * to be passed to the foreach_function.
 */
int array_foreach(const array *arr, array_foreach_func foreach_func,
                  void *foreach_data);

/* ------------------------------------------------------------------------- */
/** Call the specified function once for every element in the array.
 * Identical to array_foreach() with the exception that if a foreach
 * function returns array_foreach_delete_me as a return code, (a) this
 * will not be treated as an error for purposes of halting the
 * iteration or affecting the ultimate return code, and (b) the
 * current element will be deleted from the array.
 *
 * Note that the foreach function still may not directly add, delete,
 * or move elements in the array; returning array_foreach_delete_me
 * is the only safe way to delete elements during the iteration.
 */
int array_foreach_delete(array *arr, array_foreach_func foreach_func,
                         void *foreach_data);


#ifdef __cplusplus
}
#endif

#endif /* __ARRAY_H_ */
