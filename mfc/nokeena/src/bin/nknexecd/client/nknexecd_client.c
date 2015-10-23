/*
 * Filename :   nknexecd_client.c
 * Date:        24 February 2012
 * Author:      Raja Gopal Andra (randra@juniper.net)
 *
 * (C) Copyright 2012, Juniper Networks, Inc.
 * All rights reserved.
 *
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <netdb.h>
#include "nkn_nknexecd.h"


int
main(int argc,
     char *argv[])
{
    char		cmd[NKNEXECD_MAXCMD_LEN];
    char		basename[NKNEXECD_MAXBASENAME_LEN]; 
    nknexecd_retval_t	retval;

    memset(&retval, 0, sizeof(retval));

    /*
     * Based on the number of arguments, setup cmd and basename accordingly
     * to call nknexecd_run() API.
     * When no arguments, just run ls -ld with  sample basename
     * When just one argument is given, presume that to be cmd to be run
     * with random basename
     * When two arguments are given, assume first is command and second to
     * be the basename.
     */
    switch (argc) {
	case 1:
	    strcpy(cmd, "ls -ld ~/ /tmp /qwerty");
	    strcpy(basename, "lsbasename");
	    nknexecd_run(cmd, basename, &retval);
	    /* nknexecd_print_retval(&retval);  */
	    break;
	case 2:
	    strcpy(cmd, argv[1]);
	    strcpy(basename, "basename");
	    nknexecd_run(cmd, basename, &retval);
	    /* nknexecd_print_retval(&retval);  */
	    break;
	case 3:
	    strcpy(cmd, argv[1]);
	    strcpy(basename, argv[2]);
	    nknexecd_run(cmd, basename, &retval);
	    /* nknexecd_print_retval(&retval);  */
	    break;
	default: 
	    return 1;

    }
    return 0;
} /* main */
