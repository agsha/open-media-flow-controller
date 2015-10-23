/*
 * (C) Copyright 2010 Juniper Networks, Inc
 *
 * This file contains implementation of the client related functions of
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
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#define MAX_COMMAND_LEN 1024
#define MAX_URL_SIZE 4096
#define MAX_CONNECTIONS 25
#include "nkn_ccp_client.h"
#include "nkn_debug.h"

TAILQ_HEAD (waitq_head, ccp_client_session_ctxt) g_ccp_waitq_head;

AO_t g_ccp_post_id;
struct ccp_epoll_struct *g_ccp_epoll_node;
pthread_mutex_t g_ccp_client_waitq_mutex = PTHREAD_MUTEX_INITIALIZER;
void nkn_ccp_client_cleanup_url_buffers(ccp_client_session_ctxt_t *conn_obj);

/*
 * This function is called from the client  

 */
ccp_client_session_ctxt_t *
nkn_ccp_client_start_new_session (int32_t (*status_func)(void *ptr),
				    void (*cleanup_func) (void *ptr),
				    void *ptr, char *crawl_name)
{
    ccp_client_session_ctxt_t *conn_obj;
    int8_t err;

    conn_obj = (ccp_client_session_ctxt_t *) nkn_calloc_type (1,
					    sizeof (ccp_client_session_ctxt_t),
					    mod_ccp_client_ctxt);

    if (conn_obj == NULL) {
	DBG_LOG (SEVERE, MOD_CCP, "Calloc failed");
	goto handle_err;
    }
    conn_obj->crawl_name = crawl_name;
    /*
    * Open tcp connection to CDE
    */
    err = nkn_ccp_client_open_conn(&conn_obj->sock);
    if (err == -1) {
	goto handle_err;
    }

    if (conn_obj->sock) {
	DBG_LOG (MSG, MOD_CCP, "[%s] New ccp session started",
		conn_obj->crawl_name);
	conn_obj->dest_task_id = -1;
	AO_fetch_and_add (&g_ccp_post_id, 1);
	conn_obj->ptr = ptr;
	conn_obj->src_post_id = g_ccp_post_id;
	conn_obj->dest_task_id = -1;
	conn_obj->status = SESSION_INITIATED;
	conn_obj->status_callback_func = status_func;
	conn_obj->cleanup_callback_func = cleanup_func;
	return (conn_obj);
    }

handle_err:
    return (NULL);
}

void
nkn_ccp_client_init(char *source_name, char *dest_name, int num_crawlers)
{
    int s;
    int stack_size = 256*1024;
    pthread_t epoll_thrdid;
    pthread_t wait_thrdid;
    pthread_attr_t attr;
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
    ccp_source_name = nkn_strdup_type(source_name, mod_ccp_module);
    ccp_dest_name = nkn_strdup_type(dest_name, mod_ccp_module);
    TAILQ_INIT(&g_ccp_waitq_head);
    xmlInitParser();
    g_ccp_epoll_node = nkn_ccp_create(num_crawlers);
    pthread_create(&epoll_thrdid,
		     &attr, (void *)nkn_ccp_client_process_epoll_events, NULL);
    pthread_create(&wait_thrdid,
		     &attr, (void *)nkn_ccp_client_wait_routine, NULL);
}

int32_t
nkn_ccp_client_sm (ccp_client_session_ctxt_t *conn_obj)
{
    int err;

    while (1) {
	switch (conn_obj->status) {
	    case SESSION_INITIATED:
	        /*
	         * do nothing
	         */
	        return 0;

	    case POST_TASK:
		err = nkn_ccp_client_post_task (conn_obj);
		if (err == -1) {
		    break;
		} else {
		    return 0;
		}

	    case RECV_POST_ACK:
		err = nkn_ccp_client_recv_post_ack (conn_obj);
		if (err == -1) {
		    break;
		} else {
		    return 0;
		}

	    case SEND_QUERY:
		err = nkn_ccp_client_send_query (conn_obj);
		if (err == -1) {
		    break;
		} else {
		    return 0;
		}

	    case RECV_QUERY_RESPONSE:
		err = nkn_ccp_client_recv_query_resp (conn_obj);
		if (err == -1 ||
			conn_obj->status == TASK_COMPLETED) {
		    break;
		} else {
		    return 0;
		}

	    case ABORT_TASK:
		err = nkn_ccp_client_abort_task (conn_obj);
		if (err == -1) {
		    break;
		} else {
		    return 0;
		}

	    case RECV_ABRT_ACK:
		err = nkn_ccp_client_recv_abrt_ack (conn_obj);
		if (err == -1) {
		    break;
		} else {
		    return 0;
		}

	    case TASK_COMPLETED:
		err = nkn_ccp_client_task_completed (conn_obj);
		if (err == -1) {
		    break;
		} else {
		    return 0;
		}
	    case ERROR_CCP:
		nkn_ccp_client_remove_from_waitq (conn_obj);
	    case ERROR_CCP_POST:
		if (conn_obj->recv_buf) {
		    free(conn_obj->recv_buf);
		}
		assert(conn_obj->in_waitq == 0);
		conn_obj->cleanup_callback_func (conn_obj->ptr);
	        return (-1);
	}
    }
}

int32_t
nkn_ccp_client_open_conn (int *sock)
{
    int32_t len;
    struct sockaddr_un remote;

    if ((*sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
	DBG_LOG(ERROR, MOD_CCP, "Socket creation failed: %s", strerror(errno));
	glob_ccp_client_socket_creation_fails++;
	return (-1);
    }

    nkn_ccp_nonblock (*sock);
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, SOCK_PATH);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    if (connect(*sock, (struct sockaddr *)&remote, len) == -1) {
	DBG_LOG(ERROR, MOD_CCP, "Connect failed: %s", strerror(errno));
	return (-1);
    }
    return 0;
}

int32_t
nkn_ccp_client_parse_message (ccp_client_session_ctxt_t *conn_obj,
			       char *search_cmd)
{
    xmlChar *ptr;
    xmlChar *command;
    url_t *temp_ptr;
    xmlDocPtr doc;
    xmlNodePtr childNode;
    xmlNodePtr uriNode;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
    xmlAttr *attr;
    ccp_action_type_t payload_type;
    int32_t tid;

    doc = xmlParseMemory (conn_obj->recv_buf, strlen (conn_obj->recv_buf));
    xpathCtx = xmlXPathNewContext(doc);
    xpathObj = xmlXPathEvalExpression((xmlChar *)"/ccp/command", xpathCtx);

    if(xpathObj && xpathObj->nodesetval &&
	    xpathObj->nodesetval->nodeTab &&
	    *xpathObj->nodesetval->nodeTab) {
	ptr = (*xpathObj->nodesetval->nodeTab)->children->content;

	command = ptr;

	if (strcasestr ((char *)command, search_cmd)) {

	    if (strcasestr ((char *)command, "POSTACK")) {
		/*
		 * get tid and wait time
		 */
		tid = nkn_ccp_get_pid_tid (command);
		conn_obj->dest_task_id = tid;
		if (tid == -1){
		    glob_ccp_client_invalid_tid_cnt++;
		    DBG_LOG(ERROR, MOD_CCP, "Rxd Invalid tid");
		    goto handle_err;
		}

		if (xpathObj)
		    xmlXPathFreeObject(xpathObj);
		xpathObj = xmlXPathEvalExpression(
			    (xmlChar *)"/ccp/wait_period", xpathCtx);
		
		if(xpathObj && xpathObj->nodesetval &&
		    xpathObj->nodesetval->nodeTab &&
			*xpathObj->nodesetval->nodeTab) {
		    childNode = (*xpathObj->nodesetval->nodeTab);
		    if (childNode)
			conn_obj->wait_time = atol(
			    (char *)childNode->children->content);
		}

	    } else if (strcasestr ((char *)command, "QUERYACK")) {
		/*
		 * get tid, status, urls;
		 */
		tid = nkn_ccp_get_pid_tid (command);
		if (tid == -1){
		    glob_ccp_client_invalid_tid_cnt++;
		    DBG_LOG(ERROR, MOD_CCP, "Rxd Invalid tid");
		    goto handle_err;
		}

		if (xpathObj)
		    xmlXPathFreeObject(xpathObj);
		xpathObj = xmlXPathEvalExpression(
			    (xmlChar *)"/ccp/status", xpathCtx);
		if(xpathObj && xpathObj->nodesetval &&
		    xpathObj->nodesetval->nodeTab &&
			*xpathObj->nodesetval->nodeTab) {
		    childNode = (*xpathObj->nodesetval->nodeTab);
		    if (childNode) {
			char *status = (char *)childNode->children->content;
			if (!strcasecmp (status, "OK")) {
			    conn_obj->query_status = CCP_OK;
			} else if (!strcasecmp (status, "NOT_STARTED")) {
			    conn_obj->query_status = CCP_NOT_STARTED;
			} else if (!strcasecmp (status, "INVALID")) {
			    conn_obj->query_status = CCP_INVALID;
			} else if (!strcasecmp (status, "IN_PROGRESS")) {
			    conn_obj->query_status = CCP_IN_PROGRESS;
			} else if (!strcasecmp (status, "NOT_OK")) {
			    conn_obj->query_status = CCP_NOT_OK;
			}
		    }
		}

		if (xpathObj)
		    xmlXPathFreeObject(xpathObj);
		xpathObj = xmlXPathEvalExpression((xmlChar *)"/ccp/action",
                                                xpathCtx);
 
                if (xpathObj && xpathObj->nodesetval &&
                    xpathObj->nodesetval->nodeTab &&
                        *xpathObj->nodesetval->nodeTab) {
		    childNode = (*xpathObj->nodesetval->nodeTab);
		      attr = childNode->properties;
                    while (attr) {
                        if (strcasestr((char *)attr->name, "type")) {
                            if(attr->children) {
                                if (strcasestr((char *)attr->children->content,
                                    "add")) {
                                   payload_type = CCP_ACTION_ADD;
                                } else if (
                                    strcasestr((char *)attr->children->content,
                                        "delete")) {
                                   payload_type = CCP_ACTION_DELETE;
                                }
                            }
                        }
                       attr = attr->next;
                    }
                }
                if (xpathObj)
                    xmlXPathFreeObject(xpathObj);
                xpathObj = xmlXPathEvalExpression((xmlChar *)"/ccp/action/uri",
                                                xpathCtx);
 
                if (xpathObj && xpathObj->nodesetval &&
                    xpathObj->nodesetval->nodeTab &&
                        *xpathObj->nodesetval->nodeTab) {
                    uriNode = (*xpathObj->nodesetval->nodeTab);
                    while (uriNode) {
                        if (!conn_obj->urlnode) {
                            conn_obj->urlnode = (url_t *) nkn_calloc_type (1,
							    sizeof (url_t),
							    mod_ccp_url_st);
                            if (!conn_obj->urlnode) {
				DBG_LOG (SEVERE, MOD_CCP, "calloc failed\n");
			    } else {
				temp_ptr = conn_obj->urlnode;
				temp_ptr->next = NULL;
     			    }
                        } else {
                            temp_ptr->next = (url_t *) nkn_calloc_type (1,
								sizeof (url_t),
								mod_ccp_url_st);
                            if (!temp_ptr->next) {
				DBG_LOG (SEVERE, MOD_CCP, "calloc failed\n");
    			    }
                            temp_ptr = temp_ptr->next;
                            temp_ptr->next = NULL;
                        }
                        if(uriNode->children) {
                           temp_ptr->url = nkn_strdup_type(
					    (char *)uriNode->children->content,
							    mod_ccp_url);
                           temp_ptr->action_type = payload_type;
                        }
                        attr = uriNode->properties;
                        while (attr) {
                            if (strcasestr((char *)attr->name, "status")) {
                                if(attr->children) {
                                    if (strcasestr(
					    (char *)attr->children->content,
                                            "NOT_OK")) {
                                        temp_ptr->status = -1;
                                    } else {
                                        temp_ptr->status = 1;
                                    }
                                }

			    } else if (strcasestr(
					(char *)attr->name, "errcode")) {
				if(attr->children) {
				    temp_ptr->errcode =
					atoi((char *)attr->children->content);
				}
			    }
			    attr = attr->next;
			}
			uriNode = uriNode->next;
		    }
		}
	    } else if (strcasestr ((char *)command, "ABRTACK")) {
		/*
		 * get tid;
		 */
		tid = nkn_ccp_get_pid_tid (command);
		if (tid == -1){
		    glob_ccp_client_invalid_tid_cnt++;
		    DBG_LOG(ERROR, MOD_CCP, "Rxd Invalid tid");
		    goto handle_err;
		}
	    }

	} else {
	    DBG_LOG (MSG, MOD_CCP,
		    "[%s] Invalid message received searchcmd: %s "
		    "recv command: %s\n", conn_obj->crawl_name,
		    search_cmd, command);
	    glob_ccp_client_invalid_searchcmd_cnt++;
	    goto handle_err;
	}
        if (xpathCtx)
	    xmlXPathFreeContext(xpathCtx);
	if (xpathObj)
	    xmlXPathFreeObject(xpathObj);
	if (doc)
	    xmlFreeDoc(doc);
	return 0;
    }

handle_err:
    if (xpathCtx)
        xmlXPathFreeContext(xpathCtx);
    if (xpathObj)
	xmlXPathFreeObject(xpathObj);
    if (doc)
        xmlFreeDoc(doc);
    return (-1);
}


void
nkn_ccp_client_cleanup_send_buffers (ccp_xml_t *xmlnode)
{
    if (xmlnode->xmlbuf) {
	xmlFree(xmlnode->xmlbuf);
	xmlnode->xmlbuf = NULL;
    }

    if (xmlnode->doc) {
	xmlFreeDoc (xmlnode->doc);
	xmlnode->doc = NULL;
    }
}

int32_t
nkn_ccp_client_add_to_waitq (ccp_client_session_ctxt_t *conn_obj)
{
    pthread_mutex_lock (&g_ccp_client_waitq_mutex);
    if (!conn_obj->in_waitq) {
	TAILQ_INSERT_TAIL (&g_ccp_waitq_head, conn_obj, waitq_entries);
	DBG_LOG (MSG, MOD_CCP, "[%s] Adding context to waitq",
	    conn_obj->crawl_name);
	conn_obj->in_waitq = 1;
    } else {
	DBG_LOG (ERROR, MOD_CCP, "[%s] Crawler context already in waitq",
	    conn_obj->crawl_name);
    }
    pthread_mutex_unlock (&g_ccp_client_waitq_mutex);
    return 0;
}


int32_t
nkn_ccp_client_insert_uri_count (ccp_xml_t *xmlnode, int32_t num_urls)
{
    xmlChar *cmdContent;
    xmlBufferPtr bufptr;
    bufptr = xmlBufferCreateSize(10);
    cmdContent = (xmlChar *)xmlBufferContent(bufptr);
    sprintf((char *)cmdContent, " %d", num_urls);
    xmlNewProp(xmlnode->cmdNode, BAD_CAST "count", BAD_CAST cmdContent);
    xmlFree(cmdContent);
    xmlFree(bufptr);
    return 0;
}

int32_t
nkn_ccp_client_form_xml_message (ccp_client_session_ctxt_t *conn_obj,
				 ccp_command_types_t command)
{
    int err;

    err = nkn_ccp_start_xml_payload (&conn_obj->xmlnode, command,
			    conn_obj->src_post_id, conn_obj->dest_task_id);

    if (err == -1) {
	glob_ccp_client_start_payload_err_cnt++;
	goto handle_err;
    }

    err = nkn_ccp_end_xml_payload (&conn_obj->xmlnode);
    if (err == -1) {
	glob_ccp_client_end_payload_err_cnt++;
	goto handle_err;
    }
    return 0;

handle_err:
    return (-1);
}

int32_t
nkn_ccp_client_insert_url_entry (ccp_xml_t *xmlnode, char *url,
				 int64_t expiry, int8_t cachepin,
				 int generate_asx_flag, char *etag,
				 int64_t objsize, char *domain)
{
    char buf[256];
    xmlNodePtr uriNode;
    xmlChar *uriContent;
    xmlChar *temp;
    xmlBufferPtr bufptr;
    bufptr = xmlBufferCreateSize(MAX_URI_SIZE);
    uriContent = (xmlChar *)xmlBufferContent(bufptr);
    snprintf((char *)uriContent, MAX_URI_SIZE, "%s", url);
    uriNode = xmlNewChild(xmlnode->cmdNode, NULL, BAD_CAST "uri", NULL);
    temp = xmlEncodeSpecialChars(xmlnode->doc, BAD_CAST uriContent);
    xmlNodeSetContent(uriNode, temp);
    sprintf(buf, "%lu", expiry);
    xmlNewProp(uriNode, BAD_CAST "Expiry", BAD_CAST buf);
    sprintf(buf, "%d", cachepin);
    xmlNewProp(uriNode, BAD_CAST "CachePin", BAD_CAST buf);
    sprintf(buf, "%d", generate_asx_flag);
    xmlNewProp(uriNode, BAD_CAST "generate_asx", BAD_CAST buf);
    xmlNewProp(uriNode, BAD_CAST "etag", BAD_CAST etag);
    xmlNewProp(uriNode, BAD_CAST "domain", BAD_CAST domain);
    sprintf(buf, "%ld", objsize);
    xmlNewProp(uriNode, BAD_CAST "content_size", BAD_CAST buf);
    xmlFree(uriContent);
    xmlFree(bufptr);
    xmlFree(temp);
    return 0;
}

int32_t
nkn_ccp_client_start_new_payload (ccp_client_session_ctxt_t *conn_obj,
				    change_list_type_t payload_file_type)
{
    switch (payload_file_type) {
	case ADD_ASX:
	case ADD:
	    nkn_ccp_start_xml_payload
		(&conn_obj->xmlnode, CCP_POST,
		    conn_obj->src_post_id, conn_obj->dest_task_id);
	    conn_obj->xmlnode.cmdNode =
		xmlNewChild(conn_obj->xmlnode.rootNode,
			NULL, BAD_CAST "action", NULL);
	    xmlNewProp(conn_obj->xmlnode.cmdNode,
		    BAD_CAST "type", BAD_CAST "add");
	    xmlNewProp(conn_obj->xmlnode.cmdNode,
		    BAD_CAST "instance", BAD_CAST conn_obj->crawl_name);
	    break;

	case DELETE_ASX:
	case DELETE:
	    nkn_ccp_start_xml_payload
		(&conn_obj->xmlnode, CCP_POST,
		 conn_obj->src_post_id, conn_obj->dest_task_id);
	    conn_obj->xmlnode.cmdNode =
		xmlNewChild(conn_obj->xmlnode.rootNode,
			NULL, BAD_CAST "action", NULL);
	    xmlNewProp(conn_obj->xmlnode.cmdNode,
		    BAD_CAST "type", BAD_CAST "delete");
	    xmlNewProp(conn_obj->xmlnode.cmdNode,
		    BAD_CAST "instance", BAD_CAST conn_obj->crawl_name);

	default:
	    break;
    }
    return 0;
}


int32_t
nkn_ccp_client_insert_expiry (ccp_xml_t *xmlnode, int64_t expiry)
{
    char buf[256];
    sprintf (buf, "%ld", expiry);
    xmlNewProp(xmlnode->cmdNode, BAD_CAST "Expiry", BAD_CAST buf);
    return 0;
}

int32_t
nkn_ccp_client_add_client_domain(ccp_xml_t *xmlnode, char *client_domain)
{
    xmlNewProp(xmlnode->cmdNode,
	    BAD_CAST "Client_domain", BAD_CAST client_domain);
    return 0;
}

int32_t
nkn_ccp_client_set_cachepin (ccp_xml_t *xmlnode)
{
    xmlNewProp(xmlnode->cmdNode, BAD_CAST "CachePin", BAD_CAST "1");
    return 0;
}

int32_t
nkn_ccp_client_set_genasx_flag (ccp_xml_t *xmlnode)
{
    xmlNewProp(xmlnode->cmdNode, BAD_CAST "generate_asx", BAD_CAST "1");
    return 0;
}

int
nkn_ccp_client_add_epoll_event(int event_flags, ccp_client_session_ctxt_t *obj)
{
    struct epoll_event ev;
    ev.events = event_flags;
    ev.data.ptr = obj;
    int err;
    if (epoll_ctl
	    (g_ccp_epoll_node->epoll_fd, EPOLL_CTL_ADD, obj->sock, &ev) < 0) {
        if (errno == EEXIST ) {	/* errno 17 */
	    DBG_LOG(ERROR, MOD_CCP, "[%s] Adding fd: %s",
		    obj->crawl_name, strerror(errno));
	    err = nkn_ccp_client_change_epoll_event (event_flags, obj);
	    return err;
        } else {
	    DBG_LOG(SEVERE, MOD_CCP, "[%s] Adding fd: %s",
		    obj->crawl_name, strerror(errno));
	    glob_ccp_client_add_epoll_event_err++;
	    goto handle_err;
        }
    }
    return 0;

handle_err:
    return -1;
}

int
nkn_ccp_client_change_epoll_event (int event_flags,
				   ccp_client_session_ctxt_t *conn_obj)
{
    struct epoll_event ev;
    ev.events = event_flags;
    ev.data.ptr = conn_obj;
    if (epoll_ctl(g_ccp_epoll_node->epoll_fd,
		    EPOLL_CTL_MOD, conn_obj->sock, &ev) < 0) {
        if (errno == EEXIST ) {
	    DBG_LOG(ERROR, MOD_CCP, "[%s] Changing event: %s",
		    conn_obj->crawl_name, strerror(errno));
	    return 0;
        } else {
	    DBG_LOG(SEVERE, MOD_CCP, "[%s] Changing event: %s",
		    conn_obj->crawl_name, strerror(errno));
	    glob_ccp_client_change_epoll_event_err++;
	    goto handle_err;
        }
    }
    return 0;

handle_err:
    return -1;
}

int
nkn_ccp_client_del_epoll_event (ccp_client_session_ctxt_t *conn_obj)
{
    return(epoll_ctl(g_ccp_epoll_node->epoll_fd,
			EPOLL_CTL_DEL, conn_obj->sock, NULL));
}

void
nkn_ccp_client_process_epoll_events (void *data __attribute((unused)))
{
    int32_t nfds, n;
    ccp_client_session_ctxt_t *conn_obj;

    while (1) {
        nfds = epoll_wait(g_ccp_epoll_node->epoll_fd,
			    g_ccp_epoll_node->events, MAX_CONNECTIONS, 1);

	if (nfds < 0) {
	    if (errno == EINTR)
		nfds = 0;
	}

	if (nfds == 0) {
	    //DBG_LOG (MSG, MOD_CCP, "NO ACTIVE FDS\n"); fflush (stdout);
	}

	for (n = 0; n < nfds; n++) {
	    conn_obj = (ccp_client_session_ctxt_t *)
			    g_ccp_epoll_node->events[n].data.ptr;
	    nkn_ccp_client_sm(conn_obj);
	}
    }
}

/*
 * take entries from waitq, check if wait period is crossed.
 * if so, change the state to SEND_QUERY and remove the object from the queue.
 */
void
nkn_ccp_client_wait_routine ()
{

    ccp_client_session_ctxt_t *conn_obj;

    while (1) {
	sleep (1);
	pthread_mutex_lock (&g_ccp_client_waitq_mutex);
	TAILQ_FOREACH (conn_obj, &g_ccp_waitq_head, waitq_entries) {
	    conn_obj->wait_time--;
	    if (conn_obj->wait_time <= 0) {
		switch (conn_obj->status) {
		    case SEND_QUERY:
			DBG_LOG(MSG, MOD_CCP, "[%s] Wait Time Reached,"
				    "SENDING QUERY",
				    conn_obj->crawl_name);
			nkn_ccp_client_change_epoll_event
				(EPOLL_OUT_EVENTS, conn_obj);
			break;
		}
		conn_obj->wait_time = DEFAULT_WAITING_TIME;
	    }
	}
	pthread_mutex_unlock (&g_ccp_client_waitq_mutex);
    }
}

void
nkn_ccp_client_remove_from_waitq(ccp_client_session_ctxt_t *conn_obj)
{
    pthread_mutex_lock (&g_ccp_client_waitq_mutex);
    if (conn_obj->in_waitq) {
	TAILQ_REMOVE (&g_ccp_waitq_head, conn_obj, waitq_entries);
	conn_obj->in_waitq = 0;
    }
    pthread_mutex_unlock (&g_ccp_client_waitq_mutex);
}


int32_t
nkn_ccp_client_recv_post_ack (ccp_client_session_ctxt_t *conn_obj)
{
    int err;
    int32_t bytes_read;
    DBG_LOG (MSG, MOD_CCP, "[%s] Current state: RECV_POST_ACK",
	    conn_obj->crawl_name);
    if (conn_obj->recv_buf == NULL) {
        conn_obj->recv_buf = (char *) nkn_calloc_type(4096, 1,
					    mod_ccp_recvbuf);
	conn_obj->offset = 0;
    } else {
        conn_obj->recv_buf = (char *) realloc(conn_obj->recv_buf,
					    conn_obj->offset + 4096);
    }

    if (conn_obj->recv_buf == NULL) {
	DBG_LOG (SEVERE, MOD_CCP, "Memory allocation failed");
	goto handle_err;
    }

    bytes_read = nkn_ccp_recv_message(conn_obj->sock,
				conn_obj->recv_buf + conn_obj->offset);
    if (bytes_read == 0) {
	DBG_LOG(MSG, MOD_CCP, "[%s] Server closed connection",
		conn_obj->crawl_name);
         conn_obj->status = ERROR_CCP;
         free (conn_obj->recv_buf);
         conn_obj->recv_buf = NULL;
	 glob_ccp_client_conn_reset_cnt++;
         goto handle_err;
     }

    if (bytes_read == -1) {
	glob_ccp_client_num_postack_fail_cnt++;
	if (conn_obj->retries < MAX_RETRIES) {
	    DBG_LOG (ERROR, MOD_CCP,
		    "[%s] ACK for POST didnt come, resending the data",
		    conn_obj->crawl_name);
	    nkn_ccp_client_change_epoll_event (EPOLL_OUT_EVENTS, conn_obj);
	    conn_obj->status = POST_TASK;
	    conn_obj->retries ++;
	    free (conn_obj->recv_buf);
	    conn_obj->recv_buf = NULL;
	} else {
	    DBG_LOG (ERROR, MOD_CCP,
		    "[%s] ACK for POST didnt come even after MAX_RETRIES," 
		    "Aborting the session\n",
		    conn_obj->crawl_name);
	    nkn_ccp_client_change_epoll_event (EPOLL_OUT_EVENTS, conn_obj);
	    conn_obj->status = ABORT_TASK;
	    free (conn_obj->recv_buf);
	    conn_obj->recv_buf = NULL;
	    conn_obj->offset = 0;
	}
	goto handle_err;
    } else {
	conn_obj->offset += bytes_read;
       /*
       * This function does xml parsing and fills the dest_task_id
       * and wait time details.
       */
        conn_obj->recv_buf[conn_obj->offset] = '\0';
        err = nkn_ccp_check_if_proper_xml(
			conn_obj->recv_buf, conn_obj->offset);
        if (err == 0) {
	    DBG_LOG (MSG, MOD_CCP,
		    "[%s] Not a proper xml file, recv more data\n",
		    conn_obj->crawl_name);
	    fflush (stdout);
	    nkn_ccp_client_change_epoll_event (EPOLL_IN_EVENTS, conn_obj);
        } else {
	    err = nkn_ccp_client_parse_message (conn_obj, (char *)"POSTACK");
	    if (err == -1) {
	         conn_obj->status = ERROR_CCP;
	         free (conn_obj->recv_buf);
	         conn_obj->recv_buf = NULL;
		 goto handle_err;
	     }
	     //conn_obj->status = IN_WAITQ;
	     conn_obj->status = SEND_QUERY;
	     nkn_ccp_client_change_epoll_event (EPOLL_IN_EVENTS, conn_obj);
	     free (conn_obj->recv_buf);
	     conn_obj->recv_buf = NULL;
	     conn_obj->offset = 0;
        }
    }
    return 0;

handle_err:
    return (-1);
}

int32_t
nkn_ccp_client_send_query (ccp_client_session_ctxt_t *conn_obj)
{
    DBG_LOG (MSG, MOD_CCP, "[%s] Current state: SEND_QUERY\n",
	    conn_obj->crawl_name); 
    nkn_ccp_client_cleanup_send_buffers (&conn_obj->xmlnode);
    nkn_ccp_client_form_xml_message (conn_obj, CCP_QUERY);
    nkn_ccp_send_message (conn_obj->sock, (char *)conn_obj->xmlnode.xmlbuf);
    nkn_ccp_client_cleanup_send_buffers (&conn_obj->xmlnode);
    conn_obj->wait_time = DEFAULT_WAITING_TIME;
    conn_obj->status = RECV_QUERY_RESPONSE;
    nkn_ccp_client_change_epoll_event (EPOLL_IN_EVENTS, conn_obj);
    //updating timer for recv_query_resp.
    return 0;
}

int32_t
nkn_ccp_client_recv_query_resp (ccp_client_session_ctxt_t *conn_obj)
{
    int32_t err;
    int32_t bytes_read = 0;

    DBG_LOG (MSG, MOD_CCP, "[%s] Current state: RECV_QUERY_RESPONSE\n",
	    conn_obj->crawl_name); 
    if (conn_obj->recv_buf == NULL) {
        conn_obj->recv_buf = (char *)nkn_calloc_type(4096, 1, mod_ccp_recvbuf);
	conn_obj->offset = 0;
    } else {
        conn_obj->recv_buf = (char *)realloc(
			     conn_obj->recv_buf, conn_obj->offset + 4096);
    }

    if (conn_obj->recv_buf == NULL) {
	DBG_LOG (SEVERE, MOD_CCP, "Memory allocation failed");
	goto handle_err;
    }

    bytes_read = nkn_ccp_recv_message(
			conn_obj->sock, conn_obj->recv_buf + conn_obj->offset);
    if (bytes_read == 0) {
	glob_ccp_client_conn_reset_cnt++;
	DBG_LOG(MSG, MOD_CCP, "[%s] Server closed connection",
		conn_obj->crawl_name);
         conn_obj->status = ERROR_CCP;
         free (conn_obj->recv_buf);
         conn_obj->recv_buf = NULL;
         goto handle_err;
     }
    if (bytes_read == -1) {
	glob_ccp_client_query_ack_fail_cnt++;
	conn_obj->status = ERROR_CCP;
	free (conn_obj->recv_buf);
	conn_obj->recv_buf = NULL;
	goto handle_err;
    } else {
	conn_obj->offset += bytes_read; 
    }
    /*
    * This function does xml parsing and fills the dest_task_id and wait time
    * details.
    */
    conn_obj->recv_buf[conn_obj->offset] = '\0';
    err = nkn_ccp_check_if_proper_xml (conn_obj->recv_buf, conn_obj->offset);
    if (err == 0) {
	DBG_LOG (MSG, MOD_CCP, "[%s] Not a proper xml file, recv more data",
		conn_obj->crawl_name);
	fflush (stdout);
	nkn_ccp_client_change_epoll_event (EPOLL_IN_EVENTS, conn_obj);
    } else {
	err = nkn_ccp_client_parse_message (conn_obj, (char *)"QUERYACK");
	if (err == -1) {
	    conn_obj->status = ERROR_CCP;
	    free (conn_obj->recv_buf);
	    conn_obj->recv_buf = NULL;
	    goto handle_err;
	}
	err = conn_obj->status_callback_func (conn_obj->ptr);
	nkn_ccp_client_cleanup_url_buffers(conn_obj);
	if (err == -1) {
	    conn_obj->status = ERROR_CCP;
	    free (conn_obj->recv_buf);
	    conn_obj->recv_buf = NULL;
	    goto handle_err;
	}
	free (conn_obj->recv_buf);
	conn_obj->recv_buf = NULL;
	conn_obj->offset = 0;
    }
    return 0;

handle_err:
    return (-1);
}

int32_t
nkn_ccp_client_abort_task (ccp_client_session_ctxt_t *conn_obj)
{
    DBG_LOG (MSG, MOD_CCP, "[%s] Current state: ABORT_TASK\n",
	    conn_obj->crawl_name); 
    /*
     * Form abort xml
     */
    nkn_ccp_client_form_xml_message (conn_obj, CCP_ABRT);
    nkn_ccp_send_message (conn_obj->sock, (char *)conn_obj->xmlnode.xmlbuf);
    nkn_ccp_client_cleanup_send_buffers (&conn_obj->xmlnode);
    conn_obj->wait_time = DEFAULT_WAITING_TIME;
    conn_obj->status = RECV_ABRT_ACK;
    nkn_ccp_client_change_epoll_event (EPOLL_IN_EVENTS, conn_obj);
    return 0;
}

int32_t
nkn_ccp_client_recv_abrt_ack (ccp_client_session_ctxt_t *conn_obj)
{
    int err;
    int32_t bytes_read;

    if (conn_obj->recv_buf == NULL) {
        conn_obj->recv_buf = (char *)nkn_calloc_type(4096, 1, mod_ccp_recvbuf);
	conn_obj->offset = 0;
    } else {
        conn_obj->recv_buf = (char *)realloc(
				conn_obj->recv_buf, conn_obj->offset + 4096);
    }

    if (conn_obj->recv_buf == NULL) {
	DBG_LOG (SEVERE, MOD_CCP, "Memory allocation failed");
	goto handle_err;
    }

    bytes_read = nkn_ccp_recv_message(conn_obj->sock,
				      conn_obj->recv_buf + conn_obj->offset);
    if (bytes_read == 0) {
	glob_ccp_client_conn_reset_cnt++;
	DBG_LOG(MSG, MOD_CCP, "[%s] Server closed connection",
		conn_obj->crawl_name);
         conn_obj->status = ERROR_CCP;
         free (conn_obj->recv_buf);
         conn_obj->recv_buf = NULL;
         goto handle_err;
     }
    if (bytes_read == -1) {
	glob_ccp_client_abrt_ack_fail_cnt++;
	conn_obj->status = ERROR_CCP;
	free (conn_obj->recv_buf);
	conn_obj->recv_buf = NULL;
	goto handle_err;
    } else {
	conn_obj->offset += bytes_read; 
    }

    conn_obj->recv_buf[conn_obj->offset] = '\0';
    err = nkn_ccp_check_if_proper_xml (conn_obj->recv_buf, conn_obj->offset);
    if (err == 0) {
	DBG_LOG (MSG, MOD_CCP, "[%s] Not a proper xml file, recv more data",
		conn_obj->crawl_name);
	fflush (stdout);
	nkn_ccp_client_change_epoll_event (EPOLL_IN_EVENTS, conn_obj);
    } else {
	err = nkn_ccp_client_parse_message (conn_obj, (char *)"ABRTACK");
	if (err == -1) {
	    conn_obj->status = ERROR_CCP;
	    free (conn_obj->recv_buf);
	    conn_obj->recv_buf = NULL;
	    goto handle_err;
	}
	conn_obj->status = TASK_COMPLETED;
	nkn_ccp_client_change_epoll_event (EPOLL_OUT_EVENTS, conn_obj);
	free (conn_obj->recv_buf);
	conn_obj->recv_buf = NULL;
	conn_obj->offset = 0;
    }
    return 0;

handle_err:
    return (-1);
}

int32_t
nkn_ccp_client_task_completed (ccp_client_session_ctxt_t *conn_obj)
{
    DBG_LOG (MSG, MOD_CCP, "[%s] Current state: TASK_COMPLETED\n",
	    conn_obj->crawl_name);
    /*
     * Dont know if we can free here.
     */
    nkn_ccp_client_remove_from_waitq (conn_obj);
    if (conn_obj->recv_buf) {
	free(conn_obj->recv_buf);
    }
    conn_obj->cleanup_callback_func (conn_obj->ptr);
    return 0;
}

int32_t
nkn_ccp_client_delete_epoll_event(int32_t sock)
{
    return(epoll_ctl(g_ccp_epoll_node->epoll_fd, EPOLL_CTL_DEL, sock, NULL));
}

int32_t
nkn_ccp_client_post_task (ccp_client_session_ctxt_t *conn_obj)
{
    int err;
    DBG_LOG (MSG, MOD_CCP, "[%s] Current state: POST_TASK\n",
	    conn_obj->crawl_name);
    err = nkn_ccp_send_message(conn_obj->sock,
			       (char *)conn_obj->xmlnode.xmlbuf);
    if (err == -1) {
        DBG_LOG (SEVERE, MOD_CCP, "[%s] Send failed",
		conn_obj->crawl_name);
        conn_obj->status = ERROR_CCP_POST;
	goto handle_err;
    } else {
        conn_obj->wait_time = DEFAULT_WAITING_TIME;
        nkn_ccp_client_add_to_waitq (conn_obj);
        conn_obj->status = RECV_POST_ACK;
        nkn_ccp_client_change_epoll_event (EPOLL_IN_EVENTS, conn_obj);
    }
    return 0;

handle_err:
    return (-1);
}

int32_t
nkn_ccp_client_end_payload(ccp_client_session_ctxt_t *conn_obj)
{
    int32_t err;
    err = nkn_ccp_end_xml_payload(&conn_obj->xmlnode);
    if (err == -1)
	goto handle_err;

    err = nkn_ccp_client_add_epoll_event(EPOLL_OUT_EVENTS, conn_obj);
    if (err == -1)
	goto handle_err;

    return 0;
handle_err:
    return(-1);
}

void
nkn_ccp_client_set_status_abort(ccp_client_session_ctxt_t *conn_obj)
{
    DBG_LOG(MSG, MOD_CCP,
	    "[%s] Setting the status to ABORT_TASK, prev status: %d",
	    conn_obj->crawl_name, conn_obj->status);
    conn_obj->status = ABORT_TASK;
    nkn_ccp_client_change_epoll_event (EPOLL_OUT_EVENTS, conn_obj);
}

int32_t
nkn_ccp_client_check_if_task_completed(ccp_client_session_ctxt_t *conn_obj)
{

    switch (conn_obj->status) {
	case SESSION_INITIATED:
	    return 0; //cleanup
	case ABORT_TASK:
	    nkn_ccp_client_set_status_abort(conn_obj);
	case POST_TASK:
	    return -1;
	case SEND_QUERY:
	case RECV_POST_ACK:
	case RECV_QUERY_RESPONSE:
	case RECV_ABRT_ACK:
	     conn_obj->wait_time = 0;
	     return -1;
	case TASK_COMPLETED:
	case ERROR_CCP:
	    nkn_ccp_client_change_epoll_event (EPOLL_OUT_EVENTS, conn_obj);
	    return -1;
	default:
	    return -1;
    }
}


void
nkn_ccp_client_cleanup_url_buffers(ccp_client_session_ctxt_t *conn_obj)
{
    url_t *ptr, *ptr1;
    ptr = conn_obj->urlnode;
    while(ptr) {
       if (ptr->url) {
           free(ptr->url);
       }
       ptr1 = ptr->next;
       free(ptr);
       ptr = ptr1;
    }
    conn_obj->urlnode = NULL;
}

