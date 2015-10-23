/*
 *
 */

#ifndef __HASH_TABLE_H_
#define __HASH_TABLE_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "tstring.h"

/* ------------------------------------------------------------------------- */
/** \file hash_table.h Hash table with bucket chains
 * \ingroup lc_ds
 */

typedef struct ht_table ht_table;
typedef struct ht_node ht_node;
typedef struct ht_options ht_options;
typedef uint_ptr ht_hash_result;


/**
 * Hash a key into an integer.  See comments above ht_insert() for
 * comments on key_len, which apply to all of the callback functions.
 */
typedef ht_hash_result (*ht_hash_func)(const void *key, int32 key_len);

/** Called to see if two keys are equal */
typedef tbool (*ht_key_equal_func)(const void *key1, int32 key1_len,
                                  const void *key2, int32 key2_len);

/** Called when a key is added to the hash.  It is responsible for both
 * the key and the value. */
typedef int (*ht_key_value_set_func)(const void *key, int32 key_len, 
                                     void *value,
                                     void **ret_key, int32 *ret_key_len,
                                     void **ret_value);

/** Called when a key an already added key is duplicated.  XXXX: this is
 * not currently used, as ht_duplicate() is not yet implemented.
 */
typedef int (*ht_key_value_dup_func)(const void *key, int32 key_len,
                                     void **ret_key, int32 *ret_key_len,
                                     void **value);

/** Called when a key is removed */
typedef int (*ht_key_value_free_func)(void **key, int32 key_len,
                                      void **value);


typedef int (*ht_foreach_func)(ht_table *ht, 
                               void *key, int32 key_len, 
                               void *value, void *foreach_data);

/*
 * XXX/EMT: currently if any of the last three function pointers in
 * this structure are NULL, that means we should use the default
 * behavior.  But the standard agreed upon and established by the
 * array library is that NULL should not be used for this purpose,
 * since it may have a meaning of its own that is different from
 * simply using the default.  Instead there should be a built-in
 * function for the default behavior, which would be installed by
 * ht_default_options().
 *
 * Then, for example, if the dup func is NULL, that would mean that
 * the hash table cannot be duplicated.  This is important for the
 * scenario where the hash table takes ownership of elements added to
 * it, and the developer wants to avoid writing a dup function since
 * he never intends to duplicate this particular hash table.  He can
 * then set the dup func to NULL, so attempts to duplicate would cause
 * an error, rather than what would currently happen, which is simply
 * copying all of the pointers, and later running into problems with
 * double-freeing of elements.
 */
struct ht_options {
    ht_hash_func            ho_hash_func;

    ht_key_equal_func       ho_key_equal_func;

    ht_key_value_set_func   ho_key_value_set_func;
    /* NOTE: the dup funtion is not yet used or implemented */
    ht_key_value_dup_func   ho_key_value_dup_func;
    ht_key_value_free_func  ho_key_value_free_func;

    /** If set to false, a duplicate replaces the previous element */
    tbool                    ho_insert_fails_duplicates;
};


int ht_default_options(ht_options *ret_options);

int ht_new(ht_table **ret_ht, const ht_options *options);

/**
 * Free a hash table.  Calls the free callback for all hash table
 * key/values, and frees all memory allocatd by the hash table itself.
 */
int ht_free(ht_table **ht);

/**
 * Calls the free callback for all the hash table key/values, and
 * removes all key/values from the hash table.
 */
int ht_empty(ht_table *ht);

/* XXXX: Not yet implemented */
/* ht_duplicate(const ht_table *src, ht_table **ret_dest); */

/**
 * Insert a key/value into the hash table.
 *
 * If key_len is -1, strlen(key) is computed, so only do this for keys
 * that are of type 'char *' .  If key_len will not be used by your
 * callback functions (like the hash function, set function, etc.), then
 * pass 0 for key_len, and ignore key_len in all your callbacks.
 * Passing a 0 key_len is typical if the key is a structure.
 */
int ht_insert(ht_table *ht, void *key, int32 key_len, void *value);

/**
 * Delete a key/value with the specified key.  The free callback is
 * called for the key/value.
 *
 * See the comment above ht_insert() about key_len.
 */
int ht_delete(ht_table *ht, const void *key, int32 key_len);

/**
 * Similar to ht_delete(), except that it will never automatically
 * resize (shrink) the hash table.  This is useful if you are deleting a
 * great many things, or if you are using the HT_FOREACH() macros .
 *
 * See the comment above ht_insert() about key_len.
 */
int ht_delete_no_resize(ht_table *ht, const void *key, int32 key_len);

/**
 * Find a key/value with the specified key.
 *
 * See the comment above ht_insert() about key_len.
 */
int ht_lookup(const ht_table *ht, const void *key, int32 key_len, 
              tbool *ret_found,
              void **ret_found_key, int32 *ret_found_key_len,
              void **ret_found_value);

/**
 * Find a key/value with the specified key, remove it from the table
 * (without calling the key/value free callback), and return the key and
 * value.
 *
 * See the comment above ht_insert() about key_len.
 */
int ht_remove(ht_table *ht, const void *key, int32 key_len,
              tbool *ret_found,
              void **ret_found_key, int32 *ret_found_key_len,
              void **ret_found_value);

/**
 * Similar to ht_remove() except that it will never automatically resize
 * (shrink) the hash table.
 *
 * See the comment above ht_insert() about key_len.
 */
int ht_remove_no_resize(ht_table *ht, const void *key, int32 key_len,
                        tbool *ret_found,
                        void **ret_found_key, int32 *ret_found_key_len,
                        void **ret_found_value);

int ht_get_size(const ht_table *ht, 
                uint32 *ret_num_buckets, uint32 *ret_num_keys);

uint32 ht_get_num_nodes_quick(const ht_table *ht);
uint32 ht_get_num_buckets_quick(const ht_table *ht);

int ht_foreach(ht_table *ht, ht_foreach_func foreach_func, void *foreach_data);

/** \cond */
/* These two functions and macro are INTERNAL ONLY, do not use them. */
ht_node **_ht_get_bucket_chain_handle(const ht_table *ht, uint32 chain_number);
int _ht_get_node_components(ht_node *node, void **ret_key, int32 *ret_key_len,
                            void **ret_value, ht_node ***ret_next);
/* 
 * This macro allows us to declare and reference variables such that we can
 *    nest calls to HT_FOREACH().
 */
#define _HTVAR(var,ht) _ ## var ## ht
/** \endcond */

/* 
 * The HT_FOREACH() and HT_FOREACH_END() macros allow you to iterate over
 * a hash table in place, without the need to write a callback function.
 *
 * The usage is:
 *
 * HT_FOREACH(myhashtable, mykeyvar, mykeylenvar, myvaluevar) {
 *    <my things to do per hash table entry>
 * } HT_FOREACH_END(myhashtable);
 *
 * Inside the loop you can delete the node you are on using
 * ht_delete_no_resize() or ht_remove_no_resize(), change the value.  You
 * must not change the key, or insert new nodes.  You may do 'break' to end
 * the whole iteration, or 'continue' to go on to the next node.  You can
 * also jump out of the iteration using 'goto'; there is no state that needs
 * cleaning up.
 *
 */

#define HT_FOREACH(_ht, _hkey, _hkey_len, _hvalue)                      \
    do {                                                                \
        uint32 _HTVAR(buck,_ht) = 0;                                    \
        ht_node **_HTVAR(nh,_ht) = NULL, **_HTVAR(next,_ht) = NULL;     \
        const ht_node *_HTVAR(curr,_ht) = NULL;                         \
        tbool _HTVAR(keep_going,_ht) = false;                           \
        if (!_ht) {                                                     \
            err = lc_err_unexpected_null;                               \
            break;                                                      \
        }                                                               \
        for (_HTVAR(buck,_ht) = 0;                                      \
             _HTVAR(buck,_ht) < ht_get_num_buckets_quick(_ht);          \
             _HTVAR(buck,_ht)++) {                                      \
            _HTVAR(nh,_ht) =                                            \
                _ht_get_bucket_chain_handle(_ht, _HTVAR(buck,_ht));     \
            _HTVAR(keep_going,_ht) = true;                              \
            while (1) {                                                 \
                if (!_HTVAR(keep_going,_ht) &&                          \
                    _HTVAR(nh,_ht) &&                                   \
                    _HTVAR(curr,_ht) == *_HTVAR(nh,_ht)) {              \
                    _HTVAR(nh,_ht) = _HTVAR(next,_ht);                  \
                }                                                       \
                _HTVAR(keep_going,_ht) = true;                          \
                if (!_HTVAR(nh,_ht) || !*_HTVAR(nh,_ht)) {              \
                    break;                                              \
                }                                                       \
                _HTVAR(keep_going,_ht) = false;                         \
                _ht_get_node_components(*_HTVAR(nh,_ht),                \
                                        &(_hkey), &(_hkey_len),         \
                                        &(_hvalue), &_HTVAR(next,_ht)); \
                _HTVAR(curr,_ht) = *_HTVAR(nh,_ht);                     \


#define HT_FOREACH_END(_ht)                                             \
                _HTVAR(keep_going,_ht) = true;                          \
                if (_HTVAR(nh,_ht) &&                                   \
                    _HTVAR(curr,_ht) == * _HTVAR(nh,_ht)) {             \
                    _HTVAR(nh,_ht) = _HTVAR(next,_ht);                  \
                }                                                       \
            }                                                           \
                                                                        \
           if (!_HTVAR(keep_going,_ht)) {                               \
               break;                                                   \
           }                                                            \
        }                                                               \
    } while(0)


int ht_foreach_delete(ht_table *ht, 
                      ht_foreach_func foreach_func, 
                      void *foreach_data);

int ht_foreach_remove(ht_table *ht, 
                      ht_foreach_func foreach_func, 
                      void *foreach_data);


/* ------------------------------------------------------------------------- */
/* Helper functions */

/** The string hash is taken from the public domain Bob Jenkins hash */
uint32 ht_bjh_hash(register const uint8 *k,    /* the key */
                   register uint32 length,     /* the length of the key */
                   register uint32 initval);   /* prev hash, or arb. value */

ht_hash_result ht_hash_string_func(const void *key, int32 key_len);

tbool ht_key_equal_string_func(const void *key1, int32 key1_len,
                              const void *key2, int32 key2_len);

ht_hash_result ht_hash_tstring_func(const void *key, int32 key_len);
tbool ht_key_equal_tstring_func(const void *key1, int32 key1_len,
                               const void *key2, int32 key2_len);

ht_hash_result ht_hash_direct_func(const void *key, int32 key_len);
tbool ht_key_equal_direct_func(const void *key1, int32 key1_len,
                              const void *key2, int32 key2_len);

/* ------------------------------------------------------------------------- */
/* for hash tables that have a string key and string value */
int ht_string_set_func(const void *key, int32 key_len, void *value,
                       void **ret_key, int32 *ret_key_len,
                       void **ret_value);
int ht_string_dup_func(const void *key, int32 key_len, void **ret_key,
                       int32 *ret_key_len, void **value);
int ht_string_free_func(void **key, int32 key_len, void **value);

/* ------------------------------------------------------------------------- */
/* for hash tables that have a tstring key and a tstring value */
int ht_tstring_set_func(const void *key, int32 key_len, void *value,
                        void **ret_key, int32 *ret_key_len,
                        void **ret_value);
int ht_tstring_dup_func(const void *key, int32 key_len, void **ret_key,
                        int32 *ret_key_len, void **value);
int ht_tstring_free_func(void **key, int32 key_len, void **value);

/**
 * Create a new hash table using options geared towards using a tstring
 * key and a tstring value.  This is a string map, using tstring's.
 *
 * To add things to such a table:
 *
 *   err = ht_insert(ht, name, ts_length(name), value);
 *   bail_error(err);
 *
 * To retrieve a reference to the tstring owned by the table (which you 
 * may modify, but NOT free):
 *
 *   err = ht_lookup(ht, name, ts_length(name), &found, NULL, NULL, &value_v);
 *   bail_error(err);
 *   value = value_v;
 */
int ht_new_tstring_tstring(ht_table **ret_ht, tbool insert_fails_duplicates);

/* ------------------------------------------------------------------------- */
/* for hash tables that have a tstring key and a tstr_array value */
int ht_tstr_array_dup_set_func(const void *key, int32 key_len, void *value,
                               void **ret_key, int32 *ret_key_len,
                               void **ret_value);
int ht_tstr_array_own_set_func(const void *key, int32 key_len, void *value,
                               void **ret_key, int32 *ret_key_len,
                               void **ret_value);
int ht_tstr_array_dup_func(const void *key, int32 key_len, void **ret_key,
                           int32 *ret_key_len, void **value);
int ht_tstr_array_free_func(void **key, int32 key_len, void **value);


#ifdef __cplusplus
}
#endif

#endif /* __HASH_TABLE_H_ */
