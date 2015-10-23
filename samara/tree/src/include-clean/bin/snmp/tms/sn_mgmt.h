/*
 *
 * src/bin/snmp/tms/sn_mgmt.h
 *
 *
 *
 */

#ifndef __SN_MGMT_H_
#define __SN_MGMT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "bnode.h"
#include "bnode_proto.h"
#include "md_client.h"
#include "sn_config.h"

/*
 * NOTE: SNMP modules MUST NOT use this client context to make
 *       mgmtd requests!  This is for snmpd's own internal use only.
 *       Instead, call sn_get_mcc() or sn_get_admin_mcc() to get the 
 *       context to use.
 */
extern md_client_context *sn_int_mcc;

extern tbool sn_mgmt_request_cancelled;


/* ------------------------------------------------------------------------- */
/** Establish a connection with mgmtd and prepare to send requests and
 * receive replies.
 */
int sn_mgmt_init(void);


/* ------------------------------------------------------------------------- */
/** Close our connection with mgmtd.
 */
int sn_mgmt_shutdown(void);


/* ------------------------------------------------------------------------- */
/** Send a message to the management daemon, and return the response.
 * The response structure is the caller's responsibility to free.
 */
int sn_mgmt_send_msg(gcl_session *sess, bn_request *req,
                     bn_response **ret_resp);


/* ------------------------------------------------------------------------- */
/* Number of seconds before we'll give up on receiving a reply to a
 * synchronous request sent to the management daemon.  If a request might
 * take longer than this, it would probably be best done asynchronously.
 * This can be overridden by setting SNMP_MGMTD_TIMEOUT_SEC in customer.h.
 */
#ifndef SNMP_MGMTD_TIMEOUT_SEC
#define SNMP_MGMTD_TIMEOUT_SEC 65
#endif

static const lt_dur_sec sn_mgmt_timeout_sec = SNMP_MGMTD_TIMEOUT_SEC;


/* ------------------------------------------------------------------------- */
/* Handle queued-up events that occurred while we were in the middle
 * of doing something else.
 */
int sn_mgmt_handle_queued(void);


/* ------------------------------------------------------------------------- */
/* Make sure this user has a GCL session as the correct gid, and an
 * md_client_context on which it can send requests.
 */
int sn_mgmt_ensure_session(sn_snmp_v3_user *user, tbool want_sess);


#ifdef __cplusplus
}
#endif

#endif /* __SN_MGMT_H_ */
