/*
 *
 * src/bin/snmp/tms/sn_notifs.h
 *
 *
 *
 */

#ifndef __SN_NOTIFS_H_
#define __SN_NOTIFS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "bnode_proto.h"


/* ------------------------------------------------------------------------- */
/** Handle an event request.  Extract the event name and bindings from
 * the event and call sn_notif_handle_event().
 */
int sn_notif_handle_event_request(const bn_request *event_request);


#ifdef __cplusplus
}
#endif

#endif /* __SN_NOTIFS_H_ */
