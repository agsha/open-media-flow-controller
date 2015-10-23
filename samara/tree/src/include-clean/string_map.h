/*
 *
 * string_map.h
 *
 *
 *
 */

#ifndef __STRING_MAP_H_
#define __STRING_MAP_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file string_map.h String map routines
 * \ingroup lc_ds
 */

#include "tstring.h"

typedef struct string_map string_map;

/**
 * Create a new string map.
 * \param ret_smap pointer to hold the new string map.
 * \return 0 on success, error code on failure.
 */
int smap_new(string_map **ret_smap);

/**
 * Free a string map.
 * \param inout_smap the string map to free.
 * \return 0 on success, error code on failure.
 */
int smap_free(string_map **inout_smap);

/**
 * Insert a new entry into the string map.
 * \param smap the string map.
 * \param str the string.
 * \param id the enum id of the string.
 * \return 0 on success, error code on failure.
 */
int smap_insert(string_map *smap, tstring *str, int_ptr id);

/**
 * Insert a new entry into the string map.
 * \param smap the string map.
 * \param str the string.
 * \param id the enum if of the string.
 * \return 0 on success, error code on failure.
 */
int smap_insert_str(string_map *smap, const char *str, int_ptr id);

/**
 * Get the string for the given id.
 * \param smap the string map.
 * \param id the id.
 * \param ret_str the string.
 * \return 0 on success, error code on failure.
 */
int smap_get_string(const string_map *smap, int_ptr id, tstring **ret_str);

/**
 * Get the string for the given id.
 * \param smap the string map.
 * \param id the id.
 * \param ret_str the string.
 * \return 0 on success, error code on failure.
 */
int smap_get_string_str(const string_map *smap, int_ptr id, char **ret_str);

/**
 * Get the id for the given string.
 * \param smap the string map.
 * \param str the string.
 * \param ret_id the id.
 * \return 0 on success, error code on failure.
 */
int smap_get_id(const string_map *smap, tstring *str, int_ptr *ret_id);

/**
 * Get the id for the given string.
 * \param smap the string map.
 * \param str the string.
 * \param ret_id the id.
 * \return 0 on success, error code on failure.
 */
int smap_get_id_str(const string_map *smap, const char *str,
                    int_ptr *ret_id);


#ifdef __cplusplus
}
#endif

#endif /* __STRING_MAP_H_ */
