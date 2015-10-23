/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains code which implements the Analytics Manager
 *
 * Author: Jeya ganesh babu
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <math.h>
#include "nkn_assert.h"
#include "nkn_am_api.h"
#include "nkn_am.h"
#include "nkn_mediamgr_api.h"
#include "nkn_pseudo_con.h"
#include "rtsp_om_defs.h"
#include <sys/prctl.h>
#include "nkn_lockstat.h"
#include "cache_mgr.h"
#include "nkn_namespace.h"

time_t nkn_am_hotness_time_window = 24 * 3600; /* A 24 hour window */
time_t nkn_am_hotness_prev_time_window;
time_t glob_am_hotness_interval_period = 60;
time_t nkn_am_hotness_total_interval;
uint64_t nkn_am_total_objects;
#define SEC_MUL_FACT 100

uint32_t
am_get_hit_interval(void)
{
    return (glob_am_hotness_interval_period / SEC_MUL_FACT);
}

/* Hotness Algorithm -
 *	Find the average interval period for all the objects
 *	Find the current interval period of the hit object
 *	If current Interval < 50% of Average interval - hotness += 3
 *	If current Interval > 50% of Average interval - hotness += 2
 *	If current Interval > Average Interval - hotness += 0
 *	If current Interval > 150% of Average interval - hotness -= 1
 *	Reset the hotness average every 24 hour.
 *	Decay done aggressively when the previous hit had
 *	    happened before 24 hour. The object's hotness is restarted
 *	    with the lowest limit.
 */
nkn_hot_t
am_update_hotness(nkn_hot_t *in_hotness, int num_hits, time_t object_ts,
		    uint64_t hotness_inc, char *uri)
{
    int    hotness = am_decode_hotness(in_hotness);
    int    hit_count = 1;
    int	   change = 0;
    time_t interval_check_start = am_decode_interval_check_start(in_hotness);
    nkn_hot_t out_hotness = 0;
    time_t cur_interval_period;
    int	    old_hotness;

    if(num_hits)
	hit_count = num_hits;

    if(!object_ts)
	object_ts = nkn_cur_ts;

    /* Use a scale factor of 100 to avoid rounding off due to integer division
     * when calculating glob_am_hotness_interval_period
     */
    cur_interval_period = ((object_ts - interval_check_start)/hit_count) *
				SEC_MUL_FACT;

    if(nkn_am_hotness_prev_time_window) {
	if((nkn_cur_ts - nkn_am_hotness_prev_time_window) >
		nkn_am_hotness_time_window) {
	    nkn_am_hotness_prev_time_window = nkn_cur_ts;
	}
    } else
	nkn_am_hotness_prev_time_window = nkn_cur_ts;

    if(!interval_check_start ||
	    (cur_interval_period/SEC_MUL_FACT) > nkn_am_hotness_time_window) {
	DBG_LOG(MSG3, MOD_AM, "Hotness update for uri %s hit %d objectts %ld"
		" cur_interval %ld interval_check_start %ld cur_time %ld",
		uri, num_hits, object_ts, cur_interval_period,
		interval_check_start, nkn_cur_ts);
	interval_check_start = object_ts;
	hotness = (num_hits * 3);
    } else {
	DBG_LOG(MSG3, MOD_AM, "Hotness update for uri %s hit %d"
		" objectts %ld cur_interval %ld interval_check_start %ld"
		" cur_time %ld total_int %ld glob_interval %ld"
		" total_hits %ld hotness %d", uri,
		num_hits, object_ts, cur_interval_period,
		interval_check_start, nkn_cur_ts, nkn_am_hotness_total_interval,
		glob_am_hotness_interval_period, nkn_am_total_objects, hotness);
	old_hotness = hotness;
	if (num_hits) {
	    nkn_am_total_objects += num_hits;
	    hotness += num_hits;
	    nkn_am_hotness_total_interval += (cur_interval_period * num_hits);
	    glob_am_hotness_interval_period = nkn_am_hotness_total_interval/
						    nkn_am_total_objects;
	}

	/* Reset the interval check start */
	interval_check_start = object_ts;

	if(cur_interval_period <= (50 * glob_am_hotness_interval_period)/100) {
	    hotness += (num_hits * 2);
	} else if(cur_interval_period > (glob_am_hotness_interval_period +
		    ((50 * glob_am_hotness_interval_period)/100))) {
	    change = num_hits?(num_hits*2):2;
	    hotness -= change;
	} else if(cur_interval_period <= glob_am_hotness_interval_period) {
	    change = num_hits?(num_hits*1):1;
	    hotness += change;
	} else {
	    change = num_hits?(num_hits*1):1;
	    hotness -= change;
	}

	/* If the byte served based hotness is present, then the hit
	 * based hotness increment should not be done. The hit based
	 * hotness should be done only when there is a decay (old_hotness
	 * > hotness)
	 */
	if(hotness_inc && (old_hotness < hotness))
	    hotness = old_hotness + hotness_inc;
	else
	    hotness += hotness_inc;

	DBG_LOG(MSG3, MOD_AM, "Hotness update for uri %s hit %d"
		" objectts %ld cur_interval %ld interval_check_start %ld"
		" cur_time %ld total_int %ld glob_interval %ld"
		" total_hits %ld hotness %d", uri,
		num_hits, object_ts, cur_interval_period,
		interval_check_start, nkn_cur_ts, nkn_am_hotness_total_interval,
		glob_am_hotness_interval_period, nkn_am_total_objects, hotness);

	/* Minimum Hotness */
	if(num_hits && hotness < 3)
	    hotness = 3;
	else if (hotness < 0)
	    hotness = 0;
	/* Maximum hotness */
	if(hotness > 0xffff)
	    hotness = 0xffff;
    }
    am_encode_hotness(&out_hotness, hotness);
    am_encode_update_time(&out_hotness, object_ts);
    am_encode_interval_check_start(&out_hotness, interval_check_start);
    return out_hotness;
}

void
am_encode_hotness(nkn_hot_t *out_hotness, int32_t in_temp)
{
    if(in_temp > AM_MAX_SHORT_VAL)
	in_temp = AM_MAX_SHORT_VAL;
    *out_hotness = ((uint32_t)in_temp & (0xffff))
	| (*out_hotness & 0xffffffffffff0000);
}

void
am_encode_update_time(nkn_hot_t *out_hotness, time_t object_ts)
{
    if(object_ts)
	*out_hotness = ((object_ts & 0xffffffff) << 32) |
	    (*out_hotness & 0xffffffff);
    else
	*out_hotness = ((nkn_cur_ts & 0xffffffff) << 32) |
	    (*out_hotness & 0xffffffff);
    assert(nkn_cur_ts != 0);
    assert(am_decode_update_time(out_hotness) != 0);
}

void
am_encode_interval_check_start(nkn_hot_t *out_hotness, time_t object_ts)
{
    uint32_t update_time = ((*out_hotness >> 32) & 0xffffffff);
    int32_t interval     = update_time - object_ts;

    if(interval > AM_MAX_SHORT_VAL) {
	interval = AM_MAX_SHORT_VAL;
    }
    *out_hotness = ((interval & 0xffff) << 16) |
		    (*out_hotness & 0xffffffff0000ffff);
}

int32_t
am_decode_hotness(nkn_hot_t *in_hotness)
{
    int hotness;

    /* Absolute value is the first 31 bits */
    hotness = (int)(*in_hotness & 0xffff);
    return (hotness);
}

uint32_t
am_decode_update_time(nkn_hot_t *in_hotness)
{
    return ((*in_hotness >> 32) & 0xffffffff);
}

uint32_t
am_decode_interval_check_start(nkn_hot_t *in_hotness)
{
    uint32_t update_time = ((*in_hotness >> 32) & 0xffffffff);
    int32_t interval_diff = ((*in_hotness >> 16) & 0xffff);
    return (update_time - interval_diff);
}
