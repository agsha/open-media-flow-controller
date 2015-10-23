/*
 * Copyright 1997-2000 by Pawel Krawczyk <kravietz@ceti.pl>
 *
 * See http://www.ceti.com.pl/~kravietz/progs/tacacs.html
 * for details.
 *
 * connect.c  Open connection to server.
 */

#include <netinet/in.h>
#include <netdb.h>
#include <syslog.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include "tacplus.h"
#include "libtac.h"

/*
 * take server->hostname, and convert it to server->ip and server->port
 */
static int
host2server(tacacs_server_t * server)
{
    struct hostent *hp;
    char           *p;

    if ((p = strchr(server->hostname, ':')) != NULL) {
	*(p++) = '\0';		/* split the port off from the host name */
    }

    hp = gethostbyname(server->hostname);
    if (!hp) {
	TACDEBUG((LOG_DEBUG, "DEBUG: gethostbyname(%s) failed.\n",
		  server->hostname));
	return (-1);
    }

    server->ip = *((struct in_addr *) hp->h_addr_list[0]);

    /*
     *  If the server port hasn't already been defined, go get it.
     */
    if (!server->port) {
	if (p && isdigit(*p)) {	/* the port looks like it's a number */
	    unsigned int    i = atoi(p) & 0xffff;

	    server->port = htons((u_short) i);
	} else {		/* the port looks like it's a name */
	    struct servent *svp;

	    if (p) {		/* maybe it's not "tacacs" */
		svp = getservbyname(p, "tcp");
		/*
		 * quotes allow distinction from above, lest p be tacacs 
		 */
		TACDEBUG((LOG_DEBUG,
			  "DEBUG: getservbyname('%s', tcp) returned %d.\n",
			  p, errno));
		*(--p) = ':';	/* be sure to put the delimiter back */
	    } else {
		svp = getservbyname("tacacs", "tcp");
		TACDEBUG((LOG_DEBUG,
			  "DEBUG: getservbyname(tacacs, tcp) returned %d.\n",
			  errno));
	    }

	    if (svp == (struct servent *) NULL) {
		/*
		 * if all else fails 
		 */
		server->port = htons(TAC_PLUS_PORT);
	    } else {
		server->port = svp->s_port;
	    }
	}
    }

    return (0);
}

/*
 * Returns a pointer to server structure if a valid connection is made,
 * NULL otherwise
 */
tacacs_server_t *
tac_connect(tacacs_conf_t * conf, tacacs_server_t * server, int loop)
{
    struct sockaddr_in serv_addr;
    int             fd;
    int             sockflags;
    int             retval;
    int             serr;
    socklen_t       serr_len;
    fd_set          wfds;
    struct timeval  tv;

    fd = -1;
    conf->sockfd = -1;

    if (!server) {
	syslog(LOG_ERR, "%s: no TACACS+ servers defined", __FUNCTION__);
	return (NULL);
    }

    while (server) {
	/*
	 * only look up IP information as necessary 
	 */
	if (host2server(server) != 0) {
	    syslog(LOG_ERR,
		   "Failed looking up IP address for TACACS+ server %s",
		   server->hostname);
	    goto next;
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr = server->ip;
	serv_addr.sin_port = server->port;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    syslog(LOG_WARNING,
		   "%s: socket creation error for %s: %m",
		   __FUNCTION__, inet_ntoa(serv_addr.sin_addr));
	    goto next;
	}

	/*
	 * Make the socket non-blocking 
	 */
	if ((sockflags = fcntl(fd, F_GETFL, 0)) < 0) {
	    syslog(LOG_WARNING, "%s: could not get socket flags "
		   "on to %s: %m", __FUNCTION__,
		   inet_ntoa(serv_addr.sin_addr));
	    goto next;
	}

	sockflags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, sockflags) < 0) {
	    syslog(LOG_WARNING, "%s: could not set socket to "
		   "non-blocking for %s: %m", __FUNCTION__,
		   inet_ntoa(serv_addr.sin_addr));
	    goto next;
	}

	/*
	 * Try connecting to the server 
	 */
	if (connect(fd, (struct sockaddr *) &serv_addr,
		    sizeof(serv_addr)) < 0) {
	    if (errno != EINPROGRESS) {
		syslog(LOG_WARNING,
		       "%s: connection to %s failed: %m",
		       __FUNCTION__, inet_ntoa(serv_addr.sin_addr));
		goto next;
	    }
            errno = 0;

	    /*
	     * In theory waiting on a response from the server 
	     */
	    FD_ZERO(&wfds);
	    FD_SET(fd, &wfds);
	    tv.tv_sec = server->timeout;
	    tv.tv_usec = 0;

            retval = select(fd + 1, NULL, &wfds, NULL, &tv);
	    if (retval < 0) {
		syslog(LOG_WARNING, "%s: select on socket "
		       "failed for %s: %m", __FUNCTION__,
		       inet_ntoa(serv_addr.sin_addr));
		goto next;
	    }
            else if (retval == 0) {
                syslog(LOG_WARNING, "%s: server connection timeout "
		       "of %d seconds expired for %s", __FUNCTION__,
                       server->timeout,
                       inet_ntoa(serv_addr.sin_addr));
                goto next;
            }

	    /*
	     * The connection was made
	     */
            serr = 0;
            serr_len = sizeof(serr);
	    retval = getsockopt(fd, SOL_SOCKET, SO_ERROR, &serr, &serr_len);
	    if ((retval < 0) || (serr != 0)) {
		syslog(LOG_WARNING,
		       "%s: connection to %s failed (getsockopt): %m",
		       __FUNCTION__, inet_ntoa(serv_addr.sin_addr));
		goto next;
	    }
	}

	/* remove non-blocking, as tacacs read/write usage expect blocking */
	sockflags &= ~O_NONBLOCK;
	if (fcntl(fd, F_SETFL, sockflags) < 0) {
	    syslog(LOG_WARNING, "%s: could not clear socket "
		   "non-blocking flag for %s: %m", __FUNCTION__,
		   inet_ntoa(serv_addr.sin_addr));
	    goto next;
	}

	/*
	 * connected ok 
	 */
	TACDEBUG((LOG_DEBUG, "%s: connected to %s", __FUNCTION__,
		  inet_ntoa(server->ip)));

	conf->sockfd = fd;
        fd = -1;
	server->seq_no = TAC_PLUS_INIT_SEQNO;

	return (server);
      next:
        if (fd != -1) {
            close(fd);
            fd = -1;
        }
	if (!loop) {
	    break;
	}

	server = server->next;
    }

    /*
     * all attempts failed 
     */
    if (loop) {
	syslog(LOG_ERR, "%s: all possible TACACS+ servers failed",
	       __FUNCTION__);
    }
    return (NULL);

}				/* tac_connect */


tacacs_server_t *
tac_connect_single(tacacs_conf_t * conf, tacacs_server_t * server)
{
    return (tac_connect(conf, server, FALSE));
}				/* tac_connect_single */

void
reset_server(tacacs_server_t *server, int clear_seq)
{
    if (server == NULL) {
	return;
    }

    if (server->server_msg) {
	free(server->server_msg);
	server->server_msg = NULL;
    }
    if (server->data) {
	free(server->data);
	server->data = NULL;
    }

    if (clear_seq) {
	server->seq_no = TAC_PLUS_INIT_SEQNO;
    }
}
