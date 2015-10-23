/*
 *
 * src/bin/mgmtd/mdm_events.h
 *
 *
 *
 */

#ifndef __MDM_EVENTS_H_
#define __MDM_EVENTS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "array.h"
#include "mdb_db.h"
#include "gcl.h"

/**
 * \file src/bin/mgmtd/mdm_events.h API for mgmtd modules to maintain
 * the list of events they want to be called for.
 * \ingroup mgmtd
 */

/**
 * Function to be called inside a mgmtd module whenever an event occurs
 * that the module has registered interest in.
 *
 * Note that the db pointer you will be passed is the main_db, which
 * may not always be what you expect.  For instance, if an event is
 * sent from the side effects or check function during a commit, your
 * callback will be called with the old_db, since the new_db has not
 * yet been installed as the main_db.
 *
 * \param commit Commit structure.
 * \param db A read-only reference to the config db.  Note that you can
 * do queries, or execute actions that don't modify the db, or send other
 * events; but you cannot do sets or actions that modify the db.
 * \param event_name Name of event that has occurred
 * \param bindings Bindings included in the event message
 * \param cb_reg_arg XXX/EMT what is this?
 * \param cb_arg The argument passed to md_events_handle_int_interest_add()
 * to be passed back to us.
 */
typedef int (*md_event_interest_cb_func)(md_commit *commit,
                                         const mdb_db *db,
                                         const char *event_name, 
                                         const bn_binding_array *bindings,
                                         void *cb_reg_arg,
                                         void *cb_arg);


typedef int (*md_event_interest_cb_async_start_func)(md_commit *commit,
                                             const mdb_db *db,
                                             const char *event_name, 
                                             const bn_binding_array *bindings,
                                             void *cb_reg_arg,
                                             void *cb_arg);


/* Structure filed in by internal modules that want to get events */
typedef struct md_event_int_state {
    char *mis_event_name;
    md_event_interest_cb_func mis_func;
    md_event_interest_cb_async_start_func mis_async_start_func;
    void *mis_arg;
} md_event_int_state;

int md_events_is_session_interested(md_commit *commit,
                                    const mdb_db *db,
                                    gcl_session *sess, 
                                    mdb_node_reg *event_reg,
                                    const char *event_name,
                                    const bn_binding_array *event_bindings,
                                    tbool *ret_interested);

int md_events_is_internal_interested(md_event_int_state *int_state,
                                     mdb_node_reg *event_reg,
                                     const char *event_name,
                                     const bn_binding_array *event_bindings,
                                     tbool *ret_interested);

int md_events_is_anyone_interested(md_commit *commit, 
                                   const char *event_name,
                                   tbool *ret_internal_int,
                                   tbool *ret_external_int);

/**
 * Register interest in an event from a mgmtd module.
 *
 * \param event_name Name of event we want to hear about
 * \param func Function to be called when that event occurs.
 * \param cb_arg Argument to be passed to func.
 */
int md_events_handle_int_interest_add(const char *event_name,
                                      md_event_interest_cb_func func,
                                      void *cb_arg);

int md_events_handle_int_interest_add_async(const char *event_name,
                                  md_event_interest_cb_async_start_func func,
                                  void *cb_arg);


int md_events_handle_int_interest_delete(const char *event_name,
                                         md_event_interest_cb_func func,
                                         void *cb_arg);

int md_events_handle_int_interest_delete_async(const char *event_name,
                                  md_event_interest_cb_async_start_func func,
                                  void *cb_arg);

int md_events_handle_ext_interest_session_close(gcl_session *sess);

extern array *md_event_int_interest;

/*
 * Sanity check that all internal interest event registrations are for
 * valid, registered events.  To be called by the infrastructure after
 * all registrations (initialization functions) are complete.
 */
int md_events_validate_regs(const mdb_reg_state *reg_state);


#ifdef __cplusplus
}
#endif

#endif /* __MDM_EVENTS_H_ */
