/*
 * src/bin/statsd/st_registration.h
 */

#ifndef __ST_REGISTRATION_H_
#define __ST_REGISTRATION_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "st_data.h"
#include "bnode_proto.h"
#include "md_client.h"

/** \defgroup stats Stats daemon module API */

/**
 * \file src/bin/statsd/st_registration.h Stats daemon module API
 * \ingroup stats
 */

/******************************************************************************
 ******************************************************************************
 *** Module API
 ******************************************************************************
 ******************************************************************************
 ***/

typedef struct st_report_reg st_report_reg;

extern md_client_context *st_mcc;

/* ------------------------------------------------------------------------- */
/** Stats custom CHD callback.  Called by stats daemon to compute a
 * custom CHD that was registered with the infrastructure at init
 * time.
 *
 * \param chd_id ID of the CHD we are being called to compute.
 * NOTE: this may be NULL if we are being called for the
 *  /stats/actions/calculate action, since the results are not going
 * into a CHD.
 *
 * \param chd_func_name Name of CHD function to compute, e.g. "sum" or
 * "mean".  This matches the chd_func_name which was provided with
 * this callback at registration time and which was named in the
 * configuration of the CHD we are being called for.
 *
 * \param source_data The dataset from which the CHD is to be
 * computed.  This is the dataset of the sample or CHD named in the
 * configuration as the source for this CHD.
 *
 * \param source_nodes The list of source node name patterns
 * configured for this CHD.  If there is at least one entry, this means
 * to only operate on the nodes matching the patterns in this list.
 * If empty, operate on all nodes.  May also be NULL, if we are called
 * from the /stats/actions/calculate action.  It is up to you whether
 * or not to set or heed these configuration values, depending on your
 * specific application.  Note: if you want to honor it in the standard
 * manner, call st_chd_check_node() with each node, to be told whether it
 * matches any of the configured patterns.
 *
 * \param source_nodes_parts Array parallel to source_nodes: each 
 * entry is a tstr_array with the corresponding entry from source_nodes
 * broken down into name parts.
 *
 * \param chd_data The dataset from the CHD we are computing.
 * NOTE: modules should NOT add their newly-computed instance directly
 * to this dataset (and cannot without casting it to non-const anyway);
 * this is for read-only purposes, in case the CHD needs to use previous
 * values of itself in its calcalations.
 * NOTE: this may be NULL if we are being called for the
 *  /stats/actions/calculate action, since the results are not going
 * into a CHD.
 *
 * \param callback_data The callback_data which was provided for this
 * function in registration.
 *
 * \param first_inst_id The first instance ID in the range of instances
 * from the source data set that should be used to compute the CHD.
 *
 * \param last_inst_id The last instance ID in the range of instances
 * from the source data set that should be used to compute the CHD.
 *
 * \retval ret_new_instance The newly-computed instance of the CHD.
 * The framework passes in an empty binding array, and the module adds
 * one binding to it for each series of the CHD dataset for which it
 * wants to add a value.  Don't forget that bn_binding_array_append()
 * makes a copy of the binding it is given, so either (a) use
 * bn_binding_array_append_takeover() instead; or (b) free your copies
 * of the bindings when you are done.
 */
typedef int (*st_chd_callback)(const char *chd_id, const char *chd_func_name,
                               const st_dataset *source_data,
                               const tstr_array *source_nodes,
                               const tstr_array_array *source_nodes_parts, 
                               const st_dataset *chd_data,
                               void *callback_data,
                               int32 first_inst_id, int32 last_inst_id,
                               bn_binding_array *ret_new_instance);


/* ------------------------------------------------------------------------- */
/** Tell whether the specified node name matches any of the specified
 * node name patterns, broken down into parts.  This is mainly meant
 * to be used to honor the source_nodes_parts parameter to an
 * st_chd_callback.
 *
 * node_name_patterns being NULL, or being an empty array, are both
 * interpreted the same: no restrictions.
 */
int
st_chd_check_node(const char *node_name, 
                  const tstr_array_array *node_name_patterns,
                  tbool *ret_match);


int
st_chd_check_node_parts(const tstr_array *node_name_parts, 
                        const tstr_array_array *node_name_patterns,
                        tbool *ret_match);


/* ------------------------------------------------------------------------- */
/** Register a custom CHD calculation function.
 *
 * \param chd_func_name The name by which this CHD calculation
 * function should be known.
 *
 * \param callback The function to be called when it is time to
 * calculation this CHD.
 *
 * \param callback_data A pointer to be passed back to the calculation
 * callback.
 */
int st_register_chd(const char *chd_func_name, st_chd_callback callback,
                    void *callback_data);


/* ------------------------------------------------------------------------- */
/** Custom alarm handler function type.  Called by stats daemon when
 * an alarm of a specific type is triggered.  Generally used to
 * specify additional bindings that should be added to the event
 * message to be sent out for this alarm, but could be used to take
 * other actions too.
 *
 * \param alarm_id The name of the alarm that has triggered.
 *
 * \param node_name The name of the node on which the alarm has
 * triggered (i.e. which has crossed a threshold into the error zone).
 *
 * \param node_value The value of the node on which the alarm has
 * triggered.
 *
 * \param callback_data The callback_data which was provided for this
 * function in registration.
 *
 * \retval ret_bindings Bindings to be added to the event message to
 * be sent out for this alarm.  The framework passes in an empty
 * binding array, and the module adds bindings to it.  Don't forget
 * that bn_binding_array_append() makes a copy of the binding you are
 * appending, so if you own the binding memory, in order not to leak
 * it you should use bn_binding_array_append_takeover(), or just
 * free it yourself afterwards.
 */
typedef int (*st_alarm_callback)(const char *alarm_id, const char *node_name,
                                 const bn_attrib *node_value, 
                                 void *callback_data,
                                 bn_binding_array *ret_bindings);


/* ------------------------------------------------------------------------- */
/** Register a custom alarm handler function.
 *
 * \param alarm_name The name of the alarm this handler should be
 * called for.
 *
 * \param callback The function to be called when this alarm is
 * triggered.  Note that the function will be called whenever the
 * alarm moves from a non-error state to an error state, but not
 * vice-versa.  It's OK to make mgmtd requests from this handler.
 *
 * \param callback_data A pointer to be passed back to the handler
 * callback.
 */
int st_register_alarm_handler(const char *alarm_name,
                              st_alarm_callback callback, void *callback_data);


#include "st_main.h"


/* ------------------------------------------------------------------------- */
/** Register a custom report (statistics export) object.  Note that no
 * callback function is provided, as all information necessary to
 * perform the export is provided with the registration.
 *
 * \param report_id A short string to uniquely identify this report.
 * Note that this may be user-visible, as it may be used by the user
 * to identify which report is desired.  For this reason, the string
 * may contain only alphanumberic characters and hyphens.
 *
 * \param report_descr A longer string that serves as a more readable
 * description of the report.  It does not need to be unique and may
 * contain any characters.
 *
 * \param src_type The type of stats record the source data will 
 * come from: a sample or a CHD.
 *
 * \param src_id The ID of the sample or CHD record from which the
 * source data should be taken.
 *
 * \retval ret_report_reg A pointer to the report registration
 * structure created by this call.  This is an opaque structure; the
 * pointer is needed to be passed to subsequent calls to
 * st_register_report_nodes().  Note that this is NOT passing
 * ownership of the memory pointed to by the report registration
 * returned.
 *
 * Note that the names of the nodes to be included in the report are
 * specified separately by one or more calls to
 * st_register_report_nodes().
 */
int st_register_report(const char *report_id, const char *report_descr, 
                       st_source_type src_type, const char *src_id,
                       st_report_reg **ret_report_reg);


/* ------------------------------------------------------------------------- */
/** Register the nodes for one group in a custom report.
 * Each report may have any number of groups; call this function once
 * for each group.  Each group may have any number of nodes.  Pass as
 * many parameters as you need to include all of the nodes.
 *
 * The first three parameters (report_reg, group_descr, and num_nodes)
 * are fixed; after that, there is one set of three parameters (node
 * name, node descr, and node heading) for each node you are
 * registering.
 *
 * \param report_reg The opaque report registration pointer returned
 * from st_register_report.
 *
 * \param group_descr A string describing the data in this group.
 *
 * \param num_nodes The number of nodes in the group.  Must be at
 * least 1.  The number of parameters following this one must be
 * num_nodes * 3: one set of the following three parameters for
 * each node.
 * 
 * \param node_name The name of one node to be part of this group.
 * Note that the node name must be concrete: it may not have wildcards
 * in it.  This field is mandatory.
 *
 * \param node_descr A full description of the node, to be printed in
 * the comment above the group data.  This field is mandatory.
 *
 * \param node_heading A brief description of the node, to be printed
 * in a comma-delimited heading immediately above the group data.
 * If this field is NULL, the node description will also be used as
 * the node heading.
 */
int st_register_report_group(st_report_reg *report_reg,
                             const char *group_descr, uint32 num_nodes,
                             const char *node_name, const char *node_descr,
                             const char *node_heading, ...);


/* ------------------------------------------------------------------------- */
/** Unregister a previously registered report.  This is generally
 * intenteded to be called from an event handler (e.g. config database
 * change handler) in response to an event that makes a previously
 * registered report no longer applicable.
 */
int st_unregister_report(const char *report_id);


/* ------------------------------------------------------------------------- */
/** Function to be called when there is a change to the config database.
 */
typedef int (*st_config_change_callback_func)
     (const bn_binding_array *old_bindings,
      const bn_binding_array *new_bindings, void *data);

/* ------------------------------------------------------------------------- */
/** Register to be called when there is a change to the config
 * database.  One possible application for this is to register,
 * unregister, or change the content of reports based on the
 * configuration.
 *
 * NOTE: this API may only be called during statsd initialization.
 * Calling it after your initialization function has returned
 * will result in an error.
 *
 * NOTE: for performance reasons, modules should NOT use
 * st_config_change_notif_register(), and should only call the
 * newer st_config_change_notif_register_va().  If even one module
 * calls st_config_change_notif_register(), statsd will have to
 * refrain from installing any filters on dbchange notifications,
 * which can be a performance drain.  If all callers use
 * st_config_change_notif_register_va(), we install filters so we
 * only receive the union of all subtrees/patterns registered by modules
 * (and the /statsd subtree, which we ourselves need to watch).
 */
int st_config_change_notif_register
    (st_config_change_callback_func callback_func, void *data);

/* ------------------------------------------------------------------------- */
/** Register for notification for certain configuration changes.
 * This is strongly preferred over st_config_change_notif_register()
 * for reasons explained above.
 *
 * NOTE: this API may only be called during statsd initialization.
 * Calling it after your initialization function has returned
 * will result in an error.
 */
int st_config_change_notif_register_va
    (st_config_change_callback_func callback_func, void *data,
     uint32 num_patterns, const char *pattern1, ...);


/* ------------------------------------------------------------------------- */
typedef enum {
    spf_none =         0,

    /**
     * Indicates we are calling a st_pre_post_sample_callback function
     * before taking the associated sample.  Mutually exclusive with
     * ::spf_post_sample.
     */
    spf_pre_sample =   1 << 0,

    /**
     * Indicates we are calling a st_pre_post_sample_callback function
     * after taking the associated sample.  Mutually exclusive with
     * ::spf_pre_sample.
     */
    spf_post_sample =  1 << 1,
} st_pps_flags;


/* ------------------------------------------------------------------------- */
/** Pre-sample or post-sample callback.  Called by stats daemon
 * immediately before or after every time a particular sample is
 * taken.
 *
 * \param sample_id ID of the sample in question.
 *
 * \param sample_nodes In the case of a pre-sample callback, this
 * array contains the configured node name patterns which are to be
 * sampled.  In the case of a post-sample callback, this array
 * contains the actual concrete node names (not patterns) which were
 * successfully sampled.
 *
 * \param flags A bit field of ::st_pps_flags that conveys context
 * information.  Currently it just tells us whether we are pre- or 
 * post-sample.
 *
 * \param callback_data The callback_data which was provided for this
 * function in registration.
 */
typedef int (*st_pre_post_sample_callback)(const char *sample_id,
                                           const tstr_array *sample_nodes,
                                           uint32 flags, void *callback_data);


/* ------------------------------------------------------------------------- */
/** Register a pre-sample callback function.
 *
 * \param sample_id The name of the sample record before each sample of
 * which we should call this function.
 *
 * \param callback The function to be called immediately before taking
 * this sample.
 *
 * \param callback_data A pointer to be passed back to the pre-sample
 * callback.
 */
int st_register_pre_sample_handler(const char *sample_id,
                                   st_pre_post_sample_callback callback,
                                   void *callback_data);

/* ------------------------------------------------------------------------- */
/** Register a post-sample callback function.
 *
 * \param sample_id The name of the sample record after each sample of
 * which we should call this function.
 *
 * \param callback The function to be called immediately after taking
 * this sample.
 *
 * \param callback_data A pointer to be passed back to the post-sample
 * callback.
 */
int st_register_post_sample_handler(const char *sample_id,
                                    st_pre_post_sample_callback callback,
                                    void *callback_data);


/******************************************************************************
 ******************************************************************************
 *** Internal API
 ******************************************************************************
 ******************************************************************************
 ***/

#include "st_main.h"

/** \cond */

/* ------------------------------------------------------------------------- */
/* Initialize the stats module registration subsystem.
 */
int st_reg_init(void);

/* ------------------------------------------------------------------------- */
/* Deinitialize the stats module registration subsystem.
 */
int st_reg_deinit(void);

/* ------------------------------------------------------------------------- */
/* Given the name of a CHD function, make the appropriate notations in
 * the CHD record so the right calculation function can be called
 * later.  This will have one of three results:
 *
 *   - Set chd->sc_function to of the the built-in function types,
 *     and set chd->sc_function_callback and chd->sc_function_callback_data
 *     to use our built-in functions.
 *
 *   - Set chd->sc_function to scf_custom, and fill in 
 *     chd->sc_function_callback and chd->sc_function_callback_data
 *     with the right values from the registration.
 *
 *   - Generate an error, if the function name does not match any
 *     of the built-in or any of the registered CHD functions.
 */
int st_reg_lookup_chd_function(const char *func_name, 
                               st_chd_function *ret_func_id,
                               st_chd_callback *ret_func_ptr,
                               void **ret_func_data);


/* ------------------------------------------------------------------------- */
/* Call any handlers registered for the specified alarm that has just
 * fired on the specified node and value, and add any additional
 * bindings they return to the event request.
 */
int st_reg_call_alarm_handlers(const st_alarm *alarm_rec,
                               const st_alarm_node *alarm_node,
                               const bn_attrib *value, bn_request *req);


/* ------------------------------------------------------------------------- */
/* Structure to hold record of one group of nodes to go in a stats report.
 * Nodes in a group will be output together in different columns in the
 * same part of a the report.
 */
typedef struct st_report_group {
    char *srg_descr;

    /*
     * These are parallel arrays; they serve as an array of structures
     * of three elements each.
     */
    tstr_array *srg_node_names;
    tstr_array *srg_node_descrs;
    tstr_array *srg_node_headings;
} st_report_group;

TYPED_ARRAY_HEADER_NEW_NONE(st_report_group, st_report_group *);

/* ------------------------------------------------------------------------- */
/* Structure to hold record of one registered report type.
 */
struct st_report_reg {
    char *srr_id;
    char *srr_descr;
    st_source_type srr_src_type;
    char *srr_src_id;
    st_sample *srr_sample;                /* If srr_src_type is sst_sample */
    st_chd *srr_chd;                      /* If srr_src_type is sst_chd    */
    st_report_group_array *srr_groups;
};

TYPED_ARRAY_HEADER_NEW_NONE(st_report_reg, st_report_reg *);

extern st_report_reg_array *st_report_regs;

/* ------------------------------------------------------------------------- */
/* Call all registered pre-sample or post_sample handlers for the
 * specified sample.  The 'flags' parameter must have exactly one of
 * spf_pre_sample or spf_post_sample specified, which will determine
 * which handlers are called.  Other flags may be specified and be
 * passed on to the callback.
 */
int st_reg_call_pre_or_post_sample_handlers(st_sample *sample,
                                            const bn_binding_array *bindings,
                                            uint32 flags);

/** \endcond */

#ifdef __cplusplus
}
#endif

#endif /* __ST_REGISTRATION_H_ */
