/*
 *
 * src/bin/snmp/tms/sn_registration.h
 *
 *
 *
 */

#ifndef __SN_REGISTRATION_H_
#define __SN_REGISTRATION_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "bnode.h"
#include "hash_table.h"
#include "sn_mod_reg.h"

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>


/* ************************************************************************* */
/* ** Infrastructure interface to SNMP module registration                ** */
/* ************************************************************************* */

extern array *sn_reg_mibs;

/* ------------------------------------------------------------------------- */
struct sn_mib {
    const char *  sm_name;           /* Name of MIB                          */
    oid *         sm_base_oid;       /* Absolute base OID of MIB             */
    uint8         sm_base_oid_len;   /* Length of base OID                   */
    array *       sm_scalars;        /* Entries of type (sn_scalar *)        */
    array *       sm_tables;         /* Entries of type (sn_table *)         */
    ht_table *    sm_notifs;         /* Keys are (const char *) event names  */
                                     /* Values are (sn_notif *)              */
    tbool         sm_hidden;         /* Don't register with NET-SNMP?        */
};


/* ------------------------------------------------------------------------- */
/* Initialize SNMP infrastructure registration.  Should be called by
 * the infrastructure before any modules try to initialize.
 */
int sn_reg_init(void);


/* ------------------------------------------------------------------------- */
/* To be called by the infrastructure after all modules have
 * registered their variables.  Converts the registrations into a form
 * palatable to NET-SNMP, and calls the NET-SNMP registration
 * function.
 */
int sn_reg_init_netsnmp(void);


/* ------------------------------------------------------------------------- */
/* Look up a scalar registration by OID.  The OID must end in ".0" to
 * match a scalar registration.
 */
int sn_reg_scalar_lookup_by_oid(const oid *scoid, size_t scoid_len,
                                const sn_scalar **ret_scalar);

/* ------------------------------------------------------------------------- */
/* Look up a table field registration by OID.  It is presumed that the
 * OID will be the OID of a table field, followed by one or more subids
 * as appropriate for the indices to that table.  We ignore the indices,
 * and just do prefix match to find an appropriate field.
 *
 * 'ret_idx_start' gets the index (in number of subids, starting from 0)
 * of the first subid AFTER the prefix which matched the field; i.e. 
 * the first subid in the first index.
 */
int sn_reg_field_lookup_by_oid(const oid *scoid, size_t scoid_len,
                               const sn_field **ret_field,
                               size_t *ret_idx_start);


#ifdef __cplusplus
}
#endif

#endif /* __SN_REGISTRATION_H_ */
