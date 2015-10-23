/*
 *
 * src/bin/snmp/tms/sn_main.h
 *
 *
 *
 */

#ifndef __SN_MAIN_H_
#define __SN_MAIN_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "ttypes.h"
#include "array.h"


/* ------------------------------------------------------------------------- */
/* main() is in snmpd.c and is part of NET-SNMP.  The Tall Maple SNMP
 * infrastructure. is one plug-in to NET-SNMP.  This file shows the
 * external interface that the Tall Maple SNMP infrastructure exposes
 * to NET-SNMP.
 */


/* ------------------------------------------------------------------------- */
/** The Tall Maple SNMP infrastructure's initialization function.
 * Make a connection to mgmtd and register our variables with NET-SNMP.
 *
 * NOTE: this (and all other SNMP MIB modules) is initialized before
 * the snmpd.conf config file is loaded.  One outcome of this is that
 * traps cannot be sent from our init function(s), as the trap
 * receivers are not yet set up.
 */
void sn_main_init(void);


/* ------------------------------------------------------------------------- */
/** Deinitialize the Tall Maple SNMP infrastructure.
 */
void sn_main_deinit(void);


/* ------------------------------------------------------------------------- */
/** The Tall Maple SNMP infrastructure's GET handlers for scalar and
 * columnar variables.  This is called when a GET or GETNEXT request
 * comes in and NET-SNMP determines that the answer or part of the 
 * answer should be one of the variables we have registered.
 * Note: NET-SNMP calls this function type FindVarMethod.
 */
u_char *
sn_get_handler_scalar(struct variable * vp,
                      oid * name,
                      size_t * length,
                      int exact,
                      size_t * var_len,
                      WriteMethod ** write_method);

u_char *
sn_get_handler_columnar(struct variable * vp,
                        oid * name,
                        size_t * length,
                        int exact,
                        size_t * var_len,
                        WriteMethod ** write_method);

/* ------------------------------------------------------------------------- */
/** The Tall Maple SNMP infrastructure's SET handlers for scalar variables.
 * When a SET request comes in, NET-SNMP first calls the GET handler, which
 * returns one of these as the WriteMethod, causing it to call us.
 */
int
sn_set_handler_scalar(int action,
                      u_char * var_val,
                      u_char var_val_type,
                      size_t var_val_len,
                      u_char * statP, oid * name, size_t name_len);

/*
 * Indicates whether or not snmpd is busy processing another request.
 * sn_awaiting_mgmt_reply cannot be used because right after a reply
 * is received it is set to false, where it could be picked up by
 * another event received at the same time as the reply.
 */
extern tbool sn_busy;


/*
 * Define this to enable debugging messages for SNMP.  Typically we'd
 * just log these at DEBUG and let syslogd (or lc_log_internal())
 * filter them.  But there is so much we want to log, including OIDs
 * which take a while to render into strings, that it is actually a
 * big performance hit just to construct the messages and have them
 * thrown away downstream.
 */
/* #define SNMP_TMS_DEBUG */

/*
 * Define this to be the logging level to be used when we report user
 * errors.  This is parameterized because it's a borderline call between
 * LOG_NOTICE and LOG_WARNING.
 *
 * XXX/EMT: there are some reported errors, particularly a lot of 
 * 'SNMP_ERR_BADVALUE' ones in sn_type_conv.c (caused by lc_err_bad_type)
 * which are currently reported at INFO; not sure if this is right...
 */
#define SNMP_USER_ERROR LOG_NOTICE

/*
 * Stash of all SNMP field registrations.  We use this at deinit to
 * clean up.
 *
 * XXX/EMT: this is incomplete, there is more to clean up.
 */
extern array *sn_fields;

#ifdef __cplusplus
}
#endif

#endif /* __SN_MAIN_H_ */
