/*
 *
 * src/bin/mgmtd/md_ext.h
 *
 *
 *
 */

#ifndef __MD_EXT_H_
#define __MD_EXT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "mdb_db.h"

int md_extmon_target_handler(mdb_query_target_type target_type,
                             mdb_query_state *inout_query_state);

#ifdef __cplusplus
}
#endif

#endif /* __MD_EXT_H_ */
