/*
 * Filename :   nknexecd_main.c
 * Date:        24 February 2012
 * Author:      Raja Gopal Andra (randra@juniper.net)
 *
 * (C) Copyright 2012, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/param.h>
#include <syslog.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/resource.h>


#include "nkn_nknexecd.h"

#define	BUFSIZE	4096
#define NKNTMP	"/nkn/tmp/"

rlim_t glob_rlimit_cur_data_limit;
rlim_t glob_rlimit_cur_as_limit;

static void usage(void);
static void version(void);
void catcher(int sig);
static int nknexecd_daemon_fd;
struct sockaddr_un dest_cli_addr;

volatile sig_atomic_t nknexecd_srv_shutdown = 0;

void check_nknexecd(void);
int send_msg(nknexecd_reply_t *ptr);
void nkn_launch_nknexecd_server(void);
int nknexecd_init_server(void);


/*
 * We check thru /proc entries to make sure that only one entry of nknexecd is
 * running at any point of time. If an attempt is made to run another nknexecd
 * process, it will fail with an error message.
 */
void
check_nknexecd(void)
{
    DIR *dir = opendir("/proc");
    struct dirent * ent;
    pid_t pid, mypid;
    int len;
    int fd;
    char buffer[BUFSIZE];

    mypid = getpid();
    while ((ent = readdir(dir)) != NULL) {
	if (!isdigit(ent->d_name[0]))
	    continue;
	pid = atoi(ent->d_name);
	if (pid == mypid)
	    continue;
	snprintf(buffer, BUFSIZE, "/proc/%d/cmdline", pid);
	if ((fd = open(buffer, O_RDONLY)) != -1) {
	    /* read command line data */
	    if ((len = read(fd, buffer, BUFSIZE)) > 1) {
		buffer[len] = '\0';
		if (strstr(buffer, "nknexecd") != 0) {
		    printf("\nThere is another running nknexecd process"
			   " (pid=%d)\n", pid);
		    printf("Please exit it before continue.\n\n");
		    exit(2);
		}
	    }
	}
	close(fd);
    }
    closedir(dir);
    return;
}   /* check_nknexecd */


static void
usage(void)
{
    printf("\n");
    printf("nknexecd - Juniper NKNEXECD Daemon\n");
    printf("Usage: \n");
    printf("-d        : Run as Daemon\n");
    printf("-v        : show version\n");
    printf("-h        : show this help\n");
    printf("Example:\n");
    printf("1) To run:\n");
    printf("            nknexecd -d\n");
    printf("2) To display version:\n");
    printf("            nknexecd -v\n");
    printf("\n");
    exit(1);
}   /* usage */


static void
version(void)
{
    printf("\n");
    printf("nknexecd - Juniper NKNEXECD Daemon\n");
    printf("Build Date: "__DATE__ " " __TIME__ "\n");
    printf("\n");
    exit(1);
}   /* version */

static void
daemonize(void)
{
    syslog(LOG_NOTICE, "nknexecd daemonizing...");
    if (0 != fork())
        exit(0);
    if (-1 == setsid())
        exit(0);
    signal(SIGHUP, SIG_IGN);
    if (0 != fork())
        exit(0);
}   /* daemonize */


void
catcher(int sig)
{
    switch(sig) {
	case SIGHUP:
	    break;
	case SIGINT:
	case SIGTERM:
	    nknexecd_srv_shutdown = 1;
	    break;
	case SIGKILL:
	    break;
	default:
	    break;
    }
    syslog(LOG_NOTICE, "nknexecd exited on signal %d !!", sig);
    close(nknexecd_daemon_fd);
    exit(1);
}   /* catcher */


/*
 * The helper method which actually parses the incoming request, executes
 * the requested command, creates the log files and  populates the reply.
 * The log files are created in the following format
 * /nkn/tmp/<basename>.XX.out.log - stdout file
 * /nkn/tmp/<basename>.XX.err.log - stderr file
 * XX varies from 0 to 10 and we keep only the last 10 log files.
 */
int
nknexecd_process_request(nknexecd_req_t *in_msg,
			 nknexecd_reply_t *out_msg)
{
    int		retval = -1;
    char	cmd[1024];
    char	mycmd[NKNEXECD_MAXCMD_LEN];
    char	errlog[NKNEXECD_MAXFILENAME_LEN];
    char	outputlog[NKNEXECD_MAXFILENAME_LEN];
    static int	cnt = 0;
    char	cntbuf[20];

    cnt++;
    if (cnt == 10)
	cnt = 0;
    sprintf(cntbuf, ".%d", cnt);
    memset(out_msg, 0, sizeof(out_msg));
    strcpy(mycmd, in_msg->req_payload.command);
    strcpy(errlog, NKNTMP);
    strcat(errlog, in_msg->req_payload.basename);
    strcat(errlog, cntbuf);
    strcat(errlog, ".err.log");
    strcpy(outputlog, NKNTMP);
    strcat(outputlog, in_msg->req_payload.basename);
    strcat(outputlog, cntbuf);
    strcat(outputlog, ".out.log");
    out_msg->reply_magic =  in_msg->req_magic;
    out_msg->reply_version = in_msg->req_version;
    out_msg->reply_length = in_msg->req_length;
    out_msg->reply_reply_code = retval;
    out_msg->reply_system_retcode = retval;
    strncpy(out_msg->reply_stdoutfile, outputlog,
	    sizeof(out_msg->reply_stdoutfile));
    strncpy(out_msg->reply_stderrfile, errlog,
	    sizeof(out_msg->reply_stderrfile));

    snprintf(cmd, sizeof(cmd),  "%s 2>%s 1>%s", mycmd, errlog, outputlog);
    retval =  system(cmd);
    if (WIFEXITED(retval)) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "%s exited, retval = %d", cmd,
		WEXITSTATUS(retval));
    } else if (WIFSIGNALED(retval)) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "%s killed by signal %d", cmd,
		WEXITSTATUS(retval));
    } else if (WIFSTOPPED(retval)) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "%s stopped by signal %d", cmd,
		WSTOPSIG(retval));
    }
    retval = WEXITSTATUS(retval);
    out_msg->reply_reply_code = retval;
    out_msg->reply_system_retcode = errno;

    return retval;
}   /* nknexecd_process_request */


static int
open_sock_dgram(const char *path)
{
    struct sockaddr_un unix_socket_name;

    nknexecd_daemon_fd = -1;
    memset(&unix_socket_name, 0, sizeof(struct sockaddr_un));
    if (unlink(path) < 0) {
	if (errno != ENOENT) {
	    DBG_LOG(SEVERE, MOD_NKNEXECD, "Failed to unlink %s",path);
	    return -1;
	}
    }

    unix_socket_name.sun_family = AF_UNIX;
    strcpy(unix_socket_name.sun_path, path);
    nknexecd_daemon_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    if (nknexecd_daemon_fd == -1) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "Unable to create domain socket");
	return -1;
    }
    if (bind(nknexecd_daemon_fd, (struct sockaddr *)&unix_socket_name,
	     sizeof(unix_socket_name)))
    {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "Unable to bind socket");
	close(nknexecd_daemon_fd);
	return -1;
    }
    return 0;
}   /* open_sock_dgram */


static int
receive_msg(nknexecd_req_t *ptr)
{
    struct  sockaddr_un sender_address;
    int	    address_length;
    int	    rv = 0;

    address_length = sizeof(struct sockaddr_un);
    rv = recvfrom(nknexecd_daemon_fd, ptr, sizeof(nknexecd_req_t), 0,
		  (struct sockaddr *)&sender_address,
		  (socklen_t *)&address_length);
    if (rv == -1) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "Failure in recvmsg in recv_msg");
	return -1;
    }
    return rv;
}   /* receive_msg */

int
send_msg(nknexecd_reply_t *ptr)
{
    int rv = 0;

    rv = sendto(nknexecd_daemon_fd, ptr, sizeof(nknexecd_reply_t), 0,
		(struct sockaddr *)&dest_cli_addr, sizeof(struct sockaddr_un));
    if (rv == -1) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "Failure in sendto in send_msg");
	return -1;
    }
    return rv;
}   /* send_msg */


/*
 * main functions
 */
void
nkn_launch_nknexecd_server(void)
{
    int			count = 0;
    int			ret = 0;
    int			connfd = 0, epollfd, timeout;
    struct		epoll_event ev, events[NKNEXECD_MAX_EVENTS];
    nknexecd_req_t	input_msg;
    nknexecd_reply_t	output_msg;

    epollfd = epoll_create(1); //single unix domain fd
    if (epollfd < 0) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "epoll_create() error");
	assert(0);
    }
    ev.events = 0;
    ev.events |= EPOLLIN;
    ev.data.fd = nknexecd_daemon_fd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, nknexecd_daemon_fd, &ev) < 0) {
	DBG_LOG(SEVERE, MOD_NKNEXECD, "%d fd has trouble when adding"
	" to epoll:%s", nknexecd_daemon_fd, strerror(errno));
	assert(0);
    }
    DBG_LOG(MSG, MOD_NKNEXECD, "Launched NKNEXECD daemon..started listening");
    while (1) {
	if (nknexecd_srv_shutdown == 1)
	    break; // we are shutting down
	timeout=1000;
	count = epoll_wait(epollfd, events, NKNEXECD_MAX_EVENTS, timeout);
	if (count < 0) {
	    DBG_LOG(SEVERE, MOD_NKNEXECD, "epoll_wait failed:%s",
		    strerror(errno));
	    continue;
	}
	if (count == 0) {
	    continue;	//timeout
	}
	memset(&input_msg, 0, sizeof(input_msg));
	ret = receive_msg(&input_msg);
	/* nknexecd_print_req(&input_msg);  */
	memset(&output_msg, 0, sizeof(output_msg));
	if ((ret != -1) && (input_msg.req_magic == NKNEXECD_MAGIC)) {
	    switch (input_msg.req_op_code) {
	    	case NKNEXECD_RUN:
		    nknexecd_process_request(&input_msg, &output_msg);
		    /* nknexecd_print_reply(&output_msg); */
		    send_msg(&output_msg);
		    continue;
		default:
		    break;
	    }
	} else {
	    output_msg.reply_magic =  input_msg.req_magic;
	    output_msg.reply_version = input_msg.req_version;
	    output_msg.reply_length = input_msg.req_length;
	    output_msg.reply_reply_code = -1;
	    output_msg.reply_system_retcode = -1;
	    strncpy(output_msg.reply_stdoutfile, "N/A",
		    sizeof(output_msg.reply_stdoutfile));
	    strncpy(output_msg.reply_stderrfile, "N/A",
		    sizeof(output_msg.reply_stderrfile));
	    send_msg(&output_msg);
	}
    }
    close(epollfd);
}   /* nkn_launch_nknexecd_server */


int
nknexecd_init_server(void)
{
    log_thread_start();
    /* open daemon socket */
    open_sock_dgram(NKNEXECD_SERVER_SOCKET);

    /* get the server address set to push to client */
    memset(&dest_cli_addr, 0, sizeof(struct sockaddr_un));
    dest_cli_addr.sun_family = AF_UNIX;
    strcpy(dest_cli_addr.sun_path, NKNEXECD_CLIENT_SOCKET);

    return 0;
}   /* nknexecd_init_server */


int
main(int argc,
     char *argv[])
{
    int runas_daemon = 0; // default is to run as non-daemon
    int ret = 0;
    struct rlimit rlim;

    /* Populate the Data and AS limits for debugging */
    getrlimit(RLIMIT_AS, &rlim);
    glob_rlimit_cur_as_limit = rlim.rlim_cur;

    getrlimit(RLIMIT_DATA, &rlim);
    glob_rlimit_cur_data_limit = rlim.rlim_cur;

    rlim.rlim_cur = 204857600;
    rlim.rlim_max = 204857600;
    setrlimit(RLIMIT_AS, &rlim);

    check_nknexecd();
    while ((ret = getopt(argc, argv, "dhv")) != -1) {
	switch (ret) {
	    case 'd':
		runas_daemon = 1;
		break;
	    case 'h':
		usage();
		break;
	    case 'v':
		version();
		break;
	}
    }

    if (runas_daemon)
	daemonize();

    signal(SIGHUP, catcher);
    signal(SIGKILL, catcher);
    signal(SIGTERM, catcher);
    signal(SIGINT, catcher);
    /*
     * Don't want to handle SIGPIPE as the command executed by calling system()
     * might start filling up error log.
     */

    nknexecd_init_server();

    /* Infinite loop below */
    nkn_launch_nknexecd_server();

    signal(SIGCHLD, SIG_IGN);
    return 0;
}   /* main */
