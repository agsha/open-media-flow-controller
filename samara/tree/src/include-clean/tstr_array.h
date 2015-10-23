/*
 *
 * tstr_array.h
 *
 *
 *
 */

#ifndef __TSTR_ARRAY_H_
#define __TSTR_ARRAY_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file src/include/tstr_array.h Subtype of generic array which holds
 * ::tstring elements.
 * \ingroup lc_ds
 */

/**
 * \struct tstr_array 
 * Array of tstring elements.
 *
 * A thin wrapper around extensible array library,
 * with the elements being 'tstring *'s.  The main purpose is to save
 * the caller the burden and hazard of casting all of the elements back
 * and forth between tstring * and void *.
 *
 * Note: some of the functions below could have been \#defines with
 * typecasts, but then we wouldn't have gotten typechecking on the
 * tstring * or tstring ** arguments.
 */

#include <stdlib.h>
#include "typed_array_tmpl.h"

/* Pre-declare structure type to resolve circular dependency */
TYPED_ARRAY_PRE_DECL(tstr);
#include "tstring.h"

TYPED_ARRAY_HEADER_BODY_NEW_NONE(tstr, tstring *);

/* ----------------------------------------------------------------------------
 * Options to a tstr_array.
 */
typedef struct tstr_array_options {
    /*
     * What to do with duplicates?
     */
    array_dup_policy         tao_dup_policy;
    
} tstr_array_options;

/* ------------------------------------------------------------------------- */
int tstr_array_new(tstr_array **ret_arr, const tstr_array_options *tsa_opts);

/* ------------------------------------------------------------------------- */
/** Create a new tstr_array from a variable-length list of strings.
 * The last fixed parameter is the number of strings that follow; all
 * subsequent parameters must be of type char *.  Each string will be
 * made into a tstring, and put into the array, in the order they are
 * passed.
 */
int tstr_array_new_va_str(tstr_array **ret_arr,
                          const tstr_array_options *tsa_opts,
                          uint32 num_strings, ...);

int tstr_array_new_va(tstr_array **ret_arr,
                      const tstr_array_options *tsa_opts,
                      uint32 num_strings, va_list ap);

/* ------------------------------------------------------------------------- */
/** Create a new tstr_array from an array of char *s.  'num_strings' may be
 * -1 to signify that the array is NULL terminated, and to add the entire
 * array.  Each string will be made into a tstring, and put into the array,
 * in the order they are passed.
 */
int tstr_array_new_argv(tstr_array **ret_arr,
                        const tstr_array_options *tsa_opts,
                        int32 num_strings, const char **argv);

/* ------------------------------------------------------------------------- */
/** Create a new tstr_array from a list of characters as specified by a
 * string.  Each charcter in the string 'charlist' will be made into a
 * tstring, and put into the array, in the order they are passed.
 */
int tstr_array_new_charlist(tstr_array **ret_arr,
                            const tstr_array_options *tsa_opts,
                            const char *charlist);

/* ------------------------------------------------------------------------- */
/** Populate a provided tstr_array_options structure with the default
 * options for a tstr_array.  If you do not want to accept all of the
 * defaults, call this first and then change the fields you want to be
 * different.
 */ 
int tstr_array_options_get_defaults(tstr_array_options *ret_options);

/* ----------------------------------------------------------------------------
 * Alternative forms of the search functions in case you don't have your
 * target string handy as a tstring.
 */
int tstr_array_binary_search_str(const tstr_array *arr, const char *target,
                                 uint32 *ret_found_index);
int tstr_array_linear_search_str(const tstr_array *arr, const char *target,
                                 uint32 start_index, uint32 *ret_found_index);

/**
 * Search for a string in the array which has a prefix relationship
 * with the string provided.
 *
 * \param arr Array to search
 * \param target String to search for
 * \param ignore_case Should we do a case-insensitive comparison?
 * \param target_is_prefix If \c true we search for an element of which
 * 'target' is a prefix.  If \c false we search for an element which is
 * a prefix of 'target'.
 * \param start_index Index of element from which to start searching.
 * \param ret_found_index Index at which a matching element was found,
 * if any.
 * \return lc_err_not_found if no matches were found.  Otherwise, 
 * nonzero for other error, and zero if a match was successfully found.
 */
int tstr_array_linear_search_prefix_str(const tstr_array *arr,
                                        const char *target,
                                        tbool ignore_case,
                                        tbool target_is_prefix,
                                        uint32 start_index,
                                        uint32 *ret_found_index);

/* ----------------------------------------------------------------------------
 * Combine tstr_array operations with conversions between strings and
 * tstrings.
 */ 
int tstr_array_get_str(const tstr_array *arr, uint32 idx, 
                       const char **ret_str);
const char *tstr_array_get_str_quick(const tstr_array *arr, uint32 idx);
int tstr_array_set_str(tstr_array *arr, uint32 idx, const char *str);
int tstr_array_set_str_takeover(tstr_array *arr, uint32 idx, char **inout_str);
int tstr_array_append_str(tstr_array *arr, const char *str);
int tstr_array_append_str_takeover(tstr_array *arr, char **inout_str);
int tstr_array_insert_str(tstr_array *arr, uint32 idx, const char *str);
int tstr_array_insert_str_takeover(tstr_array *arr, uint32 idx,
                                   char **inout_str);
int tstr_array_insert_str_sorted(tstr_array *arr, const char *str);
int tstr_array_insert_str_sorted_takeover(tstr_array *arr, char **inout_str);

int tstr_array_append_sprintf(tstr_array *arr, const char *format, ...)
     __attribute__ ((format (printf, 2, 3)));


/* ------------------------------------------------------------------------- */
/** Generate an array of char *s, terminated by a NULL, from the strings
 * in the array.  The resulting array will be a dynamically allocated
 * array of dynamically allocated strings, all independent of the
 * tstr_array.
 */
int tstr_array_to_argv(const tstr_array *arr, char **ret_argv[]);


/* ------------------------------------------------------------------------- */
/** Free an argv array generated by tstr_array_to_argv().
 */
int tstr_array_free_argv(char ***inout_argv);


/* ------------------------------------------------------------------------- */
/** Compares \c spec to \c arr treating certain elements in \c spec
 * specially.  An element in \c spec that is only a single asterisk
 * "*" matches any single string in the same position in \c arr.  An
 * element in \c spec that is two asterisks and is the last element in
 * the array matches one or more elements at the end of \c arr.
 *
 * \param spec The wildcarded specification of strings to match.
 *
 * \param arr The string literals to be compared against \c spec.
 *
 * \retval ret_match \c true if the arrays matched, \c false if not.
 */
int tstr_array_wildcard_match(const tstr_array *spec, const tstr_array *arr,
                              tbool *ret_match);

/**
 * Like tstr_array_wildcard_match(), except returns the result as a
 * direct return value.
 */
tbool tstr_array_wildcard_match_quick(const tstr_array *spec,
                                      const tstr_array *arr);

/**
 * Like tstr_array_wildcard_match(), except allows you to specify which
 * part of each array to start comparison at; and whether or not to allow
 * the "**" notation in the spec.
 *
 * \param spec The wildcarded specification of strings to match.
 *
 * \param arr The string literals to be compared against \c spec.
 *
 * \param start_idx_spec Index into the \c spec array at which we should
 * start comparison.  Entries before this will be ignored.  The standard
 * tstr_array_wildcard_match() call starts at zero.
 *
 * \param start_idx_arr Index into the \c arr array at which we should 
 * start comparison.  The element at this position will be compared to the
 * element at the \c start_idx_spec position in \c spec and so forth.
 *
 * \param allow_double_star Should we permit \c spec to end in a "**"
 * entry, meaning to match any number of entries in \c arr?  If false,
 * we just treat "**" as a literal string, not a wildcard.
 *
 * \retval ret_match \c true if the arrays matched, \c false if not.
 */
int tstr_array_wildcard_match_ex(const tstr_array *spec, 
                                 const tstr_array *arr,
                                 int start_idx_spec, int start_idx_arr,
                                 tbool allow_double_star, tbool *ret_match);

tbool tstr_array_wildcard_match_ex_quick(const tstr_array *spec, 
                                         const tstr_array *arr,
                                         int start_idx_spec,
                                         int start_idx_arr,
                                         tbool allow_double_star);

/*
 * Like tstr_array_wildcard_match(), except takes the pattern parts as
 * separate arguments.  start_idx_name tells which element we start
 * comparing in name_parts (0 to do whole array).  We don't have a
 * corresponding index for the pattern; we always start from the
 * first.
 */
int tstr_array_wildcard_match_va(tbool *ret_match, 
                                 const tstr_array *name_parts,
                                 int start_idx_name,
                                 uint32 num_pattern_parts,
                                 const char *pattern_part_1, 
                                 va_list other_pattern_parts);


/* ------------------------------------------------------------------------- */
/** Print each tstring in the array, one per line, to stdout.
 * Maybe useful for debugging purposes.
 */
int tstr_array_print(const tstr_array *arr, const char *prefix);

/* ------------------------------------------------------------------------- */
/** Like tstr_array_print(), only dump to the logs at LOG_NOTICE instead.
 */
int tstr_array_dump(const tstr_array *arr, const char *prefix);

/* ------------------------------------------------------------------------- */
/** Delete all entries in the array that are not prefixed with the
 * specified word.
 */
int tstr_array_delete_non_prefixed(tstr_array *arr, const char *prefix);


/* ------------------------------------------------------------------------- */
/** Remove all entries in the array that match the specified string
 *
 * CAUTION: if the 'str' you pass actually points into one of the
 * elements of the array (e.g. you got it from tstr_array_get_str_quick()
 * earlier), there could be trouble!  Once we delete the element it
 * pointed into, our key will be a dangling pointer, and behavior will
 * be unpredictable.
 */
int tstr_array_delete_str(tstr_array *arr, const char *str);

/* ------------------------------------------------------------------------- */
/** Remove the first entry in the array that matches the specified string,
 * but leave any other remaining entries.
 */
int tstr_array_delete_str_once(tstr_array *arr, const char *str);

/* ------------------------------------------------------------------------- */
int tstr_array_sort_numeric(tstr_array *arr);

/* ------------------------------------------------------------------------- */
/** Compare two tstr_arrays.
 *
 * \retval -1 if a1<a2.  0 if a1 matches a2.  1 if a1>a2.
 */
int32 tstr_array_compare(const tstr_array *a1, const tstr_array *a2);

/* ------------------------------------------------------------------------- */
/** Compare two tstr_arrays, only considering the range of elements
 * specified.  If a mismatch is detected, tell us the first index that
 * didn't match.  If they do match, -1 is returned for this index.
 *
 * \param a1 First array to compare.
 * \param a2 Second array to compare.
 * \param start_idx Index of first element to compare.
 * \param end_idx Index of last element to compare, or -1 to do all elements.
 * \retval ret_mismatch_idx The index of the first element in the specified
 * range to mismatch.  0 is returned if one of the arrays was NULL; and
 * -1 is returned if the arrays matched.
 * \return -1 if a1<a2.  0 if a1 matches a2.  1 if a1>a2.
 */
int32 tstr_array_compare_ex(const tstr_array *a1, const tstr_array *a2,
                            int32 start_idx, int32 end_idx,
                            int32 *ret_mismatch_idx);

#ifdef __cplusplus
}
#endif

#endif /* __TSTR_ARRAY_H_ */
