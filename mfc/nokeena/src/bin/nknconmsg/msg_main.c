#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


uint32_t msg_serverip;
int msg_serverport;
extern int msg_parse_line(const char *line);
extern void process_cmd(char * cmd);

static void msg_init(void);
static void usage(void);
static void version(void);
void catcher(int sig);

////////////////////////////////////////////////////////////
// Supporting functions
////////////////////////////////////////////////////////////

static void msg_init(void)
{
	msg_serverip = inet_addr("127.0.0.1");
	msg_serverport = 7957;
}

static void usage(void)
{
	printf("\n");
	printf("nkndebug - Nokeena Run Time Debug Tool\n");
	printf("Usage:\n");
        printf("-d        : input debug information ip\n");
        printf("          :       p [/x] [/s] symbol\n");
        printf("          :       w symbol value\n");
        printf("          :       call func arg1\n"); 
        printf("-p        : server port (default: 7957)\n");
        printf("-s        : server ip (default: 127.0.0.1)\n");
        printf("-x        : exit the nvsd\n");
        printf("-y        : confirm the operation\n");
        printf("-v        : show version\n");
        printf("-h        : show this help\n");
	printf("\n");
        printf("Examples:\n");
        printf("1) To display a random varible value:\n");
	printf("   	  nknconmsg -y -d \"p cur_datestr\"\n");
	printf("\n");
	exit(1);
}

static void version(void)
{
	printf("\n");
	printf("nkndebug - Nokeena Run Time Debug Tool\n");
	printf("Build Date: "__DATE__ " " __TIME__ "\n");
	printf("\n");
	exit(1);

}

void catcher(int sig)
{
	exit(1);
}


////////////////////////////////////////////////////////////
// Main function
////////////////////////////////////////////////////////////

int main(int argc, char * argv[])
{
	int ret;
	int confirmed=0;
	char * cmd = NULL;
	char arg[200];

	msg_init();
	while ((ret = getopt(argc, argv, "p:s:d:xyhvD:")) != -1) {
		switch (ret) {
		case 'd':
			cmd = strdup(optarg);
		break;
		case 'p':
			msg_serverport = atoi(optarg);
		break;
		case 's':
			msg_serverip = inet_addr(optarg);
		break;
		case 'x':
			cmd=strdup("call server_exit()");
		break;
		case 'y':
			confirmed=1;
		break;
		case 'h':
			usage();
		break;
		case 'v':
			version();
		break;
		}
	}

	/* Need confirmed to avoid mis-operation of debug */
	if(confirmed == 0) {
		printf("You need to add -y to confirm your operation\n");
		usage();
		return 0;
	}

	signal(SIGHUP,catcher);
	signal(SIGKILL,catcher);
	signal(SIGTERM,catcher);
	signal(SIGINT,catcher);

	if(cmd) msg_parse_line(cmd);

	printf("\n\tDone\n");

	return 1;
}


