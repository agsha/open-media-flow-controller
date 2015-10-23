/*
 *
 * cli_notif_utils.h
 *
 *
 *
 */

#ifndef __CLI_NOTIF_UTILS_H_
#define __CLI_NOTIF_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "cli_module.h"
#include "tstr_array.h"
#include "bnode.h"

/**
 * \file src/include/cli_notif_utils.h Helper functions for dealing
 * with event notification configuration.  Currently used by the SNMP
 * and email modules, though it could be extended for other purposes later.
 * \ingroup cli
 */

/**
 * This specifies a type of notification.  In all present uses these
 * are mutually exclusive so there is no need for them to be set up as
 * a bit field, but it doesn't hurt either.
 */
typedef enum {
    cnt_none =                0,
    cnt_snmp =           1 << 0,
    cnt_email =          1 << 1,
    cnt_email_autosupp = 1 << 2,
} cli_notify_type;

/* ------------------------------------------------------------------------- */
int cli_notifs_do_help(cli_notify_type cnt, tbool negative,
                       const char *curr_word, void *context);

/* ------------------------------------------------------------------------- */
int cli_notifs_do_comp(cli_notify_type cnt, tbool negative,
                       const char *curr_word, tstr_array *ret_completions);

/* ------------------------------------------------------------------------- */
int cli_notifs_do_exec(cli_notify_type cnt, tbool negative,
                       const char *event_friendly_name);

/* ------------------------------------------------------------------------- */
/** When using this callback, set cc_revmap_names to a pattern that
 * specifies all of the ".../enabled" bindings for your events.  We
 * will extract the event name out of the appropriate part of the
 * binding.
 *
 * The 'data' parameter should be a cli_notif_type, so we know which
 * nodes and commands to reverse-map.
 */
int cli_notifs_do_revmap(void *data, const cli_command *cmd,
                         const bn_binding_array *bindings,
                         const char *name, const tstr_array *name_parts,
                         const char *value, const bn_binding *binding,
                         cli_revmap_reply *ret_reply);

/* ------------------------------------------------------------------------- */
/** en_event_classes is a bit field of ::en_event_class, telling
 * which types of events we want listed.  (NOTE: we currently expect
 * only one flag to be set, and only in the case of email.)
 *
 * NOTE: 'invert' currently not honored.
 */
int cli_notifs_do_show(cli_notify_type cnt, const bn_binding_array *bindings,
                       uint32 en_event_classes, tbool invert);

/* ===========================================================================
 * The APIs below are for infrastructure use only.
 * ===========================================================================
 */

/* ------------------------------------------------------------------------- */
int cli_notifs_init(void);
int cli_notifs_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __CLI_NOTIF_UTILS_H_ */
