#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "conmsg_pub.h"

#define MAX_BUF_SIZE	4096

static int socketfd = -1;
static int curbuf=0;
static int lenofbuf=0;

extern uint32_t msg_serverip;
extern int msg_serverport;

static int msg_init_socket(void);
static int msg_recv_data(void);
void msg_close_socket(void);
int display_symbol(int argc, char ** argv);
int write_symbol(int argc, char ** argv);
int call_func(int argc, char ** argv);

/*
 * -------------------------------------------------------------
 * network functions
 */

static int msg_init_socket(void)
{
        struct sockaddr_in srv;
        int ret;

        // Create a socket for making a connection.
        // here we implemneted non-blocking connection
        if( (socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
                printf("%s: sockfd failed", __FUNCTION__);
                exit(1);
        }

        // Now time to make a socket connection.
        bzero(&srv, sizeof(srv));
        srv.sin_family = AF_INET;
        srv.sin_addr.s_addr = msg_serverip;
        srv.sin_port = htons(msg_serverport);

        ret = connect (socketfd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
        if(ret < 0)
        {
		return -1;
        }

	return 1;
}

static int msg_send_req(char * cmd, int len)
{
	if(send(socketfd, cmd, len, 0) <=0 ) {
		return -1;
	}

	return 1;
}

static int msg_recv_data(void)
{
	char totbuf[MAX_BUF_SIZE];
	int readsize;
	int ret;

	while(1)
	{
		readsize = MAX_BUF_SIZE;
		ret = recv(socketfd, &totbuf[curbuf+lenofbuf], readsize, 0);
		if(ret<=0) {
			return -1;
		}
		printf("%s", totbuf);
	}

	return 1;
}

void msg_close_socket(void)
{
	if(socketfd != -1) {
		shutdown(socketfd, 0);
		close(socketfd);
		socketfd = -1;
	}
}


/*
 * -------------------------------------------------------------
 * conmsg support functions
 */


int display_symbol(int argc, char ** argv)
{
	char totbuf[MAX_BUF_SIZE];
	msg_req_t * pmsg;
	msg_display_symbol_t * pdisplay;
	char * psymbol;

	/*
	 * Format: p [/x] [/s] symbole
	 */
	if(argc < 2) return 0;

	pmsg = (msg_req_t *)&totbuf[0];
	strcpy(pmsg->magic, MSG_MAGIC);
	pmsg->type = MSG_DISPLAY_SYMBOL;


	pdisplay = (msg_display_symbol_t *)&totbuf[msg_req_s];
	if(argv[1][0] == '/') {
		// we have option
		if(argc < 3) return 0;
		if(argv[1][1] == 'x') pdisplay->type = MSG_TYPE_HEX;
		else if(argv[1][1] == 's') pdisplay->type = MSG_TYPE_STRING;
		else pdisplay->type = 0;
		psymbol = argv[2];
	}
	else {
		pdisplay->type = 0;
		psymbol = argv[1];
	}
	pdisplay->address = 0;
	strcpy(pdisplay->name, psymbol);

	pmsg->size = msg_req_s + msg_display_symbol_s + strlen(psymbol) + 1;
	/*
	 * Send out request
	 */
	if(msg_init_socket()==-1) return 0;
	
	if(msg_send_req(totbuf, pmsg->size)==1) {
		msg_recv_data();
	}

	msg_close_socket();
	return 1;
}

int write_symbol(int argc, char ** argv)
{
	char totbuf[MAX_BUF_SIZE];
	msg_req_t * pmsg;
	msg_write_symbol_t * pwrite_l;
	char * psymbol;

	/*
	 * Format: w symbole value
	 */
	if(argc < 3) return 0;

	pmsg = (msg_req_t *)&totbuf[0];
	strcpy(pmsg->magic, MSG_MAGIC);
	pmsg->type = MSG_WRITE_SYMBOL;


	pwrite_l = (msg_write_symbol_t *)&totbuf[msg_req_s];
	psymbol = argv[1];
	strcpy(pwrite_l->name, psymbol);
	if( (argv[2][0] == '0') && (argv[2][1] == 'x') ) {
		// COnvert Hex to uint64_t
		char * p;

		p=&argv[2][2];
		pwrite_l->value = 0;
		while(*p != 0) {
			pwrite_l->value <<= 4;
			if(*p>='0' && *p<='9') pwrite_l->value += *p-'0';
			else if(*p>='a' && *p<='f') pwrite_l->value += *p-'a'+10;
			else if(*p>='A' && *p<='F') pwrite_l->value += *p-'A'+10;
			else { return 0; } // Error
			p++;
		}
	}
	else {
		pwrite_l->value = atol(argv[2]);
	}

	pmsg->size = msg_req_s + msg_write_symbol_s + strlen(psymbol) + 1;

	/*
	 * Send out request
	 */
	if(msg_init_socket()==-1) return 0;
	
	if(msg_send_req(totbuf, pmsg->size)==1) {
		msg_recv_data();
	}

	msg_close_socket();
	return 1;
}

int call_func(int argc, char ** argv)
{
	char totbuf[MAX_BUF_SIZE];
	msg_req_t * pmsg;
	msg_call_func_t * pcall;
	char * psymbol;

	/*
	 * Format: call func (arg1, arg2 ... )
	 */
	if(argc < 2) return 0;

	pmsg = (msg_req_t *)&totbuf[0];
	strcpy(pmsg->magic, MSG_MAGIC);
	pmsg->type = MSG_CALL_FUNC;

	pcall = (msg_call_func_t *)&totbuf[msg_req_s];
	pcall->argc = argc - 2;
	psymbol = argv[1];
	strcpy(pcall->name, psymbol);
	strcpy(pcall->arg1, argv[2]);

	pmsg->size = msg_req_s + msg_call_func_s + 80 + 80 + 1;

	/*
	 * Send out request
	 */
	if(msg_init_socket()==-1) return 0;
	
	if(msg_send_req(totbuf, pmsg->size)==1) {
		msg_recv_data();
	}

	msg_close_socket();
	return 1;
}

