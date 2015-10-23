/*
 * src/bin/mgmtd/md_mod_reg.h
 */

#ifndef __MD_MOD_REG_H_
#define __MD_MOD_REG_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/*****************************************************************************
 *****************************************************************************
 ** Mgmtd module registration API
 **
 ** (Note: this header is included by modules, so no internal header
 ** files or declarations should be here.)
 *****************************************************************************
 *****************************************************************************
 */

#include "common.h"
#include "bnode.h"
#include "mdb_db.h"
#include "md_mod_commit.h"
#include "md_client.h"
#include "license.h"
#include "tacl.h"
#include "md_audit.h"

/** \defgroup mgmtd Mgmtd module API */

/**
 * \file src/bin/mgmtd/md_mod_reg.h API for registering mgmtd nodes
 * \ingroup mgmtd
 */

/** \cond */

/* Used by clients of the monitoring infrastructure */
#define MD_MODULE

typedef struct md_module md_module;

TYPED_ARRAY_HEADER_NEW_NONE(md_module, struct md_module *);
int md_module_array_new_var(md_module_array **ret_arr);
int md_module_array_new_const(md_module_array **ret_arr);

/** \endcond */

/*****************************************************************************
 *****************************************************************************
 ** Overall module registration API
 **
 ** Before registering any nodes, you must first register your module
 ** overall.
 *****************************************************************************
 *****************************************************************************
 */

/**
 * Module registration flags.  These apply various modifications to
 * your module registration; put a combination of these ORed together
 * in the md_mod_flags parameter to mdm_add_module().
 */
typedef enum {
    /**
     * Usage of this flag is RARE.  If you think you need it, please
     * read below and consider.
     *
     * By default, your module's commit callbacks (side effects, check,
     * and apply functions) are called when at least one node that it
     * registered is involved in the commit being processed.  If you need 
     * to be called in a broader range of cases, you have two options:
     *
     *   1. Use mdm_set_interest_subtrees() to name the additional subtrees
     *      for which you want to be called.  This is usually the preferable
     *      option, as it tries to avoid calling you in cases you didn't
     *      care about (which would be a drag on performance, and clutters
     *      the logs at level INFO).  Pass names of registration nodes here
     *      (possibly including wildcards); do not use any specific wildcard
     *      instance names, as these will not work.
     *
     *   2. Use this flag (modrf_all_changes).  This will cause you to be
     *      called for ALL commits, regardless of what nodes are involved.
     *      This is usually not the preferable option, because it will
     *      usually end up calling you in a lot of cases that are not
     *      relevant to you.  It can be used as a last resort, generally
     *      either (a) if you really do need to be called for everything,
     *      like mdm_changes.c, or (b) if it's too complex or error-prone
     *      to figure out which subtrees you care about.
     *
     *      Note that using modrf_all_changes IS preferable to calling
     *      mdm_set_interest_subtrees() with "/" (the root of the whole
     *      node hierarchy).
     */
    modrf_all_changes             =  1 << 1,

    /**
     * Normally your module's side effects, check, and apply functions
     * are only called when some of the nodes involved are underneath
     * its module root.  If you want to be called for the first
     * (initial) commit, even if you have no config nodes, set this
     * flag.  This may be useful to syncronize externally held state
     * with module state or configuration.
     * 
     */
    modrf_initial_commit          =  1 << 2,

    /**
     * Normally your module may only register nodes whose names fall
     * underneath its module root node.  If you want this restriction
     * lifted, set this flag.
     */ 
    modrf_namespace_unrestricted  =  1 << 3,

    /**
     * Normally your module's upgrade function will only be called if
     * the database being loaded was last written with your module present,
     * and already has an older semantic version number for your module.
     * Use this flag to request that mgmtd transfer the semantic version
     * number from a different module, making your upgrade logic eligible
     * to be called even if your module is brand new.
     *
     * NOTE: this flag must be used in conjunction with
     * mdm_set_upgrade_from().
     */
    modrf_upgrade_from            =  1 << 4,

} mdm_module_reg_flags;

/**
 * Register a new module with the mgmtd infrastructure.  Your module
 * must call this once before registering any nodes.
 *
 * Please refer to the Samara Technical Guide for detailed
 * descriptions of what each of these parameters are for.
 *
 * NOTE: mon_iterate_func and mon_iterate_arg are currently ignored.
 */
int mdm_add_module(const char *module_name, uint32 module_version,
                   const char *root_node_name, const char *root_config_name,
                   uint32 md_mod_flags,

                   mdm_commit_side_effects_func side_effects_func, 
                   void *side_effects_arg,
                   mdm_commit_check_func check_func, void *check_arg,
                   mdm_commit_apply_func apply_func, void *apply_arg,
                   
                   int32 commit_order, int32 apply_order,

                   mdm_upgrade_downgrade_func updown_func, void *updown_arg,
                   mdm_initial_db_func initial_func, void *initial_arg,
                   
                   mdm_mon_get_func mon_get_func, 
                   void *mon_get_arg,
                   mdm_mon_iterate_func mon_iterate_func,
                   void *mon_iterate_arg,
                   
                   md_module **ret_module);

#ifdef PROD_FEATURE_I18N_SUPPORT
/**
 * Only applicable if you are internationalizing your product.
 * 
 * Specify what gettext domain will contain the string catalog for
 * this module.  Generally all modules in a single directory all share
 * the same gettext domain, so in a customary setup there will be two
 * gettext domains: one for Tall Maple's reference modules, and one
 * for the customer's own modules.
 *
 * IMPORTANT: call this before registering any of your nodes, as the
 * value you pass here is subsequently copied into each node
 * registration as it comes in.
 */
int mdm_module_set_gettext_domain(md_module *mod, const char *gettext_domain);
#endif /* PROD_FEATURE_I18N_SUPPORT */


/**
 * \name Capabilities / Privileges
 *
 * There are four classes of operations (query, set, action, and event).
 * Each operation defines privilege flags in a single octet, so eight
 * flags are possible.  Three flags per operation class are currently
 * defined (basic, restricted, and privileged; explained further below).
 * The value for the privilege flags defined in the 'mdm_capability_type'
 * enum should be a single bit within a byte.
 *
 * The parts of these you are likely to use as a module writer:
 *
 *   - mcf_cap_node_* to set permissions on config and monitoring nodes.
 *   - mcf_cap_action_* to set permissions on action nodes.
 *   - mcf_cap_event_* to set permissions on event nodes.
 *   - mcf_cap_*_caps tells you what user groups have what capabilities.
 *
 * There are various graft points below to define new capability levels,
 * and to define new privilege groups (combinations of capabilities that
 * map to a gid that can be assigned to users).
 */
/*@{*/

/**
 * Operation class (also number of bits to shift flags for that operation
 * class to fit it into a 32-bit field representing flags for all classes)
 *
 * As a module writer, you will not use these directly.
 */
typedef enum {
    mot_query  = 0 * 8,
    mot_set    = 1 * 8,
    mot_action = 2 * 8,
    mot_event  = 3 * 8,

    /*
     * We have no mot_LAST here because it would be meaningless:
     * (3*8)+1 = 25.
     */

    /*
     * These are masks that can be used to isolate the bits pertaining
     * to a single operation type.
     */
    mot_query_mask =   0xff << mot_query,
    mot_set_mask =     0xff << mot_set,
    mot_action_mask =  0xff << mot_action,
    mot_event_mask =   0xff << mot_event
} mdm_operation_type;

extern const lc_enum_string_map mdm_operation_name_map[];

/*
 * XXX/EMT: privilege flag notes:
 * The CMC and reserved flags are not consecutive with
 * the others because we initially failed to reserve any flags for
 * our own usage and customers started using flags starting from
 * 0x08.  There is a shortage of flags here, and if possible
 * perhaps this should become a 16-bit field (thus storing
 * capabilities in a uint64 rather than a uint32).  We can also
 * always rearrange these with some minor coordination with
 * customers; they have only to change their graft points, but not
 * do any db upgrades since the gids stored in the db are not
 * influenced by these values.
 */

/* ======================================================================== */
/** Privilege flags within an operation class.  Note that these flags
 * are not inherently ordered or overlapping; the user may technically
 * have any combination of these flags.  However, they are designed
 * such that if a user has a given flag, he should also have all of
 * the lower flags.  So by convention, the only four possible
 * combinations of flags in a given operation class are all three flags;
 * just restricted and basic; just basic; and none of them.
 *
 * 0x80 is reserved for future Samara internal usage.
 */
typedef enum {
    mct_basic      = 0x01,
    mct_restricted = 0x02,
    mct_privileged = 0x04,
    mct_cmc        = 0x40,

/* -------------------------------------------------------------------------
 * Customer-specific graft point 1: capabilities enum.
 * Add your own privilege flags here.
 * Values available are: 0x08, 0x10, 0x20
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 1
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */

    mct_LAST
} mdm_capability_type;

static const lc_enum_string_map mdm_capability_name_map[] = {
    {mct_basic,      "basic"},
    {mct_restricted, "restricted"},
    {mct_privileged, "privileged"},
    {mct_cmc,        "cmc"},

/* -------------------------------------------------------------------------
 * Customer-specific graft point 2: privilege flag name map
 * For every privilege flag you added in graft point 1, add an 
 * entry here mapping the enum to a brief descriptive string.
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 2
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */

    {0,              NULL}
};

/* ======================================================================== */
/** Operation-specific privilege flags.  These could be called "capability
 * flags" because they each signify a specific capability that the user
 * may have.  A user's permissions are composed of a set of these flags;
 * and any operation that the user may wish to perform requires a certain
 * set of these flags.  The user is authorized to perform the action if
 * their capability flags are a superset of those required.
 */
typedef enum {
    mcf_cap_query_basic       = mct_basic << mot_query,
    mcf_cap_query_restricted  = mct_restricted << mot_query,
    mcf_cap_query_privileged  = mct_privileged << mot_query,
    mcf_cap_query_cmc         = mct_cmc << mot_query,

/* -------------------------------------------------------------------------
 * Customer-specific graft point 3: query capability flags.
 * If you added any general privilege flags above in graft points 1 and 2,
 * add the 'query' versions of them here.  These are for controlling 
 * who can make 'query' requests against nodes they are applied to.
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 3
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */


    mcf_cap_set_basic         = mct_basic << mot_set,
    mcf_cap_set_restricted    = mct_restricted << mot_set,
    mcf_cap_set_privileged    = mct_privileged << mot_set,
    mcf_cap_set_cmc           = mct_cmc << mot_set,

/* -------------------------------------------------------------------------
 * Customer-specific graft point 4: set flags
 * If you added any general privilege flags above in graft points 1 and 2,
 * add the 'set' versions of them here.  These are for controlling 
 * who can make 'set' requests against nodes they are applied to.
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 4
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */


    mcf_cap_node_basic        = (mcf_cap_query_basic | mcf_cap_set_basic),
    mcf_cap_node_restricted   = (mcf_cap_query_restricted | 
                                 mcf_cap_set_restricted),
    mcf_cap_node_privileged   = (mcf_cap_query_privileged | 
                                 mcf_cap_set_privileged),
    mcf_cap_node_cmc          = (mcf_cap_query_cmc |
                                 mcf_cap_set_cmc),

/* -------------------------------------------------------------------------
 * Customer-specific graft point 5: "node" flags (query/set combo)
 * These capabilities are simply the union of the query and set capabilities
 * at each level.  When registering configuration nodes, generally we want
 * to require the same level of privilege to query and set them, so we use
 * these as a convenience.  So again, add query/set versions of all of your
 * new privilege flags from graft points 1 and 2 here.
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 5
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */


    mcf_cap_action_basic      = mct_basic << mot_action,
    mcf_cap_action_restricted = mct_restricted << mot_action,
    mcf_cap_action_privileged = mct_privileged << mot_action,
    mcf_cap_action_cmc        = mct_cmc << mot_action,

/* -------------------------------------------------------------------------
 * Customer-specific graft point 6: action capability flags
 * If you added any general privilege flags above in graft points 1 and 2,
 * add the 'action' versions of them here.  These are for controlling 
 * who can make 'action' requests against nodes they are applied to.
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 6
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */


    mcf_cap_event_basic       = mct_basic << mot_event,
    mcf_cap_event_restricted  = mct_restricted << mot_event,
    mcf_cap_event_privileged  = mct_privileged << mot_event,
    mcf_cap_event_cmc         = mct_cmc << mot_event,

/* -------------------------------------------------------------------------
 * Customer-specific graft point 7: event capability flags
 * If you added any general privilege flags above in graft points 1 and 2,
 * add the 'event' versions of them here.  These are for controlling 
 * who can make 'event' requests against nodes they are applied to.
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 7
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */


/* =========================================================================
 * Here we define the sets of capabilities that different user classes
 * (sometimes called "privilege classes") have.
 */

    /**
     * Capabilities for the 'admin' privilege class.  The admin user
     * starts out in this class, and other users may be added.
     */
    mcf_cap_admin_caps        = (

/* -------------------------------------------------------------------------
 * Customer-specific graft point 8: mcf_cap_admin_caps.
 * The admin privilege class must ALWAYS have all the capabilities.
 * So if you have added any additional capability flags in the graft
 * points above, make sure to add all of them here, ORed together.
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 8
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */

                                 mcf_cap_query_basic |
                                 mcf_cap_query_restricted | 
                                 mcf_cap_query_privileged | 
                                 mcf_cap_query_cmc |
                                 mcf_cap_set_basic |
                                 mcf_cap_set_restricted |
                                 mcf_cap_set_privileged |
                                 mcf_cap_set_cmc |
                                 mcf_cap_action_basic |
                                 mcf_cap_action_restricted |
                                 mcf_cap_action_privileged |
                                 mcf_cap_action_cmc |
                                 mcf_cap_event_basic |
                                 mcf_cap_event_restricted |
                                 mcf_cap_event_privileged |
                                 mcf_cap_event_cmc),

/* -------------------------------------------------------------------------
 * Customer-specific graft point 9: new capability classes.
 * If you want to add any new capability classes, define them here.
 * Call them mcf_cap_*_caps.
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 9
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */

    /**
     * Capabilities for the 'monitor' privilege class.  The monitor user
     * starts out in this class, and other users may be added.
     */
    mcf_cap_monitor_caps      = (

/* -------------------------------------------------------------------------
 * Customer-specific graft point 10: mcf_cap_monitor_caps.
 * Generally the monitor user should be able to query everything,
 * perform a limited set of actions, and perform no sets.  So if you
 * added any privilege flags, make sure to add their 'query' versions here;
 * and also any 'action' versions you feel are appropriate.  We recommend
 * not adding any 'set' capabilities here.
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 10
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */

                                 mcf_cap_query_basic | 
                                 mcf_cap_query_restricted | 
                                 mcf_cap_query_cmc |
                                 /*
                                  * Monitor has no set capabilities 
                                  */
                                 mcf_cap_action_basic |
                                 mcf_cap_event_basic),

    /**
     * Capabilities for the 'unpriv' privilege class.  No users start out
     * in this class, but some can be added.  This class has no privileges
     * whatsoever.
     */
    mcf_cap_unpriv_caps      =  0,

    /**
     * Capabilities for the 'cmcrendv' privilege class.  This is the
     * privilege level held by the cmcrendv user on the CMC server and
     * client.  The CMC rendezvous client runs as this user on the CMC
     * client; and logs into this account on the CMC server to
     * announce itself.
     *
     * We need basic query so rendv_client can get its config.
     * We need basic action so we can register to receive config
     * change notifications.  We need CMC action so we can announce
     * ourselves to the server.  We need CMC event so the server
     * mgmtd module can send an event saying that a new client is
     * available.  We need basic event so RGP connected events
     * and config change events can be sent as a result of our
     * rendezvous announcement action.
     *
     * We do NOT need any set capabilities, even in the case where
     * auto-accept is enabled and the appliance record will be created
     * immediately as a result of our action.  This action executes
     * the set as uid 0, not using the privileges of the provided
     * commit.
     */
    mcf_cap_cmcrendv_caps    = (mcf_cap_query_basic |
                                mcf_cap_action_cmc |
                                mcf_cap_action_basic |
                                mcf_cap_event_basic |
                                mcf_cap_event_cmc),

} mdm_capabilities;

/* ======================================================================= */
/** The set of capabilities that a user has is encoded in their gid.
 * A gid maps to a set of capabilities.  Here we define those mappings.
 */
typedef struct mdm_gid_capabilities {
    mdc_gid_type mgcm_gid;
    mdm_capabilities mgcm_capabilities;
    tbool mgcm_enforce_query_capability;
} mdm_gid_capabilities;

static const mdm_gid_capabilities mdm_gid_cap_map[] = {
    {mgt_admin,      mcf_cap_admin_caps, false},
    {mgt_radmin,     mcf_cap_admin_caps, false},
    {mgt_monitor,    mcf_cap_monitor_caps, false},
    {mgt_unpriv,     mcf_cap_unpriv_caps, false},
    {mgt_cmcrendv,   mcf_cap_cmcrendv_caps, false},

/* -------------------------------------------------------------------------
 * Customer-specific graft point 11: mapping gids from mdc_gid_type
 * to capabilities.  If you added any new privilege classes in graft
 * point 9, you'll need to map a gid to them here.  You'll also need
 * to add an instance of the mgt_... enum.  These enums are defined in
 * src/include/md_client.h, and you can use its graft point 1 to add
 * new gids.
 * -------------------------------------------------------------------------
 */
#ifdef INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT
#undef MD_MOD_REG_HEADER_INC_GRAFT_POINT
#define MD_MOD_REG_HEADER_INC_GRAFT_POINT 11
#include "../bin/mgmtd/md_mod_reg.inc.h"
#endif /* INC_MD_MOD_REG_HEADER_INC_GRAFT_POINT */

};

/*@}*/

/* ========================================================================= */
/**
 * Node registration flags
 *
 * Note: most of the individual flags are not ones that you need to
 * concern yourself with (please ignore the man behind the curtain).
 * Generally you should use the ones under the "Common flag
 * combinations" label below.
 */
typedef enum {
    /** \cond */

    /* XXX should make notifies a 'kind' */
    mrf_kind_config         =  1 << 1,
    mrf_kind_state          =  1 << 2,
    mrf_kind_MASK           =  ( mrf_kind_config | mrf_kind_state),

    mrf_rtype_binding       =  1 << 3,
    mrf_rtype_event         =  1 << 4,
    mrf_rtype_action        =  1 << 5,
    mrf_rtype_MASK          =  (mrf_rtype_binding | mrf_rtype_event | 
                                mrf_rtype_action),

    mrf_loc_internal        =  1 << 6,
    mrf_loc_external        =  1 << 7,
    mrf_loc_MASK            =  (mrf_loc_internal | mrf_loc_external),

    mrf_type_literal        =  1 << 8,
    mrf_type_wc             =  1 << 9,
    mrf_type_MASK           =  (mrf_type_literal | mrf_type_wc),

    mrf_owner_local         =  1 << 10,
    mrf_owner_remote        =  1 << 11,   /* Pass through */
    mrf_owner_MASK          =  (mrf_owner_local | mrf_owner_remote),

    mrf_async_blocking      =  1 << 12,
    mrf_async_nonbarrier    =  1 << 13,   /* For remote nodes only */
    mrf_async_MASK          =  (mrf_async_blocking | mrf_async_nonbarrier),

    /**
     * Specify that this node can only be fetched using a direct query
     * GET request.  No ITERATE request will ever return this node, or
     * enter the subtree underneath this node.
     *
     * Note that this does not apply to external monitoring nodes.
     *
     * This could be useful for nodes that are expensive to serve, but
     * want to go underneath subtrees that are commonly queried with
     * iterate/subtree.
     */
    mrf_get_only            =  1 << 14,

    /**
     * Set this flag to indicate that the ACL-related registration
     * fields on this node (except for local ACLs added using
     * mdm_node_acl_add_local()) can be inherited by descendants.
     *
     * By default, ACLs are not inheritable.  We want this default
     * for two reasons:
     *
     *   1. To prevent accidental inheritance, which might assign
     *      unintended ACLs to some nodes, and suppress some warnings
     *      about nodes that do not have ACLs explicitly declared.
     *
     *   2. To allow an inherited ACL to be overridden at some interior
     *      node without disturbing its descendants, if desired.
     *
     * Note that this affects not only mrn_acl, but also mrn_acl_func
     * and mrn_acl_data.  All three fields will be inheritable from a
     * node that sets this flag.  However, the overrides for mrn_acl
     * and mrn_acl_func are independent; setting one does not 
     * automatically override the other.  (mrn_acl_data follows 
     * mrn_acl_func in this regard)  That is, setting one of the fields
     * overrides that field, but does not cause the other fields to be
     * overridden with NULLs if they are not set.
     *
     * Local ACLs set using mdm_node_acl_add_local() are not affected
     * by this flag, nor do they affect it.  See comments below on
     * mrn_acl_local for futher explanation.
     *
     * (XXX/EMT: note that due to this behavior, you cannot override a
     * non-NULL callback function or data with a NULL one.  We could
     * add a new reg flag to force an override, if this is needed.)
     */
    mrf_acl_inheritable     =  1 << 15,

    /**
     * This flag affects the dbchange events which are constructed and
     * sent as a result of a database commit.  It applies to all
     * literal registration nodes in the subtree rooted at this node,
     * potentially including this node itself (if it is a literal).
     * It requests that if a node matching the affected registration
     * nodes is deleted, that a record of that delete not be included
     * in the dbchange event.
     *
     * Most typically, you would use this flag on a wildcard
     * registration node, to request that when an instance of that
     * node is deleted, dbchange records not be included for the
     * literals under that node.  This would mean that recipients of
     * the event would not be told what values the literals had right
     * before deletion.  But in many cases, all recipients need to
     * know is which object was deleted.
     *
     * This is mainly used for performance reasons, particularly in
     * cases of objects which may have many instances and/or many
     * child nodes.  Constructing and sending a dbchange event with
     * many nodes can become expensive, and this flag cuts down on
     * the number of nodes included in the case of deletes.
     *
     * Note that this only affects dbchange events.  It does NOT
     * affect either (a) the mdb_db_change_array passed to mgmtd
     * modules, or (b) the database change auditing log messages.
     *
     * See also related flag ::mrf_change_no_notify.
     */
    mrf_delete_literal_no_notify = 1 << 16,

    /**
     * Applicable to configuration nodes only.
     * (XXX/EMT: could be extended to monitoring nodes too, if 
     * a "default" was defined for them.)
     *
     * Specifies that if this node is a config literal, it is eligible to
     * be omitted from query results which would otherwise have included
     * it.  It will be omitted if (a) the query uses the
     * ::bqnf_sel_class_omit_defaults or ::mdqf_sel_class_omit_defaults
     * flags, and (b) the node's ba_value attribute matches its registered
     * initial value.
     *
     * This flag is automatically inherited by all descendants of the node
     * on which it is used.  It only affects config literals; but if a
     * developer wants to apply it to all literal children of a wildcard,
     * it may be used on the wildcard to take advantage of the
     * inheritance behavior.
     *
     * NOTE: this flag is not thwarted by the ::mrf_exposure_obfuscate
     * registration flag.  If both flags are in use, the node will
     * still be eligible for suppression from query results.  This is a
     * form of information leakage.  If this is not desirable, the
     * flags should not be combined.
     */
    mrf_omit_defaults_eligible = 1 << 17,

    /**
     * Applicable to configuration nodes only.
     *
     * Clear the ::mrf_omit_defaults_eligible flag on this node and the
     * subtree underneath it.  This overrides the other flag after it
     * has been propagated through inheritance.  So there's no reason
     * to use both flags on the same node; typically you'd use 
     * ::mrf_omit_defaults_eligible on a larger subtree, most of whose
     * nodes do want to be eligible for having defaults omitted;
     * and then use ::mrf_omit_defaults_not_eligible on subtrees (or
     * individual nodes) within the larger subtree, which are the
     * exceptions to this rule.
     */
    mrf_omit_defaults_not_eligible = 1 << 18,

    /**
     * Like ::mrf_get_only, except:
     *
     *   (a) If this node has descendants, the flag will not prevent an
     *       Iterate from entering the subtree rooted at this node.
     *       That is, only the single node itself on which the flag is
     *       used is prevented from being returned.
     *
     *   (b) Also applies to external monitoring nodes.  If this flag is
     *       used on an external monitoring node, the node's registration
     *       information will not be included in the request to the provider.
     *       Note that if the provider returns an answer for this node
     *       anyway, the infrastructure will not filter it out.  But if the
     *       provider uses mdc_dispatch_mon_request(), it will naturally
     *       be skipped.
     *
     * One possible use for this flag is for "dummy nodes" added to
     * provide a place to hang an ACL to control Iterate permissions on
     * wildcard nodes.  Most wildcard nodes do not naturally have an
     * explicitly registered parent; however, under PROD_FEATURE_ACLS
     * with PROD_FEATURE_ACLS_QUERY_ENFORCE enabled, they must in order
     * to define the permissions to iterate over the wildcard.  You do 
     * not need to use this flag on such a node, but you can to lessen
     * the risk of adding a new node, since sometimes modules and clients
     * get confused when new, unexpected nodes appear.
     */
    mrf_get_only_allow_subtree = 1 << 19,

    mrf_maxaccess_ro        =  1 << 21,
    mrf_maxaccess_wo        =  1 << 22,
    mrf_maxaccess_rw        = (mrf_maxaccess_ro | mrf_maxaccess_wo),
    mrf_maxaccess_exec      =  1 << 23,
    mrf_maxaccess_notify    =  1 << 24,
    mrf_maxaccess_MASK      = (mrf_maxaccess_ro | mrf_maxaccess_wo |
                               mrf_maxaccess_exec | mrf_maxaccess_notify),

    mrf_status_current      =  1 << 25,
    mrf_status_deprecated   =  1 << 26,
    mrf_status_MASK         = (mrf_status_current | mrf_status_deprecated),

    /**
     * No special exposure characteristics.  Note that this flag will be
     * set on ALL nodes even if other exposure flags are set, because it
     * is included in all of the prepackaged sets of flags below.  This 
     * effectively makes the flag meaningless, so it should be ignored.
     */
    mrf_exposure_normal     =  1 << 27,
    
    /**
     * For an action or event node, do not log the raw bindings
     * contained in a request for this node.
     *
     * For an action node, audit logging is still done for the
     * bindings, but the values are all obfuscated by default
     * (as if maba_obfuscate_logging were set on each one).
     * This default behavior can be overridden by setting the
     * maaf_bindings_default_sensitivity_override flag on the 
     * action node; this makes the bindings logged normally,
     * unless overridden by per-binding registration, which can
     * be used to obfuscate or suppress only whichever bindings 
     * want such treatment.
     *
     * Furthermore, if mrf_exposure_sensitive is set, any action
     * response failure message is not audit logged, as it might contain
     * sensitive information about the data that triggered the error (for
     * example a badly formed URL that contains an embedde passowrd.
     * Conversely, without mrf_exposure_sensitive set, action response messages
     * are audit logged.  Action registration audit flags may be used to
     * override the sensitivity of the action response message.  See
     * maaf_response_msg_override_not_sensitive and
     * maaf_response_msg_override_sensitive for details.
     *
     * For a configuration node, do not log changes to this node in
     * the configuration audit log.
     */
    mrf_exposure_sensitive  =  1 << 28,

    /**
     * For a configuration node, when saving to persistent storage,
     * encrypt the contents of the node using a key hardwired into the
     * mgmtd binary.  This also affects the results you can get back
     * from a query if the mdqf_as_saved or bqnf_as_saved query flags
     * are set.  The key is defined as md_config_crypt_key_str in
     * customer.h.
     *
     * NOTE: this flag is only heeded if PROD_FEATURE_SECURE_NODES is
     * enabled.  If that feature is not enabled, the flag has no effect.
     */
    mrf_exposure_encrypt    =  1 << 29,

    /**
     * This flag may only be used on literal (non-wildcard)
     * configuration nodes.  It has three effects, the first two
     * of which are contingent on PROD_FEATURE_SECURE_NODES.
     *
     *  (a) When queried over the GCL from a mgmtd client, the contents
     *      of the node will be replaced with a fixed value from 
     *      bn_obfuscated_values.  This is NOT done when the node is 
     *      queried by a mgmtd module.  A GCL client who wants the cleartext
     *      value can use the bqnf_cleartext query flag; a mgmtd client who
     *      wants the obfuscated value can use mdqf_obfuscate.
     *
     *      If a binding node's value is obfuscated, the ba_obfuscation 
     *      attribute will be present in that binding, set to 'true'.
     *
     *      NOTE: this effect is only enforced if PROD_FEATURE_SECURE_NODES
     *      is enabled; or if (PROD_FEATURE_OBFUSCATE_NONADMIN is enabled,
     *      and the GCL query comes from a "less privileged" user.
     *      (Under Capabilities, "less privileged" means they do not have
     *      query_privileged or action_privileged; under ACLS, it means they
     *      are not a member of the 'admin' auth group.)
     *
     *  (b) Whenever the value of the node appears in a
     *      /mgmtd/notify/dbchange event, its value will be obfuscated
     *      (and have ba_obfuscation set).
     *
     *      Again, this is only done if PROD_FEATURE_SECURE_NODES is
     *      enabled... but PROD_FEATURE_OBFUSCATE_NONADMIN has no effect
     *      here.  If we are doing this, a /mgmtd/notify/dbchange/cleartext
     *      event is also sent, with all of the values unobfuscated.
     *      
     *  (c) When changes to the node are logged for config change audit
     *      logging, the value will not be mentioned.  No obfuscated value
     *      is used either; it simply says that the node has changed.
     *
     *      NOTE: this effect is enforced regardless of whether or not 
     *      PROD_FEATURE_SECURE_NODES is enabled.
     */
    mrf_exposure_obfuscate  =  1 << 30,

    mrf_exposure_MASK       =  (mrf_exposure_normal | mrf_exposure_sensitive |
                                mrf_exposure_encrypt | mrf_exposure_obfuscate),

    /**
     * Use this flag to specify that changes to instances of this node
     * should not be included in configuration change notification
     * events.  Generally you would only use this on wildcards, or
     * children of wildcards, in cases where you expect those
     * wildcards to be very numerous.  Generating configuration change
     * notifications places a heavy load on the CPU and memory
     * resources of the system, and these can overwhelm a system
     * particularly on a database switch, when potentially almost the
     * entire database can change at once.
     *
     * Note that it is not sufficient to use this flag on the wildcard
     * registration itself, or on its parent.  If you want to exclude
     * the entire subtree, you have to put this node on every
     * registered node in that subtree.
     *
     * Note that using this does not eliminate all traces of changes
     * from the dbchange events.  For every node registration that had
     * at least one instance changed, the change event will include
     * one entry of change type "unspecified".  This is much cheaper
     * than the default behavior, since it is only one entry (with no
     * attributes) regardless of how many instances of the
     * registration were changed.
     *
     * See also related flag ::mrf_delete_literal_no_notify.
     *
     * NOTE: we specify this as "1u << 31" to force the compiler think
     * this is an unsigned enum.
     *
     */
    mrf_change_no_notify    = 1u << 31,
 
#if 0
    mrf_src_registration    =  1 << ,
    mrf_src_db              =  1 << ,

    mrf_visi_normal         =  1 << ,
    mrf_visi_hidden         =  1 << ,
#endif

    /** \endcond */

    /* --------------------------------------------------------------------- */
    /** @name Common flag combinations
     *
     * These are the flags that you should apply to your node
     * registrations.  Flags are put in the mrn_node_reg_flags field
     * of the md_reg_node structure.  Generally your choice of flags
     * from the list below is restricted by the type of node you are
     * registering (configuration, monitoring, action, or event).
     */
    /*@{*/

    /**
     * Use when registering a literal configuration node.  Literal
     * means the last component of the binding name is a string
     * literal (not a "*").
     */
    mrf_flags_reg_config_literal = ( mrf_kind_config | mrf_rtype_binding |
                                     mrf_loc_internal | 
                                     mrf_maxaccess_rw | mrf_status_current | 
                                     mrf_type_literal | mrf_owner_local |
                                     mrf_async_blocking | mrf_exposure_normal ),

    /**
     * Same as ::mrf_flags_reg_config_literal, except also has
     * ::mrf_exposure_obfuscate and ::mrf_exposure_encrypt set.
     */
    mrf_flags_reg_config_literal_secure = ( mrf_flags_reg_config_literal |
                                            mrf_exposure_obfuscate |
                                            mrf_exposure_encrypt ),

    /**
     * Use when registering a wildcard configuration node.  Wildcard
     * means the last component of the name is a "*".
     */
    mrf_flags_reg_config_wildcard = ( mrf_kind_config | mrf_rtype_binding | 
                                      mrf_loc_internal | 
                                      mrf_maxaccess_rw | mrf_status_current | 
                                      mrf_type_wc | mrf_owner_local |
                                      mrf_async_blocking | 
                                      mrf_exposure_normal),

    /**
     * Use when registering a monitor literal to be served from this
     * mgmtd module.
     */
    mrf_flags_reg_monitor_literal = ( mrf_kind_state | mrf_rtype_binding | 
                                      mrf_loc_internal | 
                                      mrf_maxaccess_ro | mrf_status_current | 
                                      mrf_type_literal | mrf_owner_local |
                                      mrf_async_blocking | mrf_exposure_normal),

    /**
     * Use when registering a monitor wildcard to be served from this
     * mgmtd module.
     */
    mrf_flags_reg_monitor_wildcard = ( mrf_kind_state | mrf_rtype_binding | 
                                       mrf_loc_internal | 
                                       mrf_maxaccess_ro | mrf_status_current | 
                                       mrf_type_wc | mrf_owner_local |
                                       mrf_async_blocking | mrf_exposure_normal),

    /**
     * Use when registering a monitor literal to be served from an
     * external daemon.
     */
    mrf_flags_reg_monitor_ext_literal = ( mrf_kind_state | mrf_rtype_binding | 
                                          mrf_loc_external | 
                                          mrf_maxaccess_ro | 
                                          mrf_status_current | 
                                          mrf_type_literal | mrf_owner_local |
                                          mrf_async_blocking | mrf_exposure_normal),

    /**
     * Use when registering a monitor wildcard to be served from an
     * external daemon.
     */
    mrf_flags_reg_monitor_ext_wildcard = ( mrf_kind_state | mrf_rtype_binding |
                                           mrf_loc_external | 
                                           mrf_maxaccess_ro |
                                           mrf_status_current | 
                                           mrf_type_wc | mrf_owner_local |
                                           mrf_async_blocking | mrf_exposure_normal),

    /**
     * Use when registering a monitor literal to be served from an
     * external daemon for which queries should not be barriered by any
     * in process messages, and which should not cause a barrier to any
     * other message processing.
     */
    mrf_flags_reg_monitor_ext_literal_nonbarrier =
                                        ( (mrf_flags_reg_monitor_ext_literal & 
                                           (~mrf_async_MASK)) |
                                          mrf_async_nonbarrier),

    /**
     * Use when registering a monitor wildcard to be served from an
     * external daemon for which queries should not be barriered by any
     * in process messages, and which should not cause a barrier to any
     * other message processing.
     */
    mrf_flags_reg_monitor_ext_wildcard_nonbarrier =
                                        ( (mrf_flags_reg_monitor_ext_wildcard & 
                                           (~mrf_async_MASK)) |
                                          mrf_async_nonbarrier),

    /*
     * Use when registering notify-only nodes.
     * (XXX/EMT: need to expand docs to cover these!)
     */
    mrf_flags_reg_notify_literal = ( mrf_kind_state | mrf_rtype_binding | 
                                     mrf_loc_internal | 
                                     mrf_maxaccess_notify | 
                                     mrf_status_current | 
                                     mrf_type_literal | mrf_owner_local |
                                     mrf_async_blocking | mrf_exposure_normal),
    mrf_flags_reg_notify_wildcard = ( mrf_kind_state | mrf_rtype_binding | 
                                      mrf_loc_internal | 
                                      mrf_maxaccess_notify |
                                      mrf_status_current | 
                                      mrf_type_wc | mrf_owner_local |
                                      mrf_async_blocking | mrf_exposure_normal),

    /**
     * Use when registering action nodes.
     */
    mrf_flags_reg_action =          ( mrf_kind_state | mrf_rtype_action | 
                                      mrf_loc_internal | 
                                      mrf_maxaccess_exec | mrf_status_current |
                                      mrf_type_literal | mrf_owner_local |
                                      mrf_async_blocking | mrf_exposure_normal),

    /**
     * Use when registering event nodes.
     */
    mrf_flags_reg_event =           ( mrf_kind_state | mrf_rtype_event | 
                                      mrf_loc_internal | 
                                      mrf_maxaccess_notify | 
                                      mrf_status_current |
                                      mrf_type_literal | mrf_owner_local |
                                      mrf_async_blocking | mrf_exposure_normal),

    /*
     * Use when registering external providers who will provide nodes we do
     * not own, and as such do not have registration info for, like CMC or HA.
     */
    mrf_flags_reg_passthrough_ext_binding = ( mrf_kind_MASK | 
                                              mrf_rtype_binding |
                                              mrf_loc_external | 
                                              mrf_type_MASK |
                                              mrf_owner_remote |
                                              mrf_async_blocking |
                                              mrf_maxaccess_MASK | 
                                              mrf_status_current | 
                                              mrf_exposure_normal),

    mrf_flags_reg_passthrough_ext_event = ( mrf_kind_MASK | 
                                            mrf_rtype_event |
                                            mrf_loc_external | 
                                            mrf_type_MASK |
                                            mrf_owner_remote |
                                            mrf_async_blocking |
                                            mrf_maxaccess_MASK | 
                                            mrf_status_current | 
                                            mrf_exposure_normal),

    mrf_flags_reg_passthrough_ext_action = ( mrf_kind_MASK | 
                                             mrf_rtype_action |
                                             mrf_loc_external | 
                                             mrf_type_MASK |
                                             mrf_owner_remote |
                                             mrf_async_blocking |
                                             mrf_maxaccess_MASK | 
                                             mrf_status_current | 
                                             mrf_exposure_normal),

    /*@}*/

    /* XXX? in initial config? */
} mdm_node_reg_flags;


/* Configuration Node Change Auditing */

/**
 * Flags to specify how changes to this node should be converted into
 * a user-visible message for configuration change auditing purposes.
 *
 * (See mdm_action_audit_flags for action auditing)
 */
typedef enum {
    mnaf_none = 0,

    /**
     * Just use the node name in any configuration change log messages 
     * pertaining to the node, and do not complain about it.  Do not set
     * the mrn_audit_descr field in conjunction with this flag, as it
     * will be ignored.  This will result in the same log message as if
     * you simply did not set mrn_audit_descr, except that there will
     * be no complaint.  Use mainly for nodes which are never expected
     * to change, or which would only be changed "under the hood"
     * (e.g. during testing, by a field engineer, etc.)
     */
    mnaf_use_node_name = 1 << 0,

    /**
     * This node is a boolean type, and represents a flag for enabling
     * or disabling some functionality.  This alters the way change
     * messages for the node will be generated.  Instead of this:
     *
     *   &lt;OBJECT&gt;: &lt;FIELD&gt; changed from "true" to "false"
     *
     * we will generate a message like this:
     *
     *   &lt;OBJECT&gt;: &lt;FIELD&gt; changed from enabled to disabled
     *
     * Also, &lt;FIELD&gt; may be the empty string, which is appropriate if
     * the flag pertains to enabling or disabling the object itself.
     * Then, the message is composed in the same manner, except with the
     * extra space removed:
     *
     *   &lt;OBJECT&gt;: changed from enabled to disabled.
     */
    mnaf_enabled_flag = 1 << 1,

    /**
     * This node is a boolean type, and does NOT represent a flag for
     * enabling or disabling functionality.  It should be treated like
     * any other node, and have its values displayed as '"true"' and
     * '"false"', instead of 'enabled' and 'disabled'.  Using this
     * flag simply prevents the infrastructure from warning you that
     * there is a boolean node not marked with mnaf_enabled_flag.
     */
    mnaf_not_enabled_flag = 1 << 2,

    /**
     * Specifies that audit log message generation for this node is
     * handled by a global callback registered with
     * mdm_audit_register_handler().  The purpose of this flag is
     * simply to prevent the infrastructure from complaining that the
     * other registration fields are not filled out.
     */
    mnaf_handled_by_callback = 1 << 3,

    /**
     * Specifies that no log message should be generated for this
     * node, and no warning message should be logged at registration
     * time.  Normally should only be used on nodes whose changes are
     * always accompanied by changes to another node which is audited.
     * e.g. an instance of "/snmp/traps/event/ *" does not need logging
     * because we can get all the information we need from changes to
     * its child.
     */
    mnaf_suppress_logging = 1 << 4,

    /**
     * Specifies that the log message for this node should be suppressed
     * (as with ::mnaf_suppress_logging) unless at least one of the old
     * and new values is different from the registered initial value.
     *
     * Use this field with caution, since in most cases its result
     * could be confusing to the user, who would not necessarily know
     * why the field was sometimes mentioned and sometimes excluded
     * from audit messages, and might draw incorrect conclusions based
     * on these discrepancies.  This flag is mainly intended for
     * nodes which are not exposed in the UI, and so are expected to
     * retain their initial values unless someone looks under the hood
     * and changes them using a hidden command.
     */
    mnaf_suppress_logging_if_default = 1 << 5,

} mdm_node_audit_flags;

/** Bit field of ::mdm_node_audit_flags ORed together */
typedef uint32 mdm_node_audit_flags_bf;

/*
 * XXXX/EMT: mdm_node_audit_func: could provide a flag for literals
 * under wildcard nodes, where we would automatically include the
 * object's description to the prefix that goes before the message we
 * generate.
 */

/**
 * Custom callback to convert db change records into user-visible
 * descriptions of the changes.
 *
 * A function of this type may be called in one of two ways:
 *
 *   1. If the function pointer is provided in the registration of a
 *      mgmtd node, it will be called once for every change record for
 *      the node(s) that it was registered on.  It is implicit that the
 *      function is going to produce an audit message for this node, so
 *      it does not have to (and should not) call mdm_audit_consume_change()
 *      to report that it is consuming the node.  It should only append to
 *      inout_messages the message(s) that it wants to produce for the node.
 *      Usually, this will only be one message.
 *
 *   2. If the function pointer is passed in a call to 
 *      mdm_audit_register_handler(), it will be called once per commit.
 *      In addition to appending its results to inout_messages, it must
 *      report which changes it is consuming by calling
 *      mdm_audit_consume_change() on each change record it has taken
 *      care of producing messages for.
 *
 * \param commit Opaque context structure for this commit.
 *
 * \param old_db Configuration database before this change.
 *
 * \param new_db Configuration database after this change.
 *
 * \param changes List of all changes being made during this commit.
 *
 * \param change The particular change this function is being called to
 * make an auditing message for.  If this function pointer was registered
 * with mdm_audit_register_handler(), this will be NULL.
 *
 * \param max_messages_left This tells how many more messages we are
 * permitted to log, before hitting the configured maximum; or -1 if
 * there is no limit.  Callbacks are free to ignore this number if
 * they like; they can add any number of messages, and the
 * infrastructure will remove ones beyond the limit.  This is solely a
 * matter of efficiency -- if a callback might potentially generate a
 * lot of messages, it should check this number and bail out when it
 * hits the maximum, so as not to waste effort generating the rest.
 *
 * \param audit_cb_arg An opaque argument to be passed back as the
 * first parameter to mdm_audit_consume_change() if you call it.
 * This is only relevant if the audit function was registered with
 * mdm_audit_register_handler(), since otherwise, the changes you 
 * consume are implicit.
 *
 * \param arg The argument that was provided with this function
 * pointer, either in node registration, or in a call to
 * mdm_audit_register_handler().
 *
 * \retval inout_messages Messages that the function wants to be logged.
 * Each of these will have the standard prefix "Config change %s, item %d: "
 * prepended to it, so it should just be what comes after that.  This
 * array comes pre-allocated; the function should just append to it.
 */
typedef int (*mdm_node_audit_func)
    (md_commit *commit, const mdb_db *old_db, const mdb_db *new_db, 
     const mdb_db_change_array *changes, const mdb_db_change *change,
     int32 max_messages_left, void *audit_cb_arg, void *arg,
     tstr_array *inout_messages);

/**
 * Custom callback to render an attribute into a displayable value for
 * usage in a config change auditing log message.  While a
 * mdm_node_audit_func renders an entire log message, this function is
 * only used to render a single data value, and may be called twice
 * for a given change (if the node is being modified, so there is an
 * old value and a new value).  The results of this function will then
 * be used in constructing a standard audit message.
 *
 * \param commit Opaque context structure for this commit.
 * \param old_db Configuration database before this change.
 * \param new_db Configuration database after this change.
 * \param changes List of all changes being made during this commit.
 * \param change The particular change being processed now.
 * \param attrib The particular attribute you are to render into a value.
 * This will presumably be one of the attributes from the 'change' record.
 * \param arg The argument that was provided with this function
 * pointer in node registration.
 * 
 * \retval ret_value Your rendering of this attribute into a value.
 */
typedef int (*mdm_node_audit_value_func)
    (md_commit *commit, const mdb_db *old_db, const mdb_db *new_db, 
     const mdb_db_change_array *changes, const mdb_db_change *change,
     const bn_attrib *attrib, void *arg, tstring **ret_value);

/**
 * Register a handler to be called once per commit to scan the change
 * list and possibly produce audit messages describing some of the
 * changes.
 */
int mdm_audit_register_handler(md_module *module, 
                               mdm_node_audit_func audit_func, void *arg);

/**
 * To be called from a mdm_node_audit_func, to signal that it wishes
 * to consume the specified change record.
 *
 * NOTE: only call this from audit functions registered using
 * mdm_audit_register_handler().  If the audit function was registered
 * in a node registration, you should not call this, because the
 * infrastructure automatically assumes that you are going to consume
 * the change record for which you were called.
 *
 * \param audit_cb_arg The second-to-last parameter passed to your 
 * mdm_node_audit_func.
 *
 * \param change The change record which you wish to consume.
 */
int mdm_audit_consume_change(void *audit_cb_arg, const mdb_db_change *change);

/**
 * Request structure, containing additional parameters to an
 * ::mdm_node_acl_func (besides those passed directly to it).
 * Note that some or all of these parameters are provided only for
 * certain operations.
 */
typedef struct mdm_node_acl_request {

    /**
     * @name Global parameters.
     * These are always passed, regardless of operation type.
     */
    /*@{*/

    /**
     * Proposed operation on this node.
     *
     * For now, this will always be one of the following eight options:
     *   - ::tao_query_get
     *   - ::tao_query_iterate
     *   - ::tao_query_advanced
     *   - ::tao_set_create
     *   - ::tao_set_delete
     *   - ::tao_set_modify
     *   - ::tao_action
     *   - ::tao_traverse
     *
     * tao_query_get is passed when someone tries to fetch the value
     * of a single node of a specific name.  This includes wildcard
     * instances, whether they are named in the request or are
     * being fetched as part of an iterate.
     *
     * tao_query_iterate is passed on the name of the parent of the
     * wildcard node, when someone tries to iterate over that wildcard.
     * Note that to see the results of the iteration, they must also
     * pass tao_query_get on the wildcard instances themselves.
     *
     * tao_query_advanced is passed on the name of the node named in the
     * advanced query.  Note that query permissions are also enforced
     * on every sub-query done during the processing of the advanced
     * query, so advanced query permissions alone generally do not 
     * confer any additional visibility.
     *
     * The Set and Action operations are only passed when the operation is
     * intended to be performed on the very node on which this callback is
     * registered.  The Traverse operation is only passed when the
     * operation is intended to be performed on a descendant of the node
     * on which this callback is registered.
     *
     * Note that in the future, more granularity may be added in
     * controlling the type of set operations that can be performed,
     * e.g. which attributes can be set.  However, to avoid disturbing
     * existing modules, this field will never reflect that additional
     * granularity.  i.e. if a node is modified, this will always
     * contain tao_set_modify instead of something more specific.
     * The module can always consult mnaq_change for more details on a
     * specific set request.
     */
    ta_oper mnaq_oper;

    /*@}*/

    /**
     * @name Operation-specific parameters.
     * These parameters are only passed for certain operation types,
     * as described in the comment for each.  Note that there are no
     * additional parameters passed for the ::tao_traverse operation.
     */
    /*@{*/

    /**
     * Passed for any Set operation (::tao_set_create, ::tao_set_delete,
     * or ::tao_set_modify).  This tells the details of the proposed change
     * to the node.  Note that in the case of a Set operation, mnaq_oper is
     * redundant with this field, since it can be derived from the
     * mdc_change_type field of this structure.
     */
    const mdb_db_change *mnaq_set_change;

    /**
     * Passed for any Action operation (::tao_action).  These are the
     * bindings attached to the action request, if any.
     */
    const bn_binding_array *mnaq_action_bindings;

    /*@}*/

} mdm_node_acl_request;


/**
 * Response structure, containing fields which an ::mdm_node_acl_func can
 * fill out.
 *
 * XXX/EMT: one possible
 * addition would be to allow the function to consume other changes
 * besides the one it was called for, to increase efficiency in the
 * case of a commit involving several changes for many of which the
 * access control logic is the same.  For example, in md_passwd, we
 * want to only permit any operations on users who have a subset of 
 * the authorization groups of the acting user.  The determination is
 * solely based on the username of the user account being modified;
 * so if several fields of the same user are created or modified, we
 * don't need to do the same check again.
 */
typedef struct mdm_node_acl_response {

    /**
     * Should we permit this access?  If this is the final custom
     * callback to be called, this will be the authoritative answer.
     * If not, this will simply be fed as input to the next callback,
     * which may factor this into its decision, or ignore it.
     *
     * Note that when your function is called, this is initialized to
     * the same value as was found in the 'permitted' parameter to your 
     * function.  So to accept the judgement of the check preceding you
     * (a previous callback function, or the ACL itself), simply
     * leave this field unmodified.
     */
    tbool mnar_ok;

    /**
     * Custom error message to give user if permission is denied.
     * If left NULL, the user will get a generic error message,
     * like "Permission denied".  If set, this message will be 
     * appended, with proper punctuation to set it apart, e.g.
     * "Permission denied: <message>".
     *
     * Note that this field is only heeded if mnar_ok is false.
     * If permission is granted, this field is ignored.
     */
    char *mnar_msg;

} mdm_node_acl_response;


/**
 * Custom callback to determine accessibility of a node if ACLs are
 * enabled.
 *
 * A function of this type can be assigned to a node registration, and
 * will be called whenever we need to check accessibility of the node
 * OR ONE OF ITS DESCENDANTS.  Please note carefully how your callback
 * can expect to be called, particularly on nodes with descendants:
 *
 *   - First, the proposed operation is checked against the ACL
 *     registered on the node.
 *
 *   - Second, the result of that test is passed this function.  The function
 *     may change the determination if it wishes to.  If the function does
 *     nothing, then the result is unchanged.
 *
 *   - Next, all explicitly-registered ancestors of this node are checked,
 *     but the check is different for the node itself.  For each ancestor,
 *     we first check its ACL, and if any of three operations are permitted,
 *     that node passes: (a) the requested operation, (b) Query: Get,
 *     or (c) Traverse.
 *
 *   - After checking the ACL of a given ancestor, if it has a custom ACL
 *     callback, that is called.  Again, the result of the ACL on that
 *     same node is provided as input.  But instead of being called with
 *     the proposed operation, this custom callback is called with
 *     ::tao_traverse.
 *
 *   - Note that the result from each node is NOT passed upwards to the
 *     custom callbacks on ancestors, so there is no opportunity to
 *     override results from different nodes.  A custom callback may
 *     override the result from an ACL, or another custom callback on
 *     the same node only.  And in order for an operation to succeed,
 *     all nodes checked (i.e. the requested node, plus all of its
 *     explicitly-registered ancestors) must succeed.
 *
 * Therefore, a custom callback on any node with descendants which can
 * be the object of Set or Action requests can expect to be called with
 * ::tao_traverse (as well as with any operation appropriate to the
 * node itself).  However, if the ACL on your node already permit
 * Query: Get, Traverse, or the same operation as the descendant,
 * and if this is what you want, then you do not have to do anything
 * in this case.
 *
 * NOTE: you MUST NOT do any of the following from this callback:
 *   - Query external monitoring nodes.
 *   - Invoke external actions.
 *   - Invoke internal async/long-running actions.
 *   - Modify old_db or new_db (impossible anyway without a cast).
 *
 * That is, you MAY do any of the following:
 *   - Query config nodes.
 *   - Query internal monitoring nodes.
 *   - Invoke internal actions which run to completion.
 *
 * NOTE: this callback is called after basic infrastructure validation,
 * such as that the data type is correct, etc.  However, no check function
 * have run yet, so be careful about what assumptions you make.
 *
 * \param commit Opaque context structure for this commit
 *
 * \param old_db Old database before change, if this is a Set request.
 * If this is any other kind of request (which will not alter the
 * database), this will be the same as new_db.
 *
 * \param new_db New proposed database, as it would be after the change
 * in progress, if this is a Set request.  If this is any other kind of
 * request, this will be the same as old_db.
 *
 * \param node_name The name of the node which is proposed to be
 * operated on.  Note that this is the actual node name (with specific
 * wildcard instances, if any wildcards are involved), NOT just the
 * registration name.
 *
 * \param node_name_parts The same name as passed in 'node_name', except
 * broken down into component parts for easier access.
 *
 * \param reg_name The name of the registration node on which this
 * callback function was registered (i.e. whatever was in mrn_name).
 * Note that this may not be the registration which matches node_name,
 * if you are being called because a descendant is being operated on.
 *
 * \param permitted Indicates whether this operation was permitted by
 * whatever checks came before the callback now being called.  ACL checks
 * are done in series, with the answer from one being passed as input
 * to the next.  The last check is always authoritative, and can
 * override positive or negative determinations by any of the
 * previous checks.  The order of checks is:
 *   1. The ACL registered on the node.
 *   2. The custom ACL callback registered on the node, if any.
 *   3. The custom ACL callback given to override the node registration,
 *      if any.
 *
 * \param req Additional parameters to this function.
 *
 * \param data The data provided in the ::mrn_acl_data field 
 * of the md_reg_node structure where this function was provided.
 *
 * \retval ret_response A structure containing the function's response.
 * See this structure definition for clarification on individual fields.
 */
typedef int (*mdm_node_acl_func)
    (md_commit *commit, const mdb_db *old_db, const mdb_db *new_db,
     const char *node_name, const tstr_array *node_name_parts, 
     const char *reg_name, tbool permitted, const mdm_node_acl_request *req,
     void *data, mdm_node_acl_response *ret_response);

/**
 * A custom ACL function (conforming to ::mdm_node_acl_func) which
 * does nothing.  It just accepts whatever default answer is passed to
 * it, and never changes the outcome.
 *
 * Using this function has basically the same effect as having no
 * function at all (mrn_acl_func set to NULL).  A case where you might
 * want this function is if your node is set to inherit a custom ACL
 * function from an ancestor, but yuo want to override it to have no
 * function.  Setting mrn_acl_func to NULL will not accomplish this,
 * because that means to accept the inherited value.  Set it to this
 * instead.
 */
int mdm_node_acl_func_noop(md_commit *commit, const mdb_db *old_db, 
                           const mdb_db *new_db, const char *node_name,
                           const tstr_array *node_name_parts, 
                           const char *reg_name, tbool permitted,
                           const mdm_node_acl_request *req, void *data, 
                           mdm_node_acl_response *ret_response);


/* Action Auditing */

/**
 * Action registration audit flags specify how auditing will be handled for
 * Automatic Action Auditing, and how custom Audit Callback functions are
 * expected to handle their audit logging.
 *
 * The action auditing feature is only available when built with the
 * PROD_FEATURE_ACTION_AUDITING make flag enabled. Action Audit messages are
 * intended for an end user audience, and are meant to contain user-friendly
 * messages.  Audit messages should not be confused with the internal ACTION
 * log messsages that are always generated when actions are executed, and which
 * contain raw presentations of node and binding data.  The latter will
 * continue to be logged in parallel even when Action Auditing is in force.
 *
 * If PROD_FEATURE_ACTION_AUDITING is enabled in the build, it is expected that
 * all action nodes will meet minimal audit registration requirements, such as
 * setting the mrn_action_audit_descr or appropriate mrn_action_audit_flags,
 * and nodes that do not will trigger complaints at registration time.
 *
 * Action nodes that do not have explicit registrations will still be logged
 * under Automatic Action Auditing, but audit messsages will follow the
 * behavior specified under the maaf_none setting.
 *
 * Note that PROD_FEATURE_ACCOUNTING forwards action audit messages to an
 * accounting server, and PROD_FEATURE_ACTION_AUDITING, which only enables
 * audit messages in local logs, is not required for that to happen.  However,
 * if PROD_FEATURE_ACCOUNTING is enabled, audit node registration requirements
 * will be enforced.
 *
 * Action Audit messages are generated at two points, once after the request is
 * validated, just prior to invoking the action handler, and a second time
 * when the action has completed.  At the request stage, the action description
 * is logged, along with a unique action audit number, and any bindings
 * eligible for logging.  At the completion stage, a log message including the
 * same audit number reports success or failure.  Currently, custom callbacks
 * for audit logging are supported only at the action request stage.  See the
 * mdm_action_audit_func type for more information on custom log messages.
 */
typedef enum {
    /**
     * This is the default for nodes that have not been registered
     * with any special audit flags.  This setting is appropriate for Automatic
     * Action Auditing of nodes that provide a mrn_action_audit_descr for the
     * action and mra_param_audit_descr for all displayable bindings, or for
     * nodes that handle audit logging by defining mrn_action_audit_func
     * callback.
     *
     * If the node does not have sufficient directives or a custom callback,
     * then the system will log a complaint at registration time.
     *
     * Nodes that lack sufficient audit handling directives will still be
     * logged under Automatic Action Auditing rules, however the action
     * description will default to the action's node name, and action request
     * parameters will default to using raw binding names and values, wherever
     * user-friendly descriptions are missing.
     *
     * Note that if the node is registered as mrf_exposure_sensitive,
     * the bindings will normally be obfuscated in the Audit Log.
     * This may be overridden with the
     * maaf_bindings_default_sensitivity_override flag, which makes the
     * bindings logged as normal (unless overridden by per-binding
     * registration flags).
     */
    maaf_none = 0,

    /**
     * Forces the use of node and binding names in the action audit message and
     * don't complain at registration time about the lack of user-friendly
     * audit handling.  When this flag is set, the mrn_action_audit_descr and
     * mra_param_audit_descr registration fields should not be set, as they
     * will be ignored.  This flag forces the same audit log message that is
     * generated for nodes that do not specify an audit description or callback
     * function, but setting this flag will prevent complaints at node
     * registration time.  This setting is primarily intended for actions such
     * as those in hidden and diagnostic commands that should be audited but
     * are not intended for general users. 
     */
    maaf_use_node_name = 1 << 0,

    /**
     * Specifies that no audit log message should be generated for this
     * action node, and no warning message should be logged at registration
     * time.  Note that action failure will still be logged at INFO level
     * unless maaf_suppress_unaudited_failure_logging is also specified.
     */
    maaf_suppress_logging = 1 << 2,

    /**
     * Suppress logging as in maaf_suppress_logging if this action
     * invocation does not have an interactive (logged in) user associated
     * with it.  This can be useful for squelching actions that are issued
     * by automated system processes, and actions that are triggered
     * indirectly by other actions.  Note that action failure will still be
     * logged at INFO level unless maaf_suppress_unaudited_failure_logging
     * is also specified.
     */
    maaf_suppress_logging_if_non_interactive = 1 << 3,

    /**
     * Specifies that the action's request binding parameters should not
     * inherit log obfuscation behavior by default when the action node is
     * marked mrf_exposure_sensitive.  Instead, binding parameters will
     * be logged as normal, unless the maba_suppress_logging or 
     * maba_obfuscate_logging flag is used.  This allows the registrant to 
     * specify explicitly which bindings are actually sensitive, while 
     * allowing the remaining bindings to be logged.
     */
    maaf_bindings_default_sensitivity_override = 1 << 4,

    /**
     * If the action node is marked mrf_exposure_sensitive, log any
     * response msg.  Without this flag specified, the responses from
     * actions marked mrf_exposure_sensitive are not logged.  By default
     * responses from actions are logged.
     */
    maaf_response_msg_override_not_sensitive = 1 << 5,

    /**
     * The action response message is sensitive and should not be
     * logged, even if the action is not marked mrf_exposure_sensitive.
     */
    maaf_response_msg_override_sensitive = 1 << 6, 

    /**
     * Specifies that the action response message for this action does
     * not represent completion of the underlying operation.
     *
     * Currently, the effect of this flag is that when we receive the
     * response message, instead of reporting success or failure, we
     * log a different message saying the action has been initiated.
     * Nothing is logged by the auditing infrastruture when the operation
     * is complete, because that is not known.
     *
     * The code handling this action may wish to log something
     * independently reporting its completion.  In the future, there
     * may be a way to notify the action auditing infrastructure of this,
     * so it can log an official message (with the same "Action ID ..."
     * prefix) on completion.
     */
    maaf_response_not_completion = 1 << 7,

    /**
     * Use this when the action invokes database changes which return
     * both commit and apply codes, and if the apply codes should NOT be
     * interpreted as overall action failure.
     *
     * Action return status, like database commit status, is set using
     * md_commit_set_return_status_msg(), or
     * md_commit_set_return_status_msg_fmt().
     * 
     * During a database commit, these functions set one of two independent 16
     * bit error codes, one for the commit phase, and one for the apply phase.
     * Apply error codes are non-fatal to database commits, and are normally
     * viewed as advisory in nature.
     *
     * However, unless this flag is set, the action will report general failure
     * even if the failure occurs only in the apply phase.  This flag will
     * cause the action to report success, but the apply failure message will
     * still appear in the audit log.
     */
    maaf_return_status_is_from_commit = 1 << 8,

    /**
     * An unaudited action is any action that is subject to audit suppression,
     * whether conditional or unconditional (see maaf_suppress_logging*).
     *
     * Unaudited action successes do not appear in the system logs in any form
     * except at DEBUG level.  By default, however, failures will result in
     * an "Unaudited action failure" message being issued at INFO level.
     *
     * While unaudited actions are intended to remain behind the scenes,
     * failures need some level of visibility when there is a real problem.
     * Failures are often better handled at a higher level by the entity
     * responsible for the action.  But unless care is taken to implement
     * error reporting from the originating entity, problems may go unnoticed
     * or be hard to track down.
     *
     * This message is issued on action completion.  Suppressed actions do
     * not have an audit number, nor are the original action parameters
     * available for logging at that stage.  However the message will
     * include the mrn_action_audit_descr if defined, as well as mrn_name.
     *
     * If the calling entity already takes responsibility for logging
     * failure conditions, this message may be lowered to DEBUG level by
     * setting this flag.
     */

    maaf_suppress_unaudited_failure_logging = 1 << 9,

    /*
     * XXX/SML:  we do not provide a distinct 'maaf_action_handled_by_callback'
     * flag because we use the presence of an mrn_action_audit_func pointer to
     * flag the use of custom logging.  If NULL, we use automatic auditing. 
     */

} mdm_action_audit_flags;

/** Bit field of ::mdm_action_audit_flags ORed together */
typedef uint32 mdm_action_audit_flags_bf;

/**
 * For Automatic Action Audit Handling (i.e. not handled through a
 * mrn_action_audit_func callback function), action parameter descriptions and
 * values are logged by default.  While the inclusion of an action's request
 * parameters may be suppressed for the entire action node using the
 * maaf_suppress_logging flag, these flags provide logging directives on a per
 * binding basis.  These flags should also be honored by Action Audit Callback
 * functions, however that remains at the purview of the implementation and
 * cannot be enforced.
 */
typedef enum {
     /**
      * This flag is the default setting for bindings that do not have specific
      * handling flags.  By default, bindings that are not sensitive due to
      * inherited node level sensitivity will be included in Automatic
      * Action Audit message handling.  Action Audit Callbacks should use this
      * flag when considering how to handle the parameter, but they should also
      * check mdm_action_audit_flags and mdm_node_reg_flags for appropriate
      * inherited behavior(s).
      *
      * For callbacks, handling remains at the purview of the callback
      * implementation, so there is no guarantee that the binding will be
      * included in the custom callback message, but handlers are expected to
      * in particular to honor inherited sensitivity settings.
      */
    maba_none = 0,

    /**
     * Use this flag to exclude a binding from Automatic Audit Logging even
     * though it might not be sensitive.  This flag needs to be employed for
     * particular bindings that should be suppressed under a sensitive node
     * when the maaf_bindings_default_sensitivity_override flag has been used
     * to disable suppression of bindings from the log under a sensitive node.
     */
    maba_suppress_logging = 1 << 0,

    /**
     * Use this flag to request that a sensitive action binding's value be
     * displayed in an obfuscated fashion.
     */
    maba_obfuscate_logging = 1 << 1,

    /**
     * Obfuscate only the user's password in a URL, i.e. a substring of
     * the form:
     *
     *      username:pw\@host
     *
     * will appear as:
     *
     *      username:********\@host
     *
     */
    maba_obfuscate_url_password_only = 1 << 2,

} mdm_action_binding_audit_flags;

/** Bit field of ::mdm_action_binding_audit_flags ORed together */
typedef uint32 mdm_action_binding_audit_flags_bf;


/**
 * Directives that control audit loging behavior for this action.
 * This structure is passed to custom action node callbacks and is subject to
 * growth, so that when new features are added there should be no need to
 * alter the mdm_action_audit_func API.
 *
 * Data in these directives is honored by Automatic Action Auditing, and
 * callbacks are also expected to honor it whenever relevant.
 */
typedef struct mdm_action_audit_directives {
    /**
     * The user-friendly description of the action for audit messages, that was
     * provided through mrn_action_audit_descr at node registration time.
     */
    const char *maad_action_descr;

    /**
     * Node level flags for action audit behavior that were provided through
     * mrn_action_audit_flags at node registration time.
     */
    mdm_action_audit_flags_bf maad_audit_flags;
} mdm_action_audit_directives;


typedef struct mdm_reg_action mdm_reg_action;


/**
 * Custom Action Node Audit Callback Function
 *
 * The following callback may be defined to provide custom action request
 * audit log records for Action Auditing.
 *
 * Before writing a custom callback, be sure to determine whether Automatic
 * Action Auditing features will suffice.  This callback is used to handle
 * auditing of the entire action node.  It might be also sufficient to write
 * mdm_action_param_audit_value_func callbacks for individual parameters,
 * rather than writing a callback for the entire action.
 *
 * The audit function is invoked after the action has been validated in terms
 * of user capabilities and legal request parameters, but just prior to
 * invocation of the action's registered Action Function Handler.
 *
 * When the action request is complete, an automatic audit log message is
 * issued by the system to report success or failure. This occurs after the
 * action request has completed, just prior to transmission of the GCL
 * response message to the client. There is no custom audit callback for that
 * stage at this time.
 *
 * The completion status audit log message will contain the Standard Action
 * Audit Messsage Prefix described below, so that request / response audit
 * messages may be associated properly in the logs.  This is especially
 * important for asynchronous transactions, which may complete long after the
 * original request, and it ensures both human and programmatic ability to
 * associate log messages for the same action.  For actions that complete after
 * a response, see maaf_response_not_completion in mdm_action_audit_flags. 
 *
 * Callbacks must log their action audit messages by calling one of the
 * following functions (see md_audit.h):
 *
 *     - md_audit_action_add_line() Add a fully formatted audit log message
 *     - md_audit_action_add_line_fmt() Add an audit log message using a
 *     printf-style format string
 *
 * Refer to the documentation of these functions for details.
 *
 * The infrastructure will augment these lines with common audit data.
 * The following Standard Action Audit Message Prefix will appear in every
 * audit message line, where parameters are denoted within curly braces,
 * and all other characters are literal:
 *
 * Action ID #{action_audit_num}: {callback_message}
 * 
 * The request completion automatic response message will be of the form:
 *
 * Action ID #{action_audit_num}: response: {success|failure{: {failure-message}}}
 *
 * where {callback_message} is a line returned in the inout_messages array,
 * and {action_audit_num} is a unique audit transaction ID asssigned by the
 * system.  Note that action_audit_num starts at 1 when the system is booted,
 * and is only unique for the duration of a single mgmtd life cycle.  On
 * failure, the GLC response message returned through
 * md_commit_set_return_status_msg is appended as {failure-message} above.  If
 * the module's failure response contains sensitive data such as a password,
 * you may squelch failure message contents with the
 * maaf_response_msg_override_sensitive action node registration flag.
 *
 * For consistency of format with Automatic Action Auditing, callbacks
 * generally should include the maad_action_descr in the first callback
 * message line, followed by any parameter information.
 *
 * \param commit Opaque context structure for this commit action.
 *
 * \param db The current configuration database.
 *
 * \param action_node_name The full node name for the requested action.
 *
 * \param req_bindings The list of bindings (action parameters) in the request.
 *
 * \param node_reg_flags Node registration flags from mrn_node_reg_flags.  Some
 * settings such as sensitivity flags (mrf_exposure_sensitive) are relevant to
 * auditing behavior.  See mdm_action_audit_flags for more information on how
 * general node registration flags and specific audit directives are related.
 *
 * \param audit_directives A collection of registration time directives that
 * includes a user-friendly description of the action as well as audit logging
 * behavioral flags that should be honored by the callback, and possible future
 * fields.  See mdm_action_audit_directives for details.
 *
 * \param action_parameters The registration time definition of this action's
 * parameters as provided to mdm_add_node in node->mrn_actions[].  Note that
 * while there are several alternative fields that may be used to register a
 * parameter's static default value, only mra_param_default_value_bn_attrib
 * will contain the default value, so *do not* reference the fields
 * mra_param_default_value_str, mra_param_default_value_integer or
 * mra_param_default_value_uint64.  Also note that action_parameters are
 * destroyed after the callback returns, so do not hang on to any reference to
 * this array.  The display of parameter attribute values may be made "pretty"
 * through the md_audit_log_render_value_as_str() function, which is also used
 * by Automatic Audit Logging.
 *
 * \param action_param_count The number of action parameters defined in
 * node->mrn_actions[] (as specified when calling mdm_new_action).
 *
 * \param arg An argument optionally provided at node registration time in the
 * mrn_action_audit_func_arg member of md_reg_node.
 * 
 * \param ctxt An opaque variable supplied by the infrastructure in the call
 * to the mdm_action_audit_func.  This parameter must be passed in calls to
 * the audit log functions md_audit_action_add_line() and
 * md_audit_action_add_line_fmt() (see md_audit.h).
 */
typedef int (*mdm_action_audit_func)
    (md_commit *commit, const mdb_db *db, const char *action_node_name,
     const bn_binding_array *req_bindings, uint32 node_reg_flags,
     const mdm_action_audit_directives *audit_directives,
     const mdm_reg_action *action_parameters[], uint32 action_param_count,
     void *arg, mdm_action_audit_context *ctxt);


/**
 * Custom callback to render an attribute into a displayable value for
 * usage in an action auditing log message.  While a
 * mdm_action_audit_func renders an entire log message, this function is
 * only used to render a single data value for a binding parameter when the
 * native string representation of the bindings value returned by
 * bn_binding_get_tstr will not suffice.  The value returned by this function
 * will be used in its place.  In this way, standard audit messaging may be
 * preserved in all other aspects rather than having to write an entire custom
 * mdm_action_audit_func.
 *
 * Note that parameter callback functions operate under Automatic Action
 * Auditing, but if an mrn_action_audit_func is also defined, it may invoke
 * the mdm_action_param_audit_value_func, but the infrastucture will not do so.
 *
 * \param commit Opaque context structure for this commit.
 * \param db The running configuration database.
 * \param action_name The name of the action node for this parameter.
 * \param param_name The binding node name for this parameter.
 * \param param_attrib The action parameter attribute to be rendered into
 * a custom display value.
 * \param arg The argument that was provided with this function
 * pointer in node registration.
 * 
 * \retval ret_value Your rendering of this attribute's value.
 * \retval ret_suppress Set this to true if you wish to suppress this parameter
 * from the audit logs altogether, as if it had been marked with the
 * maba_suppress_logging flag.  In this case, you may return NULL for
 * ret_value.
 */
typedef int (*mdm_action_param_audit_value_func)
    (md_commit *commit, const mdb_db *db, const char *action_name,
     const char *param_name, const bn_attrib *param_attrib,
     void *arg, tstring **ret_value, tbool *ret_suppress);


/******************************************************************************
 ******************************************************************************
 ** Node registration structures
 **
 ** These are structures which you will out and then pass to the mgmtd
 ** infrastructure in order to register nodes.  The main one is
 ** md_reg_node, below.
 ******************************************************************************
 ******************************************************************************
 **/

/**
 * A structure representing one binding which is expected to be passed
 * with a particular action request.  If you're registering an action
 * node, your mrn_actions field will have an array of these.
 */
struct mdm_reg_action {
    /**
     * Binding name for this action parameter
     *
     * NOTE:  Action parameter names containing wildcards are not validated
     * against client requests, and are therefore *excluded* from system level
     * handling such as type checking, required parameter check, and
     * and default value support.  See bug 13345 for more information.
     */
    const char *mra_param_name;

    /**
     * Binding type for this action parameter
     *
     * NOTE:  The type bt_any may be used to allow a client to pass an
     * arbitrary parameter type in an action request.  No type checking will be
     * performed against client bindings.defaults will be ignored.  The
     * mra_param_required is still enforced.  A default value may of course be
     * generated by the action handling code if needed.
     */
    bn_type mra_param_type;

    /**
     * Whether this is a required parameter for the action handler.
     *
     * This defaults to false to support legacy parameter handling that
     * predates the existence of this field.  If a parameter is marked as
     * required, the infrastructure will reject any action request that does
     * not include this parameter.  If not required, a default value may
     * optionally be defined.  See below.
     */
    tbool mra_param_required;

    /**
     * @name Parameter Default Value Handling
     *
     * Parameters that are not required may be defined with a default value
     * using one of the methods below.  If a default value is defined for an
     * optional parameter, and the client omits this parameter, the
     * infrastructure will add the missing parameter and assign it the
     * specified default value prior to invoking the action handler.
     *
     * A handler should therefore perform a sanity check and should bail_null
     * if an optional parameter defined with a default value is found missing
     * from the action handler's callback parameter list.  Furthermore, an
     * action handler should never attempt to assign a default value on its own
     * if one is defined at registration time.  Handlers that need to assign
     * default values for optional parameters dynamically may do so by leaving
     * the registration default value for such parameters undefined (NULL).
     *
     * There are two levels of default value:  dynamic from a node and static.
     * These two levels may co-exist, and a static default value, if
     * available, will be used as the fallback value in the event that the
     * node specified to contain the default value is not in the database.
     *
     * A static default value may be defined using one of the
     * mutually-exclusive API formats below.  If more than one method is
     * defined at registration time, the registration will be rejected with an
     * error.  Range checking is enforced based on the mra_param_type.
     */
    /*@{*/

    /**
     * Static default value in string format.  This form of default value
     * supports registration time specification for any supported binding
     * type.  If the string value submitted is inappropriate for this
     * parameter's binding type, an error will be logged at registration time.
     * If the default value is an integer, or an integer expression, it is
     * recommended that you use mra_param_default_value_integer or
     * mra_param_default_value_uint64 instead.  For floats, binary buffers, or
     * other types not easily supported in a string, you may alternatively use
     * mra_param_default_value_bn_attrib to create the attribute directly.
     */
    const char *mra_param_default_value_str;

    /**
     * Static default value for general integer format.
     *
     * This form of default value supports registration time specification
     * of all signed or unsigned integers types, except for uint64, directly
     * from an integer or integer expression.  Use
     * mra_param_default_value_uint64 for the bt_uint64 type.
     *
     * While mra_param_default_value_str could also be used for numbers, this
     * method is more natural and is preferred for readability.
     *
     * This field defaults to INT64_MIN prior to being set by the registrant,
     * so this reserved value is equivalent to not specifying a general integer
     * format default value.  Use an alternate method if this value is needed.
     */
    int64 mra_param_default_value_integer;

    /**
     * Static default value in uint64 integer format.
     *
     * This is method is similar to mra_param_default_value_integer but is
     * accepted only for bt_uint64 types.  Use mra_param_default_value_integer
     * for all other (i.e. smaller) unsigned integer types.
     *
     * This field defaults to UINT64_MAX prior to being set by the registrant,
     * so this reserved value is equivalent to not specifying a uint64 format
     * default value.  Use an alternate method if this value is needed.
     */
    uint64 mra_param_default_value_uint64;

    /**
     * Static default value in bn_attrib format.
     *
     * This method may be used to specify the static default value in any
     * format not covered by the methods above, such as a uint64 that needs to
     * include UINT64_MAX in its valid range, an integer value that includes
     * INT64_MIN, or a binary buffer.  A copy of this attribute will be made
     * upon calling mdm_add_node, so the caller is responsible for freeing any
     * memory allocated for the registration process.
     */
    const bn_attrib *mra_param_default_value_bn_attrib;

    /**
     * Dynamic default value from a node.
     *
     * This form of default value supports the asssignment of a value
     * dynamically from a configuration or state node at action request time.
     * This will take precedence over the mra_param_default_value_str or
     * mra_param_default_value_uint64 (if defined), however if the node
     * does not exist, a registration time static default value will
     * automatically be used as a fallback if one is defined;  otherwise the
     * action handler may either log an error or provide its own fallback
     * default value.
     *
     * IMPORTANT:  DO NOT reference an external monitoring node, as
     * these queries can result in failure under various circumstances.
     */
    const char *mra_param_default_value_node;

    /*@}*/

    /**
     * @name Parameter Action Audit Handling
     *
     * This feature is only active when built with the
     * PROD_FEATURE_ACTION_AUDITING make flag enabled.  When enabled,
     * action parameters are logged along with the action unless otherwise
     * supressed due to inherited action node sensitivity or due to
     * per-parameter settings specified in mra_param_audit_flags.  See
     * mdm_action_binding_audit_flags for details.  Actions should
     * define mra_param_audit_descr at registration time to provide a user
     * friendly description of the parameter for the log message.  If the
     * system's automatic message format will not suffice, a callback may be
     * defined in the mrn_action_audit_func registration parameter for custom
     * action node audit logging.  See mrn_action_audit_flags for node
     * level control of action audit logging.
     */
    /*@{*/

    /**
     * User-friendly action parameter description.
     *
     * This value will be used as the parameter description in any action audit
     * log message that includes this parameter.  NOTE: If audit messages are
     * handled by a custom mdm_action_audit_func, this value may be honored or
     * ignored.  This remains at the purview of the callback implementation.
     * Under Automatic Action Auditing, if this parameter is loggable 
     * and mra_param_audit_descr is NULL, a registration time complaint
     * will result, and the mra_param_name string will be used in its place,
     * unless the maaf_use_node_name flag is set to suppress the complaint.
     */
    const char *mra_param_audit_descr;

    /**
     * Action parameter (binding) audit flags
     *
     * Flags that control whether and how this action binding
     * should be logged in the audit log.  See mdm_action_binding_audit_flags
     * for details.  NOTE: for an mrn_action_audit_func, honoring action audit
     * flags remains soley at the purview of the callback implementation, so
     * while it is recommended that callbacks support this flag, there should
     * be no such expectation of this without inspecting the callback source
     * code, however a mra_param_audit_value_func callback will not affect
     * these flags since they are enforced by the infrastructure outside of
     * mra_param_audit_value_func callback invocation.
     */
    mdm_action_binding_audit_flags_bf mra_param_audit_flags;

    /**
     * Action parameter (binding) audit value display flags
     *
     * Flags that control the display format for this parameter in the audit
     * log.  These flags impact the display of only certain binding types, and
     * they are honored by the function md_audit_log_render_value_as_str, which
     * is called under Automatic Action Auditing, and may also be called by a
     * mdm_action_param_audit_value_func to format a value.
     */
    md_audit_value_display_flags_bf mra_param_audit_display_flags;

    /**
     * Action parameter audit value rendering callback function
     *
     * Define this function only if the system's string representation of the
     * binding value is insufficient for display.  If not defined, the value
     * displayed will be the result returned by the function
     * md_audit_log_render_value_as_str(), which is called to honor the
     * mra_param_audit_display_flags.  Note that the mra_param_audit_value_func
     * is not invoked by the system if a mrn_action_audit_func has been defined
     * to handle audit logging for the node, so in that case honoring it is at
     * the purview of the mrn_action_audit_func callback implementation.
     */
    mdm_action_param_audit_value_func mra_param_audit_value_func;

    /**
     * Action parameter audit value rendering callback argument
     *
     * Optional registration time data to be passed to
     * the mra_param_audit_value_func.
     */
    void *mra_param_audit_value_func_arg;

    /**
     * An optional string to be appended to the value for display in audit
     * logging, e.g. to tell what units the value is in.  If defined, the
     * suffix is appended after calling a custom mra_param_audit_value_func_arg
     * (if defined), or during Automatic Audit Logging value rendering, but
     * custom node level callback functions that handle parameters on their own
     * will need honor the suffix independently, e.g. by passing it in a call
     * to md_audit_log_render_value_as_str, or by appending it separately.
     *
     * Note that a space is NOT automatically added between the value and the
     * suffix, so the suffix should start with a space if you want one.
     */
    const char *mra_param_audit_value_suffix;

    /*@}*/
};


/**
 * A structure representing one binding (or pattern of binding name)
 * which is expected to be passed with a particular event request.
 * If mgmtd receives an event, it will ensure all the required
 * bindings are present in any forwarding of the event.
 * 
 * Note that the 'mre_binding_name' field is a binding name pattern,
 * where components of the binding name may be '*'s.  This matches
 * any string for that component of the name.  The bindings in the
 * event must include at least one which matches this pattern.
 *
 * If the 'mre_binding_name' is an absolute name (starts with a '/'), it
 * must be the name of a registered db node.  'mre_binding_type' can
 * optionally be used to specify the type the binding must have for non-
 * registered nodes, and to override the type for registered nodes.
 */
typedef struct mdm_reg_event {
    const char *mre_binding_name;
    bn_type mre_binding_type;

#if 0
    tbool mre_binding_required;
#endif

} mdm_reg_event;

/* ------------------------------------------------------------------------ */
/** This is the structure you fill out to register any kind of node in
 * the mgmtd hierarchy.  Don't create an empty structure yourself;
 * call mdm_new_node() for a config or monitoring node;
 * mdm_new_action() for an action node; and mdm_new_event() for an
 * event node.  Then fill out the structure and pass it to
 * mdm_add_node().
 */
typedef struct md_reg_node {

    /** @name Fields common to all nodes */

    /*@{*/

    /**
     * The name of your node in the mgmtd hierarchy.  This is a
     * hierarchical name with components separated by the '/'
     * character.  This must be an absolute node name, meaning that it
     * must start with a '/'.  (In other contexts, one might specify a
     * node name relative to another node, in which case it would not
     * start with a '/', just like absolute and relative pathnames in
     * a filesystem.)
     *
     * Unless you specified modrf_namespace_unrestricted in your call
     * to mdm_add_module(), this name must be underneath the module's
     * root node (or config root node, if specified) that you
     * specified in your call to the same function.
     */
    const char *mrn_name;

    /**
     * Bit field containing flags for this node.  Should be a set of
     * flags from mdm_node_reg_flags ORed together, though generally
     * you will use the premade combinations like
     * mrf_flags_reg_config_literal, rather than forming your own
     * combinations.
     */ 
    uint32 mrn_node_reg_flags;

    /**
     * Bit field expressing what capabilities are required to perform
     * what sorts of requests on this node.  Should be a set of flags
     * from mdm_capabilities ORed together.
     *
     * Only enforced if PROD_FEATURE_CAPABS is enabled.
     * This feature is enabled by default.
     */
    uint32 mrn_cap_mask;

    /**
     * Access Control List, expressing access control restrictions on
     * this node.
     *
     * NOTE: you may not assign an ACL from tacl.h to this directly
     * (and the compiler will enforce this because of the 'const').
     * Those are reusable ACLs, whereas this field is specific to this
     * node.  Use mdm_node_acl_add() to build this.
     *
     * Only enforced if PROD_FEATURE_ACLS is enabled.
     * This feature is disabled by default.
     */
    ta_acl *mrn_acl;

    /**
     * ACLs which are only for this node alone, and independent of the 
     * ACL inheritance system (the ::mrf_acl_inheritable flag).
     * This means they are different from the ACLs in mrn_acl in
     * two ways:
     *
     *   1. They are not to be be passed on to descendant nodes if this
     *      node has the inheritable flag set.
     *
     *   2. They do not disqualify the node from inheriting from an
     *      ancestor.
     *
     * After all nodes are done being registered, inheritance is
     * honored, and any inheritable ACLs from mrn_acl are propagated
     * downwards to any registered nodes which do not have their own
     * ACLs in mrn_acl.  After this, ACLs from mrn_acl_local are
     * appended to mrn_acl in each node.
     *
     * NOTE: as with mrn_acl, you may not assign ACLs directly here.
     * Use mdm_node_acl_add_local() to build this.
     *
     * Only enforced if PROD_FEATURE_ACLS is enabled.
     * This feature is disabled by default.
     */
    ta_acl *mrn_acl_local;

    /**
     * A function to be called to determine access to this node, if
     * PROD_FEATURE_ACLS is enabled.  If this function is provided,
     * accessibility will still first be determined according to the 
     * registered ACL; the result of this determination is then passed
     * as one of the inputs to this function.
     *
     * Note that ACL functions can be inherited if the
     * ::mrf_acl_inheritable flag is used on an ancestor.  Setting
     * this field overrides any inherited ACL function (and its
     * associated mrn_acl_data), but does NOT automatically override
     * the ACL in mrn_acl.
     */
    mdm_node_acl_func mrn_acl_func;

    /**
     * Data to be passed to ::mrn_acl_func.
     */
    void *mrn_acl_data;

    /**
     * A comment describing the node.  It is not currently used for
     * anything.
     */
    const char *mrn_description;

    /**
     * This field is set automatically by the infrastructure;
     * your module SHOULD NOT CHANGE IT.
     */
    mdm_node_reg_flags mrn_new_rtype;

    /**
     * Currently for external action nodes only, but could apply to
     * other types of nodes in the future.  Controls how long mgmtd
     * will wait for a reply from the external provider, before timing
     * out and sending back its own response to the client.
     *
     * By default, this has the value 0, which means to use mgmtd's
     * default, which is defined as md_waitfor_timeout_async in
     * md_client.h.  So you only need to set this if you want to
     * override that default.
     */
    lt_dur_sec mrn_timeout;

    /*@}*/
    
    /* -------------------------------------------------------------------- */
    /** @name Fields for binding (config/monitoring) nodes */
    
    /*@{*/

    /**
     * Data type of this node.
     */
    bn_type mrn_value_type;

    /**
     * Data type modifier flags.  This should be a combination of the
     * bn_type_flags enum (defined in src/include/bnode.h) ORed together,
     * although setting this to anything but its default of 0 is very rare.
     *
     * When this node is queried, as we are making up the response,
     * any flags specified here will be ORed onto the type flags
     * stored in the database.  This will NOT affect the contents of
     * the persistent database.  This distinction is invisible to end
     * users, but means that if you later change this registration
     * field, the new flags will become immediately visible to
     * queries, and no upgrade is necessary.
     *
     * NOTE: this is not currently honored for monitoring nodes.  
     * It only applies to configuration nodes.
     *
     * This field defaults to 0, meaning that no flags will be added.
     */
    uint32 mrn_value_type_flags;

    /**
     * Opposite of mrn_value_type_flags: specifies type flags to be
     * removed from the value attribute of instances of this node
     * in answers to a query.  The bitwise NOT of this field will be
     * ANDed with the type flags when building up a query response,
     * so you only need to list the flags you want removed.
     *
     * NOTE: this is not currently honored for monitoring nodes.  
     * It only applies to configuration nodes.
     *
     * This field defaults to 0, meaning that no flags will be removed.
     */
    uint32 mrn_value_type_flags_remove;

    /*@}*/

    /* -------------------------------------------------------------------- */
    /** @name Fields for configuration nodes */

    /**
     * This is a registration-time field that, if non-NULL will be 
     * passed back to mgmtd clients in query responses.  It is a string.
     */
    const char *mrn_info;

    /**
     * Message to return if someone tries to perform a 'set' operation
     * on this node, and the new value is detected as invalid by the
     * mgmtd infrastructure before calling the module's check
     * function.  (e.g. it does not meet the constraints expressed in
     * the registration fields)  This message will be accompanied by
     * a nonzero error code.
     *
     * For I18N, this field should be given a non-localized string
     * that is marked for localization with the N_() macro.
     * Mgmtd will look up the localized string when it needs it.
     */
    const char *mrn_bad_value_msg;

    /**
     * Initial value of the node, whose exact semantics depends on the
     * node name.
     *
     * If defining a pure literal node, this is the value the node will
     * have in the initial database.
     *
     * If defining a literal node underneath one or more wildcards,
     * this is the default value it will be given whenever an instance
     * of those wildcards is created.
     *
     * If defining a wildcard node, do not fill in this field.
     *
     * See the Samara Technical Guide sections on initial and default
     * values for further documentation.
     */
    const char *mrn_initial_value;

    /**
     * Allows other attributes besides the "value" attribute to be set
     * in the initial database.  mrn_initial_value specifies the the
     * contents of the "value" attribute in string form.  If this is
     * not flexible enough because you want to set initial values on
     * other attributes besides the "value" attribute, fill in one of
     * these two fields.  This is very rare.
     */
    bn_attrib *mrn_initial_value_attrib;
    bn_attrib_array *mrn_initial_attribs;

    /*@}*/

    /* -------------------------------------------------------------------- */
    /** @name Constraints placed on node values
     *
     * These perform the same role
     * as your module's 'check' function, in validating 'set' requests
     * on this node.  But these are applied by the infrastructure
     * before your 'check' function is called, so they save you work
     * if your constraints can be expressed in these terms.
     *
     * If the set request does not meet these constraints, the
     * infrastructure sends an error response back, with the message
     * specified in the mrn_bad_value_msg field.
     */

    /*@{*/

    /**
     * Minimum number of permissible instances of this wildcard.
     * Not applicable to literal nodes.
     *
     * NOTE: this setting is currently not enforced (see bug 12736).
     */
    int32 mrn_limit_wc_count_min;

    /**
     * Maximum number of permissible instances of this wildcard.
     * Not applicable to literal nodes.
     */ 
    int32 mrn_limit_wc_count_max;

    /*
     * XXX/EMT: unknown, and not implemented anyway (see bug 10548)
     */
    const char **mrn_limit_choices;

    /**
     * An array of strings expressing the set of permissible values
     * for this node, in string form.  This can be applied to
     * non-string data types; you just have to render the permissible
     * values into strings to put them here.  The array must be
     * terminated by a NULL string pointer so mgmtd knows when the
     * list ends.
     */
    const char **mrn_limit_str_choices;

    /**
     * An alternative to mrn_limit_str_choices that expresses the list
     * of permissible strings all in a single string, with the choices
     * delimited by whatever character comes first.  For example, to
     * permit "one", "two", and "three", you could use the comma as
     * a delimiter and put ",one,two,three" in this field.
     */
    const char *mrn_limit_str_choices_str;

    /**
     * Specifies that the string value of this node must consist
     * solely of characters contained in this string.
     */
    const char *mrn_limit_str_charlist;

    /**
     * Specifies that the string value of this node may not contain
     * any of the characters contained in this string.
     */
    const char *mrn_limit_str_no_charlist;

    /**
     * Specifies that the string value of this node must match this 
     * regular expression.
     *
     * NOT YET IMPLEMENTED
     */
    const char *mrn_limit_str_regex;

    /**
     * Specifies that the string value of this node must match this 
     * glob pattern.
     *
     * NOT YET IMPLEMENTED
     */
    const char *mrn_limit_str_glob;

    /**
     * Specifies that the numeric value of this node must not be less
     * than this number (expressed in string form).  If
     * mrn_limit_num_min_exclusive is set, the minimum itself is not
     * permissible; otherwise, it is.
     */
    const char *mrn_limit_num_min;
    tbool mrn_limit_num_min_exclusive;

    /**
     * Specifies that the numeric value of this node must not be
     * greater than this number (expressed in string form).  If
     * mrn_limit_num_max_exclusive is set, the maximum itself is not
     * permissible; otherwise, it is.
     */
    const char *mrn_limit_num_max;
    tbool mrn_limit_num_max_exclusive;

    /*
     * Limit the number of characters (or digits) the value may have.
     * This is applied regardless of data type, and refers to the
     * string representation of the data.
     */
    int32 mrn_limit_str_min_chars;
    int32 mrn_limit_str_max_chars;

    /*@}*/

    /**
     * @name Integer alternatives
     *
     * These are optional alternatives for mrn_initial_value,
     * mrn_limit_num_min, and mrn_limit_num_max, for cases when the
     * module would rather specify these in their native type.  The
     * values provided are validated to ensure they fall within the
     * actual range of mrn_value_type.  They can be used for any
     * integer data type, though if you are making a uint64 that is
     * greater than 2^63-1, you should use mrn_..._uint64 instead.
     *
     * It is illegal to fill out both the string and the integer
     * versions of a given field, or to use the integer versions if
     * mrn_value_type is not an integer type.
     *
     * They default to INT64_MIN, and will be ignored if they have
     * this value.
     *
     * Note that you can still set mrn_limit_num_min_exclusive or
     * mrn_limit_num_max_exclusive when using these, as they are not
     * specific to the string representations.
     */
    /*@{*/
    int64 mrn_initial_value_int;
    int64 mrn_limit_num_min_int;
    int64 mrn_limit_num_max_int;
    /*@}*/

    /**
     * @name Integer alternatives for uint64 data type.
     *
     * These are the same as mrn_initial_value_int,
     * mrn_limit_num_min_int, and mrn_limit_num_max_int; except that
     * their different data type allows them to represent the full
     * range of the bt_uint64 data type.  These may ONLY be used for
     * bt_uint64: other data types, even unsigned ones, should use 
     * the int64 versions instead.
     *
     * These default to UINT64_MAX, and will be ignored if they
     * have this value.
     *
     * (XXX/EMT: if someone wanted to use UINT64_MAX here, or
     * INT64_MIN for one of the above, we could have a separate enum
     * field that says which value to use.  It could default to an
     * "auto" setting which would preserve existing behavior.  Ditto
     * for the md_upgrade_rule fields.)
     */
    uint64 mrn_initial_value_uint64;
    uint64 mrn_limit_num_min_uint64;
    uint64 mrn_limit_num_max_uint64;

    /** 
     * "Per-node check function"
     * A function to be called during the check (validation) phase of
     * mgmtd's three-phase commit process.  Unlike mdm_commit_check_func,
     * which is for an entire module, this function is only called if
     * this exact node is changed.  The function should inspect the
     * given node value, and decide if it wants to accept or reject the
     * change.  To accept the change, no action is necessary.  To reject
     * the change, the module must call 
     * md_commit_set_return_status_msg() or
     * md_commit_set_return_status_msg_fmt() to specify a return status
     * code and explanation message.
     *
     * Note that this function is only called if this registered
     * node is modified.  It will necessarily be called during an
     * initial commit.
     *
     * NOTE: this function will also be called when the node is
     * deleted.  In this case, 'change_type' will be mdct_delete,
     * and 'new_attribs' will be NULL.
     */
    mdm_commit_check_node_func mrn_check_node_func;
    void *mrn_check_node_arg;

    /**
     * A function to be called during the side effects phase of mgmtd's
     * three-phase commit process.  Unlike mdm_commit_side_effects_func,
     * which is for an entire module, this function is only called if
     * this exact node is changed.  The function should inspect the
     * given node value, and make any other changes required to the
     * proposed config database.
     *
     * Note that this function is only called if this registered
     * node is modified.  It will necessarily be called during an
     * initial commit.
     */
    mdm_commit_side_effects_node_func mrn_side_effects_node_func;
    void *mrn_side_effects_node_arg;


    /* -------------------------------------------------------------------- */
    /** @name Fields for auditing of config changes
     */
    /*@{*/

    /**
     * User-friendly description of this node.  If this is a wildcard
     * registration, this string may be parameterized with $v$ to 
     * represent the value of the node (the same as the last binding
     * name part), or $n$ (e.g. $1$, $2$, etc.) to represent the nth
     * binding name part, starting from 1.
     */
    const char *mrn_audit_descr;

    /**
     * Option flags.
     */
    mdm_node_audit_flags_bf mrn_audit_flags;

    /**
     * A string representation of a special value of this node which
     * should be rendered differently.  For example, a string may be
     * empty, or an integer that is only meaningful when positive may
     * be -1 or 0, to mean that this object does not wish to override
     * some global default, or that the associated feature should be
     * disabled.  If the string representation of the node's value
     * matches this string, we will render the value as the contents
     * of mrn_audit_special_descr instead.
     */
    const char *mrn_audit_special_value;

    /**
     * The string to use to describe the value of this node, if its
     * actual value matches mrn_audit_special_value.
     */
    const char *mrn_audit_special_descr;

    /**
     * A string to be appended to the value for this node.
     * e.g. to tell what units the value is in.  Note that a space
     * between this and the value is NOT automatically added, so this
     * should start with a space if you want one.
     */
    const char *mrn_audit_value_suffix;

    /**
     * Custom change auditing callback.  This function will be called
     * for each change record seen pertaining to the node it is
     * registered on.  It should only produce a message for that one
     * node, and append it to the provided tstr_array.
     */
    mdm_node_audit_func mrn_audit_func;

    /**
     * Data to be passed to mrn_audit_func when it is called.
     */
    void *mrn_audit_func_data;

    /**
     * Custom value rendering callback.  This is to be used in
     * conjunction with mrn_audit_descr, and instead of
     * mrn_audit_func.  While mrn_audit_func produces an entire
     * message, this function only renders one value into a string,
     * which is then used in the building of a full log message to
     * describe the change.
     */
    mdm_node_audit_value_func mrn_audit_value_func;

    /**
     * Data to be passed to mrn_audit_value_func when it is called.
     */
    void *mrn_audit_value_func_data;

    /*@}*/

    /* -------------------------------------------------------------------- */
    /** @name Fields for monitoring (dynamic state) nodes */

    /*@{*/

    /**
     * Function to call (and argument to pass to it) when someone
     * requests to get this node.  Clearly this applies to literal
     * nodes; but it also applies to wildcard nodes too.  When you are
     * called to get an instance of a wildcard node, validate that it
     * is a legal value, and if so, return the node with a value equal
     * to the last component of the binding name you were asked for.
     * (e.g. if passed "/net/interface/state/eth0", you would return
     * a value of "eth0".)
     *
     * Only applicable to internal monitoring nodes (if your node
     * flags were mrf_flags_reg_monitor_literal or
     * mrf_flags_reg_monitor_wildcard)
     */
    mdm_mon_get_func mrn_mon_get_func;

    /** 
     * Argument to pass to mrn_mon_get_func whenever calling it.
     */
    void *mrn_mon_get_arg;

    /**
     * Function to call (and argument to pass to it) when someone
     * requests to iterate over the instances in this wildcard node.
     *
     * Only applicable to wildcard internal monitoring nodes (if your
     * node flags were mrf_flags_reg_monitor_wildcard)
     */
    mdm_mon_iterate_func mrn_mon_iterate_func;

    /**
     * Argument to pass mrn_mon_iterate_func whenever calling it.
     */
    void *mrn_mon_iterate_arg;

    /**
     * Name of mgmtd's GCL client to whom requests for this node
     * should be forwarded.
     *
     * Only applicable to external monitoring nodes (if your node
     * flags were mrf_flags_reg_monitor_ext_literal or
     * mrf_flags_reg_monitor_ext_wildcard).
     */
    const char *mrn_extmon_provider_name;

    /**
     * == External Passthrough specific
     * (XXX/EMT: document)
     * */
    const char *mrn_passthrough_provider_name;

    /*@}*/

    /* -------------------------------------------------------------------- */
    /** @name Fields for action nodes */

    /*@{*/

    /**
     * Processing function to be called when mgmtd receives an action
     * request for this node.
     *
     * If you want mgmtd to automatically forward the action request to
     * an external daemon, set this to md_commit_forward_action, and 
     * set mrn_action_arg to a pointer to a md_commit_forward_action_args
     * structure that you fill out.  This structure is defined in 
     * src/bin/mgmtd/md_utils.h
     */
    mdm_action_std_func mrn_action_func;

    /**
     * Alternate processing function for actions which need to be able 
     * to modify the database.  If your action does, set this field
     * instead of mrn_action_func.
     *
     * NOTE: do NOT modify the database passed to you using the mdb API!
     * This may appear to work, but it will not do all the correct commit
     * processing, and the changes will not be applied to the system.
     * You MUST make up a set request message, and then pass it to
     * md_commit_handle_commit_from_module(), in order to modify the db.
     */
    mdm_action_modify_func mrn_action_config_func;

    /**
     * Alternate processing function for actions which need to be able
     * to complete asynchronously.  If your action does, set this field
     * instead of mrn_action_func.  Note that async completion is
     * mutually exclusive with modifying the config db (another 
     * special type of action).
     */
    mdm_action_std_async_start_func mrn_action_async_start_func;

    /**
     * Alternate processing function for actions which need to be able to
     * complete asynchronously, and without itself forcing a barrier in
     * message processing, nor causing a barrier to with respect to future
     * message processing.  If your action does, set this field instead of
     * mrn_action_func.  Note that async completion is mutually exclusive
     * with modifying the config db (another special type of action), and that
     * actions of this type may not complete async internally by forking.
     *
     * NOTE: only the infrastructure function md_commit_forward_action()
     * can be assigned to this field for now.  You cannot write your
     * own function to go here, as some of the APIs you'd need to make
     * it work are behind the abstraction layer.
     */
    mdm_action_std_async_nonbarrier_start_func 
        mrn_action_async_nonbarrier_start_func;

    /**
     * An argument to be passed back to any of the prior four action
     * handler functions.
     */
    void *mrn_action_arg;

    /**
     * *Deprecated with  PROD_FEATURE_ACTION_AUDITING*
     * While mrn_action_log_handle is still functional for
     * backward compatability, its function has been moved to the
     * maaf_suppress_logging flag for users of PROD_FEATURE_ACTION_AUDITING.
     *
     * To use the new feature, replace instances of:
     *
     *    node->mrn_action_log_handle = false;
     *
     * with:
     *
     *    node->mrn_action_audit_flags |= maaf_suppress_logging;
     *
     * Indicates whether mgmtd should log a message when this action
     * is performed.  By convention we usually request logging for
     * actions initiated directly by the user, but not for actions
     * initiated indirectly or automatically by the system.
     */
    tbool mrn_action_log_handle;

    /**
     * Number of bindings expected to be passed with an action request
     * to this node.  This is filled in for you when you call
     * mdm_new_action().
     */ 
    int32 mrn_action_num_params;

    /**
     * Array of pointers to mdm_reg_action structures.  The array and the
     * structures it points to are allocated for you when you call
     * mdm_new_action().  Just fill out each of the structures.
     */
    mdm_reg_action **mrn_actions;

    /**
     * User-friendly description (or name) of the action for audit messages.
     *
     * This value will appear as the action description in every action audit
     * message generated for this action.  If audit messages are handled via
     * the custom mdm_action_audit_func, this value should be honored by the
     * callback function.
     *
     * If there is no custom callback, and if no action description is
     * provided, the action node name will be used in its stead, however the
     * system will generate an action node registration time complaint if the
     * maaf_use_node_name or maaf_suppress_logging flag is not set in
     * maad_audit_flags.
     */
    const char *mrn_action_audit_descr;

    /**
     * Node level flags for action audit behavior.  These flags apply to
     * Automatic Action Auditing, should be applied by callbacks when using an
     * mdm_action_audit_func.  See mdm_action_audit_flags for details.
     */
    mdm_action_audit_flags_bf mrn_action_audit_flags;

    /**
     * Audit callback function for the creation of custom action audit log
     * messages when Automatic Action Audit messages will not suffice.  For
     * standard audit messages, simply set the mrn_action_audit_descr field
     * and any non-default settings desired for the mrn_action_audit_flags.
     * Custom node audit callback functions are only required when the audit
     * message needs handling thats unavailable through automatic auditing.
     * Note that customized treatment of binding parameters can usually be
     * handled through the audit features available in mdm_reg_action parameter
     * registration, including, if necessary, custom value rendering through a
     * mra_param_audit_value_func callback that is considered part of Automatic
     * Action Auditing.
     */
    mdm_action_audit_func mrn_action_audit_func;

    /**
     * An optional registration-time argument to be passed to
     * mdm_action_audit_func.
     */
    void *mrn_action_audit_func_arg;

    /*@}*/

    /* -------------------------------------------------------------------- */
    /** @name Fields for event nodes */

    /*@{*/

    /**
     * Number of bindings expected to be passed with an event request
     * to this node.  This is filled in for you when you call
     * mdm_new_event().
     */ 
    int32 mrn_event_num_bindings;

    /**
     * Array of mdm_reg_event structures.  The array and the
     * structures it points to are allocated for you when you call
     * mdm_new_event().  Just fill out each of the structures.
     */
    mdm_reg_event **mrn_events;

    /*
     * XXX/EMT: document these fields
     */
    mdm_event_criteria_compare_func mrn_event_criteria_compare_func;
    void *mrn_event_criteria_compare_arg;
    void *mrn_event_arg;
    tbool mrn_event_log_handle;

    /*@}*/

} md_reg_node;


/*****************************************************************************
 *****************************************************************************
 ** Node registration API
 **
 ** After having registered your module with mdm_add_module()
 ** (and optionally declaring your gettext domain, if you are doing
 ** I18N, with mdm_module_set_gettext_domain()), call these functions
 ** to register your nodes.
 *****************************************************************************
 *****************************************************************************
 */

/**
 * Create a new registration structure for a config or monitoring
 * node.
 */
int mdm_new_node(const md_module *mod, md_reg_node **ret_node);


/**
 * Create a new registration structure for an event node.
 *
 * Example events: config change, syslog message, interface up/down,
 * various stats daemon alarms
 */
int mdm_new_event(const md_module *mod, md_reg_node **ret_node, 
                  uint32 num_bindings);

/**
 * Create a new registration structure for an action node.
 *
 * Example actions: set date/time, reboot system
 */
int mdm_new_action(const md_module *mod, md_reg_node **ret_node, 
                   uint32 num_params);

/*
 * This is the INTERNAL function for adding ACLs to a registration node.
 * Do not call this directly -- use mdm_node_acl_add() instead.
 *
 * The parameters after 'node' and 'local' are of type 'const ta_acl *',
 * and MUST be terminated by a NULL.  The purpose of the wrapper macro is 
 * to add the NULL automatically, relieving the burden from the developer.
 */
int mdm_node_acl_add_internal(md_reg_node *node, tbool local, ...);

/**
 * Add one or more ACLs to a registration node.  If the node does not
 * have an ACL yet, the ones provided are just copied into the node; if
 * it did, copies of the one provided are appended to the existing ACL.
 *
 * Note that every node must have an ACL.  If you want the node to be 
 * fully accessible to everyone, add the standard ACL 'tacl_all_any'.
 * If you want the node to have an empty ACL, leaving it accessible
 * only to the system administrator, pass NULL as the ACL.
 *
 * Note that the ACL can be inherited if the ::mrf_acl_inheritable
 * flag is used on an ancestor.  Setting this field overrides any
 * inherited ACL, but does NOT automatically override the mrn_acl_func
 * (if it was set).
 */
#define mdm_node_acl_add(node, ...)                                           \
    mdm_node_acl_add_internal(node, false, __VA_ARGS__, NULL);

/**
 * Add one or more local ACLs to a registration node.  These are only
 * different from ACLs added via mdm_node_acl_add() in that they are
 * not propagated downwards by the ::mrf_acl_inheritable flag.
 * After inheritance is completed, these ACLs are combined with others
 * on the same node, and there is no different treatment from there on
 * outwards.
 */
#define mdm_node_acl_add_local(node, ...)                                     \
    mdm_node_acl_add_internal(node, true, __VA_ARGS__, NULL);

/**
 * Register a node.  After you have called one of the above three
 * functions to create a registration structure, and filled out the
 * structure, call this to hand it over to mgmtd.
 */
int mdm_add_node(md_module *mod, md_reg_node **inout_node);

/**
 * Flags relevant to overriding mgmtd node registrations.
 */
typedef enum {

    monf_none = 0,

    /**
     * Clear out all the existing flags in mrn_node_reg_flags, and
     * replace them with whatever is found in the new node.
     *
     * Without this flag, the default is to OR the new flags in 
     * without disturbing the existing flags.
     */
    monf_replace_flags = 1 << 0,

    /**
     * Clear out all the existing ACLs, added using mdm_node_acl_add(),
     * and replace them with whatever is found in the new node.  Note
     * that this does not apply to any other ACL-related fields
     * (mrn_acl_local, mrn_acl_func, or mrn_acl_data).
     *
     * Without this flag, the default is to append the new ACLs
     * without disturbing the existing ACLs.
     */
    monf_replace_acl = 1 << 1,

    /**
     * Clear out all the existing local ACLs, added using
     * mdm_node_acl_add_local(), and replace them with whatever is
     * found in the new node.  Note that this does not apply to any
     * other ACL-related fields (mrn_acl, mrn_acl_func, or
     * mrn_acl_data).
     *
     * Without this flag, the default is to append the new local ACLs
     * without disturbing the existing local ACLs.
     */
    monf_replace_acl_local = 1 << 2,

    /**
     * Clear out the existing mrn_acl_func and mrn_acl_data. and
     * replace them with whatever is found in the new node.
     *
     * Without this flag, if the new node has an mrn_acl_func, it 
     * and its corresponding mrn_acl_data are appended to the node,
     * such that both functions will be called in series, with the
     * original function being called first.  The output from each
     * function is fed as input to the next, until the final one,
     * whose answer is authoritative.
     *
     * XXXX/EMT/RBAC: bug 13013: if there would be >1 ACL func (this flag
     * is NOT passed, and one is specified in both the old and new
     * registrations), it's an error.  This should be supported.
     */
    monf_replace_acl_func = 1 << 3,

    /**
     * Clear the existing description string in mrn_description, and
     * replace it with whatever is found in the new node.
     *
     * Without this flag, the default is to append the new 
     * mrn_description onto the existing mrn_description.
     */
    monf_replace_description = 1 << 4,

} mdm_override_node_flags;

/** Bit field of ::mdm_override_node_flags ORed together */
typedef uint32 mdm_override_node_flags_bf;

/**
 * Amend or override parts of a pre-existing node registration which
 * are relevant to ACLs:
 *
 *   1. Look up the record of the node named by node->mrn_name.
 *
 *   2. Add all the registration flags from node->mrn_node_reg_flags
 *      (i.e. OR it together with the pre-existing flags).  Only
 *      ACL-related flags (which for now means only mrf_acl_inheritable)
 *      may be specified here.
 *
 *   3. Append all the ACLs from this node onto the pre-existing ones.
 *
 *   4. If mrn_acl_func is set in the new node, append this and its
 *      mrn_acl_data to the old node.
 *
 *   5. Append the contents of node->mrn_description to any existing
 *      description.
 *
 * There are two scenarios where this function is expected to be used:
 *
 *   - To set ACLs and the mrf_acl_inheritable flag on the root node of
 *     the current module.  The root node is automatically created for 
 *     you when you call mdm_add_module(), and your module is not allowed
 *     to do anything else with it.  The one exception is to add ACLs 
 *     to it, since this may be the common subtree root you need to
 *     use inheritance to propagate common ACLs to all of your nodes.
 *
 *     Note that such nodes are created with tacl_all_traverse 
 *     (non-inheritable) by default, meaning that this node will not 
 *     interfere with a user performing an operation on a node in this
 *     subtree.  By default any ACL you provide will be appended to 
 *     this, though you may use ::monf_replace_acl to start fresh.
 *
 *  - To override ACLs, and possibly set or clear the mrf_acl_inheritable
 *    flag, on nodes registered by another module.  Note that in order for
 *    this to work, your module must initialize after the other one.  
 *    Module init order is not guaranteed unless you set a special
 *    symbol; see "Initialization and deinitialization of modules" in
 *    src/include/dso.h for details.
 *
 * Note that no other fields in the registration structure may be set.
 * If the node passed has anything other than the following fields set,
 * this function will fail with an error:
 *   - mrn_name
 *   - mrn_node_reg_flags (mrf_acl_inheritable flag only)
 *   - mrn_acl (set using mdm_node_acl_add() API)
 *   - mrn_acl_local (set using mdm_node_acl_add_local() API)
 *   - mrn_acl_func and mrn_acl_data
 *   - mrn_description
 *
 * It is also an error if the node named is not found in the
 * registration tree.
 *
 * \param module The module who is calling us, which is not necessarily
 * the module which originally registered the node.
 *
 * \param inout_node A registration node containing the information
 * needed for the override.  As described above, only certain flags
 * are legal to be set in this structure; setting other fields will
 * result in error.  
 *
 * \param flags Option flags.
 */
int mdm_override_node_flags_acls(md_module *module, md_reg_node **inout_node,
                                 mdm_override_node_flags_bf flags);

/**
 * Set all parameters in a newly created action node to be required / optional.
 * Typically this is used after mdm_new_action to mark all parameters as
 * required prior to marking individual parameters as optional.  This is useful
 * in cases where the majority of an action's parameters are required.
 */
int
mdm_action_node_params_set_required(md_reg_node *inout_node, tbool required);


/* ------------------------------------------------------------------------- */
/** @name Licensing
 */
/*@{*/

/**
 * Register a licensed feature.  Called from your mgmtd module to put
 * some configuration under the control of the licensing system,
 * usually to prevent a feature from being enabled or configured until
 * a valid license key is installed.  See doc/design/licenses.txt
 * and the Samara Technical Guide for details.
 *
 * This function is used to register features regardless of whether
 * they will be used with type 1 or type 2 licenses.
 *
 * \param module The module structure you got back from mdm_add_module().
 *
 * \param feature_name The name of the licensed feature.  This must match
 * the feature name provided to genlicense.  The name of the feature that
 * a license goes with will show in plaintext as part of the license key.
 *
 * \param feature_descr A user-friendly description of this feature.
 * May be NULL to decline to give a description.  (XXX/EMT: I18N)
 *
 * \param restrict_tree_pattern The root of the subtree under which the
 * user will not be permitted to make changes if the feature is not
 * licensed.  NOT YET IMPLEMENTED (this condition is not enforced).
 *
 * \param track_active_node_name The name of a boolean configuration
 * node registered by your module which will be made to reflect whether
 * or not the feature has been activated by a valid license key.
 * The license key module will manage this node for you using its 
 * side effects function, and will not allow the user to override it.
 * This is the node your feature will read to see if it should offer
 * itself.  This parameter is optional; set to NULL if you do not want
 * to have such a node.
 *
 * \param track_active_node_activated_value The value that the node
 * specified by track_active_node_name should have if the feature is
 * active.  If the feature is not active, it will have the opposite
 * value.
 *
 * \param activate_node_name NOT YET IMPLEMENTED
 *
 * \param activate_node_value NOT YET IMPLEMENTED
 *
 * \param deactivate_node_name NOT YET IMPLEMENTED
 *
 * \param deactivate_node_value NOT YET IMPLEMENTED
 */
int mdm_license_add_licensed_feature(md_module *module, 
                                     const char *feature_name,
                                     const char *feature_descr,
                                     const char *restrict_tree_pattern,
                                     
                                     const char *track_active_node_name,
                                     tbool track_active_node_activated_value,
                                     
                                     const char *activate_node_name,
                                     tbool activate_node_value,
                                     
                                     const char *deactivate_node_name,
                                     tbool deactivate_node_value);

/**
 * Data to be passed to a ::mdm_license_activation_func for it to
 * determine if a particular activation option value should render
 * a license inactive.
 *
 * Passed as a struct to allow us to include additional information in 
 * the future without breaking the API.
 */
typedef struct mdm_license_activation_data {
    const char *mld_feature_name;
    lk_option_tag_id mld_option_tag_id;
    const bn_attrib *mld_option_value;
} mdm_license_activation_data;

/**
 * Callback function to be associated with an activation option.
 * If a license specifying a given activation option is installed,
 * the infrastructure will call this callback during the side-effects
 * commit phase, to ask if the system meets the criteria set forth by
 * that option.  Set ret_active to true if the criteria is met, false
 * if not.  The license will be considered active if ALL activation 
 * options specified in it get 'true' back from their respective 
 * callbacks.
 *
 * \param commit Mgmtd commit state structure.
 *
 * \param db Current mgmtd node database.
 *
 * \param lic_data Information about the activation option we are to 
 * evaluate.
 *
 * \param data Opaque data registered by the caller along with this 
 * function.
 *
 * \retval ret_active Specifies whether or not the license should be
 * active, according to this option.  In order for a license to be active,
 * all of its activation options must return 'true' for this.
 */
typedef int (*mdm_license_activation_func)
    (md_commit *commit, const mdb_db *db, 
     const mdm_license_activation_data *lic_data, void *data, 
     tbool *ret_active);

/**
 * Another callback function to be associated with an activation
 * option.  While every activation option must have an
 * mdm_license_activation_func to be useful, specifying this one is
 * purely optional, and generally only applies to activation options
 * whose results (whether or not they make a license inactive) may
 * change without being triggered by a configuration change.
 *
 * Functions of this type are called for every corresponding
 * activation option on every license in the config, every time any
 * configuration changes.  They determine when, if ever, the system
 * should recheck the active status of those licenses.  They can
 * express this in either (or both) of two ways: by specifying a fixed
 * time interval after which we should check; or by specifying the name
 * of a mgmtd event whose occurrence should trigger a recheck.
 *
 * XXXX/EMT: rechecking because of a mgmtd event is NOT YET IMPLEMENTED.
 *
 * \param commit Mgmtd commit state structure.
 *
 * \param db Current mgmtd node database.
 *
 * \param lic_data Information about the activation option we are to 
 * evaluate.
 *
 * \param active_last_update The datetime (in milliseconds) at which 
 * the active flag of this license was most recently updated.
 * This can be used to determine whether or not that calculation is
 * up-to-date (e.g. if the license expires at a particular datetime).
 *
 * \param data Opaque data registered by the caller along with this 
 * function.
 *
 * \retval ret_next_check_time The time at which the active status of
 * this particular license should be rechecked.  Return -1 to not
 * recheck the license on a time basis.  Notice that this time is in
 * milliseconds.  If the time is in the past, the license will be
 * rechecked almost immediately (we set a timer for 0 ms).
 *
 * \retval ret_next_check_event The name of a mgmtd event which should
 * trigger a recheck of the active status of this particular license.
 * Return NULL to not recheck the license based on an event.  (NOTE:
 * rechecking based on events is NOT YET IMPLEMENTED).
 */
typedef int (*mdm_license_activation_recheck_func)
    (md_commit *commit, const mdb_db *db,
     const mdm_license_activation_data *lic_data, 
     lt_time_ms active_last_update, void *data,
     lt_time_ms *ret_next_check_time, tstring **ret_next_check_event);

/**
 * Register an activation tag.  Note that activation tags are declared
 * in license.h, but are ineffective unless also registered with this
 * function, so you can provide a callback that will enforce the tag.
 * The 'data' is an opaque piece of data to be passed back to the 
 * callback whenever it is called.
 *
 * If the option tag ID passed is not an activation option found
 * in license.h, an error will be returned.
 *
 * NOTE: an activation option which is satisfied (i.e. does not make
 * the license inactive) at one time might not be satisfied at another
 * time.  The 'recheck_callback' parameter here gives you a way to 
 * trigger rechecks at certain times or after certain events.
 * To control what kinds of configuration changes trigger a recheck,
 * please use graft point #2 in mdm_license.c.
 */
int mdm_license_add_activation_tag(lk_option_tag_id option_tag_id,
                        mdm_license_activation_func activation_callback,
                        void *activation_data,
                        mdm_license_activation_recheck_func recheck_callback,
                        void *recheck_data);

/* ------------------------------------------------------------------------- */
/** Structure containing the results of decoding a license.
 */
typedef struct mdl_license {
    tstring *mll_license; /* Original license key string entered by user */
    lk_license_type mll_license_type;
    tstring *mll_feature;
    tbool mll_is_well_formed;
    tbool mll_is_valid;
    tbool mll_is_revoked;
    tbool mll_is_active;
    tbool mll_is_registered;
    lt_time_ms mll_active_last_update;
    lk_hash_type mll_hash_type;

    /*
     * Copied from the lv_magic_number field of the lk_validator that was
     * used to validate this license.  A mdm_license_activation_check_func
     * may use this to apply different policies to different validators,
     * if desired.
     *
     * Note that this is only populated for type 2 licenses.  For type 1,
     * it will be zero.
     */
    uint32 mll_validator_magic_number;

    /*
     * These two arrays are parallel, i.e. an element in position 'n' in
     * one corresponds to the element in position 'n' in the other.
     *
     * mll_options is just a list of all of the options found in the
     * license.  Its order is not meaningful, and it cannot be indexed
     * by option tag id because a single license might have multiple
     * instances of the same option tag id.
     *
     * mll_options_passed tells whether each of the options in the
     * corresponding slot in mll_options passed its activation
     * criteria.  Thus it is only meaningful for activation options,
     * and the slots corresponding to informational options will be
     * empty.
     */
    lk_option_array *mll_options;
    tbool_array *mll_options_passed;

    /*
     * A message returned by a global activation check function,
     * telling why it marked the license as inactive.  This should 
     * generally only be set if all the activation options passed,
     * and then one of the global checks failed activation.
     */
    tstring *mll_activation_failure_reason;

} mdl_license;

/**
 * Function to be called to check the active status of a license.  You are
 * called after all activation options have been checked and passed.  See
 * mdm_license_add_activation_check() below for further details.
 *
 * \param commit Commit context.
 * \param db Database.
 * \param license The license to be checked for activation.  Ignore
 * mll_is_active, which is undefined (we are in the process of computing it);
 * and ignore mll_active_last_update.
 * \param data The data you passed to mdm_license_add_activation_check().
 * \retval ret_active Your determination about the active status of the
 * license.  The boolean which this points at starts out set to 'true',
 * so if you don't change it, the license will by default be considered
 * still active.  Only change it to 'false' if you wish to veto this
 * license's active status, so it will be considered inactive.
 * \retval ret_message If you decide the license is not active, this can
 * be used to return an explanation message.  If you are not setting 
 * ret_active to false, you should not return a message here, as it will
 * be ignored.
 */
typedef int (*mdm_license_activation_check_func)
    (md_commit *commit, const mdb_db *db, 
     const mdl_license *license, void *data, tbool *ret_active,
     tstring **ret_message);

/**
 * Register an activation check callback.  Unlike the activation tag
 * registration above, this is not specific to any particular activation
 * option, nor is it specific to anything else either.  It will be
 * consulted every time we check the active status of any valid license,
 * unless something else determines first that the license is not active.
 * As with activation options, the function may veto a license (declare
 * that it is not active), but it may not do the opposite; even if we
 * think the license should be active as far as we care, it still has
 * to pass all the other tests.
 */
int mdm_license_add_activation_check(mdm_license_activation_check_func func,
                                     void *data);

/*@}*/


/*****************************************************************************
 *****************************************************************************
 ** Miscellaneous other APIs
 **
 ** XXX/EMT: most of these need documentation
 *****************************************************************************
 *****************************************************************************
 */

int mdm_override_capmasks(void);

int mdm_translate_int_field(const char *node_name, bn_type value_type,
                            int64 snum, char **ret_num_str);

int mdm_translate_int_or_uint_field(const char *node_name, bn_type value_type,
                                    int64 snum, uint64 unum, 
                                    char **ret_num_str);

/**
 * To be used in conjunction with the ::modrf_upgrade_from flag to
 * mdm_add_module().  Makes this module eligible to have its upgrade
 * logic run, even if it has not been seen before.  Usually used in
 * cases where a new module (the one calling this function) is taking
 * over some or all of the nodes previously owned by another module.
 * See "Running Upgrades in a New Module" section in Samara Technical
 * Guide for details.
 *
 * \param module Mgmtd module requesting the special treatment.
 *
 * \param from_module_name Name of the module whose nodes we are
 * taking over.
 *
 * \param cutover_version Semantic version number of the other module,
 * at which we are performing the transfer.
 *
 * \param ignore_missing_module If 'false', we are asserting that the
 * module named in \c from_module_name exists, and a warning will be 
 * logged if it does not.  If 'true', we admit that the other module may
 * not exist, and do not want any complaints if it does not.  Either way,
 * if the other module does not exist, we will act just as if there had
 * not been an "upgrade from" request: the initial values of this module
 * will be used as usual.
 */
int mdm_set_upgrade_from(md_module *module, const char *from_module_name,
                         int32 cutover_version, tbool ignore_missing_module);

/*
 * This is an INTERNAL-USE-ONLY API.  Module authors should use 
 * the 'mdm_set_interest_subtrees' macro below instead.
 *
 * (Exception: Samara base modules who need to put graft points 
 * in their calls to mdm_set_interest_subtrees() may use this,
 * since the preprocessor gets confused when using the macro
 * version below.)
 *
 * The parameters after 'module' are of type 'const char *', and are
 * the names of subtree roots in the node hierararchy.  The last parameter
 * must be a NULL delimiter.
 */
int mdm_set_interest_subtrees_internal(md_module *module, ...);

/**
 * Provide a list of registration tree node names, which are to be
 * treated as roots of subtrees which interest the module during 
 * commit processing.  It is not necessary to do this for nodes 
 * registered by the module itself, for which it will automatically
 * be called.
 *
 * Repeated calls to this function are cumulative; all node names
 * given are honored, and the last call does NOT remove names given
 * by previous calls.
 *
 * Please note that these are REGISTRATION nodes, not database nodes,
 * so you should use wildcards here rather than any specific wildcard
 * instance names.  (If you do use a specific wildcard instance name,
 * it will be mapped onto the corresponding registration node, and
 * the instance name will be ignored.)  See ::modrf_all_changes for
 * further details.
 *
 * Non-configuration nodes in the subtree provided are automatically
 * ignored, so you do not have to confine your subtrees to
 * configuration.  For example, to listen to all interface-related
 * changes, you could use "/net/interface" instead of 
 * "/net/interface/config", and the non-config nodes in this subtree
 * would not cause any drag on performance.
 */
#define mdm_set_interest_subtrees(module, ...) \
    mdm_set_interest_subtrees_internal(module, __VA_ARGS__, NULL)


/**
 * Get a pointer to a symbol from another module.  Its implementation
 * differs depending on whether we are running with dynamic or static
 * linking of modules.  If linking is dynamic, this just calls into
 * the lc_dso library to get the symbol.  If linking is static, this
 * tries to look up the pointer in a table which must be manually
 * maintained in md_modules.c.
 *
 * If the symbol could not be found, NULL is returned with a success
 * code.
 *
 * NOTE: please do NOT use this to call functions or reference
 * variables in the Samara base mgmtd modules.  The implementations of
 * our modules are not considered public APIs, and we will feel free
 * to change them at any time with no warning.  This is mainly
 * intended for calls between two modules owed by the same developer
 * or group.
 *
 * Please also note that this is a dangerous function in general,
 * because there is no type-checking on the symbols you get back.
 * You will have to cast them in order to use them, and if you cast
 * them to the wrong thing, Bad Things will happen.
 */
int mdm_get_symbol(const char *module_name, const char *symbol_name,
                   void **ret_symbol);

/*
 * Debugging functions; generally not for module use.
 */
int md_module_dump(const md_module_array *arr, uint32 idx, md_module *elem,
                   void *data);

int md_module_dump_all(const md_module_array *mods);

/*
 * Dumps to the specified fd the node registration tree, formatted to look
 * like nodes.txt .  This is to help verify that nodes.txt matches the
 * current actual registrations.
 *
 * If fd is -1, logging goes to syslog, if -2, to stdout , otherwise fd
 * must be an open fd that the output will be written to.
 */
int mdm_reg_dump_tree_nodestxt(mdb_reg_state *reg_state, int fd);


/* ------------------------------------------------------------------------- */
/** @name Capability mask overrides
 */
/*@{*/

/**
 * Option flags for rules in ::md_override_cap_rule.
 */
typedef enum {
    mocrf_none = 0,

    /**
     * The rule applies only to the single node named by the rule.
     * (If this flag is not specified, the rule applies to the subtree
     * rooted at the node named by the rule, including the node itself.)
     */
    mocrf_self_only = 1 << 0,

} md_override_cap_rule_flag;

/** Bit field of ::md_override_cap_rule_flag ORed together */
typedef uint32 md_override_cap_rule_flag_bf;

/**
 * Operation to be performed by a ::md_override_cap_rule.
 *
 * Note that a rule may never set any capability flags which are
 * inappropriate for a node, given its node type, e.g. setting query
 * flags on an event node.  Any flags in ::mocr_caps which are not
 * appropriate for the node will be either ignored, or flagged as
 * an error.
 */
typedef enum {
    mocro_none = 0,

    /**
     * Replace the capability flags for the nodes affected with the
     * value specified by the ::mocr_caps field, overwriting any
     * previous flags.
     */
    mocro_repl,

    /**
     * Replace the capability flags for the query operation only,
     * without affecting the flags for other operations (sets).
     * The ::mocr_caps field should only contain query capability
     * flags; any other flags will be ignored.
     *
     * This option can only be used if the ::mocr_node_types field
     * specifies ::mont_config and/or ::mont_state.  (It is mainly
     * interesting for config nodes; for state nodes, its effect 
     * is the same as that of ::mocro_repl)
     */
    mocro_repl_query,

    /**
     * Replace the capability flags for the set operation only,
     * without affecting the flags for any other operations (queries).
     * The ::mocr_caps field should only contain set capability
     * flags; any other flags will be ignored.
     *
     * This option can only be used if the ::mocr_node_types field
     * specifies ::mont_config.
     */
    mocro_repl_set,

    /**
     * Set the capability flags for the nodes affected to the result
     * of a bitwise AND with the contents of the ::mocr_caps field.
     *
     * In other words, clear whichever capability flags are not set
     * in the ::mocr_caps field.
     */
    mocro_and,

    /**
     * Set the capability flags for the nodes affected to the result
     * of a bitwise AND with the bitwise NOT of the contents of the
     * ::mocr_caps field.
     *
     * In other words, clear whichever capability flags are set in 
     * the ::mocr_caps field.
     */
    mocro_and_not,

    /**
     * Set the capability flags for the nodes affected to the result
     * of a bitwise OR with the contents of the ::mocr_caps field.
     * This can be used to set certain capability bits, while leaving
     * others unaffected.
     */
    mocro_or,

    /**
     * Reset the capability flags for the nodes affected to their
     * originally-registered values, regardless of what overrides 
     * have been applied to them by earlier rules.
     *
     * In this case, the ::mocr_caps field is ignored.
     */
    mocro_reset,

} md_override_cap_rule_oper;

/**
 * Expresses the type of mgmtd nodes to which a capability override
 * should be applied.
 *
 * Note that mont_all (or any other combination of flags besides
 * mont_binding) may only be used with the mocro_reset operation.
 *
 * Note that 'passthrough' nodes count as both config and state, so
 * rules applying to either kind will apply also to passthrough nodes.
 */
typedef enum {
    mont_none =          0,
    mont_config =   1 << 0,
    mont_state  =   1 << 1,
    mont_action =   1 << 2,
    mont_event  =   1 << 3,
    mont_binding =  mont_config | mont_state,
    mont_all =      mont_binding | mont_action | mont_event,
} md_override_node_type;

/** Bit field of ::md_override_node_type ORed together */
typedef uint32 md_override_node_type_bf;

/**
 * A more powerful alternative to md_capmask_override_list, which
 * allows more complex overrides to be done with fewer rules, in
 * general.
 */
typedef struct md_override_cap_rule {

    /**
     * The name of a node which is the root of the subtree of nodes to
     * which this rule applies -- though if the mocrf_self_only flag
     * is used, the rule applies only to this single node, not the
     * entire subtree.
     *
     * Note that although this is declared as a 'const char *' 
     * (for static rules), it is treated like a 'char *' for dynamic
     * rules constructed using md_mod_reg.c graft point #3.  That is,
     * you should assign a dynamically allocated pointer to it, and
     * this memory will be freed automatically by the infrastructure
     * after the rule is honored.
     */
    const char *mocr_node_name;

    /**
     * Option flags
     */
    md_override_cap_rule_flag_bf mocr_flags;

    /**
     * Indicates the type(s) of nodes to which this rule should apply.
     *
     * Note that except in the case of the mocro_reset operation, it
     * does not make sense to combine any of these flags together,
     * other than the two which are already combined into the
     * mont_binding option.  Any other combination will result in an
     * error.  With mocro_reset, any combination is permitted.  Use
     * mont_all to apply to all nodes, or mont_none (0) to disable a
     * rule altogether.
     */
    md_override_node_type_bf mocr_node_types;
    
    /**
     * What operation is to be performed on the capabilities of the
     * nodes affected.  Dictates how the ::mocr_caps field will
     * be interpreted.
     */
    md_override_cap_rule_oper mocr_oper;

    /**
     * Capability flags.  The precise use of this number is dictated by
     * the contents of the ::mocr_oper field.  If ::mocro_reset is
     * selected, this field is ignored.
     */
    uint32 mocr_caps;

} md_override_cap_rule;

TYPED_ARRAY_HEADER_NEW_NONE(md_override_cap_rule,
                            struct md_override_cap_rule *);
     
/*@}*/

/* ------------------------------------------------------------------------- */
/** @name ACL extensions/overrides
 *
 * XXXX/EMT/RBAC: this override mechanism is NOT YET IMPLEMENTED 
 * (bug 13013) -- there is no infrastructure support to take a 
 * md_acl_override_rule and honor it.  These structures are subject to
 * change during implementation, so they should not be used for now.
 */
#if 0
/*@{*/

typedef enum {

    maof_none = 0,

    /**
     * By default the rule applies to the entire subtree rooted at the
     * named node.  If this flag is used, the rule applies only to the
     * node itself.
     */
    maof_self_only,

} md_acl_override_flag;

/** Bit field of ::md_acl_override_flag ORed together */
typedef uint32 md_acl_override_flag_bf;

/**
 * An operation to perform on an ACL which we are going to override in
 * some way.
 */
typedef enum {
    
    maoo_none = 0,
    
    /**
     * Clear the existing ACL (Access Control List), and make a new
     * ACL with only the specified ACL.
     */
    maoo_replace,

    /**
     * Append the specified ACL to the existing ACL.
     *
     * (If ACLs become ordered in the future, e.g. because we have
     * negative ACLs, we might support a maoo_prepend which would
     * insert the new ACL at the beginning.  Hopefully there would not
     * be a need to insert one in the middle.)
     */
    maoo_append,

    /**
     * Keep the existing ACL, except remove all ACL entries in it
     * which match any of the entries in the specified ACL.
     */
    maoo_remove,

} md_acl_override_oper;

/**
 * A single rule for overriding an ACL on a previously-registered
 * mgmtd node.
 */
typedef struct md_acl_override_rule {
    /**
     * The name of the node whose ACLs to override.  By default the
     * rule applies to the entire subtree rooted at this node, but if
     * ::maof_self_only is specified, it only applies to this node
     * itself.
     */
    const char *maor_node_name;

    /** ACL override operation */
    md_acl_override_oper maor_oper;

    /** ACL override flags */
    md_acl_override_flag_bf maor_flags;

    /**
     * ACL for use in this override.
     */
    const ta_acl *maor_acl;

} md_acl_override_rule;

/*@}*/
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MD_MOD_REG_H_ */
