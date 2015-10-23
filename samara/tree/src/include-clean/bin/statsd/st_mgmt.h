/*
 *
 * src/bin/statsd/st_mgmt.h
 *
 *
 *
 */

#ifndef __ST_MGMT_H_
#define __ST_MGMT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "ttime.h"
#include "mdc_backlog.h"

static const lt_dur_sec st_mgmt_timeout_sec = 150;

extern mdc_bl_context *st_blc;

int st_mgmt_init(void);

/* Second stage init: register filter on config change notifs */
int st_mgmt_init_2(void);

int st_mgmt_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __ST_MGMT_H_ */
