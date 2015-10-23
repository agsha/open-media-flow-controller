/*
 *
 * src/bin/mgmtd/md_mon_map.h
 *
 *
 *
 */

#ifndef __MD_MON_MAP_H_
#define __MD_MON_MAP_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "mdb_db.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"
#include "md_moni.h"

/* ========================================================================= */
/* Function prototype for mgmtd side of monitoring infrastructure
 * =========================================================================
 */

/* ------------------------------------------------------------------------- */
/** This function does the binding name registration for mgmtd modules for
 *  variables and run-time state (i.e. the monitoring infrastructure) that
 *  is to be associated with the bindings.
 *  If the source of the state is not local to mgmtd, the caller should
 *  provide a gcl handle.
 *  It only makes sense to pass in a 'total_var_map' if the source of data
 *  is not external. Otherwise, this 'total_var_map' will exist in the
 *  external provider.
 */
int
md_mon_var_map_init(md_module *module, mi_var_map *msm_data, uint32 msm_size,
                    const char *extmon_provider, array *total_var_map);

#ifdef __cplusplus
}
#endif

#endif /* __MD_MON_MAP_H_ */
