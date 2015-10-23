/*
 *
 * web_cli.h
 *
 *
 *
 */

#ifndef __WEB_CLI_H_
#define __WEB_CLI_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <time.h>
#include "common.h"
#include "ttime.h"
#include "tstring.h"
#include "str_array.h"
#include "tstr_array.h"
#include "gcl.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "md_client.h"
#include "libevent_wrapper.h"
#include "tacl.h"
#include "web.h"

/**
 * Option flags for web_execute_cli_commands(), web_execute_cli_command_ex()
 */
typedef enum {
    wef_none = 0,

    /**
     * Should we halt execution when we get an error?  By default, we
     * continue regardless of any errors.
     */
    wef_halt_on_error = 1 << 0,

    /**
     * Should any errors included in ret_errors also be included in
     * ret_output?  (By default they are not) This may seem redundant,
     * but the point is to interleave them properly, in the case where
     * halt_on_error is false.  Otherwise, you don't know) how they
     * were interleaved, and if you just print both errors and output,
     * they will not necessarily be ordered correctly relative to each
     * other.
     */
    wef_include_errors_in_output = 1 << 1,

    /**
     * Should the CLI commands themselves be included in ret_output?
     * If so, we include them right before the output each produced,
     * enclosed in "<b> ... </b>" to make them show up bolded.
     */
    wef_include_cmds_in_output = 1 << 2,

    /**
     * Should we try to detect requests for help?  That would be a 
     * question mark '?' character which was neither quoted nor
     * escaped.  If we detect one, we ignore everything else on the
     * line, fetch help from the CLI on the line up until the '?',
     * and print it italicized (in an \<em\> tag).
     */
    wef_do_help = 1 << 3,

    /**
     * Disable html body context escaping of command output (including both
     * CLI help responses and CLI error / non-error messages).  This flag
     * should be used with caution, since normally the automatic escaping of
     * cli output is desired to prevent inadvertent interpretation of CLI
     * output as html.  However, this might be needed, e.g.  if the output
     * is to be augmented or massaged and then escaped at a later stage.
     * Be careful, however, since the web_execute_cli_commands() function
     * already injects HTML bolding around the output if the flag
     * wef_include_cmds_in_output is set.
     */
    wef_no_html_esc_cmd_output = 1 << 4,

} web_execute_flags;

/** Bit field of ::web_execute_flags ORed together */
typedef uint32 web_execute_flags_bf;

/**
 * Used to execute a cli command and get the result.
 * \param web_data per request state
 * \param command the command.
 * \retval ret_error the error.
 * \retval ret_output the output.
 * \retval ret_success Did the command succeed?
 * \return 0 on success, error code on failure.  Note that a command
 * failing does not count as a failure here.  To return a nonzero value,
 * we need some more fundamental internal error.
 */
int web_execute_cli_command(web_request_data *web_data,
                            const char *command,
                            tstring **ret_error,
                            tstring **ret_output,
                            tbool *ret_success);

/**
 * Used to execute an array of cli commands and get the results.
 * We will keep executing until an error is returned (if we were told to
 * halt on error), or we finish.
 *
 * Caution:  The command output may contain embedded HTML.  Commands are
 * enclosed in "<b> ... </b>" tags to display them in bold if the
 * wef_include_cmds_in_output flag is set.  Because of this, command output
 * is html-escaped during this call to protect against injection of HTML
 * special characters from source data into the output stream.  If the
 * output is to be appended to the result message string using
 * web_set_msg_result*(), or web_report_results*() family of APIs, then
 * call web_set_msg_contains_html(g_web_data, true), otherwise the embedded
 * html itself will be escaped prior to emission.  If the output is emitted
 * to the web through another pathway, be sure it is not unintentionally
 * escaped again downstream prior to emission.  Note, finally, that while
 * not recommended, escaping may be disabled altogether if needed by adding
 * the web_execute_flags value wef_no_html_esc_cmd_output.
 *
 * \param web_data per request state
 * \param commands the array of commands, in the order to be executed.
 * \param flags Option flags to control our behavior.
 * \retval ret_error the concatenation of all non-empty error strings
 * returned, or NULL if no errors occurred.  Note that in this context,
 * "error strings" only refers to ones returned by the CLI infrastructure,
 * not those printed by modules.  Errors printed by modules are still
 * considered errors from the standpoint of whether the command succeeded,
 * but they show up in the regular output.
 * \retval ret_errors the cumulative error output of all commands.
 * Note that this is only errors detected by the CLI infrastructure,
 * not errors detected while a command was executing.
 * \retval ret_output the cumulative output of all commands.
 * \retval ret_success Did all of the commands succeed?
 * \return 0 on success, error code on failure.  Note that a command
 * failing does not count as a failure here.  To return a nonzero value,
 * we need some more fundamental internal error.
 */
int web_execute_cli_commands(web_request_data *web_data,
                             const tstr_array *commands,
                             web_execute_flags_bf flags,
                             tstring **ret_errors,
                             tstring **ret_output,
                             tbool *ret_success);

/**
 * Used to execute a cli command and get the result for use in HTML context.
 * Command errors and output are HTML body escaped by default (see flags).
 *
 * \param web_data per request state
 * \param command the command.
 * \param flags Option flags to control our behavior.  Unsupported flags are
 * ignored.  The only flags supported are wef_none for automatic escaping,
 * or wef_no_html_esc_cmd_output to suppress escaping.
 * \retval ret_error the error.
 * \retval ret_output the output.
 * \retval ret_success Did the command succeed?
 * \return 0 on success, error code on failure.  Note that a command
 * failing does not count as a failure here.  To return a nonzero value,
 * we need some more fundamental internal error.
 */
int web_execute_cli_command_ex(web_request_data *web_data,
                               const char *command,
                               web_execute_flags_bf flags,
                               tstring **ret_error,
                               tstring **ret_output,
                               tbool *ret_success);


#ifdef __cplusplus
}
#endif

#endif /* __WEB_CLI_H_ */
