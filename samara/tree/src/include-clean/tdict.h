/*
 *
 * tdict.h
 *
 *
 *
 */

#ifndef __TDICT_H_
#define __TDICT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file tdict.h Tall Maple dictionary data type.
 * \ingroup lc_ds
 *
 * Maps strings to strings, one-to-one.  This is a thin wrapper around
 * the ht_table data structure (from src/include/hash_table.h), to
 * make common tasks easier.
 */

#include "common.h"

typedef struct tdict tdict;


/**
 * Create a new dictionary.
 *
 * \retval ret_dict Newly created dictionary.
 *
 * \param options Options parameter to allow for future expansion.
 * For now, NULL must be passed here; in the future, NULL will mean
 * to accept default settings.
 */
int tdict_new(tdict **ret_dict, void *options);

/**
 * Free a dictionary.
 *
 * \param inout_dict Dictionary to be freed.  Pointer will be set to
 * NULL on successful free.
 */
int tdict_free(tdict **inout_dict);

/**
 * Remove all entries from the dictionary, but leave the dictionary
 * allocated.
 *
 * \param dict Dictionary.
 */
int tdict_empty(tdict *dict);

/**
 * Set an entry in the dictionary.  If the entry already existed, it
 * will be silently replaced.  Calling this with a value of NULL is
 * the same as calling tdict_remove() on the key, except that it is not
 * an error if the key did not already exist.
 *
 * \param dict Dictionary.
 * \param name Name of mapping to set.
 * \param value New value for this name.
 * \param flags Flags bitfield for future expansion.  For now, must be zero.
 */
int tdict_set(tdict *dict, const char *name, const char *value,
              uint32 flags);

/**
 * Remove an entry from the dictionary.  If the entry did not exist,
 * lc_err_not_found is returned.
 *
 * \param dict Dictionary.
 * \param name Name of mapping to remove.
 */
int tdict_remove(tdict *dict, const char *name);

/**
 * Look up an entry in the dictionary.
 *
 * NOTE: if the entry is not found, ret_value gets NULL, but this is
 * NOT an error.  (i.e. we do NOT return lc_err_not_found under any
 * circumstances)
 *
 * \param dict Dictionary.
 * \param name Name of mapping to look up.
 * \retval ret_value Value found for this name, if any.  If the name is
 * not found, NULL is returned with no error.  NOTE: the pointer returned
 * here is to the original copy of the string within the tdict data 
 * structure, and is NOT a duplicate for the caller.  That is, the caller
 * does not own it, and must not free it.
 */
int tdict_lookup(const tdict *dict, const char *name, 
                 const char **ret_value);

/**
 * Function to be called back by tdict_foreach().
 */
typedef int (*tdict_foreach_func)(const tdict *dict,
                                  const char *name, const char *value,
                                  void *callback_data);

/**
 * Call a callback function once for every entry in the dictionary.
 */
int tdict_foreach(const tdict *dict, tdict_foreach_func callback_func,
                  void *callback_data);

#ifdef __cplusplus
}
#endif

#endif /* __TDICT_H_ */
