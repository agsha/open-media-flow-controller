/*
 *
 * backtrace.h
 *
 *
 *
 */

#ifndef __BACKTRACE_H_
#define __BACKTRACE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/**
 * \file src/include/backtrace.h Utilities for logging a backtrace of 
 * a running process
 * \ingroup lc
 */

#include "common.h"

/**
 * Log a backtrace of the current process.
 *
 * From the checkin comment:
 * For the symbol resolution stuff to even sort of work, you must
 * add LDADD+=-rdynamic to your Makefile.
 *     
 * To get the addresses converted to lines, you can either use gdb (and
 * "info line *0x....." , or use the addr2line binary.  Both must be used
 * against the binary that logged the message.  An example usage of
 * addr2line:
 *
 * <pre>
 *   cat /var/log/messages | grep '\[0x.*\] $' | 
 *   sed 's/^.*\[\(0x[0-9a-f]*\)\]/\\1/' |
 *   addr2line -e /opt/tms/bin/cli
 * </pre>
 *
 * (NOTE: immediately before the '1' is just a single backslash.
 * If you are reading these comments in Doxygen, it should be as
 * you see it; if you are reading this directly in a header file,
 * you'll see an extra backslash, which was necessary to make the
 * Doxygen version come out correctly.)
 *
 * \param log_level The syslog logging severity level at which to
 * log the message.  (See output of <tt>'man 3 syslog'</tt> for a list)
 * \param prefix A string that each line to be logged will be 
 * prefixed with.
 */
int lbt_backtrace_log(int log_level, const char *prefix);

#ifdef __cplusplus
}
#endif

#endif /* __BACKTRACE_H_ */
