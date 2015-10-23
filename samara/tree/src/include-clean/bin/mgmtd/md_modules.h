/*
 *
 * src/bin/mgmtd/md_modules.h
 *
 *
 *
 */

#ifndef __MD_MODULES_H_
#define __MD_MODULES_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "md_mod_reg.h"

int md_load_modules(void);
int md_init_modules(mdb_reg_state *reg_state);
int md_deinit_modules(void);
int md_unload_modules(void);


#ifdef __cplusplus
}
#endif

#endif /* __MD_MODULES_H_ */
