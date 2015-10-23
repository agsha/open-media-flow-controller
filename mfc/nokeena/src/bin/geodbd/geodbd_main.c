/*
 * Filename :   geodbd_main.c
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

#include "pe_geodb.h"

static void usage(void);
static void version(void);
void catcher(int sig);
static int geodbd_daemon_fd;
extern void config_and_run_counter_update(void );
struct sockaddr_un dest_cli_addr;

int glob_run_under_pm = 1;
int g_geoip_network_thread_wait = 1;
volatile sig_atomic_t geodbd_srv_shutdown = 0;
extern int geodbd_mgmt_initiate_exit(void);
extern int geodbd_mgmt_init(void);

void check_geodbd(void);
void check_geodbd(void)
{
        DIR * dir = opendir("/proc");
        struct dirent * ent;
        pid_t pid, mypid;
        int len;
        int fd;
        char buffer[4096];

        mypid = getpid();

        while ((ent = readdir(dir)) != NULL)
        {
                if (!isdigit(ent->d_name[0]))
                        continue;

                pid = atoi(ent->d_name);
                if (pid == mypid) continue;

                snprintf(buffer, 4096, "/proc/%d/cmdline", pid);
                if ((fd = open(buffer, O_RDONLY)) != -1)
                {
                        // read command line data
                        if ((len = read(fd, buffer, 4096)) > 1)
                        {
                                buffer[len] = '\0';
                                if(strstr(buffer, "geodbd") != 0) {
                                        printf("\nThere is another running geodbd process (pid=%d)\n", pid);
                                        printf("Please exit it before continue.\n\n");
                                        exit(2);
                                }
                        }
                }

                close(fd);
        }

        closedir(dir);

        return;
}

static void usage(void)
{
        printf("\n");
        printf("geodbd - Juniper GEO IP Daemon\n");
        printf("Usage: \n");
        printf("-d        : Run as Daemon\n");
        printf("-v        : show version\n");
        printf("-h        : show this help\n");
        printf("Example:\n");
        printf("1) To run:\n");
        printf("            nknlogd -d\n");
        printf("2) To display version:\n");
        printf("            nknlogd -v\n");
        printf("\n");
        exit(1);
}

static void version(void)
{
        printf("\n");
        printf("geodbd - Juniper GEO IP Daemon\n");
        printf("Build Date: "__DATE__ " " __TIME__ "\n");
        printf("\n");
        exit(1);

}

static void daemonize(void)
{

	printf("daemonizing...\n");
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
			geodbd_srv_shutdown = 1;
			geodbd_mgmt_initiate_exit();
			break;
		case SIGKILL:
			break;
		default:
			break;
	}
	printf("Exit on signal!!\n");
	close(geodbd_daemon_fd);
	exit(1);
}
////////////////////////////////////////////////////////////
// Supporting functions
////////////////////////////////////////////////////////////
static int open_sock_dgram(const char *path)
{
        struct sockaddr_un unix_socket_name;
        geodbd_daemon_fd = -1;
        memset(&unix_socket_name,0,sizeof(struct sockaddr_un));
        if (unlink(path) < 0)
        {
                if (errno != ENOENT)
                {
                        DBG_LOG(SEVERE, MOD_GEODB,
                                "Failed to unlink %s",path);
                        return -1;
                }
        }

        unix_socket_name.sun_family = AF_UNIX;
        strcpy(unix_socket_name.sun_path, path);
        geodbd_daemon_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
        if (geodbd_daemon_fd == -1)
        {
                DBG_LOG(SEVERE, MOD_GEODB,
                        "Unable to create domain socket");
                return -1;
        }
        if (bind(geodbd_daemon_fd, (struct sockaddr *)&unix_socket_name, sizeof(unix_socket_name)))
        {
                DBG_LOG(SEVERE, MOD_GEODB,
                        "Unable to bind socket");
                close(geodbd_daemon_fd);
                return -1;
        }
        return 0;
}

static int receive_msg(geo_ip_req_t *ptr)
{
        struct sockaddr_un sender_address;
        int address_length;
        int rv = 0;
        address_length = sizeof(struct sockaddr_un);
        rv = recvfrom(geodbd_daemon_fd, ptr, sizeof(geo_ip_req_t),0,
                                (struct sockaddr *)&sender_address,
                                (socklen_t *)&address_length);
        if (rv == -1)
        {
                DBG_LOG(SEVERE, MOD_GEODB,
                        "Failure in recvmsg in recv_msg");
                return -1;
        }

        return rv;
}

int send_msg(geo_ip_t *ptr);
int send_msg(geo_ip_t *ptr)
{
        int rv = 0;
        struct in_addr in;
        in.s_addr = ptr->ip;
        rv = sendto(geodbd_daemon_fd, ptr, sizeof(geo_ip_t),0,
                                (struct sockaddr *)&dest_cli_addr,
                                sizeof(struct sockaddr_un));
        if (rv == -1)
        {
                DBG_LOG(SEVERE, MOD_GEODB,
                        "Failure in sendto in send_msg");
                //free(ptr);
                return -1;
        }
        //free(ptr);
        return rv;
}


////////////////////////////////////////////////////////////////////////
// main Functions
////////////////////////////////////////////////////////////////////////
void nkn_launch_geodbd_server(void);
void nkn_launch_geodbd_server(void)
{
	config_and_run_counter_update();
	int count = 0;
	int ret = 0;
	int connfd = 0, epollfd, timeout;
	struct epoll_event ev, events[PE_MAX_EVENTS];
	geo_ip_req_t input_msg;
	geo_ip_t output_msg;

	epollfd = epoll_create(1); //single unix domain fd
        if (epollfd < 0) {
                DBG_LOG(SEVERE, MOD_GEODB, "epoll_create() error");
                assert(0);
        }
	ev.events = 0;
	ev.events |= EPOLLIN;
	ev.data.fd = geodbd_daemon_fd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, geodbd_daemon_fd, &ev) < 0) {
		DBG_LOG(SEVERE, MOD_GEODB, "%d fd has trouble when adding to epoll:%s\n",
			geodbd_daemon_fd, strerror(errno));
		assert(0);
	} 
	DBG_LOG(MSG, MOD_GEODB, "Launched Geo IP DB daemon..started listening");
	while(1) {
		if (geodbd_srv_shutdown == 1) break; // we are shutting down
		timeout=1000;
		count = epoll_wait(epollfd, events, PE_MAX_EVENTS, timeout);
		if (count < 0) {
			DBG_LOG(SEVERE, MOD_GEODB, "epoll_wait failed:%s", strerror(errno));
			continue;
		}
		if (count == 0) {
			continue;//timeout
		}
		memset(&input_msg, 0, sizeof(input_msg));
		ret = receive_msg(&input_msg);
		memset(&output_msg, 0, sizeof(output_msg));
		if ((ret != -1) && (input_msg.magic == GEODB_MAGIC)) {
			switch (input_msg.op_code) {
			case GEODB_LOOKUP:
				geodb_lookup(&input_msg, &output_msg);
				send_msg(&output_msg);
				continue;
			case GEODB_VERSION:
				geodb_version(output_msg.ginf.country, 64);
				send_msg(&output_msg);
				continue;
			case GEODB_START:
				geodb_open(0);
				continue;
			case GEODB_STOP:
				geodb_close();
				continue;
			default:
				break;
			}
		}
		else {
			output_msg.ginf.status_code = 500;
		}
		output_msg.magic = input_msg.magic;
		output_msg.ip = input_msg.ip;
		output_msg.pe_task_id = input_msg.pe_task_id;
		send_msg(&output_msg);
	}
	close(epollfd);
}

int geodbd_init_server(void);
int geodbd_init_server(void) 
{
        log_thread_start();
        geodb_open(0);
        //open daemon socket
        open_sock_dgram("/config/nkn/.pemgr_d");

        //get the server address set to push to client
        memset(&dest_cli_addr, 0, sizeof(struct sockaddr_un));
        dest_cli_addr.sun_family = AF_UNIX;
        strcpy(dest_cli_addr.sun_path, "/config/nkn/.pemgr_c");

        config_and_run_counter_update();
	return 0;
}

int main(int argc, char * argv[])
{
	int runas_daemon=0;
	int ret =0;
	check_geodbd();
	while ((ret = getopt(argc, argv, "dhv")) != -1) {
		switch (ret) {
		case 'd':
			runas_daemon=1;
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

		
	geodbd_init_server();

	//Init the mgmt module
	geodbd_mgmt_init();
	
	//Infinite loop below
	nkn_launch_geodbd_server();


	signal(SIGCHLD,SIG_IGN);
	return 0;
}

