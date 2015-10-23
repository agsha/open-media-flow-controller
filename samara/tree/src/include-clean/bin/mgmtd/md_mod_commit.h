/*
 *
 * src/bin/mgmtd/md_mod_commit.h
 *
 *
 *
 */

#ifndef __MD_MOD_COMMIT_H_
#define __MD_MOD_COMMIT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "gcl.h"
#include "mdb_db.h"
#include "mdb_changes.h"
#include "tacl.h"

/**
 * \file src/bin/mgmtd/md_mod_commit.h API for use while mgmtd is
 * handling a request message (generally called a "commit" even if
 * it doesn't change the database).
 * \ingroup mgmtd
 */

/* ================================================== */
/* Module Commit Processing */
/* ================================================== */

/**
 * A function to be called during the side effects phase of mgmtd's
 * three-phase commit process.  It may make changes to the proposed
 * new database that reflect side effects of the original set of
 * proposed changes.
 *
 * Note that this function is only called if any of the following are true:
 *   (a) any of the nodes being modified fall underneath the module's
 *       configuration root as specified in its call to mdm_add_module()
 *   (b) if any of the nodes being modified fall underneath a subtree 
 *       specified in a call to mdm_set_interest_subtrees().
 *   (c) if the modrf_all_changes flag was specified with mdm_add_module().
 *
 * \param commit Commit structure reflecting the current commit state.
 *
 * \param old_db The old mgmtd database before the proposed change.
 * 
 * \param inout_new_db The new mgmtd database, including all proposed
 * changes so far.  If you want to add side effects, use the API
 * in mdb_db.h to modify this database.
 *
 * \param change_list <b>NOTE: should be const; DO NOT MODIFY</b>
 * An array of mdb_db_change structures expressing
 * the list of proposed changes to the database.  You do not need to
 * (and should not) update this list with the changes you are making;
 * it is automatically maintained by the infrastruture.
 *
 * \param arg The argument you provided in your call to mdm_add_module()
 * to be passed back to your side effects function.
 */
typedef int (*mdm_commit_side_effects_func)(md_commit *commit, 
                                            const mdb_db *old_db, 
                                            mdb_db *inout_new_db, 
                                            mdb_db_change_array *change_list,
                                            void *arg);

/**
 * A function to be called during the check (validation) phase of
 * mgmtd's three-phase commit process.  It should inspect the list of
 * proposed changes and/or the new database, and decide if it wants to
 * accept or reject the changes.  To accept the changes, no action is
 * necessary.  To reject the changes, the module must call call
 * md_commit_set_return_status_msg() or
 * md_commit_set_return_status_msg_fmt() to specify a return status
 * code and explanation message.
 *
 * Note that this function is only called if any of the following are true:
 *   (a) any of the nodes being modified fall underneath the module's
 *       configuration root as specified in its call to mdm_add_module()
 *   (b) if any of the nodes being modified fall underneath a subtree 
 *       specified in a call to mdm_set_interest_subtrees().
 *   (c) if the modrf_all_changes flag was specified with mdm_add_module().
 *
 * \param commit Commit structure reflecting the current commit state.
 *
 * \param old_db The old mgmtd database before the proposed change.
 * You can issue queries against this using the API in mdb_db.h,
 * though in most cases validation involves checking the new_db
 * or the change_list.
 *
 * \param new_db The new mgmtd database including the proposed 
 * change.
 *
 * \param change_list <b>NOTE: should be const; DO NOT MODIFY</b>
 * An array of mdb_db_change structures expressing
 * the list of proposed changes to the database.  Note that since the
 * side effects phase comes before the check phase, this will include
 * all of the side effects as well.
 *
 * \param arg The argument you provided in your call to mdm_add_module()
 * to be passed back to your check function.
 */
typedef int (*mdm_commit_check_func)(md_commit *commit, 
                                     const mdb_db *old_db,
                                     const mdb_db *new_db, 
                                     mdb_db_change_array *change_list,
                                     void *arg);

/**
 * A function to be called during the apply phase of mgmtd's
 * three-phase commit process.  Should take whatever actions are
 * necessary to apply the changes to the system.  Note that if
 * an external daemon will act on the changes, you do not need to
 * do anything here; the daemon should register to receive
 * configuration change notification events, which are sent 
 * automatically by the infrastructure.  This function is normally
 * used to rewrite configuration files of legacy daemons, or 
 * apply configuration to kernel components through system calls,
 * or running utility programs, etc.
 *
 * Note that this function is only called if any of the following are true:
 *   (a) any of the nodes being modified fall underneath the module's
 *       configuration root as specified in its call to mdm_add_module()
 *   (b) if any of the nodes being modified fall underneath a subtree 
 *       specified in a call to mdm_set_interest_subtrees().
 *   (c) if the modrf_all_changes flag was specified with mdm_add_module().
 *
 * \param commit Commit structure reflecting the current commit state.
 *
 * \param old_db The old mgmtd database before the proposed change.
 *
 * \param new_db The new mgmtd database including the proposed 
 * change.
 *
 * \param change_list <b>NOTE: should be const; DO NOT MODIFY</b>
 * An array of mdb_db_change structures expressing
 * the list of proposed changes to the database.
 *
 * \param arg The argument you provided in your call to mdm_add_module()
 * to be passed back to your apply function.
 */
typedef int (*mdm_commit_apply_func)(md_commit *commit, const mdb_db *old_db,
                                     const mdb_db *new_db, 
                                     mdb_db_change_array *change_list,
                                     void *arg);


/**
 * A function to be called during the check (validation) phase of
 * mgmtd's three-phase commit process.  Functions of this type are
 * registered against a single node, unlike mdm_commit_check_func ,
 * which is against an entire module.  It should inspect the given node
 * value, and decide if it wants to accept or reject the change.  To
 * accept the change, no action is necessary.  To reject the change,
 * the module must call md_commit_set_return_status_msg() or
 * md_commit_set_return_status_msg_fmt() to specify a return status code
 * and explanation message.
 *
 * Note that this function is only called if the node registered against
 * is modified.  It will necessarily be called during an initial commit.
 *
 * \param commit Commit structure reflecting the current commit state.
 *
 * \param old_db The old mgmtd database before the proposed change.
 * You can issue queries against this using the API in mdb_db.h,
 * though in most cases validation involves checking the new_db
 * or the change_list.
 *
 * \param new_db The new mgmtd database including the proposed 
 * change.
 *
 * \param change_list An array of mdb_db_change structures expressing
 * the list of proposed changes to the database.  Note that since the
 * side effects phase comes before the check phase, this will include
 * all of the side effects as well.
 *
 * \param node_name The name of the changed node
 * 
 * \param node_name_parts The name array, one element per part, of the changed node
 *
 * \param change_type If this is a modify, an add or a delete
 *
 * \param old_attribs Previous value of the node in question
 *
 * \param new_attribs Proposed new value of the node in question
 * 
 * \param arg The argument set in the node registration
 * to be passed back to your check function.
 */
typedef int (*mdm_commit_check_node_func)(md_commit *commit, 
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
 * A function to be called during the side effects (validation) phase of
 * mgmtd's three-phase commit process.  Functions of this type are
 * registered against a single node, unlike mdm_commit_side_effects_func ,
 * which is against an entire module.  It may make changes to the proposed
 * new database that reflect side effects of the original set of
 * proposed changes.
 *
 * Note that this function is only called if the node registered against
 * is modified.  It will necessarily be called during an initial commit.
 *
 * \param commit Commit structure reflecting the current commit state.
 *
 * \param old_db The old mgmtd database before the proposed change.
 * You can issue queries against this using the API in mdb_db.h .
 *
 * \param inout_new_db The new mgmtd database, including all proposed
 * changes so far.  If you want to add side effects, use the API
 * in mdb_db.h to modify this database.
 *
 * \param change_list An array of mdb_db_change structures expressing
 * the list of proposed changes to the database.
 *
 * \param node_name The name of the changed node
 * 
 * \param node_name_parts The name array, one element per part, of the changed node
 *
 * \param change_type If this is a modify, an add or a delete
 *
 * \param old_attribs Previous value of the node in question
 *
 * \param new_attribs Proposed new value of the node in question
 * 
 * \param arg The argument set in the node registration
 * to be passed back to your side effects function.
 */
typedef int (*mdm_commit_side_effects_node_func)(md_commit *commit,
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
                                        bn_type reg_value_type,
                                        const bn_attrib_array *reg_initial_attribs,
                                        void *arg);


/**
 * Function to be called during a database upgrade.  (It is not
 * currently used for downgrades, which are handled automatically by
 * the infrastructure.)  It is called once for every single-number
 * jump in semantic version.  It may make changes to the database
 * as part of the upgrade.
 *
 * \param old_db Always NULL for now.  Should be the old mgmtd
 * database before any upgrades have happened.
 *
 * \param inout_new_db The current proposed database, after whatever
 * upgrades have been done so far.  You may modify this database
 * directly using the API in mdb_db.h to effect your upgrades.
 *
 * \param from_module_version The semantic version for this module
 * from which we are upgrading.
 * 
 * \param to_module_version The semantic version for this module
 * to which we are upgrading.  Since upgrades are done one step at
 * a time, this will always be from_module_version+1, even if 
 * the upgrade needed spans multiple semantic version numbers.
 *
 * \param arg The argument you provided in your call to mdm_add_module()
 * to be passed back to your upgrade function.
 */
typedef int (*mdm_upgrade_downgrade_func)(const mdb_db *old_db,
                                          mdb_db *inout_new_db,
                                          uint32 from_module_version,
                                          uint32 to_module_version,
                                          void *arg);

/**
 * A function to be called when mgmtd wants to retrieve the value of 
 * a specific monitoring node registered by your module.
 *
 * Note that this may be called for either literal or wildcard
 * nodes.  In the case of wildcard nodes, it may be called as a 
 * result of a direct get request for that node; or if a mgmtd 
 * caller has requested an iterate over the wildcard instances, 
 * it will be called once for each instance retrieved from
 * the iterate function.  The value of a wildcard node must match
 * the last part of the name (everything after the last unescaped
 * '/' character), and its type must match the registered type.
 *
 * \param commit Commit structure reflecting the current commit state.
 * A "commit" here is really just the servicing of a single request
 * to mgmtd, even if the db is not changing so we are not technically
 * "commiting" anything.
 *
 * \param db Pointer to current mgmtd database.  You can use this to
 * query other config or monitoring nodes, but cannot modify it.
 *
 * \param node_name The absolute name of the node being requested.
 *
 * \param node_attribs (Seldom used)
 *
 * \param arg The argument you specified in the
 * md_reg_node::mrn_mon_get_arg field of your node registration
 * structure, along with the pointer to this function.
 *
 * \retval ret_binding A binding representing the node queried.
 * Its name must match the node_name parameter, and its type
 * must match that registered for this node.
 *
 * \retval ret_node_flags Intended for future expansion, currently
 * unused.
 */
typedef int (*mdm_mon_get_func)(md_commit *commit, const mdb_db *db,
                                const char *node_name,
                                const bn_attrib_array *node_attribs,
                                bn_binding **ret_binding,
                                uint32 *ret_node_flags,
                                void *arg);


/* XXX still have to add in the 'lookup void *' to help save lookup costs */

/**
 * A function to be called by your internal monitoring wildcard
 * iterate function for every instance of a monitoring wildcard.
 *
 * \param commit The commit structure (just pass on the one given
 * to you).
 *
 * \param db The mgmtd database structure (just pass on the one given
 * to you).
 *
 * \param child_relative_name The name of the wildcard instance you
 * are declaring.  Note that this is just its name relative to the 
 * parent (so it must not have any unescaped '/' characters in it),
 * and is not typed; the value of this node will be provided separately
 * by your get function.
 *
 * \param iterate_cb_arg The opaque argument passed to your iterate
 * function, for the sole purpose of passing along to this function.
 */
typedef int (*mdm_mon_iterate_names_cb_func)(md_commit *commit,
                                             const mdb_db *db, 
                                             const char *child_relative_name,
                                             void *iterate_cb_arg);

/**
 * Function to be called when mgmtd wants to retrieve a list of 
 * instances of an internal monitoring wildcard.  Call the
 * function pointer passed to you as iterate_cb once for every
 * instance of the wildcard.
 *
 * \param commit Commit structure reflecting the current commit state.
 * To be passed back to iterate_db_arg.
 *
 * \param db Pointer to current mgmtd database.  
 * To be passed back to iterate_db_arg.
 *
 * \param parent_node_name Name of the mgmtd node which is the parent
 * of the wildcard to iterate over.  For example, if this is the function
 * registered for "/a/b/ *", then this parameter would contain "/a/b".
 *
 * \param node_attrib_ids (Seldom used)
 * 
 * \param max_num_nodes Unused.  Allows room for future expansion where
 * mgmtd could do a partial iterate by telling you the maximum number
 * of instances it wanted to receive.
 *
 * \param start_child_num Unused.  Allows room for future expansion where
 * mgmtd could do a partial iterate by telling you the index of the 
 * child to start returning (i.e. skip the first n children).
 *
 * \param get_next_child Unused.  (Allows room for future expansion where
 * mgmtd could do a partial iterate by telling you the name of the 
 * child to start immediately after (i.e. skip all children before and
 * including the one with this name).  Decide whether would this mean to
 * get just the next one, or up to max_num_nodes after it.)
 *
 * \param iterate_cb A mgmtd infrastructure function which you should call
 * once for every instance of the wildcard requested.  With each call,
 * provide the name of an instance.
 *
 * \param iterate_cb_arg An opaque argument that you should pass back
 * to iterate_cb.
 *
 * \param arg The argument you specified in the
 * md_reg_node::mrn_mon_iterate_arg field of your node registration
 * structure, along with the pointer to this function.
 */
typedef int (*mdm_mon_iterate_func)(md_commit *commit,
                                    const mdb_db *db,
                                    const char *parent_node_name,
                                    const uint32_array *node_attrib_ids,
                                    int32 max_num_nodes,
                                    int32 start_child_num,
                                    const char *get_next_child,
                                    mdm_mon_iterate_names_cb_func iterate_cb,
                                    void *iterate_cb_arg,
                                    void *arg);

/**
 * A function to be called to handle an action request within a mgmtd
 * module.  The action must complete synchronously, and not modify
 * the database.
 *
 * To pass an action request on to a daemon for handling there, use
 * md_commit_forward_action().  If you want to do something of your
 * own before or after forwarding the request, register a
 * mdm_action_std_func anyway and call md_commit_forward_action()
 * yourself.  If not, just use md_commit_forward_action() as your
 * action handler function.  If you do this, make sure to provide a
 * pointer to a md_commit_forward_action_args structure as the 
 * argument to be passed back to the action function.
 *
 * \param commit Commit structure reflecting the current commit state.
 *
 * \param db Current mgmtd database.  You may use this to make queries,
 * but you may not modify the database.  If you need to write an 
 * action function that modifies the database, set
 * md_reg_node::mrn_action_config_func instead of
 * md_reg_node::mrn_action_func.
 *
 * \param action_name The name of the action that has been requested.
 *
 * \param params The bindings contained in the action request.
 *
 * \param arg The argument you specified in the
 * md_reg_node::mrn_action_arg to be passed back to your action
 * function.
 */
typedef int (*mdm_action_std_func)(md_commit *commit, const mdb_db *db,
                                   const char *action_name, 
                                   bn_binding_array *params, void *cb_arg);

/**
 * A function to be called to handle an action request within a mgmtd
 * module.  The module may complete the action request asynchronously,
 * but may not modify the database.
 */
typedef int (*mdm_action_std_async_start_func)(md_commit *commit, 
                                               const mdb_db *db,
                                               const char *action_name, 
                                               bn_binding_array *params,
                                               void *cb_arg);

typedef int (*mdm_action_std_async_completion_proc_func)(md_commit *commit, 
                                                    const mdb_db *db,
                                                    const char *action_name, 
                                                    void *cb_arg,
                                                    void *finalize_arg,
                                                    pid_t pid, 
                                                    int wait_status,
                                                    tbool completed);

/**
 * A function to be called to handle an action request within a mgmtd
 * module.  The module can forward the action to an external process, and
 * return requesting asynchronous completion, without causing a barrier to
 * message processing either before or after itself.  The callback may not
 * modify the database.  This is useful if your action's execution cannot
 * possibly depend on the execution of other in flight queries and actions,
 * and similarly future queries and actions do not have any order
 * dependency with respect to this action.
 */
typedef int (*mdm_action_std_async_nonbarrier_start_func)(md_commit *commit, 
                                              const mdb_db *db,
                                              const char *action_name, 
                                              bn_binding_array *params,
                                              void *cb_arg);

/**
 * A function to be called to handle an action request within a mgmtd
 * module.  The module may modify the database by constructing a 
 * set request and calling md_commit_handle_commit_from_module().
 * The action must complete synchronously.
 */
typedef int (*mdm_action_modify_func)(md_commit *commit, mdb_db **db,
                                      const char *action_name, 
                                      bn_binding_array *params, void *cb_arg);


typedef int (*mdm_event_criteria_compare_func)(md_commit *commit, 
                                               const mdb_db *db,
                                               gcl_session *sess,
                                               const char *event_name, 
                                               const bn_binding_array 
                                               *event_bindings, 
                                               const bn_binding_array 
                                               *match_criteria,
                                               void *cb_arg,
                                               tbool *ret_interest);

typedef int (*mdm_event_std_async_completion_proc_func)(md_commit *commit, 
                                                    const mdb_db *db,
                                                    const char *event_name, 
                                                    void *cb_arg,
                                                    void *finalize_arg,
                                                    pid_t pid, 
                                                    int wait_status,
                                                    tbool completed);


typedef int (*mdm_initial_db_func)(md_commit *commit, mdb_db *db, void *arg);

int md_commit_find_session(const md_commit *commit, const char *remote_name,
                           gcl_session **ret_sess);


/**
 * @name Return status 
 *
 * This group of functions is used for mgmtd modules processing a commit
 * to tell the infrastructure what return code (error code) and message
 * they want to return in the pending response message.
 */

/*@{*/

/**
 * Set the return status code, without specifying a message.
 *
 * NOTE: your return code is ORed with any existing return code.
 * During the 'check' phase, this usually isn't an issue because the
 * commit is halted after the first error anyway.  But if multiple
 * modules set error codes during the apply phase, where these are
 * non-fatal, they'll get combined.  So a client trying to interpret
 * this value (as a specific error code, more specifically than just
 * it being nonzero) may get confused, unless you are using a
 * different bit for every error you might report.
 */
int md_commit_set_return_status(md_commit *commit, 
                                uint16 return_code);

/**
 * Get the return status code for the current phase.
 *
 * CAUTION: this will return EITHER the "commit return code" or the
 * "apply return code", depending on what phase the commit is in.
 * The former means an impending commit failure, while the latter is
 * just informational whining that does not halt the commit.  This
 * call does not allow the caller to know which of the two values 
 * they are getting.  See md_commit_get_return_status_combined()
 * for a possibly better alternative.
 */
int md_commit_get_return_status(const md_commit *commit, 
                                uint16 *ret_return_code);

/**
 * Get a value which combines the "commit return code" and
 * "apply return code" into a single number, just as a GCL response
 * message does.  The caller can then use mdc_response_code_components()
 * to break it apart into the two components if desired.
 */
int md_commit_get_return_status_combined(const md_commit *commit,
                                         uint32 *ret_return_code);

/**
 * Get the return status message for this commit.
 */
int md_commit_get_return_status_msg(const md_commit *commit, 
                                    tstring **ret_return_msg);

/**
 * Set the return status code and return message.
 */
int md_commit_set_return_status_msg(md_commit *commit, 
                                    uint16 return_code, 
                                    const char *return_message);

/**
 * Like md_commit_set_return_status_msg() except that the message is a
 * printf-style format string, followed by a variable argument list of
 * items to substitute into the format string.
 */
int md_commit_set_return_status_msg_fmt(md_commit *commit, 
                                        uint16 return_code, 
                                        const char *return_message_fmt, ...)
     __attribute__ ((format (printf, 3, 4)));

/*@}*/

/**
 * @name Return bindings
 */

/*@{*/

/*
 *  We could also want one of these that takes a uint32_array and a
 *     bn_binding_array . 
 */
int md_commit_binding_append(md_commit *commit, uint32 node_id,
                             const bn_binding *binding);
int md_commit_binding_append_takeover(md_commit *commit, uint32 node_id,
                                      bn_binding **inout_binding);

int md_commit_binding_append_single_array(md_commit *commit, uint32 node_id,
                                          const bn_binding_array *bindings);
int md_commit_binding_append_array(md_commit *commit, 
                                   const bn_binding_array *bindings,
                                   const uint32_array *node_ids);
int md_commit_binding_append_array_takeover(md_commit *commit, 
                               bn_binding_array **inout_bindings,
                               uint32_array **inout_node_ids);

/*@}*/

/**
 * Tells whether the commit currently being processed is in the 
 * context of mgmtd's "one-shot" commit or not.  The "one-shot" commit
 * mode means mgmtd has been launched, generally at boot, to simply
 * load and apply the active config database, then exit.  The normal
 * commit mode is when mgmtd is launched and intends to remain running.
 *
 * Modules should generally not care about this, but may in certain
 * cases, e.g. if they are going to launch a daemon that they expect
 * to keep running.  The network interface module will not launch any
 * DHCP clients in one-shot mode.
 */
int md_commit_get_commit_mode(const md_commit *commit, tbool *ret_one_shot);

/**
 * Tells whether the commit currently being processed is the first
 * commit from when mgmtd has been launched.  It is set both when
 * one-shot mode is set, and during the first "normal" commit during
 * normal startup.
 *
 * Modules should generally not care about this, but may in certain
 * cases, e.g. if they are trying to clean up or syncronize some state
 * held outside of mgmtd.
 */
int md_commit_get_commit_initial(const md_commit *commit, tbool *ret_initial);

/**
 * Tells whether the commit currently being processed has the "no apply"
 * flag set.  Normal commits, which include the apply phase, will have
 * this set to 'false'.  If this is set to 'true', the apply phase
 * will be skipped; that is, we are only running side effects and
 * check; and then returning from the commit without applying its
 * contents to the system.
 *
 * This mode is currently only used when an upgrade is done while
 * opening a database.  The plan is to load the database, apply any
 * necessary upgrades, and then run side effects to make sure the
 * database is internally consistent before saving the upgraded
 * version back to disk.
 */
int md_commit_get_commit_no_apply(const md_commit *commit,
                                  tbool *ret_no_apply);

/**
 * Tells whether the commit currently being processed is in the
 * context of an upgrade.
 *
 * This might be useful information, e.g. in a side effects function
 * trying to work around bug 10749.
 */
int md_commit_get_commit_is_upgrade(const md_commit *commit,
                                    tbool *ret_is_upgrade);

/* Async action completion related types and functions */
int md_commit_set_completion_proc_async(md_commit *commit, 
                                        tbool complete_async);

int md_commit_get_completion_proc_async(const md_commit *commit, 
                                        tbool *ret_complete_async);

/**
 * Specify what to do with a child process on shutdown, before calling
 * the completion handler.
 *
 * XXX/EMT: another option we could offer here would be to wait a short
 * while first, then send a SIGTERM, and then wait again.  This would take
 * care of edge cases where someone tries to shut us down right when a
 * process that doesn't actually take very long to finish was running.
 * It might be better to wait a half second, rather than killing it
 * right away.  The 'unused' parameter to the async completion reg
 * functions could be used for the pre-signal delay.
 */
typedef enum {
    mccs_none = 0,

    /** Allow the child to continue running */
    mccs_leave_running,

    /** Send the child a SIGTERM */
    mccs_signal_sigterm,

} md_commit_completion_shutdown_type;

/*
 * Amount of time we'll typically wait for a child process of ours to exit
 * during shutdown.  This is the default used by the wrappers
 * md_commit_add_completion_proc_state_action() and 
 * md_commit_add_completion_proc_state_event().  Modules may specify
 * a different number by calling the ..._full() variants directly.
 */
static const lt_dur_ms md_child_process_wait_time_ms = 2000; /* 2 seconds */

/**
 * Register for asynchronous completion of an internal action handler,
 * pending the exit of a child process we have forked.
 *
 * \param commit Commit context.
 *
 * \param pid Pid of child process we have forked.  If you have used the
 * lc_launch() API, this would come from the lr_child_pid field of the
 * lc_launch_result.
 *
 * \param wait_status In the normal case where the process has not yet 
 * exited ('done' is false), this parameter is ignored, and you should
 * pass zero.  In the edge case where the process being registered has
 * already exited, this is its wait status from waitpid() or equivalent.
 *
 * \param done Has this process exited yet?  Normally, this will be 
 * 'false'.
 *
 * \param shutdown_type What to do if this process is still running when
 * it is time to shut down.
 *
 * \param unused Reserved for future use.  Callers should pass 0.
 *
 * \param shutdown_kill_wait If shutdown_type is ::mccs_signal_sigterm, how
 * long should we wait for the child to exit after sending the SIGTERM?
 * 0 means to not wait at all, but to move on immediately.
 *
 * \param proc_descr Description of this process, for logging purposes.
 *
 * \param action_finalize_func Function to call to finalize the action,
 * once the process has exited (or we have decided to give up on it exiting).
 *
 * \param finalize_arg Argument to pass back to action_finalize_func.
 */
int md_commit_add_completion_proc_state_action_full(md_commit *commit, 
                                      pid_t pid,
                                      int wait_status,
                                      tbool done,
                                      md_commit_completion_shutdown_type
                                               shutdown_type,
                                      lt_dur_ms unused,
                                      lt_dur_ms shutdown_kill_wait,
                                      const char *proc_descr,
                                      mdm_action_std_async_completion_proc_func
                                      action_finalize_func,
                                      void *finalize_arg);

/**
 * Like md_commit_add_completion_proc_state_action_full(), except for
 * event handlers instead of action handlers.
 */
int md_commit_add_completion_proc_state_event_full(md_commit *commit, 
                                       pid_t pid,
                                       int wait_status,
                                       tbool done,
                                       md_commit_completion_shutdown_type
                                              shutdown_type,
                                       lt_dur_ms unused,
                                       lt_dur_ms shutdown_kill_wait,
                                       const char *proc_descr,
                                       mdm_event_std_async_completion_proc_func
                                       event_finalize_func,
                                       void *finalize_arg);

/**
 * Wrapper around md_commit_add_completion_proc_state_action_full().
 * Passes {::mccs_signal_sigterm, 0, md_child_process_wait_time_ms}
 * for {shutdown_type, unused, shutdown_kill_wait}, respectively.
 */
int md_commit_add_completion_proc_state_action(md_commit *commit, 
                                      pid_t pid,
                                      int wait_status,
                                      tbool done,
                                      const char *proc_descr,
                                      mdm_action_std_async_completion_proc_func
                                      action_finalize_func,
                                      void *finalize_arg);

/**
 * Wrapper around md_commit_add_completion_proc_state_event_full().
 * Passes {::mccs_signal_sigterm, 0, md_child_process_wait_time_ms}
 * for {shutdown_type, unused, shutdown_kill_wait}, respectively.
 */
int md_commit_add_completion_proc_state_event(md_commit *commit, 
                                       pid_t pid,
                                       int wait_status,
                                       tbool done,
                                       const char *proc_descr,
                                       mdm_event_std_async_completion_proc_func
                                       event_finalize_func,
                                       void *finalize_arg);

/*
 * Not to be used by modules, for now.  But lives here for symmetry
 * with md_commit_set_completion_proc_async(), which is.
 */
int md_commit_set_completion_msg_async(md_commit *commit, 
                                       tbool complete_async);

int md_commit_get_completion_msg_async(const md_commit *commit, 
                                       tbool *ret_complete_async);

int md_commit_get_completion_msg_async_allowed(const md_commit *commit, 
                             tbool *ret_complete_async_allowed);

int md_commit_cap_check(const md_commit *commit, int32 gid,
                        tbool *ret_exceeds_commit);

/**
 * Get information about the user who made the request associated with
 * the provided commit structure.
 *
 * \param commit Commit state
 *
 * \param orig Should we return "original" user information, when
 * available?  This will only affect the case when
 * md_commit_handle_commit_from_module() is below you in the stack; in
 * this case, a 'true' here will get you the original username, while
 * 'false' will give you the one made up by
 * md_commit_commit_state_new(), ADMIN_INTERNAL.
 *
 * \retval ret_uid The UID of the user who made the request.
 *
 * \retval ret_gid The GID of the user who made the request.
 *
 * \retval ret_username The username of the user who made the request.
 *
 * \retval ret_is_interactive Whether or not the session is interactive
 * (generally, meaning that it is being driven by an end user).
 */
int md_commit_get_user_info(const md_commit *commit, tbool orig,
                            int32 *ret_uid, int32 *ret_gid,
                            tstring **ret_username,
                            tbool *ret_is_interactive);

/**
 * A companion to md_commit_get_user_info(), which tells if the username
 * returned by that function is synthetic.  A "real" username (for which
 * 'false' would be returned) is one that represents an actual user 
 * account on the system.  A "synthetic" username is one that was made
 * up by either the GCL or mgmtd, due to lack of a real user account.
 * This could be for a few different reasons, including:
 *
 *   - The GCL on the client end made up a username like "i:<pid>-<uid>-<gid>"
 *     because the environment variables telling it the username were empty
 *     or unset, probably because the client is a noninteractive program.
 *
 *   - Mgmtd locally made up a username, ADMIN_INTERNAL, because the commit
 *     was initiated internally by some code within mgmtd, and not directly
 *     from a GCL request.
 *
 *   - Mgmtd locally made up a username "UNKNOWN-<uid>/<gid>" because for 
 *     some reason, it was given no username or an empty username by the 
 *     GCL.  This is an error case, and is not expected to ever happen.
 *
 * \param commit Commit state
 *
 * \param orig Should we return "original" user information, when
 * available?  See 'orig' parameter to md_commit_get_user_info().
 *
 * \retval ret_is_synth 'true' for a synthetic username, 'false' for a 
 * real one.
 */
int md_commit_get_username_is_synth(const md_commit *commit, tbool orig,
                                    tbool *ret_is_synth);

/*
 * Look up the local username of the current commit, if it is known.
 * This will only work under PROD_FEATURE_ACLS.  It shows the first
 * username found in the config database at the time the commit's session
 * was opened, which shares the uid of the session.
 */
int md_commit_get_local_username(const md_commit *commit,
                                 tstring **ret_local_username);

/*
 * These are the supported methods for authenticating users.
 *
 * NOTE: these strings are mirrored in lpc_get_trusted_auth_info() in
 * src/base_os/linux_common/pam/lib/libpam_common/lpc_main.c.
 */
#define MD_AAA_LOCAL_AUTH_METHOD   "local"
#define MD_AAA_RADIUS_AUTH_METHOD  "radius"
#define MD_AAA_TACACS_AUTH_METHOD  "tacacs+"
#define MD_AAA_LDAP_AUTH_METHOD    "ldap"

typedef enum {
    maat_none = 0,
    maat_local,
    maat_radius,
    maat_tacacs,
    maat_ldap,
    maat_LAST = maat_ldap
} md_aaa_auth_type;

extern const lc_enum_string_map md_aaa_auth_type_map[];

extern const char md_aaa_local_auth_method[];
extern const char md_aaa_radius_auth_method[];
extern const char md_aaa_tacacs_auth_method[];
extern const char md_aaa_ldap_auth_method[];


/*
 * Get the authentication type associated with the current commit,
 * if any.  This will only have anything if the user logged in
 * interactively and sent a login action with a TRUSTED_AUTH_INFO.
 * For other cases, maat_none will be returned.
 */
int md_commit_get_auth_method(const md_commit *commit,
                              md_aaa_auth_type *ret_auth_method);

/**
 * Get the capability flags associated with the specified commit
 * structure.
 */
int md_commit_get_capabilities(const md_commit *commit,
                               uint32 *ret_capabilities);

/**
 * Tell whether the user context represented by the specified commit
 * structure is a member of the specified auth group.
 */
int md_commit_has_auth_group(const md_commit *commit, ta_auth_group auth_group,
                             tbool *ret_has_auth_group);

/**
 * Get the set of auth groups for the specified commit structure.
 */       
int md_commit_get_auth_groups(const md_commit *commit, 
                              uint32_array **ret_auth_groups);

/**
 * Get the names of ACL roles for the specified commit structure.
 *
 * NOTE: in most cases, you should be concerned with auth groups and
 * not roles, so md_commit_get_auth_groups() is more likely to be
 * interesting.  ACL enforcement is generally done on auth groups,
 * and roles are simply containers which hold sets of auth groups.
 */
int md_commit_get_roles(const md_commit *commit, tstr_array **ret_roles);

/**
 * Get information about the request we are processing, and/or the 
 * session from which it came.  All of these parameters are optional;
 * pass a NULL to not receive any given piece of information.
 *
 * \param commit Commit structure from which to extract information
 *
 * \param orig Should we return the "original" GCL session, when
 * available?  This will only affect the case when
 * md_commit_handle_commit_from_module() is below you in the stack; in
 * this case, a 'true' here will get you the original GCL session,
 * while 'false' will get you NULL, since the original session is not
 * carried forward into the mdc_gcl_session field of the new commit.
 *
 * \retval ret_request_msg_id The request message ID associated with
 * this commit, or -1 if there isn't one.
 *
 * \retval ret_session_id The session ID associated with this commit,
 * or 0 if there isn't one.
 *
 * \retval ret_peer_name The name of the remote peer of the session on
 * which this request arrived, or NULL if there isn't one.
 */
int md_commit_get_request_info(const md_commit *commit, tbool orig,
                               int64 *ret_request_msg_id, 
                               int_ptr *ret_session_id,
                               tstring **ret_peer_name);
                            
/* XXX should be some way to delete / query completion state records */

/**
 * Warning: you should not use this function unless you know exactly why you
 * need it.  Direct modification of the main_db at the wrong time can result
 * in data corruption.
 */
int md_commit_get_main_db(const md_commit *commit, mdb_db ***ret_main_db);

/**
 * Returns the number of requests of any sort (query, set, action,
 * event) that have been processed.  This starts at zero, and
 * increases by one for every request.  Note that this includes
 * md_commit_handle_..._from_module() calls too, so processing a
 * single GCL request may cause it to go up by more than one.  It is
 * meant to be used to confirm that we are still processing the same
 * request as we were at some earlier point when the return value of
 * this function was saved.
 */
int32 md_commit_get_request_count(void);

/**
 * Tell whether the provided commit structure is in the apply phase of
 * a set request.  Commits which don't pertain to set requests will
 * never return 'true' for this; those that do will only return 'true'
 * if the side effects and check phases have all completed.
 */
int md_commit_get_doing_apply(const md_commit *commit, tbool *ret_doing_apply);

/**
 * Get the current db revision id.  Note that this number is
 * incremented during every commit, after the 'side effects' and
 * 'check' phases are done, and before the 'apply' phase begins.
 */
int32 md_commit_get_db_revision_id(void);

#ifdef __cplusplus
}
#endif

#endif /* __MD_MOD_COMMIT_H_ */
