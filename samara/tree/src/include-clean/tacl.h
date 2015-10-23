/*
 *
 * tacl.h
 *
 *
 *
 */

#ifndef __TACL_H_
#define __TACL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "bnode.h"

/** \defgroup acls LibTACL: APIs for Access Control Lists (ACLs) */


/* ========================================================================= */
/** \file src/include/tacl.h Tall Maple Access Control List APIs
 * \ingroup acls
 *
 * This file contains both the base ACL APIs, and the standard set of
 * auth groups, roles, and pre-defined ACLs used by the Samara base
 * modules.
 */

#include "common.h"

#define TA_CLASS_MAX_ATTRIBS          16
#define TA_ROLE_MAX_AUTH_GROUPS       16
#define TA_ACL_INIT_MAX               16

/* See also MD_USERNAME_LENGTH_MAX */
#define TA_MAXLEN_USERNAME            31

#define TA_MAXLEN_DESCR               63


/* ========================================================================= */
/*                               Operations                                  */
/* ========================================================================= */

/**
 * Note that when ACLs are moved into the config db, we'll probably
 * have to insert some more operations for more granularity (mainly to
 * control which attributes can be read or written; and possibly to
 * distinguish between creating a node with its default value
 * vs. setting your own value).  This is not expected to break
 * anything, because "set_modify" would still mean the ability to
 * modify everything, and you'd have to be more specific to get
 * anything different.  There will now be an ACL attribute, and we
 * will probably restrict access to this attribute from some roles
 * (like monitor); but since it was not there to query before, it
 * would not be a breaking change for it to be inaccessible to some.
 */
typedef enum {

    /* ........................................................................
     * Operations that are relevant to mgmtd nodes.
     */
    tao_none =                      0,
    tao_query_get =                 1 <<  0,
    tao_query_iterate =             1 <<  1,
    tao_query_advanced =            1 <<  2,
    tao_set_create =                1 <<  3,
    tao_set_delete =                1 <<  4,
    tao_set_modify =                1 <<  5,
 /* tao_set_modify_value =          1 <<  6, */
 /* tao_set_modify_acl =            1 <<  7, */
 /* tao_set_modify_other =          1 <<  8, */
    tao_action =                    1 <<  9,
    tao_event_send =                1 << 10,
 /* tao_event_receive =             1 << 11, */
    tao_traverse =                  1 << 12,

    tao_query = (tao_query_get | tao_query_iterate | tao_query_advanced),
    tao_set   = (tao_set_create | tao_set_delete | tao_set_modify),

    tao_mgmtd_node_oper_MASK = (tao_query_get |
                                tao_query_iterate |
                                tao_query_advanced |
                                tao_set_create |
                                tao_set_delete |
                                tao_set_modify |
                             /* tao_set_modify_value |  */
                             /* tao_set_modify_acl |    */
                             /* tao_set_modify_other |  */
                                tao_action |
                                tao_event_send |
                             /* tao_event_receive |     */
                                tao_traverse),

    /* ........................................................................
     * Operations that are relevant to the CLI.
     */
    tao_cli_exec =                  1 << 13,
    tao_cli_revmap =                1 << 14,

    tao_cli_oper_MASK = (tao_cli_exec |
                         tao_cli_revmap),

    tao_all_oper_MASK = (tao_mgmtd_node_oper_MASK | tao_cli_oper_MASK),

} ta_oper;

/**
 * Bit field of ::ta_oper ORed together
 */
typedef uint32 ta_oper_bf;

extern const lc_enum_string_map ta_oper_map[];

#define ta_oper_to_str(oper) lc_enum_to_string(ta_oper_map, oper)


/* ========================================================================= */
/*                           Authorization Groups                            */
/* ========================================================================= */

/**
 * Authorization groups.
 */
typedef enum {
    taag_none = 0,

    /**
     * This is a special authorization group.
     *
     * Membership in this auth group confers the ability to perform
     * any operation on any node.  That is, all ACL privilege checks
     * will automatically succeed for one who is a member of
     * taag_admin.
     *
     * When PROD_FEATURE_ADMIN_NOT_SUPERUSER is disabled, membership
     * in taag_admin also confers uid/gid 0.
     */
    taag_admin = 1,

    /**
     * This is a special authorization group.
     *
     * All users are always in this group.  If all users should be
     * permitted a certain operation, permission can be granted to
     * this group, and it will apply to all users.
     */
    taag_all = 2,


/* ------------------------------------------------------------------------- */
/* Internal-use graft point 9910: authorization groups.
 * This is the Samara base platform equivalent of customer graft point #10.
 * -------------------------------------------------------------------------
 */
#ifndef TACL_NO_TMS
#undef TACL_HEADER_INC_GRAFT_POINT
#define TACL_HEADER_INC_GRAFT_POINT 9910
#include "tacl.tms.inc.h"
#endif /* TACL_NO_TMS */


/* ------------------------------------------------------------------------- */
/* Customer-specific graft point 10: additional authorization groups.
 *
 * NOTE: authorization groups [0..9999] are reserved for internal
 * Samara usage, so customers should number their auth groups starting
 * at 10000.
 *
 * NOTE: in a future release, authorization group numbers will be
 * stored in the config database, and will thus need to be stable.
 * Although they are not technically required to be stable now, it
 * is recommended that you explicitly assign numbers to all of them,
 * in preparation for this future transition.
 * -------------------------------------------------------------------------
 */
#ifdef INC_TACL_HEADER_INC_GRAFT_POINT
#undef TACL_HEADER_INC_GRAFT_POINT
#define TACL_HEADER_INC_GRAFT_POINT 10
#include "../include/tacl.inc.h"
#endif /* INC_TACL_HEADER_INC_GRAFT_POINT */

    taag_LAST

} ta_auth_group;


extern const lc_enum_string_map ta_auth_group_map[];

#define ta_auth_group_to_str(auth_group) lc_enum_to_string(ta_auth_group_map, \
                                                           auth_group)

extern const ta_auth_group ta_auth_groups_enable[];
extern const ta_auth_group ta_auth_groups_config[];


/**
 * Does the specified auth group grant the ability to enter "enable"
 * mode in the CLI?  This is a somewhat arbitrary distinction drawn
 * by the CLI, which should basically be given only to "higher-level"
 * auth groups.
 *
 * Note that any auth group that returns true for
 * ta_auth_group_allow_config() will also return true for this.
 *
 * (This is unlikely to have much application outside the CLI's own
 * enable command, but is present here to be alongside
 * ta_auth_group_allow_config(), which is of more general interest.)
 */
tbool ta_auth_group_allow_enable(ta_auth_group group);

/**
 * Does the specified auth group grant the ability to enter "config"
 * mode in the CLI?
 *
 * One way to think about it is whether this auth group has been
 * granted Set permissions on any nodes.  However, this function's
 * answer will remain static at runtime, while the permissions
 * granted to an auth group may become variable over time.
 */
tbool ta_auth_group_allow_config(ta_auth_group group);


/* ========================================================================= */
/*                                 Roles                                     */
/* ========================================================================= */


typedef enum {
    tar_none = 0,

    tar_admin,

/* ------------------------------------------------------------------------- */
/* Internal-use graft point 9920: roles.
 * This is the Samara base platform equivalent of customer graft point #20.
 * -------------------------------------------------------------------------
 */
#ifndef TACL_NO_TMS
#undef TACL_HEADER_INC_GRAFT_POINT
#define TACL_HEADER_INC_GRAFT_POINT 9920
#include "tacl.tms.inc.h"
#endif /* TACL_NO_TMS */

/* ------------------------------------------------------------------------- */
/* Customer-specific graft point 20: additional roles.
 *
 * IMPORTANT NOTE: if you add new roles to this list, and if you have
 * PROD_FEATURE_ACLS_DYNAMIC enabled in your product, you must write
 * upgrade logic to ensure there are no user-defined roles with the
 * same names as the ones you are adding.  If you don't, and if there
 * is overlap, the database may fail the initial commit.
 * -------------------------------------------------------------------------
 */
#ifdef INC_TACL_HEADER_INC_GRAFT_POINT
#undef TACL_HEADER_INC_GRAFT_POINT
#define TACL_HEADER_INC_GRAFT_POINT 20
#include "../include/tacl.inc.h"
#endif /* INC_TACL_HEADER_INC_GRAFT_POINT */

} ta_role;

extern const lc_enum_string_map ta_role_map[];

typedef struct ta_role_mapping {
    ta_role tarm_role;
    ta_auth_group tarm_auth_groups[TA_ROLE_MAX_AUTH_GROUPS + 1];
} ta_role_mapping;

extern const ta_role_mapping ta_role_mappings[];

typedef enum {
    troo_none = 0,
    troo_add,
    troo_remove,
} ta_role_override_oper;

typedef struct ta_role_override {
    ta_role tro_role;
    ta_role_override_oper tro_oper;
    ta_auth_group tro_auth_groups[TA_ROLE_MAX_AUTH_GROUPS + 1];
} ta_role_override;

extern const ta_role_override ta_role_overrides[];

#define TA_CAPAB_MAX_ROLES 16

typedef struct ta_capab_to_roles {
    uint32 tctr_capab;
    ta_role tctr_roles[TA_CAPAB_MAX_ROLES];
} ta_capab_to_roles;

extern const ta_capab_to_roles ta_capabs_to_roles[];


/* ========================================================================= */
/*                            ACL creation APIs                              */
/* ========================================================================= */

typedef struct ta_acl ta_acl;
typedef struct ta_acl_entry ta_acl_entry;

/**
 * Create a single ACL entry.
 *
 * \retval ret_acl_entry The newly-created ACL entry.
 *
 * \param descr An optional string to describe this ACL entry.  This
 * is limited to TA_MAXLEN_DESCR characters.
 *
 * \param opers A bit field of ::ta_oper ORed together, listing all of
 * the operations which this ACL entry grants to users matching it.
 *
 * \param username The username for this ACL entry, if any.  An ACL entry
 * has one or more actors, and at most one of them may be a username.
 * Pass NULL if none of the actors is a username.  Usernames are limited
 * to TACL_MAXLEN_USERNAME characters.
 *
 * \param ... Zero or more parameters of type '::ta_auth_group', terminated
 * by a 0 (or taag_none) sentinel.
 */
int ta_acl_entry_new(ta_acl_entry **ret_acl_entry,
                     const char *descr, ta_oper_bf opers,
                     const char *username, ...);

/**
 * Add a new auth group to an ACL entry.
 */
int ta_acl_entry_add_auth_group(ta_acl_entry *acl_entry,
                                ta_auth_group auth_group);

/**
 * Duplicate an ACL entry.
 */
int ta_acl_entry_dup(const ta_acl_entry *src, ta_acl_entry **ret_dest);


/**
 * Create an empty ACL with no ACL entries.  Note that the description is
 * limited to TA_MAXLEN_DESCR characters.
 */
int ta_acl_new(ta_acl **ret_acl, const char *descr);


/**
 * Duplicate an ACL.
 */
int ta_acl_dup(const ta_acl *src, ta_acl **ret_dest);

/**
 * Append a single ACL entry to an existing ACL.  A copy is made of
 * the ACL entry passed in, so the caller retains ownership of it.
 */
int ta_acl_append_entry(ta_acl *acl, const ta_acl_entry *entry);


/**
 * Append a single ACL entry to an existing ACL, taking over memory
 * ownership of the ACL entry passed in.
 */
int ta_acl_append_entry_takeover(ta_acl *acl, ta_acl_entry **inout_entry);


/**
 * Append all of the entries from one ACL onto the end of another.
 */
int ta_acl_append_acl(ta_acl *acl1, const ta_acl *acl2);


/**
 * Structure to hold data to initialize an ACL from a single ACL
 * entry.  This is designed so you can define an array of these and
 * then pass it to ta_acls_init_from_entries().  This can either
 * create a new ACL with the entry specified, or append the entry
 * to an existing ACL.  See tacl_defs.c for examples.
 *
 * Note that in order to qualify to perform the listed operations,
 * the user must be a member of ALL listed auth groups, plus match
 * the specified username, if any.
 */
typedef struct ta_acl_init_from_entry {
    /**
     * A pointer to a pointer to the ACL to initialize.  If the ACL
     * pointer is NULL, a new ACL is allocated and set to contain the
     * new entry.  If the ACL pointer is non-NULL, the entry is
     * appended to the ACL.
     */
    ta_acl **taie_acl;

    /**
     * A descriptive string by which this ACL can be identified.
     * If the ACL already existed, this will replace the existing name
     * if non-NULL; specify NULL to leave the previous description.
     * Note that descriptions are limited to TA_MAXLEN_DESCR characters.
     */
    const char *taie_descr;

    /** Bit field of ::ta_oper ORed together */
    ta_oper_bf taie_opers;

    /**
     * Optional username.  Note that usernames are limited to
     * TA_MAXLEN_USENAME characters.
     */
    const char *taie_username;

    /**
     * Set of auth groups; only set as many as you need.  The user must
     * be a member of all of these auth groups (plus the username, if
     * specified) to be permitted the listed operations.
     */
    ta_auth_group taie_auth_groups[TA_ACL_INIT_MAX + 1];
} ta_acl_init_from_entry;


/**
 * Structure to hold data to initialize an ACL from one or more
 * existing ACLs.  This is designed so you can define an array of
 * these and then pass it to ta_acls_init_from_acls().  This can
 * be used to either create new ACLs or append to existing ones;
 * the specified ACLs will be concatenated and appended to the
 * ACL specified.  See tacl_defs.c for examples.
 */
typedef struct ta_acl_init_from_acls {
    /**
     * A pointer to a pointer to the ACL to initialize.  If the ACL
     * pointer is NULL, a new ACL is allocated and set to contain the
     * new entry.  If the ACL pointer is non-NULL, the entry is
     * appended to the ACL.
     */
    ta_acl **taia_acl;

    /**
     * A descriptive string by which this ACL can be identified.
     * If the ACL already existed, this will replace the existing name
     * if non-NULL; specify NULL to leave the previous description.
     * Note that descriptions are limited to TA_MAXLEN_DESCR characters.
     */
    const char *taia_descr;

    /**
     * Set of ACLs; only set as many as you need.  Note the extra
     * level of indirection, since the ACLs themselves are not
     * constants and so could not be put into a static definition.
     */
    ta_acl **taia_acls[TA_ACL_INIT_MAX + 1];
} ta_acl_init_from_acls;


/**
 * Initialize multiple ACLs from entries.
 *
 * This is a helper function to allow many ACLs to be defined in a
 * data-driven way.  Note that the last element in the array MUST be
 * empty, with the first structure member (taie_acl) being NULL, as a
 * delimiter.
 *
 * The ACLs created are owned by the variables your structure points
 * at.  However, the infrastructure will also retain the name and ACL
 * struct pointer in a table of its own, to offer centralized lookup
 * via ta_acl_lookup_by_descr().  The pointer to a pointer is retained,
 * so there is no danger of a dangling pointer; if you subsequently
 * free your ACL structure, the lookup function would return the new
 * pointer contents (presumably NULL).
 */
int ta_acls_init_from_entries(const ta_acl_init_from_entry acl_inits[]);

/** Free ACLs allocated by ta_acls_init_from_entries() */
int ta_acls_deinit_from_entries(const ta_acl_init_from_entry acl_inits[]);


/**
 * Initialize multiple ACLs from other ACLs.
 *
 * This is a helper function to allow many ACLs to be defined in a
 * data-driven way.  Note that the last element in the array MUST be
 * empty, with the first structure member (taia_acl) being NULL, as a
 * delimiter.
 *
 * The ACLs created are owned by the variables your structure points
 * at.  However, as with ta_acls_init_from_entries(), the
 * infrastructure will also retain the name and ACL struct pointer in
 * a table of its own, to offer centralized lookup via
 * ta_acl_lookup_by_descr().
 */
int ta_acls_init_from_acls(const ta_acl_init_from_acls acl_inits[]);

/** Free ACLs allocated by ta_acls_init_from_acls() */
int ta_acls_deinit_from_acls(const ta_acl_init_from_acls acl_inits[]);

/**
 * Look up an ACL that was previously created using
 * ta_acls_init_from_entries() or ta_acls_init_from_acls().
 */
int ta_acl_lookup_by_descr(const char *descr, const ta_acl **ret_acl);

/**
 * Option flags for the rendering of an ACL or ACL entry into a string.
 */
typedef enum {
    tarf_none = 0,

    /**
     * Indent each line of the ACL by three characters.
     */
    tarf_indent_3 = 1 << 0,

    /**
     * Most compact rendering: only show ACL entry names, separated by
     * commas.  Skip actual ACL entry contents, which are hopefully
     * implied by the name.
     */
    tarf_compact = 1 << 1,

} ta_acl_render_flags;

/** Bit field of ::ta_acl_render_flags ORed together */
typedef uint32 ta_acl_render_flags_bf;

/**
 * Get the string description of an ACL entry, if any.
 */
int ta_acl_entry_get_descr(const ta_acl_entry *acl_entry,
                           const char **ret_descr);


/**
 * Get the string description of an ACL, if any.
 */
int ta_acl_get_descr(const ta_acl *acl, const char **ret_descr);


/**
 * Get the number of entries in an ACL.
 */
int ta_acl_get_num_entries(const ta_acl *acl, uint32 *ret_num_entries);


/**
 * Get a human-readable string representation of the contents of an
 * ACL entry.
 */
int ta_acl_entry_to_string(const ta_acl_entry *acl_entry,
                           ta_acl_render_flags_bf flags,
                           tstring **ret_str);


/**
 * Get a human-readable string representation of the contents of an ACL.
 */
int ta_acl_to_string(const ta_acl *acl, ta_acl_render_flags_bf flags,
                     tstring **ret_str);


/**
 * Remove all operations from the specified ACL which are not in the
 * provided operation set, and remove all ACL entries which have no
 * operations left afterwards.
 */
int ta_acl_filter_operations(ta_acl *acl, ta_oper_bf opers);


/**
 * Free an ACL entry
 */
int ta_acl_entry_free(ta_acl_entry **inout_acl_entry);


/**
 * Free an ACL
 */
int ta_acl_free(ta_acl **inout_acl);


/* ========================================================================= */
/*                           Pre-Defined ACLs                                */
/* ========================================================================= */

extern ta_acl *tacl_all_any;
extern ta_acl *tacl_all_traverse;


/* ------------------------------------------------------------------------- */
/* Internal-use graft point 9950: extern declarations of ACL variables.
 * This is the Samara base platform equivalent of customer graft point #50.
 * -------------------------------------------------------------------------
 */
#ifndef TACL_NO_TMS
#undef TACL_HEADER_INC_GRAFT_POINT
#define TACL_HEADER_INC_GRAFT_POINT 9950
#include "tacl.tms.inc.h"
#endif /* TACL_NO_TMS */


/* ------------------------------------------------------------------------- */
/* Customer-specific graft point 50:
 *   - Extern-declare any ACL variables here.
 * -------------------------------------------------------------------------
 */
#ifdef INC_TACL_HEADER_INC_GRAFT_POINT
#undef TACL_HEADER_INC_GRAFT_POINT
#define TACL_HEADER_INC_GRAFT_POINT 50
#include "../include/tacl.inc.h"
#endif /* INC_TACL_HEADER_INC_GRAFT_POINT */


/* ========================================================================= */
/* For infrastructure use only -- do not call from modules.
 */
int ta_init(void);

int ta_deinit(void);

/*
 * Given a set of roles that a user has in configuration, determine
 * what auth groups they are in (and also what UNIX groups are implied
 * by these auth groups).  We follow three rules:
 *   1. Add all the auth groups that are contained by the roles.
 *   2. Add the "all" auth group, which everyone is in.
 *   3. Add all auth group implications, repeatedly until done.
 */
int ta_role_strings_to_auth_groups(const tstr_array *role_names, 
                                   uint32_array **ret_auth_groups,
                                   uint32_array **ret_unix_groups);

int ta_acl_entry_check_auth(const ta_acl_entry *acl_entry,
                            const char *username, 
                            const uint32_array *auth_groups,
                            ta_oper_bf *ret_opers_granted);

/*
 * Check to see if a given set of operations should be permitted,
 * and/or return a list of all operations which are permitted to
 * the user.
 *
 * \param acl The ACL to check.
 *
 * \param username The username of those user whose permissions are 
 * being checked.
 *
 * \param auth_groups The list of authorization to which the user
 * belongs.
 *
 * \param opers The set of operations which the user wants to be able to
 * perform.  Only if ALL of these operations are permitted will 'ret_ok'
 * get back 'true'.
 *
 * \retval ret_ok Gets 'true' iff the user is able to perform all of the
 * operations listed in opers.
 *
 * \retval ret_no_override Gets 'true' if nothing else can
 * override this determination.  This happens if the user is in the
 * 'admin' auth group; so nothing (like a custom ACL function) can 
 * change the authorization.
 *
 * \retval ret_opers_granted Returns the full set of operations which are
 * granted to this user by this ACL.  If NULL is passed here, the function
 * may return before checking all ACL entries, if the set checked so far
 * already grant all operations asked for.  However, if a pointer is passed
 * here, we have to check all ACL entries, to make sure we have a complete
 * list of permitted operations.  Therefore, only pass a pointer here if
 * you really want to know.
 */
int ta_acl_check_auth(const ta_acl *acl, const char *username, 
                      const uint32_array *auth_groups, ta_oper_bf opers,
                      tbool *ret_ok, tbool *ret_no_override,
                      ta_oper_bf *ret_opers_granted);

#ifdef __cplusplus
}
#endif

#endif /* __TACL_H_ */
