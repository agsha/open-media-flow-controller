/* GRAFT POINT 3 to exculde secure node from sysdump */
#if CLI_CONFIG_INC_GRAFT_POINT == 3
"/nkn/ssld/config/certificate/*/passphrase",
"/nkn/ssld/config/key/*/passphrase",
#endif

#if CLI_CONFIG_INC_GRAFT_POINT == 2
    tstring *build_version = NULL;
    tstring *hostid = NULL;

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/system/version/composite",
                                                  NULL, &build_version);
    bail_error(err);

    if (!build_version) {
        /*
         * We might not have it if we're revmapping a subtree.
         *
         * XXX/EMT: speaking of which, the comment at the top should
         * say something different if we've only got a subtree.
         * But not a big deal for now, since that's a hidden command.
         */
        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &build_version, 
                                   "/system/version/composite", NULL);
        bail_error(err);
    }

    err = bn_binding_array_get_value_tstr_by_name(bindings, "/system/hostid",
                                                  NULL, &hostid);
    bail_error(err);

    if (!hostid) {
        /*
         * We might not have it if we're revmapping a subtree.
         *
         * XXX/EMT: speaking of which, the comment at the top should
         * say something different if we've only got a subtree.
         * But not a big deal for now, since that's a hidden command.
         */
        err = mdc_get_binding_tstr(cli_mcc, NULL, NULL, NULL, &hostid, 
                                   "/system/hostid", NULL);
        bail_error(err);
    }

    err = ts_append_sprintf(header, _("%c%c\n"),
	                                cli_comment_prefix, cli_comment_prefix);
    bail_error(err);
    if (build_version) {
	err = ts_append_sprintf(header, _("%c%c Version: %s \n"),
	        cli_comment_prefix, cli_comment_prefix,ts_str(build_version));
	ts_free(&build_version);
        bail_error(err);
    }
    if (hostid) {
        err = ts_append_sprintf
            (header, _("%c%c Host ID: %s\n"),
             cli_comment_prefix, cli_comment_prefix, ts_str(hostid));
	ts_free(&hostid);
        bail_error(err);
    }

    err = ts_append_sprintf(header, _("%c%c\n\n"),
	                                cli_comment_prefix, cli_comment_prefix);
    bail_error(err);
    
#endif
