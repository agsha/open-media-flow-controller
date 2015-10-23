/*
 *
 * src/bin/mgmtd/md_utils.h
 *
 *
 *
 */

#ifndef __MD_UTILS_H_
#define __MD_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#if defined(VALGRIND_ENABLE)
#include <valgrind/valgrind.h>
#include <valgrind/callgrind.h>
#include <valgrind/memcheck.h>
#endif

#include "bnode.h"
#include "hash_table.h"
#include "md_commit.h"
#include "mdb_db.h"
#include "proc_utils.h"
#include "ftrans_utils.h"
#include "md_upgrade.h"
#include "tacl.h"
#include "md_mod_reg.h"

/*
 * Here only for legacy, to not break .c files which depend on
 * functions from md_net_utils.h, which used to be in here.
 */
#include "md_net_utils.h"

#define MD_EMAIL_DEF_DOMAIN   "localdomain"


/**
 * \file src/bin/mgmtd/md_utils.h General utilities for mgmtd modules.
 * \ingroup mgmtd
 */

/* ================================================== */
/* Module helper functions */                                          
/* ================================================== */

int md_utils_init(void);
int md_utils_deinit(void);

typedef enum {
    mchf_none = 0,

    /**
     * Do not include a timestamp in the header.  You might use this if
     * you were going to use lao_compare_orig when activating the file,
     * since otherwise the files would never match.
     */
    mchf_no_timestamp =  1 << 0,

} md_conf_header_flags;

typedef uint32 md_conf_header_flags_bf;

/**
 * Writes the 'do not change' header to the given fd
 */
int
md_conf_file_write_header_opts(int fd, const char *file_descr, 
                               const char *prefix_str,
                               md_conf_header_flags_bf flags);

/**
 * Calls md_conf_file_write_header_opts() with no option flags.
 */
int
md_conf_file_write_header(int fd, const char *file_descr, 
                          const char *prefix_str);

/**
 * Read in the given template file and perform a search and replace
 * on special keys inside the file. Keys are delimited using the '@'
 * symbol. For example:
 *
 *     IPADDR=\@my_ip_address\@
 *
 * The given hash tables maps keys to literals that should replace
 * the key inside the template. The keys should NOT contain the
 * '@' character in them.
 *
 * It is assumed that the given hashtable contains tstring keys
 * and tstring values.
 *
 * \param template_path the full path to the template.
 * \param table the mappings between keys and literals.
 * \param dest_fd the destination fd.
 * \return 0 on success, error code on failure.
 */
int md_generate_conf_file_from_template(const tstring *template_path,
                                        const ht_table *table, int dest_fd);

/**
 * Convenience function for create a tstring/tstring hash table
 * for use with the md_generate_conf_file_from_template function above.
 * The returned pointer will be NULL on error.
 * \param ret_table will contain the new hash table.
 * \return 0 on success, error code on failure.
 */
int md_new_template_table(ht_table **ret_table);

/**
 * Add an entry into the template table.
 * \param table the table to which to append an entry
 * \param name the name of the entry.
 * \param value the value of the entry.
 * \return 0 on success, error code on failure.
 */
int md_template_table_append(ht_table *table, const tstring *name,
                             const tstring *value);
int md_template_table_append_str(ht_table *table, const char *name,
                                 const char *value);

/**
 * Query a node from the db and use the resulting value to create a 
 * new entry in the template table.
 * \param commit Opaque commit structure provided by mgmtd infrastructure.
 * \param db Database to query node from.
 * \param node_name Name of node to query.
 * \param table Template table to add entry to.
 * \param table_entry_name Name of entry to create in the table.
 * \param flags Currently unused.  Reserved for future use to add 
 * options without breaking the API, e.g. transliterating booleans
 * to "yes" and "no".
 */
int md_template_table_copy_node(md_commit *commit, const mdb_db *db,
                               const char *node_name, ht_table *table,
                               const char *table_entry_name, int flags);

/**
 * Convenience function that goes through all the leaf nodes under
 * node_name and adds their values (as a string) into the given table
 * for use with the md_generate_conf_file_from_template function above.
 * In the template file, you will want to use (for example):
 *
 *   @/system/hostname@
 *
 * If you called:
 *
 *   err = md_populate_template_table(table, binding_array);
 *   bail_error(err);
 *
 * \param table the table.
 * \param binding_array the array of bindings.
 * \return 0 on success, error code on failure.
 */
int md_populate_template_table(ht_table *table,
                               bn_binding_array *binding_array);

/*
 * XXX yet to spec:
 * 
 * - read/write proc file / value from proc file
 * - read/write sysctl
 */


/**
 * Tells how to process an action which is to be forwarded to an
 * external daemon.
 */
typedef enum {
    /**
     * Do not block, but do not return real response: return success
     * immediately.
     *
     * NOTE: this is still subject to barrier behavior, so "immediately"
     * means "immediately when it comes time to process this request";
     * we do not wait for the daemon's response.
     */
    mfat_immediate = false,

    /**
     * Block, and return response when it arrives.  Note that this blocks
     * mgmtd entirely while the action is outstanding, so do not use this
     * with long-running actions.
     */
    mfat_blocking = true,

    /**
     * Do not block, and return the real response when it arrives.
     * Also, treat this action as completely independent of other
     * requests with respect to ordering: do not act as a barrier,
     * and do not be held up by other barriers.
     *
     * NOTE: in order to use this, you must assign md_commit_forward_action
     * to mrn_action_async_nonbarrier_start_func in your node registration,
     * instead of mrn_action_func.
     */
    mfat_nonbarrier_async = 2
} md_commit_forward_action_type;

/**
 * Generic action handler that can be used if the desired handling is
 * simply to forward the action on to a mgmtd client on the session it
 * has already established with mgmtd.  This finds the session
 * pointer, constructs an action message having the same action name
 * and attached bindings, and sends it to the specified session.
 *
 * The cb_arg parameter is cast as an (md_commit_forward_action_args *).
 */
typedef struct md_commit_forward_action_args {

    /**
     * The name of the client to which the action should be forwarded.
     */
    const char *mcfa_provider;

    /**
     * If true, an error code will be returned from the action if the
     * specified provider does not have a session open with mgmtd.
     * Otherwise, the action is counted as successful no matter what.
     */
    tbool mcfa_error_if_dropped;

    /**
     * If mcfa_error_if_dropped is true, the user-visible error
     * message that should accompany the error code.  If NULL, no
     * message is returned; this should only be used if something is
     * going to deal with the error programmatically and generate its
     * own message or shield the user from the error.
     */
    const char *mcfa_dropped_error_msg;

    /**
     * Select what blocking/async and barrier behavior we want for
     * this action.
     */
    md_commit_forward_action_type mcfa_block_for_response;

#ifdef PROD_FEATURE_I18N_SUPPORT
    const char *mcfa_gettext_domain;
#endif
} md_commit_forward_action_args;

/**
 * An action handler that can be used to forward an action request to
 * an external provider.  Assign this to your ::mrn_action_func field.
 * When using this, the ::mrn_action_arg field must have a pointer 
 * to a ::md_commit_forward_action_args structure.
 */
int md_commit_forward_action(md_commit *commit, const mdb_db *db,
                             const char *action_name,
                             bn_binding_array *params, void *cb_arg);

int md_commit_forward_action_full(md_commit *commit, const mdb_db *db,
                                  const char *action_name, 
                                  bn_binding_array *params,
                                  const char *provider,
                                  tbool error_if_dropped,
                                  const char *dropped_error_msg,
                                  md_commit_forward_action_type fwd_type);

/**
 * Create a commit structure based off mgmtd global state.
 * Not recommended for the faint of heart.
 *
 * Use md_commit_commit_state_free() in md_commit.h to free it
 * when you're done.
 */
int md_commit_create_commit(md_commit **ret_commit);

/**
 * Option flags for:
 *   - md_commit_handle_commit_from_module_flags()
 *   - md_commit_handle_action_from_module_flags()
 *   - md_commit_handle_event_from_module_takeover_flags()
 */
typedef enum {
    mchmf_none = 0,

    /**
     * By default, these APIs run as uid 0, regardless of the credentials
     * in the 'md_commit *' they are passed.  Sometimes this might be what
     * you want, if you have carefully determined what this request should
     * do, and it should not be hampered by the limitations of the user
     * who called you.  But alternately, you may want the enforcement that
     * would have been applied if the user had made this same request
     * directly.
     *
     * Use this flag to request that the credentials in the commit
     * provided be used for this call.  Note that this will have no effect
     * (but is not an error) if a NULL commit is passed.
     */
    mchmf_use_credentials = 1 << 0,

} md_commit_handle_module_flags;

/** Bit field of ::md_commit_handle_module_flags ORed together */
typedef uint32 md_commit_handle_module_flags_bf;


/**
 * Take an action request initiating from inside mgmtd and process it
 * as if it arrived on a client session.  As the action may change the
 * db, the new db is optionally returned.
 *
 * NOTE: this runs the action as uid 0, even if you pass your original
 * commit and the user who set the ball rolling was not uid 0.
 * See also md_commit_handle_action_from_module_flags().
 *
 * NOTE: it is not safe to invoke actions that change the database
 * from within a commit callback function (side effects, check, or
 * apply).
 */
int md_commit_handle_action_from_module(md_commit *commit,
                                        const bn_request *request,
                                        mdb_db **ret_new_db,
                                        uint16 *ret_return_code,
                                        tstring **ret_return_message,
                                        bn_binding_array **ret_return_bindings,
                                        uint32_array **ret_return_node_ids);

/**
 * Like md_commit_handle_action_from_module(), except with 'flags'.
 */
int md_commit_handle_action_from_module_flags(md_commit *commit,
                                        const bn_request *request,
                                        md_commit_handle_module_flags_bf flags,
                                        mdb_db **ret_new_db,
                                        uint16 *ret_return_code,
                                        tstring **ret_return_message,
                                        bn_binding_array **ret_return_bindings,
                                        uint32_array **ret_return_node_ids);

/**
 * Take an event request initiating from inside mgmtd and process it
 * as if it arrived on a client session.
 *
 * NOTE: this runs the action as uid 0, even if you pass your original
 * commit and the user who set the ball rolling was not uid 0.
 * See also md_commit_handle_event_from_module_takeover_flags().
 */
int md_commit_handle_event_from_module(md_commit *commit,
                                       const bn_request *request,
                                       uint16 *ret_return_code,
                                       tstring **ret_return_message,
                                       bn_binding_array **ret_return_bindings,
                                       uint32_array **ret_return_node_ids);

/**
 * Same as md_commit_handle_event_from_module(), except it takes over
 * ownership of the request given to it.  If you don't need to keep your
 * own copy of the event after sending it (most callers just free it
 * immediately afterwards), calling this prevents an additional copy
 * of the event.
 *
 * NOTE: this runs the action as uid 0, even if you pass your original
 * commit and the user who set the ball rolling was not uid 0.
 * See also md_commit_handle_event_from_module_takeover_flags().
 */
int
md_commit_handle_event_from_module_takeover(md_commit *commit,
                                        bn_request **inout_request,
                                        uint16 *ret_return_code,
                                        tstring **ret_return_message,
                                        bn_binding_array **ret_return_bindings,
                                        uint32_array **ret_return_node_ids);

/**
 * Like md_commit_handle_event_from_module_takeover(), except with 'flags'.
 */
int
md_commit_handle_event_from_module_takeover_flags(md_commit *commit,
                                        bn_request **inout_request,
                                        md_commit_handle_module_flags_bf flags,
                                        uint16 *ret_return_code,
                                        tstring **ret_return_message,
                                        bn_binding_array **ret_return_bindings,
                                        uint32_array **ret_return_node_ids);

/**
 * Take a set (commit) request initiating from a mgmtd module, and
 * process it as if it arrived on a client session.  Usually this is 
 * used from an internal action callback which was registered with the
 * mrn_action_config_func field.  There are certain other contexts from
 * which calling this API is not safe.  Please consult your Tall Maple
 * technical contact before calling this from anywhere other than an
 * internal action function registered with mrn_action_config_func.
 *
 * NOTE: regardless of whether you pass your own commit structure in the
 * first parameter, the commit will NOT be restricted according to the
 * privileges of the user who initiated the action.  The commit will
 * ALWAYS run as full admin (uid 0).  Passing your commit structure
 * only gets you attribution for the commit in the audit logs, and
 * possibly other secondary benefits in that vein.  See
 * md_commit_handle_commit_from_module_flags() for options to run the
 * commit with the privileges of the user who initiated the action.
 *
 * NOTE: this commit may change the main_db pointer, which could
 * invalidate the new_db pointer that you were working with before.
 * The new new_db is returned in the third argument, so if you are
 * going to need to access it after this call, make sure to take this,
 * and throw away your old new_db pointer.
 *
 * \param commit Commit context structure for this commit.  Usually you
 * would pass the commit structure with which you were invoked.  See note
 * above about what impact this has on access control.
 *
 * \param request The request to execute.  This must be a Set request;
 * any other kind will fail.
 *
 * \retval ret_new_db The revised new_db pointer.  The caller should
 * throw out the new_db pointer they were passed (which may now be
 * incorrect), and replace it with this one.
 *
 * \retval ret_return_code The response code resulting from this commit:
 * zero for success, nonzero for failure.  NOTE: you cannot tell from 
 * this whether the commit succeeded or failed -- see bug 13750.
 *
 * \retval ret_return_message The response message resulting from this
 * commit, if any.
 *
 * \retval ret_return_bindings Bindings resulting from the commit,
 * which would have been sent back in the Set response if this had
 * come in as a request.  Note that currently, Set responses never
 * contain bindings (generally only Query and Action responses do),
 * so this would be empty or NULL.  Pass NULL to skip.
 *
 * \retval ret_return_node_ids Node IDs resulting from the commit.
 * As with return bindings, Set responses generally do not contain
 * node IDs, so pass NULL here to skip this.
 */
int md_commit_handle_commit_from_module(md_commit *commit,
                                        const bn_request *request,
                                        mdb_db **ret_new_db,
                                        uint16 *ret_return_code,
                                        tstring **ret_return_message,
                                        bn_binding_array **ret_return_bindings,
                                        uint32_array **ret_return_node_ids);

/**
 * Same as md_commit_handle_commit_from_module() except:
 *
 *   - We accept a 'flags' parameter with option flags.
 *
 *   - 'ret_return_code' is a uint32 instead of a uint16.  As described in
 *     bug 13750, the uint16 did not let the caller know if the commit 
 *     succeeded or failed, since it could be either a check failure or 
 *     an apply failure.  This one, just like the uint32 return code a
 *     GCL client gets back from mgmtd, has room for both.  You can use
 *     mdc_response_code_components() from md_client.h to deconstruct
 *     this into the response codes from the two phases.
 */
int md_commit_handle_commit_from_module_flags(md_commit *commit,
                                        const bn_request *request,
                                        md_commit_handle_module_flags_bf flags,
                                        mdb_db **ret_new_db,
                                        uint32 *ret_return_code,
                                        tstring **ret_return_message,
                                        bn_binding_array **ret_return_bindings,
                                        uint32_array **ret_return_node_ids);


/**
 * Send the "/pm/actions/send_signal" action to PM to cause it to send
 * a signal to a process, or the "/pm/actions/restart_process" action to
 * cause it to kill and then restart a process.
 *
 * If PM does not have a connection to mgmtd, failure is returned.
 * But if the message was successfully forwarded to PM, success
 * is returned -- we do not wait for a response from PM to confirm 
 * that it was able to successfully execute the request (XXX/EMT).
 */
int md_send_signal_to_process(md_commit *commit, int32 signum,
                              const char *proc_name, tbool quiet_process);

int md_restart_process(md_commit *commit, const char *proc_name);

/*
 * NOTE: md_restart_process_condition_map (in md_utils.c)
 * must be kept in sync.
 */
typedef enum {
    mrc_always = 0,
    mrc_only_if_running,
    mrc_only_if_not_running
} md_restart_process_condition;

extern const lc_enum_string_map md_restart_process_condition_map[];

int md_restart_process_ex(md_commit *commit, const char *proc_name,
                          md_restart_process_condition cond,
                          uint16 *ret_code, tstring **ret_msg);

typedef enum {
    mtpf_none = 0,

    /**
     * "Sticky" process termination means that the process won't be
     * restarted just from the auto_launch field changing from false
     * to true.  It will need to be manually restarted with a restart
     * action.
     */
    mtpf_sticky = 1 << 0,

} md_terminate_process_flags;

typedef uint32 md_terminate_process_flags_bf;

int md_terminate_process(md_commit *commit, const char *proc_name,
                         md_terminate_process_flags_bf flags,
                         uint16 *ret_code, tstring **ret_msg);

/**
 * Find the specified user's home directory based on the configured
 * bindings in the /auth/passwd subtree. If specified, return the uid
 * and/or the gid of the user as well.
 */
int
md_find_home_dir(md_commit *commit, const mdb_db *db, const char *username,
                 tstring **ret_home_dir, uint32 *ret_uid, uint32 *ret_gid);

/**
 * Determine if given user name is a reserved account.
 */
tbool md_passwd_is_resv_acct(const char *username);

/**
 * Write some number of specified keys out to a file.
 * \param commit
 * \param db the config database holding the bindings
 * \param ident_bn_prefix binding to iterate to get list of identities
 * \param key_bn_child_name child binding name of desired key
 * \param keys_bn_iter_fmt binding format string to iterate if more than
 *       one key is being written
 * \param key_file_name name of key file to write
 * \param key_dir_name possible directory to write key file to
 * \param use_home_dir write key file to .ssh directory of a user's home
 *       directory
 */
int
md_write_keys_file(md_commit *commit, const mdb_db *db,
                   const char *ident_bn_prefix, const char *key_bn_child_name,
                   const char *keys_bn_iter_fmt, const char *key_file_name,
                   const char *key_dir_name, tbool use_home_dir);

/**
 * Write some number of specified keys out to a file.
 * \param commit
 * \param db the config database holding the bindings
 * \param username specific user
 * \param ident_bn_prefix binding to iterate to get list of identities
 * \param key_bn_child_name child binding name of desired key
 * \param keys_bn_iter_fmt binding format string to iterate if more than
 *       one key is being written
 * \param key_file_name name of key file to write
 * \param key_dir_name possible directory to write key file to
 * \param use_home_dir write key file to .ssh directory of a user's home
 *       directory
 */
int md_write_user_key_file(md_commit *commit, const mdb_db *db,
                           const char *username,
                           const char *ident_bn_prefix,
                           const char *key_bn_child_name,
                           const char *keys_bn_iter_fmt,
                           const char *key_file_name,
                           const char *key_dir_name, tbool use_home_dir);

/**
 * Remove certain key files for specified user
 * \param commit
 * \param db the config database holding the bindings
 * \param username specific user
 */
int md_remove_user_key_files(md_commit *commit, const mdb_db *db,
                             const char *username);

/**
 * Return an array of lines from the output of a kernel module scan
 * \param ret_module_lines
 */
int md_do_module_scan(tstr_array **ret_module_lines);

/**
 * Argument passed to md_commit_set_completion_state_action() to allow
 * file transfer options to be completed.
 */
typedef struct md_ftrans_finalize_state {
    lc_launch_result *mffs_launch_result;
    lft_state *mffs_ft_state;
    tstring *mffs_oper_id;
    tbool mffs_begun_oper;
    tstring *mffs_target_path;
    tstring *mffs_move_to_dir;
    tbool mffs_overwrite;
} md_ftrans_finalize_state;

/**
 * Read the manufacturing database from disk.  If any of the specified
 * nodes occur there, copy them into the specified locations in the
 * provided database.  After num_nodes, provide a pair of node names
 * for each node you want to copy.  The first name is where to get
 * the value from the mfdb; the second name is where to put it in
 * the database.  If the first name is NULL, we calculate it by
 * prepending "/mfg/mfdb" to the second name.  The second name 
 * may not be NULL.
 *
 * XXX/EMT: currently patterns are not supported here, only exact
 * node names.
 */
int md_mfg_copy_nodes(md_commit *commit, mdb_db *db, uint32 num_nodes,
                      const char *mfg_node_name_1, 
                      const char *db_node_name_1, ...);

/**
 * Like md_mfg_copy_nodes(), except it copies an entire subtree.
 * The subtree_root parameter takes the name of a node in the main db
 * (i.e. without the "/mfg/mfdb" prefix).  It queries the entire
 * corresponding subtree in the mfdb, and then writes all of the
 * nodes it gets into the main db.
 */
int md_mfg_copy_subtree(md_commit *commit, mdb_db *db, 
                        const char *subtree_root);

/**
 * Tell whether any nodes underneath the specified subtree roots
 * have changed.  This is mainly useful to mgmtd modules which 
 * have registered with modrf_all_changes because they need to
 * see some changes outside their own tree, but which do not want
 * to apply their configuration unless something in one of the trees
 * they care about has changed (bug 10739).
 *
 * NOTE: modules using modrf_all_changes, and calling this API,
 * may wish to consider using mdm_set_interest_subtrees() instead,
 * to limit when they are called to only the cases they care about.
 *
 * Returns true in ret_changes if any nodes have changed underneath
 * any of the specified trees, or false if there have not.
 *
 * This is more expensive than we'd like, but it is often cheaper than
 * actually applying the config again.
 */
int md_find_changes(tbool *ret_changes, const mdb_db_change_array *change_list,
                    uint32 num_subtrees, const char *subtree_root_1, ...);

/**
 * @name Monitoring helpers
 *
 * These functions are meant to be used in the case where you have a
 * monitoring wildcard which wishes to mimic a configuration wildcard.
 * That is, you always want the set of instances to be returned to
 * match the current set of instances of a configuration wildcard
 * elsewhere in the tree.
 *
 * Assign these functions directly to the iterate and get callback
 * function pointers in your node registration for the monitoring node
 * in question.  For the corresponding void * arguments, pass a string
 * indicating the name of the parent of the configuration wildcard you
 * want to be used.  e.g. to make "/foo/state/ *" mimic
 * "/foo/config/ *", pass "/foo/config" as the passback argument for
 * both the iterate and the get functions.
 */
/*@{*/

/**
 * This is a function of type ::mdm_mon_iterate_func.
 * Assign it to md_reg_node::mrn_mon_iterate_func, and the name of
 * the config wildcard's parent to md_reg_node::mrn_mon_iterate_arg.
 */
int md_mon_iterate_mirror_config(md_commit *commit,
                                 const mdb_db *db, 
                                 const char *parent_node_name,
                                 const uint32_array *node_attrib_ids,
                                 int32 max_num_nodes, int32 start_child_num,
                                 const char *get_next_child,
                                 mdm_mon_iterate_names_cb_func iterate_cb,
                                 void *iterate_cb_arg, void *arg);

/**
 * This is a function of type ::mdm_mon_get_func.
 * Assign it to md_reg_node::mrn_mon_get_func, and the name of
 * the config wildcard's parent to md_reg_node::mrn_mon_get_arg.
 */
int md_mon_get_mirror_config(md_commit *commit, const mdb_db *db,
                             const char *node_name,
                             const bn_attrib_array *node_attribs,
                             bn_binding **ret_binding,
                             uint32 *ret_node_flags, void *arg);

/**
 * This is a function of type ::mdm_mon_get_func.  It does nothing,
 * and returns nothing.  It can be useful for dummy nodes (e.g. ones
 * created only to assign ACLs to be inherited by a subtree of nodes),
 * since you are required to provide a callback function.  By
 * convention, such nodes are typically declared to be of type
 * bt_none, though it doesn't actually matter since we'll never try to
 * construct a binding of that type.
 */
int md_mon_get_nothing(md_commit *commit, const mdb_db *db,
                       const char *node_name,
                       const bn_attrib_array *node_attribs,
                       bn_binding **ret_binding,
                       uint32 *ret_node_flags, void *arg);

/*@}*/

/**
 * This function can be used as a registration tree's capability
 * check routine. Capabilities are not always enforced. This is
 * controlled by a field in 'mdm_gid_capabilities' as to whether a
 * particular user will have capailites checked or not. The boolean
 * 'subtree_check' determines whether a node's capability mask or
 * its subtree capability mask is used for the check.
 */
int md_capability_check_handler(md_commit *commit,
                                const mdb_node_reg *reg_node,
                                uint32 cap_mask,
                                tbool subtree_check,
                                tbool *ret_cap_ok);

/**
 * Get the domain name for the host for use with outgoing email.
 * The return value is chosen according to the following rules:
 *
 *   1. If /email/client/rewrite_domain is non-empty, use that.
 *
 *   2. Otherwise, if there are any instances of 
 *      "/resolver/domain_search/ * /domainname"
 *      then use the first one of those.
 *
 *   3. Otherwise, use "localdomain".
 */
int md_utils_get_domain_name(md_commit *commit, const mdb_db *db,
                             tstring **ret_domain);

/**
 * This is a per-node check function, intended to be assigned to the 
 * mrn_check_node_func field of a node registration.  The node must
 * be of type bt_string, and intended to contain an email address.
 * This function applies some heuristics to determine if the string
 * is a valid email address, according to RFC 2822 section 3.4.
 *
 * NOTE: if you add this as a new check function, be sure to use
 * md_utils_email_addr_fix() below, to ensure that all addresses
 * already in the database are fixed to comply.  Otherwise, this would
 * cause the initial commit to fail when loading the new image, and
 * the user would not have an opportunity to fix it!
 *
 * XXX/EMT: the heuristics are trivial right now: we just require
 * that it contain an '@' character, and have at least one character
 * on either side of it.  This should be fleshed out...
 */
int md_utils_email_address_check(md_commit *commit, 
                                 const mdb_db *old_db,
                                 const mdb_db *new_db, 
                                 const mdb_db_change_array *change_list,
                                 const tstring *node_name,
                                 const tstr_array *node_name_parts,
                                 mdb_db_change_type change_type,
                                 const bn_attrib_array *old_attribs,
                                 const bn_attrib_array *new_attribs,
                                 void *arg);

/**
 * Make sure all nodes matching the specified pattern contain valid
 * email addresses.  If any are found that do not, modify them to make
 * them valid.
 *
 * name_follows_value indicates whether the last part of the name will
 * have to be updated when the value is.  This will only be the case
 * if the nodes being updated are wildcard instances.
 */
int md_utils_email_address_fix(md_commit *commit, mdb_db *db, 
                               const char *pattern, 
                               tbool name_follows_value);

/**
 * This is a per-node check function, intended to be assigned to the
 * mrn_check_node_func field of a node registration.  The node must be
 * of a type that can be rendered to a string (like bt_string or
 * bt_hostname), and intended to contain an IPv4 address.
 *
 * Note see also md_utils_ipv6_check() with ::miscf_hostname_inetaddr .
 */
int md_utils_ipv4addr_check(md_commit *commit, 
                            const mdb_db *old_db,
                            const mdb_db *new_db, 
                            const mdb_db_change_array *change_list,
                            const tstring *node_name,
                            const tstr_array *node_name_parts,
                            mdb_db_change_type change_type,
                            const bn_attrib_array *old_attribs,
                            const bn_attrib_array *new_attribs,
                            void *arg);

/**
 * Flags for md_utils_ipv6_check() .
 */
typedef enum {
    /**
     * Use the default check options.
     */
    miscf_none = 0,

    /**
     * If IPv6 configuration is unsupported, allow it anyway, although
     * with an advisory message.
     */
    miscf_allow_unsupported = 1 << 0,

    /**
     * This flag may only be set on nodes of type bt_hostname.
     *
     * If set, the node's value will be constrained to be an IPv4 and/or
     * IPv6 address (an inetaddr).  The value will be constrained to be
     * an IPv4 addesss if PROD_FEATURE_IPV6 is off or if IPv6
     * configuration is administratively disabled.
     *
     * Without this flag, bt_hostnames can be a hostname, IPv4 or IPv6
     * address (no constraints).  The value will be constrained to be a
     * hostname or IPv4 address (but not an IPv6 address) if
     * PROD_FEATURE_IPV6 is off or if IPv6 configuration is
     * administratively disabled.
     */
    miscf_hostname_inetaddr = 1 << 1,

    /**
     * This flag may only be set on nodes of type bt_hostname.
     *
     * If set, the node's value will be constrained to be an IPv4 and/or
     * IPv6 address with optional zone id (an inetaddrz).  Except
     * for allowing link local addresses with a zone id, it is the
     * same as ::miscf_hostname_inetaddr .
     */
    miscf_hostname_inetaddrz = 1 << 2,

} md_utils_ipv6_check_flags;

/* Bit field of ::md_utils_ipv6_check_flags ORed together */
typedef uint32 md_utils_ipv6_check_flags_bf;

/**
 * Per-Node Check Function for IPv6 Internet Addresses
 *
 * Per-node check functions are intended to be assigned to the
 * mrn_check_node_func field of a node registration.
 *
 * The node must be capable of containing an Internet address or address
 * prefix that is intended to be IPv6 capable, containing either an IPv4
 * or an IPv6 type.  In particular, the following node types are
 * supported:
 *
 *      bt_inetaddr
 *      bt_inetaddrz
 *      bt_inetprefix
 *      bt_ipv6addr
 *      bt_ipv6addrz
 *      bt_ipv6prefix
 *      bt_hostname
 *
 * By default, this function will reject IPv6 addresses either if IPv6
 * configuration is not supported at the feature level
 * (PROD_FEATURE_IPV6), or if IPv6 configuration is administratively
 * prohibited.
 *
 * If IPv6 is only operationally unavailable, such as through lack of
 * kernel support for IPv6, IPv6 configuration will still be accepted,
 * and it is incumbent upon the apply phase to deal with such cases as
 * needed, potentially by using md_proto_ipv6_supported().
 *
 * If needed, unsupported IPv6 configuration can be allowed even though
 * the IPv6 feature is disabled, by having ::miscf_allow_unsupported as
 * a flag in the 'arg' flags parameter at node registration time.
 * However, an advisory message will still be issued to indicate that
 * the configuration will not be effective.  Please see
 * ::md_utils_ipv6_check_flags for additional options.
 *
 * Configuration which is administratively prohibited cannot be made to
 * pass, as prohibition is absolute.
 *
 * When the IPv6 feature is not enabled, error messages for the inet and
 * hostname types that accommodates both IPv4 and IPv6 will be
 * constrained to use the message normally used for the corresponding
 * IPv4 type.  These messages are generated through calls to
 * md_utils_get_type_specific_error() using the constrained type
 * (e.g. bt_ipv4addr for bt_inetaddr).  If custom messages are supported
 * from this function at MD_UTILS_INC_GRAFT_POINT 2, messages should be
 * similarly customized for all IPv4 counterparts of the IPv6 supported
 * types listed above.
 *
 * Note: this check does not limit the deletion of an IPv6 node.
 * However bear in mind that cases where IP addresses reside under
 * wildcard nodes that shift position may appear as modifications rather
 * than deletions.  Deletion of pre-existing IPv6 configuration would
 * normally be handled on "upgrade", prior removing the IPv6 feature and
 * it is required prior to enabling prohibition.
 */

int md_utils_ipv6_check(md_commit *commit,
                        const mdb_db *old_db,
                        const mdb_db *new_db,
                        const mdb_db_change_array *change_list,
                        const tstring *node_name,
                        const tstr_array *node_name_parts,
                        mdb_db_change_type change_type,
                        const bn_attrib_array *old_attribs,
                        const bn_attrib_array *new_attribs,
                        void *arg);

/**
 * Per-Node Check function for bt_hostname nodes.
 *
 * Constrain the value of the hostname according to the original
 * pre-IPv6 (before Eucalyptus Update 5) bt_hostname validation rules.
 * This means no ':', '%', or '_' characters: only alphanumeric, '.',
 * and '-', and not starting with '-'.
 *
 * Note for developers adding this check function to a node which was
 * previously unconstrained: you'll probably need upgrade logic for
 * your nodes, lest they fail the initial commit if they contained
 * characters which are now forbidden.  The common approach is an
 * upgrade rule like the following (add your own mur_name field):
 *
 *    rule->mur_upgrade_type =        MUTT_RESTRICT_CHARS;
 *    rule->mur_allowed_chars =       CLIST_HOSTNAME_NOIPV6;
 *    rule->mur_replace_char =        '.';
 *
 * XXX/EMT: bug 13697: this may be extended later to offer option
 * flags to select different sets of acceptable values.
 */

int md_utils_hostname_check(md_commit *commit,
                            const mdb_db *old_db,
                            const mdb_db *new_db,
                            const mdb_db_change_array *change_list,
                            const tstring *node_name,
                            const tstr_array *node_name_parts,
                            mdb_db_change_type change_type,
                            const bn_attrib_array *old_attribs,
                            const bn_attrib_array *new_attribs,
                            void *arg);


/**
 * Check whether the configuration of IPv6 addresses is administratively
 * prohibited.  The feature may be enabled and supported on the
 * platform, but the configuration of IPv6 may be intentionally
 * prohibited at runtime.
 *
 * This may be to ensure interoperability with platforms that are not
 * ready to support IPv6, or for other administrative reasons that the
 * feature should not be used at present.
 *
 * Note that this prohibition of configuration cannot become enabled
 * before all existing IPv6 configuration is removed from the database.
 */
tbool md_proto_ipv6_config_prohibited(void);

/* ------------------------------------------------------------------------- */
/**
 * This is a per-node side-effects function which can be used to
 * synchronize an older (legacy) IPv4 config node with a newer config
 * node which may contain IPv4 or IPv6 addresses.  The nodes to
 * synchronize must both be literals (not wildcards).  The
 * synchronization is bi-directional, such that if only one of the two
 * nodes is changed, the other is synchronized to its value if possible.
 * This allows older code or clients to still set and get the legacy
 * config node in pure IPv4 environments.  When an ipv4addr and an
 * inetaddr are synchronized, if an IPv4 address is set into the
 * inetaddr, the ipv4addr is also set to the same IPv4 address.  If an
 * IPv6 address is set into an inetaddr, the ipv4addr is set to
 * "0.0.0.0" .  If both nodes are changed, the type which can hold a
 * superset of the other type is the winner.
 *
 * This function can synchronize :
 *
 *  Type             "Winner" type if both are changed
 *----------------------------------------------------
 *  ipv4addr    <--> inetaddr inetaddrz hostname
 *  ipv4prefix  <--> inetprefix
 *  ipv6addr    <--> inetaddr inetaddrz hostname
 *  ipv6prefix  <--> inetprefix
 *
 * To use, for each node set mrn_side_effects_node_func to this function
 * name.  Then for each node, list the full registration name of the
 * other node as the mrn_side_effects_node_arg .  Note that this can
 * contain stars (*'s) which will be filled in automatically at run
 * time.
 *
 * Note that if you have additional side effects processing, it must be
 * done on the "larger" node, for example the bt_inetaddr of the
 * bt_ipv4addr / bt_inetaddr pair.  All check function calls should only
 * look at the new (inetaddr) node.  Any future upgrade functions writen
 * should only look at the "larger" node.
 *
 * Note that if possible you should change all your clients to only
 * query and set the new (inetaddr) node.
 *
 * If you are adding a sync'd node, you still must add an upgrade rule,
 * and bump the module version number.  You may want to use a MUTT_ADD
 * rule, with mur_copy_from_node giving the name of the old node.
 *
 * Finally, you should ensure that the initial values of the
 * synchronized nodes start synchronized.
 *
 */
int
md_utils_sync_inet_side_effects(md_commit *commit,
                      const mdb_db *old_db,
                      mdb_db *inout_new_db,
                      const mdb_db_change_array *change_list,
                      uint32 change_list_idx,
                      const tstring *node_name,
                      const tstr_array *node_name_parts,
                      mdb_db_change_type change_type,
                      const bn_attrib_array *old_attribs,
                      const bn_attrib_array *new_attribs,
                      const char *reg_name,
                      const tstr_array *reg_name_parts,
                      bn_type reg_type,
                      const bn_attrib_array *reg_initial_attribs,
                      void *arg);


/**
 * Construct an operation ID to track the progress of an operation
 * initiated by a mgmtd request.  This follows our convention of using
 * the peer name, a hyphen '-', and the request message ID, as the
 * operation ID.
 */
int md_utils_construct_progress_oper_id(md_commit *commit,
                                        tstring **ret_oper_id);

typedef enum {
    mupa_none = 0,
    mupa_begin_oper,
    mupa_begin_step,
    mupa_update_oper,
    mupa_end_step,
    mupa_end_oper,
} md_utils_progress_action_type;

/**
 * Helper function for performing progress-tracking actions from
 * within a mgmtd module.  Takes only the most common subset of
 * parameters which one might want to send to actions.  Most suitable
 * for beginning and ending operations or steps; not for updating
 * steps.
 *
 * \param which_action Select which mgmtd action node to invoke (mandatory).
 *
 * \param oper_id Operation ID to provide in oper_id parameter (mandatory).
 *
 * \param oper_descr Operation type to provide in descr parameter.
 * Optional; pass NULL otherwise.  Only applicable when beginning an 
 * operation.
 *
 * \param oper_type Operation type to provide in oper_type parameter.
 * Optional; pass NULL otherwise.  Only applicable when beginning an 
 * operation.
 *
 * \param num_steps Number of steps to provide in num_steps parameter.
 * Optional; pass 0 otherwise (since this is not a valid number of steps).
 * Only applicable when beginning an operation.
 *
 * \param step_descr Step description to provide in step_descr parameter.
 * Optional; pass NULL otherwise.  Only applicable when beginning a step.
 *
 * \param response_code Response code to provide in response_code parameter.
 * Optional; pass -1 otherwise.  Only applicable when ending an operation;
 * values from other callers will be ignored.
 * 
 * \param response_msg Response message to provide in response_msg parameter.
 * Optional; pass NULL otherwise.  Only applicable when ending an operation.
 */
int md_utils_progress_action(md_utils_progress_action_type which_action,
                             const char *oper_id, const char *oper_descr,
                             const char *oper_type,
                             uint32 num_steps, const char *step_descr,
                             int64 response_code, const char *response_msg);

/**
 * A wrapper around md_commit_set_return_status_msg(), which also
 * reports the error to the progress tracking mechanism if a progress
 * operation ID is provided.
 */
int md_utils_set_return_status_msg_progress(md_commit *commit,
                                            const char *oper_id,
                                            uint32 code, const char *msg);


/**
 * Return a generic error message explaining that a value provided
 * was of the wrong type, and saying how to form a correct example
 * of this type.
 */
int md_utils_get_type_specific_error(bn_type btype, uint32 btype_flags,
                                     const char **ret_msg);

extern lc_child_completion_handle *md_cmpl_hand;

/**
 * Given a node pattern, remove all the CRs ('\\r') and LFs ('\\n')
 * from it. The function iterates through all entries matching the
 * node_pattern and modifies them if necessary.
 *
 * Useful when some one is copy pasting keys into CLI.
 *
 * \param commit
 * \param db
 * \param change_list
 * \param node_pattern Node name, can have wildcards (single * only).
 *
 * Normally would be called from *_commit_side_effects() function.
 */
int md_utils_iterate_remove_crlf(md_commit *commit, mdb_db *db,
                   mdb_db_change_array *change_list,
                   const char *node_pattern);

/**
 * Flags to be used with md_utils_iptables_append_rules().
 */
typedef enum {
    miaf_none = 0,

    /**
     * By default, if a rule we were asked to add already exists in
     * the database (disregarding the comment for matching purposes),
     * we will skip adding it.  If this flag is specified, we will add
     * it anyway.
     */
    miaf_keep_duplicates = 1 << 0,

} md_iptables_append_flags;

/** A bit field of ::md_iptables_append_flags ORed together */
typedef uint32 md_iptables_append_flags_bf;

/**
 * Given a set of rules, and optionally a set of default values for
 * each new rule, append these rules to the database.
 *
 * This function works with both rules for IPv4 packet filtering
 * (/iptables) and IPv6 packet filtering (/ip6tables).  However, the
 * two types of rules may not be mixed!  The 'rules' and 'defaults'
 * arrays provided must (a) all start with /iptables, or (b) all start
 * with /ip6tables.  Any violations of this will result in an error,
 * with no action taken.
 *
 * \param commit Commit structure.  Pass NULL if calling from an upgrade
 * function.
 *
 * \param db Database to operate on.
 *
 * \param rules Rules to add.  This should contain one or more
 * instances of "/ip[6]tables/config/table/___/chain/___/rule/___", 
 * (the first binding name part may be "iptables" or "ip6tables", but
 * must match across all bindings) and
 * children underneath.  The table and chain specified will be
 * respected.  The rule number exists only to uniquify rules within the
 * array passed -- by convention they should start at 0 and move upwards.
 * These rule numbers will be replaced with the rule numbers we choose:
 * the one(s) immediately following the highest used rule number.
 * Note that only the values you want changed from their defaults need
 * to be specified here, even if you are calling from an upgrade
 * function.  (See 'defaults' parameter below.)
 *
 * \param num_rule_nodes Number of nodes in the 'rules' parameter.
 * Use sizeof() to calculate this automatically, to avoid errors.
 *
 * \param defaults Default values to set for each rule added, if any.
 * If you are calling this from an initial values function (or basically
 * anywhere EXCEPT an upgrade function), you will usually want to pass NULL
 * here, since the defaults are automatically filled in for you.  But if you
 * are calling from an upgrade function, no implicit sets are made, so
 * we must explicitly set every child node of every rule created.  Before
 * each requested rule is added, the nodes in this defaults array are
 * set for that rule.  The names here should take the form of registration
 * nodes, with three wildcards (table, chain, and rule number) which we will
 * fill in accordingly.  This prevents you from having to specify the same
 * defaults for every rule you want to add, and makes it easier to keep your
 * specific customizations separate from the defaults.
 *
 * \param num_default_nodes Number of nodes in the 'defaults'
 * parameter, or 0 if it is NULL.  Use sizeof() to calculate this
 * automatically, to avoid errors.
 *
 * \param flags Option flags: bit field of ::md_iptables_append_flags.
 */
int md_utils_iptables_append_rules(md_commit *commit, mdb_db *db,
                                   const bn_str_value rules[],
                                   uint32 num_rule_nodes,
                                   const bn_str_value defaults[],
                                   uint32 num_default_nodes,
                                   md_iptables_append_flags_bf flags);


/* ------------------------------------------------------------------------- */
/** Option flags for md_utils_constrain_hostname(). 
 */
typedef enum {

    much_none = 0,

    /**
     * The node name provided is a node name pattern, i.e. it has one
     * or more '*' characters in it, and the function is expected to
     * operate on all nodes matching this pattern.
     */
    much_pattern = 1 << 0,

    /**
     * The node to be fixed is a wildcard instance, so it will have to
     * be renamed rather than just have its value modified.  Usually
     * this would be used in conjunction with ::much_pattern, to fix
     * all instances of a given wildcard; but it could also be used
     * alone to fix a single instance.
     *
     * Note that this is NOT to be used on literals underneath wildcards,
     * like "/a/ * /b".  This should only be used if the last binding
     * name part of the matching registration is a "*".
     */
    much_wildcard = 1 << 1,

    /**
     * Only log node changes we make at DEBUG, e.g. because the values
     * might be security-sensitive.  By default, we log at NOTICE.
     */
    much_log_debug = 1 << 2,

} md_utils_constrain_hostname_flags;

/** Bit field of ::md_utils_constrain_hostname_flags ORed together */
typedef uint32 md_utils_constrain_hostname_flags_bf;


/* ------------------------------------------------------------------------- */
/** Check if a configuration node (or a set of configuration nodes) match
 * certain constraints.  If they don't, modify the database to make them
 * meet these constraints.  By default, the constraints enforced are the
 * bt_hostname validation rules in pre-Eucalyptus Update 4 releases:
 *   - Every character must be alphanumeric, '.', or '-'.
 *   - The first character may not be '-'.
 *
 * This is mainly intended to be used either from upgrade or side
 * effects.
 *
 * Note that if any of the nodes to be operated on are not of type
 * bt_hostname, this will produce an error.
 *
 * Note that the old and new values will be logged at NOTICE by default.
 * Use ::much_log_debug to lower to DEBUG.
 *
 * \param commit Commit structure.  Pass NULL if calling from an upgrade
 * function.
 *
 * \param db Database to operate on.
 *
 * \param node_name The name of the node to operate on.  Or if the 
 * ::much_pattern option flag is specified, this may be a pattern, and
 * all matching nodes will be operated on.
 *
 * \param node_descr A simple description of the node to be constrained,
 * for logging purposes only.  This will show up along the old and new
 * values in the logs if the node has to be altered.  This does NOT
 * support any patterns or parameter substitution; it is just used as
 * a string literal.  If NULL is passed, the default description string
 * to use is "value", which will not give the user much additional context
 * to know what sort of thing is being talked about.  (XXX/EMT: it would
 * be nice to parameterize, to be more specific in the case of patterns)
 *
 * \param flags Option flags.
 */
int md_utils_constrain_hostname(md_commit *commit, mdb_db *db, 
                                const char *node_name, 
                                const char *node_descr,
                                md_utils_constrain_hostname_flags_bf flags);


/**
 * Given a db change record pertaining to a wildcard node, or a
 * literal underneath a wildcard, return a textual description of the
 * object being modified.  If it was a literal underneath the wildcard,
 * do not include a description of the particular field being modified.
 *
 * This function does not apply to pure literal nodes, and will return
 * NULL for the description if called for such a node.
 *
 * \param commit Commit structure.
 *
 * \param change The change record whose object to describe.
 *
 * \param wild_reg The registration structure for the wildcard node.
 * If you don't have it, pass NULL and it will be looked up for you.
 * This is solely for efficiency, when the infrastructure already
 * has the information and we want to save another lookup.
 *
 * \retval ret_obj_descr The object description.
 */
int md_utils_audit_get_obj_descr(md_commit *commit, 
                                 const mdb_db_change *change, 
                                 const mdb_node_reg *wild_reg, 
                                 tstring **ret_obj_descr);

/**
 * Truncate a value so that it can be displayed in the logs without consuming
 * excessive space (e.g. so as not to crowd out other data, nor be arbitrarily
 * truncated by syslog's 1024 char line max without any indicaton).
 *
 * The truncated value will be displayed with elipses followed by a truncation
 * notice string (subject to internationalization), e.g.:
 *
 * "this is a very long string that's needs to be shortened... (truncated)"
 *
 * \param val The tstring to be truncated
 *
 * \param max_len The maximum length after truncation.  Must be at least 32 to
 * allow ample room for the truncation text, which might be subject to change,
 * but will never be more 32 characters, so 32 or greater must be used.
 */
int md_utils_log_truncate_value(tstring *val, uint32 max_len);


/* ------------------------------------------------------------------------- */
/** Log a message to /var/log/systemlog.  This is a special log file which
 * is never rotated, compressed, edited, or deleted.  Unless someone goes
 * into the shell or otherwise bypasses our normal mechanisms, everything
 * ever logged here will always remain, even after a "scrub".  As such,
 * of course it must be very low-volume; we only log very important,
 * and hopefully infrequent, stuff here.
 *
 * Do not use the I18N macro _() or any of its variants on messages
 * passed to this function.  The messages in the systemlog are always
 * in English.
 *
 * Unlike syslog, the systemlog is not managed by a separate daemon.
 * It is directly accessed only by firstboot.sh and by mgmtd.
 * Since these never run concurrently, no locking is used.
 *
 * \param external A flag telling whether this log message came from a
 * source outside mgmtd.  This triggers a special notation in the log
 * message, since we don't trust the process name, pid, and timestamp
 * as much that way.
 *
 * \param process_name The name of the process generating the log message.
 * This should be the same string as the process passes to lc_log_init()
 * for use by syslog.
 *
 * \param pid The pid of the process generating the log message.
 *
 * \param descr A description of the origin of the message.  This is an
 * alternative to providing the process_name and pid parameters, and
 * is mutually exclusive with them.  Either they should be NULL and -1,
 * or this should be NULL.
 *
 * \param timestamp The time at which the log message was generated,
 * or -1 to decline to state.  Usually this will only be provided from
 * externally-generated log messages, where we want to know if there
 * has been a time lag.
 *
 * \param msg The message to be logged.  This does not take a format
 * string, partly because most or all messages logged this way should
 * also be logged into syslog, so the caller will probably want to 
 * pre-render the message before calling both logging APIs.
 */
int md_systemlog_log_external(tbool external, const char *process_name, 
                              pid_t pid, const char *descr, 
                              lt_time_ms timestamp, const char *msg);


/* ------------------------------------------------------------------------- */
/** A wrapper to md_systemlog_log_external() to be used by mgmtd modules.
 */
int md_systemlog_log(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));


/* ------------------------------------------------------------------------- */
/** Determine whether or not the authorization within a commit
 * structure has a superset of the auth groups of a specified other
 * user account.
 *
 * \param commit Commit context structure, which contains authorization
 * information (auth group membership) about the user who initiated
 * the commit.
 *
 * \param db The database in which to check.
 *
 * \param my_auth_groups_sorted A sorted array of authorization groups
 * of which the user in the commit structure is a member.  Providing
 * this is optional, as it can be derived from the commit structure.
 * It is purely for efficiency, if you already have the array fetched.
 * Pass NULL if you don't already have it.
 *
 * \param username The username of an account whose auth groups to
 * compare against those in the commit.  This will usually be different
 * from the one in the commit, though it may be the same (in which case
 * the answer to the function had better be 'true').
 *
 * \retval ret_is_superset Return 'true' if the auth groups in the
 * commit are a superset of the auth groups that the username is a
 * member of; 'false' if not.  Note that we do not require a proper
 * superset, so if the sets of auth groups are exactly the same,
 * 'true' will be returned.
 */
int md_utils_acl_auth_is_superset(md_commit *commit, const mdb_db *db,
                                  const uint32_array *my_auth_groups_sorted,
                                  const char *username,
                                  tbool *ret_is_superset);


/**
 * Standard string to use for mrn_description field of a node
 * registration, for nodes which are only created for the purpose
 * of setting an ACL to be inherited by their children.
 */
#define md_acl_dummy_node_descr "Dummy node for inherited ACLs"


#ifdef PROD_TARGET_OS_LINUX

/* ------------------------------------------------------------------------- */
/**
 * Write a string value to a settings file, typically in /proc .
 *
 * \param proc_filename Which file to write to.  This must be a full path, like:
 * "/proc/sys/net/ipv4/conf/ether2/rp_filter"
 *
 * \param proc_value The value to set.
 *
 */
int md_utils_proc_set(const char *proc_filename,
                      const char *proc_value);

/**
 * Write a string value to a settings file, typically in /proc, but only
 * if the current value contained in the file is different.  Returns if
 * the value was changed by the call.  This can be useful if applying
 * kernel configuration via /proc files.  It is not an error if the file
 * cannot be read from, to determine the current value, but an error
 * will be returned if the file cannot be written to.
 *
 * \param proc_filename The file to write to.  This must be a full path, like:
 * "/proc/sys/net/ipv4/conf/ether2/rp_filter"
 *
 * \param proc_value The value to set.
 *
 * \retval ret_changed If the value of the file was changed since it was
 * different than proc_value .  May be NULL.
 */
int md_utils_proc_set_maybe_changed(const char *proc_filename,
                                    const char *proc_value,
                                    tbool *ret_changed);

/**
 * Simplified form of md_utils_proc_set_maybe_changed()
 */
int md_utils_proc_set_maybe(const char *proc_filename,
                            const char *proc_value);


/**
 * Read a string value from a settings file, typically in /proc .  On
 * failure to read, no error is returned, but ret_proc_value will be
 * NULL.  ret_proc_value is not NULL except in error cases.
 *
 * \param proc_filename Which file to read from.  This must be a full path, like:
 * "/proc/sys/net/ipv4/conf/ether2/rp_filter"
 *
 * \param ret_proc_value The value that was read.
 *
 */
int md_utils_proc_get(const char *proc_filename,
                      char **ret_proc_value);

/* ------------------------------------------------------------------------- */
/**
 * Write a string value to a sysctl setting.  This is done via
 * /proc/sys (just like sysctl itself is these days)
 *
 * \param sysctl_name The key to set.  This must use "/" as the
 * seperator, not "." , like: "/net/ipv4/conf/ether2/rp_filter"
 *
 * \param sysctl_value The value to set.
 *
 */
int md_utils_sysctl_set(const char *sysctl_name,
                        const char *sysctl_value);

/**
 * Write a string value to a sysctl setting, but only if the current
 * value of the setting is different.  Returns if the value was changed
 * by the call.  This can be useful if applying kernel configuration via
 * sysctl settings.  It is not an error if the setting cannot be read to
 * determine the current value, but an error will be returned if the
 * setting cannot be written to.  This is done via /proc/sys (just like
 * sysctl itself is these days)
 *
 * \param sysctl_name The key to set.  This must use "/" as the
 * seperator, not "." , like: "/net/ipv4/conf/ether2/rp_filter"
 *
 * \param sysctl_value The value to set.
 *
 * \retval ret_changed If the value of the key was changed since it was
 * different than sysctl_value .  May be NULL.
 */
int md_utils_sysctl_set_maybe_changed(const char *sysctl_name,
                                      const char *sysctl_value,
                                      tbool *ret_changed);

/**
 * Like md_utils_sysctl_set_maybe_changed(), but without the changed
 * information returned.
 */
int md_utils_sysctl_set_maybe(const char *sysctl_name,
                              const char *sysctl_value);


/**
 * Read a sysctl setting.  On failure to read, no error is returned, but
 * ret_sysctl_value will be NULL.  ret_sysctl_value is not NULL except
 * in error cases. This is done via /proc/sys (just like sysctl itself
 * is these days)
 *
 * \param sysctl_name The key to get.  This must use "/" as the
 * seperator, not "." , like: "/net/ipv4/conf/ether2/rp_filter"
 *
 * \param ret_sysctl_value The current value of the key, without a
 * trailing newline.
 *
 */
int md_utils_sysctl_get(const char *sysctl_name,
                        char **ret_sysctl_value);

/**
 * Like md_utils_sysctl_get(), but converts the value to a uint64.  On
 * failure to read, lc_err_io_error is returned.  If the string to
 * integer type conversion fails, lc_err_bad_type will be returned.
 */
int md_utils_sysctl_get_uint64(const char *sysctl_name,
                               uint64 *ret_sysctl_value);


#endif /* PROD_TARGET_OS_LINUX */

/**
 * Given a uid, and optionally an expected username, look up the
 * associated username under /auth/passwd/user.  The following
 * provisions apply:
 *
 * (a) If the expected_username is specified and there is a match with both
 *     that username and the uid given, that username will be returned.
 *
 * (b) Otherwise, if the uid given is 0, we will always return "admin"
 *     (the check logic in md_passwd ensures that there will always be 
 *     an "admin" account with uid 0) regardless of any other entries 
 *     with that uid.  
 *
 * (c) Otherwise, if there is any match with the given uid, the first one
 *     found will be returned.  If there are multiple matches, there are
 *     no guarantees about which one will be returned.  Note that we do
 *     this even if the expected_username is specified, and since we are
 *     not in case (a), then a match will mean the returned username will
 *     not match the expected one.
 */
int md_utils_uid_to_username(md_commit *commit, const mdb_db *db,
                             uint32 uid, const char *expected_username,
                             char **ret_username);

#ifdef __cplusplus
}
#endif

#endif /* __MD_UTILS_H_ */
