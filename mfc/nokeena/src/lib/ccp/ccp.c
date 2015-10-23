/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains implementation of the common functions of
 * the Cache control protocol library
 *
 * Author: Srujan
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "nkn_ccp.h"
#include "nkn_debug.h"

#define MAX_COMMAND_LEN 4096
#define MAX_URL_SIZE 4096
char *ccp_source_name;
char *ccp_dest_name;

struct ccp_epoll_struct *
nkn_ccp_create (int32_t num_fds)
{
    struct ccp_epoll_struct *node;
    node = (struct ccp_epoll_struct *) nkn_calloc_type (1,
					    sizeof(struct ccp_epoll_struct),
					    mod_ccp_epoll);
    if (!node) {
	DBG_LOG(SEVERE, MOD_CCP, "Calloc Failed");
	return(NULL);
    }
    node->epoll_fd = epoll_create (num_fds);
    node->events = (struct epoll_event *) nkn_calloc_type (num_fds,
						sizeof (struct epoll_event),
						mod_ccp_epoll);
    return (node);
}

/*
 * API to send data over the socket.
 */
int32_t
nkn_ccp_send_message (int32_t sock, char *buf)
{
    ssize_t len = strlen(buf) - 1;
    ssize_t ret;
try_again:;
    if ((ret = send(sock, buf, len, 0)) == -1) {
	if(errno == EAGAIN)
	    goto try_again;
	DBG_LOG(ERROR, MOD_CCP, "Send Failed, errno: %d", errno);
	glob_ccp_send_failures++;
	goto handle_err;
    } else {
	if(ret != len) {
	    buf += ret;
	    len -= ret;
	    goto try_again;
	}
	//DBG_LOG (MSG, MOD_CCP, "Message sent: %s fd: %d\n", buf, sock);
	return 0;
    }

handle_err:
    return -1;
}

/*
 * API to recv data over the socket.
 */
int32_t
nkn_ccp_recv_message (int32_t sock, char *buf)
{
    int32_t n;
    if ((n = recv(sock, buf, MAX_COMMAND_LEN - 1, 0)) < 0) {
	DBG_LOG (MSG, MOD_CCP, "Recv failed: %d", errno);
	glob_ccp_recv_failures++;
	goto handle_err;
    } else {
	//DBG_LOG (MSG, MOD_CCP, "rxd: %s fd: %d\n", buf, sock);
	return(n);
    }

handle_err:
    return -1;
}


void
nkn_ccp_nonblock(int sockfd)
{
    int opts;
    opts = fcntl(sockfd, F_GETFL);
    if(opts < 0) {
        DBG_LOG (ERROR, MOD_CCP, "%s", strerror(errno));
	return;
    }
    opts = (opts | O_NONBLOCK);
    if(fcntl(sockfd, F_SETFL, opts) < 0) {
        DBG_LOG (ERROR, MOD_CCP, "%s", strerror(errno));
    }
}

int32_t
nkn_ccp_start_xml_payload (ccp_xml_t *xmlnode, ccp_command_types_t command,
			    int pid, int tid)
{
    char buf[256];

    memset (buf, 0, 256);
    switch (command) {
	case CCP_POST:
	    sprintf (buf, "%s %d", "POST", pid);
	    break;
	case CCP_POSTACK:
	    sprintf (buf, "%s %d %d", "POSTACK", pid, tid);
	    break;
	case CCP_QUERY:
	    sprintf (buf, "%s %d", "QUERY", tid);
	    break;
	case CCP_QUERYACK:
	    sprintf (buf, "%s %d", "QUERYACK", tid);
	    break;
	case CCP_ABRT:
	    sprintf (buf, "%s %d", "ABRT", tid);
	    break;
	case CCP_ABRTACK:
	    sprintf (buf, "%s %d", "ABRTACK", tid);
	    break;
	default:
	    DBG_LOG (MSG, MOD_CCP, "Invalid command\n");
	    glob_ccp_start_payload_err_cnt++;
	    return (-1);
    }

    xmlIndentTreeOutput = 1;
    xmlnode->doc = xmlNewDoc(BAD_CAST "1.0");
    xmlnode->rootNode = xmlNewNode(NULL, BAD_CAST "ccp");
    xmlDocSetRootElement(xmlnode->doc, xmlnode->rootNode);
    xmlNewChild(xmlnode->rootNode,
		NULL, BAD_CAST "source", BAD_CAST ccp_source_name);
    xmlNewChild(xmlnode->rootNode,
		NULL, BAD_CAST "destination", BAD_CAST ccp_dest_name);
    xmlnode->cmdNode = xmlNewChild(xmlnode->rootNode,
		NULL, BAD_CAST "command", BAD_CAST buf);
    return 0;
}

int32_t
nkn_ccp_end_xml_payload (ccp_xml_t *xmlnode)
{
    int32_t buffersize;
    xmlDocDumpFormatMemory(xmlnode->doc, &xmlnode->xmlbuf, &buffersize, 1);
    return 0;
}

/*
 * API to check if the data in buf is a proper xml or not
 * Valid xml: return -1 else return 0
 */
int32_t
nkn_ccp_check_if_proper_xml (char *buf, int64_t size)
{
    xmlDocPtr doc;
    doc = xmlReadMemory(buf, size, NULL, NULL,
	    XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (doc) {
	DBG_LOG (MSG, MOD_CCP, "Valid xml\n");
	xmlFreeDoc(doc);
	return -1;
    } else {
	return 0;
    }
}

/*
 * Api to parse the command in the xml payload and extract the pid/tid number
 * pid for POST message, tid for rest of the messages.
 */
int32_t
nkn_ccp_get_pid_tid (xmlChar *command)
{
    int32_t id;
    char *c;
    c = strrchr ((char *)command, ' ');
    if (!c) {
	glob_ccp_pid_tid_errors++;
        DBG_LOG (MSG, MOD_CCP, "Invalid command\n");
	return (-1);
    }
    c++;
    if (*c >= '0' && *c <= '9') {
        id = atoi (c);
    }
    return (id);
}
