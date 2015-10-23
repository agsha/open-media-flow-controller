/*
 * Filename :   nknexecd_common.c
 * Date:        24 February 2012
 * Author:      Raja Gopal Andra (randra@juniper.net)
 *
 * (C) Copyright 2012, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/prctl.h>
#include <sys/un.h>
#include <sys/param.h>
#include <syslog.h>
#include <sys/wait.h>

#include "nkn_nknexecd.h"

/*
 * method to print nknexecd_req_t
 */
void
nknexecd_print_req(nknexecd_req_t *in_msg)
{
    DBG_LOG(MSG, MOD_NKNEXECD, "in nknexecd_print_req");
    DBG_LOG(MSG, MOD_NKNEXECD, "in_msg.magic is %d", in_msg->req_magic);
    DBG_LOG(MSG, MOD_NKNEXECD,  "in_msg.version is %d", in_msg->req_version);
    DBG_LOG(MSG, MOD_NKNEXECD, "in_msg.length is %d", in_msg->req_length);
    DBG_LOG(MSG, MOD_NKNEXECD, "in_msg.op_code is %d", in_msg->req_op_code);
    DBG_LOG(MSG, MOD_NKNEXECD, "in_msg.payload.command is %s",
	    in_msg->req_payload.command);
    DBG_LOG(MSG, MOD_NKNEXECD, "in_msg.payload.basename is %s",
	    in_msg->req_payload.basename);
}   /* nknexecd_print_req */


/*
 * method to print nknexecd_reply_t
 */
void
nknexecd_print_reply(nknexecd_reply_t *out_msg)
{
    DBG_LOG(MSG, MOD_NKNEXECD, "printreplymesg ");
    DBG_LOG(MSG, MOD_NKNEXECD, "out_msg->magic is %d",
	    out_msg->reply_magic);
    DBG_LOG(MSG, MOD_NKNEXECD, "out_msg->version is %d",
	    out_msg->reply_version);
    DBG_LOG(MSG, MOD_NKNEXECD, "out_msg->length is %d",
	    out_msg->reply_length);
    DBG_LOG(MSG, MOD_NKNEXECD, "out_msg->reply_code is %d",
	    out_msg->reply_reply_code);
    DBG_LOG(MSG, MOD_NKNEXECD, "out_msg->reply_system_retcode is %d",
	    out_msg->reply_system_retcode);
    DBG_LOG(MSG, MOD_NKNEXECD, "out_msg->stdoutfile is %s",
	    out_msg->reply_stdoutfile);
    DBG_LOG(MSG, MOD_NKNEXECD, "out_msg->stderrfile is %s",
	    out_msg->reply_stderrfile);
}   /* nknexecd_print_reply */


/*
 * method to print nknexecd_retval_t
 */
void
nknexecd_print_retval(nknexecd_retval_t *retval)
{
    DBG_LOG(MSG, MOD_NKNEXECD, "in nknexecd_print_retval");
    DBG_LOG(MSG, MOD_NKNEXECD, "retval.retval_reply_code is %d",
	    retval->retval_reply_code);
    DBG_LOG(MSG, MOD_NKNEXECD, "retval->stdoutfile is %s",
	    retval->retval_stdoutfile);
    DBG_LOG(MSG, MOD_NKNEXECD, "retval->stderrfile is %s",
	    retval->retval_stderrfile);
} /* nknexecd_print_retval */


/*
 * Function to print n lines of file into syslog.
 * Input:
 *  filename - Name of the file
 *  no_of_lines - number of lines to be printed to syslog
 * Output:
 *  return code: 0, if success, 1 if failed to open file
 */
int
nkn_print_file_to_syslog(char *filename,
                          int no_of_lines)
{
    char    line[80];
    FILE    *fp;
    int	    cnt = 0;

    fp = fopen(filename, "r");
    if (fp == NULL) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "Error opening file %s", filename);
        return 1;
    }
    while (fgets(line, sizeof(line), fp)) {
        if (no_of_lines < 0)
            break;
        cnt++;
        if (cnt > no_of_lines)
            break;
        syslog(LOG_NOTICE, "%s", line);
    }
    fclose(fp);
    return 0;
}   /* nkn_print_file_to_syslog */


/*
 * API for NVSD to call NKNEXECD
 * Input:
 *  command - command to execute
 *  out_basename - basename to be used for stdout and stderr files
 * Output:
 * struct retval with
 *  retval_reply_code with return status of cmd
 *  retval_stdoutfile with stdout of cmd
 *  retval_stderrfile with stderr of cmd
 */
int
nknexecd_run(char *command,
	     char *out_basename,
	     nknexecd_retval_t *retval)
{
    struct		sockaddr_un address;
    struct		sockaddr_un dest_cli_address;
    int			socket_fd;
    int			client_socket_fd;
    nknexecd_req_t	reqptr;
    nknexecd_reply_t	replyptr;
    int			rv = 0;
    socklen_t		address_length;
    int			cmdlen;
    int			baselen;
    int			rval;

    log_thread_start();
    /* Check for valid input */
    cmdlen = strlen(command);
    if (cmdlen > NKNEXECD_MAXCMD_LEN) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "Command (%s) is longer then %d bytes: "
		"%d", command, NKNEXECD_MAXCMD_LEN, cmdlen);
        return EINVAL;
    }
    baselen = strlen(out_basename);
    if (baselen > NKNEXECD_MAXBASENAME_LEN) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "basename (%s) is longer then %d bytes:"
		" %d", out_basename, NKNEXECD_MAXBASENAME_LEN, baselen);
	return EINVAL;
    }
    client_socket_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    rval = errno;
    if (client_socket_fd < 0) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "client: socket() failed with error %d",
		rval);
        return rval;
    }
    unlink(NKNEXECD_CLIENT_SOCKET);
    memset(&dest_cli_address, 0, sizeof(struct sockaddr_un));
    dest_cli_address.sun_family = AF_UNIX;
    snprintf(dest_cli_address.sun_path, MAXPATHLEN, NKNEXECD_CLIENT_SOCKET);
    if (bind(client_socket_fd,
	     (struct sockaddr *) &dest_cli_address,
	     sizeof(struct sockaddr_un)) != 0) {
	rval = errno;
	DBG_LOG(SEVERE, MOD_NKNEXECD, "bind() failed for nknexecd_c %d", rval);
        return rval;
    }
    socket_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    rval = errno;
    if (socket_fd < 0) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "socket() failed with error %d", rval);
        return rval;
    }
    /* start with a clean address structure */
    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, MAXPATHLEN, NKNEXECD_SERVER_SOCKET);
    memset(&reqptr, 0, sizeof(reqptr));

    /* setup nknexecd_req_t to send to server */
    reqptr.req_magic = NKNEXECD_MAGIC;
    reqptr.req_version = 1;
    reqptr.req_length = cmdlen;
    reqptr.req_op_code = NKNEXECD_RUN;
    strncpy(reqptr.req_payload.command, command, NKNEXECD_MAXCMD_LEN);
    strncpy(reqptr.req_payload.basename, out_basename, NKNEXECD_MAXBASENAME_LEN);

    /* nknexecd_print_req(&reqptr); */
    rv = sendto(socket_fd, &reqptr, sizeof(struct nknexecd_req_s), 0,
                (struct sockaddr *)&address, sizeof(struct sockaddr_un));
    if (rv == -1) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "Failure in sendto in send_msg, fd: %d",
		socket_fd);
        return EINVAL;
    }
    /* prepare to receive reply back from server */
    memset(&replyptr, 0, sizeof(replyptr));
    rv = recvfrom(client_socket_fd, &replyptr, sizeof(nknexecd_reply_t), 0,
                  (struct sockaddr *)&dest_cli_address,
                  (socklen_t *)&address_length);
    if (rv == -1) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "Failure in recvmsg, fd %d", socket_fd);
        return EINVAL;
    }
    nknexecd_print_reply(&replyptr);
    close(socket_fd);

    retval->retval_reply_code = replyptr.reply_reply_code;
    strcpy(retval->retval_stdoutfile, replyptr.reply_stdoutfile);
    strcpy(retval->retval_stderrfile, replyptr.reply_stderrfile);

    return 0;
}   /* nknexecd_run */
