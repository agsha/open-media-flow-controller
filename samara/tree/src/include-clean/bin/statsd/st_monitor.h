/*
 *
 * src/bin/statsd/st_monitor.h
 *
 *
 *
 */

#ifndef __ST_MONITOR_H_
#define __ST_MONITOR_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "bnode.h"
#include "bnode_proto.h"

int st_mon_init(void);
int st_mon_deinit(void);

int st_mon_handle_get(const char *bname, const tstr_array *bname_parts,
                      void *data, bn_binding_array *resp_bindings);

int st_mon_handle_iterate(const char *bname, const tstr_array *bname_parts,
                          void *data, tstr_array *resp_names);

#ifdef __cplusplus
}
#endif

#endif /* __ST_MONITOR_H_ */
