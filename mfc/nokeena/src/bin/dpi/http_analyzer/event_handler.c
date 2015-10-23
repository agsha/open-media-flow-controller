/*
 * File: event_handler.c
 *   
 * Author: Sivakumar Gurmurthy 
 *
 *
 * (C) Copyright 2014 Juniper Networks, Inc.
 * All rights reserved.
 */

#ifdef _STUB_

/* Include Files */
#include <stdio.h>
#include <string.h>
#include <libwrapper/uwrapper.h>

#include "event_handler.h"
#include "thread.h"

#include "atomic_ops.h"
#include "nkn_stat.h"
#include "nkn_debug.h"

/* Extern Variables */
extern unsigned long long int packet_number;

/* Extern Function Prototypes */
void* my_calloc(size_t nmemb, size_t size);
void my_free(void *ptr);

/*
 * Local function that checks if the header that is being  processesed
 * is something we are interested in
 */
static int
isHeaderNameMatched (char *hname)
{
	unsigned int i = 0;
	const char *headers[] = {"Age", "Content-Length",
		"Range", "Host", "Referer", "Pragma", "ETag","Date",
		"Expires", "Authorization", "Cache-Control",
		"Cookie", "Transfer-Encoding", "Update"};


	for (i = 0; i < sizeof(headers)/sizeof(headers[0]); ++i) 
	{
		if (!strcasecmp(hname, headers[i]))
			return 1;
	}
	return 0;
}

// Utility function to convert the ipaddress into string
static inline void ip4_addr2str(const ctb_uint32 addr, char *str) {
    const unsigned char *bytes;
    int res;
 
    bytes = (const unsigned char *) &addr;
    res = sprintf(str, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
    if (res <= 0)
        str[0] = '\0';
}

// Function to get the source ipaddres from uapp_cnx
static void
get_src_ip(struct uapp_cnx *uapp_cnx, char *src_addr)
{
    struct dpi_flow_sig *sig;
    uip_hdr_t           *iphdr;
 
    sig         =  (struct dpi_flow_sig *)uapp_cnx->pkt->hdr.flowdata;
    iphdr       = dpi_flow_sig_get_nth_header( sig , dpi_flow_sig_get_iplevel(sig));
 
   ip4_addr2str(iphdr->saddr, src_addr);
}


// Function to get the destination ipaddres from uapp_cnx
static void
get_dest_ip(struct uapp_cnx *uapp_cnx, char *dest_addr)
{
    struct dpi_flow_sig *sig;
    uip_hdr_t           *iphdr;
 
    sig         =  (struct dpi_flow_sig *)uapp_cnx->pkt->hdr.flowdata;
    iphdr       = dpi_flow_sig_get_nth_header( sig , dpi_flow_sig_get_iplevel(sig));
 
   ip4_addr2str(iphdr->daddr, dest_addr);
}

// Function  
static void
http_session_handler(uapp_cnx_t *uapp_cnx,
				const uevent_t *ev __attribute((unused)),
				void *param __attribute((unused)))
{
	uapp_cnx->user_handle = NULL;
}

// Function  
static void
http_new_packet_handler(uapp_cnx_t *uapp_cnx,
				const uevent_t *ev __attribute((unused)),
				void *param __attribute((unused)))
{
	time_t rawtime;
        struct tm *info;
        char buffer[30];
	int len;
	int thread_id;


	/* Get the current time */
#if 0 // COMMENTED OUT - Ram
	// This logic is very expensive and since timestamp is not 
	// used at this time, we commented this logic
	//
        time (&rawtime);
        info = localtime(&rawtime);
        strftime(buffer,80, "[%d/%b/%Y:%H:%M:%S %z]", info);
#else
	/* For now just setting to '-' */
	buffer[0] = '-';
	buffer[1] = '\0';
#endif // 0
	len = strlen(buffer);

	/* Get the thread id to reference into the global array */
	thread_id = afc_get_cpuid();

	if (uapp_cnx->ucnx_type == CLEP_CNX_SERVER) {
		// print_ip_port(uapp_cnx);
		thread_details[thread_id].num_resp++;
		// printf("#### HTTP RESPONSE BEGIN PACKET#####\n");
		// printf("THE HTTP RESP PACKET address is %p\n", uapp_cnx);
	}
	else  {
		thread_details[thread_id].num_req++;
		// printf("#### HTTP REQUEST BEGIN PACKET#####\n");
		// printf("THE HTTP REQ PACKET address is %p\n", uapp_cnx);
	}
	datalist *httplist = (datalist *)uapp_cnx->user_handle;
	http_pkt * packet = (http_pkt *)my_calloc(1,sizeof(http_pkt));
	if (packet == NULL) {
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"Out of Memory");
		exit(1);
	}
	packet->uapp_cnx = uapp_cnx;
	packet->thread_id = afc_get_cpuid();
	strncpy(packet->timestamp, buffer, len);
	packet->timestamp[len]='\0';
	datalist_add(&httplist, packet);
	if (uapp_cnx->user_handle == NULL)
	{
		uapp_cnx->user_handle = httplist;

		/* DEBUG - Ram */
//		printf ("[%d]datalist_add () : new connection %p\n", thread_id, httplist);
	}
		
	return;
}


static void
http_index_handler(uapp_cnx_t *uapp_cnx, const uevent_t *ev,
				void *param __attribute((unused)))
{
	http_pkt *packet = NULL;
	uint32_t *value = (uint32_t *)ev->data;
	datalist * httplist = (datalist *)uapp_cnx->user_handle;
	datalist *curNode = get_curNode(&httplist);
	packet = (http_pkt *)curNode->data;
	if (packet){
	packet->index = *value;
	}
}


static void
http_header_statuscode_handler(uapp_cnx_t *uapp_cnx,
				const uevent_t *ev,
				void *param __attribute((unused)))
{
	uint32_t *value = (uint32_t *)ev->data;
	http_pkt *packet = NULL;
	datalist *httplist = (datalist *)uapp_cnx->user_handle;
	datalist *curNode = get_curNode(&httplist);

	packet = (http_pkt *)curNode->data;
	if (packet)
		packet->statuscode = *value;
	// printf("THE HTTP STATUS CODE is %d\n", packet->statuscode);
}

/*
 * Callback function for the Request Line
 */
static void http_header_requestline_handler(uapp_cnx_t *uapp_cnx,
                                         const uevent_t *ev,
                                         void *param __attribute((unused))) {
	http_pkt *packet = NULL;
	char *value = (char *)ev->data;
	int len = ev->len;
	char *reqline = (char *)my_calloc((len+1),sizeof(char));
	if (reqline == NULL)
	{
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"Out of Memory");
		exit(1);
	}
	strncpy(reqline, value, len);
	reqline[len]='\0';
	datalist *httplist = (datalist *)uapp_cnx->user_handle;
	datalist *curNode = get_curNode(&httplist);
	packet = (http_pkt *)curNode->data;
	if (packet) {
		packet->reqline = (char *)my_calloc((len+1),sizeof(char));
		if (packet->reqline == NULL)
		{
			fprintf(stderr, "Out of Memory\n");
			exit(1);
		}
		strncpy(packet->reqline, reqline, len);
		packet->reqline[len]='\0';
	}

	/* Free the reqline allocated in the beginning */
	my_free(reqline);
}

/*
 * Callback function for the COOKIE
 * We receive only the COOKIE value in this callback.
 * Create the http_pkt structure, copy the header name into it
 * and assign it to the user_handle.
 * The header value namely the cookie is then added to the value
 * for the above header name. 
 */
static void
http_cookie_handler(uapp_cnx_t *uapp_cnx,
                                     const uevent_t *ev,
                                     void *param __attribute((unused)))
{
#define COOKIE_HEADER_NAME "Cookie"
	http_pkt *packet = NULL;
	const char *hname = COOKIE_HEADER_NAME;
	int len = strlen(COOKIE_HEADER_NAME);
	char *name = (char *)my_calloc((len+1),sizeof(char));
	if (name == NULL)
	{
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"Out of Memory");
		exit(1);
	}

	/* First add the header name COOKIE to the list */
	/* Logic is similar to that in header_name callback below */
	strncpy(name, hname, len);
	name[len]='\0';
	int thread_id = afc_get_cpuid();

	datalist *httplist = (datalist *)uapp_cnx->user_handle;
	datalist *curNode = get_curNode(&httplist);
	packet = (http_pkt *)curNode->data;
	// Copy the header name
	if (packet){
		http_hdr * header_packet_list = packet->httphdr_list;
		httphdr_list_add_name(&header_packet_list, name, len);
		// Now link the http_hdr packet to the http_pkt structure
		packet->httphdr_list = header_packet_list;
	}
	// Free the allocated name
	my_free(name);

	/* Now add the COOKIE value to the list */
	/* Logic is similar to that in header_value callback below */
	char *hvalue = (char *) ev->data;
	len = ev->len;
	if (packet) {
		// Get the cur http hdr packet node
		http_hdr *header_packet = get_cur_httphdr_node(&packet->httphdr_list);
		// Add the value against the corresponding header name
		header_packet->header_value = (char *)my_calloc((len+1),sizeof(char));
		if (header_packet->header_value == NULL) {
			DBG_LOG(ERROR, MOD_DPI_TPROXY,"Out of Memory\n");
			exit(1);
		}
		/* Copy the header value. The name is already set
		 * The hvalue may contain more output than required
		 * Copy only the length returned by the Qosmos
		 */
		strncpy(header_packet->header_value, hvalue, len);
		header_packet->header_value[len]='\0';
	}
}

/*
 * Callback function for the Http header name
 * Create the http_pkt structure, copy the header name into it
 * and assign it to the user_handle.
 * The header value callback function will use http_pkt stucture
 * via the user handle
 */
static void
http_header_name_handler(uapp_cnx_t *uapp_cnx,
                                     const uevent_t *ev,
                                     void *param __attribute((unused)))
{
	http_pkt *packet = NULL;
	char *hname = (char *) ev->data;
	int len = ev->len;
	char *name = (char *)my_calloc((len+1),sizeof(char));
	if (name == NULL)
	{
		DBG_LOG(ERROR, MOD_DPI_TPROXY,"Out of Memory");
		exit(1);
	}
	strncpy(name, hname, len);
	name[len]='\0';
	int thread_id = afc_get_cpuid();
	if (isHeaderNameMatched(name)) 
	{
		// printf("The header name is %s uapp_cnx %p\n", name, uapp_cnx);
		thread_details[thread_id].header_matched = 1;
		datalist *httplist = (datalist *)uapp_cnx->user_handle;
		datalist *curNode = get_curNode(&httplist);
		packet = (http_pkt *)curNode->data;
		// Copy the header name
		if (packet){
			http_hdr * header_packet_list = packet->httphdr_list;
			httphdr_list_add_name(&header_packet_list, name, len);
			// Now link the http_hdr packet to the http_pkt structure
			packet->httphdr_list = header_packet_list;
		}
	}
	else
		thread_details[thread_id].header_matched = 0;
	//printf( "Thread id: %d, match: %d, Hdr %s\n", thread_id, 
	//	thread_details[thread_id].header_matched, name );
	// Free the allocated name
	my_free(name);
}

/*
 * Callback function for the Http header value
 * Copy the header value in to the http_pkt structure
 * and add the structure to the list
 * Reset the user_handle as the next new pointer to the http_pkt
 * structure has to be put there
 */
static void
http_header_value_handler(uapp_cnx_t *uapp_cnx,
                          const uevent_t *ev,
                          void *param __attribute((unused))) 
{
	int thread_id = afc_get_cpuid();
	if (thread_details[thread_id].header_matched)
	{
		// If there is a header match copy the corresponding header value
		http_pkt *packet = NULL;
		char *hvalue = (char *) ev->data;
		int len = ev->len;
		datalist *httplist = (datalist *)uapp_cnx->user_handle;
		datalist *curNode = get_curNode(&httplist);
		// printf("uapp_cnx in value is %p **%d\n", uapp_cnx, len);
		// printf("The header value is %s\n", hvalue);
		//printf("Thread : %d, uapp_cnx: %p **%d, Val %.*s\n", thread_id, uapp_cnx, len, len, hvalue);
		packet = (http_pkt *)curNode->data;
		if (packet && packet->httphdr_list) {
			// Get the cur http hdr packet node
			//if (packet->httphdr_list == NULL)
			//	printf("Thread : %d, uapp_cnx: %p Hdr_lst: %p\n", thread_id, uapp_cnx, packet->httphdr_list);
			http_hdr *header_packet = get_cur_httphdr_node(&packet->httphdr_list);
			// Add the value against the corresponding header name
			header_packet->header_value = (char *)my_calloc((len+1),sizeof(char));
			if (header_packet->header_value == NULL) {
				DBG_LOG(ERROR, MOD_DPI_TPROXY,"Out of Memory");
				exit(1);
			}
			/* Copy the header value. The name is already set
			 * The hvalue may contain more output than required
			 * Copy only the length returned by the Qosmos
			 */
			strncpy(header_packet->header_value, hvalue, len);
			header_packet->header_value[len]='\0';
		}
	}
} /* end of http_header_value_handler() */


static void
http_header_end_handler(uapp_cnx_t *uapp_cnx,
                        const uevent_t *ev __attribute ((unused)),
                        void *param __attribute ((unused))) 
{
	const char *age="-", *cache_control="-", *content_len="-", *range="-";
	const char *host="-", *pragma="-", *etag="-", *referer="-";
	const char *reqline="-", *timestamp = "-", *req_transfer_encoding = "-";
	const char *date="-", *expires="-", *req_content_len="-";
	const char *authorization="-", *cookie="-", *transfer_encoding="-";
	const char *update = "-", *sc_pragma = "-", *sc_cache_control = "-";
	const char *cookie_y="y";
	http_pkt *req_pkt = NULL;
	http_pkt *resp_pkt = NULL;
	datalist *req_list = NULL;
	int statuscode=0;
	char logresult[2048]; /* Using a 2K buffer for log entry */
	char dst_ipaddr[16];


	bzero(&logresult, sizeof(logresult));
	int thread_id = afc_get_cpuid();
	if (uapp_cnx->ucnx_type == CLEP_CNX_SERVER) {
		if (uapp_cnx->peer)
			req_list = (datalist *)uapp_cnx->peer->user_handle;
		else {
			thread_details[thread_id].no_peer_counter++;
		}
		datalist *resp_list = (datalist *)uapp_cnx->user_handle;

		// Locate the correct response packet
		datalist *curNode = get_curNode(&resp_list);
		resp_pkt = (http_pkt *)curNode->data;

		// Go through the httplist in the Request list and find the 
		// corresponding request for the response
		for ( ; req_list != NULL; req_list = req_list->next)
		{
			req_pkt = (http_pkt *)req_list->data;

			// Match both the uapp_cnx context and in the 
			// index to get the corresponding request packet
			if(req_pkt && req_pkt->uapp_cnx == uapp_cnx->peer 
					&& req_pkt->index == resp_pkt->index)
			{
				reqline = req_pkt->reqline;
				timestamp = req_pkt->timestamp;
				http_hdr  *req_hdr_list = (http_hdr *)req_pkt->httphdr_list;
				for ( ; req_hdr_list != NULL; req_hdr_list = req_hdr_list->next)
				{
					if (!strcasecmp(req_hdr_list->header_name, "Cache-Control"))
						cache_control = req_hdr_list->header_value;
					else if (!strcasecmp(req_hdr_list->header_name, "Range"))
						range = req_hdr_list->header_value;
					else if (!strcasecmp(req_hdr_list->header_name, "Pragma"))
						pragma = req_hdr_list->header_value;
					else if (!strcasecmp(req_hdr_list->header_name, "Referer"))
						referer = req_hdr_list->header_value;
					else if (!strcasecmp(req_hdr_list->header_name, "Host"))
						host = req_hdr_list->header_value;
					else if (!strcasecmp(req_hdr_list->header_name, "Authorization"))
						authorization = req_hdr_list->header_value;
					else if (!strcasecmp(req_hdr_list->header_name, "Cookie"))
						//cookie = req_hdr_list->header_value;
						cookie = cookie_y;
					else if (!strcasecmp(req_hdr_list->header_name, "Content-Length"))
						req_content_len = req_hdr_list->header_value;
					else if (!strcasecmp(req_hdr_list->header_name, "Update"))
						update = req_hdr_list->header_value;
					else if (!strcasecmp(req_hdr_list->header_name, "Transfer-Enconding"))
						req_transfer_encoding = req_hdr_list->header_value;
				}
			}
		}
		http_hdr  *resp_hdr_list = (http_hdr *)resp_pkt->httphdr_list;
		for ( ; resp_hdr_list != NULL; resp_hdr_list = resp_hdr_list->next)
		{
			if (!strcasecmp(resp_hdr_list->header_name, "Content-Length"))
				content_len = resp_hdr_list->header_value;
			else if (!strcasecmp(resp_hdr_list->header_name, "ETag"))
				etag = resp_hdr_list->header_value;
			else if (!strcasecmp(resp_hdr_list->header_name, "Age"))
				age = resp_hdr_list->header_value;
			else if (!strcasecmp(resp_hdr_list->header_name, "Expires"))
				expires = resp_hdr_list->header_value;
			else if (!strcasecmp(resp_hdr_list->header_name, "Transfer-Encoding"))
				transfer_encoding = resp_hdr_list->header_value;
			else if (!strcasecmp(resp_hdr_list->header_name, "Date"))
				date = resp_hdr_list->header_value;
			else if (!strcasecmp(resp_hdr_list->header_name, "Pragma"))
						sc_pragma = resp_hdr_list->header_value;
			else if (!strcasecmp(resp_hdr_list->header_name, "Cache-Control"))
						sc_cache_control = resp_hdr_list->header_value;
		}

		statuscode = resp_pkt->statuscode;

		// Get the destination ipaddress to be logged
		// Please note that since we are interested in the destination IP
		// of the original request, here in the response logic we 
		// actually need the source IP, hence calling get_src_ip ()
		//
		get_src_ip(uapp_cnx, dst_ipaddr);

		// Print the log entry
		snprintf(logresult, sizeof(logresult)-1,
		"%s \"%s\" %d %s %s \"%s\" %s %s \'%s\' %s %s %s \"%s\" %s %s %s %s \"%s\" \"%s\" %s %s",
			host, reqline, statuscode, content_len, age,
			range, pragma, cache_control, etag, referer, dst_ipaddr,
			authorization, cookie, req_content_len, update,
			req_transfer_encoding, transfer_encoding, date,
			expires, sc_pragma, sc_cache_control);

		thread_details[thread_id].counter++;
		fprintf(thread_details[thread_id].fp, "%s\n", logresult);
		thread_details [thread_id].bytes_written += strlen(logresult);

		/* Now that we have written the log, 
		 * we can free the header lists for this req-resp */
		if (req_pkt)
			httphdr_list_free(&(req_pkt->httphdr_list));
		if (resp_pkt)
			httphdr_list_free(&(resp_pkt->httphdr_list));

		resp_hdr_list = (http_hdr *)resp_pkt->httphdr_list;
	} /* end of if CLEP_CNX_SERVER */
	else if (uapp_cnx->ucnx_type == CLEP_CNX_CLIENT){
		thread_details[thread_id].num_req_end++;
	}

} /* end of http_header_end_handler() */


/*
 * Register all the intereste HTTP attributes with their callbak functions
 * Open the accesslog file
 */
void
event_handler_init(void) 
{
        uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_REQUEST,
                              http_new_packet_handler, NULL);
        uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_INDEX,
                              http_index_handler, NULL);
        uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_HEADER_NAME,
                              http_header_name_handler, NULL);
        uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_COOKIE,
                              http_cookie_handler, NULL);
        uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_HEADER_VALUE,
                              http_header_value_handler, NULL);
        uevent_hooks_add_parm(Q_PROTO_BASE, Q_BASE_FLOW_ID,
                              http_session_handler, NULL);
        uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_HEADER_STATUSLINE,
                              http_header_requestline_handler, NULL);
        uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_CODE,
                              http_header_statuscode_handler, NULL);
	 uevent_hooks_add_parm(Q_PROTO_HTTP, Q_HTTP_HEADER_END,
                                 http_header_end_handler, NULL);
} /* end of event_handler_init() */


void
event_handler_cleanup(void) 
{
        uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_HEADER_NAME,
                                 http_header_name_handler, NULL);
        uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_COOKIE,
                                 http_cookie_handler, NULL);
        uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_INDEX,
                              http_index_handler, NULL);
        uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_REQUEST,
                              http_new_packet_handler, NULL);
        uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_HEADER_VALUE,
                                 http_header_value_handler, NULL);
        uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_HEADER_STATUSLINE,
                              http_header_requestline_handler, NULL);
        uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_CODE,
                              http_header_statuscode_handler, NULL);
	uevent_hooks_remove_parm(Q_PROTO_HTTP, Q_HTTP_HEADER_END,
                                 http_header_end_handler, NULL);
        uevent_hooks_remove_parm(Q_PROTO_BASE, Q_BASE_FLOW_ID,
                              http_session_handler, NULL);

} /* end of event_handler_cleanup() */

#endif /* _STUB_ */

/* End of event_handler.c */
