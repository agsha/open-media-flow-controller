    /*------------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2";
    cmd->cc_help_descr =        N_("Configure player options");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_enter_prefix_mode;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 no";
    cmd->cc_help_descr =        N_("Negate/disable virtual player options");
    cmd->cc_req_prefix_count =  4;
    CLI_CMD_REGISTER;

    /*--------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 "
                                "rate-map";
    cmd->cc_help_descr =        N_("Configure rate map options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 "
                                "rate-map match";
    cmd->cc_help_descr =        N_("Configure Match options");
    CLI_CMD_REGISTER;
   
    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 "
                                "rate-map match *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Match String");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/virtual_player/config/$1$/rate_map/match/*";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 "
                                "rate-map match * rate";
    cmd->cc_help_descr =        N_("Configure rate options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 "
                                "rate-map match * rate *";
    cmd->cc_help_exp =          N_("<kbps>");
    cmd->cc_help_exp_hint =     N_("Rate");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_rate_map_config;
    CLI_CMD_REGISTER;
    
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 "
                                "rate-map match * rate * query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 "
                                "rate-map match * rate * query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure query string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             1;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_rate_map_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 "
                                "rate-map match * rate * uol";
    cmd->cc_help_descr =        N_("Configure URI Offset and length");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 "
                                "rate-map match * rate * uol *";
    cmd->cc_help_exp =          N_("<offset>");
    cmd->cc_help_exp_hint =     N_("URI offset");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 2 "
                                "rate-map match * rate * uol * *";
    cmd->cc_help_exp =          N_("<length>");
    cmd->cc_help_exp_hint =     N_("URI length");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_magic =             2;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_rate_map_config;
    CLI_CMD_REGISTER;
    /*--------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*/
    /*              "NO" COMMMANDS BEGIN BELOW THIS LINE                  */
    /*--------------------------------------------------------------------*/

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 2 ";
    cmd->cc_help_descr =        N_("Disable/delete virtual player options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 2 "
                                "rate-map";
    cmd->cc_help_descr =        N_("Disable/delete rate map");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 2 "
                                "rate-map match";
    cmd->cc_help_descr =        N_("Clear rate map options");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 2 "
                                "rate-map match *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Match string");
    cmd->cc_help_use_comp =     true;
    cmd->cc_comp_type =         cct_matching_names;
    cmd->cc_comp_pattern =      "/nkn/nvsd/virtual_player/config/$1$/rate_map/match/*";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/rate_map/match/$2$";
    cmd->cc_exec_value =        "$2$";
    CLI_CMD_REGISTER;


