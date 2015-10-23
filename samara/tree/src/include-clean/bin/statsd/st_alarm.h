/*
 *
 * src/bin/statsd/st_alarm.h
 *
 *
 *
 */

#ifndef __ST_ALARM_H_
#define __ST_ALARM_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "st_main.h"

int st_alarm_new(const char *alarm_id, st_alarm **ret_alarm);

int st_alarm_array_new(st_alarm_array **ret_arr, tbool own_alarms);

int st_alarm_handle(const st_alarm_array *arr, uint32 idx, st_alarm *alarm_rec,
                    void *data);

int st_alarm_check(st_alarm *alarm_rec);

int st_alarm_lookup_by_id(const char *id, st_alarm **ret_alarm,
                          int32 *ret_idx);

int st_alarm_get_node_rec(st_alarm *alarm_rec, const char *node_name,
                          tbool can_create, st_alarm_node **ret_alarm_node);

int st_alarm_clear(const char *alarm_id, uint32 *ret_code,
                   tstring **ret_msg);

int st_alarm_clear_one(st_alarm *alarm_rec, tbool log);

int st_alarm_reset_rate_limit(const char *alarm_id);

int st_alarm_reset_rate_limit_one(st_alarm *alarm_rec);

int st_alarm_set_suspended(const char *alarm_id, tbool suspended);

int st_alarm_set_suspended_one(st_alarm *alarm_rec, tbool suspended);

/*
 * Remove any nodes in sa_nodes whose names don't match our node
 * pattern.  Generally to be done whenever the set of node patterns
 * (from sa_node_pattern and sa_node_patterns) changes.
 */
int st_alarm_cull_nodes(st_alarm *alarm_rec);

#ifdef __cplusplus
}
#endif

#endif /* __ST_ALARM_H_ */
