/*
 *
 * cli_module.h
 *
 *
 *
 */

#ifndef __CLI_MODULE_H_
#define __CLI_MODULE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "misc.h"
#include "array.h"
#include "tstr_array.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "md_client.h"
#include "tacl.h"

/** \defgroup cli CLI module and client API */

/* ========================================================================= */
/** \file cli_module.h Libcli module API
 * \ingroup cli
 *
 * Functions and declarations provided by libcli for use by CLI
 * command modules.
 */

/* ========================================================================= */
/** @name Command registration typedefs
 *
 * Each call to register a command places a node in an n-ary tree.
 * Each node in the tree represents one of the words in the command.
 * A single multi-word command is represented by a path of nodes in
 * the tree, with the first word being a child of the root, and each
 * subsequent word being a child of the one preceding it.
 *
 * Each registration specifies a set of attributes as described below.
 * These attributes are associated with the node representing the last
 * word in the series of words specified with each registration.
 * Interior words are registered separately even when they are not
 * full commands by themselves; if nothing else, a help string must be
 * provided for them.  For example, a command set with a single
 * command "show interfaces" would have two registrations, one for
 * "show" and one for "show interfaces".
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** CLI command registration flags.  Each command node may have one or more
 * of these flags set.
 */
typedef enum {
    ccf_none = 0,
    
    /**
     * Specifies that this word ends a full, valid command.  If not
     * set, this is only being registered as a prefix of full
     * commands, and cannot be executed by itself.  For example,
     * "show" would not be terminal, but "show interfaces" would be.
     * When all modules are finished registering commands, a sanity
     * check is performed to verify that all leaf nodes have this flag
     * set.  Note that interior nodes may also have it set.
     */
    ccf_terminal = 1 << 0,

    /**
     * Specifies that this command word should be "hidden", which entails
     * acting as though it does not exist until the word is entered in
     * its entirety.  To hide an entire subtree of commands, set this
     * flag on the root of the subtree but not on any of its descendants.
     * This flag is only valid on string literal nodes.
     */
    ccf_hidden = 1 << 1,

    /**
     * Specifies that this command can only be executed in the CLI
     * shell, but not in any other libcli clients.  Generally this
     * should be used on any command that calls APIs from
     * clish_module.h.
     *
     * Note that no command using this flag should be emitted by
     * any of your reverse-mapping handlers, since then a non-shell
     * libcli client would not be able to run "show configuration".
     * This effectively means you cannot have any commands which
     * both set config nodes and call clish_module.h APIs.
     * If you think you need to do this, please ask your Tall Maple
     * technical contact for assistance.
     *
     * When this flag is used, libcli will discard this command
     * registration if it is not running in the context of the CLI
     * shell.  It can tell by calling clish_is_shell().  The version
     * of this in the CLI shell returns true; the version in libclish
     * returns false.
     */
    ccf_shell_required = 1 << 2,

    /**
     * Only appropriate for commands which are (a) wildcards, and
     * (b) terminal, and (c) leaf nodes.  Specifies that if
     * additional words appear on the command line after the terminal
     * word, they should be permitted.  If this flag is not used, the
     * default behavior on command execution is to give an error and
     * not execute the command.
     *
     * In general the default behavior should be accepted, lest the
     * user remain under the illusion that the additional parameters
     * provided had some effect.  The flag is provided to address two
     * cases:
     *
     *    - Cisco-style "no" commands, where it is desirable to allow
     *      the user to precede any cut-and-pasted command from "show
     *      configuration" output with a "no" and have it work
     *      correctly.  The flag is required when the specific value
     *      to be negated does not have to be specified, e.g.
     *      "no interface eth0 duplex" to negate
     *      "interface eth0 duplex full".
     *
     *    - Commands which wish to take an arbitrarily long list of 
     *      parameters, such as ping, traceroute, and tcpdump, which
     *      just pass through whatever they get on the command line
     *      when launching the respective binary.
     */
    ccf_allow_extra_params = 1 << 3,

    /**
     * Only appropriate for terminal commands whose last word is a
     * wildcard.  Specifies that if additional words appear on the
     * command line after the terminal word, they should all be
     * combined into a single string and used as the value for the
     * wildcard.  This is sort of as if they had been enclosed in
     * double quotes, though all whitespace between each word is
     * compressed to one space.  Mutually exclusive with
     * ccf_allow_extra_params.
     *
     * NOT YET IMPLEMENTED.
     */
    ccf_combine_last_params = 1 << 4,

    /**
     * Specifies that this command registration should take precedence
     * over any other registrations of the same command node.  Without
     * the use of this flag, if the same command node is registered
     * twice, whichever one is processed last will take precedence,
     * but the infrastructure makes no guarantees about the order
     * modules will be loaded and initialized in.  A node with this flag
     * set cannot be overridden by a later registration, so it does not
     * matter which one is registered first.
     * 
     * This mechanism is generally expected to be used by a
     * customer-specific module to override a generic module.
     */
    ccf_override = 1 << 5,

    /**
     * Normally the infrastructure will refuse to register commands
     * with underscores in the literal words.  This is because the
     * Samara reference modules use hyphens to separate words in
     * command literals, and it would be confusing to also have some
     * other commands using underscores for the same purpose.
     *
     * This flag disables this sanity check for the current command
     * only.  Some commands (like "_shell" and "_exec") violate the
     * underscore rule on purpose.
     */
    ccf_allow_underscores = 1 << 6,

    /**
     * Specifies that this may contain sensitive information, and
     * should be obfuscated if the command invocation is to be stored
     * in any persistent manner (in the logs, or the command history).
     *
     * This is usually used on wildcard nodes, where the user may
     * enter some sensitive information like a wildcard.  However, it
     * may also be used on a literal.  Usually that would be if the
     * literal (or an ancestor in the command tree) is hidden, and
     * you want to protect the name of the command more carefully.
     *
     * Note that a node registered with this flag may optionally
     * specify cc_sensitive_word_callback, which can decide that a
     * parameter is only partly sensitive (and do its own custom
     * obfuscation), or not sensitive at all (so no obfuscation is
     * done).
     */
    ccf_sensitive_param = 1 << 7,

    /**
     * Indicates that this node is the last fixed-position word in a
     * command with a variable argument list (a list of reorderable
     * and possible optional arguments).
     *
     * This flag is set explicitly by the module, and then
     * sanity-checked against the presence of subsequent command
     * registrations with a special word defined by cli_var_args_word
     * ("|"), following the word associated with this node.  A node
     * with this flag set may have an execution handler specified even
     * though it is not necessarily a terminal command, due to the
     * presence of this flag.  See the Samara Technical Guide for
     * a more detailed explanation and examples.
     *
     * This same node may or may not be marked with ccf_terminal.
     * If it is, this means that all of the arguments are optional,
     * and that the fixed-length portion of the command by itself
     * is fine.  If it is not, this means that even if all of the
     * arguments individually are optional, at least one of them
     * must be specified to form a complete command.
     */
    ccf_var_args = 1 << 8,

    /**
     * For use on the first word of an argument in a variable argument
     * list (the first word after the "|" in a command registration).
     * Specifies that this parameter is required, so the command
     * should not be considered complete unless at least one instance
     * is present.  By default, arguments in a variable argument list
     * are optional.
     */
    ccf_required_param = 1 << 9,

    /**
     * For use on the first word of an argument in a variable argument
     * list (the first word after the "|" in a command registration).
     * Specifies that this parameter is permitted to be specified
     * multiple times.  By default, each argument is permitted only
     * once, and an error will be generated if more are detected.
     */
    ccf_permit_mult = 1 << 10,

    /**
     * For use on the first word of an argument in a variable argument
     * list.  Specifies that if used at all, this parameter must come
     * last; no other parameters may follow it.  Mutually exclusive
     * with ::ccf_permit_mult.
     *
     * Note that while in general parameters underneath a varargs root
     * may not have interior terminal nodes, a parameter subtree
     * marked with this flag may.
     */
    ccf_last_param = 1 << 11,

    /**
     * Specifies that the CLI should not log warnings about the length
     * of this command or its help string.  Normally these warnings
     * are logged to notify the developer of commands whose length
     * would mess up the alignment/justification of the two columns of
     * help, but there are some cases where you can't get around this
     * and don't want the warning every time.
     */
    ccf_ignore_length = 1 << 12,

    /**
     * Specifies that this command word should be marked "deprecated". 
     * This does not change the functionality, but is an aid to developers
     * and doc writers.
     */
    ccf_deprecated = 1 << 13,

    /**
     * Specifies that this command and everything underneath it should
     * be available even when the user is in a prefix mode that this 
     * command is not a part of.  In other words, even when there are words
     * on the prefix stack that would otherwise cause this command not to 
     * be matched.
     *
     * This flag is only appropriate for top-level command nodes,
     * and for nodes immediately underneath a prefix mode root
     * (using the ::ccf_prefix_mode_root flag).  You do not need 
     * to add it to any descendants of the node it is used on; they
     * will automatically get this behavior also.
     *
     * At the top level, this is mainly just used on "show", "help",
     * "exit", "quit", and "internal".  Underneath prefix modes, this is
     * not used anywhere in the base platform, but might be used by
     * customer modules.  For example, if "a *" and "a * b *" were
     * prefix mode roots, "a * show" might use this flag.  The effect
     * would be that if you were in the lower-level mode ("a * b *"),
     * then "show ?" would bring up the commands underneath "a * show".
     *
     * See also the related flag ::ccf_prefix_merge.
     */
    ccf_universal = 1 << 14,

    /**
     * Specifies that this node is a point at which the user can enter
     * a "prefix mode", sometimes also know as a "Cisco-style CLI mode".
     * Must be used in conjunction with ccf_terminal and a custom
     * execution function.
     */
    ccf_prefix_mode_root = 1 << 15,

    /**
     * Appropriate only for nonterminal literal nodes in the subtree
     * underneath a prefix root.
     *
     * Normally, if two identical words match at different prefix 
     * levels (e.g. "show" and "a * show", when "a *" is on the
     * prefix stack), the lower-level one takes precedence.
     * i.e. the match with all higher-level ones are automatically
     * discarded.  This flag specifies that we should NOT do that,
     * but instead keep all higher-level matches also.
     *
     * Note that if matching ENDS on this word, the HIGHER-level one
     * will take precedence.  This matters only for help, i.e. in
     * determining which one's help string will get used.  It doesn't
     * matter for completion, since both words are the same they will
     * complete the same; and it doesn't matter for execution since
     * this must be a nonterminal node.
     */
    ccf_prefix_merge = 1 << 16,

    /**
     * Specifies that this command node (and thus all of its
     * descendants) should be unavailable.  That is, if the user tries
     * to use this command (help, completion, or execution), the CLI 
     * will act as though the command were not registered.  Nevertheless,
     * invocations of the command will still be reverse-mapped.
     *
     * NOTE: use this flag with caution, since invocations of the
     * command may still show up in "show config", but then those
     * invocations would fail to execute on this system, or any other
     * that has the commands unavailable.  For this reason, it is
     * often considered preferable to hide command rather than make
     * them unavailable.
     */
    ccf_unavailable = 1 << 17,

    /**
     * Specifies that any user should be able to access this command,
     * regardless of what capability flags they have.  This is in lieu
     * of honoring cc_capab_required, and a warning will be logged if
     * cc_capab_required is nonzero on a registration that specifies
     * this flag.  Generally only applicable to terminal commands.
     * Terminal commands are required to explicitly state their
     * capability requirements, so this is a means to say you don't want
     * to require any capabilities.  If you neither set this flag nor
     * assign a value to cc_capab_required, a warning will be logged.
     *
     * NOTE: this flag is only relevant if PROD_FEATURE_CAPABS is in use.
     * If you are using PROD_FEATURE_ACLS, this flag will have no effect.
     */
    ccf_available_to_all = 1 << 18,

    /**
     * Applicable only to nodes that have ccf_prefix_mode_root set.
     * Specifies that we should not use prefix modes when reverse-mapping
     * this subtree of commands, even if doing so is otherwise enabled.
     */
    ccf_prefix_mode_no_revmap = 1 << 19,

    /**
     * Relevant only if PROD_FEATURE_ACLS is enabled.  Requests that
     * all ACL-related settings registered on this node (except for
     * local ACLs or modes) be propagated to all nodes in the subtree
     * under this node which do not set those settings for themselves
     * (or inherit from a closer ancestor).
     *
     * Note that each part of the ACLs settings can be set
     * individually.  So it is possible for descendants to set only part
     * of the inheritable settings (e.g. the operation mappings for
     * the CLI "exec" operation), yet still inherit other settings 
     * (like the ACL and the mode set).  The following are the five 
     * areas of ACL settings which can be overridden individually:
     *   1. The ACL itself
     *   2. The mapping of mgmtd operations to the "exec" operation
     *   3. The mapping of mgmtd operations to the "revmap" operation
     *   4. The custom ACL callback (overrides #1-3)
     *   5. The modes the command is available in
     *
     * Note that local ACLs and modes do not participate in
     * inheritance.  As with the corresponding concept with mgmtd
     * nodes, they (a) are not propagated downwards, and (b) do not
     * prevent the node they are on from inheriting.
     */
    ccf_acl_inheritable = 1 << 20,

    /**
     * Relevant only to wildcard command nodes (when last word is "*").
     * This flag causes the value entered for this word to be normalized
     * to the data type specified in the cc_normalize_type field of the
     * command registration.
     *
     * This flag always impacts command execution, but only sometimes
     * impacts help and completion.  Specifically, for help and
     * completion, the word is only normalized if it is NOT the word
     * for which help and completion is being generated.  In other
     * words, help and completion for this word itself will be
     * generated without the benefit of normalization, as we want to
     * steer the user in the direction of entering the normalized
     * value directly.  However, if the word is used in generating
     * help and completion for another word (e.g. to query what
     * objects are available to be deleted), we use a normalized
     * version of the word.
     *
     * The normalization is done by creating an attribute of that data
     * type using the string provided, and then retrieving the string
     * representation of that attribute.  If such an attribute cannot
     * be created (i.e. the string entered is not valid for that type,
     * let alone normalized), no normalization is done.
     */
    ccf_normalize_value = 1 << 21,

    /**
     * Do not log when this CLI command is executed.  Applicable only
     * to terminal commands.
     *
     * Usage of this should be VERY RARE, as it is generally assumed
     * that all CLI command invocations will be logged.  Commands with
     * sensitive parameters should use ::ccf_sensitive_param instead.
     * ccf_suppress_logging was added for "reload force immediate",
     * which wants to have as few external dependencies as possible,
     * and therefore wants to avoid possibly blocking on syslog().
     */
    ccf_suppress_logging = 1 << 22,

} cli_command_flags;

/* Bit field of ::cli_command_flags ORed together */
typedef uint32 cli_command_flags_bf;

/* ------------------------------------------------------------------------- */
/** A list of possible expansions for a command word, and a help
 * string for each.  This structure is used by modules specify for
 * libcli a static list of options available for a wildcard command
 * word being registered.
 */
typedef struct cli_expansion_option {
    /** 
     * The literal string that is a valid option for the current word.
     * This is a mandatory field.  Presumably this represents a CLI
     * keyword, so it is not expected to be localized.
     */
    const char *ceo_option;

    /**
     * A longer string explaining the effect that selecting this option
     * will have.  This is an optional field.
     *
     * Because this is text that will be shown to the user, but is not
     * a keyword, it may be localized.  But because these structures
     * are initialized where function calls are not legal, and because
     * the locale may change before we want to print them anyway,
     * you should use the N_() macro around these strings.  This will
     * cause the string to appear in the string catalogs, and the CLI
     * infrastructure will do the lookup when it wants to display it.
     */
    const char *ceo_help;

    /**
     * This field is ignored by libcli and can be used by modules for
     * whatever purpose desired.  For example a module registering an
     * array of these structures in the cc_help_options field could put
     * a value here to tell one of its callbacks what to do when this
     * option is selected.
     */
    void *ceo_data;
} cli_expansion_option;

/**
 * Look up the ceo_data field of the record with a matching ceo_option
 * in an array of cli_expansion_option structs.  If num_options is -1,
 * assume that the array is terminated by a record with a NULL
 * ceo_option.
 *
 * NOTE: if the option string provided matches more than one option,
 * the first one found will be returned.  If you want to be able to 
 * detect ambiguous matches, call cli_expansion_option_get_data_ex()
 * instead.
 */
int cli_expansion_option_get_data(cli_expansion_option options[],
                                  int32 num_options, const char *option,
                                  void **ret_data, tbool *ret_found);

/**
 * Same as cli_expansion_option_get_data(), except returns an
 * additional boolean telling whether the option string provided
 * matched more than one option (as a prefix).  In the ambiguous case,
 * ret_found will still return true, and ret_data will still have the
 * first record found.
 */
int cli_expansion_option_get_data_ex(cli_expansion_option options[],
                                     int32 num_options, const char *option,
                                     void **ret_data, tbool *ret_found,
                                     tbool *ret_ambiguous);


/* ------------------------------------------------------------------------- */
/** Options for how to derive possible completions for a wildcard.
 * See completion callback comments below for details.
 */
typedef enum {
    /**
     * Automatic completion behavior unspecified.  Normally this will
     * default to not providing completion for the node (or using the
     * callback if there is one); but if an array of
     * cli_expansion_option structures was provided for use with help,
     * it will be used for completion by default (just as if 
     * cct_use_help_options had been specified).
     */
    cct_none = 0,
    
    /**
     * Explicitly request no automatic completion behavior.  This is
     * offered distinctly from cct_none for cases where an array of
     * help options is provided, but for whatever reason the module
     * specifically does not want to use those values for completion.
     * This might come up, for example, if the wildcard's value is
     * actually supposed to be a comma-separated list of one or more
     * of the options listed in help.
     */
    cct_no_completion,

    /**
     * Given a binding name with exactly one component of the name as
     * a wildcard ("*"), do a query to the management hierarchy and
     * find all nodes with matching names.  Use the variable portion
     * of the node (i.e. the values the wildcard takes on) as the
     * possible completions.
     */
    cct_matching_names,

    /**
     * Given a binding name with one or more components of the name
     * as a wildcard ("*"), do a query to the management hierarchy
     * and find all nodes with matching names.  Use the values of
     * these nodes as the possible completions.
     */
    cct_matching_values,

    /**
     * Use the options in the structure specified by the
     * cc_help_options field as the possible values for completion of
     * this word.
     */
    cct_use_help_options,
    cct_last
} cli_completion_type;

/* ------------------------------------------------------------------------- */
/** Which operation to perform on a configuration node when executing
 * a command using the standard execution handler.
 */
typedef enum {
    /** No operation specified */
    cxo_none = 0,

    /** Set a node to a specified type and value */
    cxo_set,

    /** Delete a node */
    cxo_delete,

    /** Create a node */
    cxo_create,

    /** Reset a node to its default */
    cxo_reset,

    /** Poke an action node optionally with a type and value */
    cxo_action,

    cxo_last
} cli_exec_operation;

/* ------------------------------------------------------------------------- */
/** Options for how to reverse-map a command.
 */
typedef enum {

    /**
     * No automatic reverse-mapping behavior.  Specify this if you are
     * going to be using a callback, or not providing any reverse-mapping
     * at all.
     */
    crt_none = 0,

    /**
     * Automatically reverse-map a command which uses the standard
     * exec handler, by looking at the node it sets and working backwards.
     */
    crt_auto,

    /**
     * Reverse-map a set of nodes, specified with a node name pattern,
     * to a command template which is parameterized using parts of the
     * names and values of nodes matched by your pattern.
     */
    crt_manual,

    crt_last
} cli_revmap_type;


/* ------------------------------------------------------------------------- */
/** A reverse mapping handler returns a structure of this type.
 */
typedef struct cli_revmap_reply {
    /**
     * Exact names of nodes which this reverse-map handler wants to
     * consume.
     */
    tstr_array *crr_nodes;

    /**
     * Binding name patterns of nodes which this reverse-map handler
     * wants to consume.  This is an another method of specifying
     * which nodes to consume, an alternative to crr_nodes.
     * 
     * NOTE: this should be RARELY used, generally only in a small
     * number of cases where you expect each pattern to match a large
     * percentage of the bindings in the whole database.  For patterns
     * than match a small number of bindings, this will be less
     * efficient than using crr_nodes.
     */
   tstr_array *crr_node_patterns;

    /**
     * Array of CLI command invocations produced from the nodes consumed.
     */
    tstr_array *crr_cmds;
} cli_revmap_reply;


/* ----------------------------------------------------------------------------
 * Forward-declare CLI command structure to solve circular dependencies.
 */
typedef struct cli_command cli_command;

/* ------------------------------------------------------------------------- */
/** Types of help that may be requested by the framework.  Description
 * help is a string describing a string literal command word.
 * Expansion help is a list of possible words to complete the wildcard
 * command word the user is on, optionally with each one accompanied
 * by a textual description.  Termination help is a description to be
 * printed next to "<cr>", explaining what will happen if the user
 * presses Enter after a terminal command.  See the CLI design
 * specification for a more in-depth explanation.
 */
typedef enum {cht_none = 0, cht_description, cht_expansion, cht_termination,
              cht_last} cli_help_type;

/* ------------------------------------------------------------------------- */
/** Help callback.
 *
 * Help is structured in two columns: the left column showing the literal
 * string that the user may type, or a description thereof enclosed in
 * angle brackets (by convention); the right column containing
 * explanatory text about the corresponding option.
 *
 * The help callback returns its answer by calling
 * cli_add_description_help(), cli_add_expansion_help(), or
 * cli_add_termination_help(), depending on the type of help
 * requested.
 *
 * If description or termination help is requested, the appropriate
 * function should only be called once, as only one answer per command
 * is allowed.
 *
 * If expansion help is requested, the function may call
 * cli_add_expansion_help() any number of times, once for each 
 * option it wishes to provide.
 *
 * The words from the command line are passed in three forms:
 *
 *   - cmd_line is the entire command line, broken into words.  Note
 *     that this only includes characters to the left of the cursor;
 *     anything under and to the right of the cursor is ignored.
 *
 *   - params is only the values of the wildcards.  This is redundant
 *     with cmd_line, but allows the code to avoid dependence on the 
 *     number of string literals mixed in, e.g. to allow the same function
 *     to be used to service a command and its "no" counterpart.
 *
 *   - curr_word is only the last word on the line; the one for which help
 *     is being requested.  This is redundant with cmd_line, but since 
 *     the majority of help functions will only be interested in this, 
 *     it saves the trouble of extracting the last word from an array.
 */
typedef int (*cli_help_callback)(void *data, cli_help_type help_type, 
                                 const cli_command *cmd,
                                 const tstr_array *cmd_line,
                                 const tstr_array *params,
                                 const tstring *curr_word,
                                 void *context);

/* ------------------------------------------------------------------------- */
/** Completion callback.
 *
 * Passed the command line in the same three forms as with the help
 * callback.  Returns a list of possible completions for the current
 * word by adding them to the pre-allocated tstr_array
 * ret_completions.  This array must not be modified in any other
 * way besides appending new elements.
 *
 * The completions should all be full words, all prefixed with the
 * current word, rather than just the portion of the word that has not
 * yet been typed.
 */
typedef int (*cli_completion_callback)(void *data, const cli_command *cmd,
                                       const tstr_array *cmd_line,
                                       const tstr_array *params,
                                       const tstring *curr_word,
                                       tstr_array *ret_completions);

/* ------------------------------------------------------------------------- */
/** Execution callback.
 *
 * As with the help and completion callbacks, the execution callback is
 * passed the command line broken up into words, as well as just the 
 * values of the wildcards.  The differences are (a) the entire line,
 * including characters after the cursor, is included; and (b) the current
 * word is not passed, as it is not interesting to this callback.
 *
 * There is no return value besides the error code; the execution
 * callback just does whatever it needs to to, which usually involves
 * printing things with cli_printf() and/or cli_printf_error(), and/or
 * talking to the management daemon with mdc_send_mgmt_msg().
 *
 * (Rationale: this function could have returned an error string
 * separately, for the libcli client to do with as it liked; but 
 * this raises too many difficulties in cases where errors may
 * be interleaved with output, etc.)
 */
typedef int (*cli_execution_callback)(void *data, const cli_command *cmd,
                                      const tstr_array *cmd_line,
                                      const tstr_array *params);

/* ------------------------------------------------------------------------- */
/** Reverse-mapping callback.
 *
 * If cc_revmap_names contains a binding name pattern, this callback
 * is called once for each binding in the database matching the
 * pattern.  The name and value of the binding is passed in the 'name'
 * and 'value' parameters, and since the name is already broken down
 * into parts for pattern-matching, the parts are passed in
 * 'name_parts'.  Additionally, the binding itself is passed directly.
 *
 * If cc_revmap_names is NULL, this callback is called once during the
 * reverse-mapping process.  In this case, 'name', 'name_parts',
 * 'value', and 'binding' are NULL.
 *
 * Should inspect the nodes, looking for nodes created by commands you
 * know about.  Return a list of nodes to be "consumed", and a list of
 * CLI commands which would have generated these nodes.  Note that
 * this function does not return the ordering constant, which is
 * specified at registration time.
 *
 * 'bindings' is an array of bindings guaranteed to be sorted by name.
 *
 * ret_reply points to a pre-allocated cli_revmap_reply, whose two
 * string arrays are also already allocated.  The callback needs only
 * add strings to the two arrays.
 *
 * Be careful to properly escape the strings you put into your 
 * command invocations.  If one of your command parameters has
 * spaces, quotes, or backslashes in it, you cannot use it as is,
 * nor will putting it in quotes necessarily be sufficient.
 * Call cli_escape_str() on any parameter that might have instances
 * of these three special characters.
 */
typedef int (*cli_revmap_callback)(void *data, const cli_command *cmd,
                                   const bn_binding_array *bindings,
                                   const char *name,
                                   const tstr_array *name_parts,
                                   const char *value, 
                                   const bn_binding *binding,
                                   cli_revmap_reply *ret_reply);


/* ------------------------------------------------------------------------- */
/** Called to determine if a particular word entered by the user on the
 * command line should be considered sensitive, and if so how should it
 * be obfuscated.  Such a function would only be called if the
 * ::ccf_sensitive_param flag was registered on the command node.
 *
 * NOTE: the 'cmd_line' and 'params' parameters are NOT currently
 * passed to this function!  They will always be NULL anytime your
 * function is called.  They are present in the prototype to allow
 * for this functionality to be added in the future without breaking
 * existing modules.
 *
 * The default if this function is not specified, or if it does not
 * return anything, is to consider the entire word sensitive, and
 * sanitize it by replacing it with cli_obscured_param.  The function
 * may do one of three things:
 *
 * <ul>
 * <li>Set ret_sensitive to false.  The entire word will be considered
 * non-sensitive, and so will be unaffected by the ::ccf_sensitive_param
 * flag.
 * <li>Set ret_sensitive to true, and return a string in ret_sanitized_word.
 * The string you return will be used as the sanitized version of the word
 * provided.  Usually this means that the word was only partly sensitive,
 * and you have done some selective obfuscation.  By convention, it is 
 * recommended that you replace any sensitive material with the contents
 * of cli_obscured_param, for consistency with other sensitive parameter
 * obfuscation.
 * <li>Set ret_sensitive to true, and return NULL in ret_sanitized_word.
 * This is the default if you do nothing; as described above, the entire
 * word will be considered sensitive.
 * </ul>
 */
typedef int (*cli_sensitive_word_callback)(void *data, const cli_command *cmd,
                                           const tstr_array *cmd_line,
                                           const tstr_array *params,
                                           const char *curr_word,
                                           tbool *ret_sensitive,
                                           tstring **ret_sanitized_word);

/* ------------------------------------------------------------------------- */
/** String used to designate a wildcard command word.  See comment about
 * cc_words_str below for explanation.
 */
#define CLI_WILDCARD_WORD "*"
extern const char cli_wildcard_word[];

/* ------------------------------------------------------------------------- */
/** String used to designate the boundary between the fixed-position
 * words and a variable argument list.  See comment with ccf_var_args
 * flag above for explanation.
 */
#define CLI_VAR_ARGS_WORD "|"
extern const char cli_var_args_word[];

/* ------------------------------------------------------------------------- */
/** String used to replace potentially sensitive parameters in CLI
 * commands, for purposes of logging and entering into the command
 * history.
 */
#define CLI_OBSCURED_PARAM "*"
extern const char cli_obscured_param[];


/* ------------------------------------------------------------------------- */
/** @name Command Modes
 *
 * This section contains definitions and APIs pertaining to command
 * modes.  These are the three primary modes -- "standard", "enable",
 * and "config" -- which determine which commands are available to 
 * the end user.  These are distinct from "prefix modes".
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** Flags representing the three primary modes the CLI may be in.
 */
typedef enum {
    cm_none = 0,
    cm_standard  = 1 << 0,
    cm_enable    = 1 << 1,
    cm_config    = 1 << 2,
} cli_mode;

/**
 * A bit field of cli_mode ORed together, expressing a set of modes.
 * For example, this could be used to represent which modes in which a
 * command should be available.
 *
 * (Note: this type is only actually used to represent which modes in
 * which a command should be available if PROD_FEATURE_ACLS is
 * enabled.  If the system is instead using PROD_FEATURE_CAPABS, the
 * availability of each command in different modes is still determined
 * by capabilities (certain capability flags are masked off in the
 * lower modes).
 *
 * The first three entries (besides "none"), which share the names of
 * the modes themselves, are the most common; they mean that the
 * command is available in that mode and all higher ones.  The ones
 * that come afterwards cover the unusual cases of commands that need
 * to disappear when higher modes are entered.
 */
typedef enum {
    cms_none =           0,
    cms_standard =       cm_standard | cm_enable | cm_config,
    cms_enable =         cm_enable | cm_config,
    cms_config =         cm_config,
    cms_standard_only =  cm_standard,
    cms_enable_only =    cm_enable,
} cli_mode_set;

/**
 * Change the command mode the CLI is currently in.  Note that this
 * does not automatically change the prompt to reflect the new mode.
 */
int cli_set_mode(cli_mode new_mode);

/**
 * Return what command mode the CLI is currently in.
 */
cli_mode cli_get_mode(void);

/*@}*/


/* ------------------------------------------------------------------------- */
/** @name Capabilities
 *
 * At any given point in time a session holds zero or
 * more capabilities which can come and go in response to various
 * actions or events.  Note that not all sessions of the same user
 * will have the same capabilities.
 *
 * The session's list of capabilities is held in a 64-bit-wide
 * bit field.  Each capability should be defined as a single bit
 * (i.e. 1 << n where n is in [0..63]).  The list of capabilities
 * is not defined in libcli; this is up to the modules.  The 
 * capability flags used by the Samara base modules are defined
 * in src/include/climod_common.h.
 */ 
/*@{*/
typedef uint64 cli_capability;
typedef uint64 cli_capability_list;
#define ccp_none  ((cli_capability)0)
#define ccp_all   ((cli_capability) UINT64_MAX)

/** 
 * Capability callback.  A function to be called by the infrastructure
 * to determine if a user has enough capabilities for this command
 * node to show up in the UI.
 */
typedef int (*cli_capability_callback)(void *data, const cli_command *cmd,
                                       cli_capability_list cap_list,
                                       tbool *ret_allow);


/*@}*/


/* ------------------------------------------------------------------------- */
/** @name ACLs
 *
 * This section contains APIs for controlling access to CLI commands
 * if PROD_FEATURE_ACLS is enabled.
 *
 * If ACLs are used as the access control system, each CLI command
 * must specify two things: what modes it should be available in, and
 * who should be able to perform which CLI operations on the command.
 * Setting the modes is simple: call cli_acl_set_modes().
 *
 * As for specifying who can do what, there are two operations we are
 * concerned with: "exec" and "revmap":
 *
 *   - To "exec" a command means that it is visible in help and completion,
 *     and that you can execute it.   If a command is not available for
 *     exec, the CLI pretends it does not exist (just as with commands
 *     for which a user did not have the required capabilities, under the
 *     old capability system).
 *
 *   - To "revmap" a command means that you can see invocations of this
 *     command in the output of "show configuration" and its variants.
 *     If a command is not available for revmap, the CLI strips out any
 *     invocations of this command before displaying "show configuration"
 *     output.
 *
 * To grant or deny access to perform these two operations on your nodes,
 * there are two steps:
 *
 *   1. Build an ACL on the node.  This ACL will use the same building blocks
 *      as mgmtd modules use to build ACLs on their nodes, and thus will
 *      permit the user to perform some set of mgmtd node operations.
 *
 *   2. Establish a mapping from the mgmtd node operations to the CLI 
 *      operations.  For example, one common mapping is: 
 *      (a) For "exec", require set_modify or set_create.
 *      (b) For "revmap", require query_get.
 *
 * Building an ACL and specifying a mapping for the "exec" operation are
 * mandatory, as there is no default for either.  The default mapping for
 * the "revmap" operation is to require the query_get operation.
 *
 * There are a few different APIs below for building operation
 * mappings.  Each of "exec" and "revmap" has a mapping, which takes as
 * input the set of mgmtd node operations permitted by the ACL, and
 * decides whether or not the user should be allowed to perform a
 * particular CLI operation.  The mapping is a nested boolean
 * expression, and the comments below describe the operation of each
 * API in terms of what it adds to the expression.
 */

/*@{*/

/*
 * This is the INTERNAL function that adds multiple ACLs to a CLI node.
 * Do not call this directly -- use the cli_acl_add() or 
 * cli_acl_add_local() macros instead.
 *
 * The parameters after 'cmd' and 'local' are of type 'const ta_acl *',
 * and MUST be terminated by a NULL.  The purpose of the wrapper macro
 * is to add the NULL automatically, relieving the burden from the
 * developer.
 */
int cli_acl_add_internal(cli_command *cmd, tbool local, ...);


/* ------------------------------------------------------------------------- */
/** Add one or more ACLs to this CLI command.
 *
 * These ACLs specify operations which apply to mgmtd nodes, not to
 * CLI commands.  A mapping (from mgmtd operations to CLI operations)
 * is used to determine which CLI operations the ACL permits.  This is
 * done, rather than specifying CLI operations directly in the ACL,
 * to facilitate synchronization of CLI modules with related mgmtd
 * modules.
 *
 * There are two CLI operations:
 *
 *   - tao_cli_revmap: by default, permitted if tao_query_get is permitted
 *     by the ACL.  Use cli_acl_opers_revmap() to change this.
 *
 *   - tao_cli_exec: no default mapping.  Must use cli_acl_opers_exec()
 *     to declare a mapping.
 *
 * \param cmd The command to which the ACLs should be added.
 *
 * \param ... One or more parameters of type 'const ::ta_acl *'.  Note
 * that a NULL sentinel is NOT required, as it is added automatically
 * by the macro.  However, using one will not cause any harm.
 */
#define cli_acl_add(cmd, ...)                                                 \
    cli_acl_add_internal(cmd, false, __VA_ARGS__, NULL);


/* ------------------------------------------------------------------------- */
/** Add one or more local ACLs to this CLI command.
 *
 * Like cli_acl_add(), except the ACLs added through this function are 
 * considered local to the node on which they are registered, and are
 * completely removed from the ACL inheritance process.  This means two
 * things: (a) if this node is marked with ::ccf_acl_inheritable, the
 * local ACLs are not propagated downwards; and (b) if an ancestor of 
 * this node is marked with ::ccf_acl_inheritable, local ACLs do not
 * prevent us from inheriting from that node.
 *
 * The other way to think of it is that the whole inheritance process
 * is done during initialization without regard to local ACLs.  When
 * this is all finished, every node's local ACLs (if any) are moved
 * into its main ACL list.
 */
#define cli_acl_add_local(cmd, ...)                                           \
    cli_acl_add_internal(cmd, true, __VA_ARGS__, NULL);


/*
 * This is the INTERNAL function that adds ACL operation mappings; do not
 * call directly.  Call one of the wrapper macros, cli_acl_opers_exec()
 * or cli_acl_opers_revmap().
 *
 * The variable-argument list accepted is of type ta_oper_bf.  This
 * list is terminated by UINT32_MAX.  (We cannot use zero, because
 * we want to let them specify tao_none for when no operations are 
 * required.)  The macros for public usage automatically add the UINT32_MAX,
 * so the caller does not have to.
 */
int cli_acl_opers_internal(cli_command *cmd, ta_oper_bf cli_oper, ...);
                          

/* ------------------------------------------------------------------------- */
/** Specify one or more sets of operations, any one of which is sufficient 
 * in mapping to the "exec" operation for this command.  Each parameter 
 * after the 'cmd' structure is a bit field containing one or more 
 * operations.  If the ACL on this command permits the user to perform
 * all of the operations in at least one of these bit fields, the "exec"
 * operation will be permitted to the user.
 *
 * NOTE: these semantics mean that listing several operations together
 * in a single parameter is different from listing them separately.
 * e.g. cli_acl_opers_exec(tao_set_modify, tao_set_create, tao_set_delete)
 * means to permit execution if the user can do any one of (modify, create,
 * OR delete).  However, cli_acl_opers_exec(tao_set) means to permit 
 * execution if the user can do all of (modify, create, AND delete).
 *
 * NOTE: none of your operation bit fields may be zero.  To permit
 * "exec" access to all users, use the tacl_all_any ACL.  If you pass
 * zero for any of these parameters, it will not be detected as an 
 * error, but rather that parameter and any that follow will be ignored.
 * Your list of parameters does NOT have to be zero-terminted.
 */
#define cli_acl_opers_exec(cmd, ...)                                          \
    cli_acl_opers_internal(cmd, tao_cli_exec, __VA_ARGS__, UINT32_MAX);


/* ------------------------------------------------------------------------- */
/** Same as cli_acl_oper_exec(), except that it operates on the mapping
 * for the "revmap" operation on this command.
 */
#define cli_acl_opers_revmap(cmd, ...)                                        \
    cli_acl_opers_internal(cmd, tao_cli_remap, __VA_ARGS__, UINT32_MAX);


/** 
 * Command availability callback for ACLs.  A function to be called by
 * the infrastructure to determine if a command should be available to
 * the user for (a) execution, or (b) reverse-mappning.
 *
 * \param data The data passed by the module to cli_acl_set_callback()
 * when this function was registered.
 *
 * \param cmd The CLI command registration record.
 *
 * \param oper The CLI operation whose availability is to be determined.
 * This will be either ::tao_cli_exec or ::tao_cli_revmap.  Note that
 * you will only be called with ::tao_cli_revmap if an instance of this
 * command appears in reverse-mapping output.  Some commands, such as
 * "show" commands, never will.
 *
 * \param username The username of the current user, whose eligibility
 * is to be tested.
 *
 * \param user_auth_groups The set of authorization groups the current
 * user belongs to.  Each member of this array is an instance of 
 * the ::ta_auth_group enum.
 *
 * \retval ret_allow Return \c true if the operation should be permitted
 * to this user, or \c false if not.
 */
typedef int (*cli_acl_callback)(void *data, const cli_command *cmd,
                                ta_oper oper, const char *username,
                                uint32_array *user_auth_groups,
                                tbool *ret_allow);


/* ------------------------------------------------------------------------- */
/** Register a callback function to be called to determine
 * availability of this node, for either execution or reverse-mapping.
 * This is called INSTEAD of checking the ACLs on the node.  It
 * replaces the ACL check, but does NOT replace the check of the
 * required mode as registered by cli_acl_set_modes().
 */
int cli_acl_set_callback(cli_command *cmd, cli_acl_callback func, 
                         void *data);


/* ------------------------------------------------------------------------- */
/** Declare the set of modes this CLI command should be available in.
 */
int cli_acl_set_modes(cli_command *cmd, cli_mode_set modes);


/* ------------------------------------------------------------------------- */
/** Like cli_acl_set_modes(), except that the modes specified here are
 * not subject to inheritance.  Unlike local ACLs, these modes are not
 * merged with the regular modes after inheritance; rather, they replace
 * whatever modes remain after inheritance.
 */
int cli_acl_set_modes_local(cli_command *cmd, cli_mode_set modes);


/**
 * Opaque data structure for holding CLI-related ACL information.
 * Modules do not need to access the members of this structure.
 */
typedef struct cli_acl_data cli_acl_data;

/*
 * This is the INTERNAL function that checks ACL permissions; do NOT
 * call directly.  Call the wrapper macro, cli_acl_check().
 *
 * The variable-argument list accepted is of type 'const ta_acl *'.
 * This list MUST be terminated by NULL.  The purpose of the wrapper
 * macro is to add the NULL automatically, relieving the burden from
 * the developer.
 */
int cli_acl_check_internal(tbool *ret_permitted, ta_oper_bf opers, ...);


/* ------------------------------------------------------------------------- */
/** Check to see if the current user is permitted to perform the
 * specified operations, according to the specified ACL(s).
 *
 * \retval ret_permitted Returns \c true if the user is permitted to
 * perform ALL of the specified operations, or \c false if not.
 *
 * \param opers One or more operations, permission for which is to be
 * tested.
 *
 * \param ... One or more parameters of type 'const ::ta_acl *', which
 * are the ACLs to be used to test the current user's permissions to
 * perform the specified operation(s).  Note that a NULL sentinel is
 * NOT required, as it is added automatically by the macro.  However,
 * using one will not cause any harm.
 */
#define cli_acl_check(ret_permitted, opers, ...)                              \
    cli_acl_check_internal(ret_permitted, opers, __VA_ARGS__, NULL);


/* ------------------------------------------------------------------------- */
/** Query from mgmtd what auth groups this user has.  This is done
 * automatically during initialization, but can be done at runtime using
 * this API if the caller has reason to believe the answer may have
 * changed.
 */
int cli_acl_requery_authorization(void);


/*@}*/


/* ------------------------------------------------------------------------- */
/** CLI command structure.  For each node in the command tree (each unique
 * word in each command the user can type), call cli_register_command()
 * with a pointer to one of these structures.
 *
 * NOTE: all strings in the registration array are assumed to be owned
 * by the caller, and as such are not freed automatically when the
 * registration tree is freed.  This is thought to be fine for all
 * current usage patterns, as all strings in registration structures
 * passed by modules are currently string constants.  If a module
 * writer needs to dynamically construct any strings, he must keep
 * track of the pointers to memory allocated, and free them in the
 * module's deinit function; but more likely, the strings will have
 * to be constructed at runtime so they will be part of a callback
 * function and not part of registration at all.
 */
struct cli_command {

    /* --------------------------------------------------------------------- */
    /** CLI command words.  This specifies what words the user needs to
     * enter to invoke this command.  Each word may either be a string
     * literal or a wildcard.  A string literal contains [a..z],
     * [0..9], and hyphens, and is literally what the user must type
     * to match the command.  A wildcard is just a "*" character by
     * itself (defined as cli_wildcard_word above), and as far as the
     * framework is concerned matches any word.  The module may
     * register a completion handler (described below) that places
     * constraints on what words may be used in place of a wildcard.
     *
     * String literals are not case-sensitive.  To avoid confusion,
     * registering string literal command words with uppercase
     * characters causes an error.  Any string literals entered by the
     * user on the command line will be automatically downcased.
     * Words entered in place of wildcards are not altered by the
     * framework.
     *
     * The commands are stored in a tree with each node being a single
     * command word.  A single registration is a string of words, and
     * the other attributes in the registration are kept with the node
     * corresponding to the last word in the registration.  Thus a
     * registration of "show interfaces *" would ensure that the
     * "show" and "interfaces" nodes existed, and would add a node "*"
     * underneath, containing all of the attributes specified in the
     * registration.
     *
     * The command words are specified by filling in cc_words_str with
     * a single string containing all of the words, separated by
     * spaces.  e.g. "show interfaces *"
     *
     * The string "|" (defined by cli_var_args_word) may be used as a
     * word to designate the boundary between the fixed-length
     * parameters and a variable argument list.  See the ccf_var_args
     * flag for further explanation.
     *
     * The following describes a legacy mechanism for doing variable
     * argument lists.  It is left in for backward compatibility, but
     * should not be used in new code, as it is much clumsier than the
     * newer mechanism provided by the ccf_var_args flag.  See the 
     * commands around that flag for explanation.  The old mechanism
     * works as follows:
     *
     *    Any number of characters in the word string may be enclosed by
     *    square brackets.  This will cause the registration provided to
     *    be duplicated and registered twice: once with a string
     *    containing the substring in the square brackets, and once with
     *    a string with that substring removed.  Multiple sets of square
     *    brackets may be used, and all possible combinations will be
     *    registered.  Square brackets may be nested.  For example,
     *    registering the command string "foo [a *] [b *] c" is
     *    equivalent to the following four registrations: 
     *      - foo c 
     *      - foo a * c
     *      - foo b * c
     *      - foo a * b * c
     *    Note that it does not include "foo b * a * c", so this mechanism
     *    is not ideal for large lists of parameters that may occur in any
     *    order.
     *  
     *    Although this mechanism may be used on any substring, it is
     *    intended to be used only to enclose entire words, and not to be
     *    used on the last word in the string: since the registration
     *    structure is kept in the tree node of the last word in the
     *    string, it is unlikely that the same structure (particularly
     *    the help string) would be appropriate for the more than one
     *    word.
     */
    const char *cc_words_str;

    /* --------------------------------------------------------------------- */
    /** CLI command registration flags.  This specifies a variety of options
     * on the command.  See definition of cli_command_flags above for a
     * list of options.  Construct cc_flags by ORing together all of the
     * options you want.
     */
    cli_command_flags_bf cc_flags;

    /* --------------------------------------------------------------------- */
    /** An optional magic number that can be used any way the developer wants.
     * One possible usage is as a unique identifier for each command, which
     * could be useful if the same callback function is used for more than
     * one command, and wants to know which command it was called for
     * without having to do string compares on the cc_words_str field.
     */
    uint32 cc_magic;

    /* --------------------------------------------------------------------- */
    /** What capabilities must the user have, or not have, to execute
     * this CLI command?  This field as cc_capab_avoid is each
     * a bit field where each bit
     * represents one possible capability a user can obtain.  For a
     * command to be available, the user must have all of the
     * capabilities whose bits are set in cc_capab_required, and none of
     * the capabilities whose bits are set in cc_capab_avoid;
     *
     * These fields may be set on terminal commands; if not set, they
     * are assumed to be empty (i.e. impose no restrictions on
     * availability).
     *
     * Neither of these fields may be set on nonterminal commands.
     * The visibility of interior nodes (which are a superset of
     * nonterminal commands) during help and completion will be
     * determined automatically based on the terminal commands
     * registered beneath them so that a currently-available command
     * is never obscured by an ancestor not being visible.
     *
     * Note that this mechanism does not allow interior terminal
     * commands to become available for execution under situations
     * that their registration does not directly call for just because
     * child nodes register to be available in those situations.  The
     * upwards-inherited availability only applies to help and
     * completion, but execution is taken directly from the
     * registration alone.  Thus, to take a real-life example from the
     * Cisco command set, "enable" could be registered to require no
     * capabilities and to avoid A and B; "enable password *" could be
     * registered to require capability B; the second registration
     * means that "enable" will still be visible when the user has
     * capability B, but will not be executable under that condition.
     *
     * See definition of cli_capability type above for a list of
     * capability bits that can be ORed into each of these.
     */
    cli_capability_list cc_capab_required;

    /**
     * See comment for cc_capab_required.
     */
    cli_capability_list cc_capab_avoid;

    /**
     * If the rules to determine whether or not a user should be
     * allowed access to the command cannot be expressed in terms of
     * cc_capab_required and cc_capab_avoid, assign a custom callback
     * function to this field instead.  If this is specified, the
     * other two capability fields are ignored; this function is all
     * that determines whether the user is allowed to use the command.
     */
    cli_capability_callback cc_capab_callback;
    void *cc_capab_data;

    /* --------------------------------------------------------------------- */
    /** DO NOT SET THIS FIELD DIRECTLY.  This is for use by the CLI
     * infrastructure.
     *
     * If PROD_FEATURE_ACLS is defined, access to nodes is limited
     * according to the ACLs on the nodes, and the authorization
     * groups the user is in.  Each command is registered with an ACL
     * of its own, saying who can use the command (get help and
     * completion from it, and execute it); and who can reverse-map it
     * (see the parameters to the command in reverse-mapping output:
     * the "show configuration" command and its variants).
     *
     * This field contains the ACL and related information, but cannot
     * be set by the module directly.  Instead, modules should use the
     * following APIs to set ACLs on their command nodes:
     *   - cli_acl_add()
     *   - cli_acl_add_local()
     *   - cli_acl_opers_exec()
     *   - cli_acl_opers_revmap()
     *   - cli_acl_set_callback()
     *   - cli_acl_set_modes()
     *   - cli_acl_set_modes_local()
     *
     * Note that unlike capabilities, an ACL must be registered on
     * every node, whether terminal or nonterminal.  The
     * ::ccf_acl_inheritable flag may be useful in reducing the burden
     * of declaring ACLs for every node.
     */
    cli_acl_data *cc_acl_data;


    /* --------------------------------------------------------------------- */
    /** Specifies that this command node can only be matched if there
     * are exactly the specified number of prefixes already on the
     * stack.  Usually used only by the infrastructure except in the
     * case of the node with a "no" word immediately underneath a
     * prefix mode root.  See Samara Technical Guide section on CLI
     * prefix modes for further info on using this field.  Defaults
     * to -1, meaning no restriction.
     */
    int32 cc_req_prefix_count;
    
    /* --------------------------------------------------------------------- */
    /** For use with the ::ccf_normalize_value flag: tells us which
     * data type should be used to normalize the value entered.
     */
    bn_type cc_normalize_type;

    /* --------------------------------------------------------------------- */
    /** A callback to be called to determine if a command word is
     * "sensitive", meaning that only an obfuscated version of it
     * should be logged or entered into the command history.
     *
     * This is only applicable to nodes which are registered with the
     * ::ccf_sensitive_param flag.  Words matching such nodes are
     * assumed to be sensitive; but if this callback is provided, you
     * have a chance to examine the word and decide that it is not
     * actually sensitive.  An example would be a URL which may or may
     * not have a password embedded in it.
     *
     * Again, this is most likely only useful for wildcard nodes, but
     * it is permitted with literal nodes as well.
     */
    cli_sensitive_word_callback cc_sensitive_word_callback;
    void *cc_sensitive_word_data;

    /* --------------------------------------------------------------------- */
    /** @name CLI command help handler.
     * This specifies what should happen when the
     * user requests help on this command word.  Choose one of the following:
     *
     *   - If this is a string literal command word, fill in cc_help_descr.
     *
     *   - If this is a wildcard command word, set cc_help_use_comp to true.
     *     This will invoke the completion handler and use the results to
     *     make up a list of help choices.  If this option is used, nothing
     *     will show up in the righthand column of help to describe the
     *     meaning of each of the options.
     *
     *   - If this is a wildcard command word, fill in cc_help_options and
     *     cc_help_num_options.  
     *
     *   - In addition to the previous two options, if this is a
     *     wildcard command word, fill in cc_help_exp and optionally
     *     cc_help_exp_hint.  The former will show in the left column,
     *     and the latter will show in the right column of the help.
     *     Even if you are listing a set of options with one of the two
     *     previous methods, a textual description of what is being listed
     *     is often helpful.
     *
     *   - In addition to any of the above, if this is also a terminal
     *     command word, optionally fill in cc_help_term.  For terminal
     *     words, if cc_help_term is NULL or a non-empty string, "<cr>"
     *     will be shown in the left column of the help, and the contents
     *     of cc_help_term, if any, will be shown in the right column.
     *     If cc_help_term contains the empty string, this line of help
     *     is suppressed.
     *
     *   - If you filled in cc_help_term, and your command can be used to
     *     enter a prefix mode, fill in cc_help_term_prefix.  If prefixes
     *     are enabled, we will show this string for termination help;
     *     if they are disabled, we will show cc_help_term.
     *
     *   - If your node has both ccf_var_args and ccf_prefix_mode_root
     *     set, the termination help you specified for entering the
     *     mode will probably not be appropriate for the case where
     *     termination help is printed from the same node after one or
     *     more var arg parameters have been specified.  In this case,
     *     if one or more var arg parameters are specified, by default 
     *     the cc_help_term and cc_help_term_prefix fields are ignored
     *     and "<cr>" is printed on its own.  If you want some specific
     *     termination help printed here, assign a string to the
     *     cc_help_term_varargs field.
     *
     *   - Fill in cc_help_callback, and optionally cc_help_data.
     *     cc_help_data will be passed to the callback, along with the
     *     current command line, when help is requested on this word.
     *
     * (XXX/EMT: document cc_help_comma_delim)
     *
     * At least one of these options must be chosen.  A registration
     * that does not provide any command help will fail.
     *
     * The cc_gettext_domain field is only relevant if you are using
     * the I18N feature.  It tells libcli what what domain to use
     * (which string catalog to look in) when looking up the strings
     * in cc_help_descr, cc_help_exp, cc_help_exp_hint, and
     * cc_help_term.  It is automatically populated with the most
     * recent gettext domain passed to cli_set_gettext_domain().  You
     * may change it before registering your command if it needs to be
     * different, although this would be very unusual.  In general
     * your module should call cli_set_gettext_domain() once before
     * registering any commands, and then leave cc_gettext_domain at
     * its default for each command.
     */
    /*@{*/
    const char *cc_help_descr;
    const char *cc_help_exp;
    const char *cc_help_exp_hint;
    const char *cc_help_term;
    const char *cc_help_term_prefix;
    const char *cc_help_term_varargs;
    cli_expansion_option *cc_help_options;
    uint32 cc_help_num_options;
    tbool cc_help_comma_delim;
    tbool cc_help_use_comp;
#ifdef PROD_FEATURE_I18N_SUPPORT
    const char *cc_gettext_domain;
#endif /* PROD_FEATURE_I18N_SUPPORT */
    cli_help_callback cc_help_callback;
    void *cc_help_data;
    /*@}*/

    /* --------------------------------------------------------------------- */
    /** @name CLI command completion handler
     * This specifies what should happen when 
     * the user requests completion on this command word.  Choose one of the
     * following:
     *
     *   - If this is not a wildcard command word, all of these
     *     options must be left blank, as the framework handles
     *     completion for string literals.
     *
     *   - Set cc_comp_type to cct_matching_names, and fill in
     *     cc_comp_pattern using exactly one wildcard.  Completions will be
     *     the values that the wildcard takes on in nodes matched.
     *
     *   - Set cc_comp_type to cct_matching_values, and fill in
     *     cc_comp_pattern.  Completions will be the values of all
     *     nodes matched.
     *
     *   - Set cc_comp_type to cct_use_help_options.  This requires
     *     cc_help_options and cc_help_num_options to have been filled
     *     out.  The left column of the options in cc_help_options will
     *     be used as possible completions.
     *
     *   - Fill in cc_comp_callback, and optionally cc_comp_data.
     *     cc_comp_data will be passed to the callback, along with the
     *     current command line, when completion is requested.
     *
     * (XXX/EMT: document cc_comp_comma_delim)
     *
     * Providing completion behavior for wildcard command words is
     * optional.  If the universe of possible entries is not
     * reasonably enumerable (e.g. setting a "real name" on a user
     * account), completion is not possible.  The user-visible
     * behavior will be that the CLI will decline to offer any
     * completions, just as if there were no valid completions at all.
     */
    /*@{*/
    cli_completion_type cc_comp_type;
    const char *cc_comp_pattern;
    tbool cc_comp_comma_delim;
    cli_completion_callback cc_comp_callback;
    void *cc_comp_data;
    /*@}*/

    /* --------------------------------------------------------------------- */
    /** @name CLI command execution handler
     * This specifies what should happen when
     * the user executes this command.  Choose one of the following:
     *
     *   - If this is a nonterminal command, do not fill in any of these
     *     fields.
     *
     *   - To perform a set, create, delete, or reset of a single
     *     management binding, set cc_exec_operation and cc_exec_name.
     *     If the operation is a set, set cc_exec_type, and cc_exec_value.
     *     The node and value can all be parameterized by using
     *     $n$ to represent the nth parameter on the command line.  For
     *     example, the implementation of "interface * duplex *" might
     *     set "/interfaces/$1$/duplex" to the value "$2$".
     *
     *     If you are setting cc_exec_value, and cc_exec_type is a
     *     numeric type, you may optionally set
     *     cc_exec_value_multiplier to a number to be multiplied by
     *     any number the user enters before storing the value.
     *     This field defaults to 1.  e.g. if you want the user to 
     *     enter a value in minutes, but the node is in seconds, you
     *     would set cc_exec_value_multiplier to 60.
     *
     *     If you do not know the type of the binding to be set, but
     *     just wish to convert whatever string the user enters into
     *     whatever type the binding already had, specify bt_any for
     *     cc_exec_type.  Note that if the node does not already
     *     exist, this will fail.
     *
     *   - To perform an action, set cc_exec_operation and
     *     cc_exec_action_name.  If the action takes a binding with
     *     It, set cc_exec name, cc_exec_type, and cc_exec_value.
     *     The action name, binding name, and binding value can be
     *     parameterized using $n$ as described above.
     *
     *   - Fill in cc_exec_callback, and optionally cc_exec_data.
     *     cc_exec_data will be passed to the callback, along with the
     *     current command line, when the command is executed.
     *
     * Note: for actions or modifies that need two bindings,
     * cc_exec_name2 et al. are available.  This is not a generalized
     * solution but there are a lot of commands that need to set
     * exactly two bindings, while few need to set three or more.
     */
    /*@{*/
    cli_exec_operation cc_exec_operation;
    const char *cc_exec_name;
    bn_type cc_exec_type;
    uint32 cc_exec_type_flags;
    const char *cc_exec_value;
    float cc_exec_value_multiplier;
    const char *cc_exec_action_name;
    cli_execution_callback cc_exec_callback;
    void *cc_exec_data;
    const char *cc_exec_name2;
    bn_type cc_exec_type2;
    uint32 cc_exec_type_flags2;
    const char *cc_exec_value2;
    float cc_exec_value2_multiplier;
    /*@}*/

    /* --------------------------------------------------------------------- */
    /** CLI reverse mapping handler.  This specifies what should happen when
     * the user requests for a configuration database to be reverse mapped
     * into a list of CLI commands that could create it from an empty
     * configuration.  Choose one of the following:
     *
     *   - Do not fill in any fields.  The association of reverse-mapping
     *     handlers with specific command registrations is provided to
     *     help code expressiveness, to keep reverse-mapping of 
     *     commands next to their registrations, but the framework doesn't
     *     actually care which node a reverse-mapping handler is registered
     *     on.  They might as well all be kept in a single array separate
     *     from the command tree.
     *
     *   - Set cc_revmap_type to crt_auto.  This will do reverse-mapping
     *     for simple commands that set a single node using the standard
     *     execution handler.  cc_exec_operation must be set to cxo_set.
     *     A node-matching pattern will be derived from cc_exec_name by
     *     replacing all $n$ references with '*'.  Commands are generated
     *     in one of two ways:
     *
     *       - If cc_exec_value is in $n$ form, all nodes matching the
     *         pattern will be reverse-mapped.  The names and values of
     *         matched nodes will be back-substituted into cc_words_str
     *         in place of the wildcards to generate command
     *         invocations.  cc_exec_value_multiplier is taken into
     *         account if it is set.
     *
     *       - Otherwise, all nodes matching the literal in cc_exec_value 
     *         will be reverse-mapped.  This will most frequently be
     *         used for reverse-mapping commands for setting boolean 
     *         nodes, where the literal will be "true" or "false".
     *         In this case too, the names of nodes are substituted
     *         into cc_words_str to generate command invocations.
     *
     *   - Set cc_revmap_type to crt_manual, and fill in cc_revmap_names
     *     and cc_revmap_cmds.  cc_revmap_names should contain a 
     *     binding name pattern, optionally parameterized with '*',
     *     and cc_revmap_cmds should contain a CLI command, optionally
     *     parameterized with $n$ and $v$, to form a CLI command from
     *     each binding matched, using the name components and the 
     *     value of each binding.
     *
     *     NOTE: in most cases you should use crt_auto.  crt_manual is
     *     offered for cases where the processing function has extra
     *     to do, but the end result is a simple list of bindings.
     *     For example, the command for adding a name server does not
     *     use the standard execution handler because it needs to figure
     *     out the array index of the node to add.  But reverse-mapping
     *     is straightforward.
     *
     *   - Fill in cc_revmap_callback, and optionally cc_revmap_names
     *     and/or cc_revmap_data.  If cc_revmap_names has a node name
     *     pattern, your callback will be called once for each matching
     *     binding with the matching binding name as a parameter;
     *     otherwise, it will be called exactly once, with NULL as the
     *     binding name parameter.  cc_revmap_data will be passed to 
     *     the callback each time.
     *
     *   - Along with any of the options above, also set cc_revmap_order,
     *     and optionally cc_revmap_suborder, to an integer to determine
     *     where these results will be listed relative to other
     *     reverse-mapping results.  Results with lower order numbers
     *     will be listed first.  Each order number defaults to 0.
     *
     *     cc_revmap_order is the primary sort criteria, and each group
     *     of commands with the same order will be preceded with a 
     *     three-line header (using '#' at the beginning of each line, to
     *     make it a comment) containing a title derived from the
     *     cli_revmap_order_value_map array in climod_common.h.
     *     cli_revmap_suborder is the secondary sort criteria, and
     *     within each suborder group, the sort is alphabetical.
     *
     *     These two ordering fields meet two different needs.
     *     cc_revmap_order is used to group logically similar commands
     *     together under a common heading in the user-visible output.
     *     cc_revmap_suborder is used to control ordering within a
     *     logically similar group.
     *
     *     Note that cli_revmap_register_block_sort() is also available
     *     as a tool to control ordering of commands within a block
     *     (where a "block" is a group of commands with the same
     *     cc_revmap_order).  Using this function means that the
     *     cc_revmap_suborder of all commands in that block will be
     *     ignored, as responsibility for sorting is passed to your
     *     custom function instead.
     */
    /*@{*/
    cli_revmap_type cc_revmap_type;
    int32 cc_revmap_order;
    int32 cc_revmap_suborder;
    const char *cc_revmap_names;
    const char *cc_revmap_cmds;
    cli_revmap_callback cc_revmap_callback;
    void *cc_revmap_data;
    /*@}*/

    /* --------------------------------------------------------------------- */
    /** @name Fields for variable argument lists
     *
     * These fields pertain only to command nodes involved in a 
     * variable argument list (where the ccf_var_args flag is used).
     * Unless otherwise specified, these fields all are to be used
     * only on a node which is a direct child of the varargs root.
     * In other words, the root node of one of the optional arguments.
     */
    /*@{*/

    /* --------------------------------------------------------------------- */
    /** What position on the command line is this word required to be
     * in, if it is used?  Counting starts from 1.  For example, if
     * you have the command "a * [b *] [c *] [d *]" where 'c' and 'd'
     * may come in any order, but if 'b' is going to be specified, it
     * must come immediately after 'a *'.  So on the 'b' node
     * (cc_words_str would be "a * | b"), set cc_req_pos to 3.
     *
     * If this field is any negative number (it defaults to -1), 
     * no constraints are placed on the ordering of the parameters.
     *
     * Use of this facility is NOT RECOMMENDED if you can avoid it, as
     * it leads to some potentially confusing UI.  It is difficult to
     * convey this constraint in the online help.
     */
    int32 cc_req_pos;

    /* --------------------------------------------------------------------- */
    /** Parameter ID, for use with specifying interdependencies
     * between arguments using cc_va_prereq_param_ids_or and
     * cc_va_mutex_param_ids.  The namespace for these is per varargs
     * root, so you don't have to coordinate them across modules, or
     * even across varargs roots within a module.
     *
     * This field is initialized to -1, meaning the argument is not a
     * parameter we care to track.  Valid parameter numbers are
     * nonnegative.
     */
    int32 cc_va_param_id;

    /* --------------------------------------------------------------------- */
    /** List of parameter IDs that are prerequisites for this
     * argument.  The "_or" at the end of the name signifies that if
     * there are multiple parameter IDs listed here, only one of them
     * will be required.  So if none of these parameter IDs have yet
     * been specified on the command line, this one may not be used.
     *
     * This field is initialized with NULL, not an empty array.  One
     * quick way to populate it would be with uint32_array_new_va().
     *
     * IMPORTANT NOTE: if you use uint32_array_new_va() to populate this,
     * don't forget to pass -1 as your last parameter!  
     */
    uint32_array *cc_va_prereq_param_ids_or;

    /* --------------------------------------------------------------------- */
    /** Shortcut for cc_va_prereq_param_ids_or if you only have one.
     * Initialized to -1, meaning no constraint.
     */
    int32 cc_va_prereq_param_id;
    
    /* --------------------------------------------------------------------- */
    /** List of parameter IDs that are mutually exclusive with this
     * argument.  If any of these parameter IDs have already been
     * specified on the command line, this one may not be used.
     *
     * This field is initialized with NULL, not an empty array.  One
     * quick way to populate it would be with uint32_array_new_va().
     *
     * IMPORTANT NOTE: if you use uint32_array_new_va() to populate this,
     * don't forget to pass -1 as your last parameter!  
     */
    uint32_array *cc_va_mutex_param_ids;

    /* --------------------------------------------------------------------- */
    /** Shortcut for cc_va_mutex_param_ids if you only have one.
     * Initialized to -1, meaning no constraint.
     */
    int32 cc_va_mutex_param_id;

    /*@}*/

    /* --------------------------------------------------------------------- */
    /** For framework use only.  Command modules must not read or write
     * this field.
     */
    void *cc_opaque;
};

/* ----------------------------------------------------------------------------
 * See cli_TEMPLATE_cmds.c for a template to use when registering
 * commands.
 */

/* ------------------------------------------------------------------------- */
/** A pointer to an instance of this structure is passed to every CLI
 * module's initialization function via the DSO library's void *data
 * parameter.
 */
typedef struct cli_module_context {
    /**
     * Is the management daemon available?  The command module may wish
     * to behave differently if it is not, e.g. not registering commands
     * which require the management daemon to execute.
     *
     * This approach was taken instead of a ccf_mgmt_required flag
     * because some modules have more complex requirements than just
     * having their commands appear or disappear.  e.g. any that want
     * to make queries during initialization would refrain.  Also one
     * possible design is to have the 'shell' and similar commands
     * generally only available from enable mode, but to let them be
     * used from standard mode if the management daemon is
     * unavailable, to facilitate troubleshooting of the system.
     * (On the other hand, it is not desirable for the system to let
     * some of its defenses down when this daemon is having trouble.)
     */
    tbool cmc_mgmt_avail;
} cli_module_context;

/* ========================================================================= */
/** @name Command Registration and Override API
 */
/*@{*/

/* ------------------------------------------------------------------------- */
/** Create a new CLI command registration structure.  All fields are
 * initialized to their defaults (mostly NULLs and zeros) so you only
 * have to fill in the fields you need to change.
 */
int cli_new_command(cli_command **ret_command);

/* ------------------------------------------------------------------------- */
/** Register a command with the CLI infrastructure.  Libcli takes
 * ownership of the cli_command structure and sets the caller's
 * pointer to NULL.  The caller should not should not free it, pass a
 * pointer to a stack variable, or overwrite it and re-use it for
 * other command registrations.
 */
int cli_register_command(cli_command **inout_command);


#define CLI_CMD_NEW                                                           \
                            err = cli_new_command(&cmd);                      \
                            bail_error_null(err, cmd);

#define CLI_CMD_REGISTER                                                      \
                            err = cli_register_command(&cmd);                 \
                            bail_error(err);

/* ------------------------------------------------------------------------- */
/** Unregister a branch of the command tree.  This will generally be
 * done by a module other than the one that registered them, and which
 * has declared an init ordering constant to cause it to init after
 * the other module.  Thus it can be used to suppress commands you
 * don't want exposed.
 *
 * The words_str parameter takes the same form as a cc_words_str field
 * in a cli_command structure.  It names a node in the command tree;
 * this node and all of its descendants will be removed from the tree.
 */
int cli_unregister_command(const char *words_str);


/* ------------------------------------------------------------------------- */
/** Get the registration structure for a specified command.  You may
 * modify fields of the command, except for cc_words_str (which is
 * inherent in its position in the tree) and cc_opaque (as always).
 * The command will remain registered, and you do not own its memory
 * and so may not free it.  Since you are editing the version that is
 * already in the tree, you do not need to register the command again
 * when you're done.
 *
 * Note: if you want to make a command hidden or unavailable, please
 * consider using cli_hide_unhide_commands() or
 * cli_set_command_options() instead.
 */
int cli_get_command_registration(const char *words_str,
                                 cli_command **ret_cmd_reg);


/* ------------------------------------------------------------------------- */
/** Suppress a collection of CLI commands, presumably ones registered
 * by a different module.  You'll need to use the plug-in module
 * ordering constant to ensure that your module is initialized after
 * the one(s) that registered the commands you are suppressing.
 *
 * \param cmd_names An array of strings, each of which represents 
 * a command to suppress.  You do not need to list any commands 
 * underneath those already suppressed; naming one command node 
 * here suppresses it and anything underneath it.
 *
 * \param num_cmds Tells how many strings are to be found in
 * cmd_names.  Should be computed using sizeof() in the case of 
 * statically-initialized arrays, to avoid errors.
 * 
 * \param unregister Selects the method of suppression.  The default
 * behavior, if this is set 'false' is to hide the commands named.  We
 * only set the hidden bit on each of the nodes named, not any of
 * their children.  The philosophy on hiding is that if the subtree
 * root is hidden, there is no need to make the user's life harder by
 * hiding all of its children as well: once they get past the subtree
 * root, everything else is visible.  If this is set to 'true', the
 * commands will actually be deleted from the registration tree and
 * will be completely unavailable to the user.  In this case, we do of
 * course delete both the node named and all of its descendants.
 */
int cli_suppress_commands(const char *cmd_names[], uint32 num_cmds,
                          tbool unregister);

/* ------------------------------------------------------------------------- */
/** Hide or unhide a group of commands.  Normally used on commands
 * that only want to be visible under certain conditions, such as when
 * a license authorizing their use is installed.
 *
 * \param cmd_names An array of strings, each of which represents the
 * cc_words_str field of a command that has previously been
 * registered.
 *
 * \param num_cmds Tells how many strings are to be found in
 * cmd_names.  Should be computed using sizeof() in the case of 
 * statically-initialized arrays, to avoid errors.
 * 
 * \param hide Pass \c true to hide the commands, and \c false to
 * unhide them.  If \c true is passed, this is the same as calling
 * cli_suppress_commands() with \c false as its last parameter.
 */
int cli_hide_unhide_commands(const char *cmd_names[], uint32 num_cmds,
                             tbool hide);

/* ------------------------------------------------------------------------- */
/** Option flags for use with cli_set_command_options().
 */
typedef enum {

    cscf_none = 0,

    /**
     * Hide or unhide the command, by setting or clearing the
     * ccf_hidden flag.  This has the same effect as
     * cli_hide_unhide_commands() would.  The "set" flag to
     * cli_set_command_options() means to hide the command.
     */
    cscf_hidden = 1 << 0,

    /**
     * The opposite of ::cscf_hidden: hide the command iff "set"
     * flag is false.
     */
    cscf_visible = 1 << 1,

    /**
     * Make the command unavailable, or available again, by setting or
     * clearing the ccf_unavailable flag.  The "set" flag to
     * cli_set_command_options() means to make the command unavailable.
     *
     * NOTE: use this flag with caution, since it will (a) make any
     * attempts to execute the command fail; and (b) still allow the
     * command to be reverse-mapped.  In other words, invocations of
     * the command may still show up in "show config", but then those
     * invocations would fail to execute on this system, or any other
     * that has the commands unavailable.  For this reason, it is often
     * considered preferable to hide commands rather than make them
     * unavailable.
     */
    cscf_unavailable = 1 << 2,

    /**
     * The opposite of ::cscf_unavailable: make the command unavailable
     * iff "set" flag is false.
     */
    cscf_available = 1 << 3,

} cli_set_command_flags;

typedef uint32 cli_set_command_flags_bf;

/* ------------------------------------------------------------------------- */
/** Option structure for use with cli_set_command_options().
 */
typedef struct cli_set_command_options_rec {
    const char *csco_cmd_words;
    cli_set_command_flags_bf csco_flags;
} cli_set_command_options_rec;

/* ------------------------------------------------------------------------- */
/** Set or clear options on an array of CLI command registrations.
 *
 * This is similar to cli_hide_unhide_commands(), except it provides
 * more flexibility, allowing commands to be made unavailable instead
 * of hidden, and allowing a different fate to be selected for each
 * command.
 *
 * \param cmd_opts Options array, with one record per CLI command
 * registration to set.  Each record tells what is to be done to that
 * particular registration.
 *
 * \param num_cmds Number of entries in the 'opts' array.
 * Typically should be computed with:
 * sizeof(arr) / sizeof(cli_set_command_options_rec)
 *
 * \param set Whether to set or clear the option requested for each
 * command.  The individual option flags documented above specify
 * what "set" or "unset"/"clear" means for each.
 */
int cli_set_command_options(const cli_set_command_options_rec cmd_opts[],
                            uint32 num_cmds, tbool set);


/*@}*/

/* ============================================================================
 * Help and completion APIs
 * ============================================================================
 */

/**
 * Help flags.
 */
typedef enum {
    chf_none =                0,

    /**
     * Applicable to expansion help.  Don't add the specified string
     * if the word we're getting help on is not a prefix of it.
     * We don't do this by default, leaving it up to the module to
     * check it, possibly for efficiency, or possibly because it wants
     * to include a special string like "<hostname>", which doesn't 
     * care about prefixes.
     */
    chf_check_prefix =   1 << 0,
} cli_help_flags;

/* ------------------------------------------------------------------------- */
/** Call this from a help callback when description help is requested
 * to provide a description for the specified command.  'context'
 * should be the context parameter passed to the help callback.
 */
int cli_add_description_help(void *context, const char *description);

/* ------------------------------------------------------------------------- */
/** Call this from a help callback when expansion help is requested to
 * provide one option for the wildcard.  The 'word' field is
 * mandatory, but the 'description' field is optional.  'context'
 * should be the context parameter passed to the help callback.
 *
 * The _ex variant takes a bit field of flags, which can specify
 * additional options.
 */
int cli_add_expansion_help(void *context, const char *word,
                           const char *description);

int cli_add_expansion_help_ex(void *context, const char *word,
                              const char *description, cli_help_flags flags);

/* ------------------------------------------------------------------------- */
/** Call this from a help callback when termination help is requested
 * to provide a description of what hitting Enter on this command will
 * do.  Normally this is only required for interior terminal nodes,
 * where the description is necessary as a counterpoint to the other
 * possible commands listed alongside "<cr>".  For a leaf node, there
 * are no other options, so no explanation is needed.
 *
 * If 'description' is NULL, or if this function is not called at all
 * even though termination help was requested, "<cr>" will be used
 * with no description.  If 'description' is the empty string,
 * termination help will be suppressed altogether.
 *
 * 'context' should be the context parameter passed to the help
 * callback.
 */
int cli_add_termination_help(void *context, const char *description);


/* ========================================================================= */
/** @name Interacting with the management daemon
 * Functions and definitions to help CLI modules send set and query
 * requests to the management daemon, and process the responses.
 */
/*@{*/

/**
 * Context pointer for using mdc_...() API.
 */
extern md_client_context *cli_mcc;

/* ------------------------------------------------------------------------- */
/** Send a request to the management daemon and wait for a response.
 * This is the most basic interface atop which all of the other
 * functions in this group are built.
 *
 * \param request The binding node request; should be constructed using
 * the GCL API, with \c cli_config_session as the GCL session.
 *
 * \retval ret_response The response from the management daemon, if
 * the message exchange succeeded.
 *
 * \return An error is returned only if the communication with the
 * management daemon failed.  If the communication succeeded but an
 * error was returned by the management daemon, the function returns
 * success; the error code is held in the response structure.
 */
int cli_send_mgmt_msg_basic(bn_request *request, bn_response **ret_response);


/* ------------------------------------------------------------------------- */
/** A thin wrapper around cli_send_mgmt_msg_basic() which automatically
 * prints any response that comes back.  This is the callback passed
 * to the md_client library for the CLI, so anyone using the md_client
 * functions should realize that the reply that comes back from the
 * requests the call generates will be automatically printed.
 */
int cli_send_mgmt_msg_print_reply(bn_request *request,
                                  bn_response **ret_response);


/* ------------------------------------------------------------------------- */
/** Substitute params in for $n$ notation.
 *
 * (XXX/EMT document this in some centralized place, since there are 
 * lots of contexts where this notation is allowed.)
 */
int cli_set_binding_subst(uint32 *ret_code, bn_set_subop set_subop, 
                          const tstr_array *params, bn_type binding_type,
                          const char *binding_value,
                          const char *binding_name);


/* ------------------------------------------------------------------------- */
/** Print a string which is parameterized with names of management
 * bindings to be queried and substituted.  The '#' sign delimits
 * names of bindings whose value is to be printed; anything surrounded
 * by a '#' on each side is treated as the name of a binding which
 * should be queried, and its value substituted into the string.
 *
 * The binding name may optionally be preceded by one or more
 * formatting directives, which are separated from each other and from
 * the binding name by tilde ('~') characters.  Each directive begins
 * with one letter to say what kind of directive it is.  Then, if there
 * is a parameter to the directive (as all of the current ones have),
 * this is followed by a colon (':') for readability, and then the
 * argument.  The directives are:
 *
 *   - W:NUM  Truncate or pad the binding value to be exactly a specified
 *     of characters.  e.g. w:20 means a 20-character field width.
 *
 *   - w:NUM  Same as 'W' directive, except we never truncate, only pad if
 *     necessary.  This is more like printf's field width option.
 *
 *   - j:{left/right}  In conjunction with 'w' directive, specify 
 *     field justification, if the value needs to be padded.  The default
 *     is "left".  This is ignored if 'w' is not specified.
 *
 *   - e:STRING  Specify a string to be rendered in lieu of the binding
 *     value, if the binding cannot be queried (i.e. we get back no value
 *     at all).  By default this is the empty string.  Note that this will
 *     NOT be used if we get back a binding whose string rendering is the
 *     empty string; 'v' and 'm' must be used for that.
 *
 *   - v:VALUE  Specify a special value, such that if the binding has this
 *     value, the string specified with the 'm' directive is printed instead.
 *     (Or if 'm' is omitted, nothing is printed.)  Often used with -1, 0,
 *     "", etc., which need a special description.  Note that the empty 
 *     string "" will match either a binding containing the empty string,
 *     or the case where the binding did not exist at all.
 *
 *   - m:STRING  Specify a message to be printed if the binding's value
 *     matches the string specified with the 'v' directive.  If 'v' is 
 *     used but 'm' is omitted, the latter is implicitly assumed to be 
 *     the empty string.
 *
 *   - d:NUM  For numeric bindings only.  Divide the number in the binding
 *     by this number, in floating point.  Often used with the 'f' directive
 *     to format the resulting floating point number.
 *
 *   - f:STRING  Format string to use to render a numeric binding.
 *     If the binding's data type was floating point, or if you used
 *     the 'd' directive (the division is done in floating point),
 *     it will be rendered as a float64.  Otherwise, the binding will
 *     be rendered as a 64-bit integer, signed or unsigned according
 *     to the original type.  You should specify everything after the
 *     '%%' sign. e.g. to render a floating point number in a 6-wide
 *     field, padded with 0s, with 2 digits after the decimal point,
 *     your directive would be "f:06.2f".  Or to render an integer
 *     in hexadecimal with no padding, you would use "f:x".

 *   - p:STRING  Specify a prefix to be prepended to the value.  This can
 *     be more useful than putting the prefix in the format string, for
 *     purposes of truncation and padding, e.g. if you want to parenthesize
 *     something.
 *
 *   - s:STRING  Specify a suffix to be appended to the value.
 *
 *   - k:{true,false}  Should we keep the prefix and suffix even if the
 *     value matches the "special value" specified by the 'v' directive?
 *     The default is 'true'; if 'false', we discard the prefix and suffix
 *     if the value matches the 'v' directive's value.  Only applicable
 *     if 'v' directive is used (possibly along with 'm'), in conjunction
 *     with 'p' and/or 's'.
 *
 *   - b:BINDING_NAME  Specify the name of the binding to be queried
 *     and printed.  Using this should be RARE, because normally the 
 *     binding name comes by itself, after all the directives.  This
 *     directive should be used LAST, and INSTEAD OF a binding name
 *     in the normal fashion.  The only interesting use of it is for
 *     binding names that are either a single character, or have a 
 *     colon as the second character.  That would cause them to be 
 *     mistaken for directives, so you would disambiguate using this
 *     directive.  These are expected to be rare cases.
 *
 *   - n:STRING  Format the value "nicely".  The interpretation of this
 *     is be different for different data types.  Currently the only data
 *     types for which this is supported are:
 *       - Time durations.  In this case the format string is ignored.
 *       - Datetimes.  In this case the format string is a comma-separated 
 *         list of arguments, but currently the only argument supported is 
 *         "r", meaning to append a description of the time relative to
 *         the current time.  e.g. if the datetime is 53 seconds in the
 *         future of the current time, it would append " (in 53 seconds)".
 *         If it was 2 minutes in the past, it would append 
 *         " (2 minutes ago)".  If they match exactly, it would append 
 *         " (now)".
 *       - Booleans (format string tells us which pair of words to use
 *         instead of "yes" and "no".  Currently the only format string
 *         supported is "enable", which means to return "enabled" and 
 *         "disabled".)
 *
 * For example, if you wanted to render a number contained in the
 * "/one/two/three" binding in an 12-character field, right justified,
 * and print "use default" if the number was -1, you'd do:
 * "#w:12~j:right~v:-1~m:use default~/one/two/three#"
 *
 * If you need any '~' characters in your string directives ('e', 'v',
 * or 'm'), escape them with a preceding backslash.
 *
 * The binding name MUST be the last part of the string inside '#'
 * characters; i.e. no directives can come after it.
 *
 * The printf-style format string and the variable-length list of
 * arguments are rendered into a single string first, then the
 * substitution is performed.  So for example, you could display the
 * interface duplex like this: cli_printf_queried("The duplex is
 * #/net/interface/config/%s/duplex#\n", itf);
 */
int cli_printf_query(const char *format, ...)
     __attribute__ ((format (printf, 1, 2)));

     
/* ------------------------------------------------------------------------- */
/** Same as cli_printf_queried() except that it also takes a bn_binding_array
 * which is expected to already contain all of the bindings referenced.
 */
int cli_printf_prequeried(const bn_binding_array *bindings,
                          const char *format, ...)
     __attribute__ ((format (printf, 2, 3)));


/* ------------------------------------------------------------------------- */
/** A combination of mdc_foreach_binding() and cli_printf_query().
 * Iterates over the set of nodes specified, printing a parameterized
 * string for each node found.  The string may contain references to
 * nodes in the management hierarchy, delimited using '#', and their
 * names may be parameterized according to the current node matched,
 * using '$'.
 *
 * \li $[1..n]$ The nth component of the name of the binding matched
 *
 * \li ^[1..n]^ The nth component of the name of the binding matched,
 * escaped for direct insertion into a full binding name.  See example
 * below for usage.
 *
 * \li $i$ The index number of the iteration we're on, starting at 1.
 * 
 * \li $v$ The value of the binding matched
 *
 * \li $n$ The full name of the binding matched.
 *
 * For example, the following would print the IP addresses of all of
 * the interfaces:
 * cli_printf_foreach_query("/net/interface/ *", NULL, NULL, 
 *     "Interface $3$: IP address #$n$/ipv4/static/1/ip#\n");
 * (without the space before the asterisk)
 *
 * or alternately:
 *
 * cli_printf_foreach_query("/net/interface/ *", NULL, NULL, 
 *     "Interface $3$: IP address #/net/interface/^3^/ipv4/static/1/ip#\n");
 *
 * First all '$' and '^' substitutions are performed, then '#' queries
 * and substitutions follow.
 *
 * NOTE: references to bindings outside the subtree spanned by
 * the binding specification will fail, rather than resulting in
 * supplemental queries.
 *
 * \param binding_spec A wildcarded binding specification.
 *
 * \param header A string to be printed first if and only if there is
 * at least one match.
 *
 * \param no_match A string to be printed if there are no matches.
 *
 * \param format A printf-style format string, followed by a
 * variable-length list of parameters.  Note that this is rendered
 * into a single string only once.
 *
 * \retval ret_num_matches The number of bindings matched, and hence the
 * number of instances of the string provided that were printed.
 */
int cli_printf_foreach_query(const char *binding_spec, const char *header,
                             const char *no_match, uint32 *ret_num_matches, 
                             const char *format, ...)
     __attribute__ ((format (printf, 5, 6)));

/* ------------------------------------------------------------------------- */
/** Same as cli_printf_foreach_query() except that rather than making a 
 * query to the management daemon, we try to fulfill references to 
 * binding nodes from the response structure provided.
 *
 * NOTE: references to bindings outside the set provided in the
 * bn_binding_array will fail, rather than result in a supplemental
 * query.
 */
int cli_printf_foreach_prequeried(const bn_binding_array *bindings, 
                                  const char *binding_spec,
                                  const char *header,
                                  const char *no_match, 
                                  uint32 *ret_num_matches,
                                  const char *format, ...)
     __attribute__ ((format (printf, 6, 7)));

/*@}*/


/* ========================================================================= */
/** @name Reverse mapping
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** Get the output of a "show configuration"-style command: the result of
 * reverse-mapping a configuration database.
 *
 * Only the nodes provided in the bindings array are reverse-mapped.
 *
 * The results are returned in a newly-allocated string array, where
 * each string contains a single line of the response.  Each line is
 * usually a single command, except in cases where special facilities
 * are used to make single commands span multiple lines.  In this
 * case, the command may be spread across several entries in the
 * string array.
 */
typedef enum {
    cre_none = 0,

    /**
     * Do not include any commands that set things to their defaults.
     */
    cre_exclude_defaults = 0x01,

    /**
     * Do not include any hidden commands that set things to their defaults.
     */
    cre_exclude_hidden_defaults = 0x02,

    /**
     * Do not include instances of hidden commands.
     */
    cre_exclude_hidden = 0x04,

    /**
     * Include all commands.
     */
    cre_all = 0x08
} cli_revmap_exclusion;


/* ------------------------------------------------------------------------- */
/** Response structure for cli_revmap_db().
 */
typedef struct cli_revmap_db_result {
    
    /**
     * Array of commands generated by reverse-mapping.
     */
    tstr_array *crdr_commands;

    /**
     * Number of commands filtered out of crdr_commands because the
     * user does not have the permissions to view them.
     *
     * NOTE: currently, filtering is only done under PROD_FEATURE_ACLS,
     * so under PROD_FEATURE_CAPABS this will always be zero.
     */
    uint32 crdr_num_cmds_filtered_perms;

} cli_revmap_db_result;

int cli_revmap_db_result_free(cli_revmap_db_result **inout_result);

int cli_revmap_db(bn_binding_array *bindings, cli_revmap_exclusion excl,
                  cli_revmap_db_result **ret_result);


/* ------------------------------------------------------------------------- */
/** Give the CLI names of bindings that should be consumed by this
 * module without producing CLI commands during reverse mapping.
 *
 * The binding name may be parameterized using '*' as a name component
 * to match any string for that component, and '**' as the last name
 * component to match names with any zero or more components at that
 * slot and beyond.
 *
 * "Ignoring" bindings means removing them from the database after all
 * of the reverse-mapping functions have run.  The goal is to prevent
 * errors from begin logged, and "internal set" commands from being
 * emitted to create these bindings.  It is generally used to consume
 * bindings that (a) have no relevance on their own because they are
 * parents of bindings that do; or (b) represent configuration which
 * is not exposed through the CLI.
 *
 * "Hiding" bindings means they are removed from the database before
 * any modules see them for reverse-mapping.  The goal here is to
 * prevent a module from reverse-mapping a binding because the result
 * would be misleading, wrong, or otherwise undesirable.  For example,
 * xinetd's side effects function enables or disabled PM launching of
 * xinetd according to whether or not any xinetd features (telnetd,
 * ftpd) are enabled.  We don't want to see the PM command to enable
 * or disable xinetd, since this configuration is already set
 * correctly by the side effects function.
 *
 * When in doubt, use the "ignore" functions rather than "hide".
 * Sometimes it is important to not remove bindings until after
 * reverse-mapping functions have run.  For example, the interface
 * module uses the "/net/interface/config/ *" bindings to iterate over,
 * but then these bindings need to be consumed later because they have
 * no configuration of their own.
 *
 * The ..._arr() variant takes a NULL-terminated array of strings.
 *
 * (XXX/EMT: break these apart into separate comments per function, for
 * better Doxygen readability.)
 */
int cli_revmap_hide_bindings(const char *name_pattern);

int cli_revmap_ignore_bindings(const char *name_pattern);

int cli_revmap_ignore_bindings_va(uint16 num_names,
                                  const char *name_pattern_1, ...);

int cli_revmap_ignore_bindings_arr(const char *name_patterns[]);

/**
 * Declare a list of binding patterns (as described in comment for
 * cli_revmap_ignore_bindings_va above) as "internal", meaning that
 * the CLI module may not reverse-map these, and that we should emit
 * "internal set ..." commands for them without complaining.
 * This only differs from our normal behavior in that we will not
 * complain about them.
 *
 * This is intended for mgmtd nodes for which there are no CLI
 * commands to set them.  Previously, CLI modules would generally tell
 * the CLI infrastructure to ignore these nodes, and as a result, the
 * reverse-mapping results did not indicate if these nodes had
 * different values (generally as a result of an "internal set ..."
 * command being done by the user).
 */
int cli_revmap_internal_bindings_va(uint16 num_names,
                                    const char *name_pattern_1, ...);

/**
 * Tell the CLI infrastructure to use an advanced query to exclude a
 * set of bindings from the results that are returned to us for
 * reverse-mapping.  This has a similar effect to
 * cli_revmap_hide_bindings(), in that no modules will see the nodes
 * that are excluded in this manner.  But while that function gets the
 * bindings back in the query results and then throws them away, this
 * function tells mgmtd it does not want the bindings, and so effort
 * is not expended in putting them into the response, sending them to
 * us, deserializing them, copying them into data structures, and then
 * immediately freeing them.  So this is more efficient when large
 * numbers of bindings are involved, though it does not take arbitrary
 * binding name patterns.
 *
 * \param wild_reg_name Wildcard node registration name whose
 * instances are to be suppressed.  For example, if you did not want
 * to get back anything underneath /auth/passwd/user, you would pass
 * "/auth/passwd/user/ *" here (without the space in front of the '*').
 */
int cli_revmap_exclude_bindings(const char *wild_reg_name);


typedef struct cli_revmap_cmd {
    tstring *crcm_cmd;
    int32 crcm_suborder;
    tbool crcm_hidden;
} cli_revmap_cmd;

/* ------------------------------------------------------------------------- */
/** Override the default sorting behavior for CLI commands within a
 * specified command block.  There is one command block for each
 * cc_revmap_order reverse-mapping ordering constant returned by
 * modules: a single call to this function will change how all
 * commands with a specified ordering constant are sorted relative to
 * each other.  Note that this will cause the cc_revmap_suborder 
 * field to be ignored for commands in this block.
 *
 * The default sort is by cc_revmap_suborder, and then an alphabetic
 * sort which ignores a leading "no " if there is one.  A custom
 * comparison function may be provided, or if NULL is specified, the
 * specified block of commands are not sorted at all.
 *
 * The compare function is passed a pointer to a pointer to a structure
 * of type cli_revmap_cmd (see above).  See cli_revmap_cmd_compare()
 * in cli_revmap.c for a sample compare function (this is the default one).
 */
int cli_revmap_register_block_sort(int32 revmap_order,
                                   array_elem_compare_func compare_func);


/* ------------------------------------------------------------------------- */
/** Helper function for reverse-mapping.
 *
 * XXX/EMT: doc.
 */
int cli_revmap_numbered_command(const bn_binding_array *bindings,
                                const char *bname_pattern,
                                uint32 fixed_word_idx, uint32 num_word_idx,
                                const char *fmt_prefix,
                                const char *fmt_suffix,
                                cli_revmap_reply *ret_reply);

/* ------------------------------------------------------------------------- */
/** Helper function for reverse-mapping: construct a node name from a 
 * printf-style format string and arguments, append the node name to the
 * array of nodes we are consuming, fetch the value of the node, and 
 * return it.
 */
int cli_revmap_consume_binding(const bn_binding_array *bindings,
                               cli_revmap_reply *inout_reply,
                               tstring **ret_value, const char *fmt, ...)
    __attribute__ ((format (printf, 4, 5)));


/*@}*/


/* ============================================================================
 * Other module APIs
 * ============================================================================
 */


#ifdef PROD_FEATURE_CAPABS

/* ------------------------------------------------------------------------- */
/** Tell libcli what capabilities the session has so the appropriate
 * set of commands can be made available.  Before any of these are
 * called, the user is assumed to have no capabilities.
 *
 * cli_set_capabilities() enables or disables whichever capabilities
 * are set in the list provided, according to the 'have' parameter.
 *
 * cli_replace_capabilities() makes the current capability list match
 * the provided one exactly.
 */
int cli_set_capabilities(cli_capability_list capabs, tbool have);
int cli_replace_capabilities(cli_capability_list capabs);

/* ------------------------------------------------------------------------- */
/** Retrieve the session's current set of capabilities, or query
 * whether or not the session has all of a specific set of capabilities.
 */
cli_capability_list cli_get_capabilities(void);
tbool cli_have_capabilities(cli_capability_list capab);

#endif /* PROD_FEATURE_CAPABS */

/* ------------------------------------------------------------------------- */
/** Print a string.  This is sent to whatever output handler was provided 
 * by the libcli client that is our execution context.  In the case of the
 * CLI shell, this usually involves printing it to stdout.
 *
 * NOTE: even though it seems like it would never fail, callers should
 * always call bail_error(err) on the return value from this function!
 * This is to handle the case where the user closes his CLI window 
 * forcefully without quitting the CLI.  Stdout will be immediately
 * closed, and the CLI's attempts to write to it will fail.  This will
 * cause cli_printf() to return lc_err_cancelled.  If you do not check
 * the return value, your execution function will try to continue
 * even though the CLI is already ready to exit and cannot print any
 * more output.
 */
int cli_printf(const char *format, ...)
     __attribute__ ((format (printf, 1, 2)));
     

/* ------------------------------------------------------------------------- */
/** Print an error string.  This is just like cli_printf() except that it
 * may modify the string somehow to make it visually apparent that it
 * is an error.
 *
 * The modification is currently to prepend cli_error_prefix to the
 * message.
 *
 * NOTE: see note about in cli_printf() about checking the return value.
 */
int cli_printf_error(const char *format, ...)
     __attribute__ ((format (printf, 1, 2)));
     
#define CLI_ERROR_PREFIX "% "
extern const char cli_error_prefix[];


/* ------------------------------------------------------------------------- */
/** Print an error string, followed by a message asking the user to enter
 * a value of the specified type.
 */
int cli_printf_type_error(bn_type type, const char *format, ...)
     __attribute__ ((format (printf, 2, 3)));


/* ------------------------------------------------------------------------- */
/** Print an error about the current command invocation being
 * incomplete.  Normally an incomplete command would be a nonterminal
 * and the infrastructure would take care of this for you; this
 * function is for commands which are *sometimes* terminal (e.g. a 
 * command to enter a prefix mode, which doesn't want to be terminal
 * if prefix modes are disabled).
 */
int cli_print_incomplete_command_error(const cli_command *cmd, 
                                       const tstr_array *cmd_line);

     
/* ------------------------------------------------------------------------- */
/** Get a pointer to a symbol from another module.
 */
int cli_get_symbol(const char *module_name, const char *symbol_name,
                   void **ret_symbol);


/* ------------------------------------------------------------------------- */
/** Same as lc_cli_cmd_pattern_match().  This function was moved to
 * libcommon so you don't have to link with libcli to use it.
 */
#define cli_cmd_pattern_match lc_cli_cmd_pattern_match


/* ------------------------------------------------------------------------- */
/*
 * Given a command line pattern and an array of literal parameters, for
 * each '*' (wildcard) found in the pattern, place the 'next' parameter
 * in that spot. Build a command with all the '*' filled in from the
 * parameters. If the number of parameters does not match the number
 * of '*'s, NULL is returned for the command.
 *
 * XXX/EMT: this should not be used anymore since it was only for use
 * with the now-defunct cc_sensitive_params field.  But it can't be
 * deleted yet, as web_config_form_profile.c is still using it
 * (apparently for a different purpose than was originally intended).
 */
int cli_cmd_pattern_subst(const char *cmd_line_pattern, tstr_array *param_arr,
                            const char *sensitive_params, tstring **ret_cmd);

/* ------------------------------------------------------------------------- */
/** Passthru to lc_cli_check_char_context() for backward compatibility.
 */
#define cli_check_char_context lc_cli_check_char_context


/* ------------------------------------------------------------------------- */
/** Escape a string so it can be used as a single parameter in a CLI
 * command invocation.  Try to make it look as "natural" as possible,
 * given what characters are in it:
 *   - If the string has no special chars, it should be unchanged.
 *   - If the string has only double quotes or backslashes, 
 *     they should be escaped, but that's all.
 *   - If the string has spaces or question marks, it should be enclosed
 *     in double quotes (in preference to the spaces being backslash-escaped).
 *
 * The string we return is dynamically allocated and must be freed 
 * by the caller.
 */
int cli_escape_str(const char *str, char **ret_str);

/* ------------------------------------------------------------------------- */
/** Like cli_escape_str(), except that we backslash-escape spaces and 
 * question marks, and thus never enclose the entire string in double
 * quotes.
 */
int cli_escape_str_noquote(const char *str, char **ret_str);

/**
 * Convenience wrapper around cli_escape_str(), to escape a tstring
 * in place.
 */
int cli_escape_tstr(tstring *ts);

/**
 * Convenience wrapper around cli_escape_str_noquote(), to escape a
 * tstring in place.
 */
int cli_escape_tstr_noquote(tstring *ts);


/* ------------------------------------------------------------------------- */
/** Helper function to interpret arguments to a command with the
 * ccf_var_args flag set.  This is more of a challenge because the
 * parameters are not at fixed positions.  This function is only 
 * appropriate if the following two requirements are met:
 *
 *   (a) None of the variable parameters are permitted to be entered
 *       more than once (ccf_permit_mult flag).
 * 
 *   (b) Each of the parameters in question takes exactly one argument.
 *       That is, everything underneath the varargs root node is a
 *       literal followed by a single wildcard.
 *
 * (If one of these requirements is not met, the module may scan the
 * literals by checking each to see if it is a prefix of one of the
 * expected literals.  Before calling the execution callback, the
 * infrastructure should already have ensured that every literal 
 * entered either matches, or is an unambiguous prefix, of one of 
 * the literals registered.)
 *
 * 'cmd_line' is the full command line, broken into words, exactly as
 * passed to the execution callback.  'first_var_arg_idx' is the 
 * index of the first word in the command that is underneath the
 * var args root; i.e. the first word we should start scanning at.
 * 'num_args' is the number of possible distinct variable arguments
 * the user might have specified; this also tells the number of 
 * function parameters that will follow, which will be 2x num_args.
 * Each following pair will be a const char *, which is the string
 * literal that identifies a possible argument; and a tstring **,
 * which is where we should return the value of the wildcard 
 * associated with that literal, if one is found.  If one is not 
 * found, NULL is returned.
 */
int cli_get_args(const tstr_array *cmd_line, int first_var_arg_idx,
                 int num_args, const char *arg1, tstring **ret_arg1_val, ...);


/* ------------------------------------------------------------------------- */
/** Provide a binding name (constructed with printf-style format string
 * and varargs list), and a password.  There are three possible avenues
 * we can take from here:
 *
 *   (a) If the password is NULL and we are not running interactively,
 *       print the provided error message about only being able to 
 *       prompt interactively.
 *
 *   (b) If the password is NULL and we are running interactively, 
 *       prompt the user with the provided string using 
 *       clish_prompt_for_password(), and then do (c).
 *
 *   (c) If the password is non-NULL, set it into the config db using
 *       the provided binding name and type ::bt_string.
 *
 * Note that we take the prompt and the error message mainly to allow
 * for variances in how the "password" is referred to, e.g. in some
 * cases it might be referred to as a "key" instead.
 */
int cli_maybe_prompt_for_password_std(const char *passwd, 
                                      const char *prompt,
                                      tbool confirm,
                                      const char *noninteractive_error,
                                      const cli_command *cmd,
                                      const tstr_array *cmd_line,
                                      const tstr_array *params,
                                      bn_type btype,
                                      const char *bname_fmt, ...)
    __attribute__ ((format (printf, 9, 10)));


/* ------------------------------------------------------------------------- */
/** A function matching the typedef of cli_sensitive_word_callback,
 * intended to be assigned to the cc_sensitive_word_callback field of
 * a command registration.  This function expects to be used on words
 * that are "URLs" as commonly used in the Samara CLI.  This includes
 * anything supported by ftrans_utils: standard http, https, ftp, or 
 * tftp URLs; or a pseudo-URL specifying an scp file transfer.
 */
int cli_url_sensitive_word_check(void *data, const cli_command *cmd,
                                 const tstr_array *cmd_line,
                                 const tstr_array *params,
                                 const char *url_word,
                                 tbool *ret_sensitive,
                                 tstring **ret_sanitized_url_word);


/* ========================================================================= */
/** @name Prefix modes
 */

/*@{*/

/**
 * NOTE: this command generally should not be called directly by
 * modules.  To implement a prefix mode, use the ccf_prefix_mode_root
 * flag and call cli_prefix_enter_mode().
 *
 * Push a new word onto the prefix stack.  Note that words can only be
 * pushed one at a time, and an error will be raised if the word
 * pushed has any unescaped spaces in it.  One or more calls to this
 * function, in conjunction with telling the CLI to refresh its
 * prompt, is usually the way to enter a prefix mode.
 *
 * \param prefix_word A single word to add to the prefix stack.
 *
 * \param chunk_leader Is this the first word being added in this set
 * of words (from a single command)?  This is how the 'exit' command
 * knows how far to go down the stack when the user wants to leave 
 * the mode.
 *
 * \param from_wildcard Is this word from a wildcard in the command
 * tree?  This is how the infrastructure knows how to populate the
 * 'params' array passed to callbacks when a prefix is involved.
 */
int cli_prefix_push(const char *prefix_word, tbool chunk_leader,
                    tbool from_wildcard);

/**
 * NOTE: this command generally should not be called directly by
 * modules.  The "exit" command, implemented in cli_privmode_cmds.c,
 * is usually the only one that should do this.
 *
 * Pop words off the prefix stack down to and including the next one
 * with the 'first_word' flag set.  If the stack is empty, this returns
 * lc_err_not_found.
 */
int cli_prefix_pop(void);

int cli_get_prefix_summary(tbool last_chunk_only, tstring **ret_prefixes_str);

int cli_prefix_modes_set_enabled(tbool enabled);

tbool cli_prefix_modes_is_enabled(void);

int cli_prefix_modes_revmap_set_enabled(tbool enabled);

tbool cli_prefix_modes_revmap_is_enabled(void);

/**
 * Enter a prefix mode.  Should be called from the execution handler
 * of the prefix mode root node.  It has no effect if prefixes are
 * disabled, so you do not need to check first.
 */
int cli_prefix_enter_mode(const cli_command *cmd, const tstr_array *cmd_line);

/* @} */


/* ============================================================================
 * Miscellaneous
 * ============================================================================
 */

static const char cli_word_separator_char = ' ';
static const char cli_escape_char = '\\';
static const char cli_quoting_char = '\"';

static const char cli_comment_prefix = '#';

extern const char *cli_username_invisible[];

extern const uint32 cli_username_num_invisible;

/*
 * A standard set of programs you might want to put in cli_exec_allow.
 * Use this as is, or append your own items to it.
 *
 * It would be nicer to only allow absolute paths, but scp is
 * specified without one in common usage.
 */
#define cli_exec_allow_standard                                               \
        "," PROG_SCP_BINARY_NAME                                              \
        "," PROG_SCP_PATH                                                     \
        "," PROG_SFTP_SERVER_PATH                                             \

/**
 * Returns an array of usernames which are suitable to be exposed to
 * end-users.  Usernames listed in the cli_username_invisible list will be
 * filtered out from the list of matching names returned by iterating over
 * the specified node.
 *
 * \param bn_root name of binding to iterate
 * \param curr_user if have partial completion of user name
 * \param ret_completions array to append to
 */
int cli_visible_users_completion(const char *bn_root, const tstring *curr_user,
                                 tstr_array *ret_completions);

/**
 * Like cli_visible_users_completion(), except if 'root_always_hidden' is
 * false, will also consider the "root" account visible as long as it's
 * not locked out.  (See bug 12555)
 */
int cli_visible_users_completion_maybe_root(const char *bn_root,
                                            const tstring *curr_user,
                                            tbool root_always_hidden,
                                            tstr_array *ret_completions);


/**
 * Validate if the child exists in the binding tree rooted at the
 * specified subtree.
 * \param child name of child
 * \param subtree_root root of the subtree
 * \param ret_valid indication if child exists.
 */
int cli_child_node_validate(const char *child, const char *subtree_root,
                            tbool *ret_valid);

#ifdef PROD_FEATURE_I18N_SUPPORT
/**
 * Set the gettext domain that should be associated with all subsequent
 * registered CLI commands, until we are told otherwise.
 *
 * In general, this should be called once at the beginning of each CLI
 * module's init function with GETTEXT_DOMAIN as its parameter.
 *
 * This is required because otherwise libcli would not know which
 * domain to use to look up the strings for the help messages.
 * It cannot be hardcoded to use cli_mods because the customer's CLI
 * modules will probably use their own distinct gettext domain.
 *
 * \param domain Name of gettext domain.  Generally just use the
 * preprocessor constant GETTEXT_DOMAIN.
 */
int cli_set_gettext_domain(const char *domain);
#endif /* PROD_FEATURE_I18N_SUPPORT */

/* ------------------------------------------------------------------------- */
/** Set the log severity level at which CLI command executions are logged.
 * Defaults to LOG_INFO.
 */
int cli_log_level_set(int log_level);

/* ------------------------------------------------------------------------- */
/** Get the log severity level at which CLI command executions are logged.
 */
int cli_log_level_get(void);


/* ------------------------------------------------------------------------- */
/** Set whether or not hidden commands are visible.  This defaults to 
 * false, of course.
 */
int cli_show_hidden_cmds(tbool show);

/* ------------------------------------------------------------------------- */
/** Query whether or not hidden commands are currently visible.
 */
tbool cli_hidden_cmds_shown(void);

/* ------------------------------------------------------------------------- */
/** Add or remove the name of a notifiable event from the list of ones
 * that should be hidden.  If 'hidden' is set, the event will be added
 * to the list of events which should be omitted from help and
 * completion, and not displayed in any 'show' commands that list
 * events (even if the event is enabled in configuration).  If
 * 'hidden' is clear, the event will be removed from the list, and
 * thus displayed as normal.
 *
 * For example, the CMC wants some of its events to only be visible if
 * the /cmc/config/available node is set.  The module queries this
 * node and watches for config changes to it, and updates this list of
 * hidden events accordingly.
 */
int cli_notifs_set_hidden_event(const char *event_name, tbool hidden);

/* ------------------------------------------------------------------------- */
/** Construct the operation ID used to track progress for a given
 * request.  This follows the convention of making it the peer ID
 * (which we can get from the global cli_mcc), a hyphen '-', and the
 * request ID.
 *
 * NOTE: the request specified must already have been sent for this to
 * work, since the request ID is only filled in when it is sent.
 */
int cli_construct_progress_oper_id(const bn_request *request,
                                   tstring **ret_oper_id);


/* ============================================================================
 * File operations
 * ============================================================================
 */

typedef enum {
    cfi_none = 0,
    cfi_tcpdump,
    cfi_debug_dump,
    cfi_snapshot,
    cfi_stats,

/* ========================================================================= */
/* Customer-specific graft point 1: file type
 * =========================================================================
 */
#ifdef INC_CLI_MOD_HEADER_INC_GRAFT_POINT
#undef CLI_MOD_HEADER_INC_GRAFT_POINT
#define CLI_MOD_HEADER_INC_GRAFT_POINT 1
#include "../include/cli_module.inc.h"
#endif /* INC_CLI_MOD_HEADER_INC_GRAFT_POINT */
} cli_file_id;

extern const lc_enum_string_map cli_file_id_path_map[];

/* ------------------------------------------------------------------------- */
/* XXX/REM document
 */
int cli_file_get_filenames(const char *dir_name, tstr_array **ret_filenames);

/* ------------------------------------------------------------------------- */
/* XXX/REM document
 */
int cli_file_completion(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params,
                        const tstring *curr_word,
                        tstr_array *ret_completions);

/* ------------------------------------------------------------------------- */
/* XXX/REM document
 */
int cli_file_show_files(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);
/* ------------------------------------------------------------------------- */
/* XXX/REM document
 */
int cli_file_show_file(void *data, const cli_command *cmd,
                       const tstr_array *cmd_line,
                       const tstr_array *params);
/* ------------------------------------------------------------------------- */
/* XXX/REM document
 */
int cli_file_delete(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line,
                    const tstr_array *params);
/* ------------------------------------------------------------------------- */
/* XXX/REM document
 */
int cli_file_upload(void *data, const cli_command *cmd,
                    const tstr_array *cmd_line,
                    const tstr_array *params);

/* ------------------------------------------------------------------------- */
/* XXX/REM document
 */
int cli_file_email_dump(void *data, const cli_command *cmd,
                        const tstr_array *cmd_line,
                        const tstr_array *params);

/* ------------------------------------------------------------------------- */
/* XXX/REM document
 */
int cli_file_view_dump(void *data, const cli_command *cmd,
                       const tstr_array *cmd_line,
                       const tstr_array *params);

/* ------------------------------------------------------------------------- */
/* XXX/REM document
 */
int cli_file_view_dump_term(void *data);

/* ------------------------------------------------------------------------- */
/* XXX/REM document
 */
int cli_file_move(void *data, const cli_command *cmd,
                  const tstr_array *cmd_line,
                  const tstr_array *params);


#ifdef __cplusplus
}
#endif

#endif /* __CLI_MODULE_H_ */
