#include <tcl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#ifndef _UINT64_T_DECLARED
typedef	__uint64_t		uint64_t;
typedef	__uint32_t		uint32_t;
#define	_UINT64_T_DECLARED
#endif

#define PE_CHECK
#include "pe_defs.h"

int	 strcasecmp(const char *, const char *);
int	 strncasecmp(const char *, const char *, size_t);


static int pec_http_recv_req_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[]);
static int pec_http_recv_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[]);
static int pec_om_recv_resp_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[]);
static int pec_om_recv_resp_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[]);
int pec_eval_http_recv_request(void);
int pec_eval_om_recv_response( void );
int pec_eval_om_send_request( void );
int pec_eval_http_send_response(void);

char *pec_read_policy_file(char * pe_name, int *buf_length);
int pec_scan_policy(char *buf, int buf_length);
static int pec_check_gettokens( char *buf, int line );
static int pec_check_setactions( char *buf, int line );


Tcl_Interp *g_interp;
char host[] = "www.juniper.net";
char uri[] = "/test/test.html";

typedef struct param_info {
	char name[64];
	int num_args;
	int flags;
} param_info_t;

param_info_t getparam_info[] = {
	{TOKEN_SOCKET_SOURCE_IP, 0, 0},
	{TOKEN_SOCKET_SOURCE_PORT, 0, 0},
	{TOKEN_SOCKET_DEST_IP, 0, 0},
	{TOKEN_SOCKET_DEST_PORT, 0, 0},
	{TOKEN_HTTP_REQ_HOST, 0, 0},
	{TOKEN_HTTP_REQ_URI, 0, 0},
	{TOKEN_HTTP_REQ_COOKIE, 1, 0},
	{TOKEN_HTTP_REQ_HEADER, 1, 0},
	{TOKEN_HTTP_REQ_METHOD, 0, 0},
	{TOKEN_HTTP_REQ_QUERY, 0, 0},
	{TOKEN_HTTP_REQ_URL_CATEGORY, 0, 0},
	{TOKEN_OM_RES_STATUS_CODE, 0, 0},
	{TOKEN_OM_RES_HEADER, 0, 0},
	{TOKEN_HTTP_RES_STATUS_CODE, 0, 0},
	{TOKEN_HTTP_RES_HEADER, 1, 0},
//	{TOKEN_CALLOUT_STATUS_CODE, 0, 0},
//	{TOKEN_CALLOUT_RESPONSE, 0, 0},
//	{TOKEN_CALLOUT_SERVER, 0, 0},
	{TOKEN_GEO_STATUS_CODE, 0, 0},
	{TOKEN_GEO_CONTINENT_CODE, 0, 0},
	{TOKEN_GEO_COUNTRY_CODE, 0, 0},
	{TOKEN_GEO_COUNTRY, 0, 0},
	{TOKEN_GEO_STATE, 0, 0},
	{TOKEN_GEO_CITY, 0, 0},
	{TOKEN_GEO_POSTAL_CODE, 0, 0},
	{TOKEN_GEO_ISP, 0, 0},
	{"", 0, 0}
};

param_info_t setaction_info[] = {
	{ACTION_SET_IP_TOS, 1, 0},
	{ACTION_REJECT_REQUEST, 0, 0},
	{ACTION_URL_REWRITE, 2, 0},
	{ACTION_REDIRECT, 1, 0},
	{ACTION_CACHE_OBJECT, 0, 0},
	{ACTION_NO_CACHE_OBJECT, 0, 0},
	{ACTION_SET_ORIGIN_SERVER, 1, 0},
	{ACTION_INSERT_HEADER, 2, 0},
	{ACTION_REMOVE_HEADER, 1, 0},
	{ACTION_MODIFY_HEADER, 2, 0},
//	{ACTION_SET_CACHE_NAME, 0, 0},
	{ACTION_CLUSTER_REDIRECT, 0, 0},
	{ACTION_CLUSTER_PROXY, 0, 0},
//	{ACTION_CALLOUT, 0, 0},
//	{ACTION_IGNORE_CALLOUT_RESP, 0, 0},
	{ACTION_CACHE_INDEX_APPEND, 1, 0},
	{ACTION_TRANSMIT_RATE, 1, 0},
	{ACTION_CL_DEFALUT_ACTION, 0, 0},
	{ACTION_MODIFY_RESPONSE_CODE, 1, 0},
	{"", 0, 0}
};

int main(int argc, char **argv)
{
	int ret;
	char *res;
	char *policy_buf;
	int buf_length;

	g_interp = Tcl_CreateInterp();
	Tcl_Init(g_interp);
	ret = Tcl_EvalFile(g_interp, argv[1]);
	if (ret != TCL_OK) {
		printf("Error Msg: %s\n", Tcl_GetStringResult(g_interp));
		return( 1 );
	}
	
	policy_buf = pec_read_policy_file(argv[1], &buf_length);
	/* If unable to read file return error 
	 */
	if (policy_buf == NULL) {
		return( 1 );
	}
	ret = pec_scan_policy(policy_buf, buf_length);
	if (ret < 0 ) {
		return( 6 );
	}
	
	ret = pec_eval_http_recv_request();
	if (ret < 0 ) {
		//printf("Res: %s\n", g_interp->result);
		return( 2 );
	}

	ret = pec_eval_om_recv_response();
	if (ret < 0 ) {
		//printf("Res: %s\n", g_interp->result);
		return( 3 );
	}

	ret = pec_eval_om_send_request();
	if (ret < 0 ) {
		//printf("Res: %s\n", g_interp->result);
		return( 4 );
	}

	ret = pec_eval_http_send_response();
	if (ret < 0 ) {
		//printf("Res: %s\n", g_interp->result);
		return( 5 );
	}

	return( 0 );
}

char *pec_read_policy_file(char * pe_name, int *buf_length)
{
	FILE * fp;
	char * script;
	size_t ret;
	struct stat bstat;
	int size = 0;
	char script_fn[1024];

	*buf_length = 0;
	if(pe_name == NULL) {
		return(NULL);
	}
	else {
		snprintf(script_fn, 1024, "%s", pe_name);
	}

	if (stat(script_fn, &bstat) == -1) {
		printf( "Policy file does not exist %s", script_fn);
		return NULL; // File does not exist
	}
	size = bstat.st_size;
	script = (char *)malloc(sizeof(char) * (size + 1 + 40));
	if (!script) {
		printf("Unable to alloc memory %s", script_fn);
		return NULL;
	}

	fp = fopen(script_fn, "r");
	if (fp == NULL) {
		printf("Cannot open script file %s", script_fn);
		free(script);
		return NULL;
	}
	ret = fread(script, size, 1, fp);
	fclose(fp);

	script[size] = 0;

	*buf_length = size;
	return script;
}

int pec_scan_policy(char *buf, int buf_length)
{
	int flag = 0;
	int pos = 0;
	//int level = 0;
	int proc_num = 0;
	int line = 1;
	int error = 0;

	while (pos < buf_length) {
		switch (buf[pos]) {
		case '\n':
			line++;
			break;
		case '#':
			while (buf[pos] != '\n')
				pos++;
			line++;
			break;
#if 0			
		case '{':
			level++;
			break;
		case '}':
			level--;
			break;
#endif			
		case '"':
			while (buf[pos] != '"')
				pos++;
			break;
		default:
			if (isalpha(buf[pos])) {
				if ((buf[pos] == 'p' || buf[pos] == 'P') && 
						(strncasecmp(&buf[pos], "pe_", 3) == 0)) {
					if (strncasecmp(&buf[pos], "pe_http_recv_request", 20) == 0) {
						proc_num = 1;
						pos += 20;
					}
					else if (strncasecmp(&buf[pos], "pe_http_send_response", 21) == 0) {
						proc_num = 2;
						pos += 21;
					}
					else if (strncasecmp(&buf[pos], "pe_om_send_request", 18) == 0) {
						proc_num = 3;
						pos += 18;
					}
					else if (strncasecmp(&buf[pos], "pe_om_recv_response", 19) == 0) {
						proc_num = 4;
						pos += 19;
					}
					else if (strncasecmp(&buf[pos], "pe_cl_http_recv_request", 23) == 0) {
						proc_num = 5;
						pos += 23;
					}
				}
				else if (buf[pos] == 'g' || buf[pos] == 'G') {
					if (strncasecmp(&buf[pos], "getparam", 8) == 0) {
						flag |= 1 << proc_num;
						pos += 8;
						error += pec_check_gettokens(&buf[pos], line);
					}
				}
				else if (buf[pos] == 's' || buf[pos] == 'S') {
					if (strncasecmp(&buf[pos], "setaction", 9) == 0) {
						flag |= 1 << proc_num;
						pos += 9;
						error += pec_check_setactions(&buf[pos], line);
					}
				}
			}
		}
		pos++;
	}
	return error;
}

static int pec_get_num_params( char *buf )
{
	int i = 0;
	char *p = buf;
	char *p1;
	int len = 0;
	char tok[128];

	/* Skip any space, tab or trailing quote after the token name
	 */
	if (*p == '\"') {
		p++;
	}
	while (*p && (*p == ' ' || *p == '\t')) {
		p++;
	}

	/* Scan till end of line
	 */
	while (*p && !(*p == ']' || *p == ';' || *p == '\n')) {
		/* Skip space or tab
		*/
		while (*p && (*p == ' ' || *p == '\t')) {
			p++;
		}
		p1 = p;
		if (*p == '\"') {
			/* If the parameter is within quotes, skip till ending qoute
			 */
			p++;
			while (*p && *p++ != '\"') {
				;
			}
		}
		else {
			/* Else skip till separator or end of line
			 */
			while (*p && !(*p == ' ' || *p == '\t' || *p == ']' || *p == ';' || *p == '\n')) {
				p++;
			}
		}
		/* If pointer has moved, count one parameter
		 */
		if (p != p1) {
			i++;
		}
	}
	return( i );
}

static int pec_check_gettokens( char *buf, int line )
{
	int i = 0;
	char *p = buf;
	char *p1;
	int len = 0;
	char tok[128];
	int n;

	while (*p && (*p == ' ' || *p == '\t' || *p == '\"')) {
		p++;
	}
	p1 = p;
	while (*p1 && !(*p1 == ' ' || *p1 == '\t' || *p1 == '\"' || *p1 == ']' || *p1 == ';' || *p1 == '\n')) {
		p1++;
	}
	len = p1 - p;
	strncpy(tok, p, len);
	tok[len] = 0;
	while (getparam_info[i].name[0]) {
		if (strncasecmp(p, getparam_info[i].name, len) == 0) {
			n = pec_get_num_params(p1);
			if (n < getparam_info[i].num_args) {
				printf( "Error: Line no. %d - Not enough parameters for token: %s\n", line, tok );
				return(-1);
			}
			return(0);
		}
		i++;
	}
	if (getparam_info[i].name[0] == 0) {
		printf( "Error: Line no. %d - Unknown token: %s\n", line, tok );
		return(-1);
	}
	
	return( 0 );
}

static int pec_check_setactions( char *buf, int line )
{
	int i = 0;
	char *p = buf;
	char *p1;
	int len = 0;
	char tok[128];
	int n;

	while (*p && (*p == ' ' || *p == '\t' || *p == '\"')) {
		p++;
	}
	p1 = p;
	while (*p1 && !(*p1 == ' ' || *p1 == '\t' || *p1 == '\"' || *p1 == ']' || *p1 == ';' || *p1 == '\n')) {
		p1++;
	}
	len = p1 - p;
	strncpy(tok, p, len);
	tok[len] = 0;
	while (setaction_info[i].name[0]) {
		if (strncasecmp(p, setaction_info[i].name, len) == 0) {
			n = pec_get_num_params(p1);
			if (n < setaction_info[i].num_args) {
				printf( "Error: Line no. %d - Not enough parameters for action: %s\n", line, tok );
				return(-1);
			}
			return(0);
		}
		i++;
	}
	if (setaction_info[i].name[0] == 0) {
		printf( "Error: Line no. %d - Unknown action: %s\n", line, tok );
		return(-1);
	}
	
	return( 0 );
}

static int pec_http_recv_req_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;

	if (objc < 2) {
		goto ht_gp_error;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);

	/*
	 * Tokens for HTTP structure
	 */
	if(strcasecmp(token, TOKEN_HTTP_REQ_HOST) == 0) {
		Tcl_SetStringObj(res, host, strlen(host));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URI) == 0) {
		Tcl_SetStringObj(res, uri, strlen(uri));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_COOKIE) == 0) {
		if (objc != 3) {
			goto ht_gp_error;
		}
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_HEADER) == 0) {
		if (objc != 3) {
			goto ht_gp_error;
		}
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_METHOD) == 0) {
		Tcl_SetStringObj(res, "GET", 3);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_QUERY) == 0) {
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}


	/*
	 * Tokens for network socket
	 */
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_IP) == 0) {
		char ip[16] = "192.168.1.1";
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_PORT) == 0) {
		char port[16] = "80";
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_IP) == 0) {
		char ip[16] = "192.168.1.2";
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_PORT) == 0) {
		char port[16] = "80";
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	
	/* URL Category */
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URL_CATEGORY) == 0) {
		char respcode[20] = "Internet Services";
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	/* GeoIP tokens */
	else if (strcasecmp(token, TOKEN_GEO_STATUS_CODE) == 0) {
		char respcode[16] = "200";
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_CONTINENT_CODE) == 0) {
		char respcode[16] = "NA";
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_COUNTRY_CODE) == 0) {
		char respcode[16] = "US";
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_COUNTRY) == 0) {
		char respcode[16] = "United States";
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_STATE) == 0) {
		char respcode[16] = "California";
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_CITY) == 0) {
		char respcode[16] = "Sunnyvale";
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_POSTAL_CODE) == 0) {
		char respcode[16] = "94089";
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if (strcasecmp(token, TOKEN_GEO_ISP) == 0) {
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else {
		goto ht_gp_error;
	}
	return TCL_OK;
	
ht_gp_error:
	printf( "Error: getparam %s\n", token );
	/* Not defined */
	return TCL_ERROR;
}

static int pec_http_recv_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;

	if (objc < 2) {
		goto ht_sa_error;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);

	/*
	 * Tokens for HTTP structure
	 */
	if(strcasecmp(token, ACTION_REJECT_REQUEST) == 0) {
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_URL_REWRITE) == 0) {
		if (objc != 4) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REDIRECT) == 0) {
		if (objc < 3) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_NO_CACHE_OBJECT) == 0) {
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_CACHE_OBJECT) == 0) {
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_SET_ORIGIN_SERVER) == 0) {
		if (objc != 3) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
#if 0
	else if(strcasecmp(token, ACTION_SET_CACHE_NAME) == 0) {
		if (objc != 3) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
#endif
	else if(strcasecmp(token, ACTION_INSERT_HEADER) == 0) {
		if (objc != 4) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REMOVE_HEADER) == 0) {
		if (objc != 3) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_MODIFY_HEADER) == 0) {
		if (objc != 4) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_CACHE_INDEX_APPEND) == 0) {
                if (objc != 3) {
                        goto ht_sa_error;
                }
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_TRANSMIT_RATE) == 0) {
		if (objc > 5 || objc < 3) {
			goto ht_sa_error;
                }
		return TCL_OK;
	}
	else {
		goto ht_sa_error;
	}
	return TCL_OK;

ht_sa_error:
	printf( "Error: setaction %s\n", token );
	return TCL_ERROR;
}

int pec_eval_http_recv_request(void)
{
	int ret;
	Tcl_Command tok;

	// Supported variables
	tok = Tcl_CreateObjCommand(g_interp, 
			"getparam", pec_http_recv_req_gettokens,
			(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

	// Supported actions
	tok = Tcl_CreateObjCommand(g_interp, 
			"setaction", pec_http_recv_setaction,
			(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

	ret = Tcl_Eval(g_interp, "pe_http_recv_request");
	if (ret != TCL_OK) {
		printf( "Error: pe_http_recv_request returns  %d\nMsg: %s\n", 
			ret, Tcl_GetStringResult(g_interp));
		return -1;
	}
	return 0;
}

static int pec_om_recv_resp_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;

	if (objc < 2) {
		goto om_gp_error;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);

	/*
	 * Tokens for Origin Server response object
	 */
	if(strcasecmp(token, TOKEN_OM_RES_STATUS_CODE) == 0) {
		char respcode[16] = "200";
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_OM_RES_HEADER) == 0) {
		if (objc != 3) {
			goto om_gp_error;
		}
		//header_name = Tcl_GetString(objv[2]);
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}

	/*
	 * Tokens for client request object
	 */
	else if(strcasecmp(token, TOKEN_HTTP_REQ_HOST) == 0) {
		Tcl_SetStringObj(res, host, strlen(host));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URI) == 0) {
		Tcl_SetStringObj(res, uri, strlen(uri));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_COOKIE) == 0) {
		char * cookie_name;
		if (objc != 3) {
			goto om_gp_error;
		}
		cookie_name = Tcl_GetString(objv[2]);
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_HEADER) == 0) {
		char * header_name;

		if (objc != 3) {
			goto om_gp_error;
		}
		header_name = Tcl_GetString(objv[2]);
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_METHOD) == 0) {
		Tcl_SetStringObj(res, "GET", 3);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_QUERY) == 0) {
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}

	/*
	 * Tokens for network socket
	 */
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_IP) == 0) {
		char ip[16] = "192.168.1.1";
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_PORT) == 0) {
		char port[16] = "80";
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_IP) == 0) {
		char ip[16] = "192.168.1.2";
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_PORT) == 0) {
		char port[16] = "80";
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else {
		/* Not defined */
		goto om_gp_error;
	}
	return TCL_OK;

om_gp_error:
	printf( "Error: getparam %s\n", token );
	return TCL_ERROR;
}

static int pec_om_recv_resp_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;

	if (objc < 2) {
		goto om_sa_error;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);

	/*
	 * Tokens for HTTP structure
	 */
	if(strcasecmp(token, ACTION_CACHE_OBJECT) == 0) {
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REDIRECT) == 0) {
		if (objc < 3) {
			goto om_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_NO_CACHE_OBJECT) == 0) {
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_INSERT_HEADER) == 0) {
		if (objc != 4) {
			goto om_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REMOVE_HEADER) == 0) {
		if (objc != 3) {
			goto om_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_MODIFY_HEADER) == 0) {
		if (objc != 4) {
			goto om_sa_error;
		}
		return TCL_OK;
	}
	else {
		goto om_sa_error;
	}
	return TCL_OK;

om_sa_error:
	printf( "Error: setaction %s\n", token );
	return TCL_ERROR;
}

int pec_eval_om_recv_response( void )
{
	int ret;
	Tcl_Command tok;

	// Supported variables
	tok = Tcl_CreateObjCommand(g_interp, 
			"getparam", pec_om_recv_resp_gettokens,
			(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

	// Supported actions
	tok = Tcl_CreateObjCommand(g_interp, 
			"setaction", pec_om_recv_resp_setaction,
			(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

	ret = Tcl_Eval(g_interp, "pe_om_recv_response");
	if (ret != TCL_OK) {
		printf( "Error: pe_om_recv_response returns %d\nMsg: %s\n", 
			ret, Tcl_GetStringResult(g_interp));
		return -1;
	}
	return 0;
}

static int pec_om_send_request_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;

	if (objc < 2) {
		goto om_gp_error;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);

	/*
	 * Tokens for client request object
	 */
	if(strcasecmp(token, TOKEN_HTTP_REQ_HOST) == 0) {
		Tcl_SetStringObj(res, host, strlen(host));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URI) == 0) {
		Tcl_SetStringObj(res, uri, strlen(uri));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_COOKIE) == 0) {
		if (objc != 3) {
			goto om_gp_error;
		}
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_HEADER) == 0) {
		if (objc != 3) {
			goto om_gp_error;
		}
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_METHOD) == 0) {
		Tcl_SetStringObj(res, "GET", 3);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_QUERY) == 0) {
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}

	/*
	 * Tokens for network socket
	 */
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_IP) == 0) {
		char ip[16] = "192.168.1.1";
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_SOURCE_PORT) == 0) {
		char port[16] = "80";
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_IP) == 0) {
		char ip[16] = "192.168.1.2";
		Tcl_SetStringObj(res, ip, strlen(ip));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_SOCKET_DEST_PORT) == 0) {
		char port[16] = "80";
		Tcl_SetStringObj(res, port, strlen(port));
		return TCL_OK;
	}
	else {
		/* Not defined */
		goto om_gp_error;
	}
	return TCL_OK;
	
om_gp_error:
	printf( "Error: getparam %s\n", token );
	return TCL_ERROR;
}

static int pec_om_send_request_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;

	if (objc < 2) {
		goto om_sa_error;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);

	/*
	 * Actions
	 */
	if(strcasecmp(token, ACTION_INSERT_HEADER) == 0) {
		if (objc != 4) {
			goto om_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REMOVE_HEADER) == 0) {
		if (objc != 3) {
			goto om_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_MODIFY_HEADER) == 0) {
		if (objc != 4) {
			goto om_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_SET_IP_TOS) == 0) {
		char * tos;

		if (objc != 3) {
			goto om_sa_error;
		}

		tos = Tcl_GetString(objv[2]);
		/* 
		 * tos byte PE configuration should like: 0b110011  configure 6 bits only
		 */
		if((strlen(tos) != 8) || (tos[0]!='0') || (tos[1]!='b')) {
			goto om_sa_error;
		}
		return TCL_OK;
	}
	else {
		goto om_sa_error;
	}
	return TCL_OK;

om_sa_error:
	printf( "Error: setaction %s\n", token );
	return TCL_ERROR;
}


int pec_eval_om_send_request( void )
{
	int ret;
	Tcl_Command tok;

	// Supported variables
	tok = Tcl_CreateObjCommand(g_interp, 
			"getparam", pec_om_send_request_gettokens,
			(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

	// Supported actions
	tok = Tcl_CreateObjCommand(g_interp, 
			"setaction", pec_om_send_request_setaction,
			(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

	ret = Tcl_Eval(g_interp, "pe_om_send_request");
	if (ret != TCL_OK) {
		printf( "Error: pe_om_send_request returns %d\nMsg: %s\n", 
			ret, Tcl_GetStringResult(g_interp));
		return -1;
	}
	return 0;
}

static int pec_http_send_resp_gettokens(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;

	if (objc < 2) {
		goto ht_gp_error;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);

	/*
	 * Tokens for response object
	 */
	if(strcasecmp(token, TOKEN_HTTP_RES_STATUS_CODE) == 0) {
		char respcode[16] = "200";
		Tcl_SetStringObj(res, respcode, strlen(respcode));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_RES_HEADER) == 0) {
		if (objc != 3) {
			goto ht_gp_error;
		}
		//header_name = Tcl_GetString(objv[2]);
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	
	/*
	 * Tokens for HTTP structure
	 */
	else if(strcasecmp(token, TOKEN_HTTP_REQ_HOST) == 0) {
		Tcl_SetStringObj(res, host, strlen(host));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_URI) == 0) {
		Tcl_SetStringObj(res, uri, strlen(uri));
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_COOKIE) == 0) {
		if (objc != 3) {
			goto ht_gp_error;
		}
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_HEADER) == 0) {
		if (objc != 3) {
			goto ht_gp_error;
		}
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_METHOD) == 0) {
		Tcl_SetStringObj(res, "GET", 3);
		return TCL_OK;
	}
	else if(strcasecmp(token, TOKEN_HTTP_REQ_QUERY) == 0) {
		Tcl_SetStringObj(res, "", 0);
		return TCL_OK;
	}
	else {
		goto ht_gp_error;
	}

ht_gp_error:
	printf( "Error: getparam %s\n", token );
	/* Not defined */
	return TCL_ERROR;
}

static int pec_http_send_resp_setaction(ClientData clientData, Tcl_Interp *interp, 
			int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj	*res;
	char * token;

	if (objc < 2) {
		goto ht_sa_error;
	}
	token = Tcl_GetString(objv[1]);

	res = Tcl_GetObjResult(interp);

	if(strcasecmp(token, ACTION_INSERT_HEADER) == 0) {
		if (objc != 4) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_REMOVE_HEADER) == 0) {
		if (objc != 3) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_MODIFY_HEADER) == 0) {
		if (objc != 4) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_MODIFY_RESPONSE_CODE) == 0) {
		if (objc != 3) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
	else if(strcasecmp(token, ACTION_SET_IP_TOS) == 0) {
		char * tos;

		if (objc != 3) {
			goto ht_sa_error;
		}

		tos = Tcl_GetString(objv[2]);
		/* 
		 * tos byte PE configuration should like: 0b110011  configure 6 bits only
		 */
		if((strlen(tos) != 8) || (tos[0]!='0') || (tos[1]!='b')) {
			goto ht_sa_error;
		}
		return TCL_OK;
	}
	else {
		goto ht_sa_error;
	}

ht_sa_error:
	printf( "Error: setaction %s\n", token );
	return TCL_ERROR;
}


int pec_eval_http_send_response(void)
{
	int ret;
	Tcl_Command tok;
	
	// Supported variables
	tok = Tcl_CreateObjCommand(g_interp, 
			"getparam", pec_http_send_resp_gettokens,
			(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

	// Supported actions
	tok = Tcl_CreateObjCommand(g_interp, 
			"setaction", pec_http_send_resp_setaction,
			(ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

	ret = Tcl_Eval(g_interp, "pe_http_send_response");
	if (ret != TCL_OK) {
		printf( "Error: pe_http_send_response returns %d\nMsg: %s\n", 
			ret, Tcl_GetStringResult(g_interp));
		return -1;
	}
	return 0;
}



