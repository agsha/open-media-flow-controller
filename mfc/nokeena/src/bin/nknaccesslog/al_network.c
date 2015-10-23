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
#include <glib.h>

#include <pthread.h>

#include "accesslog_pub.h"
#include "accesslog_pri.h"


#define MAX_BUF_SIZE	4096

static int socketfd = -1;
static char totbuf[MAX_BUF_SIZE];
static int curbuf=0;
static int lenofbuf=0;

extern uint32_t al_serverip;
extern int al_serverport;

//extern void http_accesslog_write(struct accesslog_item * item);
//extern int nkn_check_cfg(void);
void print_hex(char * name, unsigned char * buf, int size);


static int al_init_socket(void);
static int al_send_req(void);
static int al_recv_res(void);
static int al_parse_data(void);
static int al_recv_data(void);
void al_nw_client_thread(void);


pthread_mutex_t server_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t config_cond = PTHREAD_COND_INITIALIZER;

extern int al_exit_req;

extern void http_accesslog_write(accesslog_item_t * item);

// The rocket client thread 
static GThread *al_nw_thread = NULL;


extern pthread_mutex_t acc_lock;
////////////////////////////////////////////////////////////
// Network functions
////////////////////////////////////////////////////////////

void print_hex(char * name, unsigned char * buf, int size)
{
        int i, j;
        char ch;

        printf("name=%s", name);

        for(i=0;i<size/16;i++) {
                printf("\n%p ", &buf[i*16]);
                for(j=0; j<16;j++) {
                        printf("%02x ", buf[i*16+j]);
                }
                printf("    ");
                for(j=0; j<16;j++) {
                        ch=buf[i*16+j];
                        if(isascii(ch) &&
                           (ch!='\r') &&
                           (ch!='\n') &&
                           (ch!=0x40) )
                           printf("%c", ch);
                        else printf(".");
                }
        }
        printf("\n%p ", &buf[i*16]);
        for(j=0; j<size-i*16;j++) {
                printf("%02x ", buf[i*16+j]);
        }
        printf("    ");
        for(j=0; j<size-i*16;j++) {
                ch=buf[i*16+j];
                if(isascii(ch) &&
                   (ch!='\r') &&
                   (ch!='\n') &&
                   (ch!=0x40) )
                   printf("%c", ch);
                else printf(".");
        }
        printf("\n");
}




static int al_init_socket(void)
{
        struct sockaddr_in srv;
        int ret;

        // Create a socket for making a connection.
        // here we implemneted non-blocking connection
        if( (socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
                al_log_basic("%s: sockfd failed", __FUNCTION__);
                exit(1);
        }

        // Now time to make a socket connection.
        bzero(&srv, sizeof(srv));
        srv.sin_family = AF_INET;
        srv.sin_addr.s_addr = al_serverip;
        srv.sin_port = htons(al_serverport);

        ret = connect (socketfd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
        if(ret < 0)
        {
		return -1;
        }

	al_log_basic("Server has been successfully connected\n");

	return 1;
}

static int al_send_req(void)
{
	struct accesslog_req * req;
	char * p;
	int j, cnt;

    if ( ff_ptr == NULL )
        return -1;

	p = &totbuf[sizeof(struct accesslog_req)];
	for(j=0, cnt=0; j<ff_used; j++) {
		if(ff_ptr[j].string[0] == 0) continue;
		if(ff_ptr[j].field == FORMAT_STRING) continue;
		strcpy(p, ff_ptr[j].string);
		p += strlen(p)+1;
		cnt++;
	}

	// Fillin the accesslog_req header
	req = (struct accesslog_req *)totbuf;
	strcpy(req->magic, AL_MAGIC);
	req->tot_size = p-&totbuf[0];
	req->num_of_hdr_fields = cnt;

	if(send(socketfd, totbuf, p - totbuf, 0) <=0 ) {
		return -1;
	}

	al_log_basic("Sends out accesslog request ...\n");
	return 1;
}

static int al_recv_res(void)
{
	struct accesslog_res * res;

	if(lenofbuf < (int)accesslog_res_s) {
		return 0;
	}

	res = (struct accesslog_res *)&totbuf[0];
	if(res->status != ALR_STATUS_SUCCESS) {
		al_log_basic("server returns error %d (%x). Terminating and waiting to restart!!\n", res->status, res->status);
		exit(0);
        //return -1;
	}

	lenofbuf -= accesslog_res_s;
	curbuf += accesslog_res_s;

	if(lenofbuf==0) curbuf=0;
	//printf("al_recv_res: lenofbuf=%d cufbuf=%d\n", lenofbuf, curbuf);
	al_log_basic("Authentication passed, receives successful response code.\n");

	return 1;
}

static int al_parse_data(void)
{
	struct accesslog_item * item;

	while(1) {
		if(lenofbuf < (int)accesslog_item_s) break;	// Not enough data
		item = (struct accesslog_item *)&totbuf[curbuf];
		if(lenofbuf < item->tot_size) break; // not enough data

		/*
		 * avoid one bug when item->tot_size == 0
		 */
		if(item->tot_size == 0)
		{
			// Something is not right.
			return -1;
		}

		http_accesslog_write(item);
		
		lenofbuf -= item->tot_size;
		curbuf += item->tot_size;

		if(lenofbuf==0) curbuf=0;
		//printf("al_parse_data: lenofbuf=%d cufbuf=%d\n", lenofbuf, curbuf);
	}

	return 1;
}

static int al_recv_data(void)
{
	int readsize;
	int ret;

	readsize=MAX_BUF_SIZE-curbuf-lenofbuf;
	if(readsize<=300) {
		memmove(&totbuf[0], &totbuf[curbuf], lenofbuf);
		curbuf=0;
		readsize=MAX_BUF_SIZE-lenofbuf;
	}

	ret = recv(socketfd, &totbuf[curbuf+lenofbuf], readsize, 0);
	if(ret<=0) {
        al_log_debug("Socket recv: %d (%d)", ret, errno);
		//perror("Socket recv.");
		return -1;
	}
	lenofbuf+=ret;
	//printf("al_recv_data: lenofbuf=%d cufbuf=%d\n", lenofbuf, curbuf);

	//print_hex("al_recv_data", &totbuf[0], lenofbuf);

	return 1;
}

void al_close_socket(void)
{
	if(socketfd != -1) {
		shutdown(socketfd, 0);
		close(socketfd);
		socketfd = -1;
	}
}


void make_socket_to_svr(void)
{
	int ret;

	while(1) {

        if(nkn_check_cfg() == -1) goto again;
		if(al_init_socket()==-1) goto again;
		if(al_send_req()==-1) goto again;

parse_res_again:
		if(al_recv_data()==-1) goto again; 
		ret = al_recv_res();
		if(ret == -1) goto again;
		else if(ret == 0) goto parse_res_again;

		while(1)
		{
			if(al_recv_data()==-1) goto again;
			if(al_parse_data()==-1) goto again;
		}

again:
		al_close_socket();
		al_log_basic("server is not up, try again after 10 sec...\n");
		sleep(10);
	}
}



void al_nw_client_thread(void)
{

    int ret = 0;

redo:
    // Make sure config is done..
    pthread_mutex_lock(&server_lock);
    while(!al_config_done) {
        pthread_cond_wait(&config_cond, &server_lock);
        al_config_done = 0;
    }
    pthread_mutex_unlock(&server_lock);

    if (al_init_socket() == -1) goto bail;
    if (al_send_req() == -1) goto bail;

recv_res_again:
    if (al_recv_data() == -1) 
        goto bail;
    ret = al_recv_res();
    if (ret == -1) 
        goto bail;
    else if(ret == 0) 
        goto recv_res_again;

    // Run the server
    while(1) {

        if (al_exit_req) goto bail;

        if (al_recv_data() == -1) goto bail;

        if (al_parse_data() == -1) goto bail;
    }

bail:
    al_close_socket();
    if (al_exit_req) 
        g_thread_exit(NULL);
    al_log_basic("server is not up, trying again after 10sec...\n");
    sleep(10);
    goto redo;
}


void 
al_network_init(void)
{
    GError *t_err = NULL;

    al_nw_thread = g_thread_create((GThreadFunc) al_nw_client_thread, NULL, TRUE, &t_err);

    if (NULL == al_nw_thread) {
        lc_log_basic(LOG_ERR, "Unable to create socket client");
        g_error_free(t_err);
    }

    return;
}


int al_serv_init(void)
{
    int err = 0;

    al_network_init();

bail:
    return err;
}

