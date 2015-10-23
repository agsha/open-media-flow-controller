/*
 *
 * src/bin/snmp/tms/sn_modules.h
 *
 *
 *
 */

#ifndef __SN_MODULES_H_
#define __SN_MODULES_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


int sn_modules_load(void);
int sn_modules_init(void);
int sn_modules_deinit(void);
int sn_modules_unload(void);


#ifdef __cplusplus
}
#endif

#endif /* __SN_MODULES_H_ */
