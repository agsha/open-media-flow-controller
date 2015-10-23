/*
 *
 * cli_client.h
 *
 *
 *
 */

#ifndef __CLI_CLIENT_H_
#define __CLI_CLIENT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "array.h"
#include "tstr_array.h"
#include "bnode.h"
#include "bnode_proto.h"

/* ========================================================================= */
/** \file src/include/cli_client.h Libcli client API
 * \ingroup cli
 *
 * Functions and declarations provided
 * by libcli for use by libcli clients such as the CLI shell.
 */

/**
 * Provide a way to override previously specified cli command's capability
 * required (and avoid) mask with another one. Mainly used if new customer
 * privilege class is defined. This override is done without regard to the
 * setting of the 'ccf_override' which is use for multiple registrations
 * of the same command.
 */
typedef struct cli_capab_override {
    /**
     * The command registration string to override
     */
    const char *cco_words_str;

    /**
     * The new capabilities that must be possessed
     */
    uint64 cco_capab_required;

    /**
     * The new capabilities that cannot be possessed
     */
    uint64 cco_capab_avoid;
} cli_capab_override;

/* ------------------------------------------------------------------------- */
/** Function to print output, to be provided by libcli client.
 */
typedef int (*cli_output_handler_func)(const char *output, void *data);


/* ------------------------------------------------------------------------- */
/** Function to send a message to the management daemon and wait for a
 * response, to be provided by libcli client.
 */ 
typedef int (*cli_mgmt_handler_func)(void *data,
                                     gcl_session *session, 
                                     bn_request *request, 
                                     bn_response **ret_response);


/* ------------------------------------------------------------------------- */
/** \name Initialization and deinitialization.
 *
 * The outer layer just handles
 * loading (but not initializing) modules, to make it easy to get past
 * this point and set a breakpoint in the debugger inside the modules.
 * Everything else happens in the inner layer.
 *
 * The GCL session pointer is provided in case command modules need it
 * to construct messages to send to the management daemon.  It is not
 * currently required but may be in the future.
 */
/*@{*/
int cli_init_outer(void);
int cli_init_inner(cli_output_handler_func output_handler,
                   void *output_handler_data,
                   cli_mgmt_handler_func mgmt_handler,
                   void *mgmt_handler_data,
                   gcl_session *mgmt_session);
int cli_deinit_inner(void);
int cli_deinit_outer(void);
/*@}*/

/* ------------------------------------------------------------------------- */
/** Get the current output and management handlers.
 */
int cli_get_handlers(cli_output_handler_func *ret_output_handler,
                     void **ret_output_handler_data,
                     cli_mgmt_handler_func *ret_mgmt_handler,
                     void **ret_mgmt_handler_data,
                     gcl_session **ret_mgmt_session);


/* ------------------------------------------------------------------------- */
/** Set the current output and management handlers.
 */
int cli_set_handlers(cli_output_handler_func output_handler,
                     void *output_handler_data,
                     cli_mgmt_handler_func mgmt_handler,
                     void *mgmt_handler_data,
                     gcl_session *mgmt_session);


/* ------------------------------------------------------------------------- */
/** A list of possible expansions for a command word, and a help
 * string for each.  This structure is used to return to the libcli
 * client a list of possible expansions for a word being entered on
 * the command line, assembled from the help callbacks of whichever
 * command registrations were appropriate.
 *
 * Note that this is very much like cli_expansion_option, except that
 * the strings are not const, and there is no void * field.
 */
typedef struct cli_help_option {
    /** 
     * The literal string that is a valid option for the current word.
     */
    char *cho_option;

    /**
     * A longer string explaining the effect that selecting this option
     * will have.  This is an optional field.
     */
    char *cho_help;
} cli_help_option;

TYPED_ARRAY_HEADER_NEW_NONE(cli_help_option, cli_help_option *);


/* ------------------------------------------------------------------------- */
/** Get online help on the current word in the provided command line.
 * Everything after the cursor is ignored.  Help is returned as an
 * array of dynamically allocated cli_help_option *'s.
 *
 * If an error on the command line prevents help from being given, a
 * user-printable string explaining the error is returned in
 * ret_error_string.  Either help or an error will be returned, but
 * not both: exactly one of them will be set to NULL.
 *
 * XXX/EMT: should allow -1 to be passed for cursor pos to assume it's
 * at the end of the line.
 */
int cli_get_help(const char *cmd_line, uint16 cursor_pos, 
                 cli_help_option_array **ret_options,
                 tstring **ret_error_string);


/* ------------------------------------------------------------------------- */
/** Get a list of possible completions for the current word in the
 * provided command line.  Everything after the cursor is ignored.
 * Completions are returned as a string array, with each string being
 * one full word.  Each word in the array is guaranteed to be prefixed
 * by the current word so far, up to the cursor.
 *
 * Also returned is the index of the beginning of the word being
 * completed in the original command line, and the full word itself
 * in a separate string, for convenience.
 *
 * If an error on the command line prevents completion from being
 * given, a user-printable string explaining the error is returned in
 * ret_error_string.  Either completion or an error will be returned,
 * but not both.
 */
int cli_get_completions(const char *cmd_line, uint16 cursor_pos,
                        uint16 *ret_word_begin_pos,
                        tstr_array **ret_completions,
                        tstring **ret_error_string,
                        tstring **ret_matched_word);


/* ------------------------------------------------------------------------- */
/** \name Executing Single Commands
 */
/*@{*/

/* ------------------------------------------------------------------------- */
/** Execute the specified command.  If the command line is valid, any
 * output from executing the command will be printed using the
 * client's registered output function.
 *
 * NOTE: if you are calling this function from your own execution
 * handler (therefore reentrantly, since it is through this function
 * that your execution handler got called in the first place), use
 * caution and test thoroughly.  There are some cases that may not
 * work as you expect, particularly long-running commands which may
 * call clish_extended_processing_begin().
 *
 * NOTE: if ret_line_complete returns false, then ret_error_string and
 * ret_cmd_pat will return nothing, and no CLI execution function will
 * have been called.
 *
 * \param cmd_line The command line to be executed.
 *
 * \param primary_command Tells whether this command was initiated
 * directly by some outside source, as opposed to being initiated by a
 * command execution handler.  Normally this will be 'true', but
 * should only be 'false' if a command execution handler called
 * cli_execute_command() reentrantly to execute another command as
 * part of its processing.  (If you are doing this, please read the
 * NOTE above!)  In this case, we will only log the command execution
 * at level DEBUG, regardless of the normal logging level for CLI
 * command executions.
 *
 * \retval ret_error_string If libcli detects an error before
 * executing the command (generally through some problem with matching
 * it against command nodes: not matching any nodes, being ambiguous,
 * etc.), the message is returned in ret_error_string.  Any errors
 * generated by the module are printed directly, as they may need to
 * be interleaved with the output.
 *
 * \retval ret_line_complete Returns false in ret_line_complete if the
 * line provided did not terminate; i.e. if the carriage return at the
 * end was either backslash-escaped, or inside double quotes.  If this
 * happens, libcli buffers the line, and assumes that the next line
 * fed to cli_execute_command() is a continuation of it.  This will
 * continue until the line ends in a carriage return that is neither
 * escaped nor inside double quotes, at which point it will return
 * true in ret_line_complete.
 *
 * \retval ret_cmd_pat If the command that matches was registered as
 * 'sensitive exposure' and the caller needs the command pattern, it
 * will be returned in 'ret_cmd_pat'. If it's not sensitive,
 * the tstring pointed to by 'ret_cmd_pat' will be set to NULL.
 *
 * \retval ret_success Did the command fully succeed in executing as
 * the user requested?  This will always be false if anything is returned
 * in ret_error_string.  But it will also be false if the command line
 * was valid, but the command called cli_printf_error() during execution.
 * This is used to detect errors for such cases where the caller might
 * want to abort execution of a series of commands if any of them fail.
 *
 * \return Zero for success, nonzero for failure.  "Failure" here does
 * not include an invalid command (such as might produce a return
 * value in ret_error_string), or the command having printed an error
 * during execution (such as would be indicated in ret_success).
 * Failure here means something more serious, generally an internal
 * programming error.  One exception to this is lc_err_cancelled, which
 * means that the command execution was cancelled partway through, perhaps
 * by a Ctrl+C from the user.  Callers may wish to deal with this 
 * differently than other return codes, as it is not really an error.
 */
int cli_execute_command(const char *cmd_line, tbool primary_command,
                        tstring **ret_error_string, tbool *ret_line_complete,
                        tstring **ret_cmd_pat, tbool *ret_success);

/* ------------------------------------------------------------------------- */
/** Like cli_execute_command(), except that the regular output of the
 * command is returned in ret_output.  This is accomplished by
 * temporarily installing an alternative output handler which saves
 * the output in a buffer.
 */
int
cli_execute_command_capture_ex(const char *cmd_line, tbool primary_command,
                               tstring **ret_error_string,
                               tstring **ret_output, tbool *ret_line_complete,
                               tstring **ret_cmd_pat, tbool *ret_success);

/**
 * Backward compatibility wrapper around cli_execute_command_capture_ex().
 * Passes 'true' for 'primary_command', and NULL for 'ret_line_complete'
 * and 'ret_cmd_pat'.
 */
int
cli_execute_command_capture(const char *cmd_line, tstring **ret_error_string,
                            tstring **ret_output, tbool *ret_success);

/**
 * Set the CLI mode to the base level ignoring any error messages
 * in trying to do so. Do not ignore errors in code execution.
 */
int cli_execute_no_conf_disable_mode(void);

/**
 * Since the CLI library has globals that retain state about what
 * mode it's in, attempt to put the CLI in the 'enable; conf t'
 * state by first exiting to the base level and then attempting.
 */
int cli_execute_enable_conf_mode(tstring **ret_error_string);

/*@}*/

/* ------------------------------------------------------------------------- */

/**
 * Option flags for cli_execute_commands_mult().
 */
typedef enum {
    cecmf_none = 0,

    /**
     * Continue execution on failure.  By default we will halt as soon
     * as any command returns failure.  If this flag is set, we will
     * execute all commands regardless of failures.
     */
    cecmf_cont_on_failure = 1 << 0,

} cli_execute_commands_mult_flags;

/** Field of cli_execute_commands_mult_flags ORed together */
typedef uint32 cli_execute_commands_mult_flags_bf;

/**
 * Response structure for a single command executed by
 * cli_execute_commands_mult().
 */
typedef struct cli_execute_command_result {
    /**
     * Full CLI command represented by this record.  Note that if there
     * were multi-line commands, they could have been split across lines
     * in the original command line array.  But this result array will
     * have those combined into single string, and a single record.
     */
    tstring *cecr_command;

    /**
     * Index of the first line in the original command line array we
     * were passed, which began this command.  If all of the lines
     * were single line commands, this will match the index of this 
     * entry in cecmr_commands.
     */
    uint32 cecr_command_idx;

    /**
     * Error strings generated by the infrastructure before executing
     * this command.  Note that this would NOT include errors printed
     * by the module during execution using cli_printf_error().
     */
    tstring *cecr_errors;

    /**
     * Output generated from the execution of this command.  Note that
     * this may include error messages printed by the module using
     * cli_printf_error().
     */
    tstring *cecr_output;

    /**
     * The outcome of running this command.  A 'false' value here may
     * mean either of two kinds of failure: the infrastructure caught
     * an error in parsing the command, before calling any execution
     * handler; or an execution handler was called, but it ended up
     * calling cli_printf_error().  This distinction is a subtle one,
     * and one not often exposed to users, so is thus not made here.
     */
    tbool cecr_success;

} cli_execute_command_result;

int cli_execute_command_result_free(cli_execute_command_result **inout_result);

TYPED_ARRAY_HEADER_NEW_NONE(cli_execute_command_result, 
                            struct cli_execute_command_result *);

/**
 * Overall response structure for cli_execute_commands_mult().
 */
typedef struct cli_execute_commands_mult_result {

    /**
     * List of commands executed, and the results from each.
     */
    cli_execute_command_result_array *cecmr_commands;

    /**
     * Did we halt execution early, due to a failed command?
     * (Will only happen if cecmf_cont_on_failure flag is not set)
     */
    tbool cecmr_halted_early;

} cli_execute_commands_mult_result;

int cli_execute_commands_mult_result_free(cli_execute_commands_mult_result
                                          **inout_result);

/**
 * Execute a series of CLI commands in sequence.
 *
 * \param cmd_lines Commands to execute.  Multi-line commands may be
 * split across multiple array elements; these will be combined, just
 * as they would be if multiple lines were entered by the user at the
 * command prompt.  Of course, it must be possible for the infrastructure
 * to infer that it is multi-line.  Currently, this is only done if a 
 * parameter breaks in the middle of a parameter (due to an open
 * double-quote); multi-line commands cannot be split between parameters.
 *
 * \param flags Option flags.
 *
 * \param progress_oper_id NOT YET IMPLEMENTED.  You must pass NULL here;
 * anything else will fail.  This is reserved for future use as a progress 
 * tracking operation ID.
 *
 * \retval ret_result Results of command execution.
 */
int cli_execute_commands_mult(const tstr_array *cmd_lines,
                              cli_execute_commands_mult_flags_bf flags,
                              const char *progress_oper_id,
                              cli_execute_commands_mult_result **ret_result);


/* ------------------------------------------------------------------------- */
/** Get the maximum length of any registered string literal command
 * word or string literal expansion help.  Useful for clients such as
 * the CLI shell who want to print help in a justified two-column
 * format and guess at the best width for the left column.
 */
int cli_get_max_word_len(uint32 *ret_max_word_len);


/* ------------------------------------------------------------------------- */
/** Option flags for dumping libcli's current runtime state. 
 */
typedef enum {
    cds_none           = 0,

    /** Dump to event logs (can be combined with cds_stdout to get both) */
    cds_log            = 1 << 0,

    /** Dump to stdout (can be combined with cds_log to get both) */
    cds_stdout         = 1 << 1,

    /** Only dump terminal nodes */
    cds_term_only      = 1 << 2,
    
    /** Include registered static help strings */
    cds_include_help   = 1 << 3,

    /** Include capabilities (if PROD_FEATURE_CAPABS enabled) */
    cds_include_capabs = 1 << 4,

    /** Include ACLs (if PROD_FEATURE_ACLS enabled) */
    cds_include_acls   = 1 << 5,

    /** Include registration flags */
    cds_include_flags =  1 << 6,
} cli_dump_state_opt;

int cli_dump_state(cli_dump_state_opt opt);

/* ------------------------------------------------------------------------- */
/** Apply any potential CLI command override for capability required/avoid.
 */
int cli_override_capab_registrations(const cli_capab_override *overrides);

/* ------------------------------------------------------------------------- */
/** Run a sanity check on the commands registered.  Log any errors
 * observed.  If 0 is passed for a limit, no limit is used.  Note that
 * exceeding the max_descr_len is only logged at level DEBUG, since
 * word wrapping will take care of this.  The log message is to help
 * you find messages that will wrap, as it may be still somewhat
 * desirable to fit descriptions onto a single line.
 */
int cli_sanity_check_registrations(uint16 max_word_len, uint16 max_descr_len);

/* ------------------------------------------------------------------------- */
/** An output handler which appends the output it is given to a tstring
 * to which it expects to find a pointer in 'data'.
 */
int cli_stock_output_handler_capture(const char *output, void *data);

/* ------------------------------------------------------------------------- */
/** Tell whether a certain command node is registered.
 */
int cli_is_command_registered(const char *cmd_words, tbool *ret_regd);

/* ------------------------------------------------------------------------- */
/** Buffer to store incomplete command lines in progress.  This will be
 * NULL unless the last command line passed to cli_execute_command() did
 * not terminate a command, in which case it will have the entire command
 * so far, ending with a '\n' character.
 */
extern tstring *cli_multi_line_buffer;

/* ------------------------------------------------------------------------- */
/** Maximum permitted command line length.
 */
/*
 * NOTE: be careful about reducing this number!  If you reduce it, and
 * someone's CLI history has a longer line in it, the CLI will consider
 * the history file corrupt, and will print a message to this effect and
 * then toast it.  So reducing this number would require a /var upgrade
 * to either selectively remove or truncate longer lines, or (simpler but
 * more destructive) remove all CLI history files.
 */
static const uint32 cli_max_line_size = 8192;

#ifdef __cplusplus
}
#endif

#endif /* __CLI_CLIENT_H_ */
