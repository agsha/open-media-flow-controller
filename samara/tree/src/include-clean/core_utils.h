/*
 *
 * core_utils.h
 *
 *
 *
 */

#ifndef __CORE_UTILS_H_
#define __CORE_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file core_utils.h Utilities for working with coredumps
 * \ingroup lc
 */

#include "common.h"

/**
 * Option flags for lc_coredump_generate().
 */
typedef enum {
    lcf_none = 0,

    /**
     * Should we also syslog a backtrace of the call stack?
     */
    lcf_backtrace_log     = 1 << 0,

    /**
     * Should we disable the timestamp used to uniquify the genereated
     * core filename?  By default core files will be named
     * "core.<timestamp>.<pid>" .  With this flag specified, they will
     * be named "core.<pid>" .  If you specify this, and then call
     * lc_coredump_generate() multiple times, coredumps will be
     * overwritten.
     */
    lcf_timestamp_disable = 1 << 1,

    /**
     * Do not generate a coredump.  This is only useful if
     * lcf_backtrace_log is set .
     */
    lcf_disable_coredump  = 1 << 2,

} lc_coredump_flags;

/** 
 * A bit field of ::lc_coredump_flags ORed together.
 */
typedef uint32 lc_coredump_flags_bf;


/**
 * Creates a coredump of a running program.
 * 
 * \param pid pid of process to dump a core of, or -1 for the current
 *  process.
 *
 * \param output_dir directory to dump the core in, or NULL for the
 * current working directory of the given process.
 *
 * \param flags A bit field of ::lc_coredump_flags ORed together.
 */
int lc_coredump_generate(pid_t pid, const char *output_dir, 
                         lc_coredump_flags_bf flags);

#ifdef __cplusplus
}
#endif

#endif /* __CORE_UTILS_H_ */
