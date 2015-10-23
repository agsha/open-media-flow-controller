/*
 *
 * src/bin/mgmtd/mdm_acl.h
 *
 *
 *
 */

#ifndef __MDM_ACL_H_
#define __MDM_ACL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


#include "dso.h"
#include "md_main.h"
#include "mdb_changes.h"
#include "tacl.h"


int md_acl_init(const lc_dso_info *info, void *data);

int md_acl_deinit(const lc_dso_info *info, void *data);

/*
 * Traverse the registration tree, and:
 *   1. Sanity check that each node has ACLs.
 *   2. If ACLs are inheritable, propagate them to descendants who have not
 *      defined their own ACLs.
 *
 * Finally, traverse the tree again and combine any local ACLs in with
 * the main ACLs.
 *
 * Since we do this as a preorder traversal, the sanity check is fine 
 * because by the time we get to a node, we have already done all of its
 * ancestors, so if it's going to inherit something, it would have done so
 * already.
 */
int md_acl_inherit(mdb_reg_state *reg_state);


/*
 * Determine the set of auth groups a specified user is in.  Which user
 * we want can be specified in two ways:
 *
 *   1. By uid.  If this is coming from an incoming GCL request and we
 *      want to know what they can do, this is the only way, since the
 *      username from the GCL is unreliable, and sometimes it is 
 *      something synthetic, like ADMIN_INTERNAL, "scheduled_job",
 *      or "<pid>-0-0".  The username may still be passed, but it is 
 *      only for debugging purposes (e.g. for log messages).
 *
 *   2. By username.  In a different context, we may already know the
 *      username we're interested in, e.g. in the md_passwd side effects
 *      callback.  We don't have any uid to give, because it might be 
 *      subject to change during this commit; that's part of why we
 *      want to know the user's auth groups.  Pass -1 for the uid,
 *      so we'll know the username is real.
 *
 * Uid 0 is a special case.  Generally each uid is only used by a
 * single user; but because uid 0 has a unique meaning, it may be
 * shared by multiple accounts (as well as by some of those synthetic
 * cases).  As far as ACL enforcement, we don't care what the username
 * is, since uid 0 always implies the 'admin' auth group; so we can just
 * blindly add it.
 *
 * In addition to a uid/username, an md_session_state structure may be
 * provided.  If this is NULL, you are just getting the roles and auth
 * groups configured on the named account.  But if it is provided, any
 * extra roles on that particular session (added via the login action)
 * are added.  Note that the extra roles do NOT get factored into the
 * list of UNIX groups, since they are on the session and not on the
 * account.
 *
 * XXXX/EMT/RBAC: bug 14081: it may be a perf issue to fetch this every time.
 * We could do some caching...
 */
int md_acl_compute_auth_groups(const mdb_db *db, int32 uid,
                               const char *username,
                               const md_session_state *session_state,
                               tstring **ret_local_username,
                               tstr_array **ret_roles,
                               uint32_array **ret_auth_groups,
                               uint32_array **ret_unix_groups);


/*
 * Convert an mdb_db_change record into an ACL operation.
 *
 * Note that while currently, we only return a single operation flag,
 * that may not always be the case.  e.g. if we start to distinguish
 * between modification of different attributes, a single change
 * record could require multiple ACL operations.
 */
int mdb_db_change_to_acl_oper(const mdb_db_change *change,
                              ta_oper_bf *ret_opers);


/*
 * Check to see if a particular operation should be permitted against
 * a particular node, in the context of a particular commit (which
 * presumably already knows what auth groups the requester is in).
 *
 * 'old_db' and 'new_db' may be the same, and 'change' may be NULL, if
 * it's not a Set request.  'node_name_parts' may be NULL, and they will
 * be computed from the node name.  'bindings' is only for actions.
 */
int md_acl_check(md_commit *commit, const mdb_db *old_db, const mdb_db *new_db,
                 const char *node_name, const tstr_array *node_name_parts,
                 const mdb_db_change *change, const bn_binding_array *bindings,
                 ta_oper_bf oper, const mdb_node_reg *reg, tbool *ret_ok,
                 char **ret_msg);

#ifdef PROD_FEATURE_ACLS_QUERY_ENFORCE
/*
 * Standardized ACL check function for libmdb enforcement of ACL
 * restrictions.  (Only used for queries for now)
 */
int md_acl_check_handler(md_commit *commit, const mdb_db *db,
                         const mdb_node_reg *reg_node,
                         const char *node_name,
                         const tstr_array *node_name_parts,
                         ta_oper_bf opers_wanted, tbool *ret_ok);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MDM_ACL_H_ */
