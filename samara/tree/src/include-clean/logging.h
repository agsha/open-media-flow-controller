/*
 *
 */

#ifndef __LOGGING_H_
#define __LOGGING_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file logging.h Logging interface routines.
 * \ingroup lc
 *
 * This API is to be used in lieu of calling openlog() and syslog()
 * directly.
 */

#include <syslog.h>
#include "common.h"

/*
 * It would make more sense to #include customer.h from common.h, to make
 * it more explicit that everyone should get it.  But that is effectively
 * the case, since common.h includes bail.h, which includes logging.h.
 * (Not moving it now lest there be any dependencies that would be broken.)
 */
#include "customer.h"

typedef struct lc_log_state lc_log_state;

/**
 * Logging options.
 *
 * These can be specified in three places:
 *
 *   1. The user can configure default for most of them using CLI 
 *      commands under the "logging" subtree of commands.
 *      
 *   2. Each binary can specify them as flags to lc_log_init() during
 *      initialization.
 *
 *   3. Callers to lc_log_internal() can specify them on a per-message 
 *      basis.  However, lc_log_internal() is generally not called directly
 *      by our clients -- it is semi-private to this library (despite being
 *      in this header file).  Callers should use lc_log_basic() and 
 *      lc_log_debug(), which do not accept flags.
 *
 * Mainly these are set per #1, and so your code can ignore them.
 * The options specified to lc_log_init() simply override the
 * user-specified values, which is usually not needed.
 *
 * NOTE: these must be kept in sync with 
 * src/java/packages/com/tallmaple/common/LcLogOptions.java.
 */
typedef enum {
    LCO_none =                  0x0000,

    /** Ignore the default flags */
    LCO_IGNORE_DEFAULTS =       0x0001,

    /** Do not log the pid of the logging process (we do by default) */
    LCO_NO_PID =                0x0002,

    /** If not logging to syslog, we normally add fields to make the
     * output look like syslog; but if this flag is specified, we don't.
     */
    LCO_NO_SYSLOG_FORMAT =      0x0004,

    /** Include an extra field with a number of seconds (either since the
     * epoch, or since logging began, depending on whether you specify
     * LCO_TIME_RELATIVE).  With this option (LCO_TIME_SEC), the precision
     * is only seconds.
     */
    LCO_TIME_SEC =              0x0008,

    /** Like LCO_TIME_SEC except precision is to 1 decimal point
     * (deciseconds) 
     */
    LCO_TIME_DS =               0x0010,

    /** Like LCO_TIME_SEC except precision is to 2 decimal points
     * (centiseconds) 
     */
    LCO_TIME_CS =               0x0020,

    /** Like LCO_TIME_SEC except precision is to 3 decimal points
     * (milliseconds) 
     */
    LCO_TIME_MS =               0x0040,

    /** Like LCO_TIME_SEC except precision is to 6 decimal points
     * (microseconds) 
     */
    LCO_TIME_US =               0x0080,

    /**
     * If any of the LCO_TIME_... fields are specified, this specifies
     * that we should log the time relative to when lc_log_init()
     * was called.
     */
    LCO_TIME_RELATIVE =         0x0100,

    /** If we are adding an extra seconds field as per any other 
     * LCO_TIME_... option, this tells us to only show the six
     * least significant digits to the left of the decimal point.
     * Note that of course the messages will still have their regular
     * syslog timestamp, so the information being lost was redundant.
     */
    LCO_TIME_SEC_TRUNC_6DIG =   0x0200,

    /**
     * Like LCO_TIME_SEC_TRUNC_6DIG, except only leave one digit to
     * the left of the decimal.
     */
    LCO_TIME_SEC_TRUNC_1DIG =   0x0400,

    /**
     * In each log message, include information about where in the
     * code the logging statement occurred: the source filename, the
     * name of the function, and the line number within the file.
     * This option is selected automatically by lc_log_debug(),
     * and not by lc_log_basic().
     */
    LCO_SOURCE_INFO =           0x0800,

    /**
     * Do not log the thread ID of the calling thread.  This is 
     * implied for a singlethreaded program; it will only have an
     * effect in a multithreaded program, where logging the thread
     * ID is the default.
     */
    LCO_NO_THREAD_ID =          0x1000,

    /** XXX/EMT what is this field?  "Do not use the system defaults" */
    LCO_NO_SYS_OPTIONS =        0x2000,

    /** Internal use only: skip locking, do not use! */
    LCO_SKIP_LOCK =             0x4000,

    /* _MONOTONIC_ID =      */
    /* _NO_COMPONENT_NAME = */
} lc_log_options;

/**
 * Specify one or more targets to which to log.
 *
 * NOTE: these must be kept in sync with 
 * src/java/packages/com/tallmaple/common/LcLogTargets.java.
 */
typedef enum {
    LCT_BUILTIN_MASK = 0xff,

    /** Standard syslog logging */
    LCT_SYSLOG       = 0x01,

    /** Log to stdout */
    LCT_STDOUT       = 0x02,

    /** Log to stderr */
    LCT_STDERR       = 0x04,

    /** NOT YET IMPLEMENTED */
    LCT_CONSOLE      = 0x08,

    /** NOT YET IMPLEMENTED */
    LCT_FILE         = 0x101,
    LCT_FD           = 0x102,

    /*  syslog style things not supp: user, pipe to program, remote syslog */

} lc_log_targets;


/* These names and values are in syslog.h, and here only for reference */
#if 0
enum {
    LOG_first = 0,
    LOG_EMERG        = 0, /* system is unusable */
    LOG_ALERT        = 1, /* action must be taken immediately */
    LOG_CRIT         = 2, /* critical conditions */
    LOG_ERR          = 3, /* error conditions */
    LOG_WARNING      = 4, /* warning conditions */
    LOG_NOTICE       = 5, /* normal but significant condition */
    LOG_INFO         = 6, /* informational */
    LOG_DEBUG        = 7, /* debug-level messages */
    LOG_last
};
#endif

/*
 * Not defined as an enum, or present in the enum maps below, since
 * this is a signed integer.
 */
#define LOG_NONE -1

extern const lc_enum_string_map lc_log_level_map[];

extern const lc_enum_string_map lc_log_level_lower_map[];


/* ------------------------------------------------------------------------ */
/** Convert a logging level string into an integer.  Note that the
 * real levels [LOG_EMERG..LOG_DEBUG] are [0..7], but we also have to 
 * have some representation for "none", for which we use LOG_NONE (-1).
 *
 * \param sev_str A logging severity level, in lowercase (i.e. one of the
 * entries in lc_log_level_lower_map), or "none".
 *
 * \retval ret_sev_int The integer representation of that severity level
 * (as syslog wants), or LOG_NONE for "none".
 *
 * \return 0 for success; lc_err_bad_type if sev_str does not match any
 * of the recognized values.
 */
int lc_log_severity_string_to_int(const char *sev_str, int *ret_sev_int);


/* ---------------------------------------------------------------------------
 * Map from syslog facilities to user-friendly facility names.
 * These are the keywords the user can use to specify a facility
 * ("class") in the CLI, so they must be short and contain no spaces.
 */
static const lc_enum_string_map lc_log_facility_name_map[] = {
    { LOG_LOCAL7, "mgmt-front" },
    { LOG_LOCAL6, "mgmt-back" },
#ifndef lc_log_mgmtd_share_facility
    { LOG_LOCAL5, "mgmt-core" },
#endif

/* ========================================================================= */
/* Customer-specific graft point 1: additional logging facility names
 * =========================================================================
 */
#ifdef INC_LOGGING_HEADER_INC_GRAFT_POINT
#undef LOGGING_HEADER_INC_GRAFT_POINT
#define LOGGING_HEADER_INC_GRAFT_POINT 1
#include "../include/logging.inc.h"
#endif /* INC_LOGGING_HEADER_INC_GRAFT_POINT */

    { 0, NULL }
};

/* ------------------------------------------------------------------------ */
/** Map from syslog facilities to user-friendly facility descriptions.
 * This is longer text used to describe the facility ("class") in the
 * CLI help or Web UI, so it can be longer.
 */
static const lc_enum_string_map lc_log_facility_descr_map[] = {
    { LOG_LOCAL7, "System management front-end components" },
    { LOG_LOCAL6, "System management back-end components" },
#ifndef lc_log_mgmtd_share_facility
    { LOG_LOCAL5, "System management core" },
#endif

/* ========================================================================= */
/* Customer-specific graft point 2: additional logging facility descriptions
 * =========================================================================
 */
#ifdef INC_LOGGING_HEADER_INC_GRAFT_POINT
#undef LOGGING_HEADER_INC_GRAFT_POINT
#define LOGGING_HEADER_INC_GRAFT_POINT 2
#include "../include/logging.inc.h"
#endif /* INC_LOGGING_HEADER_INC_GRAFT_POINT */

    { 0, NULL }
};

/* ---------------------------------------------------------------------------
 * Map from syslog facility numbers to syslog facility names to go
 * into syslog.conf.  Doesn't need a graft point, since the set of
 * these is predetermined.
 */
extern const lc_enum_string_map lc_log_facility_keyword_map[];

/**
 * Message tags are used to allow filtering of messages by type, component,
 * and abitrary subcomponents.  Message tags are flags, stored in a uint32.
 * The tags common in three parts:
 *    - common flags:  One or more of these may apply to any message.  They 
 *                     describe the type and intended audience of the message
 *    - component id:  This is a number identifying the major component
 *    - per component flags:  One or more tags which identify a sub-component
 *                            or some other more fine-grained bucket
 */
typedef enum {
    LMT_COMMON_TAGS_MASK =     0x000000000000ffffull,
    LMT_COMPONENT_ID_MASK =    0x00000000ffff0000ull,
    LMT_COMPONENT_TAGS_MASK =  0xffffffff00000000ull,
} lc_msg_tag_masks;

/* For defining component tags.  Takes on x=0 to x=255 */
#define LC_COMPONENT_TAG(x) ( ((1 << (x)) << 32ULL) & LMT_COMPONENT_TAGS_MASK )

typedef enum {    
    LTC_INTERNAL = 0x01,  /* Message is for engineering use, not cutomers */
    LTC_ERROR =    0x02,
    LTC_EVENT =    0x04,
    LTC_INFO =     0x08,
    LTC_PACKET =   0x10
} lc_common_tags;

/* For defining component id flags */
#define LC_COMPONENT_ID(x) ( ( (x) << 16ULL) & LMT_COMPONENT_ID_MASK )
#define LC_GET_COMPONENT_ID(x) ( ((x) & LMT_COMPONENT_ID_MASK) >> 16ULL )

/*
 * NOTE: lc_component_id_map (in logging.c) must be kept in sync.
 */
typedef enum {
    LCI_none = 0,
    LCI_CLI = 1,
#ifdef PROD_FEATURE_WIZARD
    LCI_WIZARD,
#endif /* PROD_FEATURE_WIZARD */
    LCI_WEB,       /* request handler */
    LCI_WSMD,      /* web session manager daemon */
    LCI_CGI,       /* cgi launcher */
    LCI_SNMPD,
    LCI_SNMPKEYS,
    LCI_PM,
    LCI_MGMTD,
    LCI_MDREQ,
    LCI_MDDBREQ,
    LCI_GENLICENSE,
    LCI_DUMPLICENSE,
    LCI_STATSD,
    LCI_KERN,
    LCI_LIBS,
    LCI_TESTS,
    LCI_NVSTOOL,
#ifdef PROD_FEATURE_FRONT_PANEL
    LCI_FPD,
#endif /* PROD_FEATURE_FRONT_PANEL */
    LCI_UNK,
#ifdef PROD_FEATURE_SCHED
    LCI_SCHED,
#endif /* PROD_FEATURE_SCHED */
    LCI_RGP,
#ifdef PROD_FEATURE_CMC_CLIENT
    LCI_RENDV_CLIENT,
#endif /* PROD_FEATURE_CMC_CLIENT */
#ifdef PROD_FEATURE_CMC_SERVER
    LCI_RBMD,
#endif /* PROD_FEATURE_CMC_SERVER */
#ifdef PROD_FEATURE_CLUSTER
    LCI_CLUSTERD,
#endif /* PROD_FEATURE_CLUSTER */

#ifdef PROD_FEATURE_JAVA
    /*
     * XXX/EMT: ultimately we'll want each Java component to have its
     * own id.
     */
    LCI_JAVA,
#endif /* PROD_FEATURE_JAVA */

    LCI_PROGRESS,

#ifdef PROD_FEATURE_XML_GW
    LCI_XG,
#endif /* PROD_FEATURE_XML_GW */

#ifdef PROD_FEATURE_VIRT
    LCI_TVIRTD,
#endif /* PROD_FEATURE_VIRT */

#ifdef PROD_FEATURE_AAA
/* XXX/SML:  For now, only TACACS uses acctd */
#ifdef PROD_FEATURE_TACACS
    LCI_ACCTD,
#endif /* PROD_FEATURE_TACACS */
#endif /* PROD_FEATURE_AAA */

    LCI_PAM_TALLYBYNAME,

    /*
     * There is no LCI_last here because it would not be accurate
     * due to the potential presence of customer-defined IDs from
     * customer.h.
     */
} lc_component_id;

extern const lc_enum_string_map lc_component_id_map[];


/* XXX how to get syslog current log level issue */

/** @name Main logging API 
 *
 * This section describes the most commonly-used logging APIs.
 */

/*@{*/

/**
 * Initialize logging subsystem.  Ultimately calls openlog().
 *
 * \param process_name Name of calling process, to be reflected in 
 * every line logged.
 * \param msg_prefix A string to prefix every log message.  Commonly
 * used by user interfaces like the CLI or Web UI to tell the username
 * they are acting on behalf of.
 * \param default_log_options Bit field of ::lc_log_options ORed together.
 * \param default_msg_tags (CURRENTLY UNUSED)  Bit field of
 * ::lc_msg_tag_masks ORed together.
 * \param default_max_log_level A syslog log severity level to use to
 * filter messages logged.  No messages at a lower severity level than
 * this will be logged.  Generally one should pass LOG_DEBUG (the lowest
 * severity level) here, to do no additional filtering than the user
 * has configured.
 * \param syslog_facility Syslog facility to use for this component.
 * This provides coarse-grained categorization of log messages.
 * Currently the user can specify different minimum log severity levels
 * based on facility.  Generally the only facilities used by components
 * that would call this API are LOG_LOCAL0 through LOG_LOCAL7.  
 * But review ::lc_log_facility_descr_map for current usage; some of these
 * (the two or three higher-numbered ones) are reserved for use by 
 * Samara components.
 * \param builtin_targets A bit field of ::lc_log_targets ORed together.
 */
int lc_log_init(const char *process_name,
                const char *msg_prefix, 
                uint32 default_log_options,
                uint64 default_msg_tags,
                int8   default_max_log_level,
                int    syslog_facility,
                uint32 builtin_targets);/* Only targets with no args allowed */

/**
 * The main logging API for user-targetted log messages.
 * \param level The log severity level to use for this message:
 * LOG_NOTICE, LOG_WARNING, etc.
 * \param format A printf-style format string, followed by a variable
 * argument list, to form the string to log.
 */
#define lc_log_basic(level, format, ...)                                   \
    do {                                                                   \
        lc_log_internal(__FUNCTION__, __FILE__, __LINE__,                  \
                        0, 0, level, format, ## __VA_ARGS__);              \
    } while(0)

/**
 * The main logging API for developer-targetted log messages.
 * Acts the same as lc_log_basic() except that the source filename,
 * function name, and line number where the log message originated
 * is included in the log message.
 * \param level The log severity level to use for this message:
 * LOG_DEBUG, LOG_INFO, etc.
 * \param format A printf-style format string, followed by a variable
 * argument list, to form the string to log.
 */
#define lc_log_debug(level, format, ...)                                   \
    do {                                                                   \
        lc_log_internal(__FUNCTION__, __FILE__, __LINE__,                  \
                LCO_SOURCE_INFO, 0, level, format, ## __VA_ARGS__); \
    } while(0)

/*@}*/

/* In sync with MAXLINE from syslogd.c */
#define LC_LOG_MAX_LINE 1023
#define LC_LOG_MAX_LINE_STR "1023"

/** \cond */
/* This is the INTERNAL function that does the logging, don't call directly */
int lc_log_internal(const char *function, const char *file, uint32 line,
                    uint32 log_opts, uint32 msg_tags, int level,
                    const char *format, ...)
     __attribute__ ((format (printf, 7, 8)));
/** \endcond */


int lc_log_target_remove(lc_log_state *log_state, 
                      uint32 log_target, const char *target_filename, 
                      int target_fd);

int lc_log_get_max_log_level(int *ret_max_log_level);

int lc_log_close(void);


/* These need to be macros to get the c function, file, and line */

#define lc_log(opts, tags, level, format, ...)                             \
    do {                                                                   \
        lc_log_internal(__FUNCTION__, __FILE__, __LINE__,                  \
                        opts, tags, level, format, ## __VA_ARGS__);        \
    } while(0)

#define lc_log_tag(tags, level, format, ...)                               \
    do {                                                                   \
        lc_log_internal(__FUNCTION__, __FILE__, __LINE__,                  \
                        0, tags, level, format, ## __VA_ARGS__);           \
    } while(0)

#define lc_log_dtag(tags, level, format, ...)                              \
    do {                                                                   \
        lc_log_internal(__FUNCTION__, __FILE__, __LINE__,                  \
             LCO_SOURCE_INFO, tags, level, format, ## __VA_ARGS__);        \
    } while(0)

/**
 * If 'suspended' is true, suspend all logging until called back with
 * 'suspended' set to false.  All calls to logging functions declared
 * in logging.h will return immediately without doing anything.
 *
 * This facility is mainly intended to support forking from
 * multithreaded applications, where it is not safe to call syslog()
 * until you exec another binary on top of the forked child.  It is
 * not intended that logging ever be unsuspended, as this happens
 * automatically on the exec.  After calling lc_log_suspend(), the
 * code between the fork and exec may call lc_log_...() to its heart's
 * content and not incur any danger.
 *
 * This call is NOT threadsafe.  That should not be a problem, as
 * there is only one thread present after forking from a multithreaded
 * program.
 */
void lc_log_suspend(tbool suspended);

/**
 * There are certain options/settings (logging level, etc.) that we
 * normally check at most once per second.  This function resets the
 * timestamps we use to track that, to force them to be rechecked
 * the next time something is logged.  Call this when you know
 * one of the settings has changed, so you don't have to wait up to
 * a second for it to take effect.
 */
int lc_log_recheck(void);

/**
 * Set a mapping between a native thread ID (what is returned from
 * pthread_self()) and another unsigned integer.  This function is
 * offered because native thread IDs are huge numbers which only
 * differ in a few digits, making them both take up too much space,
 * and annoying to differentiate at a glance.
 *
 * This function should be called only after lc_log_init().  Calling
 * this function will log a message containing both the native and
 * compact thread IDs, so the mapping can be established from looking
 * at the logs.  All subsequent messages from the specified native
 * thread ID will only be logged using the specified compact thread ID.
 *
 * In case some native thread IDs are mapped and some are not, a
 * different notation is used depending on whether or not we are
 * logging a native thread ID or a compact thread ID.  These notations
 * are defined by lc_log_tid_prefix_native and
 * lc_log_tid_prefix_compact.
 */
int lc_log_set_thread_id(uint64 native_thread_id, uint32 compact_thread_id);

#define lc_log_tid_prefix_native "TID"
#define lc_log_tid_prefix_compact "tid"

#ifdef __cplusplus
}
#endif

#endif /* __LOGGING_H_ */
