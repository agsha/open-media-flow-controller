/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains implementation of the server related functions of
 * the Cache control protocol library
 *
 * Author: Srujan
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "nkn_ccp_server.h"
#include "nkn_debug.h"
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define MAX_FDS 25
#define MAX_COMMAND_LEN 1024
#define MAX_DATA_SIZE 4096
#define MAX_CONNECTIONS 25

/*
 * This is the maximum waiting period for the client
 * before it can query the status of the previously posted task.
 */
#define MAX_CCP_SERVER_WAIT_TIME 30

AO_t g_ccp_tid;
void(*g_ccp_server_dest_callback)(ccp_client_data_t *);
struct ccp_epoll_struct *g_ccp_server_epoll_node;
int32_t g_ccp_server_sock;
pthread_mutex_t server_ctxt_list_mutex;
TAILQ_HEAD(ccp_server_ctxt_list_t, ccp_server_session_ctxt)
	    g_ccp_server_ctxt_list;

char query_message[CCP_MAX_STATUS+1][MAX_STATUS_LEN] = {
    "UNKNOWN",
    "OK",
    "NOT_STARTED",
    "IN_PROGRESS",
    "INVALID",
    "NOT_OK"
};

char action_message[CCP_MAX_ACTION+1][MAX_STATUS_LEN] = {
    "UNKNOWN",
    "add",
    "delete",
    "list",
};

int32_t nkn_ccp_server_retry_start_session(void);

static int32_t
nkn_ccp_server_start_session(void)
{
    int err;

    g_ccp_server_epoll_node = nkn_ccp_create(255);

    err = nkn_ccp_server_open_tcp_conn(&g_ccp_server_sock);
    if (err == -1) {
	goto handle_err;
    }
    return(0);

handle_err:
    return(-1);
}

static void *
nkn_ccp_server_process_epoll_events(void *data __attribute((unused)))
{
    int32_t nfds, n, err;
    int client_fd;
    struct epoll_event ev;
    struct ccp_inout_buf *inout_data;

    while (1) {
	nfds = epoll_wait(g_ccp_server_epoll_node->epoll_fd,
			g_ccp_server_epoll_node->events, MAX_CONNECTIONS, -1);

	if (nfds < 0) {
	    if(errno == EINTR)
		nfds = 0;
	}

	for (n = 0; n< nfds; n++) {

	    inout_data = (ccp_inout_buf_t *)
				g_ccp_server_epoll_node->events[n].data.ptr;

	    if (g_ccp_server_epoll_node->events[n].events & (EPOLLERR|EPOLLHUP)) {
		epoll_ctl(g_ccp_server_epoll_node->epoll_fd,
					EPOLL_CTL_DEL, inout_data->sock, NULL);
		DBG_LOG(MSG, MOD_CCP, "Received EPOLLERR event");
		close(inout_data->sock);
		if (inout_data->sock == g_ccp_server_sock) {
		    err = nkn_ccp_server_retry_start_session();
		    if (err == -1) {
			nkn_ccp_server_cleanup_inout_buffer(inout_data);
		    }
		} else {
		    if (inout_data->ptr == NULL) {
	    		nkn_ccp_server_cleanup_inout_buffer(inout_data);
		    } else {
	    		inout_data->conn_close = 1;
		    }
		}
		continue;
	    }

	    if (inout_data->sock == g_ccp_server_sock &&
		    (g_ccp_server_epoll_node->events[n].events & EPOLLIN)) {
		while (1) {
		    client_fd = accept(g_ccp_server_sock, NULL, NULL);

		    if (client_fd == -1) {
			break;
		    } else {
			DBG_LOG(MSG, MOD_CCP, "NEW connection accepted\n");
			ev.events = EPOLL_IN_EVENTS;
			inout_data = (ccp_inout_buf_t *)nkn_calloc_type(1,
						    sizeof(ccp_inout_buf_t),
						    mod_ccp_inoutdata);
			inout_data->sock = client_fd;
			inout_data->ptr = NULL;
			ev.data.ptr = inout_data;
			if (epoll_ctl(g_ccp_server_epoll_node->epoll_fd,
					EPOLL_CTL_ADD, client_fd, &ev) < 0) {
			    if (errno == EEXIST ) {	/* errno 17 */
				DBG_LOG(MSG, MOD_CCP, "Adding listen fd: %s", strerror(errno));
			    } else {
				DBG_LOG(ERROR, MOD_CCP, "Adding listen fd: %s", strerror(errno));
			    }
			}
		    }
		}
	     } else {
		if (g_ccp_server_epoll_node->events[n].events & EPOLLIN) {
		    nkn_ccp_server_read_data(inout_data);
		} else if (g_ccp_server_epoll_node->events[n].events &
				    EPOLLOUT) {
		    nkn_ccp_server_send_data(inout_data);
		}
	     }
	}
    }

    return(0);
}

extern char *ccp_source_name, *ccp_dest_name;
void
nkn_ccp_server_init(void(*dest_callback_func)(ccp_client_data_t *),
			char *source_name, char *dest_name)
{
    int s;
    int stack_size = 256*1024;
    pthread_attr_t attr;
    TAILQ_INIT(&g_ccp_server_ctxt_list);
    ccp_source_name = nkn_strdup_type(source_name, mod_ccp_module);
    ccp_dest_name = nkn_strdup_type(dest_name, mod_ccp_module);
    pthread_t epoll_thrdid;
    xmlInitParser();
    g_ccp_server_dest_callback = dest_callback_func;
    nkn_ccp_server_start_session();
    pthread_mutex_init(&server_ctxt_list_mutex, NULL);
    s = pthread_attr_init(&attr);
    if (s!= 0) {
	DBG_LOG(ERROR, MOD_CCP, "Error in pthread_attr_init\n");
	return;
    }
    s = pthread_attr_setstacksize(&attr, stack_size);
    if (s!= 0) {
	DBG_LOG(ERROR, MOD_CCP, "Error in pthread_attr_init\n");
	return;
    }
    pthread_create(&epoll_thrdid,& attr, nkn_ccp_server_process_epoll_events,
		    NULL);
}

static int32_t
nkn_ccp_server_change_epoll_events(int event_flags, int32_t fd,
				    ccp_inout_buf_t *inout_data,
				    char *crawl_name)
{
    DBG_LOG(MSG, MOD_CCP, "[%s] epoll_out: fd: %d",
		(crawl_name ? crawl_name: "NULL"), fd);
    struct epoll_event ev;
    ev.events = event_flags;
    ev.data.ptr = inout_data;
    if (epoll_ctl(g_ccp_server_epoll_node->epoll_fd,
		    EPOLL_CTL_MOD, fd, &ev) < 0) {
	DBG_LOG(ERROR, MOD_CCP, "[%s] Change epoll event: %s",
		(crawl_name ? crawl_name: "NULL"), strerror(errno));
	if(errno == EEXIST)
	    return 0;
	return -1;
    }
    return(0);
}

static int32_t
nkn_ccp_server_insert_query_status(ccp_xml_t *server_response,
				    query_status_t query_status)
{
    xmlNewChild(server_response->rootNode, NULL, BAD_CAST "status",
		BAD_CAST query_message[query_status]);
    return 0;
}

static int32_t
nkn_ccp_server_form_invalid_message(ccp_inout_buf_t *inout_data,
				    ccp_command_types_t command, int tid)
{
    int err;

    inout_data->server_response = (ccp_xml_t *)nkn_calloc_type(1,
						    sizeof(ccp_xml_t),
						    mod_ccp_inoutdata);

    err = nkn_ccp_start_xml_payload(inout_data->server_response,
				    command, 0, tid);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_server_insert_query_status(inout_data->server_response,
						CCP_INVALID);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_end_xml_payload(inout_data->server_response);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_server_change_epoll_events(EPOLL_OUT_EVENTS,
					    inout_data->sock, inout_data, NULL);

    if (err == -1)
	goto handle_err;

    return 0;

handle_err:
    if(inout_data->server_response)
	nkn_ccp_server_cleanup_send_buf(&inout_data->server_response);
    return(-1);
}

static void
nkn_ccp_server_generate_asx_data(ccp_urlnode_t *urlnode, char *crawl_name)
{
    char temp[2048];
    char *url, *ptr;
    url = nkn_strdup_type(urlnode->url, mod_ccp_url);
    if (url){
	ptr = strstr(url, ".asx");
	if (!ptr) {
	    DBG_LOG(ERROR, MOD_CCP, "[%s] url doesn't have asx extension,"
		    "even if asx_gen attribute is set",
		    (crawl_name ? crawl_name:"NULL"));
	    return;
	} else {
	    *ptr = '\0';
	}
    } else {
	DBG_LOG(ERROR, MOD_CCP, "[%s] strdup failed",
		(crawl_name ? crawl_name:"NULL"));
	return;
    }

    sprintf(temp, "<ASX version=\"3.0\">\n"
	    "<ENTRY>\n"
	    "<ref HREF=\"%s\"/>\n"
	    "</ENTRY>\n"
	    "</ASX>", url);
    if (url) {
	free(url);
	url = NULL;
    }

    urlnode->data = nkn_strdup_type(temp, mod_ccp_data);
    if (!urlnode->data) {
	DBG_LOG(ERROR, MOD_CCP, "[%s] strdup failed",
		(crawl_name ? crawl_name:"NULL"));
	return;
    }
    urlnode->data_len = strlen(urlnode->data);
}

static ccp_server_session_ctxt_t *
nkn_ccp_parse_xml(ccp_inout_buf_t *inout_data)
{
    char *xmlbuff = inout_data->recv_buf;
    ccp_server_session_ctxt_t *conn_obj = NULL;
    xmlChar *ptr;
    xmlChar *command;
    ccp_urlnode_t *temp_ptr;
    xmlDocPtr doc;
    xmlNodePtr uriNode;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlAttr *attr;
    int32_t tid, pid;

    doc = xmlParseMemory(xmlbuff, strlen(xmlbuff));
    xpathCtx = xmlXPathNewContext(doc);
    xpathObj = xmlXPathEvalExpression((xmlChar *)"/ccp/command", xpathCtx);

    if (xpathObj && xpathObj->nodesetval &&
	    xpathObj->nodesetval->nodeTab && *xpathObj->nodesetval->nodeTab) {
	ptr = (*xpathObj->nodesetval->nodeTab)->children->content;

	command = ptr;

	if (strcasestr((char *)command, "QUERY")) {
	    tid = nkn_ccp_get_pid_tid(command);
	    if (tid == -1)
		goto handle_err;

	    conn_obj = find_server_ctxt(tid);
	    if (conn_obj) {
		conn_obj->client_ctxt.command = CCP_QUERY;
		DBG_LOG(MSG, MOD_CCP, "[%s] Rxd QUERY MESSAGE", 
			(conn_obj->client_ctxt.crawl_name?
			 conn_obj->client_ctxt.crawl_name:
			 "NULL"));
	    } else {
		nkn_ccp_server_form_invalid_message(inout_data, CCP_QUERYACK,
						    tid);
	    }

	} else if (strcasestr((char *)command, "ABRT")) {
	    tid = nkn_ccp_get_pid_tid(command);
	    if (tid == -1)
		goto handle_err;

	    conn_obj = find_server_ctxt(tid);
	    if (conn_obj) {
		conn_obj->client_ctxt.command = CCP_ABRT;
		DBG_LOG(MSG, MOD_CCP, "[%s] Rxd ABRT MESSAGE", 
			(conn_obj->client_ctxt.crawl_name?
			 conn_obj->client_ctxt.crawl_name:
			 "NULL"));
	    } else {
		nkn_ccp_server_form_invalid_message(inout_data, CCP_ABRTACK,
						    tid);
	    }

	} else if (strcasestr((char *)command, "POST")) {
	    pid = nkn_ccp_get_pid_tid(command);
	    if (pid == -1)
		goto handle_err;

	    conn_obj = get_new_server_session_ctxt();
	    conn_obj->client_ctxt.command = CCP_POST;
	    conn_obj->pid = pid;

	    if (xpathObj)
		xmlXPathFreeObject(xpathObj);
	    xpathObj = xmlXPathEvalExpression((xmlChar *)"/ccp/action",
						xpathCtx);

	    if (xpathObj && xpathObj->nodesetval &&
			    xpathObj->nodesetval->nodeTab &&
			    *xpathObj->nodesetval->nodeTab) {
	        uriNode =(*xpathObj->nodesetval->nodeTab);

		attr = (*xpathObj->nodesetval->nodeTab)->properties;

		while(attr) {
		    if (!strcasecmp((char *)attr->name, "Expiry")) {
			if(attr->children)
			    conn_obj->client_ctxt.expiry = atoll((char *)
						    attr->children->content);
		    } else if (!strcasecmp((char *)attr->name, "Client_domain")) {
			if(attr->children)
			    conn_obj->client_ctxt.client_domain = nkn_strdup_type((char *)
						    attr->children->content,
						    mod_ccp_client_domain);
		    } else if (!strcasecmp((char *)attr->name, "instance")) {
			if(attr->children)
			    conn_obj->client_ctxt.crawl_name = nkn_strdup_type((char *)
						    attr->children->content,
						    mod_ccp_crawl_name);
		    } else if (!strcasecmp((char *)attr->name, "Cachepin")) {
			if(attr->children)
			    conn_obj->client_ctxt.cache_pin = atoi((char *)
						    attr->children->content);
		    } else if (!strcasecmp((char *)attr->name, "count")) {
			if(attr->children)
			    conn_obj->client_ctxt.count = atoi((char *)
						    attr->children->content);
		    } else if (strcasestr((char *)attr->name, "type")) {
			if(attr->children) {
			    if (strcasestr(
				(char *)attr->children->content, "add")) {
				conn_obj->client_ctxt.action = CCP_ACTION_ADD;
			    } else if (strcasestr(
				(char *)attr->children->content, "delete")) {
				conn_obj->client_ctxt.action = CCP_ACTION_DELETE;
			    } else if (strcasestr(
				(char *)attr->children->content, "list")) {
				conn_obj->client_ctxt.action = CCP_ACTION_LIST;
			    }
			}
		    }

		    attr = attr->next;
		}
	    }

	    /*
	     * Post message collect the urls and attributes.
	     */
	    if (xpathObj)
		xmlXPathFreeObject(xpathObj);
	    xpathObj = xmlXPathEvalExpression((xmlChar *)"/ccp/action/uri",
						xpathCtx);

	    if (xpathObj && xpathObj->nodesetval &&
		xpathObj->nodesetval->nodeTab &&
				*xpathObj->nodesetval->nodeTab) {
	        uriNode = (*xpathObj->nodesetval->nodeTab);

	        while (uriNode) {
		    if (uriNode->type != XML_ELEMENT_NODE)
			goto next_entry;
	            if (!conn_obj->client_ctxt.url_entry) {
			conn_obj->client_ctxt.url_entry = (ccp_urlnode_t *)
					    nkn_calloc_type(1,
						    sizeof(ccp_urlnode_t),
						    mod_ccp_url_st);
			temp_ptr = conn_obj->client_ctxt.url_entry;
			temp_ptr->next = NULL;
	            } else {
			temp_ptr->next = (ccp_urlnode_t *)nkn_calloc_type(1,
							sizeof(ccp_urlnode_t),
							mod_ccp_url_st);
			temp_ptr = temp_ptr->next;
			temp_ptr->next = NULL;
	            }

		    if(uriNode->children)
			temp_ptr->url = nkn_strdup_type((char *)uriNode->children->content,
							mod_ccp_url);

		    /* Initialize expiry/cache_pin from the global param */
		    temp_ptr->expiry = conn_obj->client_ctxt.expiry;
		    temp_ptr->cache_pin = conn_obj->client_ctxt.cache_pin;

	            attr = uriNode->properties;
	            while (attr) {
			if (strcasestr((char *)attr->name, "Expiry")) {
			    if(attr->children)
				temp_ptr->expiry = atoll((char *)
						    attr->children->content);
			} else if (strcasestr((char *)attr->name, "Cachepin")) {
			    if(attr->children)
				temp_ptr->cache_pin = atoi((char *)
						    attr->children->content);
			} else if (strcasestr((char *) attr->name,
				    "generate_asx")) {
			    if (attr->children)
				temp_ptr->asx_gen_flag = atoi((char *)
						    attr->children->content);
			} else if (strcasestr((char *)attr->name,
				    "domain")) {
			    if (attr->children)
				temp_ptr->domain = nkn_strdup_type((char *)attr->children->content, mod_ccp_url);
			}
			attr = attr->next;
		    }
		    /*
		     * generate asx data if asx_gen_flag is 1.
		     */
		    if (temp_ptr->asx_gen_flag)
			nkn_ccp_server_generate_asx_data(temp_ptr,
					    conn_obj->client_ctxt.crawl_name);
next_entry:;
		    uriNode = uriNode->next;
	        }
	    }
	    conn_obj->dest_callback_func = g_ccp_server_dest_callback;
	    AO_fetch_and_add(&g_ccp_tid, 1);
	    conn_obj->tid = g_ccp_tid;
	    DBG_LOG(MSG, MOD_CCP, "[%s] Rxd POST MESSAGE",
			(conn_obj->client_ctxt.crawl_name?
			 conn_obj->client_ctxt.crawl_name:
			 "NULL"));
	}
    }

    if (xpathCtx)
	xmlXPathFreeContext(xpathCtx);
    if (xpathObj)
	xmlXPathFreeObject(xpathObj);
    if (doc)
	xmlFreeDoc(doc);
    return(conn_obj);

handle_err:
    if (xpathCtx)
	xmlXPathFreeContext(xpathCtx);
    if (xpathObj)
	xmlXPathFreeObject(xpathObj);
    if (doc)
	xmlFreeDoc(doc);
    return(NULL);
}

void
nkn_ccp_server_read_data(ccp_inout_buf_t *inout_data)
{
    int num_bytes;
    ccp_server_session_ctxt_t *conn_obj;
    struct epoll_event ev;

    if (inout_data->recv_buf == NULL) {
	inout_data->recv_buf = (char *)nkn_calloc_type(MAX_DATA_SIZE, 1,
							mod_ccp_recvbuf);
	if (inout_data->recv_buf == NULL) {
	    DBG_LOG(SEVERE, MOD_CCP, "Calloc Failed");
	    return;
	}
	inout_data->offset = 0;
    } else{
	inout_data->recv_buf = (char *)realloc(inout_data->recv_buf,
					    inout_data->offset + MAX_DATA_SIZE);
    }

    num_bytes = recv(inout_data->sock, inout_data->recv_buf +
		    inout_data->offset, (MAX_DATA_SIZE - 1), 0);

    if (num_bytes == 0) {
	DBG_LOG(MSG, MOD_CCP, "Client closed the connection\n");
	close(inout_data->sock);
	epoll_ctl(g_ccp_server_epoll_node->epoll_fd,
	    EPOLL_CTL_DEL, inout_data->sock, NULL);
	if (inout_data->ptr == NULL) {
	    nkn_ccp_server_cleanup_inout_buffer(inout_data);
	} else {
	    inout_data->conn_close = 1;
	}
	return;
    } else if (num_bytes == -1) {
	DBG_LOG(MSG, MOD_CCP, "err: %s", strerror(errno));
	if (errno == EAGAIN) {
	    ev.events = EPOLL_IN_EVENTS;
	    ev.data.ptr = inout_data;
	    if (epoll_ctl(g_ccp_server_epoll_node->epoll_fd, EPOLL_CTL_MOD,
	    	inout_data->sock, &ev) < 0 ){
	     	DBG_LOG(MSG, MOD_CCP, "Error while changing epoll fd\n");
	    }
        }	
	return;
    }
    inout_data->offset += num_bytes;

    inout_data->recv_buf[inout_data->offset] = '\0';
    if (nkn_ccp_check_if_proper_xml(inout_data->recv_buf,
				    inout_data->offset) == 0) {
	/*
	 * Not a proper xml file
	 */
	ev.events = EPOLL_IN_EVENTS;
	ev.data.ptr = inout_data;
        if (epoll_ctl(g_ccp_server_epoll_node->epoll_fd, EPOLL_CTL_MOD,
			inout_data->sock, &ev) < 0 ){
	    DBG_LOG(MSG, MOD_CCP, "Error while changing epoll fd\n");
	}
    } else {
	/*
	 * Parse succesful
	 * create server session context 
	 * nkn_ccp_parse_xml will parse the data,
	 * if the command is a POST, createa a new session ctxt and
	 * populates the entries and returns;
	 * if the command is ABRT/QUERY, searches for taskid and returns
	 * the ctxt.
	 */
	inout_data->recv_buf[inout_data->offset] = '\0';
	conn_obj = nkn_ccp_parse_xml(inout_data);

	if (conn_obj) {
	    conn_obj->inout_data = inout_data;
	    conn_obj->inout_data->ptr = conn_obj;
	    free(inout_data->recv_buf);
	    inout_data->recv_buf = NULL;
	    inout_data->offset = 0;
	    conn_obj->client_ctxt.command_ack = 0;
	    conn_obj->dest_callback_func(&conn_obj->client_ctxt);
	} else if (conn_obj == NULL) {
	    DBG_LOG(MSG, MOD_CCP, "Unable to find context\n");
	}
    }
    return;
}

int32_t
nkn_ccp_server_send_data(ccp_inout_buf_t *inout_data)
{
    int err = nkn_ccp_send_message(inout_data->sock,
			    (char *)inout_data->server_response->xmlbuf);

    nkn_ccp_server_cleanup_send_buf(&inout_data->server_response);
    nkn_ccp_server_change_epoll_events(EPOLL_IN_EVENTS, inout_data->sock,
					inout_data, NULL);
    return(err);
}

void
nkn_ccp_server_ctxt_cleanup(void *ptr)
{
    ccp_server_session_ctxt_t *conn_obj = (ccp_server_session_ctxt_t *)ptr;
    if (conn_obj) {
	/*
	 * wantedly inout_data is not freed
	 * since it is shared by multiple server ctxts corresponsding to single
	 * connection.
	 */
	if (conn_obj->inout_data->conn_close) {
	    nkn_ccp_server_cleanup_inout_buffer(conn_obj->inout_data);
	    conn_obj->inout_data = NULL;
	} else {
	    conn_obj->inout_data->ptr = NULL;
	}

	nkn_ccp_server_cleanup_client_ctxt(&conn_obj->client_ctxt);
	pthread_mutex_lock(&server_ctxt_list_mutex);
	TAILQ_REMOVE(&g_ccp_server_ctxt_list, conn_obj, entries);
	pthread_mutex_unlock(&server_ctxt_list_mutex);
	free(conn_obj);
    }
}

void
nkn_ccp_server_cleanup_client_ctxt(ccp_client_data_t *ptr)
{
    if (ptr) {
	//free client_domain
	if (ptr->client_domain) {
	    free(ptr->client_domain);
	    ptr->client_domain = NULL;
	}
	if (ptr->crawl_name) {
	    free(ptr->crawl_name);
	    ptr->crawl_name = NULL;
	}
	ccp_urlnode_t *urlptr = ptr->url_entry;
	ccp_urlnode_t *urlptr_prev = urlptr;;
	while (urlptr) {
	    free(urlptr->url);
	    if(urlptr->domain)
		free(urlptr->domain);
	    if (urlptr->data)
		free(urlptr->data);
	    urlptr = urlptr->next;
	    free(urlptr_prev);
	    urlptr_prev = urlptr;
	}
    }
}

void
nkn_ccp_server_cleanup_inout_buffer(ccp_inout_buf_t *ptr)
{
    ccp_server_session_ctxt_t *conn_obj = (ccp_server_session_ctxt_t *)ptr->ptr;

    if (ptr) {
	if(conn_obj && (conn_obj->inout_data == ptr)) {
	    conn_obj->inout_data = NULL;
	}
	if (ptr->recv_buf)
	    free(ptr->recv_buf);
	if (ptr->server_response)
	    nkn_ccp_server_cleanup_send_buf(&ptr->server_response);

	free(ptr);
    }
}

void
nkn_ccp_server_cleanup_send_buf(ccp_xml_t **server_response)
{
    if (*server_response) {
	xmlFree((*server_response)->xmlbuf);
	xmlFreeDoc((*server_response)->doc);
	free(*server_response);
	*server_response = NULL;
    }
}

ccp_server_session_ctxt_t *
get_new_server_session_ctxt()
{
    ccp_server_session_ctxt_t *conn_obj;

    conn_obj = (ccp_server_session_ctxt_t *)nkn_calloc_type(1,
					    sizeof(ccp_server_session_ctxt_t),
					    mod_ccp_server_ctxt);
    pthread_mutex_lock(&server_ctxt_list_mutex);
    TAILQ_INSERT_TAIL(&g_ccp_server_ctxt_list, conn_obj, entries);
    pthread_mutex_unlock(&server_ctxt_list_mutex);

    conn_obj->inout_data = NULL;
    conn_obj->tid = -1;
    return(conn_obj);
}

int32_t
nkn_ccp_server_open_tcp_conn(int *sock)
{
    int32_t len;
    int val = 1;
    struct sockaddr_un remote;
    struct epoll_event ev;
    ccp_server_session_ctxt_t *conn_obj;

    if ((*sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
	DBG_LOG(ERROR, MOD_CCP,
		"Failed to create socket: %s", strerror(errno));
        goto handle_err;
    }

    nkn_ccp_nonblock(*sock);
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, SOCK_PATH);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    unlink(remote.sun_path);

    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (bind(*sock, (struct sockaddr *)&remote, len) < 0) {
	DBG_LOG(ERROR, MOD_CCP, "Bind failed: %s", strerror(errno));
        goto handle_err;
    }

    if (listen(*sock, MAX_CONNECTIONS) < 0) {
	DBG_LOG(ERROR, MOD_CCP, "Listen failed: %s", strerror(errno));
        goto handle_err;
    }

    conn_obj = get_new_server_session_ctxt();
    if (conn_obj == NULL) {
	DBG_LOG(MSG, MOD_CCP, "SEVERE Calloc failed\n");
	goto handle_err;
    }

    conn_obj->inout_data = (ccp_inout_buf_t *)nkn_calloc_type(1,
						sizeof(ccp_inout_buf_t),
						mod_ccp_inoutdata);
    conn_obj->inout_data->sock = g_ccp_server_sock;
    conn_obj->dest_callback_func = g_ccp_server_dest_callback;

    ev.events = EPOLL_IN_EVENTS;
    ev.data.ptr = conn_obj->inout_data;

    if (epoll_ctl(g_ccp_server_epoll_node->epoll_fd, EPOLL_CTL_ADD,
				*sock, &ev) < 0) {
        if(errno == EEXIST ) {
	    DBG_LOG(ERROR, MOD_CCP, "Adding listen fd: %s", strerror(errno));
            goto handle_err;
        } else {
	    DBG_LOG(SEVERE, MOD_CCP, "Adding listen fd: %s", strerror(errno));
            goto handle_err;
        }
    }
    return 0;
handle_err:
    glob_ccp_server_conn_fail_cnt++;
    return -1;
}


ccp_server_session_ctxt_t *
find_server_ctxt(int32_t tid)
{
    ccp_server_session_ctxt_t *conn_obj;
    pthread_mutex_lock(&server_ctxt_list_mutex);
    conn_obj = TAILQ_FIRST(&g_ccp_server_ctxt_list);

    while (conn_obj != NULL) {
	if (conn_obj->tid == tid) {
	    pthread_mutex_unlock(&server_ctxt_list_mutex);
	    return(conn_obj);
	}
	conn_obj = TAILQ_NEXT(conn_obj, entries);
    }
    pthread_mutex_unlock(&server_ctxt_list_mutex);

    return (NULL);
}

int32_t
nkn_ccp_server_handle_response(ccp_client_data_t *client_ctxt)
{
    ccp_server_session_ctxt_t *conn_obj;
    conn_obj = (ccp_server_session_ctxt_t *) client_ctxt;
    ccp_xml_t *server_response;
    server_response = (ccp_xml_t *)nkn_calloc_type(1, sizeof(ccp_xml_t),
						    mod_ccp_xml);
    conn_obj->inout_data->server_response = server_response;
    int err;

    switch (conn_obj->client_ctxt.command) {
	case CCP_POST:
	    /*
	     * Form POSTACK message
	     * insert wait_period tag.
	     */
	    err = nkn_ccp_server_form_postack_message(server_response,
							conn_obj);
	    if (err == -1)
		goto handle_err;
	    conn_obj->end_session = 0;
	    DBG_LOG(MSG, MOD_CCP, "[%s] Sending POSTACK MESSAGE", 
			(conn_obj->client_ctxt.crawl_name?
			 conn_obj->client_ctxt.crawl_name:
			 "NULL"));
	    break;
	case CCP_QUERY:
	    /*
	     * Form QUERYACK Message
	     * Insert Status Tag.
	     * if status == NOT_OK
	     * send NOT_OK uris. 
	     */
	    err = nkn_ccp_server_form_queryack_message(server_response,
							conn_obj);
	    if (err == -1)
		goto handle_err;
	    if(conn_obj->client_ctxt.query_status == CCP_OK ||
		    conn_obj->client_ctxt.query_status == CCP_NOT_OK ||
		    conn_obj->client_ctxt.query_status == CCP_INVALID)
		conn_obj->end_session = 1;
	    DBG_LOG(MSG, MOD_CCP, "[%s] Sending QUERYACK MESSAGE", 
			(conn_obj->client_ctxt.crawl_name?
			 conn_obj->client_ctxt.crawl_name:
			 "NULL"));
	     break;
	case CCP_ABRT:
	    /*
	     * Form ABRTACK Message
	     * send.
	     */
	    err = nkn_ccp_server_form_abrtack_message(server_response,
							conn_obj);
	    if (err == -1)
		goto handle_err;
	    conn_obj->end_session = 1;
	    DBG_LOG(MSG, MOD_CCP, "[%s] Sending ABRTACK MESSAGE", 
			(conn_obj->client_ctxt.crawl_name?
			 conn_obj->client_ctxt.crawl_name:
			 "NULL"));
	    break;
	default:
	    DBG_LOG(MSG, MOD_CCP, "Invalid Message\n");
	    break;
    }

    err = nkn_ccp_server_change_epoll_events(EPOLL_OUT_EVENTS,
			    conn_obj->inout_data->sock, conn_obj->inout_data,
			    conn_obj->client_ctxt.crawl_name);
    if (conn_obj->end_session) {
	nkn_ccp_server_ctxt_cleanup(conn_obj);
    }

    if (err == -1)
	goto handle_err;

    return 0;
handle_err:
    return(-1);
}


int32_t
nkn_ccp_server_form_postack_message(ccp_xml_t *server_response,
				    ccp_server_session_ctxt_t *conn_obj)
{
    char buf[256];
    int err;

    err = nkn_ccp_start_xml_payload(server_response, CCP_POSTACK, conn_obj->pid,
				    conn_obj->tid);
    if (err == -1)
	goto handle_err;

    if (conn_obj->client_ctxt.wait_time >
	    MAX_CCP_SERVER_WAIT_TIME) {
	sprintf(buf, "%d", MAX_CCP_SERVER_WAIT_TIME);
    } else {
	sprintf(buf, "%d", conn_obj->client_ctxt.wait_time);
    }
    err = nkn_ccp_server_insert_wait_period(server_response, buf);
    if (err == -1)
	goto handle_err;
    err = nkn_ccp_end_xml_payload(server_response);
    if (err == -1)
	goto handle_err;

    return 0;

handle_err:
    return(-1);
}


int32_t
nkn_ccp_server_insert_wait_period(ccp_xml_t *server_response, char *wait_time)
{
    xmlNewChild(server_response->rootNode, NULL, BAD_CAST "wait_period",
		BAD_CAST wait_time);
    return 0;
}

static int32_t
nkn_ccp_server_insert_url_status (ccp_xml_t *server_response, char *url,
					query_status_t status,
					int errcode)
{
    char buf[256];
    xmlNodePtr uriNode;
    xmlChar *uriContent;
    xmlBufferPtr bufptr;

    if(server_response->cmdNode) {
	bufptr = xmlBufferCreateSize(MAX_URI_SIZE);
	uriContent = (xmlChar *)xmlBufferContent(bufptr);
	sprintf((char *)uriContent, "%s", url);
	uriNode = xmlNewChild(server_response->cmdNode, NULL, BAD_CAST "uri",
				uriContent);
	xmlNewProp(uriNode, BAD_CAST "status", BAD_CAST query_message[status]);
	if(errcode) {
	    sprintf(buf, "%d", errcode);
	    xmlNewProp(uriNode, BAD_CAST "errcode", BAD_CAST buf);
	}
	xmlFree(uriContent);
	xmlFree(bufptr);
    }
    return 0;
}

static int32_t
nkn_ccp_server_insert_command(ccp_xml_t *server_response,
				    ccp_action_type_t action)
{
    server_response->cmdNode =
	xmlNewChild(server_response->rootNode, NULL, BAD_CAST "action", NULL);
    xmlNewProp(server_response->cmdNode, BAD_CAST "type",
		BAD_CAST action_message[action]);
    return 0;
}

int32_t
nkn_ccp_server_form_queryack_message(ccp_xml_t *server_response,
					ccp_server_session_ctxt_t *conn_obj)
{
    int err;
    ccp_urlnode_t *url_entry;

    err = nkn_ccp_start_xml_payload(server_response, CCP_QUERYACK,
				    conn_obj->pid, conn_obj->tid);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_server_insert_query_status(server_response,
					    conn_obj->client_ctxt.query_status);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_server_insert_command(server_response,
					conn_obj->client_ctxt.action);
    if (err == -1)
	goto handle_err;

    url_entry = conn_obj->client_ctxt.url_entry;

    while(url_entry) {

	err = nkn_ccp_server_insert_url_status (server_response, url_entry->url,
						url_entry->status,
						url_entry->errcode);
	if (err == -1)
	    goto handle_err;

	url_entry = url_entry->next;
    }

    err = nkn_ccp_end_xml_payload(server_response);
    if (err == -1)
	goto handle_err;

    return 0;

handle_err:
    return(-1);
}

int32_t
nkn_ccp_server_form_abrtack_message(ccp_xml_t *server_response,
				    ccp_server_session_ctxt_t *conn_obj)
{
    int err;

    err = nkn_ccp_start_xml_payload(server_response, CCP_ABRTACK, conn_obj->pid,
				    conn_obj->tid);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_end_xml_payload(server_response);
    if (err == -1)
	goto handle_err;

    return 0;

handle_err:
    return(-1);
}

int32_t
nkn_ccp_server_retry_start_session()
{
    int err,num_retries = 5;

    while(num_retries) {
	err = nkn_ccp_server_open_tcp_conn(&g_ccp_server_sock);
	if (err == -1) {
	    num_retries--;
	} else {
	    break;
	}
    }
    if (num_retries <= 0) {
	DBG_LOG(SEVERE, MOD_CCP, "Server connection setup failed");
	return (-1);
    }
    return 0;
}

