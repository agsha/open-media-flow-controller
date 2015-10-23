/*
 *
 * tacl.tms.inc.h
 *
 *
 *
 */

/* 
 * This file is included by src/include/tacl.h, unless TACL_NO_TMS is
 * defined.  It contains TMS-specific ACL-related definitions which
 * are inserted into tacl.h just like a set of graft points.  The
 * purpose of having this in a separate file is to separate out the
 * bare infrastructure from the particular authorization scheme (set
 * of auth groups, roles, and ACLs) chosen for use with the base
 * Samara modules.  TACL_NO_TMS can be used when you want to use just
 * the bare bones of libtacl.
 */


/* ------------------------------------------------------------------------- */
/* Graft point 9910: authorization groups
 *
 * This particular set of groups is not part of the core ACL API
 * Rather, it is part of the standard structure of ACLs used by the
 * Samara base modules.  Customers are encouraged to adopt this and
 * extend it to their needs.
 *
 * The Samara standard set of auth groups is based on the idea of
 * several "tracks", where a user may have a different level of
 * privilege on each track.  There is one all-encompassing track,
 * which you could think of as the "generalist track", which covers
 * all nodes.  There are also other tracks organized by feature area.
 * So each node's ACL usually mentions two tracks: a level required on
 * the generalist track, and a level required on the feature-specific
 * track which applies to that node.  For example, if you had a
 * "virtualization specialist" user, they might have only level 5
 * access to everything (on the generalist track), but then level 9
 * access to virtualization-related nodes.
 *
 * There is an auth group for every operation at every level for every
 * track.  Roles are formed by combining these.  For a given operation
 * and track, if a role has an auth group representing a certain level
 * of privilege for that operation and track, the role should have all
 * corresponding lower levels too.
 *
 * This ends up being structured very much like our capabilities
 * system, except for having greater extensibility and multiple
 * tracks.  We will initially use four levels for the main track,
 * similar to "basic", "restricted", and "privileged" capabilities
 * except for one additional level to make room for the "operator"
 * role.  We will initially have only one level for each feature
 * track.  (These are both in addition to the implicit level of not
 * having any privileges at all.)  But we leave some space in the
 * number to allow for future expansion: the level numbers range from
 * 0-9.
 *
 * We don't currently have any "event" tracks, because we only want
 * events to be able to be sent by admin.   Putting event_9 on every
 * definition adds clutter and has no effect since no one besides
 * admin should ever have level 9.
 *
 * Bug 11117 NOTE: when ACLs are stored in the config db, these
 * numbers will have to become stable.  For now, it is not necessary,
 * but we assign numbers anyway in preparation.
 */
#if TACL_HEADER_INC_GRAFT_POINT == 9910
#if 0 /* Fool emacs indenting */
typedef enum {
#endif
    
    /*
     * A special-case auth group used to allow all base Samara roles to
     * query all base Samara nodes.  This is like taag_all_query_9, with
     * two important differences:
     *
     *    (a) All base Samara roles include taag_special_query_all, while
     *        only admin-equivalent ones include taag_all_query_9.
     *
     *    (b) Mgmtd will not report taag_special_query_all to clients
     *        (via its monitoring nodes under /mgmtd/session) among
     *        the auth groups of any users.
     *
     * The intended result here is that anyone with a base Samara role is
     * permitted to query any base Samara mgmtd nodes, BUT that any other
     * enforcement of query restrictions (e.g. by the CLI for reverse-mapping)
     * still be able to be done.
     */
    taag_special_query_all       = 9,

    /*
     * Generalist track: encompasses all nodes.
     */
    taag_all_query_3             = 13,
    taag_all_query_5             = 15,
    taag_all_query_7             = 17,
    taag_all_query_9             = 19,
    taag_all_set_1               = 21,
    taag_all_set_3               = 23,
    taag_all_set_5               = 25,
    taag_all_set_7               = 27,
    taag_all_set_9               = 29,
    taag_all_action_3            = 33,
    taag_all_action_5            = 35,
    taag_all_action_7            = 37,
    taag_all_action_9            = 39,

    /*
     * Network: interfaces, IP, routes, bridging, bonding, vlans, etc.
     */
    taag_net_query_7             = 117,
    taag_net_set_7               = 127,
    taag_net_action_7            = 137,

    /*
     * Authentication and authorization
     *
     * NOTE: these are the only known non-admin groups which can
     * easily be used to escalate to admin privileges.  End users may
     * still wish to assign these auth groups to non-admin users for
     * administrative purposes, and with some care they may be able to
     * at least be informed if such a user escalates his own
     * privileges.  But it should be done with the userstanding that
     * it is a paper-thin wall separating these users from the
     * privileges of a full admin.
     */
    taag_auth_query_9            = 219,
    taag_auth_set_9              = 229,
    taag_auth_action_9           = 239,

    /*
     * Statistics
     */
    taag_stats_query_7           = 317,
    taag_stats_set_7             = 327,
    taag_stats_action_7          = 337,

    /*
     * Virtualization
     */
    taag_virt_query_7            = 417,
    taag_virt_set_7              = 427,
    taag_virt_action_7           = 437,

    /*
     * Clustering
     *
     * We need a lower level because of bug 12580: full cluster privileges
     * would include sending proxied action requests (which are run as
     * admin on the remote system).
     */
    taag_clust_query_7           = 517,
    taag_clust_query_9           = 519,
    taag_clust_set_7             = 527,
    taag_clust_set_9             = 529,
    taag_clust_action_7          = 537,
    taag_clust_action_9          = 539,

    /*
     * CMC (Central Management Console)
     *
     * We need a lower level because of bug 12580: full CMC privileges
     * would include sending proxied action requests (which are run on
     * the remote system with whatever capabilities the CMC server
     * logged in with, most often admin).
     */
    taag_cmc_query_7             = 617,
    taag_cmc_query_9             = 619,
    taag_cmc_set_7               = 627,
    taag_cmc_set_9               = 629,
    taag_cmc_action_7            = 637,
    taag_cmc_action_9            = 639,
    taag_cmc_rendv               = 690,

    /*
     * Diagnostics: logs, sysdumps, email notification.
     */
    taag_diag_query_7            = 717,
    taag_diag_set_7              = 727,
    taag_diag_action_7           = 737,

    /*
     * System: a miscellaneous bucket for all remaining nodes.
     *
     * We need a lower level because of bug 12580: full system
     * privileges would allow a user to attain admin, e.g. by
     * switching to a different config file.
     */
    taag_sys_query_7             = 817,
    taag_sys_query_9             = 819,
    taag_sys_set_7               = 827,
    taag_sys_set_9               = 829,
    taag_sys_action_7            = 837,
    taag_sys_action_9            = 839,

#if 0
};
#endif
#endif /* TACL_HEADER_INC_GRAFT_POINT == 9910 */


/* ------------------------------------------------------------------------- */
/* Graft point 9920: roles
 */
#if TACL_HEADER_INC_GRAFT_POINT == 9920
#if 0 /* Fool Emacs indenting */
typedef enum {
#endif

    tar_operator,
    tar_monitor,

    tar_auth_admin,
    tar_auth_admin_ro,

#ifdef PROD_FEATURE_CLUSTER
    tar_clust,
    tar_clust_ro,
#endif

#if defined(PROD_FEATURE_CMC_SERVER) || defined(PROD_FEATURE_CMC_CLIENT)
    tar_cmc,
    tar_cmc_ro,
    tar_cmc_rendv,
#endif

    tar_diag,
    tar_diag_ro,

    tar_net,
    tar_net_ro,

    tar_stats,
    tar_stats_ro,

    tar_sys,
    tar_sys_ro,

    tar_virt,
    tar_virt_ro,

#if 0
};
#endif
#endif /* TACL_HEADER_INC_GRAFT_POINT == 9920 */


/* ------------------------------------------------------------------------- */
/* Graft point 9950: extern-declarations of pre-defined ACLs
 */
#if TACL_HEADER_INC_GRAFT_POINT == 9950

/*
 * Note: these variables are actually defined and given values in
 * tacl_defs.tms.inc.c.
 */

/*
 * The first set of ACLs defined below mostly have names of the form
 * "tacl_<area>_<group>_<level>".  The "<group>" part names the class
 * of auth group, which are named after the kinds of operations
 * possible: query, set, action, and event.  It is implicit that this
 * group is being granted access to the operation of the same name.
 *
 * In some cases, the form is "tacl_<area>_<group>_<level>_<oper>",
 * mainly with <oper> being "action".  This form is used when the
 * operation permitted is NOT the one implied by the group class.
 * We have these because some actions are really queries at heart
 * (e.g. /stats/actions/query), while some are really sets at heart
 * (e.g. /virt/actions/vm/clone).  In there cases, we usually like to
 * control access to those actions based on membership in
 * query-related or set-related auth groups, not on action-related
 * auth groups.  e.g. if you have a "look but don't touch" user, you
 * don't want to have to put him into an "action" auth group so he can
 * do stats queries.  Therefore, "tacl_*_query_*_action" means those in
 * the query group can perform actions, and so forth.
 *
 * The second set of ACLs defined below have names of the form
 * "tacl_<area>_<level>".  These combine the ACLs for the four basic
 * operations (query, set, action, and event) for the given level,
 * for a more compact representation of standard ACLs.  Note that it's
 * OK to assign to a node an ACL that permits operations that don't
 * apply to the node; those operations are just ignored.  So these ACLs
 * are appropriate for most of the "standard" cases:
 *   - Config nodes: applicable if you want to require the same level
 *     of access for queries and sets.
 *   - Monitoring nodes: nearly always applicable, since query-->query
 *     is the only likely group-->operation mapping you'd want.
 *   - Action nodes: applicable if you want to require membership in
 *     an action group, but not if you want to require query or set
 *     group membership.
 *   - Event nodes: nearly always applicable, since event-->event
 *     is the only likely group-->operation mapping you'd want.
 *
 * XXXX/EMT/RBAC: bug 13808: we should catch registrations which
 * mistakenly use
 * tacl_*_{query,set}_* on an action node, since they probably meant
 * to use tacl_*_{query,set}_*_action instead.  We could probably do
 * this by issuing a warning if the ACL provided was not empty
 * (i.e. it had at least one entry), but everything got filtered out
 * (because they only granted access to irrelevant operations).
 * If someone really wants an empty ACL, they can assign an empty
 * ACL, but if they assign a non-empty one and it ends up effectively
 * empty, this probably indicates a mistake.  Do this test for each
 * call to mdm_node_acl_add(), not just at the end, since they may
 * have added other ACLs too which would make it non-empty.
 */

extern ta_acl *tacl_all_query_3;
extern ta_acl *tacl_all_query_3_action;
extern ta_acl *tacl_all_query_5;
extern ta_acl *tacl_all_query_5_action;
extern ta_acl *tacl_all_query_7;
extern ta_acl *tacl_all_query_7_action;
extern ta_acl *tacl_all_query_9;
extern ta_acl *tacl_all_query_9_action;
extern ta_acl *tacl_all_set_1;
extern ta_acl *tacl_all_set_1_action;
extern ta_acl *tacl_all_set_3;
extern ta_acl *tacl_all_set_3_action;
extern ta_acl *tacl_all_set_5;
extern ta_acl *tacl_all_set_5_action;
extern ta_acl *tacl_all_set_7;
extern ta_acl *tacl_all_set_7_action;
extern ta_acl *tacl_all_set_9;
extern ta_acl *tacl_all_set_9_action;
extern ta_acl *tacl_all_action_3;
extern ta_acl *tacl_all_action_5;
extern ta_acl *tacl_all_action_7;
extern ta_acl *tacl_all_action_9;

extern ta_acl *tacl_net_query_7;
extern ta_acl *tacl_net_query_7_action;
extern ta_acl *tacl_net_set_7;
extern ta_acl *tacl_net_set_7_action;
extern ta_acl *tacl_net_action_7;

extern ta_acl *tacl_virt_query_7;
extern ta_acl *tacl_virt_query_7_action;
extern ta_acl *tacl_virt_set_7;
extern ta_acl *tacl_virt_set_7_action;
extern ta_acl *tacl_virt_action_7;

extern ta_acl *tacl_auth_query_9;
extern ta_acl *tacl_auth_query_9_action;
extern ta_acl *tacl_auth_set_9;
extern ta_acl *tacl_auth_set_9_action;
extern ta_acl *tacl_auth_action_9;

extern ta_acl *tacl_stats_query_7;
extern ta_acl *tacl_stats_query_7_action;
extern ta_acl *tacl_stats_set_7;
extern ta_acl *tacl_stats_set_7_action;
extern ta_acl *tacl_stats_action_7;

extern ta_acl *tacl_clust_query_7;
extern ta_acl *tacl_clust_query_7_action;
extern ta_acl *tacl_clust_set_7;
extern ta_acl *tacl_clust_set_7_action;
extern ta_acl *tacl_clust_action_7;

extern ta_acl *tacl_clust_query_9;
extern ta_acl *tacl_clust_query_9_action;
extern ta_acl *tacl_clust_set_9;
extern ta_acl *tacl_clust_set_9_action;
extern ta_acl *tacl_clust_action_9;

extern ta_acl *tacl_cmc_query_7;
extern ta_acl *tacl_cmc_query_7_action;
extern ta_acl *tacl_cmc_set_7;
extern ta_acl *tacl_cmc_set_7_action;
extern ta_acl *tacl_cmc_action_7;

extern ta_acl *tacl_cmc_query_9;
extern ta_acl *tacl_cmc_query_9_action;
extern ta_acl *tacl_cmc_set_9;
extern ta_acl *tacl_cmc_set_9_action;
extern ta_acl *tacl_cmc_action_9;

extern ta_acl *tacl_cmc_action_rendv;

extern ta_acl *tacl_diag_query_7;
extern ta_acl *tacl_diag_query_7_action;
extern ta_acl *tacl_diag_set_7;
extern ta_acl *tacl_diag_set_7_action;
extern ta_acl *tacl_diag_action_7;

extern ta_acl *tacl_sys_query_7;
extern ta_acl *tacl_sys_query_7_action;
extern ta_acl *tacl_sys_set_7;
extern ta_acl *tacl_sys_set_7_action;
extern ta_acl *tacl_sys_action_7;

extern ta_acl *tacl_sys_query_9;
extern ta_acl *tacl_sys_query_9_action;
extern ta_acl *tacl_sys_set_9;
extern ta_acl *tacl_sys_set_9_action;
extern ta_acl *tacl_sys_action_9;

extern ta_acl *tacl_all_3;
extern ta_acl *tacl_all_5;
extern ta_acl *tacl_all_7;
extern ta_acl *tacl_all_9;

extern ta_acl *tacl_net_7;
extern ta_acl *tacl_virt_7;
extern ta_acl *tacl_auth_9;
extern ta_acl *tacl_stats_7;
extern ta_acl *tacl_clust_7;
extern ta_acl *tacl_clust_9;
extern ta_acl *tacl_cmc_7;
extern ta_acl *tacl_cmc_9;
extern ta_acl *tacl_diag_7;
extern ta_acl *tacl_sys_7;
extern ta_acl *tacl_sys_9;

/*
 * Module-specific combined ACLs.  The names of the ACLs below begin
 * with "sbm_", which stands for "Samara Base Module".  They are more
 * complex combinations of ACLs, each of which is meant to be the full
 * ACL for some set of nodes in the Samara base modules.  This helps us
 * ensure consistenacy between the ACLs in mgmtd, CLI, and Web UI.
 * 
 * General conventions:
 *
 *   _qa: "query action", for a node which is technically an Action,
 *        but which basically performs the role of a Query.  So this ACL
 *        grants the Action operation to those in a Query auth group.
 *        This needs to be a separate ACL because we don't want to generally
 *        grant access to other actions to those with query privileges only.
 *
 *   _sa: "set action", for a node which is technically an Action,
 *        but which basically performs the role of a Set.  See "_qa".
 *
 * Note that we often forego having higher ACLs on nodes which contain
 * plaintext passwords.  Usually, the requirements to set these are the
 * same as for the other config nodes around them, and the only difference
 * is who is supposed to be able to read them.  They are all registered
 * with the "secure" node reg flag, meaning they will be obfuscated at
 * least for non-admin, and possibly also for admin, depending on what
 * PROD_FEATUREs are enabled.  In a few places we have used higher ACLs
 * anyway, arbitrarily; and this will be helpful in the future, when
 * query ACLs are enforced by mgmtd, and we don't want to trust the
 * client.
 *
 * sbm_aaa variants:
 *
 *   sbm_aaa_hi: read admin, write admin.  e.g. RADIUS password.
 *               Read requirements higher for sensitive information.
 *               (Currently unnecessary due to "secure" node marking,
 *               but might be wanted in a stricter world with real
 *               query ACL enforcement.)
 *
 *   sbm_aaa:    read monitor, write admin.  e.g. RADIUS server config.
 *               Not much harm in viewing the data, but changing it would
 *               permit escalation.
 *
 *   sbm_aaa_lo: read monitor, write operator.  e.g. user full name, roles.
 *               Write requirements can be lower because the nodes have a
 *               custom ACL function which only permits operations on
 *               equal or inferior accounts.
 *
 *   sbm_aaa_myacct: read operator, write set_1 (almost anyone).
 *                   This is for hashed user passwords only.
 *                   Though we want to prevent monitor from seeting
 *                   any hashed passwords, we want to allow almost
 *                   anyone to set their own password (see bug 10872).
 *                   This MUST be used in conjunction with a custom
 *                   ACL function to prevent someone from editing 
 *                   the passwords of accounts they are not allowed to.
 */

extern ta_acl *tacl_sbm_aaa_lo;
extern ta_acl *tacl_sbm_aaa;
extern ta_acl *tacl_sbm_aaa_medhi;
extern ta_acl *tacl_sbm_aaa_hi;
extern ta_acl *tacl_sbm_aaa_myacct;
extern ta_acl *tacl_sbm_certs;
extern ta_acl *tacl_sbm_clust;
extern ta_acl *tacl_sbm_clust_hi;
extern ta_acl *tacl_sbm_config_lo;
extern ta_acl *tacl_sbm_config;
extern ta_acl *tacl_sbm_config_save;
extern ta_acl *tacl_sbm_cmc;
extern ta_acl *tacl_sbm_cmc_hi;
extern ta_acl *tacl_sbm_cmc_sa;
extern ta_acl *tacl_sbm_crypto;
extern ta_acl *tacl_sbm_diag;
extern ta_acl *tacl_sbm_diag_hi;
extern ta_acl *tacl_sbm_email;
extern ta_acl *tacl_sbm_graphs;
extern ta_acl *tacl_sbm_graphs_net;
extern ta_acl *tacl_sbm_graphs_virt;
extern ta_acl *tacl_sbm_license;
extern ta_acl *tacl_sbm_logs;
extern ta_acl *tacl_sbm_logcfg_lo;
extern ta_acl *tacl_sbm_logcfg;
extern ta_acl *tacl_sbm_net_lo;
extern ta_acl *tacl_sbm_net;
extern ta_acl *tacl_sbm_pm;
extern ta_acl *tacl_sbm_sched;
extern ta_acl *tacl_sbm_sched_qa;
extern ta_acl *tacl_sbm_shell;
extern ta_acl *tacl_sbm_snmp;
extern ta_acl *tacl_sbm_snmp_hi;
extern ta_acl *tacl_sbm_ssh;
extern ta_acl *tacl_sbm_ssh_hi;
extern ta_acl *tacl_sbm_ssh_hi_sa;
extern ta_acl *tacl_sbm_stats;
extern ta_acl *tacl_sbm_stats_qa;
extern ta_acl *tacl_sbm_sys;
extern ta_acl *tacl_sbm_sys_hi;
extern ta_acl *tacl_sbm_sys_xhi;
extern ta_acl *tacl_sbm_time;
extern ta_acl *tacl_sbm_virt;
extern ta_acl *tacl_sbm_web;
extern ta_acl *tacl_sbm_wizard;
extern ta_acl *tacl_sbm_xg;
extern ta_acl *tacl_sbm_xinetd;

#endif /* TACL_HEADER_INC_GRAFT_POINT == 9950 */
