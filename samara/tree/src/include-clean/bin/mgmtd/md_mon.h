/*
 *
 * src/bin/mgmtd/md_mon.h
 *
 *
 *
 */

#ifndef __MD_MON_H_
#define __MD_MON_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "gcl.h"
#include "mdb_db.h"

int md_mon_handle_query_session_response(gcl_session *sess, mdb_db **main_db, 
                                         const bn_response *response);


#ifdef __cplusplus
}
#endif

#endif /* __MD_MON_H_ */
