/*
 *
 * event_notifs.h
 *
 *
 *
 */

#ifndef __EVENT_NOTIFS_H_
#define __EVENT_NOTIFS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/** \defgroup events Event notification: SNMP and email */

/**
 * \file src/include/event_notifs.h Definitions of events of which a
 * user can be notified via email or SNMP.
 * \ingroup events
 */

#include "common.h"

#define EVENT_NOTIFS_CUSTOM_SNMP_AUTH_TRAP "CUSTOM:SNMP_auth_trap"

/**
 * Class of event.  Events are classified as either informational
 * or failures.
 *
 * This classification is only important for email notifications;
 * SNMP makes no distinction between informational and failure
 * events.  For email events, it determines two things:
 *
 *   (a) Which recipients it is sent to, as each recipient has a
 *       "send me failures" flag and a "send me events" flag.
 *
 *   (b) The subject line of the email, which will use different
 *       language depending on the severity of the event.
 *
 * Note that each event may only be one or the other, so for that
 * purpose, these are not intended to be combinable flags.  However,
 * they are declared that way because some code in other contexts
 * sometimes wants to select both event classes (e.g. sending a test
 * email to recipients for both event classes).
 *
 * If an event has en_autosupport set, it will be sent to autosupport
 * recipients as well.  If you only want it to go to autosupport, set
 * the class to eec_none.  If you additionally want it to go to
 * some regular event recipients, set one of these flags as usual.
 */
typedef enum {
    eec_none =               0,

    /** Informational event */
    eec_info =               1 << 0,

    /** Failure event */
    eec_failure =            1 << 1
} en_event_class;

typedef struct en_event_notif {
    /**
     * If this notification is to be triggered by a mgmtd event, this
     * contains the absolute name of the mgmtd event to trigger this
     * notification.  This is the predominant case, and the ONLY case
     * supported for customer code adding en_event_notif records of
     * their own.
     *
     * There are rare cases (only in the infrastructure -- this is not
     * something a customer can do with the graft point) of
     * notifications which are not triggered by mgmtd events, but
     * which are implemented in other ways.  These cases are
     * distinguishable because this field does not begin with a '/'
     * (which indicates an absolute event name).
     */
    const char *en_event_name;

    /**
     * Name the user will use to refer to the event in the CLI and
     * possibly Web UI.  Should have no spaces, to prevent difficulty
     * in using as a CLI parameter.  By convention we use the hyphen
     * '-' character to separate words.  Note that this should be short,
     * since users will be entering it in CLI commands.  Use 
     * en_friendly_descr for a longer, more verbose description.
     */
    const char *en_friendly_name;

    /**
     * A longer description that can be displayed to explain what the
     * relatively terse "friendly name" means.
     */
    const char *en_friendly_descr;

    /**
     * Should we offer this event to be sent as an SNMP trap?
     * Enable only if there is an SNMP module that registers for the
     * event to convert it into a trap.
     */
    tbool en_allow_snmp_trap;

    /**
     * Should we offer this event to be sent as an email?
     * Enable only if there is code in md_email_events.c (directly, or
     * through a graft point) that knows how to convert this event
     * into an email).
     */
    tbool en_allow_email;

    /**
     * Event class (see above for description)
     */
    en_event_class en_class;

    /**
     * Should this event also be sent to autosupport?  This is only
     * important for email events: currently autosupport is done
     * through the email mechanism.
     *
     * Note that which events are sent to autosupport is configurable
     * via tha /email/notify/events/&lt;NAME&gt;/autosupport nodes.  This
     * field is used as the default setting, but the settings may
     * subsequently deviate.
     */
    tbool en_autosupport;

#ifdef PROD_FEATURE_I18N_SUPPORT
    /**
     * Gettext domain in which we should look up the en_friendly_descr
     * field.  This is required because the strings here may come from
     * multiple places, due to the graft point.
     *
     * We have arbitrarily chosen to put the strings defined in this
     * file into the mgmtd gettext domain, so they are all marked as
     * such.  Any records a customer adds will need to point to 
     * whatever other gettext domain they set up.
     */
    const char *en_gettext_domain;
#endif
} en_event_notif;

/* Must match GETTEXT_PACKAGE as defined in the mgmtd Makefile */
#define MGMTD_GETTEXT_DOMAIN "mgmtd"

static const en_event_notif en_event_notifs[] = {
    {
        "/pm/events/failure/crash", "process-crash",
        N_("A process in the system has crashed"), true, true,
        eec_failure, true,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/pm/events/failure/unexpected_exit", "process-exit",
        N_("A process in the system unexpectedly exited"), true, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/pm/events/failure/liveness", "liveness-failure",
        N_("A process in the system was detected as hung"), true, true,
        eec_info, true,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/cpu_util_indiv/rising/error", "cpu-util-high",
        N_("CPU utilization has risen too high"), true, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/cpu_util_indiv/rising/clear", "cpu-util-ok",
        N_("CPU utilization has fallen back to normal levels"), false, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/paging/rising/error", "paging-high",
        N_("Paging activity has risen too high"), true, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/paging/rising/clear", "paging-ok",
        N_("Paging activity has fallen back to normal levels"), false, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/fs_mnt_bytes/falling/error", "disk-space-low",
        N_("Filesystem free space has fallen too low"), true, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/fs_mnt_bytes/falling/clear", "disk-space-ok",
        N_("Filesystem free space is back in the normal range"), false, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/memory_pct_used/rising/error", "memusage-high",
        N_("Memory usage has risen too high"), true, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/memory_pct_used/rising/clear", "memusage-ok",
        N_("Memory usage has fallen back to acceptable levels"), false, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/intf_util/rising/error", "netusage-high",
        N_("Network utilization has risen too high"), true, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/intf_util/rising/clear", "netusage-ok",
        N_("Network utilization has fallen back to acceptable levels"),
        false, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/disk_io/rising/error", "disk-io-high",
        N_("Disk I/O per second has risen too high"), true, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/disk_io/rising/clear", "disk-io-ok",
        N_("Disk I/O per second has fallen back to acceptable levels"),
        false, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
#ifdef PROD_FEATURE_CMC_SERVER
    {
        "/stats/events/cmc_status/rising/error", "cmc-status-failure",
        N_("The CMC has detected an error in a managed appliance"), true, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/stats/events/cmc_status/rising/clear", "cmc-status-ok",
        N_("A CMC status error has been corrected"), false, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/cmc/events/rendezvous/new_client", "cmc-new-client",
        N_("A new potential CMC client has announced itself"), false, true, 
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/cmc/events/version_mismatch", "cmc-version-mismatch",
        N_("The CMC connected to an appliance with a different system "
           "software version"), true, true, 
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
#endif /* PROD_FEATURE_CMC_SERVER */

#ifdef PROD_FEATURE_CLUSTER
    {
        "/cluster/events/unexpected_join", "unexpected-cluster-join",
        N_("A node has unexpectedly joined the cluster"), true, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },

    {
        "/cluster/events/unexpected_leave", "unexpected-cluster-leave",
        N_("A node has unexpectedly left the cluster"), true, true,
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },

    {
        "/cluster/events/unexpected_size", "unexpected-cluster-size",
        N_("The number of nodes in the cluster is unexpected"), true,
        true, eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
#endif/* PROD_FEATURE_CLUSTER */
#ifdef PROD_FEATURE_SMARTD
    {
        "/smart/events/warning", "smart-warning",
        N_("Smartd warnings"), true, true,
        eec_failure, true,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
#endif
    {
        "/system/events/unexpected_shutdown", "unexpected-shutdown",
        N_("Unexpected system shutdown"), true, true,
        eec_failure, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/net/interface/events/link_up", "interface-up",
        N_("An interface's link state has changed to up"), true, true, 
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/net/interface/events/link_down", "interface-down",
        N_("An interface's link state has changed to down"), true, true, 
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/mgmtd/session/events/login", "user-login",
        N_("A user has logged into the system"), true, true, 
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/mgmtd/session/events/logout", "user-logout",
        N_("A user has logged out of the system"), true, true, 
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },
    {
        "/snmp/events/test_trap", "test-trap",
        N_("Send a test trap"), true, false, 
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },

    /*
     * This one is a special case, because it is not driven by a mgmtd
     * event.  We only have this record in order to get the event to
     * be offered to clients who want to enable or disable the event,
     * and to get the mgmtd node set.  But setting the node will not
     * lead to anyone registering to receive any event.  It is handled
     * elsewhere: md_snmp.c looks for this config, and the setting
     * affects what we write into snmpd.conf for 'authtrapenable'.
     *
     * This is distinguished from other records because the first field
     * (en_event_name) does not begin with '/' (bn_name_separator_char).
     * (It would be nicer to have this driven by a separate enum, but
     * that would break existing customer code which adds events with
     * the graft point below.  The overloading we're doing now is also
     * not contagious, because it's not something a customer may do.)
     */
    {
        EVENT_NOTIFS_CUSTOM_SNMP_AUTH_TRAP, "snmp-authtrap",
        N_("An SNMP v3 request has failed authentication"), true, false, 
        eec_info, false,
#ifdef PROD_FEATURE_I18N_SUPPORT
        MGMTD_GETTEXT_DOMAIN
#endif
    },

#ifdef INC_EVENT_NOTIFS_INC_GRAFT_POINT
#undef EVENT_NOTIFS_INC_GRAFT_POINT
#define EVENT_NOTIFS_INC_GRAFT_POINT 1
#include "event_notifs.inc.c"
#endif /* INC_EVENT_NOTIFS_INC_GRAFT_POINT */
};

#define en_num_event_notifs (sizeof(en_event_notifs) / sizeof(en_event_notif))

/*
 * XXX/EMT: we should also have a mapping of the CMC status tests to
 * friendly strings, with a graft point to add your own.  We have
 * only a single pair of event notifications for any CMC status
 * failure, but customers may be adding their own status checks.
 * In the mean time, the workaround is that we just show the ID, and
 * hope that this is descriptive enough.
 */

#ifdef __cplusplus
}
#endif

#endif /* __EVENT_NOTIFS_H_ */
