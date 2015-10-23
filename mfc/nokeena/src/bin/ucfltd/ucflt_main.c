/*
 * Filename :   ucfltd_main.c
 * Date:        04 July 2011
 * Author:      Muthukumar
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
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

#include "pe_ucflt.h"

static void usage(void);
static void version(void);
void catcher(int sig);
static int ucfltd_daemon_fd;
extern void config_and_run_counter_update(void );
extern int read_ucflt_cfg(char * configfile);
extern int ts_read_custom_cat_file(char * custom_sites_file);

struct sockaddr_un dest_cli_addr;

int glob_run_under_pm = 1;
volatile sig_atomic_t ucfltd_srv_shutdown = 0;
//extern int ucfltd_mgmt_init(void);

// Config variables from /config/nkn/nkn.ucflt.conf
extern int   ts_db_debug_enable;
extern int   ts_db_max_size ;
extern int   ts_db_max_age ;
extern int   ts_db_port_num ;
extern char *ts_db_server_name ;
extern char *ts_db_searial_num ;

char *ts_category_list_file = (char *)"/nkn/ucflt/nkn.ts.custom.sites" ;

static void usage(void)
{
        DBG_LOG(MSG, MOD_UCFLT,"\n");
        DBG_LOG(MSG, MOD_UCFLT,"ucfltd - Juniper URL Category filter \n");
        DBG_LOG(MSG, MOD_UCFLT,"Usage: \n");
        DBG_LOG(MSG, MOD_UCFLT,"-d        : Run as Daemon\n");
        DBG_LOG(MSG, MOD_UCFLT,"-v        : show version\n");
        DBG_LOG(MSG, MOD_UCFLT,"-h        : show this help\n");
        DBG_LOG(MSG, MOD_UCFLT,"Example:\n");
        DBG_LOG(MSG, MOD_UCFLT,"1) To run:\n");
        DBG_LOG(MSG, MOD_UCFLT,"            nknlogd -d\n");
        DBG_LOG(MSG, MOD_UCFLT,"2) To display version:\n");
        DBG_LOG(MSG, MOD_UCFLT,"            nknlogd -v\n");
        DBG_LOG(MSG, MOD_UCFLT,"\n");
        exit(1);
}

static void version(void)
{
        DBG_LOG(MSG, MOD_UCFLT,"\n");
        DBG_LOG(MSG, MOD_UCFLT,"ucfltd - Juniper URL Category filter Daemon\n");
        DBG_LOG(MSG, MOD_UCFLT,"Build Date: "__DATE__ " " __TIME__ "\n");
        DBG_LOG(MSG, MOD_UCFLT,"\n");
        exit(1);

}

static void daemonize(void)
{

	DBG_LOG(MSG, MOD_UCFLT,"daemonizing...\n");
        if (0 != fork()) exit(0);

        if (-1 == setsid()) exit(0);

        signal(SIGHUP, SIG_IGN);

        if (0 != fork()) exit(0);
}

void catcher(int sig)
{
	switch(sig) {
		case SIGHUP:
			break;
		case SIGINT:
		case SIGTERM:
			ucfltd_srv_shutdown = 1;
			//ucfltd_mgmt_initiate_exit();
			break;
		case SIGKILL:
			break;
		default:
			break;
	}
	DBG_LOG(SEVERE, MOD_UCFLT,"Exit on signal!!\n");
	close(ucfltd_daemon_fd);
	exit(1);
}
////////////////////////////////////////////////////////////
// Supporting functions
////////////////////////////////////////////////////////////
static int open_sock_dgram(const char *path)
{
        struct sockaddr_un unix_socket_name;
        ucfltd_daemon_fd = -1;
        memset(&unix_socket_name,0,sizeof(struct sockaddr_un));
        if (unlink(path) < 0)
        {
                if (errno != ENOENT)
                {
                        DBG_LOG(MSG, MOD_UCFLT,
                                "Failed to unlink %s",path);
                        return -1;
                }
        }

        unix_socket_name.sun_family = AF_UNIX;
        strcpy(unix_socket_name.sun_path, path);
        ucfltd_daemon_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
        if (ucfltd_daemon_fd == -1)
        {
                DBG_LOG(MSG, MOD_UCFLT,
                        "Unable to create domain socket");
                return -1;
        }
        if (bind(ucfltd_daemon_fd, (struct sockaddr *)&unix_socket_name, sizeof(unix_socket_name)))
        {
                DBG_LOG(MSG, MOD_UCFLT,
                        "Unable to bind socket");
                close(ucfltd_daemon_fd);
                return -1;
        }
        return 0;
}

static int receive_msg(ucflt_req_t *ptr)
{
        struct sockaddr_un sender_address;
        int address_length;
        int rv = 0;
        address_length = sizeof(struct sockaddr_un);
        rv = recvfrom(ucfltd_daemon_fd, ptr, sizeof(ucflt_req_t),0,
                                (struct sockaddr *)&sender_address,
                                (socklen_t *)&address_length);
        if (rv == -1)
        {
                DBG_LOG(ERROR, MOD_UCFLT, "Failure in recvmsg in recv_msg");
                return -1;
        }

        return rv;
}

int send_msg(ucflt_resp_t *ptr);
int send_msg(ucflt_resp_t *ptr)
{
        int rv = 0;
        rv = sendto(ucfltd_daemon_fd, ptr, sizeof(ucflt_resp_t),0,
                                (struct sockaddr *)&dest_cli_addr,
                                sizeof(struct sockaddr_un));
        if (rv == -1)
        {
                DBG_LOG(ERROR, MOD_UCFLT,
                        "Failure in sendto in send_msg");
                return -1;
        }
        return rv;
}


////////////////////////////////////////////////////////////////////////
// main Functions
////////////////////////////////////////////////////////////////////////
void nkn_launch_ucfltd_server(void);
void nkn_launch_ucfltd_server(void)
{
	config_and_run_counter_update();

	int count = 0;
	int ret = 0;
	int connfd = 0, epollfd, timeout;
	struct epoll_event ev, events[PE_MAX_EVENTS];
	ucflt_req_t input_msg;
        ucflt_resp_t output_msg;

	epollfd = epoll_create(1); //single unix domain fd
        if (epollfd < 0) {
                DBG_LOG(MSG, MOD_UCFLT, "epoll_create() error");
                assert(0);
        }
	ev.events = 0;
	ev.events |= EPOLLIN;
	ev.data.fd = ucfltd_daemon_fd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ucfltd_daemon_fd, &ev) < 0) {
		DBG_LOG(MSG, MOD_UCFLT, "%d fd has trouble when adding to epoll:%s\n",
			ucfltd_daemon_fd, strerror(errno));
		assert(0);
	} 

	DBG_LOG(MSG, MOD_UCFLT, "Launched URL Category filter daemon. Started listening");
	while(1) {
		if (ucfltd_srv_shutdown == 1) break; // we are shutting down
		timeout=1000;

		count = epoll_wait(epollfd, events, PE_MAX_EVENTS, timeout);
		if (count < 0) {
			DBG_LOG(MSG, MOD_UCFLT, "epoll_wait failed:%s", strerror(errno));
			continue;
		}
		if (count == 0) {
			continue;//timeout
		}

		memset(&input_msg, 0, sizeof(input_msg));
		ret = receive_msg(&input_msg);


		if ((ret != -1) && (input_msg.magic == UCFLT_MAGIC)) {
			switch (input_msg.op_code) {
			case UCFLT_LOOKUP:
				ucflt_lookup(&input_msg);
				continue;
			case UCFLT_VERSION:
				continue;
			case UCFLT_START:
				ucflt_open(0);
				continue;
			case UCFLT_STOP:
				ucflt_close();
				continue;
			default:
				break;
			}
		}
		else {
                        memset(&output_msg, 0, sizeof(output_msg)) ;
			output_msg.info.status_code = 500;
			output_msg.magic = input_msg.magic;
			output_msg.pe_task_id = input_msg.pe_task_id;
			send_msg(&output_msg);
		}
	}
	close(epollfd);
}

/*
 * Open the TS SDK DB connection and UD sockets to communicate
 * with nvsd.
 */
int ucfltd_init_server(void);
int ucfltd_init_server(void) 
{
        log_thread_start();
        ucflt_open(0);

        //open daemon socket
        open_sock_dgram("/config/nkn/.ucflt_d");

        //get the server address set to push to client
        memset(&dest_cli_addr, 0, sizeof(struct sockaddr_un));
        dest_cli_addr.sun_family = AF_UNIX;
        strcpy(dest_cli_addr.sun_path, "/config/nkn/.ucflt_c");

        config_and_run_counter_update();
	return 0;
}

int main(int argc, char * argv[])
{
	int runas_daemon=0;
	int ret =0;
        char *config_file = NULL;

	while ((ret = getopt(argc, argv, "f:dhv")) != -1) {
		switch (ret) {
		case 'd':
			runas_daemon=1;
		break;
		case 'f':
			config_file=optarg;
		break;
		case 'h':
			usage();
		break;
		case 'v':
			version();
		break;
		}
	}

	if(runas_daemon) daemonize();

	signal(SIGHUP,catcher);
	signal(SIGKILL,catcher);
	signal(SIGTERM,catcher);
	signal(SIGINT,catcher);
	signal(SIGPIPE,SIG_IGN);

		
        /* Read config parameters for url category filter*/
	read_ucflt_cfg(config_file);

	ucfltd_init_server();

        ts_read_custom_cat_file(ts_category_list_file) ;
	//Init the mgmt module
	//ucfltd_mgmt_init();
	
	//Infinite loop below
	nkn_launch_ucfltd_server();


	signal(SIGCHLD,SIG_IGN);
	return 0;
}

