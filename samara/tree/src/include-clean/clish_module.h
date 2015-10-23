/*
 *
 * clish_module.h
 *
 *
 *
 */

#ifndef __CLISH_MODULE_H_
#define __CLISH_MODULE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <sys/time.h>
#include <signal.h>
#include "common.h"
#include "ttime.h"
#include "tstr_array.h"
#include "bnode.h"
#include "cli_module.h"

/* ========================================================================= */
/** \file clish_module.h CLI shell module API
 * \ingroup cli
 *
 * Functions and declarations provided by the CLI shell for use by
 * modules.
 *
 * Empty stubs of these functions are also available in libclish.
 * Libcli clients other than the CLI shell should link with libclish
 * so that modules who use these APIs do not cause link errors while
 * loading.
 *
 * CAUTION: a module that calls any of the APIs in this header file
 * should probably be putting the ccf_shell_required flag on the 
 * registration for the command(s) that trigger the calls.  Please
 * be aware that the base platform does expose at least two ways the
 * user can run any CLI command through a non-shell libcli client: 
 * (a) the job scheduler, and (b) the Web UI's "Execute CLI Commands"
 * form on the Configurations page.  Commands which do not use the
 * ccf_shell_required, yet call APIs from this header, risk seeming
 * available in those non-shell contexts, but failing when run.
 */

#ifndef clish_max_word_len
#define clish_max_word_len           16
#endif

#define clish_help_padding            1
#define clish_standard_screen_width  80


/* ------------------------------------------------------------------------- */
/** Tell the CLI shell that it's time to quit.  This can be triggered
 * either because the user requested it or due to some other event.
 * This function just takes note of the request and returns, so the
 * caller (generally a module's execution function) can finish what it
 * is doing.  When the module returns, the shell will deinitialize
 * everything gracefully and exit.
 */
int clish_initiate_exit(void);

/* ------------------------------------------------------------------------- */
/** Tell the CLI shell we are restarting the system.  This advises it
 * not to get alarmed when the present command being executed is
 * cancelled, as otherwise it logs an advisory about command
 * cancellation.
 */
int clish_prepare_for_restart(void);

/* ------------------------------------------------------------------------- */
/** Tell the CLI shell we are about to exec another binary over the
 * CLI.  The cleanup to be done here includes flushing the history to
 * persistent storage, and closing GCL sessions so the socket files
 * don't get leftover.
 */
int clish_prepare_to_exec(void);

/* ----------------------------------------------------------------------------
 * Set the current inactivity timeout.
 * \param dur_sec New inactivity timeout, in seconds.
 */
int clish_autologout_set(lt_dur_sec dur_sec);

/* ----------------------------------------------------------------------------
 * Get the current inactivity timeout.
 * \retval ret_dur_sec Current inactivity timeout, in seconds.
 */
int clish_autologout_get(lt_dur_sec *ret_dur_sec);

/* ------------------------------------------------------------------------- */
/** Begin extended command processing.  Sometimes a command wants
 * to return from its processing function because we need to return to
 * the event loop, but it is not done with the command.  An example is
 * the ping command, which forks a process and wants to listen for its
 * output, but does not want to return to the command prompt yet.
 *
 * Effectively what this does is disable printing of the command
 * prompt, and take STDIN_FILENO off the list of file descriptors
 * to watch.  It also means the shell will not respond to SIGINT,
 * so the caller should probably use the shell's event notification
 * mechanisms (clish_event_callback_register() et al.) to register
 * to hear about SIGINT.  The exception to this would be if you are
 * launching another program which will take over the TTY (as in the
 * case of clish_run_program_fg()); there, the other program will
 * receive the SIGINT and the CLI shell will not.
 * 
 * If a module begins extended command processing, it must have at
 * least one event notification callback registered in order to regain
 * control to end extended command processing; otherwise the CLI will
 * be stuck in that state forever.
 */
int clish_extended_processing_begin(void);

/* ------------------------------------------------------------------------- */
/**
 * End extended command processing.  See clish_extended_processing_begin()
 * for further details.
 */
int clish_extended_processing_end(void);

/* ------------------------------------------------------------------------- */
/** For modules that want to print things asynchronously, such as in
 * response to an event, when it is not their turn (e.g. not during
 * command processing).  The module should call clish_async_print_begin() 
 * before printing anything, and clish_async_print_end() when finished.
 *
 * XXX/EMT: NOTE: this does not work in all cases, use with caution.
 * See bug 11383.
 */
int clish_async_print_begin(void);

/* ------------------------------------------------------------------------- */
/** Terminate out-of-turn printing initiated by clish_async_print_begin().
 */
int clish_async_print_end(void);


/* ------------------------------------------------------------------------- */
/** Are we running in the CLI shell or not?  The CLI shell's
 * implementation of this function returns true; libclish's returns
 * false.  This is mainly used by libcli to determine whether or not
 * to accept registrations of commands with the ccf_shell_required
 * flag set.
 */
tbool clish_is_shell(void);


/* ------------------------------------------------------------------------- */
/** Are we in the middle of initializing, immediately after being
 * freshly launched?  This will return 'true' up until the CLI shell
 * enters its main loop for the first time, and then 'false' thereafter.
 * This is useful, say, for modules who want to do something when the
 * CLI first starts up, but do not want to repeat it whenever they
 * are reinitialized (e.g. because we lost our mgmtd session).
 */
tbool clish_is_initializing(void);


/* ------------------------------------------------------------------------- */
/** Are we running the CLI in a normal interactive mode?  This requires
 * that clish_is_shell() and gl_is_terminal() both return true.
 * The reason clish_is_shell() is not sufficient is that it can be true
 * when we are running with certain command-line options (e.g. "-d"),
 * or when input is being piped into us.
 */
tbool clish_is_interactive(void);


/* ------------------------------------------------------------------------- */
/** @name Running external programs.
 * Functions and definitions to help CLI modules which want to run
 * external programs and print their output.
 */
/*@{*/

/* ------------------------------------------------------------------------- */
/** A handler to be called when output becomes available from a process
 * run using clish_run_program().
 *
 * \param fd The file descriptor on which activity has been detected.
 * It is guaranteed to have already been set as nonblocking.  The
 * function should read whatever is available from this file
 * descriptor and then perform whatever action is desired.
 *
 * \param data The pointer passed to clish_run_program() for
 * output_handler_data.
 */
typedef int (*clish_output_handler_func)(int fd, void *data);

/* ------------------------------------------------------------------------- */
/** A handler to be called after a program launched by
 * clish_run_program() has terminated.
 *
 * \param data The pointer passed to clish_run_program() for
 * termination_handler_data.
 */
typedef int (*clish_termination_handler_func)(void *data);

/* ------------------------------------------------------------------------- */
/** Fork a process and run the specified command, printing its output
 * into the CLI session.
 *
 * This is meant to be called by a CLI command execution function
 * which wants to run an external program and print that program's
 * output.  The command will finish either when the program runs to
 * completion, or when the user hits Ctrl+C.
 *
 * This function uses lc_launch() to fork and exec the process, and
 * requests that pipes be created for its stdout and stderr.  It then
 * registers callbacks for those fds with the CLI shell framework and
 * calls clish_extended_processing_begin() so the prompt doesn't come
 * back.  It can call a custom handler for printing the output,
 * generally to be used if output filtering is desired, or it can
 * print the output directly.  It also registers with the CLI shell to
 * be informed when the process terminates, so it can clean up and
 * call clish_extended_processing_end() to declare the command
 * execution over and get back to the prompt.  Finally, it can call
 * a handler provided by the module at the very end.
 *
 * The module should call this function, which will return
 * immediately, and then return from its execution function.  Because
 * of what this function does, the command will not be considered over
 * until after the process terminates and the termination handler
 * provided is called.
 *
 * \param abs_path An absolute path to the binary to be run.  A copy is
 * made so ownership of the path passed is not taken by the function.
 * Relative paths are not permitted.
 *
 * \param argv The argv array for the process to be run.  Note that
 * argv[0] should usually be the name of the binary itself (although
 * sometimes it needs to be a different name in the case of programs
 * that act differently depending on how they were invoked).  A copy
 * is made so ownership is not taken by the function.
 *
 * \param working_dir The working directory to run the program in,
 * if NULL is passed, the value of the environment variable HOME is used.
 *
 * \param output_handler A function to be called whenever output from
 * the launched program becomes available on either stdout or stderr.
 * If NULL is passed, the default output handler will be used, which
 * just prints the output to the CLI session.
 *
 * \param output_handler_data A pointer to be passed back to the
 * output handler whenever it is called.  If the output handler is
 * NULL, this must also be NULL.
 *
 * \param termination_handler A function to be called after the
 * launched process has terminated, right before control is returned
 * to the framework and the prompt reprinted.  If NULL, no function
 * will be called.
 *
 * \param termination_handler_data A pointer to be passed back to the
 * termination handler when it is called.  If the termination handler
 * is NULL, this must also be NULL.
 *
 * NOTE: if the program being run prints to both stdout and stderr, the
 * text from the two may be interleaved incorrectly when printed to
 * the CLI session, since they are sent on two independent pipes.
 * (This requires a fix in proc_utils.)
 *
 * NOTE: if you are going to call cli_printf() or any of its variants
 * to print output from the same command execution as calls this
 * function (either before or after calling it), you MUST call
 * clish_paging_suspend() BEFORE calling this function.
 */
int clish_run_program(const char *abs_path, const tstr_array *argv, 
                      const char *working_dir,
                      clish_output_handler_func output_handler,
                      void *output_handler_data,
                      clish_termination_handler_func termination_handler,
                      void *termination_handler_data);


/* ------------------------------------------------------------------------- */
/** Like clish_run_program(), except that instead of creating pipes to send
 * the program's output back to the CLI, assigns the TTY so the program
 * can write to it (and read from it) directly.  The TTY is reclaimed 
 * after the program exits.
 *
 * NOTE: if you are going to call cli_printf() or any of its variants
 * to print output from the same command execution as calls this
 * function (either before or after calling it), you MUST call
 * clish_paging_suspend() BEFORE calling this function.
 */
int clish_run_program_fg(const char *abs_path, const tstr_array *argv, 
                         const char *working_dir,
                         clish_termination_handler_func termination_handler,
                         void *termination_handler_data);

/* ------------------------------------------------------------------------- */
/** Flags to be passed to clish_run_program_fg_ex().
 */
typedef enum {
    crpf_none =          0,

    /**
     * Instead of setting the SHELL variable to /bin/sh as normal,
     * set it to /sbin/nologin.
     */
    crpf_no_shell = 1 << 0,

    /**
     * Allow launching of a program with a relative path, where the
     * directories in the PATH env var will be searched.  The default is
     * to only allow an absolute path to be specified.
     */
    crpf_allow_relative_path = 1 << 1,

    /**
     * Do not call clish_extended_processing_end() before calling the
     * termination handler.  If you use this flag, it is up to your
     * termination handler to call that whenever it is done executing
     * the command.  This allows the flexibility to run another
     * program before finishing, if desired.
     */
    crpf_leave_extended_processing = 1 << 2,

} clish_run_program_flags;

/** Bit field of ::clish_run_program_flags ORed together */
typedef uint32 clish_run_program_flags_bf;


/* ------------------------------------------------------------------------- */
/** Like clish_run_program(), except that it takes an additional argument
 * with flags.
 */
int clish_run_program_ex(const char *abs_path, const tstr_array *argv, 
                         const char *working_dir,
                         clish_output_handler_func output_handler,
                         void *output_handler_data,
                         clish_termination_handler_func termination_handler,
                         void *termination_handler_data,
                         clish_run_program_flags_bf flags);


/* ------------------------------------------------------------------------- */
/** Like clish_run_program_fg(), except that it takes an additional argument
 * with flags.
 */
int
clish_run_program_fg_ex(const char *path, const tstr_array *argv,
                        const char *working_dir,
                        clish_termination_handler_func termination_handler,
                        void *termination_handler_data,
                        clish_run_program_flags_bf flags);


/*@}*/


/* ------------------------------------------------------------------------- */
/** Register a string to be available for substitution into command
 * prompts using the $n$ mechanism described in clish_prompt_push().
 * The prompts are constructed using these strings:
 *
 *   Standard mode: "$0$$1$$2$$3$$4$$5$$6$$P$ > "
 *   Enable mode:   "$0$$1$$2$$3$$4$$5$$6$$P$ # "
 *   Config mode:   "$0$$1$$2$$3$$4$$5$$6$ ($7$config$8$$p$$9$) # "
 *
 * Per the definitions (clish_prompt_subst_hostname and
 * clish_prompt_subst_cluster) in climod_common.h, the numbers {1, 4, 5}
 * are reserved for use by the infrastructure.  Thus you may use any
 * of the other numbers; choose which one based on where you want your
 * text to show up relative to the other components of the prompt.
 * The basic ranges are:
 *   0:    before everything
 *   1:    RESERVED
 *   2-3:  between hostname and other infrastructure text
 *   4-5:  RESERVED
 *   6:    after everything (in all prompts)
 *   7:    before "config" in config mode prompt
 *   8:    between "config" and prefix mode words in config mode prompt
 *   9:    after everything in parentheses in config mode prompt
 *
 * Note that $p$ is automatically substituted out for a concatenation
 * of all words on the prefix stack, separated by spaces.  Its behavior
 * cannot be overridden.  $P$ is the same, except that it puts this
 * string in parentheses.
 *
 * XXX/EMT: if bug 12649 (prefixes not reflected in prompt in enable mode)
 * is fixed, we may have a policy that if any of 7, 8, 9, and 'p'
 * is populated with any text, we'll have parentheses, and fill in those
 * values just as we do for config mode, except without the word "config".
 *
 * \param idx Identifier for this string.  This is the number to be
 * placed inside the '$' characters in the prompt string.  The numbers
 * used should not be too sparse, as they are used as indices into a
 * string array, but need not be contiguous.
 *
 * \param str The string to register for substitution.  The CLI does
 * not take ownership of the pointer you provide.
 *
 * \param redisplay If \c true the prompt will be redisplayed after
 * the change is made.  If \c false the new string will be registered
 * and will be rendered the next time the prompt is normally
 * redisplayed (e.g. a command is entered and the CLI re-prompts for
 * a new line).
 */
int clish_prompt_subst_register(uint32 idx, const char *str, tbool redisplay);


/* ------------------------------------------------------------------------- */
/** Push a string onto the prompt stack.  This will replace the current
 * CLI prompt immediately, but the current one will be saved on the 
 * stack for later restoration in LIFO order.
 *
 * \param prompt The new prompt string.  A copy of this string is made
 * so ownership is not taken.
 *
 * \param redisplay If \c true the prompt will be redisplayed after
* the change is made.  
 *
 * If at least one prompt is not pushed onto the stack during module
 * initialization, the CLI shell will provide a generic prompt
 * automatically.
 *
 * The prompt string may contain sequences of the form $n$ where n is
 * a nonnegative integer.  This will dynamically substitute whatever
 * string was registered under the number n using
 * clish_prompt_subst_register().  The important aspect of this
 * substitution that cannot be easily duplicated by the caller when
 * providing the prompts is that if a referenced string is changed
 * after a prompt is pushed, that prompt is updated automatically
 * instead of having to be removed from the stack and replaced.
 *
 * The prompt string may also contain $p$, which will be substituted
 * out for a string containing all of the prefixes on the prefix
 * stack, separated by spaces.  This is used to indicate which prefix
 * mode the user is in.  Similarly, $P$ will do the same except the
 * string being added will be in parentheses.
 */
int clish_prompt_push(const char *prompt, tbool redisplay);


/* ------------------------------------------------------------------------- */
/** Pop a string off of the prompt stack.  The string is not returned,
 * as the caller generally will not need to know what it was.  The
 * prompt is immediately set to whatever prompt was left on the stack
 * beneath.  If this was the last prompt on the stack, the pop will
 * fail.
 *
 * \param redisplay If \c true the prompt will be redisplayed after
 * the change is made.  
 */
int clish_prompt_pop(tbool redisplay);


/* ------------------------------------------------------------------------- */
/** Recalculate and reinstall the prompt, and optionally force it to
 * be redisplayed.
 */
int
clish_prompt_update(tbool redisplay);


/* ========================================================================= */
/** \name Event notification
 *
 * This section contains functions to allow modules to register to be
 * called back when certain events occur.
 */
/*@{*/

/* ------------------------------------------------------------------------- */
/** Type of event.
 *
 * Used by modules to indicate what kind of event(s) they are
 * registering for.  They are all mutually exclusive, except for
 * cet_timeout, which can be combined with any other event type, and
 * cet_fd_readable/cet_fd_writable, which can also be combined with
 * each other.
 *
 * Also used by CLI shell to tell the callback what kind of event has
 * occurred.  In this case only a single bit will be set.
 *
 * Note: SIGCHLD has its own event type because a child process can
 * only be reaped once and you need to reap it to know who is
 * interested in it.  Thus the CLI shell reaps the process and calls
 * anyone interested in that pid.
 */
typedef enum {
    cet_none =            0,
    cet_timeout =         1 << 0,
    cet_fd_readable =     1 << 1,
    cet_fd_writable =     1 << 2,
    cet_signal =          1 << 3,
    cet_sigchld =         1 << 4
} clish_event_type;


/* ------------------------------------------------------------------------- */
/** Signals handled by the CLI.  Modules may not register to be
 * notified of signals via cet_signal for any signals not listed here.
 * Note also that it is an error to register to SIGCHLD using
 * cet_signal.
 */
static const int clish_signals_handled[] = {
    SIGHUP, SIGINT, SIGPIPE, SIGTERM, SIGQUIT, SIGUSR1, SIGUSR2, SIGCHLD,
    SIGCONT, SIGWINCH
};


/* ------------------------------------------------------------------------- */
/** Information about an event that has occurred.  An instance of this
 * structure is passed from the shell to the module's event handler
 * whenever an event occurs.
 */ 
typedef struct clish_event_instance {
    /* Basic fields present for all events */
    clish_event_type cei_type;
    void *cei_data;

    /* Event-specific fields */
    int cei_fd;             /* if event type is cet_fd_*                   */
    int cei_signum;         /* if event type is cet_signal                 */
    int cei_pid;            /* if event type is cet_sigchld                */
    int cei_status_code;    /* if event type is cet_sigchld                */
} clish_event_instance;


/* ------------------------------------------------------------------------- */
/** Function to be called when an event occurs.  A pointer to an event
 * instance is provided; fields are selectively filled in according to
 * the event type.  The callback is not responsible for freeing the
 * space allocated to hold the event instance structure.
 *
 * The return value of the function determines the future fate of the
 * event registration.  If the function returns lc_err_no_error:
 *
 *    - If the event was a timeout, the event persists and the timer 
 *      is reset; i.e. it will go off again when the same amount of
 *      time elapses again.
 *
 *    - If the event was a SIGCHLD, the event is removed.  This is the
 *      assumed preferred behavior as the pid it was registered on no
 *      longer exists.
 *
 *    - Otherwise, the event persists.  If the event registration had an
 *      associated timeout, the timer is not reset.
 *
 * If the function returns lc_err_reset_timeout:
 *
 *    - If the event was a timeout, the behavior is the same as with
 *      lc_err_no_error.
 *
 *    - If the event was a SIGCHLD, this is not a valid return value, so
 *      it is treated as an error.
 *
 *    - Otherwise, if the event registration had an associated timeout,
 *      the timer is reset.  If it did not have an associated timeout,
 *      the registration persists and no error is raised.
 *
 * If the function returns lc_err_event_delete:
 *
 *    - The event record is deleted and will not recur, regardless of
 *      the event type.
 */ 
typedef int (*clish_event_callback_func)(const clish_event_instance *event);


/* ------------------------------------------------------------------------- */
/** Event registration request.  An instance of this structure is
 * passed from the module to the shell when an event callback is
 * registered.  Note that certain fields are only appropriate to
 * certain event types, and will be ignored if not appropriate.
 */
typedef struct clish_event_registration {
    /* Basic fields present for all registrations */
    clish_event_type cer_type;
    clish_event_callback_func cer_callback;
    void *cer_data;

    /* Event-specific fields */
    lt_dur_ms cer_timeout_ms;  /* if event type is cet_timeout              */
    int cer_fd;                /* if event type is cet_fd_*                 */
    int cer_signum;            /* if event type is cet_signal               */
    pid_t cer_pid;             /* if event type is cet_sigchld              */

    /* Opaque, for framework use only, do not use */
    void *cer_opaque;
} clish_event_registration;


/* ------------------------------------------------------------------------- */
/** Create an empty event notification registration structure.
 */
int clish_event_callback_new(clish_event_registration **ret_reg);


/* ------------------------------------------------------------------------- */
/** Register to be notified when an event occurs.
 *
 * Note: registering for read activity on STDIN_FILENO is a special
 * case because the CLI shell was already listening to that itself.
 * This causes the CLI shell to unregister its own callback, which is
 * then restored when the module unregisters its callback.
 *
 * Ownership of the event registration structure is assumed by the
 * framework.  XXX/EMT: framework should zero the pointer, so caller knows
 * to free it if there's an error.  Should also offer a function to
 * free an event registration.
 */
int clish_event_callback_register(clish_event_registration *reg);


/* ----------------------------------------------------------------------------
 * Reset the timer on a timeout event previously registered.
 * 
 * NOT YET IMPLEMENTED.
 */
#if 0
int clish_event_timer_reset(clish_event_registration *reg);
#endif


/* ------------------------------------------------------------------------- */
/** Cancel an event callback registration.  Note that callbacks can
 * also unregister themselves when they are called; this API is
 * provided for cases where some other part of the code needs to
 * unregister the callback without waiting for the event to occur.
 *
 * The caller must pass in the same pointer to an event registration
 * structure as was originally passed on registration.  The structure
 * is freed and the pointer set to NULL by this function.
 *
 * NOTE: a signal or sigchld handler (with or without a timer) may not
 * unregister another signal or sigchld handler.  This is because when
 * we call these handlers, we iterate over the registrations using
 * array_foreach().
 */ 
int clish_event_callback_unregister(clish_event_registration **inout_reg);

typedef int (*clish_config_change_callback_func)
     (const bn_binding_array *old_bindings,
      const bn_binding_array *new_bindings, void *data);

/* ------------------------------------------------------------------------- */
/** Register for notification of ALL configuration changes.
 *
 * NOTE: this API may only be called during CLI initialization.
 * Calling it after your initialization function has returned
 * will result in an error.
 *
 * NOTE: for performance reasons, modules should NOT use
 * clish_config_change_notif_register(), and should only call the
 * newer clish_config_change_notif_register_va().  If even one module
 * calls clish_config_change_notif_register(), the CLI will have to
 * refrain from installing any filters on dbchange notifications,
 * which can be a performance drain.  If all callers use
 * clish_config_change_notif_register_va(), we install filters so we
 * only receive the union of all subtrees/patterns registered by modules.
 *
 * \param callback_func Function to be called if any configuration changes.
 *
 * \param callback_data Data to be passed to callback_func whenever it
 * is called.
 */
int clish_config_change_notif_register
    (clish_config_change_callback_func callback_func, void *callback_data);

/**
 * Register for notification for certain configuration changes.
 * This is strongly preferred over clish_config_change_notif_register()
 * for reasons explained above.
 *
 * NOTE: this API may only be called during CLI initialization.
 * Calling it after your initialization function has returned
 * will result in an error.
 *
 * \param callback_func Function to be called with configuration changes,
 * at least whenever nodes matching any of the registered node patterns
 * has changed.  NOTE: you may also be called with additional changes
 * that do not match your pattern.  You will be called anytime a 
 * configuration change includes at least one node matching any of the
 * patterns registered by any module; and the arrays will include all
 * of the bindings in that change, even those which don't match any
 * registered patterns.
 *
 * \param callback_data Data to be passed to callback_func whenever it
 * is called.
 *
 * \param num_patterns Number of node name pattern strings to follow.
 * Must be at least 1.
 *
 * \param pattern1 First node name pattern.
 */
int clish_config_change_notif_register_va
    (clish_config_change_callback_func callback_func, void *callback_data,
     uint32 num_patterns, const char *pattern1, ...);


typedef int (*clish_action_callback_func)
     (const char *action_name, const bn_binding_array *bindings, void *data);

/* ------------------------------------------------------------------------- */
/** Register for notification of management actions.  (NOT YET IMPL)
 */
int clish_mgmt_action_notif_register
    (clish_action_callback_func callback_func, void *data);

typedef int (*clish_mgmt_event_callback_func)
     (const char *event_name, const bn_binding_array *bindings, void *data);

/* ------------------------------------------------------------------------- */
/** Register for notification of management events.
 */
int clish_mgmt_event_notif_register
    (const char *event_name, clish_mgmt_event_callback_func callback_func,
     void *data);

int clish_mgmt_event_notif_unregister
    (const char *event_name, clish_mgmt_event_callback_func callback_func);

/*@}*/


/* ------------------------------------------------------------------------- */
/** This is a runtime switch modules can use to enable or disable paging,
 * presumably according to a user preference.
 */
int clish_paging_set_enabled(tbool enabled);

/* ------------------------------------------------------------------------- */
/** This is a runtime query for whether or not paging is currently enabled.
 */
tbool clish_paging_is_enabled(void);

/* ------------------------------------------------------------------------- */
/** This may be called from a module's execution function to cause
 * paging of output to be suspended only for the duration of that
 * command.  It is intended to be used for commands that for some
 * reason or another don't want their own output to be paged.
 * Generally the reason would be that (a) they run for a long time,
 * (b) they generate output incrementally rather than all at the
 * end, and (c) they do not generate enough output that it would
 * generally need to be paged.  Or, they are going to be calling
 * clish_run_program_fg_ex() repeatedly, while printing stuff in
 * between, and they want the output to be interleaved correctly.
 *
 * You do not need to reenable paging later.  It will be reenabled 
 * automatically when your command is finished.
 */
int clish_paging_suspend(void);

/* ------------------------------------------------------------------------- */
/** This is a runtime switch modules can use to enable or disable
 * progress updates, presumably according to a user preference.
 */
int clish_progress_set_enabled(tbool enabled);

/* ------------------------------------------------------------------------- */
/** This is a runtime query for whether or not progress updates are
 * currently enabled.
 */
tbool clish_progress_is_enabled(void);

/* ------------------------------------------------------------------------- */
/** This is a runtime query for whether or not progress updates are
 * displayable on the CLI console or not (for e.g. not if CLI is not
 * in shell mode).
 */
tbool clish_progress_is_displayable(void);

/* ------------------------------------------------------------------------- */
/** @name Terminal size control
 *
 * Force the terminal to a specified number of lines or columns,
 * or cause it to be auto-detected.
 * Note: this will only last until the next time a SIGWINCH is received,
 * at which point it will be automatically determined.
 */
/*@{*/

/** Automatically resize the terminal dimensions.
 *
 * \param redisplay -  boolean flag to control whether to redisplay screen
 *                     or not after resizing.
 * \param force_flag - Whether to forcibly get the dimensions directly from
 *                     the terminal or not. If FALSE, gl_resize_terminal will
 *                     be executed first and in case of error, will fallback
 *                     to read directly from terminal. (Check the function
 *                     description for lc_tty_size_get before using this
 *                     function).
 */
int clish_terminal_auto_resize(tbool redisplay, tbool force_flag);

int clish_terminal_force_num_lines(uint32 num_lines);
int clish_terminal_force_num_columns(uint32 num_columns);
int clish_terminal_get_size(uint32 *ret_num_lines, uint32 *ret_num_cols);
int clish_terminal_set_type(const char *type);
/*@}*/

/* ------------------------------------------------------------------------- */
/** Indicate what the maximum cli command size can be. 
 */
int clish_get_max_line_size(uint32 *max_line_size);

/* ------------------------------------------------------------------------- */
/** Load the command history from disk for the current user.
 */
int clish_history_load(void);

/** Save the command history from disk for the current user, not
 * including the currently-executing command, if any.
 */
int clish_history_save(void);

/** Write the currently-executing command to the history (without any
 * of its parameters being obscured), and save the history.  This is
 * useful in cases where the command may not finished executing,
 * because we are exec'ing something on top of the CLI, such as the
 * bash shell or the Wizard.
 */
int clish_history_flush(void);

/**
 * Signal that the CLI should not save the history file to disk.
 * This is a fairly specialized API, intended to be used when the 
 * user account no longer exists.
 */
int clish_history_no_save(void);


/* ------------------------------------------------------------------------- */
/** Purge the history records, both in memory and on disk, for the
 * current user.
 */
int clish_history_clear(void);


/* ========================================================================= */
/** \name Prompting for interactive input, part 1.
 */
/*@{*/

typedef int (*clish_wait_for_input_func)(const char *answer, void *data);

/* ------------------------------------------------------------------------- */
/** Wait for the user to enter a string and hit Enter.  Then call back
 * the specified function with the answer, and the specified parameter.
 * If 'obscure_answer' is set, '*' is printed in lieu of each character
 * entered, suitable for entering passwords.
 *
 * Your function will be called back with the answer the user entered,
 * and the 'void *' you provided to this function.  If the user hits
 * Ctrl+C, NULL will be passed for the answer.  Generally you should
 * interpret this as a request to cancel whatever command was being
 * executed.
 *
 * NOTE: the caller must call clish_extended_processing_begin() before
 * calling this function, and clish_extended_processing_end() from their
 * answer handling callback after they are satisfied with the answer.
 * This cannot be done automatically by clish_wait_for_input() because
 * you might want to call it back again if you don't like the first
 * answer the user gives, but clish_extended_processing_end() should
 * only be called once during the execution of a command.
 *
 * NOTE: if you are going to print anything before calling this
 * function (which you generally should so the user knows what you're
 * waiting for!), be sure to call clish_paging_suspend() before doing so.
 * That will suspend paging just for the duration of this command;
 * it will be automatically reenabled (if it was enabled before) when 
 * your command finishes.
 */
int clish_wait_for_input(tbool obscure_answer, 
                         clish_wait_for_input_func func, void *data);

/* ------------------------------------------------------------------------- */
/** Indicates the result of a call to clish_prompt_for_password().
 */
typedef enum {
    cpr_none = 0,

    /** 
     * We successfully got a password, confirmed by the user if this
     * was requested by the caller.
     */
    cpr_ok,

    /**
     * The user cancelled password entry with Ctrl+C, so no password
     * was entered.
     */
    cpr_cancelled,

    /**
     * Confirmation was requested, and it failed (the user's two entries
     * did not match).
     */
    cpr_confirm_failed,
} clish_password_result;

/* ------------------------------------------------------------------------- */
/** Function to be called when the password requested from
 * clish_prompt_for_password() has been supplied, and optionally
 * confirmed.  The ret_continue field will be treated as false if not
 * set, but if set to true, we will not terminate extended processing
 * when this function returns.  This is useful if you are trying to
 * string together multiple password prompts in the same command
 * invocation.
 */
typedef int (*clish_prompt_for_password_func)(const char *password,
                                              clish_password_result result,
                                              const cli_command *cmd,
                                              const tstr_array *cmd_line,
                                              const tstr_array *params,
                                              void *data, tbool *ret_continue);

/* ------------------------------------------------------------------------- */
/** This is a helper function built on top of clish_wait_for_input()
 * that is specialized for one purpose: to prompt the user for
 * passwords that should not be displayed in the clear.  It prompts
 * the user for a password, obscuring input (echoing all characters 
 * as '*').  It then may optionally ask a second time for confirmation.
 * The provided callback is called in all cases, regardless of the
 * result of these prompts, but it will be given parameters to indicate
 * whether the user pressed Ctrl+C, failed confirmation of the password,
 * or successfully entered a password.
 *
 * We make a few assumptions to save the caller work in the common
 * case.  One is that the caller will not be doing any other extended
 * processing before or after calling us.  We take care of calling
 * clish_extended_processing_begin() and clish_extended_processing_end()
 * at the appropriate times, based on this assumption.  The other is
 * that you will not be printing so much output that paging will be
 * needed, so we call clish_paging_suspend() automatically as well.
 *
 * It is expected that generally, confirmation will be used when
 * prompting for a password to set on the local system, since the
 * consequences of getting it wrong could be great inconvenience.
 * But when prompting for a password to log into a remote system,
 * confirmation is not as important, since one can usually just
 * try again.
 *
 * This function does not block on the input; it returns immediately,
 * and then your execution function should return.  Your callback will
 * always be called after the prompting is finished, to give you an
 * opportunity to free any memory associated with the 'data'
 * parameter.  Make sure that you do not pass any pointers to local
 * variables in your original execution function, since it will be
 * out of scope by the time your callback is called.
 *
 * \param need_setup Generally you should pass true here. If you were
 * trying to string multiple password prompts together by calling this
 * function again from the callback called by the first invocation,
 * you would pass false here from the second (inner) invocation.
 *
 * \param prompt What string should we use to prompt the user?
 * If not specified, we use "Password: ".
 *
 * \param confirm Specifies whether or not we should prompt a second
 * time for confirmation of the password.
 *
 * \param func Function to be called if the user enters two matching
 * passwords.  Note that if the two passwords entered do not match,
 * this function will not be called; a generic error message will be
 * printed, and the command terminated.
 *
 * \param cmd CLI command structure, to be passed back to func.
 *
 * \param cmd_line Command line array, to be passed back to func.
 *
 * \param params Parameter array, to be passed back to func.
 *
 * \param data Generic void *, to be passed back to data.
 */
int clish_prompt_for_password(tbool need_setup, const char *prompt,
                              tbool confirm,
                              clish_prompt_for_password_func func,
                              const cli_command *cmd,
                              const tstr_array *cmd_line,
                              const tstr_array *params, void *data);

/**
 * Like clish_prompt_for_password() except lets you specify whether or
 * not to obscure input.
 */
int
clish_prompt_for_input(tbool need_setup, const char *prompt,
                       tbool confirm, tbool obscure,
                       clish_prompt_for_password_func func,
                       const cli_command *cmd,
                       const tstr_array *cmd_line,
                       const tstr_array *params, void *data);

/* ------------------------------------------------------------------------- */
/** Check the provided URL to see if it is one that can accept a
 * password (currently only those beginning with "scp://"), and if so,
 * check if it specifies one.  If not, and if the CLI is configured to
 * prompt for missing passwords, then do so.  Otherwise, just call the
 * handler directly.  This is just a helper function build on top of
 * clish_prompt_for_password(), for the convenience of the many modules
 * that need to accept passwords in URLs.
 */
int clish_maybe_prompt_for_password_url(const char *remote_url, 
                                        clish_prompt_for_password_func func,
                                        const cli_command *cmd,
                                        const tstr_array *cmd_line,
                                        const tstr_array *params, void *data);


/* ------------------------------------------------------------------------- */
/** A structure containing data to be passed back to
 * clish_std_password_handle_answer().
 */
typedef struct clish_std_password_handle_answer_data {
    /**
     * Name of the binding into which the password should be set.
     */
    char *cphd_bname;

    /**
     * Data type of the binding specified by cphd_bname.
     * Defaults to ::bt_string.
     */
    bn_type cphd_btype;

} clish_std_password_handle_answer_data;


/* ------------------------------------------------------------------------- */
/** Allocate a new ::clish_std_password_handle_answer_data structure,
 * and initialize its fields to their defaults.
 */
int clish_std_password_handle_answer_data_create
    (clish_std_password_handle_answer_data **ret_cphd);
                                               

/* ------------------------------------------------------------------------- */
/** Free the contents of a ::clish_std_password_handle_answer_data
 * structure, and the memory allocated to hold the structure itself.
 * Note that CLI modules should not generally need to call this, 
 * since clish_std_password_handle_answer() will free it for you
 * when it is called back.
 */
int clish_std_password_handle_answer_data_free
    (clish_std_password_handle_answer_data **inout_cphd);
                                               

/* ------------------------------------------------------------------------- */
/** A standard ::clish_prompt_for_password_func that can be used as the
 * callback parameter passed to clish_prompt_for_password() in common
 * cases that meet certain constraints.
 *
 * This function can be used if your entire response to getting a
 * password is to set a single binding with the password as the value.
 * The 'data' parameter passed to it must be a pointer to a
 * dynamically allocated ::clish_std_password_handle_answer_data
 * structure, and that will tell it how to make this set request using
 * the password entered.
 *
 * This function will free the clish_std_password_handle_answer_data
 * structure and its contents when finished, so the caller who passed
 * this function pointer to clish_prompt_for_password() needs only
 * to allocate the structure, but not free it.
 */
int clish_std_password_handle_answer(const char *password,
                                     clish_password_result result,
                                     const cli_command *cmd,
                                     const tstr_array *cmd_line,
                                     const tstr_array *params,
                                     void *data, tbool *ret_continue);

/*@}*/

/* ========================================================================= */
/** \name Prompting for interactive input, part 2.
 */
/*@{*/

/**
 * Option flags for clish_prompt_for_input_ex().
 */
typedef enum {
    cpfie_none = 0,

    /**
     * Specify that we have already prompted for input before during
     * this command execution, so the standard initialization to accept
     * input is not necessary.
     */
    cpfie_no_init_needed = 1 << 0,

    /**
     * Secure echo: echo each character as '*' instead of itself.
     * Note that this does not apply to newlines; regardless of this
     * flag, when the user enters a newline it will be echoed, and the
     * prompt_per_line will be printed to prompt for the next line.
     */
    cpfie_obscure_input = 1 << 1,

    /**
     * Accept multiple lines of input.  If this is specified:
     *
     *   - After printing the 'prompt' string given to
     *     clish_prompt_for_input_ex(), we will print a newline.
     *     Then before every line of input, including the first line,
     *     we will print a prompt "> ".
     *
     *   - Instead of the input being automatically concluded when the
     *     first newline is received, the callback function will be called
     *     with a result code of ::cpier_newline.  The function may judge
     *     whether or not input is finished at that time.
     *
     *   - Input will also be considered finished when Ctrl+D is entered
     *     on a line by itself.  At any other time, Ctrl+D is ignored.
     */
    cpfie_multiline = 1 << 2,

} clish_prompt_for_input_ex_flags;


/** Bit field of ::clish_prompt_for_input_ex_flags ORed together */
typedef uint32 clish_prompt_for_input_ex_flags_bf;


/**
 * Result codes to be passed to ::clish_prompt_for_input_ex_func.
 */
typedef enum {
    cpier_none = 0,

    /** 
     * Input has been completed successfully, and this is the entire
     * answer.
     */
    cpier_ok,

    /**
     * Input has been completed by the user pressing Ctrl+C.  No answer
     * is provided, as this is a request to abort.
     */
    cpier_cancelled,

    /**
     * Input is not yet completed.  The ::cpfi_multiline option has been
     * specified, and the user has just entered a newline.  We are being
     * called with the full input so far, and are asked to decide whether
     * the input is finished.
     */
    cpier_newline,

} clish_prompt_for_input_ex_result;


/**
 * Context structure passed to a ::clish_prompt_for_input_ex_func
 * function.  This includes some fields which pass data to the function,
 * and some fields which accept return data from the function.
 */
typedef struct clish_prompt_for_input_ex_context {

    /**
     * If the ::cpfi_multiline option was used, and this function is
     * called with the ::cpier_newline result code, this field will
     * contain just the line which was just entered, without any LF
     * character at the end.  It is therefore a subset of the string
     * passed in the 'input' parameter to the function, which will also
     * have all previous lines, and the LF character after this line.
     */
    const tstring *cpiec_this_line;    

    /**
     * If the ::cpfi_multiline option was used, and this function is called
     * with the ::cpier_newline result code, it may set this field to
     * indicate whether or not input is finished.  The default is 'false',
     * so a Ctrl+D on a line by itself would be the only way to exit
     * multi-line input.
     */
    tbool cpiec_finished_this_input;

    /**
     * If this function is called with a terminal result code,
     * ::cpier_ok or ::cpier_cancelled, it may set this field to indicate
     * whether or not it plans to issue further prompts.  The default is
     * 'false', meaning this was the last prompt.
     */
    tbool cpiec_allow_more_inputs;

} clish_prompt_for_input_ex_context;


/**
 * Callback function to be called on certain events triggered by a call
 * to clish_prompt_for_input_ex().
 */
typedef int (*clish_prompt_for_input_ex_func)
    (clish_prompt_for_input_ex_result result,
     const tstring *input,
     const cli_command *cmd, const tstr_array *cmd_line,
     const tstr_array *params, void *data,
     clish_prompt_for_input_ex_context *inout_context);


/**
 * Like clish_prompt_for_input(), except supports additional option flags,
 * including multi-line input.
 *
 * \param prompt Prompt to print before we start accepting input.
 *
 * \param flags Option flags.
 *
 * \param func Callback function to call when input is finished, and
 * possibly at other times.  See function signature for further details.
 *
 * \param cmd 'cmd' parameter from original command execution callback.
 * To be passed back to callback function 'func'.
 *
 * \param cmd_line 'cmd_line' parameter from original command execution
 * callback.  To be passed back to callback function 'func'.
 *
 * \param params 'params' parameter from original command execution
 * callback.  To be passed back to callback function 'func'.
 *
 * \param data Data to pass back to callback function 'func'.
 */
int clish_prompt_for_input_ex(const char *prompt,
                              clish_prompt_for_input_ex_flags_bf flags,
                              clish_prompt_for_input_ex_func func,
                              const cli_command *cmd,
                              const tstr_array *cmd_line,
                              const tstr_array *params, void *data);

/*@}*/


/* ------------------------------------------------------------------------- */
/** Get the username of the currently logged-in user.  If the user was
 * authenticated using RADIUS, TACACS or LDAP, this is the name they logged
 * in as, not the local user to which it was mapped.  (If you want that
 * instead, use clish_get_username_local())
 */
const char *clish_get_username(void);

/* ------------------------------------------------------------------------- */
/** Get the name of the local user account under which the currently
 * logged-in user is operating.  If the user authenticated from the
 * local passwd file, this will match the results of
 * clish_get_username().  But if the user authenticated with RADIUS,
 * TACACS or LDAP, and the username he logged in as did not exist locally,
 * it would be mapped to some local account -- and that's what would be
 * returned here.
 */
const char *clish_get_username_local(void);

/* ------------------------------------------------------------------------- */
/** Prompt the user for input during initialization.  This is ONLY to
 * be called from a module's initialization function.  We will get one
 * line of input and return it in the provided buffer, with a '\\0'
 * termintor at the end.  The buffer size must be at least 2 to allow
 * for some input and a delimiter.  If the user presses Ctrl+C at this
 * prompt, it will exit the CLI.
 */
int clish_init_get_input(char *buff, uint32 buff_size);

/* ------------------------------------------------------------------------- */
/** Send a request to mgmtd, but return immediately without waiting for the
 * reply.
 *
 * NOTE: THIS IS CURRENTLY FOR INTERNAL USE ONLY.  When you send a
 * message using this function, there is no way for the module to get
 * called back when the response arrives, and the CLI infrastructure
 * will log at WARNING when it does.
 */
int clish_send_mgmt_msg_async(bn_request *request);

/* ------------------------------------------------------------------------- */
/** Print a progress bar, preceded with a '\\r' character.
 *
 * Note that the caller should likely call clish_paging_suspend()
 * before this, lest the pager get in the way of progress updates.
 */
int clish_progress_update_bar(float32 percent_done);

/* ------------------------------------------------------------------------- */
/** Option flags that can be passed to cli_progress_track().
 */
typedef enum {
    cptf_none = 0,

    /**
     * Show progress for each individual step of the operation.
     * By default, we only show progress on the entire operation.
     */
    cptf_show_steps = 1 << 0,

    /**
     * Suppress the actual display of progress, and act as though
     * progress were disabled.  That is, just show the return
     * code/message at the end.  One might use this if the progress
     * tracking for an operation is so misleading as to be worse than
     * displaying nothing.
     */
    cptf_quiet = 1 << 1,

} clish_progress_track_flags;

/** Bit field of ::clish_progress_track_flags ORed together */
typedef uint32 clish_progress_track_flags_bf;


/* ------------------------------------------------------------------------- */
/** Track the progress of an entire operation.  Poll the status periodically
 * and print updates to the screen, until the operation is complete.
 *
 * The caller may provide either 'req' OR 'progress_oper_id'.
 * If 'req' is provided, it is used to compute the progress operation ID,
 * based on our standard conventions.  But if the operation is known to 
 * have a different operation ID (e.g. if it was started by an external
 * provider, which did not have access to the the request ID between the
 * CLI and mgmtd), specify 'progress_oper_id' instead.  In this case,
 * 'req' will be ignored.
 *
 * Note that this function may be called from a non-shell libcli
 * client, or when the shell is running without a TTY, although
 * progress will not be displayed in that case.  It will still perform
 * its other functions, which include waiting until the operation is
 * complete before returning, and setting the CLI code and message
 * from the tracked operation if there is an error.
 */
int clish_progress_track(const bn_request *req,
                         const char *progress_oper_id,
                         clish_progress_track_flags_bf flags,
                         uint32 *ret_code, tstring **ret_msg);


#ifdef PROD_FEATURE_WIZARD

/**
 * Flags to be specified on an invocation of the Wizard using
 * clish_invoke_wizard().
 */
typedef enum {
    cwf_none = 0,

    /**
     * Skip the Wizard's welcome message.
     */
    cwf_skip_welcome = 1 << 0,

    /**
     * Skip displaying of the Wizard's step numbers.
     *
     * NOTE: this should not be used without cwf_no_skip_summary, since
     * the summary page asks the user to type step numbers to return
     * to previous steps.  The user can still count from the top, 
     * since they are always monotonically increasing from 1, but this
     * is kind of obnoxious.
     */
    cwf_skip_step_numbers = 1 << 1,

    /**
     * Skip the Wizard's summary/confirmation screen.
     */
    cwf_skip_summary = 1 << 2,

    /**
     * Do not automatically save the configuration ("write mem") after
     * the user approves all the changes.
     */
    cwf_skip_save = 1 << 3,

    /**
     * Tell the Wizard that the steps you are using will not be
     * applying the requested changes as they are answered, but rather
     * only at the end, after the user has approved all of them.
     * 
     * The impact of this currently is just in terms of the messages
     * the Wizard will print in certain circumstances.  For example,
     * if the user presses Ctrl+C on the summary screen, we print a
     * message which says whether or not the changes have been
     * completely cancelled, vs. applied but not saved.
     */
    cwf_steps_delay_apply = 1 << 4,

    /**
     * Ask for confirmation to use the Wizard.
     * By default, we do not.
     */
    cwf_prompt_confirm = 1 << 5,

    /**
     * Enforce mandatory steps in the Wizard.
     * By default, we do not enforce them.
     */
    cwf_enforce_mandatory = 1 << 6,

} clish_wizard_flags;

/* ------------------------------------------------------------------------- */
/** Invoke the Wizard.  You may either pass it a list of blocks to run,
 * or specify 0 for num_blocks to run all default blocks (i.e. everything
 * EXCEPT those registered with wbf_normally_skip).
 *
 * We'll tell the Wizard to chain back to the CLI, just like
 * "configuration jump-start" does.  Note that for now, the user will
 * end up in Standard mode, regardless of which mode they started from
 * (bug 12129).
 *
 * NOTE: this function does not return (unless something goes wrong)!
 * It execs the Wizard on top of the CLI process.  The Wizard will
 * exec the CLI when it is done, but the old CLI will be gone.
 */
int clish_invoke_wizard(clish_wizard_flags flags, int num_blocks, 
                        const char *block1_name, ...);

#endif /* PROD_FEATURE_WIZARD */

#define CLISH_PROGRAM_NAME "cli";
extern const char clish_program_name[];


/* ------------------------------------------------------------------------- */
/** Log the specified message to the systemlog (a special log file
 * which is not managed by syslog, and which never gets rotated or
 * deleted).  Only mgmtd writes to this file, so the log message is sent
 * via an action request to mgmtd.
 *
 * NOTE: the action request is sent asynchronously, i.e. we do not
 * wait for a response, or even confirmation that the message has been
 * sent out.
 *
 * XXX/EMT: this should be in libcli.  But it's currently here for
 * easier access to the log prefix which we made up, containing the
 * username, since we want to include that in the message.  It's the
 * CLI shell who does that, since it's the one that calls
 * lc_log_init(), and it relies on env vars which may not be set in
 * other contexts.
 */
int clish_systemlog_log(const char *fmt, ...)
    __attribute__ ((format (printf, 1, 2)));


/* ------------------------------------------------------------------------- */
/* Reread underlying CLI config file.  This is not a great
 * abstraction, and not something we're crazy about exposing to
 * callers.
 */
int clish_cli_config_reread(void);


/*
 * FOR INTERNAL USE ONLY -- DO NOT CALL FROM MODULES!
 * This API may be removed in a future release.
 *
 * Used by libcli if it is going to be running for a long time
 * without returning (e.g. in cli_execute_commands_mult(), as in
 * bug 14091).  Called periodically to allow the CLI shell to 
 * process incoming events, to avoid excessive buildup.
 *
 * In non-CLI-shell clients, this does nothing.
 */
int clish_yield(void);


#ifdef __cplusplus
}
#endif

#endif /* __CLISH_MODULE_H_ */
