/*
 *
 * src/bin/snmp/tms/sn_config.h
 *
 *
 *
 */

#ifndef __SN_CONFIG_H_
#define __SN_CONFIG_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "gcl.h"
#include "bnode.h"
#include "md_client.h"


int sn_config_init(void);

int sn_config_deinit(void);

int sn_config_change_handle(const bn_binding_array *bindings,
                            uint32 idx, const bn_binding *binding,
                            const tstring *name,
                            const tstr_array *name_components,
                            const tstring *name_last_part,
                            const tstring *value, void *callback_data);

extern tstr_array *sn_config_event_names;

/* ............................................................................
 * Our records of SNMP v3 users and communities.  We need to know at
 * least whether or not they are permitted to use SNMP SETs.
 *
 * We'll have one record for communities, and it will be identifiable
 * by a NULL username.
 *
 * XXX/EMT: later, we'll also know their capability class or set of
 * ACL roles.
 */
typedef struct sn_snmp_v3_user {
    tbool ssvu_is_community; /* If true, ssvu_username is NULL */
    char *ssvu_username;
    tbool ssvu_enabled;
    tbool ssvu_permit_sets;
    uint32 ssvu_gid;
    gcl_session *ssvu_sess;
    md_client_context *ssvu_mcc;
    
    /*
     * This just helps us keep track of whether our session is up to
     * date.  If the gid of the account changes, we have to break and
     * reform our session (and md_client_context).
     */
    uint32 ssvu_session_gid;

    /*
     * Keep track of the last time we processed a SET from this user.
     * If we end up with too many GCL sessions open, we may close the
     * session associated with the user who was least recently active.
     *
     * This is a system uptime (not vulnerable to clock changes),
     * rather than a datetime.
     */
    lt_dur_ms ssvu_last_activity;

} sn_snmp_v3_user;

TYPED_ARRAY_HEADER_NEW_NONE(sn_snmp_v3_user, struct sn_snmp_v3_user *);

extern sn_snmp_v3_user_array *sn_snmp_v3_users;

/*
 * Look up an SNMP v3 user record by name.
 * 
 * create_if_not_found: true means to create a new record (with the
 * username filled in) if one is not found.  False means to return
 * NULL if none is found.
 *
 * delete: true means to delete the record if it is found (and return
 * NULL for the user record).  False means to leave it alone (and return
 * whatever we find).  Mutually exclusive with create_if_not_found.
 */
int sn_config_lookup_user(tbool is_community, const char *username, 
                          tbool create_if_not_found, tbool delete,
                          sn_snmp_v3_user **ret_user);

#ifdef __cplusplus
}
#endif

#endif /* __SN_CONFIG_H_ */
