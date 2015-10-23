/*
 *	Author: Jeya ganesh babu
 *
 * Copied from nvsd/am
 *
 *      Copyright (c) Ankeena Networks, 2008-09
 *	Copyright (c) Juniper Networks, 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <assert.h>
#include <math.h>

#include "nkn_am_api.h"
#include "nkn_am.h"
#include "nkn_mediamgr_api.h"
#define AM_DECAY_DEF_RATIO (30 * 60) /* Set it as 0.5 hours */

void
am_encode_hotness(nkn_hot_t *out_hotness, int32_t in_temp)
{
    /*
     * The temperature can only be 16 bits, but in_temp is 32 bits.
     * Within NVSD the hotness temperature cannot be negative but this
     * temperature has had decay applied against it, so it can go negative.
     * It is important to preserve the negative value because the decay is
     * aggressive and almost all objects can have negative temperatures.
     *
     * Within nvsd, objects can achieve hotness values of 64K, so it isn't
     * ideal to be chopping values as 32K.  It is possible to drop the interval
     * and use the full 32-bits but this code is good enough, given that we
     * plan to depracate external eviction soon.
     */
    if (in_temp > 0x7FFF) {
	/* Preserve sign of large positive integer */
	in_temp = 0x7FFF;
    } else if (in_temp < (signed short)0x8000) {
	/* Preserve sign of large negative integer */
	in_temp = 0x8000;
    }
    *out_hotness = ((uint32_t)in_temp & (0xffff))
	| (*out_hotness & 0xffffffffffff0000);
}

void
am_encode_update_time(nkn_hot_t *out_hotness, time_t object_ts)
{
    *out_hotness = ((object_ts & 0xffffffff) << 32) |
	    (*out_hotness & 0xffffffff);
}

void
am_encode_interval_check_start(nkn_hot_t *out_hotness, time_t object_ts)
{
    uint32_t update_time = ((*out_hotness >> 32) & 0xffffffff);
    int32_t interval     = update_time - object_ts;

    if (interval > AM_MAX_SHORT_VAL) {
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

extern void my_AM_decay_hotness(nkn_hot_t *in_seed,
				time_t cur_time,
				int sync_interval,
				uint64_t size,
				float A,
				float B,
				float C,
				float D,
				float E);

void
my_AM_decay_hotness(nkn_hot_t *in_seed,
		    time_t cur_time,
		    int sync_interval,
		    uint64_t	    size,
		    float	    A,
		    float	    B,
		    float	    C,
		    float	    D,
		    float	    E)
{
    int in_time, hotness_diff;
    int in_hotness, in_hits;
    float in_hotnessf, sizef;
    GTimeVal diff;
    AM_obj_t pobj;

    memset(&pobj, 0, sizeof(pobj));
    in_hotness = am_decode_hotness(in_seed);
    in_time = am_decode_update_time(in_seed);
    sizef = size;

    hotness_diff = (cur_time - in_time)/sync_interval;

    /*
     * This code was designed for 2.0.X hotness.  It doesn't fit 100%
     * for the new hotness algorithm which has decay capability built-in.
     * 'in_hotness / 3' may not be the number of hits.  I'm willing to live
     * with this because external eviction is getting depracated.
     *
     */
    in_hits = in_hotness / 3;
    in_hotnessf = A * in_hits;
    in_hotnessf += (B * size);
    in_hotnessf += (C * hotness_diff);
    in_hotnessf += (D * in_hits * size);
    in_hotnessf += (E * in_hits * log(sizef));
    in_hotness = in_hotnessf;

#if 0
    s_update_new_hit_average(&pobj, diff);
    s_update_new_freq(&pobj, pobj.stats.total_num_hits, diff);
#endif

    am_encode_hotness(in_seed, in_hotness);
    am_encode_update_time(in_seed, cur_time);
}
