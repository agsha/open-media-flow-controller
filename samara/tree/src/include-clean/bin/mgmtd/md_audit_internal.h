/*
 *
 * src/bin/mgmtd/md_audit_internal.h
 *
 *
 *
 */

#ifndef __MD_AUDIT_INTERNAL_H_
#define __MD_AUDIT_INTERNAL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "md_audit.h"
#include "tstring.h"
#include "md_commit.h"
#include "mdb_db.h"
#include "bnode.h"
    
extern const char md_audit_synthetic_actor[];
extern const char md_audit_requested_by_str[];
extern const char md_audit_action_descr_str[];
extern const char md_audit_action_param_str[];
extern const char md_audit_action_status_str[];

struct mdm_action_audit_context {
    const md_commit *maac_commit;    /* Placeholder.  Not used yet */

    /*
     * Rendered messages to be logged.
     *
     * NOTE: this structure does not own this memory.  It can't be a
     * const pointer because we need to be able to modify it, but the
     * memory is owned by other code.
     */
    tstr_array *maac_log_messages;

    tbool maac_have_description;     /* Has descr. been provided yet? */
    const mdb_node_reg *maac_node_reg; /* Internal action node registration */
    int maac_param_num;              /* Currently not exposed publicly */
    int maac_line_num;               /* Currently not exposed publicly */
};


/*
 * The functions defined here are called by mgmtd action and change auditing
 * modules, and are *not* intented for general use.
 */
int md_audit_handle_action_audit_request(md_commit *commit, const mdb_db *db,
                                         const mdb_node_reg *action_reg,
                                         const tstring *action_name,
                                         const bn_binding_array *req_bindings);

int md_audit_handle_action_audit_response(md_commit *commit);

int md_audit_display_actor(md_commit *commit,
                           const mdb_db *db,
                           tbool orig,
                           tstring **ret_actor,
                           tstring **ret_username,
                           tstring **ret_user_full_name);

int md_audit_acct_append_message(bn_binding_array *msg_bindings,
                                 uint32 msg_num, const char *msg);

uint64 md_audit_acct_transaction_id_next(void);

int md_audit_acct_transaction(md_commit *commit, const mdb_db *db,
                              const char *username,
                              const bn_binding_array *msg_bindings,
                              uint64 reuse_transaction_id,
                              uint64 *ret_transaction_id);

#ifdef __cplusplus
}
#endif

#endif /* __MD_AUDIT_INTERNAL_H_ */
