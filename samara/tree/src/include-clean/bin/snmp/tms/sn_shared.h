/*
 *
 * src/bin/snmp/tms/sn_shared.h
 *
 *
 *
 */

#ifndef __SN_SHARED_H_
#define __SN_SHARED_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


/*
 * This header file contains definitions shared between Samara and
 * some NET-SNMP code.  Because it is #included by NET-SNMP, it should
 * not include any of our standard headers.
 */

typedef struct sn_netsnmp_info {

    /* ........................................................................
     * Info about how this request was authenticated.
     */

    /*
     * SNMP_SEC_MODEL_SNMPv1, SNMP_SEC_MODEL_SNMPv2c, or
     * SNMP_SEC_MODEL_USM.
     */
    int sni_security_model;

    /*
     * If sni_security_model was SNMP_SEC_MODEL_SNMPv1 or
     * SNMP_SEC_MODEL_SNMPv2c, the community name which was used to
     * authenticate.
     */
    char *sni_community;

    /*
     * If sni_security_model was SNMP_SEC_MODEL_USM, the username of
     * the SNMP v3 user with which the request was authenticated.
     */
    char *sni_security_name;

    /* ........................................................................
     * Other info.
     */

    /*
     * SNMP request ID of the active request.
     */
    int sni_reqid;

    /*
     * Current request mode (MODE_SET_RESERVE1, etc.)  This is passed 
     * directly to the SET handlers, so they don't need it.  But it is
     * NOT passed to the GET handlers, and they DO need it: if the request
     * in progress is actually a SET, they don't want to do the work of
     * looking up the value, since no one actually cares.
     */
    int sni_req_mode;

} sn_netsnmp_info;


extern sn_netsnmp_info sn_global_netsnmp_info;


/*
 * The maximum number of additional fds we'll register with NET-SNMP
 * for use with the GCL.  If there were a lot of active users, we
 * might have otherwise exceeded this number.  But we'll watch
 * ourselves, and keep to this limit.
 */
#define NUM_GCL_EXTRA_FDS 32


#ifdef __cplusplus
}
#endif

#endif /* __SN_SHARED_H_ */
