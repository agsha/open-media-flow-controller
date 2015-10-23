/*
 *
 * src/bin/statsd/st_chd.h
 *
 *
 *
 */

#ifndef __ST_CHD_H_
#define __ST_CHD_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "st_main.h"

/*
 * NOTE: st_chd_calc_reason_map (in st_chd.c) must be kept in sync.
 */
typedef enum {
    sccr_none = 0,
    sccr_new_full_inst,
    sccr_new_partial_inst,
    sccr_timer_expired
} st_chd_calc_reason;

extern const lc_enum_string_map st_chd_calc_reason_map[];

/*
 * 'data' should point to a boolean indicating whether the new
 * instance is a partially-calculated instance (true), or not (false).
 * This is interesting only for new instances of CHDs; it should
 * always be false if the new instance is of a sample record.
 */
int st_chd_handle_new_instance(const st_chd_array *arr, uint32 idx,
                               st_chd *chd, void *data);

int st_chd_handle_timer(int fd, short event_type, void *data,
                        lew_context *ctxt);

int st_chd_calculate(st_chd *chd, st_chd_calc_reason sccr);

int st_chd_compute_next_time_interval(st_chd *chd);

int st_chd_reschedule(st_chd *chd, tbool ensure_delay);

int st_chd_lookup_by_id(const char *id, st_chd **ret_chd);

int st_chd_clear(const char *chd_id, uint32 *ret_code,
                 tstring **ret_msg);

/*
 * Iterate over all of the CHDs in the provided array and reset each
 * of their sc_next_interval_first_inst fields to 0.  This is to be
 * called when the source dataset (whether a CHD or a sample) is
 * cleared, and hence its instance ID space reset.
 */
int st_chd_reset_all(st_chd_array *chds);

void st_chd_free_for_array(void *elem);

int st_chd_builtin_funcs(const char *chd_id, const char *chd_func_name,
                         const st_dataset *source_data,
                         const tstr_array *source_nodes,
                         const tstr_array_array *source_nodes_parts, 
                         const st_dataset *chd_data,
                         void *callback_data,
                         int32 first_inst_id, int32 last_inst_id,
                         bn_binding_array *ret_new_instance);

#ifdef __cplusplus
}
#endif

#endif /* __ST_CHD_H_ */
