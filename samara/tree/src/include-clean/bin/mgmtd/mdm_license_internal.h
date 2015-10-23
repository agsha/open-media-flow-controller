/*
 *
 * src/bin/mgmtd/mdm_license_internal.h
 *
 *
 *
 */

#ifndef __MDM_LICENSE_INTERNAL_H_
#define __MDM_LICENSE_INTERNAL_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "license.h"
#include "md_mod_commit.h"
#include "md_mod_reg.h"

/* ------------------------------------------------------------------------- */
/* This header file has internal declarations for licensing.  It should
 * not be used by any code beyond licensing implementation.
 * ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* Runtime state for a registered licenseable feature.
 */
typedef struct mdl_feature_state {
    /*
     * Name of feature
     */
    tstring *mfs_feature_name;

    /*
     * Is there at least one valid, active license installed for this
     * feature?
     */
    tbool mfs_is_active;

    /*
     * Roll-up summary of the options specified on all of the valid
     * and active licenses for this feature.  This is calculated for
     * informational options only, according to the rules described by
     * the lot_summary, lot_empty_behavior, lot_default_value,
     * and lot_relevant_features fields in the lk_option_tag structure
     * in which the option was defined.
     *
     * It has one entry per option, so if a license has multiple
     * instances of the same option tag id, they are treated just the
     * same as if they had occurred in different licenses.  (This may
     * not be very useful, but we didn't expect cases like that to 
     * want to use this summarizing mechanism anyway.)
     *
     * The array index is the option tag id, so this array may be
     * sparse.
     */
    lk_option_array *mfs_options;
    
} mdl_feature_state;

TYPED_ARRAY_HEADER_NEW_NONE(mdl_feature_state, struct mdl_feature_state *);


/* ------------------------------------------------------------------------- */
/* Record of the state of a licenseable feature.  This contains
 * essentially the same information as mdl_feature_state, except that
 * it keeps them in bn_binding form (as the monitoring nodes we
 * serve).
 */
typedef struct mdl_feature_bindings {
    tstring *mlfb_feature_name;
    bn_binding_array *mlfb_feature_bindings;
} mdl_feature_bindings;

TYPED_ARRAY_HEADER_NEW_NONE(mdl_feature_bindings,
                            struct mdl_feature_bindings *);


/* ------------------------------------------------------------------------- */
/* Record of a registered licenseable feature.
 */
typedef struct mdl_feature {

    char *mlf_feature_name;
    char *mlf_feature_descr;
    char *mlf_restrict_tree_pattern;

    char *mlf_track_active_node_name;
    tbool mlf_track_active_node_value;

    char *mlf_activate_node_name;
    tbool mlf_activate_node_value;

    char *mlf_deactivate_node_name;
    tbool mlf_deactivate_node_value;

} mdl_feature;

TYPED_ARRAY_HEADER_NEW_NONE(mdl_feature, struct mdl_feature *);

TYPED_ARRAY_HEADER_NEW_NONE(mdl_license, struct mdl_license *);


/* ------------------------------------------------------------------------- */
/* Cache of license key state.
 *
 * XXX/EMT: currently, within mlc_licenses, the index matches the
 * value of the wildcard under /license/key.  This makes life a bit
 * easier, but would deal badly if someone chose very sparse indices.
 * Not currently a problem because our UIs always keep the indices
 * consecutive.
 */
typedef struct mdl_license_cache {
    /*
     * All license keys.
     */
    mdl_license_array *mlc_licenses;

    /*
     * State of all registered features.
     *
     * NOTE: this array is "parallel" to mdl_reg_features, in that an
     * element at a given position in one corresponds to the element
     * in the same position in the other.  However, while the reg
     * array is always complete, this one may be missing some entries,
     * depending on what we have needed to calculate so far.
     */
    mdl_feature_state_array *mlc_features;

    /*
     * This is the 'key' to looking up a license cache in our array of 
     * caches.
     */
    uint64 mlc_change_id;

    /*
     * This will tell us when we can throw out an old cache: if it is
     * for a db revision ID different from the current one (and
     * different from the current one minus one, to allow for accessing
     * cached information computed during the side effects phase of the
     * commit that just went through), then we can throw it out.
     */
    int32 mlc_db_revision_id;
} mdl_license_cache;

TYPED_ARRAY_HEADER_NEW_NONE(mdl_license_cache, struct mdl_license_cache *);


/* ------------------------------------------------------------------------- */
/* APIs for managing mdl_feature_state, mdl_feature, and mdl_license
 * structures, and arrays thereof.
 */

int mdl_feature_state_new(const char *feature_name,
                          mdl_feature_state **ret_feature_state);
int mdl_feature_state_free(mdl_feature_state **inout_feature_state);
int mdl_feature_state_array_new(mdl_feature_state_array **ret_arr);

int mdl_feature_bindings_new(const char *feature_name,
                             mdl_feature_bindings **ret_feature);
int mdl_feature_bindings_free(mdl_feature_bindings **inout_feature);
int mdl_feature_bindings_array_new(mdl_feature_bindings_array **ret_arr);

int mdl_feature_new(mdl_feature **ret_feature);
int mdl_feature_free(mdl_feature **inout_feature);
int mdl_feature_array_new(mdl_feature_array **ret_arr);

int mdl_license_new(const char *license_str, md_commit *commit,
                    const mdb_db *db, tbool check_valid, tbool check_active, 
                    mdl_license **ret_license);
int mdl_license_free(mdl_license **ret_license);
int mdl_license_array_new(mdl_license_array **ret_arr);

int mdl_license_cache_new(uint64 change_id, int32 db_revision_id,
                          mdl_license_cache **ret_cache);
int mdl_license_cache_free(mdl_license_cache **inout_cache);
int mdl_license_cache_array_new(mdl_license_cache_array **ret_arr);


/* ------------------------------------------------------------------------- */
/* Global variables
 */

extern mdl_feature_array *mdl_reg_features;
extern mdl_license_cache_array *mdl_license_caches;


/* ------------------------------------------------------------------------- */
/* Miscellaneous licensing-related APIs.
 */

int mdl_license_state_init(md_module *module);

/*
 * Return an up-to-date cache of license state.  If we already have a
 * cache for the current change ID, and we are not being forced to
 * refresh, take the cache we already have; otherwise, start a new
 * one.  Then iterate through it to make sure it is fully populated.
 * Then return it.
 */
int md_license_get_state(md_commit *commit, const mdb_db *db,
                         const char *lic_id, const char *feature_name, 
                         tbool force_refresh,
                         const mdl_license_cache **ret_cache);

int mdl_render_option_value(lk_option_tag_id option_tag_id,
                            const bn_attrib *option_value,
                            tstring **ret_val_str);

/*
 * Given an mdl_license data structure with the mll_license field already
 * containing a key, interpret the license and fill out the rest of the
 * fields in the structure.
 */
int mdl_license_populate(mdl_license *license, md_commit *commit, 
                         const mdb_db *db, tbool check_valid,
                         tbool check_active);

/*
 * Figure out if any licensed feature state has changed since the last
 * time this function was called, and send an event message if so.
 * The event message sent is /license/events/feature_state_change.
 */
int mdm_license_events_check_feature_changes(md_commit *commit, 
                                             const mdb_db *db);

/*
 * Query license key state, and maybe send key_state_change events
 * announcing any differences since the last query.
 *
 * This function can be called in two modes:
 *
 *   1. After a commit, pass 'false' for calc_changes.  We still want to
 *      query state and save it, so we'll have a standard for comparison
 *      when we do want to calculate changes later.  But we don't want to
 *      calculate changes now, because keys may have moved around, etc.,
 *      and we are not required to send this event during config changes.
 *
 *   2. After a scheduled recheck, pass 'true' for calc_changes.  Since it
 *      was just a recheck, we don't expect that any licenses will have
 *      moved around since our last query, so we can assume everything 
 *      matches and just send any changes we see.
 */
int mdm_license_events_check_key_changes(md_commit *commit, const mdb_db *db,
                                         tbool calc_changes);

int mdm_license_events_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __MDM_LICENSE_INTERNAL_H_ */
