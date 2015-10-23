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

int udpsocket;
int socketfd = -1;
char totbuf[MAX_BUF_SIZE];
uint32_t rtsp_ip;
char * rtsp_ipstr;
char * localip = "172.19.172.203";
int rtsp_port;
char * rtsp_filename;
char rtsp_session_str[100];
unsigned int client_port;
unsigned int udp_port = 27389;
FILE * elfd = NULL;
char * rtsp_output_filename="MP2T.ts";
char * rtsp_uri="rtsp://172.19.172.14/testvideo.mpg";
int seq = 0;
int should_stop = 0;


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

static int rtsp_init_udp_socket(void)
{
	struct sockaddr_in si_me;

	if ((udpsocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
		printf("rtsp_init_udp_socket: failed\n");
		exit(2);
	}
    
	memset((char *) &si_me, 0, sizeof(si_me));
again:
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(udp_port);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(udpsocket, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
		udp_port ++;
		goto again;
	}
	printf("rtsp_init_udp_socket udp listen on port=%d\n", udp_port);

	return 0;
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

#if 0
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
#endif // 0

static int rtsp_send_SETUP(void)
{
	char * p, *p2;
	char * options ="SETUP rtsp://%s/%s RTSP/1.0\r\n"
			"User-Agent: MBASE_MPEG_PLAYER_KA\r\n"
			"Transport: RAW/RAW/UDP;unicast;destination=%s;port=%d\r\n"
			"CSeq: %d\r\n"
			"\r\n";
	
	sprintf(totbuf, options, rtsp_ipstr, rtsp_filename, localip, udp_port, seq++);
	rtsp_socket_handshake();

	/* get session string information */
	p = strstr(totbuf, "Session:");
	if(!p) {
		printf("Cannot find session string\n");
		exit(1);
	}
	p+=9;
	p2 = rtsp_session_str;
	while(*p != ';' && *p!='\r') {
		*p2++ = *p++;
	}
	*p2 = 0;

	/* get UDP port information */
	p = strstr(totbuf, "client_port=");
	if(!p) {
		printf("Cannot find client_port string\n");
		exit(1);
	}
	p+=12;
	client_port = atoi(p);

	printf("SETUP handshake done session string = %s client_port=%d....\n\n", rtsp_session_str, client_port);
	return 1;
}

static int rtsp_send_PLAY(void)
{
	char * options ="PLAY rtsp://%s/%s RTSP/1.0\r\n"
			"User-Agent: MBASE_MPEG_PLAYER_KA\r\n"
			"Scale: 1.000\r\n"
			"Range: npt=0.000-\r\n"
			"CSeq: %d\r\n"
			"Session: %s\r\n"
			"\r\n";
	struct sockaddr_in si_other;
	int slen=sizeof(si_other);
	ssize_t rlen;
	
	sprintf(totbuf, options, rtsp_ipstr, rtsp_filename, seq++, rtsp_session_str);
	rtsp_socket_handshake();

	should_stop = 0;
	while(should_stop == 0) {
		rlen = recvfrom(udpsocket, totbuf, MAX_BUF_SIZE, 0, (struct sockaddr *)&si_other, &slen);
		if (rlen == -1) {
			printf("recvfrom: failed\n");
			exit(3);
		}
		printf("Received packet from 0x%x:%d   Data Length=%d\n", 
			si_other.sin_addr.s_addr, ntohs(si_other.sin_port), rlen);
	}

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
	close(udpsocket);
}


static void rtsp_launch_socket_loop(void)
{
	int ret;

	rtsp_init_udp_socket();
	rtsp_init_socket();
	//rtsp_send_OPTIONS();
	//rtsp_send_DESCRIBE();
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

		if(totret >= 9699328) {
			return;
		}
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


