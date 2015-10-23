#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define MAX_BUF_SIZE	8192

int socketfd = -1;
char totbuf[MAX_BUF_SIZE];
uint32_t rtsp_ip;
char * rtsp_ipstr;
int rtsp_port;
char * rtsp_filename;
char * rtsp_session_str;
FILE * elfd = NULL;
char * rtsp_output_filename="MP2T.ts";
char * rtsp_uri="rtsp://172.19.172.60/touhou.ts";
int seq = 0;


////////////////////////////////////////////////////////////
// Network functions
////////////////////////////////////////////////////////////

static int rtsp_init_socket(void);
static int rtsp_socket_handshake(void);
static int rtsp_send_OPTIONS(void);
static int rtsp_send_DESCRIBE(void);
static int rtsp_send_SETUP(void);
static int rtsp_send_PLAY(void);
static int rtsp_send_TEARDOWN(void);
static void rtsp_close_socket(void);
static void rtsp_open_file(void);
static void rtsp_saveto_file(char * p, int size);
static void rtsp_close_file(void);
static void rtsp_parse_server(void);
static void usage(void);
static void catcher(int sig);

/*--------------------------------------------------------------------------*/

static int rtsp_init_socket(void)
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
        srv.sin_addr.s_addr = rtsp_ip;
        srv.sin_port = htons(rtsp_port);

        ret = connect (socketfd, (struct sockaddr *)&srv, sizeof(struct sockaddr));
        if(ret < 0)
        {
		printf("Failed to connect to server %s\n", rtsp_ipstr);
                exit(1);
        }
	printf("Server has been successfully connected ....\n");

	return 1;
}

static int rtsp_socket_handshake(void)
{
	int ret;

	printf("=====================\n");
	/*
	 * Send request to nvsd server.
	 */
	if(send(socketfd, totbuf, strlen(totbuf), 0) <=0 ) {
		perror("Socket recv.");
		exit(1);
	}
	printf("sending: <%s>\n", totbuf);

	/*
	 * Read response from nvsd server.
	 */
	ret = recv(socketfd, totbuf, MAX_BUF_SIZE, 0);
	if(ret<0) {
		perror("Socket recv.");
		exit(1);
	}
	totbuf[ret] = 0;
	printf("receiving (%d): <%s>\n", ret, totbuf);
}

static int rtsp_send_OPTIONS(void)
{
	char * options ="OPTIONS rtsp://%s/%s RTSP/1.0\r\n"
			"CSeq: %d\r\n"
			"User-Agent: VLC media player (LIVE555 Streaming Media v2008.11.13)\r\n"
			"\r\n";

	sprintf(totbuf, options, rtsp_ipstr, rtsp_filename, seq++);
	rtsp_socket_handshake();

	printf("OPTIONS handshake done ....\n\n");
	return 1;
}

static int rtsp_send_DESCRIBE(void)
{
	char * options ="DESCRIBE rtsp://%s/%s RTSP/1.0\r\n"
			"Accept: application/sdp\r\n"
			"CSeq: %d\r\n"
			"\r\n";
	
	sprintf(totbuf, options, rtsp_ipstr, rtsp_filename, seq++);
	rtsp_socket_handshake();

	printf("DESCRIBE handshake done ....\n\n");
	return 1;
}

static int rtsp_send_SETUP(void)
{
	char * p, *p2;
	char * options ="SETUP rtsp://%s/%s RTSP/1.0\r\n"
			"User-Agent: Avtrex\r\n"
			"Transport: MP2T/H2221/TCP;unicast;interleaved=0\r\n"
			"Range: npt=0.000-\r\n"
			"x-mayNotify:\r\n"
			"CSeq: %d\r\n"
			"\r\n";
	
	sprintf(totbuf, options, rtsp_ipstr, rtsp_filename, seq++);
	rtsp_socket_handshake();

	p = strstr(totbuf, "Session:");
	if(!p) {
		printf("Cannot find session string\n");
		exit(1);
	}
	p+=9;
	p2 = strchr(p, ';');
	if(!p2) {
		p2 = strchr(p, '\r');
		if(!p2) {
			printf("Cannot find session timeout\n");
			exit(1);
		}
	}
	*p2=0;
	rtsp_session_str = strdup(p);

	printf("SETUP handshake done session string = %s ....\n\n", rtsp_session_str);
	return 1;
}
static int rtsp_send_PLAY(void)
{
	char * options ="PLAY rtsp://%s/%s RTSP/1.0\r\n"
			"x-noFlush:\r\n"
			"Scale: 1.000\r\n"
			"Range: npt=0.000-\r\n"
			"Speed: 2.000\r\n"
			"x-mayNotify:\r\n"
			"CSeq: %d\r\n"
			"Session: %s\r\n"
			"\r\n";
	
	sprintf(totbuf, options, rtsp_ipstr, rtsp_filename, seq++, rtsp_session_str);
        int ret, i;

        printf("=====================\n");
        /*
         * Send request to nvsd server.
         */
        if(send(socketfd, totbuf, strlen(totbuf), 0) <=0 ) {
                perror("Socket recv.");
                exit(1);
        }
        printf("sending: <%s>\n", totbuf);

        /*
         * Read response from nvsd server.
         */
        ret = recv(socketfd, totbuf, MAX_BUF_SIZE, 0);
        if(ret<0) {
                perror("Socket recv.");
                exit(1);
        }
        totbuf[ret] = 0;

        printf("receiving (%d): <", ret);
	for(i=0;i<ret;i++) {
		if(totbuf[i]!='$') {
			fputc(totbuf[i], stdout);
		}
		else {
			break;
		}
	}
        printf(">\n");


	//rtsp_socket_handshake();

	rtsp_saveto_file(&totbuf[i], ret-i);

	printf("PLAY handshake done ....\n\n");
	return 1;
}

static int rtsp_send_TEARDOWN(void)
{
        char * options ="TEARDOWN rtsp://%s/%s RTSP/1.0\r\n"
                        "CSeq: %d\r\n"
			"Session: %s\r\n"
                        "\r\n";

        sprintf(totbuf, options, rtsp_ipstr, rtsp_filename, seq++, rtsp_session_str);
        rtsp_socket_handshake();

        printf("TEARDOWN handshake done ....\n\n");
        return 1;
}


static void rtsp_close_socket(void)
{
	if(socketfd != -1) {
		shutdown(socketfd, 0);
		close(socketfd);
		socketfd = -1;
	}
}


static void rtsp_launch_socket_loop(void)
{
	int ret;

	rtsp_init_socket();
	//rtsp_send_OPTIONS();
	rtsp_send_DESCRIBE();
	rtsp_send_SETUP();
	rtsp_send_PLAY();
	rtsp_send_TEARDOWN();
	rtsp_close_socket();
	return;
}

////////////////////////////////////////////////////////////
// File operation functions
////////////////////////////////////////////////////////////

static void rtsp_open_file(void)
{
	elfd = fopen(rtsp_output_filename, "wb");
        if(elfd == NULL) {
		printf("Failed to open output file %s (%s)\n", rtsp_output_filename, strerror(errno));
		exit(1);
	}
}

static void rtsp_saveto_file(char * p, int size)
{
        int fret, totret;

	totret = size;
	while(1) {
		fret = fwrite(totbuf, 1, size, elfd);
		if(fret != size) {
			printf("fwrite failure\n");
			return;
		}

		//printf("Save %d bytes into file %s (%d==%d)\n", totret, rtsp_output_filename, size, fret);

        	size = recv(socketfd, totbuf, MAX_BUF_SIZE, 0);
        	if(size<=0) {
			return;
		}
		totret += size;

		//if(totret >= 9699328) {
		//return;
		//}
	}
	return;
}

static void rtsp_close_file(void)
{
	if(elfd) {
		fclose(elfd);
		elfd=NULL;
	}
}

////////////////////////////////////////////////////////////
// Supporting functions
////////////////////////////////////////////////////////////

static void rtsp_parse_server(void)
{
	char * p;
	char * server = strdup(&rtsp_uri[7]);
	// uri: rtsp://172.19.172.20/touhou.zillion

	p = strchr(server, '/');
	if(!p) {
		printf("rtsp_parse_server: error. uri=%s\n", rtsp_uri);
		exit(1);
	}
	*p=0;
	p++;
	rtsp_filename = strdup(p);

	p=strchr(server, ':');
	if(!p) {
		rtsp_port=554;
	}
	else {
		*p=0;
		p++;
		rtsp_port=atoi(p);
	}
	rtsp_ip=inet_addr(server);
	rtsp_ipstr=strdup(server);

	free(server);

	printf("filename=%s ip=0x%x port=%d\n", rtsp_filename, rtsp_ip, rtsp_port);
}

static void usage(void)
{
        printf("\n");
        printf("nknrtsp - rtsp test client for zillion\n");
        printf("Usage: \n");
        printf("-u <uri> : default: %s\n", rtsp_uri);
        printf("-f <filename> : default: %s\n", rtsp_output_filename);
        printf("\n");
        exit(1);
}

static void catcher(int sig)
{
	rtsp_close_socket();
	rtsp_close_file();
        exit(1);
}

////////////////////////////////////////////////////////////////////////
// main Functions
////////////////////////////////////////////////////////////////////////

int main(int argc, char * argv[])
{
    int ret;

    while ((ret = getopt(argc, argv, "u:f:h")) != -1) {
        switch (ret) {
        case 'u':
             rtsp_uri=optarg;
             break;
        case 'f':
             rtsp_output_filename=optarg;
             break;
        case 'h':
	     usage();
             break;
        }
    }

        signal(SIGHUP,catcher);
        signal(SIGKILL,catcher);
        signal(SIGTERM,catcher);
        signal(SIGINT,catcher);
        signal(SIGPIPE,SIG_IGN);

    rtsp_parse_server();

    rtsp_open_file();
    rtsp_launch_socket_loop();
    rtsp_close_file();
    return 1;
}


