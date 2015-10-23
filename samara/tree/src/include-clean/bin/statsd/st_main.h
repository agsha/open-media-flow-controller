/*
 *
 * src/bin/statsd/st_main.h
 *
 *
 *
 */

#ifndef __ST_MAIN_H_
#define __ST_MAIN_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


#include "common.h"
#include "ttime.h"
#include "bnode.h"
#include "st_data.h"
#include "libevent_wrapper.h"

typedef struct st_sample st_sample;
typedef struct st_chd st_chd;
typedef struct st_alarm st_alarm;
typedef struct st_alarm_node st_alarm_node;

TYPED_ARRAY_HEADER_NEW_FULL(st_sample, st_sample *);
TYPED_ARRAY_HEADER_NEW_FULL(st_chd, st_chd *);
TYPED_ARRAY_HEADER_NEW_NONE(st_alarm, st_alarm *);
TYPED_ARRAY_HEADER_NEW_NONE(st_alarm_node, st_alarm_node *);


/* ------------------------------------------------------------------------- */
/* What severity level should normal stats daemon operational messages
 * be logged at?  This is generally DEBUG for a production system,
 * but wants to be INFO for stats daemon debugging, or when
 * we get decent log filtering.
 */
#define ST_NLL LOG_DEBUG

/*
 * These can be used to enable some additional debug logging which we
 * don't normally want to log even at LOG_DEBUG, both for performance
 * reasons and because they clutter the logs.  Although these can be
 * enabled from here, if you are a customer you should enable them
 * from your customer.h instead.
 *
 * Each of these enables a separate set of additional log messages.
 */
/* #define ST_DEBUG1 */
/* #define ST_DEBUG2 */
/* #define ST_DEBUG3 */


/*
 * This enables some performance instrumentation.
 */
/* #define ST_PERF */


/* ------------------------------------------------------------------------- */
/* Sample records: st_sample
 *
 * Record for one node that is configured to be sampled.  This holds
 * the configuration for the sample, as well as the cache of sampled
 * data not yet written to disk.
 */

typedef enum {
    ssm_none = 0, ssm_direct, ssm_delta, ssm_aggregate
} st_sample_method;

static lc_enum_string_map st_sample_method_map[] = {
    {ssm_none, "none"},
    {ssm_direct, "direct"},
    {ssm_delta, "delta"},
    {ssm_aggregate, "aggregate"},
    {0, NULL}
};

typedef enum {
    sdc_none = 0, sdc_increasing, sdc_decreasing
} st_delta_constraint;

static lc_enum_string_map st_delta_constraint_map[] = {
    {sdc_none, "none"},
    {sdc_increasing, "increasing"},
    {sdc_decreasing, "decreasing"},
    {0, NULL}
};

struct st_sample {
    /* ........................................................................
     * Configuration
     */
    tstring *            ss_id;
    tbool                ss_enable;
    tstr_array *         ss_node_patterns;
    tstr_array_array *   ss_node_patterns_parts;
    lt_dur_sec           ss_interval;
    uint32               ss_num_to_keep;
    st_sample_method     ss_sample_method;
    st_delta_constraint  ss_delta_constraint;  /* If sample_method is delta */
    tbool                ss_delta_average;
    uint32               ss_sync_interval;
    int32                ss_num_to_cache;

    /*
     * These are the CHDs and alarms that depend on this sample.
     * Each time a new sample is taken, we call the handlers for each
     */
    st_chd_array *     ss_chds;
    st_alarm_array *   ss_alarms;

    /* ........................................................................
     * Runtime state
     */
    st_dataset *       ss_data;
    lew_event *        ss_next;
    uint32             ss_samples_until_flush;
    lt_time_ms         ss_next_sample;

    /*
     * If the sampling method is 'delta' these two parallel arrays will
     * hold information about the last sample of every node taken.
     * ss_last_samples holds the last sample taken; and
     * ss_last_sample_inst_ids holds the instance ID of that sample.
     */
    bn_binding_array * ss_last_samples;
    uint32_array *     ss_last_sample_inst_ids;
};


/* ------------------------------------------------------------------------- */
/* Computed Historical Datapoint records: st_chd
 *
 * Record for one CHD that is configured to be computed.  This holds
 * the configuration for the CHD, as well as the cache of computed
 * data not yet written to disk.
 */

typedef enum {
    sst_none = 0,
    sst_sample,
    sst_chd
} st_source_type;

static lc_enum_string_map st_source_type_map[] = {
    {sst_none, "none"},
    {sst_sample, "sample"}, 
    {sst_chd, "chd"},
    {0, NULL}
};

typedef enum {
    slt_none = 0,
    slt_instances,
    slt_time
} st_select_type;

static lc_enum_string_map st_select_type_map[] = {
    {slt_none, "none"},
    {slt_instances, "instances"}, 
    {slt_time, "time"},
    {0, NULL}
};

typedef enum {
    scf_none = 0,
    scf_min,
    scf_max,
    scf_mean,
    scf_first,
    scf_last,
    scf_sum,
    scf_custom,
} st_chd_function;

static lc_enum_string_map st_chd_function_map[] = {
    {scf_none, "none"},
    {scf_min, "min"},
    {scf_max, "max"},
    {scf_mean, "mean"},
    {scf_first, "first"},
    {scf_last, "last"},
    {scf_sum, "sum"},
    {scf_custom, "custom"},
    {0, NULL}
};

#include "st_registration.h"

struct st_chd {
    /* ........................................................................
     * Configuration
     */
    tstring *          sc_id;
    tbool              sc_enable;
    st_source_type     sc_source_type;
    tstring *          sc_source_id;     /*  So we can resolve to ptr later */
    tstr_array *       sc_source_nodes;
    tstr_array_array * sc_source_nodes_parts;
    st_sample *        sc_source_sample; /*  If source type is sst_sample   */
    st_chd *           sc_source_chd;    /*  If source type is sst_chd      */
    uint32             sc_num_to_keep;
    st_select_type     sc_select_type;
    uint32             sc_num_to_use;     /*  If select_type is instances   */
    uint32             sc_num_to_advance; /*  If select_type is instances   */
    lt_dur_sec         sc_interval_len;   /*  If select_type is time        */
    lt_dur_sec         sc_interval_dist;  /*  If select_type is time        */
    lt_dur_sec         sc_interval_phase; /*  If select_type is time        */
    tstring *          sc_function_name;

    /*
     * sc_function is an enum that tells which built-in function, or
     * scf_custom if the function is a custom one.  But we store something
     * in sc_function_callback regardless: if it's a built-in, we store
     * st_chd_builtin_funcs() there, and sc_function_callback_data simply
     * has sc_function cast as a void *.  This way, there is no special
     * case when it comes time to do the calculation.
     * 
     */
    st_chd_function    sc_function;
    st_chd_callback    sc_function_callback;
    void *             sc_function_callback_data;

    uint32             sc_sync_interval;
    int32              sc_num_to_cache;
    tbool              sc_calc_partial;
    tbool              sc_alarm_partial;

    /*
     * These are the CHDs and alarms that depend on this CHD.
     * Each time a new CHD is computed, we call the handlers for each
     */
    st_chd_array *     sc_chds;
    st_alarm_array *   sc_alarms;

    /* ........................................................................
     * Runtime state
     */
    st_dataset *       sc_data;        /* The CHD data                       */
    st_dataset *       sc_source_data; /* The data on which the CHD is based */
    lew_event *        sc_next;
    uint32             sc_chds_until_flush;
    
    /*
     * If select type is slt_instances, what is/was the first instance ID
     * to be used in the first calculation of the CHD?  If this is
     * negative, it means to compute the CHD as soon as there are enough
     * source instances to do so.
     */
    int32             sc_next_interval_first_inst;

    /*
     * Is the sc_next_interval_first_inst trustworthy yet?
     * We first set sc_next_interval_first_inst at the end of
     * initialization, based on the set of instances available
     * in the source dataset at that time, but this number may
     * be wrong, since a new instance could arrive before we are
     * computed.  We are only setting it to have a "straw man"
     * number to report from our monitoring nodes.  Once we 
     * calculate for the first time, we update it according to
     * the set of available instance, and consider it "anchored",
     * and therefore trustworthy from then on.
     */
    tbool             sc_next_interval_first_inst_anchored;

    /*
     * If select type is slt_time, sc_next_interval_begin and
     * sc_next_interval_end mark the time range over which the next
     * instances of this CHD should be calculated.
     */
    lt_time_sec        sc_next_interval_begin;
    lt_time_sec        sc_next_interval_end;

    /*
     * Configuration of this CHD has recently changed, and we need to
     * reschedule it.  We use this flag, rather than rescheduling on
     * the spot, because we are processing changes one at a time, and
     * want to make sure we have seen all the changes before we do any
     * rescheduling.
     */
    tbool              sc_resched;

};


/* ------------------------------------------------------------------------- */
/* Alarm records: st_alarm
 *
 * Record for one configured alarm.  This holds the configuration for
 * the alarm, as well as all of its runtime state.
 */

typedef enum {
    stt_none = 0, stt_sample, stt_chd
} st_trigger_type;

static lc_enum_string_map st_trigger_type_map[] = {
    {stt_none, "none"},
    {stt_sample, "sample"}, 
    {stt_chd, "chd"},
    {0, NULL}
};

/* ............................................................................
 * Per-node, per-condition runtime state
 */
typedef struct st_alarm_node_cond {
    lt_time_sec        sanc_last_error;
    lt_time_sec        sanc_last_clear;
    tbool              sanc_error;
    bn_attrib *        sanc_watermark;
} st_alarm_node_cond;

/* ............................................................................
 * Per-node runtime state
 */
struct st_alarm_node {
    tstring *          san_node_name;
    lt_time_sec        san_last_check_time;
    bn_attrib *        san_last_check_value;
    st_alarm_node_cond san_cond_rising;
    st_alarm_node_cond san_cond_falling;
};

/* ............................................................................
 * Per-condition configuration
 */
typedef struct st_alarm_cond {
    tbool              sac_enable;
    bn_attrib *        sac_error_threshold;
    bn_attrib *        sac_clear_threshold;
    tbool              sac_event_on_clear;
} st_alarm_cond;

/* ............................................................................
 * Rate-limit buckets
 */
typedef enum {
    srlbt_short = 0, srlbt_medium, srlbt_long, srlbt_max
} st_rlim_bucket_type;

/* ............................................................................
 * Alarm types
 */
typedef enum {
    sat_error = 0, sat_clear, sat_max
} st_alarm_type;

/* ......................................................................... */
/* Event repeat policy.
 *
 * This enum, set on an alarm, specifies whether or not, and under
 * what conditions, we should resend events we have already sent.
 */
typedef enum {
    saer_none = 0,
    
    /*
     * Normal behavior: send an event only when the alarm changes
     * state.
     */
    saer_single,

    /*
     * In addition to normal events, also resend the error event
     * periodically as long as the alarm is in an error state (i.e. as
     * long as it has not passed back over its clear threshold).
     */
    saer_while_not_cleared,

    /*
     * In addition to normal events, also continue to send the error
     * event as long as the alarm has not passed back over its error
     * threshold.
     *
     * This is similar to ::saer_while_not_cleared, but with the
     * difference coming when the alarm has passed back over the
     * alarm threshold but not yet back over the clear threshold.
     * This option would stop resending events at that point, while
     * the other option would continue resending them.
     *
     * XXX/EMT: clarify what happens if the value passes back over the
     * alarm threshold.
     *
     * XXX/EMT: this option is not yet implemented, and the mgmtd
     * module will prevent us from getting this value.
     */
    saer_while_error,

} st_alarm_event_repeat;

static lc_enum_string_map st_alarm_event_repeat_map[] = {
    {saer_single,                "single"},
    {saer_while_not_cleared,     "while_not_cleared"},
    {saer_while_error,           "while_error"},
    {0,                          NULL}
};

struct st_alarm {
    /* ........................................................................
     * Alarm-wide configuration
     */
    tstring *          sa_id;
    tbool              sa_enable;
    tbool              sa_ignore_first_value;
    tstring *          sa_trigger_type_str;
    tstring *          sa_trigger_id;
    st_trigger_type    sa_trigger_type;
    st_sample *        sa_trigger_sample; /*  If trigger type is stt_sample  */
    st_chd *           sa_trigger_chd;    /*  If trigger type is stt_chd     */
    tstring *          sa_node_pattern;
    tstr_array *       sa_node_patterns;
    tstring *          sa_event_name_root;
    st_alarm_cond      sa_cond_rising;
    st_alarm_cond      sa_cond_falling;
    tbool              sa_clear_if_missing;
    lt_dur_sec         sa_rate_limit_win[srlbt_max];
    uint32             sa_rate_limit_max[srlbt_max];
    st_alarm_event_repeat sa_repeat;

    /* ........................................................................
     * Alarm-wide runtime state
     */
    tstring *          sa_last_event_name;
    lt_time_sec        sa_last_event_time;
    lt_time_sec        sa_last_reset_time;
    lt_time_sec        sa_last_rate_limit_start[sat_max][srlbt_max];
    uint32             sa_rate_limit_count[sat_max][srlbt_max];
    uint32             sa_rate_limit_skipped[sat_max];
    tbool              sa_suspended;
    st_alarm_node_array *sa_nodes;
    tbool              sa_dirty;
    
    /*
     * Starts at 0 and increments every time the alarm is tested.
     * Used to enforce the ignore_first_value option.
     */
    uint32             sa_test_idx;
};


/* ------------------------------------------------------------------------- */
/* Instances of samples, CHDs, and alarms.  These hold all of the
 * active records in the stats daemon.
 */
extern st_chd_array *st_chds;
extern st_alarm_array *st_alarms;
extern st_sample_array *st_samples;


/* ------------------------------------------------------------------------- */
/* Global configuration
 */
extern int32 st_global_num_to_cache;


/* ------------------------------------------------------------------------- */
/* Keep track of whether the stats daemon is in the process of exiting,
 * as this may affect our responses to some events.
 */
extern tbool st_exiting;

/* ------------------------------------------------------------------------- */
/* Keep track of whether PM is in the process of shutting down the
 * system.  We may not have been requested to exit yet, but if we're
 * in any part of the shutdown process, we may need to act differently,
 * e.g. suspend sending events for alarms.
 */
extern tbool st_shutdown_imminent;

extern tstring *st_hostname;

int st_initiate_exit(void);


/* ------------------------------------------------------------------------- */
/* Context for libevent wrapper
 */
extern lew_context *st_lwc;

extern tbool st_is_initializing;

#ifdef __cplusplus
}
#endif

#endif /* __ST_MAIN_H_ */
