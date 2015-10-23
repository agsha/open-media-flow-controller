/*
 *
 * pro_mod_api.h
 *
 *
 *
 */

#ifndef __PRO_MOD_API_H_
#define __PRO_MOD_API_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "tbuffer.h"
#include "ttime.h"

/** \defgroup progress Progress Wrapper module API */

/**
 * \file src/include/pro_mod_api.h API for progress wrapper modules.
 * \ingroup progress
 *
 * A module's job is to figure out the current state of progress of
 * the utility which it was written to monitor.  It can do this by
 * watching the output of the utility on stdout or stderr; or by other
 * means, such as polling the size of a file the utility is working on
 * creating.
 *
 * The infrastructure chooses a module to call based on the path to
 * the utility binary (or a user override, if used).
 *
 * The wrapper's stdin is passed through to the utility's stdin
 * unmodified.  The utility's stdout and stderr is passed back to the
 * wrapper's, but only after being run past the module.  The module
 * may derive any information it wants by scanning this data, and/or
 * can make modifications to the data (e.g. to filter out output that 
 * only pertains to progress tracking).  If the module does nothing,
 * the output is passed through unmodified.
 *
 * Each module registers a single callback, which can be called under
 * two circumstances:
 *
 *   1. Some output has been read from the utility being run.  This is 
 *      the module's opportunity to read and/or filter the output.
 *
 *   2. A periodic timer has gone off.  (Only if pif_timer flag
 *      specified at registration.)  The interval of this timer defaults
 *      to PRO_DEFAULT_MIN_POLL_INTERVAL_MS (in pro_main.c), and can be
 *      overridden by the -p command line argument to the wrapper.  This
 *      is the module's opportunity to poll any other sources of
 *      information besides the program's output.
 *
 * When a module wishes to report its determination of how the current
 * step is progressing, it can call pro_report_progress().
 */


/**
 * Tells the reason why a module's handler is being called.
 */
typedef enum {
    pcr_none,

    /** Data has been read from the utility's stdout */
    pcr_stdout,

    /** Data has been read from the utility's stderr */
    pcr_stderr,

    /** The periodic polling timer has gone off */
    pcr_timer,
} pro_callback_reason;


/**
 * Module's handler function prototype.
 *
 * \param reason Tells why the module is being called.
 *
 * \param callback_data The data passed to pro_reg_interpreter() when 
 * this interpreter was registered.
 *
 * \param final Indicates that this is the last time the module will
 * be called for this reason during the execution of this utility.
 * This is mainly applicable to pcr_stdout and pcr_stderr, and means
 * that the module may not return anything in ret_data_save.  That is,
 * it must process all of the remaining output now, assuming that
 * there will be no more.
 *
 * \param inout_data_passthru Only applicable if 'reason' is
 * pcr_stdout or pcr_stderr.  Contains the raw data read from the
 * stream.  If some data was returned in ret_data_save in a previous
 * call, that will be at the beginning, followed by whatever new data
 * has since come in.
 *
 * \retval ret_data_save A buffer of data (presumably copied from 
 * inout_data_passthru) to be saved, and passed at the beginning of 
 * the inout_data_passthru buffer next time.  Normally to be used if
 * you are not sure what to make of the data in inout_data_passthru,
 * because the block being read is not yet complete.
 */
typedef int (*pro_interpreter_callback)(pro_callback_reason reason,
                                        void *callback_data, tbool final,
                                        tbuf *inout_data_passthru,
                                        tbuf **ret_data_save);

/**
 * Registration flags for progress interpreters.
 *
 * By default, a progress interpreter's handler is called only for
 * activity on stdout and stderr.
 */
typedef enum {
    pif_none = 0,

    /** Do not call the handler with stdout activity; straight passthru */
    pif_no_stdout = 1 << 0,

    /** Do not call the handler with stderr activity; straight passthru */
    pif_no_stderr = 1 << 1,

    /** Call the handler periodically, according to sampling interval */
    pif_timer = 1 << 2,

} pro_interpreter_flags;

/** A bit field of ::pro_interpreter_flags ORed together */
typedef uint32 pro_interpreter_flags_bf;


/**
 * Register a progress interpreter for a specified program.
 *
 * \param util_path Full filesystem path to the utility for which the
 * interpreter wants to be called.  Currently, only one interpreter 
 * can register for a given path.
 *
 * \param interp_name A string by which the interpreter can be referred to.
 * This may be used in logs to identify what is going on; and also if the
 * user specifies the "-f" command line option to force a particular 
 * interpreter, it will be this name used to choose one.
 *
 * \param handler Handler to be called when appropriate.  This will be
 * automatically called when activity is seen on stdout or stderr
 * while this utility is running.  If the pif_timer flag is specified 
 * in 'flags', it will also be called periodically, on a fixed interval.
 *
 * \param flags Option flags.
 *
 * \param handler_data Data to be passed to the handler every time it is 
 * called.
 */
int pro_reg_interpreter(const char *util_path, const char *interp_name,
                        pro_interpreter_callback handler,
                        pro_interpreter_flags_bf flags, void *handler_data);


/**
 * Do we plan to provide quantitative progress updates?  That is, will
 * we ever be filling in percent_done, total_time, or total_time_left?
 * This will be used as a signal to the client on how to display
 * progress information.  If no, the client will probably display the
 * status string, instead of putting up a progress bar.
 */
typedef enum {
    pq_unspecified = 0,
    pq_yes,
    pq_no
} pro_quant;


/**
 * Update the state of progress on the operation being tracked.
 *
 * This will ultimately result in the progress being reported in
 * whatever way the wrapper is doing it (initially, by mdreq), but
 * that may not happen right when this is called.  e.g. the
 * infrastructure may not report progress until the callback has
 * returned, in case it calls this function more than once.  Also, 
 * it might delay reporting to honor the maximum allowed reporting
 * frequency.
 *
 * \param percent_done Percentage of operation finished, in [0..100],
 * or any negative number to leave the percentage unmodified.
 *
 * \param total_time Estimate of the total amount of time the entire
 * operation will have taken from start to finish, or any negative 
 * number to not specify.  Note that this is mutually exclusive with
 * total_time_left: if a module specifies both, one of them (undefined
 * which) will be ignored.
 *
 * \param total_time_left Estimate of the amount of time left from now
 * until the operation finishes, or any negative number to not specify.
 * Note that this is mutually exclusive with total_time.
 *
 * \param step_status Free-form description of current step status.
 * Optional; pass NULL to not change previous status.
 *
 * \param quant Specify whether or not we plan to provide percentage
 * done information for this step.  Pass pq_unspecified to not specify
 * yet.
 */
int pro_report_progress(float32 percent_done, lt_dur_ms total_time,
                        lt_dur_ms total_time_left, const char *step_status,
                        pro_quant quant);

/**
 * Convert a tbuffer into a tstring, converting any 0 bytes in the
 * contents into the number specified in replace_zero.  We pass the
 * output in a tbuffer to preserve 0 bytes, but a module may want a
 * tstring version of the output for easier analysis.
 */
int pro_tbuf_to_tstr(const tbuf *tb, uint8 replace_zero, tstring **ret_ts);

/**
 * Get the file path supplied with the "--src-file" argument, if any.
 */
const char *pro_get_src_file(void);

/**
 * Get the file path supplied with the "--dest-file" argument, if any.
 */
const char *pro_get_dest_file(void);

/**
 * Get the file path supplied with the "--dest-dir" argument, if any.
 */
const char *pro_get_dest_dir(void);

/**
 * Get the file size supplied with the "--file-size" argument, or -1 if
 * the argument was not specified.
 */
int64 pro_get_file_size(void);

/**
 * Get the full set of command-line arguments passed to the utility
 * being launched.  This is its argv, so the 0th element will be the
 * path to the utility itself.  Note that this does NOT include the
 * arguments passed to the progress wrapper itself.
 */
const tstr_array *pro_get_argv(void);

/**
 * A callback to be called from pro_extract_lines() with every complete
 * line of output read from stdout or stderr.
 *
 * \param data The handler_data passed to pro_extract_lines().
 *
 * \param reason Tells whether the data was seen on stdout or stderr.
 * Will only be pcr_stdout or pcr_stderr.
 *
 * \param line The line of output read.
 *
 * \retval ret_consume Indicate to the caller whether this line of output
 * should be consumed (return 'true'), or passed through unmodified 
 * (return 'false).
 */
typedef int (*pro_line_callback)(void *data, pro_callback_reason reason,
                                 const char *line, tbool *ret_consume);


/**
 * Extract complete lines (terminated with '\n' characters) from the
 * output of the utility program.  Call a provided handler with every
 * complete line extracted.  Complete lines are either passed through,
 * or not, depending on the 'ret_consume' return value from the
 * handler.  If there is an incomplete line at the end of the output,
 * it is transferred to ret_data_save.
 */
int pro_extract_lines(pro_callback_reason reason, tbool final, 
                      tbuf *inout_data, tbuf **ret_data_save, 
                      pro_line_callback handler, void *handler_data);


/**
 * Poll the size of the file specified with the "--dest-file" argument.
 * Report progress based on its size.  The progress report can be done
 * in one of three ways:
 *
 *   1. If you specify total_file_size, we report percent_done based
 *      on that.
 *
 *   2. If total_file_size is -1, but the --file-size parameter was passed
 *      to the progress wrapper, we report percent_done based on that.
 *
 *   3. If the file size is not available from either source, we do not
 *      calculate percent done.  Instead, we just update the step status
 *      string.  The new status string is constructed by dividing the
 *      current file size by scale_factor (allowing you to display the
 *      number in bytes, kB, MB, or GB), and then rendering it using
 *      smprintf() with status_fmt and the resulting number.  Your 
 *      status_fmt should have exactly one %f (optionally specifying 
 *      field width, precision, etc.).
 *
 * Note that scale_factor and status_fmt are only used in case #3.
 */
int pro_poll_file_size(int64 total_file_size, int scale_factor,
                       const char *status_fmt);

/**
 * Poll the size of a file found in the directory specified with the
 * "--dest-dir" argument.  Assumes that only one file will be found in
 * this directory, and that it is the one we are interested in.  Pass
 * -1 for total_file_size to use the file size specified with the
 * "--file-size" argument instead.
 */
int pro_poll_dir_file_size(int64 total_file_size, int scale_factor,
                           const char *status_fmt);

/**
 * Tell the progress wrapper infrastructure to preserve a specified file
 * descriptor when it launches the utility.  This is a mechanism you can
 * use to communicate with the utility, e.g. how scp can take a password
 * in an fd specified in the SSH_ASKPASS_FD environment variable.
 */
int pro_launch_preserve_fd(int fd);

#ifdef __cplusplus
}
#endif

#endif /* __PRO_MOD_API_H_ */
