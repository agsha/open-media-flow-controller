    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1";
    cmd->cc_help_descr =        N_("Configure player options");
    cmd->cc_flags =             ccf_terminal | ccf_prefix_mode_root;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_enter_prefix_mode;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 1";
    cmd->cc_help_descr =        N_("Configure player options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_delete;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$";
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_type =         bt_string;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 no";
    cmd->cc_help_descr =        N_("Negate/disable virtual player options");
    cmd->cc_req_prefix_count =  4;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify";
    cmd->cc_help_descr =        N_("Configure hash options");
    cmd->cc_flags =             ccf_terminal |  ccf_var_args;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_hash_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | digest";
    cmd->cc_help_descr =        N_("Configure hash options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | digest *";
    cmd->cc_help_exp =          N_("<digest type>");
    cmd->cc_help_options =      cli_digest_opts;
    cmd->cc_help_num_options =  sizeof(cli_digest_opts)/sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_hash_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | data";
    cmd->cc_help_descr =        N_("Configure data options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | data *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("String");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_hash_config;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | data uol";
    cmd->cc_help_descr =        N_("Configure URI offset/length");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | data uol *";
    cmd->cc_help_exp =          N_("<offset>");
    cmd->cc_help_exp_hint =     N_("URI offset");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | data uol * *";
    cmd->cc_help_exp =          N_("<length>");
    cmd->cc_help_exp_hint =     N_("URI length");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_hash_config;
    CLI_CMD_REGISTER;

    /*-------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | match";
    cmd->cc_help_descr =        N_("Set match string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | match query-string-parm";
    cmd->cc_help_descr =        N_("Set match string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | match query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Query string");
    cmd->cc_flags =             ccf_terminal; 
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    //cmd->cc_help_callback =     cli_nvsd_virtual_player_match_help;
    //cmd->cc_help_data =         (void*) "/nkn/nvsd/virtual_player/config/%s/hash_verify/match/uri_query";
    //cmd->cc_comp_callback =     cli_nvsd_virtual_player_match_comp;
    //cmd->cc_comp_data =         (void*) "/nkn/nvsd/virtual_player/config/%s/hash_verify/match/uri_query";
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_hash_config;
    //cmd->cc_revmap_order =      cro_mgmt;
    //cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | shared-secret";
    cmd->cc_help_descr =        N_("Configure share secret string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | shared-secret *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Shared secret string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "hash-verify | shared-secret * *";
    cmd->cc_help_options =      cli_shared_secret_opt;
    cmd->cc_help_num_options =  sizeof(cli_shared_secret_opt)/sizeof(cli_expansion_option);
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_callback =     cli_nvsd_virtual_player_hash_config;
    CLI_CMD_REGISTER;

    /*--------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "fast-start";
    cmd->cc_help_descr =        N_("Configure fast-start options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "fast-start query-string-parm";
    cmd->cc_help_descr =        N_("Configure Query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "fast-start query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Query string");
    //cmd->cc_help_callback =     cli_nvsd_virtual_player_match_help;
    //cmd->cc_help_data =         (void*) "/nkn/nvsd/virtual_player/config/%s/fast_start/uri_query";
    //cmd->cc_comp_callback =     cli_nvsd_virtual_player_match_comp;
    //cmd->cc_comp_data =         (void*) "/nkn/nvsd/virtual_player/config/%s/fast_start/uri_query";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/virtual_player/actions/fast_start/uri_query";
    cmd->cc_exec_name =         "name";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_name2 =        "value";
    cmd->cc_exec_type2 =	bt_string;
    cmd->cc_exec_value2 =       "$2$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "fast-start size";
    cmd->cc_help_descr =        N_("Configure size option");
    CLI_CMD_REGISTER;


    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "fast-start size *";
    cmd->cc_help_exp =          N_("<size-kB>");
    cmd->cc_help_exp_hint =     N_("Size in kilobytes");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/virtual_player/actions/fast_start/size";
    cmd->cc_exec_name =         "name";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_name2 =        "value";
    cmd->cc_exec_type2 =	bt_string;
    cmd->cc_exec_value2 =        "$2$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "fast-start time";
    cmd->cc_help_descr =        N_("Configure time option");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "fast-start time *";
    cmd->cc_help_exp =          N_("<seconds>");
    cmd->cc_help_exp_hint =     N_("Time in seconds to start from");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/virtual_player/actions/fast_start/time";
    cmd->cc_exec_name =         "name";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_name2 =        "value";
    cmd->cc_exec_type2 =	bt_string;
    cmd->cc_exec_value2 =       "$2$";
    CLI_CMD_REGISTER;
    /*--------------------------------------------------------------------*/


    /*--------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "seek";
    cmd->cc_help_descr =        N_("Configure seek options");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "seek query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string");
    //cmd->cc_flags =             ccf_terminal;
    //cmd->cc_capab_required =    ccp_set_rstr_curr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "seek query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure query string");
    //cmd->cc_help_callback =     cli_nvsd_virtual_player_match_help;
    //cmd->cc_help_data =         (void*) "/nkn/nvsd/virtual_player/config/%s/seek/uri_query";
    //cmd->cc_comp_callback =     cli_nvsd_virtual_player_match_comp;
    //cmd->cc_comp_data =         (void*) "/nkn/nvsd/virtual_player/config/%s/seek/uri_query";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/seek/uri_query";
    cmd->cc_exec_value =        "$2$";
    cmd->cc_exec_type =         bt_string;
    cmd->cc_exec_name2 =        "/nkn/nvsd/virtual_player/config/$1$/seek/active";
    cmd->cc_exec_value2 =       "true";
    cmd->cc_exec_type2 =        bt_bool;
    //cmd->cc_revmap_order =      cro_mgmt;
    //cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;
    /*--------------------------------------------------------------------*/
    
    /*--------------------------------------------------------------------*/
    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "assured-flow";
    cmd->cc_help_descr =        N_("Configure assured flow options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "assured-flow query-string-parm";
    cmd->cc_help_descr =        N_("Configure query string");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "assured-flow query-string-parm *";
    cmd->cc_help_exp =          N_("<string>");
    cmd->cc_help_exp_hint =     N_("Configure query string");
    //cmd->cc_help_callback =     cli_nvsd_virtual_player_match_help;
    //cmd->cc_help_data =         (void*) "/nkn/nvsd/virtual_player/config/%s/assured_flow/uri_query";
    //cmd->cc_comp_callback =     cli_nvsd_virtual_player_match_comp;
    //cmd->cc_comp_data =         (void*) "/nkn/nvsd/virtual_player/config/%s/assured_flow/uri_query";
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/virtual_player/actions/assured_flow/uri_query";
    cmd->cc_exec_name =         "name";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_name2 =        "value";
    cmd->cc_exec_type2 =	bt_string;
    cmd->cc_exec_value2 =       "$2$";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "assured-flow auto";
    cmd->cc_help_descr =        N_("Automatically determine rate for session");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/virtual_player/actions/assured_flow/auto";
    cmd->cc_exec_name =         "name";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_name2 =        "value";
    cmd->cc_exec_type2 =	bt_string;
    cmd->cc_exec_value2 =       "true";
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "assured-flow rate";
    cmd->cc_help_descr =        N_("Configure rate");
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "virtual-player * type 1 "
                                "assured-flow rate *";
    cmd->cc_help_exp =          N_("<kbps>");
    cmd->cc_help_exp_hint =     N_("Rate");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_action;
    cmd->cc_exec_action_name =  "/nkn/nvsd/virtual_player/actions/assured_flow/rate";
    cmd->cc_exec_name =         "name";
    cmd->cc_exec_type =		bt_string;
    cmd->cc_exec_value =        "$1$";
    cmd->cc_exec_name2 =        "value";
    cmd->cc_exec_type2 =	bt_string;
    cmd->cc_exec_value2 =       "$2$";
    CLI_CMD_REGISTER;
    /*--------------------------------------------------------------------*/
    
    /*--------------------------------------------------------------------*/
    /*              "NO" COMMANDS START BELOW THIS LINE                   */
    /*--------------------------------------------------------------------*/


    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 1 "
                                "hash-verify";
    cmd->cc_help_descr =        N_("Disable hash-verify");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/hash_verify/active";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    //cmd->cc_revmap_type =       crt_auto;
    //cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 1 "
                                "hash-verify data";
    cmd->cc_help_descr =        N_("Clear data string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/hash_verify/data/string";
    cmd->cc_exec_value =        "";
    cmd->cc_exec_type =         bt_string;
    //cmd->cc_revmap_type =       crt_auto;
    //cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 1 "
                                "hash-verify data uol";
    cmd->cc_help_descr =        N_("Clear UOL parameters");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/hash_verify/data/uri_offset";
    cmd->cc_exec_value =        "0";
    cmd->cc_exec_type =         bt_uint32;
    cmd->cc_exec_name2 =         "/nkn/nvsd/virtual_player/config/$1$/hash_verify/data/uri_len";
    cmd->cc_exec_value2 =        "0";
    cmd->cc_exec_type2 =         bt_uint32;
    //cmd->cc_revmap_type =       crt_auto;
    //cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 1 "
                                "hash-verify match";
    cmd->cc_help_descr =        N_("Clear match query string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/hash_verify/match/uri_query";
    cmd->cc_exec_value =        "";
    cmd->cc_exec_type =         bt_string;
    //cmd->cc_revmap_type =       crt_auto;
    //cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 1 "
                                "hash-verify shared-secret";
    cmd->cc_help_descr =        N_("Clear shared secret string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/hash_verify/secret/value";
    cmd->cc_exec_value =        "";
    cmd->cc_exec_type =         bt_string;
    //cmd->cc_revmap_type =       crt_auto;
    //cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;



    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 1 "
                                "fast-start";
    cmd->cc_help_descr =        N_("Disable fast-start options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/fast_start/active";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    //cmd->cc_revmap_type =       crt_auto;
    //cmd->cc_revmap_order =      cro_mgmt;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 1 "
                                "seek";
    cmd->cc_help_descr =        N_("Clear seek options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/seek/active";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    //cmd->cc_revmap_order =      cro_mgmt;
    //cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 1 "
                                "seek query-string-parm";
    cmd->cc_help_descr =        N_("Clear query string");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/seek/uri_query";
    cmd->cc_exec_value =        "";
    cmd->cc_exec_type =         bt_string;
    //cmd->cc_revmap_order =      cro_mgmt;
    //cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

    
    
    
    
    
    CLI_CMD_NEW;
    cmd->cc_words_str =         "no virtual-player * type 1 "
                                "assured-flow";
    cmd->cc_help_descr =        N_("Clear assured flow options");
    cmd->cc_flags =             ccf_terminal;
    cmd->cc_capab_required =    ccp_set_rstr_curr;
    cmd->cc_exec_operation =    cxo_set;
    cmd->cc_exec_name =         "/nkn/nvsd/virtual_player/config/$1$/assured_flow/active";
    cmd->cc_exec_value =        "false";
    cmd->cc_exec_type =         bt_bool;
    //cmd->cc_revmap_order =      cro_mgmt;
    ////cmd->cc_revmap_type =       crt_auto;
    CLI_CMD_REGISTER;

