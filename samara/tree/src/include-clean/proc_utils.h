/*
 *
 * proc_utils.h
 *
 *
 *
 */

#ifndef __PROC_UTILS_H_
#define __PROC_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include "tstring.h"
#include "typed_arrays.h"
#include "tstr_array.h"
#include "ttime.h"

/* ========================================================================= */
/** \file proc_utils.h Functions to help in launching and
 * manipulating processes.
 * \ingroup lc
 */

/* ------------------------------------------------------------------------- */
/** Options for how to figure out what fd to give a launched process
 * for one of {stdin, stdout, stderr}.  Used as part of an
 * ::lc_io_origin structure.
 */
typedef enum {
    lioo_none = 0,

    /** 
     * Inherit this fd from the parent process (do nothing). 
     */
    lioo_preserve, 

    /**
     * Close this fd; then open /dev/null and dup2() it onto this fd.
     */
    lioo_devnull,

    /**
     * Close this fd; then create a pipe, and dup2() the appropriate
     * end of it (the reading end for stdin, the writing end for
     * stdout/stderr) onto this fd for the launched process, and
     * return the other end of it to the parent process.  Note that
     * this option is not available if the lp_fork flag is false.
     */
    lioo_pipe_parent,

    /**
     * Same as ::lioo_pipe_parent, except that the same fd will be
     * dup2()ed onto all streams that have this option set.  This is
     * only valid if used on both STDOUT and STDERR, but not STDIN,
     * since pipes are not bidirectional.  This may be useful for
     * capturing the full output of a program, interleaved correctly;
     * with separate pipes, the relative ordering of STDOUT and STDERR
     * output can be lost.
     *
     * NOT YET IMPLEMENTED.
     */
    lioo_pipe_parent_combined,

    /**
     * Close this fd; then open the specified filename for the
     * appropriate operation (reading for stdin, append for
     * stdout/stderr), and dup2() it onto this fd.  Must be
     * accompanied by an absolute path to use.  In the case of stdout
     * and stderr, must be accompanied by a mode to be used in case
     * the file needs to be created.
     */
    lioo_filename_redir,
    
    /**
     * The generic version of the above three options.  Close this fd,
     * then dup2() whatever fd is provided by the caller onto this
     * one.
     */
    lioo_fd_redir,

    /**
     * Only valid for stdout, stderr, or both.  Must be used in
     * combination with lp_wait set to true.  A temporary file is
     * created, and the output stream(s) specified are redirected to
     * this file.  When the program exits, the contents of this file
     * are automatically read in the lc_launch_result structure; the
     * caller need not call lc_launch_complete_capture().
     */
    lioo_capture,

    /**
     * Only valid for stdout, stderr, or both.  Must be used in
     * combination with lp_wait set to false.  A temporary file is
     * created, and the output stream(s) specified are redirected to
     * this file.  After lc_launch() returns (i.e. after the program
     * exits), the caller must call lc_launch_complete_capture(); this
     * ensures that all of the output is captured, and then it is
     * transferred from the temporary file into the lr_captured_output
     * field of the lc_launch_result structure.
     */
    lioo_capture_nowait,

    lioo_last,
} lc_io_origin_opts;

/* ------------------------------------------------------------------------- */
/** Specify what fd to give a launched process for one of {stdin,
 * stdout, stderr}.  A launch request has one instance of this for
 * each of those three streams.
 */
typedef struct lc_io_origin {
    /** Defaults to lioo_preserve */
    lc_io_origin_opts lio_opts;

    /** Only heeded if lio_opts is lioo_filename_redir.
     * Its default is NULL.  NOTE: when you provide a string using this
     * field, you are giving up ownership of the string.  When the launch
     * params struct is freed later, the strings in each of its three
     * instances of this struct will be freed.  Therefore, you must not
     * free them yourself, and you must also not use the same tstring for
     * more than one of these instances, as it would then be double-freed.
     */
    tstring *lio_filename;

    /** Only heeded if lio_opts is lioo_filename_redir and if
     * the structure is for stdout or stderr.  Its default is 
     * (S_IRUSR | S_IWUSR): read and write for owner, and nothing else.
     */
    int lio_mode;

    /** Only heeded if lio_opts is lioo_fd_redir.  Its default is -1.
     */
    int lio_fd;
} lc_io_origin;

/* ------------------------------------------------------------------------- */
/** @name Standard file descriptor indices.
 *
 * Indices into the lp_io_origins and lc_io_fds arrays for each of the
 * three main stdio fds.
 */
/*@{*/
static const int lc_io_stdin  = 0;
static const int lc_io_stdout = 1;
static const int lc_io_stderr = 2;

#define lc_io_num_streams 3

/** Perhaps a bit pedantic, but an attempt to not assume what values
 * the standard fds actually have.
 */
static const int lc_io_fds[] = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
/*@}*/


/* ------------------------------------------------------------------------- */
/** Specify which file descriptors to close for the child process,
 * aside from stdin, stdout, and stderr, whose fate is determined
 * separately.  cfo_close_some and cfo_preserve_some must be
 * accompanied by an array of fd numbers to close or preserve.
 */
typedef enum {
    cfo_none = 0,
    cfo_close_all,
    cfo_close_some,
    cfo_preserve_some,
    cfo_preserve_all,
    cfo_last
} lc_close_fd_option;


/* ------------------------------------------------------------------------- */
/** Specify what to do with the existing environment when launching a
 * process.  eo_preserve_some and eo_clear_some must be accompanied by
 * an array of names of environment variables to preserve or clear.
 */
typedef enum {
    /**
     * Setting not specified; not a valid choice.
     */
    eo_none = 0,

    /** 
     * Preserve the existing environment untouched
     */
    eo_preserve_all, 

    /**
     * Preserve only those variables listed in lp_env_preserve.
     * All others are cleared.
     */
    eo_preserve_some, 

    /**
     * Clear all variables from the environment.
     */
    eo_clear_all, 

    /** 
     * Clear only those variables listed in lp_env_clear.
     * All others are preserved.
     */
    eo_clear_some,

    eo_last
} lc_environ_option;


/* ------------------------------------------------------------------------- */
/** A function to be called from the child process created in a call
 * to lc_launch().  This function should call exit() when it is done;
 * if it returns, an error will be logged by lc_launch().
 *
 * \param data The data passed in the
 * lc_launch_params::lp_child_start_func_data field of the structure 
 * that provided the pointer to this function.
 */
typedef int (*lc_launch_child_start_func)(void *data);


/* ------------------------------------------------------------------------- */
/** Parameters to specify how to exec or launch (fork and exec) a
 * process using lc_launch().
 *
 * Do not fill out this structure from scratch; call
 * lc_launch_params_defaults() first to populate it and then change
 * the fields you want to be different.
 */
typedef struct lc_launch_params {

    /**
     * Path to the binary to launch.  Defaults to NULL.  This is the
     * only field which is mandatory to change from its default.
     *
     * Exception: if you are going to set lp_fork to true, and
     * lp_child_start_func to a function to be called from the child,
     * this can (and must) be left empty.
     *
     * Must be absolute if lp_allow_relative_path is not set;
     * if it is, may be relative to current working directory, or
     * may be a binary name found in one of the directories listed
     * in the PATH environment variable.
     */
    tstring *lp_path;

    /**
     * If lp_fork is true, this is a function that should be called
     * instead of exec'ing the program in lp_path.  If this is set,
     * the following fields are ignored, and should not be set:
     * lp_path, lp_allow_relative_path, and lp_argv.
     *
     * NOTE: this function should not return to the caller, except
     * in case of serious internal error.  The proc_utils API will
     * log at ERR if this function returns.  The function should
     * either remain running, or call exit() when it is finished.
     */
    lc_launch_child_start_func lp_child_start_func;

    /**
     * Data to be passed to lp_child_start_func when it is called.
     */
    void *lp_child_start_func_data;

    /**
     * String description of lp_child_start_func, usually just the
     * name of the function itself.  This is just to help make log
     * messages about the handler more meaningful.  A suffix of "()"
     * (commonly used when naming C functions) is not appended for you
     * automatically, but it is recommended that the caller include it.
     */
    const char *lp_child_start_func_name;

    /**
     * Should we permit lp_path to be non-absolute?  This will cause
     * us to use execvp() to execute the binary, rather than execve().
     * This is illegal from a multithreaded application.  Defaults 
     * to false.  If false, a non-absolute lp_path will result in
     * an error.
     */
    tbool lp_allow_relative_path;

    /**
     * Arguments to pass to program as its argv.  As such the 0th
     * element should in general match the lp_path field unless you
     * are trying to trick the process.  Defaults to NULL; the caller
     * must allocate and populate the array or else it will be treated
     * as empty.
     */
    tstr_array *lp_argv;

    /**
     * Specify whether or not to fork() before calling exec().
     * Note that if this is false, lioo_pipe_parent is an illegal
     * setting for lp_io_opts[x].lio_opts, and lc_launch() never
     * returns unless there is an error.  Defaults to true.
     */
    tbool lp_fork;

    /**
     * If lp_fork is true, specify whether or not to wait for the
     * launched process to terminate before returning.  Defaults
     * to false.
     *
     * CAUTION: be wary of using this in a multithreaded process when
     * you have a SIGCHLD handler registered which calls wait() or 
     * some variant thereof.  This may cause a race condition where
     * the other thread reaps your process before we get a chance to.
     *
     * CAUTION: make sure the child process does not block on anything
     * it expects the parent process to do!  The lc_launch() call will
     * return immediately with an error if this is specified in 
     * conjunction with lioo_pipe_parent on stdin.
     *
     * CAUTION: if the process never terminates, this will block
     * forever!  waitpid() does not take a timeout.
     */
    tbool lp_wait;

    /*
     * XXX/EMT: could offer an option to be used in combination with
     * lp_wait and with an augmented lioo_filename_redir that dups the
     * same fd onto stdout and stderr, to return the combined output
     * (properly interleaved) of the two in a buffer.
     */

    /**
     * If lp_fork is true, and if the child we fork fails before or
     * during the exec(), call waitpid() with its PID to reap the
     * failed child process.  If false, do not reap it, so the child
     * will be a zombie until the caller takes care of it.
     * Defaults to true.
     */
    tbool lp_reap_failed_child;

    /**
     * Working directory for the launched process.  If NULL, it will
     * inherit the working directory of the parent process.  Defaults
     * to NULL.
     */
    tstring *lp_working_dir;

    /* XXXX-PORT: uid_t could be bigger! */

    /**
     * Uid to assign to the new process using seteuid()
     * If -1, leave the uid the same as the parent process.
     * Defaults to -1. 
     */
    int32 lp_uid;

    /**
     * Gid to assign to the new process using setegid()
     * If -1, leave the gid the same as the parent process.
     * Defaults to -1.
     */
    int32 lp_gid;

    /**
     * Specifies whether or not to call setsid() for child process, to
     * put it into its own process session.  Defaults to false.
     */
    tbool lp_setsid;

    /**
     * Specifies whether or not launched process needs to take control of
     * the TTY.  Defaults to false.
     *
     * If this is set to true, the parent will cede control of the
     * TTY.  After the child process exits, the parent should call
     * lc_launch_reclaim_tty() to get it back.
     */
    tbool lp_seize_tty;

    /**
     * There is one lp_io_opts structure for each of {stdin, stdout,
     * stderr}, specifying what to give the launched process for each
     * one of these.  Use lc_io_stdin, lc_io_stdout, and lc_io_stderr
     * as array indices to access the three array members.  See
     * lc_io_origin comment above for details on how to fill it in,
     * and what the defaults are.
     *
     * lp_close_other_fds specifies whether or not to close all other
     * fds besides these three.  Defaults to true.
     */
    lc_io_origin lp_io_origins[3];

    /**
     * Specify which other file descriptors the launched process
     * should inherit.  This pertains only to fds other than stdin,
     * stdout, and stderr.  Defaults to cfo_close_all,
     */
    lc_close_fd_option lp_close_fd_opts;

    /**
     * If lp_close_fd_opts is ::cfo_close_some, the list of file
     * descriptors to close.
     */
    uint32_array *lp_fd_close;

    /**
     * If lp_close_fd_opts is ::cfo_preserve_some, the list of file
     * descriptors not to close.
     */
    uint32_array *lp_fd_preserve;

    /**
     * Changes to make to signal mask of the launched process using
     * sigprocmask(): passed as the 'how' argument to sigprocmask().
     * Defaults to SIG_SETMASK, which in conjunction with the default
     * for lp_sigmask_set, would cause the new process to have no signals
     * blocked.
     *
     * Note that all signal handlers are reset to SIG_DFL in the new
     * process.
     */
    int lp_sigmask_how;

    /**
     * Changes to make to signal mask of the launched process using
     * sigprocmask(): passed as the 'set' argument to sigprocmask().
     * Defaults to an empty signal mask set, which in conjunction with
     * the default for lp_sigmask_how, would cause the new process to
     * have no signals blocked.
     */
    sigset_t lp_sigmask_set;

    /**
     * Specify what environment the launched process should inherit.
     * Defaults to eo_clear_all.
     *
     * The requests from this, lp_env_clear, and lp_env_preserve are
     * honored first, before lp_env_set_names and lp_env_set_values.
     * Then, the values they specify are set, overwriting any
     * preserved environment variables with the same names.
     *
     * Note that when the launch params object is freed, any string
     * arrays put here are freed as well.
     */
    lc_environ_option lp_env_opts;

    /**
     * If lp_env_opts is ::eo_clear_some, specifies which environment
     * variables to clear.
     */
    tstr_array *lp_env_clear;

    /**
     * If lp_env_opts is ::eo_preserve_some, specifies which environment
     * variables to preserve.
     */
    tstr_array *lp_env_preserve;

    /**
     * Names of environment variables to set when launching this
     * process.  These variables are set after clearing whichever
     * variables are specified through lp_env_opts, lp_env_clear, and
     * lp_env_preserve.  The values are specified in
     * lp_env_set_values.
     *
     * This field starts out NULL; you must allocate a new tstr_array
     * if you want to set environment variables.
     */
    tstr_array *lp_env_set_names;
    
    /**
     * Values of environment variables to set when launching this
     * process.  This array must be "parallel" to lp_env_set_names;
     * i.e. they must have the same number of members, and the member
     * at any given position in one corresponds to the member at the
     * same position in the other.
     *
     * This field starts out NULL; you must allocate a new tstr_array
     * if you want to set environment variables.
     */
    tstr_array *lp_env_set_values;

    /**
     * What scheduling priority ('nice' level) should the launched
     * process be given using setpriority().  If the value is
     * INT32_MIN, setpriority() will not be called.  Defaults to
     * INT32_MIN.  If it is not INT32_MIN, it must be in the range
     * [PRIO_MIN..PRIO_MAX]; higher levels are lower priority.
     */
    int32 lp_priority;

    /**
     * What CPU affinity mask (list of CPUs the process is allowed 
     * to execute on) should the launched  process be given using 
     * sched_setaffinity().  If the value is NULL, sched_setaffinity() 
     * will not be called.  Defaults to NULL, which means the process
     * inherits the CPU affinity mask from its parent (by default, all
     * CPUs in the system are permitted).  If it is not NULL, 
     * it must be a list of unique CPU numbers (zero-based).
     *
     * Currently supported only on Linux systems.
     */
    uint16_array *lp_affinity;

    /**
     * Maximum size, in bytes, to be permitted for core files from
     * this process.  This will set both the hard and soft limits for
     * core files; unless the process is being launched with uid 0, it
     * will not be able to undo the limit.  Zero indicates that the
     * process should not produce any cores.  Defaults to
     * RLIM_INFINITY (no limit).
     */
    rlim_t lp_core_size_limit;

    /**
     * Maximum size, in bytes, to be permitted for this process's
     * virtual memory. This will set both the hard and soft limits for
     * memory; unless the process is being launched with uid 0, it
     * will not be able to undo the limit. Defaults to RLIM_INFINITY
     * (no limit).
     */
    rlim_t lp_memory_size_limit;

    /**
     * Maximum number of file descriptors which the process should be
     * allowed to open.  Defaults to -1, which means do not change.
     */
    int32 lp_fd_limit;

    /**
     * Umask to give the newly-launched process.  -1 means to inherit
     * the umask from the parent.  Defaults to -1.
     */
    int32 lp_umask;

    /**
     * If false, we abort the launch and return failure if anything
     * goes wrong.  If true, nonfatal errors are logged but the launch
     * proceeds anyway.  Nonfatal errors include problems changing the
     * working directory, or setting the uid or gid.
     * 
     * Note that setsid() ought never to fail, as it is only supposed
     * to fail if the process is already a group leader, which is not
     * possible since we just forked it.
     * 
     * Defaults to false.
     */
    tbool lp_tolerate_errors;

    /**
     * Log level at which to log information about the process being
     * launched.  This would include the binary name, arguments sent,
     * and whether or not it is being forked.  If the log level is not
     * between LOG_DEBUG and LOG_EMERG, nothing is logged.  Defaults
     * to INT32_MIN (i.e. don't log anything).
     */
    int32 lp_log_level;

    /**
     * Log level at which the argv of the process being launched
     * should be logged.  The default (INT32_MAX) means to adopt
     * whatever log level is set in lp_log_level, so set this only 
     * if you want the argv logged at a different level than the rest
     * of that information.  Most commonly, this would come up if the
     * argv contains potentially sensitive information, and so you
     * wanted it to be logged at a lower level, or not logged at all.
     *
     * Use INT32_MIN to request that the argv not be logged at all.
     *
     * Note: this option may not be set if lp_log_level is unset.
     * In other words, it can only be used to modify the logging 
     * level of the argv if the other parameters are also logged
     * at some level.
     */
    int32 lp_log_level_argv;

    /**
     * Specify the directory in which to place the file holding the
     * captured program output from lioo_capture or lioo_capture_nowait.
     * By default, it will use prod_vtmp_root, which is /vtmp.
     * This is a 64 MB tmpfs, which has the advantage of (usually) having
     * space even if /var fills up.  But if you need to capture more than
     * 64 MB of output (or might run concurrently with other components
     * such that the total usage of /vtmp might exceed 64 MB), you 
     * might want to change this to /var/tmp or something.
     *
     * Defaults to NULL, meaning to use the default directory /vtmp.
     */
    tstring *lp_capture_dir;

} lc_launch_params;

/* ------------------------------------------------------------------------- */
/** Contains information resulting from a call to lc_launch(), if
 * lp_fork was specified.  lr_child_pid is always filled out, but will
 * be -1 if lp_wait was specified, since the child process will no
 * longer be in existence.  The three lr_child_*_fd fields are filled
 * out only if the corresponding lc_io_origin structure in the request
 * was set to lioo_pipe_parent; otherwise they are set to -1.
 */
typedef struct lc_launch_result {
    /**
     * PID of child launched.  If lp_wait was specified, this will be -1,
     * since the child is no longer running.
     */
    pid_t lr_child_pid;

    /**
     * Exit status of child, if lp_wait was specified.  This is the status
     * code returned from waitpid(); use the macros its man page describes
     * for extracting information from it, e.g. WEXITSTATUS.
     */
    int lr_exit_status;

    /**
     * Output of process on stdout and/or stderr, if lioo_capture or
     * lioo_capture_nowait was specified on either.  This is
     * dynamically allocated and must be freed by the caller.
     */
    tstring *lr_captured_output;

    /**
     * FD of open file used to capture output on stdout and/or stderr
     * if lioo_capture_nowait was specified on either, and if lp_wait
     * was false.  This is not intended to be referenced by the
     * caller, but rather for internal use by the library.  After
     * lc_launch() returns, the temporary file is left open, and
     * lr_captured_output is not yet populated.  The caller must then
     * call lc_launch_complete_capture().  This transfers the captured
     * output to lr_captured_output and cleans up the temporary file.
     */
    int lr_nowait_capture_fd;

    /**
     * Filename of temporary file used to capture output on stdout
     * and/or stderr if lioo_capture_nowait was specified on either,
     * and if lp_wait was false.  This is not intended to be
     * referenced by the caller, but rather for internal use by the
     * library.
     */
    char lr_nowait_capture_filename[1024]; /* XXX magic number */

    /** Only populated if IO origin for stdin is lioo_pipe_parent */
    int lr_child_stdin_fd;

    /** Only populated if IO origin for stdout is lioo_pipe_parent */
    int lr_child_stdout_fd;

    /** Only populated if IO origin for stderr is lioo_pipe_parent */
    int lr_child_stderr_fd;

} lc_launch_result;

extern const lc_launch_result LC_LAUNCH_RESULT_INIT;

/* For backward compatibility */
#define LC_LAUNCH_RESULT_INITIALIZER LC_LAUNCH_RESULT_INIT

/* ------------------------------------------------------------------------- */
/** Return a dynamically-allocated instance of lc_launch_params, filled
 * with the default parameters.  The comment next to each field in
 * lc_launch_params above tells whether or not the memory needed for
 * each individual member is allocated by default.
 *
 * \retval ret_launch_params Default values
 */
int lc_launch_params_get_defaults(lc_launch_params **ret_launch_params);

/* ------------------------------------------------------------------------- */
/** Free a dynamically-allocated lc_launch_params structure, along with
 * all of its contents.
 *
 * \param inout_launch_params structure to free
 */
int lc_launch_params_free(lc_launch_params **inout_launch_params);

/* ------------------------------------------------------------------------- */
/** Free the contents of an lc_launch_result structure.  This is currently
 * only needed when capturing output.
 *
 * \param inout_launch_result structure to free
 */
int lc_launch_result_free_contents(lc_launch_result *inout_launch_result);

/* ------------------------------------------------------------------------- */
/** Launch a process according to the parameters specified in the
 * provided lc_launch_params structure.  The params structure may not
 * be NULL, as some of the fields are mandatory to be filled out.
 *
 * If lp_fork was true and lp_wait was false, returns as soon as
 * process is forked.  If lp_fork was true and lp_wait was true,
 * returns as soon as the forked process exits.  If lp_fork was false,
 * returns only if there was an error, otherwise never returns as the
 * current process is replaced.
 *
 * If an error occurs before exec'ing the child, an error will be
 * returned from lc_launch().
 *
 * \param params Specified parameters.  This function does not take
 * ownership of the structure, so the caller must free it.  This
 * function does not need the structure to live beyond its return,
 * even if the process has not terminated yet, so the caller may free
 * it as soon as lc_launch() returns.
 *
 * \retval ret_result may be NULL if the caller does not care about the
 * results, or if lp_fork is not set.  If it is not, it should already
 * point to an allocated structure, which this function simply fills in.
 * None of the members that are filled in need to be freed.
 */ 
int lc_launch(const lc_launch_params *params, lc_launch_result *ret_result);

/* ------------------------------------------------------------------------- */
/** A shortcut to launching processes in a standard manner.  Fork a
 * process, preserving all environment variables, wait for it to
 * finish, and return its exit code.  Optionally return whatever it
 * prints to stdout.
 *
 * \retval ret_status The exit status of the process.
 *
 * \retval ret_output If this is non-NULL, returns a pointer to a
 * tstring holding what the process printed to stdout during its run.
 * Note that if this is NULL, the output of the process will not be
 * captured at all, and if there is any, it will still go to the
 * stdout or stderr of the running process.
 *
 * \param argc Number of arguments on the command line, including the 
 * name of the binary.
 *
 * \param path Full path to the program to be launched.  This is
 * argv[0].  The variable argument list following it contains the
 * other arguments, according to how many were specified in argc.
 *
 * Failure is only returned if we were unsuccessful in launching the
 * program.  Otherwise, success is returned, regardless of the
 * program's exit code.  A warning will be logged in failure cases.
 */

/*
 * int lc_launch_quick(int32 *ret_status, tstring **ret_output, uint32 argc,
 *                  const char *prog_path, ...);
 */
#define lc_launch_quick(ret_status, ret_output, argc, path, ...)  \
                        lc_launch_quick_status(ret_status, ret_output, true, \
                                               argc, path, ## __VA_ARGS__)


/* ------------------------------------------------------------------------- */
/** A shortcut to launching processes in a standard manner.  Fork a
 * process, preserving all environment variables, wait for it to
 * finish, and return its exit code.  Optionally return whatever it
 * prints to stdout.  Optionally do not warn in warn_exit_status is false.
 *
 * \retval ret_status The exit status of the process.  Note that this
 * is not the exit code, but the status code returned from waitpid(),
 * which encodes more information.  Use the macros its man page describes
 * for extracting information from it, e.g. WEXITSTATUS.
 *
 * \retval ret_output If this is non-NULL, returns a pointer to a
 * tstring holding what the process printed to stdout during its run.
 * Note that if this is NULL, the output of the process will not be
 * captured at all, and if there is any, it will still go to the
 * stdout or stderr of the running process.
 *
 * \param warn_exit_status If this is false, do not log a warning if the exit
 * status is non-zero.
 *
 * \param argc Number of arguments on the command line, including the 
 * name of the binary.
 *
 * \param prog_path Full path to the program to be launched.  This is
 * argv[0].  The variable argument list following it contains the
 * other arguments, according to how many were specified in argc.
 *
 * Failure is only returned if we were unsuccessful in launching the
 * program.  Otherwise, success is returned, regardless of the
 * program's exit code.  A warning will be logged in failure cases.
 */
int lc_launch_quick_status(int32 *ret_status, tstring **ret_output, 
                           tbool warn_exit_status,
                           uint32 argc,
                           const char *prog_path, ...);


/* ------------------------------------------------------------------------- */
/** Reclaim the TTY.  To be called by the parent process after a child
 * process, launched with the lp_seize_tty option, exits.
 */
int lc_launch_reclaim_tty(void);

/* ------------------------------------------------------------------------- */
/** Complete a stdout/stderr file capture for a launch done with the
 * options for one or both of those streams set to
 * lioo_capture_nowait, when lp_wait was set to false.  Call this only
 * after the process has exited.  It ensures that all output has been
 * captured into the temporary file, then reads the file's contents
 * into the lr_captured_output field of the launch result structure,
 * and cleans up the temporary file.
 */
int lc_launch_complete_capture(lc_launch_result *inout_launch_result);


/* ------------------------------------------------------------------------- */
/** Send a signal to a process whose PID has been written into a file
 * in a well-known directory.
 *
 * THIS FUNCTION IS DEPRECATED.  You should instead do this through PM
 * using the /pm/actions/send_signal action.  If you are in a mgmtd
 * module, call md_send_signal_to_process() to send this action.
 */
int
lc_process_send_signal(int32 signal_number, const char *process_name,
                       const char *pid_filename);

/* ------------------------------------------------------------------------- */
/** Wait for a child process to terminate and return its PID.
 *
 * This is a wrapper to waitpid().  waitpid() can either be blocking
 * or nonblocking.  Blocking is sometimes inappropriate because the
 * caller may not be sure that there is a child to be reaped.  But if
 * it is called nonblocking, sometimes the PID of a child that just
 * recently exited will not be returned.  This appears to be a bug in
 * waitpid() on Linux, and this wrapper function helps work around it.
 *
 * This function calls waitpid() nonblocking, and potentially polling
 * several times in quick succession for terminated children.
 *
 * It also smooths over some of waitpid()'s other idiosyncracies, like
 * returning PIDs of children that are just stopped but not
 * terminated, and returning an error (instead of just no PID) if the
 * process does not have any child processes at all.
 *
 * \param pid PID of process to wait for.  This has the same semantics
 * as it does for waitpid(), except that values less than -1 are not
 * supported.  That is: -1 to wait for any process, 0 to wait for any
 * process with the same process group ID, and >0 to wait for a
 * process with that PID.
 *
 * \param num_retries Number of times we should retry if a call to
 * waitpid() does not return a terminated child process.  If this
 * is zero, we try only once.
 *
 * \param retry_interval Number of milliseconds between each retry
 * attempt.  Note that this is done using usleep() -- there is no
 * integration with the caller's event loop -- so the process is
 * blocked during the retries.
 *
 * \retval ret_pid The PID of the process that exited, or -1 if no
 * terminated child process was found.
 *
 * \retval ret_status A code containing information about the
 * circumstances under which the child process exited, if any.
 * The macros WIFEXITED, WEXITSTATUS, WIFSIGNALED, and WTERMSIG
 * can be used.
 *
 * \return A standard error code.  Note that it is not considered an
 * error for no process to be found (this is communicated with a -1
 * return PID), or for the calling process to have no children.
 */
int
lc_wait(pid_t pid, uint32 num_retries, lt_dur_ms retry_interval,
        pid_t *ret_pid, int *ret_status);


/* ------------------------------------------------------------------------- */
/** Check if a process exists
 *
 * \param pid PID of process to search for.
 *
 * \retval ret_pid_exists true if found, false on not found and error cases.
 * 
 */
int lc_pid_exists(pid_t pid, tbool *ret_pid_exists);

/* ------------------------------------------------------------------------- */
/** Send process a SIGTERM, wait for its termination, and reap it.
 *
 * If the process does not exist, nothing is logged, and
 * lc_err_not_found is returned.  If any other problems are
 * encountered terminating the process (e.g. insufficient permissions),
 * an error is logged and a generic error code returned.
 *
 * \param pid PID of process to terminate.
 *
 * \param initial_signal signal to send to terminate process
 *
 * \param wait_time time in ms to wait to reap process after initial_signal
 *  is sent.
 *
 * \param final_signal signal to send to process after it has failed to exit
 * during wait_time .  -1 for no such signal.
 * 
 */
int
lc_kill_and_reap(pid_t pid, int initial_signal, lt_dur_ms wait_time,
                 int final_signal);

/* ------------------------------------------------------------------------- */
/** Check the exit status of a process and log a message on error.
 *
 * Looks at the exit status of a process and logs an appropriate message
 *
 * \param exit_status exit status from waitpid() or lr_exit_status
 *
 * \retval ret_normal_exit Boolean true if process exited normally
 *
 * \param message Format message to print with messages.
 *
 */
int lc_check_exit_status(int exit_status, tbool *ret_normal_exit, 
                         const char *message, ...)
     __attribute__ ((format (printf, 3, 4)));


/* ------------------------------------------------------------------------- */
/** Check the exit status of a process and log a message on error.
 *
 * Looks at the exit status of a process and logs an appropriate message
 *
 * The NAME_logevel parameters are each either a syslog log level (like
 * LOG_WARNING) or -1.  If success_loglevel or failure_loglevel are -1,
 * nothing will be logged.  If exitcode_loglevel is -1, a message will be
 * logged at the success_loglevel unless success_loglevel is -1 as well.
 *
 * \param exit_status exit status from waitpid() or lr_exit_status
 *
 * \param success_loglevel the log level for successful messages
 *
 * \param exitcode_loglevel the log level for non-0 return code messages
 *
 * \param failure_loglevel the log level for failure messages
 *
 * \retval ret_normal_exit Boolean true if process exited normally
 *
 * \param message Format message to print with messages.
 *
 */
int lc_check_exit_status_full(int exit_status, tbool *ret_normal_exit, 
                              int success_loglevel,
                              int exitcode_loglevel,
                              int failure_loglevel,
                              const char *message, ...)
     __attribute__ ((format (printf, 6, 7)));

/* ------------------------------------------------------------------------- */
/**
 * Get the file descriptor of the controlling TTY, if any, or -1
 * otherwise.
 *
 * \retval ret_ctty fd of controlling TTY
 */
int lc_launch_get_ctty(int *ret_ctty);

/* ------------------------------------------------------------------------- */
/** Set the CPU affinity mask of a launch process
 *
 * \param params Libcommon launch parameters
 *
 * params->lp_affinity must be an array of the numbers of CPUs on which this 
 * process is allowed to execute on. Array must be not NULL, non-empty, and 
 * contain unique entries. CPU numbers start at 0 and must be smaller than 
 * the maximum number of processors the OS can support. If any CPU numbers 
 * will point to processors not present on the current machine, function will 
 * log a warning, and either continue execution (if params->lp_tolerate_errors
 * is true), or terminate with an error (if params->lp_tolerate_errors is 
 * false). In the former case the result of applying affinity mask will be 
 * undefined.
 *
 */
int lc_platform_set_cpu_affinity(const lc_launch_params *params);

/* ------------------------------------------------------------------------- */
/** Set (change) the CPU affinity mask of a currently running process 
 *
 * \param pid The process ID of the process to be changed
 *
 * \param affinity_arr An array of cpu numbers where this process may run
 *
 * \param tolerate_errors Whether to log a message or to bail on error
 *
 * This function is similar to lc_platform_set_cpu_affinity(), but it has a
 * generic API so that it may be applied to any process or thread oustide
 * the context of launching a process.
 *
 * The affinity_arr must be an array of the ID numbers of the CPUs on which
 * this process may execute. The array must be not NULL, but may be empty, and
 * it should contain unique entries. CPU numbers start at 0 and must be smaller
 * than the maximum number of processors the OS can support.
 *
 * If any CPU numbers reference processors not present on the current machine,
 * or if the OS request fails for any reason, this function will either 
 * log a message and continue execution (if tolerate_errors is true),
 * or terminate and return lc_err_generic_error (if tolerate_errors is 
 * false). In the former case the result of applying the affinity mask will
 * depend on whether the OS call succeeded with the remaining data.
 *
 * If the process ID is not found, this function will log a message noting the
 * request for a stale pid, and will return lc_err_not_found if tolerate_errors
 * is false.
 *
 */
int lc_platform_set_proc_cpu_affinity(const pid_t pid,
                                      const uint16_array *affinity_arr,
                                      tbool tolerate_errors);

/* ------------------------------------------------------------------------- */
/** Platform-specific function to return maximum number of CPUs
 *  which can be supported by the operating system. This is *NOT* the same
 *  as the number of CPUs/cores present in the current machine.
 */
uint16 lc_platform_maximum_number_of_cpus(void);

/* ------------------------------------------------------------------------- */
/** Platform-specific function to return the current number of CPUs available
 *  on the system.
 *
 * Returns the number of CPUs present, or zero on error.
 */
uint16 lc_platform_number_of_cpus(void);

/* ------------------------------------------------------------------------- */
/** Platform-specific function to check whether a process is running, and if
 * so, optionally return it's actual execution path, and whether that path
 * matched the expected execution path.  Note that with symbolic links, it is
 * possible for the actual execution path to differ in name from the expected
 * execution path, while the image file referenced by both paths may still be
 * identical, in which case ret_is_expected_executable would return true.
 *
 * \param pid Process ID of the process to search for.
 *
 * \param expected_exec_path Full path of the executable file expected to match
 * this PID.  If non-NULL, ret_is_expected_executable will return whether the
 * file referenced by this path has the same identity as the executable image
 * of the running process.  Otherwise, is_expected_executable always returns
 * false.
 *
 * \retval ret_is_running If non-NULL, returns whether this PID is running,
 * regardless of the expected executable path.
 *
 * \retval ret_is_expected_executable If non-NULL, returns true if the
 * executable file specified by expected_exec_path is identical to the image
 * file currently that is currently running under this PID.
 *
 * \retval ret_actual_exec_path If non-NULL, returns the actual execution path.
 * If the process is not running, returns NULL.
 *
 * \return Zero for success, nonzero for failure.  Specifically, 
 * lc_err_file_not_found  if the expected_exec_path provided does not exist.
 */
int lc_platform_proc_pid_is_running(pid_t pid, const char *expected_exec_path,
                                    tbool *ret_is_running,
                                    tbool *ret_is_expected_executable,
                                    tstring **ret_actual_exec_path);

/* ------------------------------------------------------------------------- */
/** Platform-specific function to return the command line parameters of a
 * process.  The first element is the executable path by which the process
 * was invoked.  Returns an empty array if the process is not running.
 *
 * \param pid Process ID of the process to find.
 *
 * \retval ret_cmdline The full command line used when the process
 * was executed.  Note that this may change whenever a process calls exec().
 */
int lc_platform_proc_pid_get_cmdline(pid_t pid, tstr_array **ret_cmdline);

/* ------------------------------------------------------------------------- */
/** Platform-specific function to return the parent process id of any process.
 *
 * \param pid the process id of the (child) process to be looked up
 *
 * \retval ret_ppid pointer to a pid_t variable to receive the parent pid.
 *  Returns (*ret_ppid == -1) if the child pid process does not exist.
 */
int lc_platform_get_parent_pid(pid_t pid, pid_t *ret_ppid);

/* ------------------------------------------------------------------------- */
/** Linux platform-specific function to return a field substring from the
 *  file /proc/{pid}/stat for the specified process ID (pid)
 *
 * \param pid the process id of the process to be looked up
 *
 * \param field_idx index of the desired /proc/{pid}/stat field where 0 is
 * the index for the first (leftmost) field.
 *
 * \retval ret_field_ts address of a tstring pointer variable to receive a 
 * new tstring with the specified field.  The caller is reponsible for freeing
 * the tstring.  If the field cannot be returned, either because the the pid
 * file does not exist (i.e. the pid is not running), or if the index is out of
 * range (i.e. there is no data at the specified index position), then NULL is
 * returned, and there is nothing to free.
 */
int 
lc_platform_linux_get_proc_stat_field(pid_t pid, uint16 field_idx,
                                      tstring **ret_field_ts);

/* ------------------------------------------------------------------------- */
/** @name Child completion handling.
 *
 * The following are for the child completion handling functions.  These
 * allow for a centralized way for processes to track a set of children,
 * and to have a callback function called when the child exits.
 */
/*@{*/
typedef struct lc_child_completion_handle lc_child_completion_handle;

typedef int (*lc_child_completion_callback_func)
     (lc_child_completion_handle *cmpl_hand,
      pid_t pid, 
      int wait_status,
      tbool completed,
      void *arg);

int lc_child_completion_init(lc_child_completion_handle **cmpl_hand);

int lc_child_completion_deinit(lc_child_completion_handle **cmpl_hand);

int lc_child_completion_register(lc_child_completion_handle *cmpl_hand,
                                 pid_t pid,
                                 lc_child_completion_callback_func func, 
                                 void *arg);

/*
 * Note that there is no way to unregister a child completion handler.
 * Be careful about adding one, since if a child completion handler
 * were to delete itself (or any other child completion handlers),
 * lc_child_completion_handle_completion() could get confused when it
 * comes time to delete the handler it just called.
 */

int lc_child_completion_handle_completion(lc_child_completion_handle 
                                          *cmpl_hand,
                                          pid_t pid, 
                                          int wait_status,
                                          tbool was_reaped);
/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* __PROC_UTILS_H_ */
