#ifndef _HTTP_FLOW_H
#define _HTTP_FLOW_H 1

#define MAX_HDR		    (16)
#define MAX_FLOW	    (512*1024)
#define MAX_INDEX	    (64)

typedef enum http_flow_state {
	HTTP_FLOW_FREE = 0,
	HTTP_FLOW_REQ,
	HTTP_FLOW_RESP,
} http_flow_state_t;

typedef struct http_hdr {
	char *name;
	char *value;
	int hdr_len;
	int value_len;
} http_hdr_t;

typedef struct http_flow {
	http_flow_state_t state;
	int flow_index;
	int index;
	int req_cur_index;
	int resp_cur_index;
	int resp_code;
	int content_length;
	int req_hdr_cnt;
	int resp_hdr_cnt;
	char *req_line;
	int req_line_len;
	http_hdr_t req_hdr[MAX_HDR];
	http_hdr_t resp_hdr[MAX_HDR];
	struct http_flow *index_table[];
} http_flow_t;


#endif

