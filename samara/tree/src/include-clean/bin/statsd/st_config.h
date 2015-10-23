/*
 *
 * src/bin/statsd/st_config.h
 *
 *
 *
 */

#ifndef __ST_CONFIG_H_
#define __ST_CONFIG_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


/* ------------------------------------------------------------------------- */
/* Initialize the data structures used to hold the stats daemon's
 * configuration, as well as the sampled data.
 */
int st_config_init(void);


/* ------------------------------------------------------------------------- */
/* Query the stats daemon's configuration from the management daemon.
 */
int st_config_query(void);


/* ------------------------------------------------------------------------- */
/* Handle both the initial query and any changes to stats alarms.
 * This follows the new model of combining initialization and change
 * handling, which we want to migrate to.  So far, alarms are the only
 * objects that are done this way.
 */
int st_config_handle_alarms(const bn_binding_array *bindings);


/* ------------------------------------------------------------------------- */
/* Handle one changed node in our configuration.  This call does not
 * handle changes to alarms, as they are handled in
 * st_config_handle_alarms().  It handles any other changes that
 * statsd handles at all, though some are currently ignored (bug 10179).
 */
int st_config_handle_change(const bn_binding_array *arr, uint32 idx,
                            bn_binding *binding, void *data);


/* ------------------------------------------------------------------------ */
/* Reinspect all CHD records to see if any need to be rescheduled
 * (according to the sc_resched flag).  This would happen if any of
 * their timing parameters changed recently.
 */
int st_config_recheck_chds(void);


/* ------------------------------------------------------------------------- */
/* Deallocate all of the memory used to hold the configuration,
 * as well as the sampled data.
 */
int st_config_free(void);

#ifdef __cplusplus
}
#endif

#endif /* __ST_CONFIG_H_ */
