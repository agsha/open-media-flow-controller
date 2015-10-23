/*
 * Filename:    cli_nkn_al_cmds.c
 * Date:        2008-11-15
 * Author:      Dhruva Narasimhan
 *
 * (C) Copyright 2008, Nokeena Networks, Inc
 * All rights reserved.
 *
 */

/*----------------------------------------------------------------------------
 * cl_nkn_al_cmds.c: Command definitions for controlling nknaccesslog
 * via the CLI
 *
 *---------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * HEADER FILES
 *---------------------------------------------------------------------------*/
#include "common.h"
#include "climod_common.h"
#include "nkn_defs.h"


int
cli_nkn_al_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context);

int cli_nkn_al_cmds_show(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params);
/*---------------------------------------------------------------------------
 * This is the module entry function. 
 *
 * NOTE: This should always be named after the filename that you choose.
 */
int
cli_nkn_al_cmds_init(
        const lc_dso_info *info,
        const cli_module_context *context)
{
        /* Always define an "error" variable and check for return values
         * of ALL samara API calls.
         *
         * NOTE: It is important that this variable be named as "err" if
         * you are going to use the helper macros CLI_CMD_NEW &
         * CLI_CMD_REGISTER (defined in
         * $PROD_TREE_ROOT/src/include/cli_module.h)
         */
        int err = 0;

        /* This is a command structure which will be used to initialize
         * & register CLI commands. This variable will be "reused" for
         * every command.
         *
         * NOTE: It is important that this variable be named as "cmd" if
         * you are going to use the helper macros CLI_CMD_NEW &
         * CLI_CMD_REGISTER (defined in
         * $PROD_TREE_ROOT/src/include/cli_module.h)
         */
	UNUSED_ARGUMENT(info);

        cli_command *cmd = NULL;

        return err;


#ifdef PROD_FEATURE_I18N_SUPPORT
        /* This is pretty much redundant and can be skipped in your
         * modules, unless you want internationalization support.
         */
        err = cli_set_gettext_domain(GETTEXT_DOMAIN);
        bail_error(err);
#endif /* PROD_FEATURE_I18N_SUPPORT */

        /* Check whether mgmtd or CMC is available. If not, then there
         * are no config nodes to check or register against and we can 
         * skip registering this command module. This check is needed if
         * we are defining commands that are going to set or get a value
         * from a config node.
         */
        if (context->cmc_mgmt_avail == false) {
                goto bail;
        }

        /* Internationalizing strings.. This is important if we expect
         * to support displaying some message in different languages.
         *
         *   _("s") puts "s" into the string catalog, and invokes  gettext
         *   with whatever domain GETTEXT_DOMAIN is defined to be.
         *
         *   I_("s") does neither.  It is for "internal" messages which we may
         *   want to localize later, but not right now.  It can be changed later
         *   to behave the same as _("s").
         *
         *   N_("s") puts "s" into the string catalog, but does not invoke 
         *   gettext.
         *
         *   GT_(s, d) does not put its parameter (which will generally be a
         *   variable rather than a string literal) into the string catalog,
         *   but does invoke gettext.  It also requires a gettext domain to be
         *   passed to it.
         *
         *   D_(s, d) puts "s" into the string catalog, and invokes gettext
         *   with a specified domain.
         *
         *   For more information, please visit 
         *   http://172.19.172.50/var/nkn/tallmaple/samara/current/doxygen/i18n_8h.html
         */
        
        /*--------------------------------------------------------------------
         * Here, we will create commands that appear to the user as one
         * of the following:-
         *
         *      1. mfd logging enable
         *      2. no mfd logging enable
         *
         *      3. mfd logging filename <filename>
         *      4. mfd logging filesize <size>
         *      5. mfd logging format <quoted string describing a format>
         *      6. mfd logging diskspace <size> 
         *      7. mfd logging enablesyslog <no>
         *
         *      8. show mfd
         *
         * Conventions:
         * -----------
         *  - Command registration starts with a <<<<<<... comment line
         *    so its easy to search and/or highlight individual
         *    commands. 
         *-------------------------------------------------------------------*/

        /* NOTE:
         * The cli_command structure documentation is available in 
         * http://172.19.172.50/var/nkn/tallmaple/samara/current/doxygen/structcli__command.html
         */

        /*-------------------------------------------------------------------*/
        /* Before we create our commands we need to describe the tree
         * that the leaf or "terminal" command is going to follow.
         *
         * Create a new parent called "mfd". 
         *
         * CLI_CMD_NEW creates a new "cmd" structure.  DO NOT 
         * - free it, 
         * - pass a pointer to a stack variable, or 
         * - overwrite it and re-use it for other command registrations.
         */


        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        /* Set the words string field. This will be the string that will
         * appear on the command line.
         */
        cmd->cc_words_str =             "mfd";
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "?".
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr =    
                N_("Configure the Media Flow Controller");

        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;



        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<< no mfd  <<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        /* Set the words string field. This will be the string that will
         * appear on the command line. Note that 2 words are given where the
         * preceding word defines a parent.
         */
        cmd->cc_words_str =             "no mfd";
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "no ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("Negate certain Media Flow Controller options");
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;



        /*<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd logging  <<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging";
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "mfd ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("Configure Media-Flow-Controller logging options");
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;




        /*<<<<<<<<<<<<<<<<<<<<<<<<< no mfd logging <<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "no mfd logging";
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "no mfd ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("Negate Media-Flow-Controller logging options");
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;



        /* So far we have established 2 parents - a "mfd" and a "no mfd"
         * and have also created a child called "logging" under a parent
         * called "mfd" and "no mfd". These are not terminal nodes, i.e
         * leaf nodes and therefore do nothing interesting other than
         * displaying help messages to the user.
         *
         * We start defining some command nodes that will actually
         * set/reset some config nodes that were registered in mgmtd.
         */



        /*<<<<<<<<<<<<<<<<<<<<<<<< mfd logging enable <<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging enable";
        /* Set the flags field to indicate that this is a terminal
         * (leaf) node.
         */
        cmd->cc_flags =                 ccf_terminal;
        
        /* Capability flag.
         * "set" is to indicate which mode the user is in.
         * "rstr" is short for restricted.
         */
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "mfd logging ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("Enable the Media Flow Controller logging");
        /* This flag specifies what should happen when the user executes
         * this command. 
         *
         * cxo_set => Set a node to a specified type and value.
         */
        cmd->cc_exec_operation =        cxo_set;
        /* This set operation should change this node name. */
        cmd->cc_exec_name =             "/pm/process/nknaccesslog/auto_launch";
        /* What is the data type of the node (and also the exec
         * operation) */
        cmd->cc_exec_type =             bt_bool;
        /* Execution value */
        cmd->cc_exec_value =            "true";
        /* Reverse mapping -
         * This specifies what should happen when
         * the user requests for a configuration database to be reverse mapped
         * into a list of CLI commands that could create it from an empty
         * configuration.
         *
         * crt_auto => This will do reverse-mapping for simple commands that 
         * set a single node using the standard execution handler.
         */
        cmd->cc_revmap_type =           crt_auto;
        cmd->cc_revmap_order =          cro_mgmt;
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;
        
        

        /*<<<<<<<<<<<<<<<<<<<<<<<<< no mfd logging enable <<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "no mfd logging enable";
        /* Set the flags field to indicate that this is a terminal
         * (leaf) node.
         */
        cmd->cc_flags =                 ccf_terminal;
        
        /* Capability flag.
         * "set" is to indicate which mode the user is in.
         * "rstr" is short for restricted.
         */
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "no mfd logging ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("Disable the Media Flow Controller logging");
        /* This flag specifies what should happen when the user executes
         * this command. 
         *
         * cxo_set => Set a node to a specified type and value.
         */
        cmd->cc_exec_operation =        cxo_set;
        /* This set operation should change this node name. */
        cmd->cc_exec_name =             "/pm/process/nknaccesslog/auto_launch";
        /* What is the data type of the node (and also the exec
         * operation) */
        cmd->cc_exec_type =             bt_bool;
        /* Execution value */
        cmd->cc_exec_value =            "false";
        cmd->cc_revmap_type =           crt_auto;
        cmd->cc_revmap_order =          cro_mgmt;
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;



        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging filesize";
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "mfd logging ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("Configure maximum filesize for a single file");
        CLI_CMD_REGISTER;




        /*<<<<<<<<<<<<<<<<<<<< mfd logging filesize <size> <<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure. */
        CLI_CMD_NEW;
        /* This is a different command than the previous one. This is
         * the actual terminal (leaf) command. The one above is the
         * parent for this. 
         *
         * Note that we give a wildcard at the end of the command string
         * to indicate that we expect the user to supply data here.
         *
         * There is a single wildcard only because thats the way this
         * command is designed, but we can have more than one argument
         * in which case another wildcard can be given.
         */
        cmd->cc_words_str =             "mfd logging filesize *";
        /* Set the flags field to indicate that this is a terminal
         * (leaf) node.
         */
        cmd->cc_flags =                 ccf_terminal;
        
        /* Capability flag.
         * "set" is to indicate which mode the user is in.
         * "rstr" is short for restricted.
         */
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        /* Help text for the expression. This is displayed when the user
         * does a <tab> or ? after typing the command. */
        cmd->cc_help_exp =              N_("<size in MBytes>");
        cmd->cc_help_term =             N_("Set maximum filesize");
        /* This flag specifies what should happen when the user executes
         * this command. 
         *
         * cxo_set => Set a node to a specified type and value.
         */
        cmd->cc_exec_operation =        cxo_set;
        /* This set operation should change this node name. */
        cmd->cc_exec_name =             "/nkn/accesslog/config/max-filesize";
        /* What is the data type of the node (and also the exec
         * operation) */
        cmd->cc_exec_type =             bt_uint16;
        /* Execution value  - For a command that takes in a value that
         * is not bounded (like true or false) , the value can be
         * referred to as $n$ variables where n is the argument list
         * index. Here we expect a single argument only so */
        cmd->cc_exec_value =            "$1$";
        cmd->cc_revmap_type =           crt_auto;
        cmd->cc_revmap_order =          cro_mgmt;
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;




        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging diskspace";
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "mfd logging ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("Configure maximum diskspace to use for access logs");
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;




        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging diskspace *";
        /* Set the flags field to indicate that this is a terminal
         * (leaf) node.
         */
        cmd->cc_flags =                 ccf_terminal;
        
        /* Capability flag.
         * "set" is to indicate which mode the user is in.
         * "rstr" is short for restricted.
         */
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        cmd->cc_help_exp =              N_("<size in MBytes>");
        cmd->cc_help_term =             N_("Set total diskspace");
        /* This flag specifies what should happen when the user executes
         * this command. 
         *
         * cxo_set => Set a node to a specified type and value.
         */
        cmd->cc_exec_operation =        cxo_set;
        /* This set operation should change this node name. */
        cmd->cc_exec_name =             "/nkn/accesslog/config/total-diskspace";
        /* What is the data type of the node (and also the exec
         * operation) */
        cmd->cc_exec_type =             bt_uint16;
        cmd->cc_exec_value =            "$1$";
        cmd->cc_revmap_type =           crt_auto;
        cmd->cc_revmap_order =          cro_mgmt;
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;


        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging syslog";
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "mfd logging ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("enable syslog to log all access output");
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;




        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging syslog *";
        /* Set the flags field to indicate that this is a terminal
         * (leaf) node.
         */
        cmd->cc_flags =                 ccf_terminal;
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        cmd->cc_help_exp =              N_("<syslog>");
        cmd->cc_help_term =             N_("yes | no");
        /* This flag specifies what should happen when the user executes
         * this command. 
         *
         * cxo_set => Set a node to a specified type and value.
         */
        cmd->cc_exec_operation =        cxo_set;
        /* This set operation should change this node name. */
        cmd->cc_exec_name =             "/nkn/accesslog/config/syslog";
        /* What is the data type of the node (and also the exec
         * operation) */
        cmd->cc_exec_type =             bt_string;
        cmd->cc_exec_value =            "$1$";
        cmd->cc_revmap_type =           crt_auto;
        cmd->cc_revmap_order =          cro_mgmt;
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;



        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging filename";
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "mfd logging ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("Configure filename where the logger writes to");
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;




        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging filename *";
        /* Set the flags field to indicate that this is a terminal
         * (leaf) node.
         */
        cmd->cc_flags =                 ccf_terminal;
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        cmd->cc_help_exp =              N_("<filename>");
        cmd->cc_help_term =             N_("Set the logfile name");
        /* This flag specifies what should happen when the user executes
         * this command. 
         *
         * cxo_set => Set a node to a specified type and value.
         */
        cmd->cc_exec_operation =        cxo_set;
        /* This set operation should change this node name. */
        cmd->cc_exec_name =             "/nkn/accesslog/config/filename";
        /* What is the data type of the node (and also the exec
         * operation) */
        cmd->cc_exec_type =             bt_string;
        cmd->cc_exec_value =            "$1$";
        cmd->cc_revmap_type =           crt_auto;
        cmd->cc_revmap_order =          cro_mgmt;
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;




        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging format";
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "mfd logging ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("Set the format of the log message written to access.log");
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;




        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "mfd logging format *";
        /* Set the flags field to indicate that this is a terminal
         * (leaf) node.
         */
        cmd->cc_flags =                 ccf_terminal;
        /* Capability flag.
         * "set" is to indicate which mode the user is in.
         * "rstr" is short for restricted.
         */
        cmd->cc_capab_required =        ccp_set_rstr_curr;
        /* This seems like a very messy help string - but I just picked
         * from the code comments in the accesslog. 
         */
        cmd->cc_help_exp =              N_("<format string>. "
                        "Use combination of following.\n"
                        " %b : FORMAT_BYTES_OUT_NO_HEADER\n"
                        " %c : FORMAT_CACHE_HIT\n"
                        " %d : FORMAT_ORIGIN_SIZE\n"
                        " %e : FORMAT_CACHE_HISTORY\n"
                        " %f : FORMAT_FILENAME\n"
                        " %h : FORMAT_REMOTE_HOST\n"
                        " %i : FORMAT_HEADER\n"
                        " %l : FORMAT_REMOTE_IDENT\n"
                        " %m : FORMAT_REQUEST_METHOD\n"
                        " %o : FORMAT_RESPONSE_HEADER\n"
                        " %q : FORMAT_QUERY_STRING\n"
                        " %r : FORMAT_REQUEST_LINE\n"
                        " %s : FORMAT_STATUS\n"
                        " %t : FORMAT_TIMESTAMP\n"
                        " %u : FORMAT_REMOTE_USER\n"
                        " %v : FORMAT_SERVER_NAME\n"
                        " %w : FORMAT_PEER_STATUS\n"
                        " %x : FORMAT_PEER_HOST\n"
                        " %y : FORMAT_STATUS_SUBCODE\n"
                        " %C : FORMAT_COOKIE\n"
                        " %D : FORMAT_TIME_USED_MS\n"
                        " %E : FORMAT_TIME_USED_SEC\n"
                        " %H : FORMAT_REQUEST_PROTOCOL\n"
                        " %I : FORMAT_BYTES_IN\n"
                        " %O : FORMAT_BYTES_OUT\n"
                        " %U : FORMAT_URL\n"
                        " %V : FORMAT_HTTP_HOST\n"
                        " %W : FORMAT_CONNECTION_STATUS\n"
                        " %X : FORMAT_REMOTE_ADDR\n"
                        " %Y : FORMAT_LOCAL_ADDR\n"
                        " %Z : FORMAT_SERVER_PORT\n");
        cmd->cc_help_term =             N_("Set format");
        /* This flag specifies what should happen when the user executes
         * this command. 
         *
         * cxo_set => Set a node to a specified type and value.
         */
        cmd->cc_exec_operation =        cxo_set;
        /* This set operation should change this node name. */
        cmd->cc_exec_name =             "/nkn/accesslog/config/format";
        /* What is the data type of the node (and also the exec
         * operation) */
        cmd->cc_exec_type =             bt_string;
        cmd->cc_exec_value =            "$1$";
        cmd->cc_revmap_type =           crt_auto;
        cmd->cc_revmap_order =          cro_mgmt;
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;



        /*<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  mfd <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<*/
        /* Allocate a new cli_command structure */
        CLI_CMD_NEW;
        cmd->cc_words_str =             "show mfd";
        /* Set the flags field to indicate that this is a terminal
         * (leaf) node.
         */
        cmd->cc_flags =                 ccf_terminal;
        /* Capability flag.
         * "query" is to indicate which mode the user is in - in this
         * case it is a "show"
         * "rstr" is short for restricted.
         */
        cmd->cc_capab_required =        ccp_query_rstr_curr;
        /* Set the help description that is displayed when the user
         * types a ?. 
         *
         * Mode : "configure terminal"
         * When Displayed : user typed "show ?"
         *
         * Note that we use the N_() macro to set the string.
         */
        cmd->cc_help_descr = 
                N_("Display Media Flow Controller configuration and status");
        /* This is the execution callback to be be executed when the
         * user enters this command. See below for definition of this
         * callback function.
         */
        cmd->cc_exec_callback =         cli_nkn_al_cmds_show;
        /* Register the command with the CLI infrastructure */
        CLI_CMD_REGISTER;

        /*------------------- Additional notes. -----------------------*/
        /* In some cases, you may need to execute a command which takes
         * in an argument which is a list. The list, for example, could
         * be a list of files that we read from a known location. 
         * 
         * A typical use case for this is when we design a command to
         * display the accesslog. Since the accesslog is rotated, we
         * dont really know if the user wishes to look at some
         * historical log or the current log. Lets say we designed the 
         * command to look like "show mfd log access *". There are 4 log
         * files available - 3 are historic and 1 is the current. When
         * the user hits a <tab> key or "?" after entering the command
         * we want the following list to be displayed (to make the
         * command more intuitive):-
         * current      (Current access log)
         * access.1     (Archive Log file 1)
         * access.2     (Archive Log file 2)
         * access.3     (Archive Log file 3)
         *
         * In such a case, the number of options is variable since we do
         * not know how many archived files really exist. 
         *
         * Here, the help will need to call a function which can return
         * a list of possible completetion terms. This can be done by
         * writing the following code.
         *
         * cmd->cc_help_callback = my_al_help;
         * cmd->cc_comp_callback = my_al_completion;
         *
         * int
         * my_al_completion(void *data, 
         *      const cli_command *cmd,
         *      const tstr_array *cmd_line,
         *      const tstr_array *params,
         *      const tstring *curr_word,
         *      tstr_array *ret_completions)
         * {
         *      Passed the command line in the same three forms as with
         *      the help callback. Returns a list of possible
         *      completions for the current word by adding them to the
         *      pre-allocated tstr_array ret_completions. This array
         *      must not be modified in any other way besides appending
         *      new elements.
         *
         *      The completions should all be full words, all prefixed
         *      with the current word, rather than just the portion of
         *      the word that has not yet been typed.
         * }
         *
         * For a REAL (!!) use case, look in
         * /var/nkn/tallmaple/samara/current/tree/src/bin/cli/modules/cli_logging_cmds.c :1768
         *
         */



bail:
        return err;
}


/*----------------------------------------------------------------------------
 * This function is called as a callback when the user enters "show mfd"
 * in the command line. 
 * Right now this simply shows logging details, but can be refined
 * further to show more "MFD" related details or if the command is
 * changed to "show mfd logging" to show only logging specific details.
 */
int cli_nkn_al_cmds_show(
        void *data,
        const cli_command *cmd,
        const tstr_array *cmd_line,
        const tstr_array *params)
{
        int err = 0;
        bn_binding_array *bindings = NULL;

	UNUSED_ARGUMENT(data);
	UNUSED_ARGUMENT(cmd);
	UNUSED_ARGUMENT(cmd_line);
	UNUSED_ARGUMENT(params);

        /* first thing to do is to get the bindings from the database
         * for a given node.
         *
         * mdc_get_binding_children
         *      Retrieve nodes underneath a specified binding node in
         *      the management tree.
         */
        err = mdc_get_binding_children(
                        cli_mcc,        // Md_clint library context
                        NULL,           // ret_code : return code from the req
                        NULL,           // ret_msg  : User friendly return msg
                        false,          // Should the binding returned have the 
                                        // binding name also broken down 
                                        // into parts for quicker access
                        &bindings,      // return bindings
                        false,          // include_self : If true, the node 
                                        // named by binding_name will be 
                                        // returned in the results; otherwise 
                                        // it will not.
                        true,           // subtree : retrieve all nodes under 
                                        // the root
                        "/nkn");        // Name of root node
        bail_error(err);

        /* Print a string which is parameterized with names of management
         * bindings to be queried and substituted.  The '#' sign  delimits
         * names of bindings; anything surrounded by a '#' on each side is
         * treated as the name of a binding which should be queried, and its
         * value substituted into the string.  The printf-style format string
         * and the variable-length list of arguments are rendered into a
         * single string first, then the substitution is performed.
         *
         * For example, you could display the interface  duplex like this:
         * cli_printf_queried("The duplex is #/interfaces/%s/duplex#\n", itf);
         */
        err = cli_printf_query
                        (_("Media Flow Controller Logging enabled : "
                           "#/pm/process/nknaccesslog/auto_launch#\n"));
        bail_error(err);
        
        err = cli_printf_query
                (_("Syslog : "
                   "#/nkn/accesslog/config/syslog#\n"));
        bail_error(err);

        err = cli_printf_query
                (_("Access Log filename : "
                   "#/nkn/accesslog/config/filename#\n"));
        bail_error(err);

        err = cli_printf_query
                (_("Access Log filesize : "
                   "#/nkn/accesslog/config/max-filesize# MBytes\n"));
        bail_error(err);

        err = cli_printf_query
                (_("Access log format : "
                   "#/nkn/accesslog/config/format#\n"));
        bail_error(err);

bail:
        bn_binding_array_free(&bindings);

        return err;
}

/*----------------------------------------------------------------------------
 * END OF FILE
 *---------------------------------------------------------------------------*/

