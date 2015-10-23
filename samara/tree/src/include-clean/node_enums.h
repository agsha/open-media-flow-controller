/*
 *
 * node_enums.h
 *
 *
 *
 */

#ifndef __NODE_ENUMS_H_
#define __NODE_ENUMS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file node_enums.h Enumerations for possible values for binding nodes
 * which use a 'string' data type to represent an enum.
 * \ingroup lc
 *
 * This file defines enums which represent the set of possible values
 * for certain string binding nodes.  These nodes can be configuration or
 * monitoring nodes; or they can be parameters in actions or events.
 * Newly-added entries which are likely to be used only in the context
 * of a single node should be named as follows:
 *   - Begin with "nen_" for "node enum".
 *   - Then have the absolute name of the binding in the mgmtd hierarchy,
 *     with slashes mapped to underscore '_' characters, and with wildcards
 *     (asterisks) mapped to the word "star".
 *   - In the case of parameters to actions and events, after the name of
 *     the action or event itself, the parameter name should follow in the
 *     same format.
 *
 * However, some relaxation of these conventions is permitted if the
 * enum was already defined under a different name, and/or might be
 * used for more than one mgmtd node.  In that case, they must at
 * least follow these restrictions:
 *   - Begin with the standard prefix for the component in the system,
 *     e.g. "md_" for mgmtd, "pm_" for Process Manager, etc.
 *   - If they are primarily owned by a plug-in module, have the standard
 *     prefix for that module.  e.g. "md_crypto_" for enums for use in
 *     md_crypto.c.
 *
 * In either case, these points should also be followed:
 *   - Per standard coding guidelines, members of the enum should begin
 *     with an acronym made from the first letters of each of the words
 *     forming the name of the enum.
 *   - Each enum should have a comment above it containing the original
 *     name of the node (and parameter name, in the case of an action or
 *     event).
 *
 * Each of these enums has at least one lc_enum_string_map that goes
 * with it, which maps these enums to the actual strings used in the
 * mgmtd node names.  The main map is the name of the enum with "_map"
 * appended.  These maps are extern-declared here, and defined in
 * node_enums.c in libcommon.
 */

#include "common.h"


/* ------------------------------------------------------------------------- */
/* Possible values for:
 *
 * /auth/passwd/events/force_logout // event
 *     reason // string
 */
typedef enum {
    napeflr_none,
    napeflr_deleted,
    napeflr_disabled,
    napeflr_locked_out,
    napeflr_uid_changed,
    napeflr_lost_unix_group,
} nen_auth_passwd_events_force_logout_reason;

extern const lc_enum_string_map
    nen_auth_passwd_events_force_logout_reason_map[];


/* ------------------------------------------------------------------------- */
/* Possible values for:
 *
 * /pm/events/shutdown // event
 *     shutdown_type // string
 */
typedef enum {
    /* No shutdown method has been specified */
    pso_none = 0,

    /* Reinitialize Process Manager */
    pso_reinit,

    /* Reboot the system */
    pso_reboot,

    /* Reboot the system without waiting */
    pso_reboot_force,

    /* Halt the system */
    pso_halt,

    /* Just exit Process Manager */
    pso_exit,
} pm_shutdown_options;

extern const lc_enum_string_map pm_shutdown_options_map[];


/* ------------------------------------------------------------------------- */
/* Possible values for:
 *
 * /snmp/state/request/owner/ * /reqid/ * /status // ro-mon // string
 */
typedef enum {
    nssrs_none = 0,
    nssrs_running,
    nssrs_complete,
    nssrs_cancelled,
} nen_snmp_state_request_status;

extern const lc_enum_string_map nen_snmp_state_request_status_map[];


#ifdef __cplusplus
}
#endif

#endif /* __NODE_ENUMS_H_ */
