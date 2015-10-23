/*
 *
 * src/bin/statsd/st_modules.h
 *
 *
 *
 */

#ifndef __ST_MODULES_H_
#define __ST_MODULES_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

int st_modules_load(void);
int st_modules_init(void);
int st_modules_deinit(void);
int st_modules_unload(void);

#ifdef __cplusplus
}
#endif

#endif /* __ST_MODULES_H_ */
