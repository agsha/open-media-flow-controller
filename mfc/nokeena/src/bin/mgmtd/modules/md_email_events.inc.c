/*
 *
 * Filename:  md_email_events.inc.c,v $
 * Date:      $Date: 2009/06/12 $
 * Author:    Chitra Devi R
 * Nokeena Networks , Inc
 *
 */

/* ------------------------------------------------------------------------ */
/* md_email_events.inc.c: shows how to use graft point 2 in
 * md_email_events.c to translate event messages into custom email
 * notifications.
 */

/*
 * Note that graft point 1 is deprecated.  It used to declare a list
 * of events we knew how to render into emails, but that information
 * has been moved to a graft point in include/event_notifs.inc.c
 */

/*
 * Graft point 2: event handling
 */
#if MD_EMAIL_EVENTS_INC_GRAFT_POINT == 2

    else if (!strcmp(event_name,
                     "/nkn/nvsd/notify/nvsd_stuck")) {
	lt_time_sec curr_time;
	lt_date ret_date;
	lt_time_sec ret_daytime_sec;
	char nkncnt_dump_file[256] = {0};
	char time_buf[64] = {0};

        lc_log_basic(LOG_NOTICE, "nvsd stuck - Sending email!\n");
	bn_binding *binding = NULL;

        *ret_separate_versions = true;

	subject = smprintf(_("NVSD might be Hung !"));
	bail_null(subject);

	/*
	 * Summary version
	 */

	subj_summ = strdup(_("NVSD might be stuck!"));
	bail_null(subj_summ);
	body_summ = NULL;

	/*
	 * Detailed version
	 */


	err = bn_binding_array_get_first_matching_value_tstr
		(bindings, "failed_check", 0, NULL, NULL, &value);
	bail_error(err);

	/* get the time in current time zone and convert as string*/
	curr_time  = lt_curr_time();

	lt_time_to_date_daytime_sec (curr_time, &ret_date, &ret_daytime_sec);
	lt_daytime_sec_to_buf (ret_daytime_sec, 64, time_buf);

	snprintf(nkncnt_dump_file, sizeof(nkncnt_dump_file),"/var/log/nkn/nkncnt_%s.dump",time_buf);
	bail_null(nkncnt_dump_file);



        subj_detail = strdup(_("NVSD might be stuck"));
        bail_null(subj_detail);
        err = lc_launch_quick(NULL, &output, 2, "/opt/nkn/bin/nkncntdump.sh", nkncnt_dump_file);
        bail_error(err);


        body_detail = smprintf
            (_("\n=================================================="
               "\nEvent information:\n\n"
               "NVSD might be stuck:%s   "
               "\n==================================================\n\n"
               "==================================================\n"
	       "Counters values are dumped in the below file\n"
	       "%s"
               "\n\n==================================================\n")
		,ts_str(value),nkncnt_dump_file);
        bail_null(body_detail);
    }



/* ........................................................................
     * Individual CPU busy percentage too high
     */
    else if (!strcmp(event_name,
                     "/stats/events/nkn_cpu_util_ave/rising/error")) {

        bn_binding *binding = NULL;

        *ret_separate_versions = true;

        /*
         * Summary version
         */

        subj_summ = strdup(_("CPU utilization too high"));
        bail_null(subj_summ);
        body_summ = NULL;

        /*
         * Detailed version
         */

        subj_detail = strdup(_("CPU utilization too high"));
        bail_null(subj_detail);

        err = bn_binding_array_get_first_matching_value_tstr
            (bindings, "/system/cpu/all/busy_pct", 0, NULL, &name, &value);
        bail_error_null(err, value);

        err = lc_launch_quick(NULL, &output, 4, prog_top_path, "-n",
                              "1", "-b");
        bail_error_null(err, output);

        body_detail = smprintf
            (_("\n=================================================="
               "\nEvent information:\n\n"
               "Average CPU usage percentage over the past minute:   "
               "%s%%\n"
               "\n==================================================\n\n"
               "==================================================\n"
               "Output of 'top -n 1 -b':\n\n%s"
               "\n\n==================================================\n"),
             ts_str(value), ts_str(output));
        bail_null(body_detail);
    }

    /* ........................................................................
     * Individual CPU busy percentage clear
     */
    else if (!strcmp(event_name,
                     "/stats/events/nkn_cpu_util_ave/rising/clear")) {

        *ret_separate_versions = true;
        bn_binding *binding = NULL;

        /*
         * Summary version
         */

        subj_summ = strdup(_("CPU utilization alarm clearing"));
        bail_null(subj_summ);
        body_summ = NULL;

        /*
         * Detailed version
         */

        subj_detail = strdup(_("CPU utilization alarm clearing"));
        bail_null(subj_detail);

        err = bn_binding_array_get_first_matching_value_tstr
            (bindings, "/system/cpu/all/busy_pct", 0, NULL, &name, &value);
        bail_error_null(err, value);

        err = lc_launch_quick(NULL, &output, 4, prog_top_path, "-n",
                              "1", "-b");
        bail_error_null(err, output);

        body_detail = smprintf
            (_("\n=================================================="
               "\nEvent information:\n\n"
               "Average CPU usage percentage over the past minute:   "
               "%s%%\n"
               "\n==================================================\n\n"
               "==================================================\n"
               "Output of 'top -n 1 -b':\n\n%s"
               "\n\n==================================================\n"),
              ts_str(value), ts_str(output));
        bail_null(body_detail);
    }



#endif /* GRAFT_POINT 2 */
