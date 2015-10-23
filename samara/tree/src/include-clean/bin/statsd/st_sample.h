/*
 *
 * src/bin/statsd/st_sample.h
 *
 *
 *
 */

#ifndef __ST_SAMPLE_H_
#define __ST_SAMPLE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "st_main.h"

int st_sample_handle_timer(int fd, short event_type, void *data,
                           lew_context *ctxt);

/*
 * Capture a sample immediately.  Note that this does not do any
 * checking or updating of the backlog mechanism -- it assumes that
 * its caller is already "protected".
 */
int st_sample_capture(st_sample *sample);

int st_sample_reschedule(st_sample *sample, tbool immediate, 
                         tbool ensure_delay);

int st_sample_lookup_by_id(const char *id, st_sample **ret_sample);

int st_sample_clear(const char *sample_id, uint32 *ret_code,
                    tstring **ret_msg);

/* Pass NULL for sample_id to force all samples */
int st_sample_force(const char *sample_id, uint32 *ret_code,
                    tstring **ret_msg);

void st_sample_free_for_array(void *elem);


#ifdef __cplusplus
}
#endif

#endif /* __ST_SAMPLE_H_ */
