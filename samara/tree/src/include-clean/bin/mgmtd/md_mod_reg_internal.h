/*
 *
 * src/bin/mgmtd/md_mod_reg_internal.h
 *
 *
 *
 */

#ifndef __MD_MOD_REG_INTERNAL_H_
#define __MD_MOD_REG_INTERNAL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


#include "common.h"
#include "md_mod_reg.h"

struct md_module {
    char *mdm_name;
    uint32 mdm_semantic_version;
    char *mdm_root_node;
    char *mdm_root_config_node;
    uint32 mdm_flags;

    /*
     * Specifies the order in which we call initial DB, upgrade, and
     * side effects functions.
     */
    int32 mdm_commit_order;

    /*
     * Specifies the order in which we call apply functions.
     */
    int32 mdm_apply_order;

    mdm_commit_check_func mdm_check_func;
    void *mdm_check_arg;
    mdm_commit_side_effects_func mdm_side_effects_func;
    void *mdm_side_effects_arg;
    mdm_commit_apply_func mdm_apply_func;
    void *mdm_apply_arg;
    mdm_initial_db_func mdm_initial_func;
    void *mdm_initial_arg;
    mdm_upgrade_downgrade_func mdm_updown_func;
    void *mdm_updown_arg;
        
    mdm_mon_get_func mdm_get_func;
    void *mdm_get_arg;
    mdm_mon_iterate_func mdm_iterate_func;
    void *mdm_iterate_arg;

    char *mdm_upgrade_from_name;
    int32 mdm_upgrade_cutover_version;
    tbool mdm_upgrade_from_ignore_missing;

    /*
     * Parallel arrays for audit functions, and data to be passed to
     * each.  Each module is allowed to have any number of audit
     * functions.
     */
    array *mdm_node_audit_funcs;
    array *mdm_node_audit_funcs_data;

    /*
     * Names of subtree roots in the node hierarchy, in which this
     * module is interested.  This is mainly used only during
     * registration, as a temporary holding place for what the modules
     * request, until it can be encoded in the way we really want it,
     * which is to append the 'md_module *' to the mnr_modules field
     * of all of the reg nodes in each subtree.  We cannot do that
     * when the module calls mdm_set_interest_subtrees() because it's
     * possible that not all of the nodes in those subtrees (or even
     * any of them) are registered yet.
     */
    tstr_array *mdm_interest_subtrees;
    
#ifdef PROD_FEATURE_I18N_SUPPORT
    const char *mdm_gettext_domain;
#endif /* PROD_FEATURE_I18N_SUPPORT */

};

int md_mod_compare_commit_order(const void *elem1, const void *elem2);
int md_mod_compare_apply_order(const void *elem1, const void *elem2);

/*
 * Pointers to config change auditing functions (type
 * mdm_node_audit_func) registered by modules using
 * mdm_audit_register_handler().  This will NOT include functions
 * provided in node registrations.
 */
extern array *md_node_audit_funcs;
extern array *md_node_audit_funcs_data;

/*
 * Do some sanity checks on the node registrations, assuming that all
 * registrations are currently done.
 */
int md_sanity_check_regs(mdb_reg_state *reg_state);

int md_postreg_cleanup(mdb_reg_state *reg_state);

#ifdef __cplusplus
}
#endif

#endif /* __MD_MOD_REG_INTERNAL_H_ */
