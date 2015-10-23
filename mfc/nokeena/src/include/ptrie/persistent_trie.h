/*
 * persistent_trie.h -- Persistent Trie interface built upon cprops trie.
 *
 * Persistent tree (ptrie) provides a lock less read by value interface 
 * along with transaction like updates and persistence allowing recovery 
 * to the last commit point upon abnormal termination.
 */

#ifndef _PERSISTENT_TRIE_H_
#define _PERSISTENT_TRIE_H_

#include "ptrie/persistent_trie_int.h"
/*
 *******************************************************************************
 * Interface Definitions
 *******************************************************************************
 */

/* Features:
 *   1. Lockless return by value reader interfaces.
 *   2. Transaction like semantics on update.
 *   3. Data persistence with transaction recovery semantics at system restart.
 *
 * Reader side interfaces:
 *   1. ptrie_prefix_match()
 *   2. ptrie_exact_match()
 *   
 *   - No caller restrictions.
 *
 * Update side interfaces:
 *   1. ptrie_tr_prefix_match()
 *   2. ptrie_tr_exact_match()
 *   3. ptrie_update_appdata()
 *   4. ptrie_add()
 *   5. ptrie_remove()
 *   6. ptrie_reset()
 *   7. ptrie_get_fh_data()
 *
 *   8. ptrie_begin_xaction()
 *   9. ptrie_end_xaction()
 *
 *   - Calls 1-6 and 9 can only be invoked after ptrie_begin_xaction()
 *     has been called.
 *
 *   - Caller insures that all calls associated with a given context are
 *     made within a single thread (ie no thread locking considerations).
 *
 * Overview:
 *   Two in memory Trie(s) are maintained which are referred to as
 *   CurrentTrie and ShadowTrie.
 *   All reader side interfaces operate on the CurrentTrie.
 *   Update side interfaces operate on the ShadowTrie.
 *
 *   Two checkpoint files are maintained which are referred to as
 *   CurrentCkpt and ShadowCkpt.   
 *   The checkpoint file is a key/value representation of the memory Trie.
 *
 *   A begin_xaction/end_xaction paradigm is used where all actions within 
 *   a begin_xaction/end_xaction only operate on the ShadowTrie where updates
 *   are logged in a memory transaction log.
 *   When end_xaction is invoked:
 *     1) The ShadowTrie updates are written to ShadowCkpt.
 *     2) The ShadowTrie updates are written to CurrentCkpt.
 *     3) The ShadowTrie and CurrentTrie are atomically swapped allowing 
 *        the readers to see the new updates.
 *     4) The stale Trie, which was formerly the CurrentTrie and is now the
 *        ShadowTrie, is updated so that it is identical to the CurrentTrie.
 *
 *   The checkpoint files are written using an ordered write scheme which is
 *   as follows:
 *     1) Time and sequence number are updated in the primary header.
 *     2) Updates applied.
 *     3) Priamry header copied to the secondary header.
 *
 *   Checkpoint files where the primary and secondary headers are not equal
 *   are considered inconsistent and will be synchronized from the other
 *   checkpoint file assuming it is consistent.
 *
 *   At system start, recovery is as follows:
 *     1) Checkpoint file reconcilation:
 *       A) Both checkpoint files consistent
 *          a) File1 seqno == File2 seqno, no action required.
 *          b) File1 seqno >  File2 seqno, copy File1 to File2
 *          c) File1 seqno <  File2 seqno, copy File2 to File1
 *
 *       B) One inconsistent checkpoint file, copy consistent file contents
 *          to the inconsistent file.
 *
 *       C) Both checkpoint files inconsistent, initialize the files to the
 *          zero state.
 *
 *     2) File1 becomes the CurrentCkpt and File2 becomes ShadowCkpt.
 *        The CurrentTrie and ShadowTrie are initialized from the CurrentCkpt.
 */

#define PTRIE_INTF_VERSION 1

typedef struct ptrie_config {
    int interface_version; // Set to PTRIE_INTF_VERSION
    /*
     * NULL proc or value pointer arg will use the system default.
     */
    int *log_level;
    int (*proc_logfunc)(ptrie_log_level_t level, const char *fmt, ...);
    void *(*proc_malloc)(size_t size);
    void *(*proc_calloc)(size_t nmemb, size_t size);
    void *(*proc_realloc)(void *ptr, size_t size);
    void (*proc_free)(void *ptr);
    char pad[128];
} ptrie_config_t;

/*
 * ptrie_init() - Subsystem initialization
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int 
ptrie_init(const ptrie_config_t *cfg);

/*
 * new_ptrie_context() - Create Persistent Trie (ptrie) context
 *
 *  - (*copy_app_data)() user data specific function for app_data_t
 *  - (*destruct_app_data)() deallocate deep data associated with app_data_t,
 * 			     note that caller deallocates app_data_t.
 *
 * NULL proc arg implies no action required.
 *
 * Return:
 *  !=0, Success, pointer ptrie_context_t
 *  == 0, Error
 */
ptrie_context_t *
new_ptrie_context(void (*copy_app_data)(const app_data_t *src,
				        app_data_t *dest),
		  void (*destruct_app_data)(app_data_t *d));

/*
 * delete_ptrie_context() - Delete Persistent Trie (ptrie) context
 */
void 
delete_ptrie_context(ptrie_context_t *ctx);

/*
 * ptrie_recover_from_ckpt() - Recover Trie from checkpoint file
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int 
ptrie_recover_from_ckpt(ptrie_context_t *ctx,
			const char *ckp_file1, const char *ckp_file2);

/*
 * ptrie_prefix_match() - Reader side prefix match, return app_data_t
 *
 * Assumptions:
 *  1) Reader side interface.
 *  2) Concurrent callers supported.
 *  3) Return values as defined by cp_trie_prefix_match()
 */
int 
ptrie_prefix_match(ptrie_context_t *ctx, const char *key, app_data_t *ad);

/* 
 * ptrie_exact_match() - Reader side exact match, return app_data_t
 *
 * Assumptions:
 *  1) Reader side interface.
 *  2) Concurrent callers supported.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int 
ptrie_exact_match(ptrie_context_t *ctx, const char *key, app_data_t *ad);

/* 
 * ptrie_lock() - Reader side ptrie lock primitive
 *
 * Assumptions:
 *  1) Reader side interface.
 *  2) Concurrent callers supported.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int
ptrie_lock(ptrie_context_t *ctx);

/* 
 * ptrie_unlock() - Reader side ptrie unlock primitive
 *
 * Assumptions:
 *  1) Reader side interface.
 *  2) Concurrent callers supported.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int
ptrie_unlock(ptrie_context_t *ctx);

/* 
 * ptrie_list_keys() - Reader side, list keys associated with reader ptrie
 *		       checkpoint file.
 *
 * For each key, invoke int (*proc)(key, arg). non zero proc return 
 * indicates abort further action and return.
 *
 * Assumptions:
 *  1) Reader side interface.
 *  2) Concurrent callers supported.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int 
ptrie_list_keys(ptrie_context_t *ctx, void *proc_arg,
		int (*proc)(const char *key, void *proc_arg));

/*
 * ptrie_tr_prefix_match() - Update side prefix match, return (app_data_t *)
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *  4) Return values as defined by cp_trie_prefix_match()
 */
int 
ptrie_tr_prefix_match(ptrie_context_t *ctx, const char *key, app_data_t **ad);

/* 
 * ptrie_tr_exact_match() - Update side exact match, return (app_data_t *)
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *
 * Return:
 *  !=0, Success, pointer app_data_t
 *  ==0, Error
 */
app_data_t *
ptrie_tr_exact_match(ptrie_context_t *ctx, const char *key);

/*
 * ptrie_update_appdata() - Update side, update trie app_data_t
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *  4) orig_ad must be obtained via ptrie_tr_prefix_match() or
 *     ptrie_tr_exact_match().
 *
 * Return:
 *    ==0, success
 *    > 0, error
 *    < 0, transaction log full, commit current transaction and retry
 */
int 
ptrie_update_appdata(ptrie_context_t *ctx, const char *key,
		     app_data_t *orig_ad, const app_data_t *new_ad);

/*
 * ptrie_add() - Update side, add or update trie app_data_t 
 *	     	 associated with given key
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *  4) orig_ad must be obtained via ptrie_tr_prefix_match() or
 *     ptrie_tr_exact_match().
 *  5) orig_ad is only valid within the current ptrie_begin_xaction()
 *     context.   When ptrie_end_xaction() is issued, all orig_ad pointers
 *     are invalid.
 *
 * Return:
 *    ==0, success
 *    > 0, error
 *    < 0, transaction log full, commit current transaction and retry
 */
int 
ptrie_add(ptrie_context_t *ctx, const char *key, app_data_t *ad);

/*
 * ptrie_remove - Update side, remove trie app_data_t associated 
 *	          with the given key
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *  
 * Return:
 *    ==0, success
 *    > 0, error
 *    < 0, transaction log full, commit current transaction and retry
 */
int 
ptrie_remove(ptrie_context_t *ctx, const char *key);

/*
 * ptrie_reset - Reset trie and checkpoint file state to null.
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Must be executed under ptrie_{begin,end}_xaction()
 *  3) Concurrent callers not supported, user enforces this condition.
 *  
 * Return:
 *    ==0, success
 *    > 0, error
 *    < 0, transaction log full, commit current transaction and retry
 */
int 
ptrie_reset(ptrie_context_t *ctx);

/*
 * ptrie_get_fh_data() - Update side, get fh_user_data_t persistent data 
 *			 associated with last xaction commit.
 * Assumptions:
 *  1) Update side interface.
 *
 * Return:
 *  !=0, Success, pointer fh_user_data_t
 *  ==0, Error
 */
const fh_user_data_t *
ptrie_get_fh_data(ptrie_context_t *ctx);

/*
 * ptrie_begin_xaction() - Update side, begin transaction
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Concurrent callers not supported, user enforces this condition.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int 
ptrie_begin_xaction(ptrie_context_t *ctx);

/*
 * ptrie_end_xaction() - Update side, commit or abort transaction
 *
 * Assumptions:
 *  1) Update side interface.
 *  2) Concurrent callers not supported, user enforces this condition.
 *
 * Return:
 *  ==0, Success
 *  >0, Error, commit aborted
 *  <0, Unrecoverable error, reinitialize system
 *
 */
int ptrie_end_xaction(ptrie_context_t *ctx, int commit, fh_user_data_t *fhd);

#endif /* _PERSISTENT_TRIE_H_ */

/*
 * End of persistent_trie.h
 */
