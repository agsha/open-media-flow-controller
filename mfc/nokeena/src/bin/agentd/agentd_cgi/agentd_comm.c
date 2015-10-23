#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/syslog.h>
#include <sys/poll.h>

#define MAXLEN 8192
#define UNIX_PATH_MAX 256

#define AGENTD_SOCKET "/vtmp/ingester/uds-agentd-server"
#define LOGFILE "/var/tmp/agentd_comm.log"
#define STRING_LENGTH 8192 
//#define POLL_TIMEOUT 5000
#define POLL_TIMEOUT -1 /* Note: Timeout is set to infinite. This could be blocked on agentd forever, if agentd does not respond! */ 

#define ERROR_XML(cd,msg) printf("<?xml version=\"1.0\"?><mfc-response><header><status><code>%d</code><message>%s</message></status></header></mfc-response>",cd,msg);

typedef struct _agentd_cgi_hdr {
    unsigned long data_len;
} agentd_cgi_hdr;

typedef struct _agentd_message  {
    agentd_cgi_hdr hdr;
    char mfc_data[0];
} agentd_message;

/* Error codes

1001 Poll failed
1002 Agentd not responding 
1003 Socket creation failed
1004 Connect failed
1005 Post data inappropriate
1006 Receiving response from agentd failed
1007 Sending request to agentd failed

*/

static int senddata(char *data)
{
    struct sockaddr_un address;
    int  socket_fd, n, addr_len;
    int total_len;
    char buffer[MAXLEN] ;
    struct pollfd ufds[1];
    int rv = 0;
    unsigned int nbytes = 0;
    int sent = 0;
    agentd_message * msg = NULL; 
    agentd_cgi_hdr hdr = {0}; 
    int data_len = 0;
    unsigned long total_recv_size = 0;

    socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if(socket_fd < 0)
    {
        ERROR_XML(1004,"Socket creation failed");
	return 1;
    }

    data_len = strlen(data);
    total_len = sizeof(agentd_message) + data_len;
    msg = (agentd_message *)malloc (total_len + 1); 
    if (msg == NULL) {
        syslog (LOG_ERR, "Error allocating memory for agent buffer : %d\n", total_len);
        ERROR_XML(1007, "Sending request to agentd failed");
        return 1;
    }
    memset (msg, 0, total_len+1); 
    msg->hdr.data_len = data_len;
    memcpy(msg->mfc_data, data, data_len);

    address.sun_family = AF_UNIX;
    n = snprintf(address.sun_path, UNIX_PATH_MAX, AGENTD_SOCKET);
    addr_len = sizeof(address.sun_family) + n;

    if(connect(socket_fd,
           (struct sockaddr *) &address,
           addr_len) != 0)
    {
        ERROR_XML(1003,"Connect failed");
	close(socket_fd);
        if (msg) free (msg); 
	return 1;
    }

    while (sent < total_len) {
        n = write(socket_fd, msg+sent, total_len-sent);
        if (n < 0) {
            syslog(LOG_ERR, "Error while sending : %s", strerror(errno));
            syslog(LOG_ERR, "Sent %d(tot=%d) to agentd", sent, total_len);
            ERROR_XML(1007, "Sending request to agentd failed");
            close(socket_fd);
            if (msg) free (msg); 
            return 1;
        } 
        sent += n;
    }
    syslog(LOG_DEBUG, "sent %d(tot=%d) to agentd", sent, total_len);
    if (msg) free (msg); 
    //Poll on the socket for read
    ufds[0].fd = socket_fd;
    ufds[0].events = POLLIN;

    rv = poll(ufds,1,POLL_TIMEOUT);
    if (rv == -1) {
        ERROR_XML(1001,"Polling Failed");
    } else if (rv == 0) {
        ERROR_XML(1002,"Agentd not responding");
    } else {
        if(ufds[0].revents & POLLIN) {
            nbytes = read (socket_fd, &hdr, sizeof (agentd_cgi_hdr));
            if (nbytes < 1) {
                ERROR_XML(1006, "Receiving response from agentd failed");
                close (socket_fd);
                return 1;
            }
            syslog (LOG_DEBUG, "Agentd response len = %ld", hdr.data_len);
            nbytes = 0;
            total_recv_size = 0;
            while (total_recv_size != hdr.data_len) {
                nbytes = read(socket_fd, buffer, sizeof(buffer)-1);
                if (nbytes < 1) {
                    ERROR_XML(1006, "Receiving response from agentd failed");
                    close (socket_fd);
                    return 1;
                }
                total_recv_size += nbytes;
                buffer[nbytes] = 0;
                printf("%s", buffer);
                fflush(stdout);
            }
            printf("\n");
            fflush(stdout);
        }
    }
    return 0;
}
#if 0
static void unencode(char *src, char *last, char *dest)
{
    for(; src != last; src++, dest++)
	if(*src == '+') {
	    *dest = ' ';
	} else if(*src == '%') {
	    int code;
	    if(sscanf(src+1, "%2x", &code) != 1) code = '?';
	    *dest = code;
	    src +=2; 
	}  else {
	    *dest = *src;
	}
	*dest = '\n';
	*(dest+1) = '\0';
}
#endif

/*
static int save_input_file (const char * input) {
    int result = 0;
    FILE *tmp_fp = NULL;
    tmp_fp = fopen ("/config/nkn/agentd_cgi_input.xml" , "a+");
    if (tmp_fp != NULL) {
        result = fwrite (input, strlen(input), 1, tmp_fp);
        fclose (tmp_fp);
        tmp_fp = NULL;
    }

    syslog (LOG_DEBUG, "fwrite wrote %d items", result);
    return result;
}
*/

int main(void)
{
    char *lenstr = NULL;
    char *input = NULL, *data = NULL;
    unsigned long len=0;
    size_t result=0;
    char *str = NULL;

    openlog("agentd_cgi", LOG_PID, LOG_DAEMON);
    printf("%s%c%c\n",
    "Content-Type:text/xml;charset=iso-8859-1",13,10);

    lenstr = getenv("CONTENT_LENGTH");
    if(lenstr == NULL || sscanf(lenstr,"%ld",&len)!=1 ) {
        ERROR_XML(1005,"Post data inappropriate");
    } else {
	syslog(LOG_DEBUG, "content-length=> %s", lenstr);
	input = (char *)malloc(len+ 1);
	memset(input,0,len+1);
	data = (char *)malloc(len+1);
	memset(data,0,len+1);
        while (result < len) {
    	    result += fread(input+result,1,len-result,stdin); 
            syslog (LOG_DEBUG, "fread read %ld items", result);
        }
        /* save_input_file (input); */
	/*TODO:
	 * commenting out this function call for now 
	 * it is introducing junk characters in the DATA causing parser error.
	 * will revisit this function.
	 */
        //unencode(input, input+len, data);
	syslog(LOG_DEBUG, "after: input %s, data: %s", input, data);
        //str = strstr(data,"<");
        str = strstr(input,"<");
        if(str) {
	    senddata(str);
        } else {
            ERROR_XML(1005,"Post data inappropriate");
        }
	if (data) {
	    free(data);
	}
	if (input) {
	    free(input);
	}
    }
    return 0;
}
