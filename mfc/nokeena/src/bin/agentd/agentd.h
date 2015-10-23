#ifndef _AGENTD_H_
#define _AGENTD_H_

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>

#include <libxml/parser.h>
#include "common.h"
#include "bnode.h"
#include "typed_array_tmpl.h"
#include "file_utils.h"

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <cprops/hashtable.h>

#define AGENTD_MAX_POST_CMDS	1024
#define AGENTD_MAX_PRE_CMDS	1024
#define AGENTD_MAX_POST_HANDLERS 1024

/* max number of dist-id supported */
#define NKN_MAX_DISTRIB_ID  256

/* maximum length of dist-id */
#define MAX_DISTRIB_ID_LEN  256
/* Structure definition for messages between agentd and agentd_cgi */

typedef struct _agentd_cgi_hdr {
    unsigned long data_len;
} agentd_cgi_hdr;

typedef struct _agentd_cgi_message  {
    agentd_cgi_hdr hdr;
    char mfc_data[0];
} agentd_cgi_message;
/* -------------------------------------------------------------- */

typedef char agentd_value_t[1024]; /* Increased from 256 as SSHv2 auth-keys can be longer than 256 characters */

enum agentd_cfg_change_type {
    agentd_cfg_addition = 0,
    agentd_cfg_deletion,
    agentd_cfg_modification
};

enum agentd_req_type {
    agentd_req_none = 0,
    agentd_req_config,
    agentd_req_get,
    agentd_req_stats,
    agentd_req_operational,
    agentd_req_rest_post,
    agentd_req_rest_put,
    agentd_req_rest_delete,
    agentd_req_rest_get,
};

typedef enum agentd_req_type agentd_req_type;

#define AGENTD_CFG_CHANGE_ADDITION	( 1 << agentd_cfg_addition )
#define AGENTD_CFG_CHANGE_DELETION	( 1 << agentd_cfg_deletion )
#define AGENTD_CFG_CHANGE_MODIFICATION	( 1 << agentd_cfg_modification )


#define AGENTD_STR_MFC_REQUEST	    "mfc-request"
#define AGENTD_STR_MFC_RESPONSE	    "mfc-response"
#define AGENTD_STR_HEADER	    "header"
#define AGENTD_STR_DATA		    "data"
#define AGENTD_STR_STATUS           "status"
#define AGENTD_STR_CODE             "code"
#define AGENTD_STR_MESSAGE          "message"
#define AGENTD_STR_TYPE		    "type"
#define AGENTD_STR_DISTRIB_ID	    "distribution-id"

#define AGENTD_STR_REQ_TYPE_NONE	"NONE"
#define AGENTD_STR_REQ_TYPE_GET		"GET"
#define AGENTD_STR_REQ_TYPE_OPERATIONAL "OPERATIONAL"
#define AGENTD_STR_REQ_TYPE_CONFIG	"CONFIG"
#define AGENTD_STR_REQ_TYPE_STATS	"STATS"
#define AGENTD_STR_REQ_TYPE_REST_POST	"ADD"
#define AGENTD_STR_REQ_TYPE_REST_PUT	"MODIFY"
#define AGENTD_STR_REQ_TYPE_REST_DELETE	"DELETE"
#define AGENTD_STR_REQ_TYPE_REST_GET	"RGET"

struct arg_values {
    agentd_value_t val_args[8];
    uint16_t nargs;
};
/* define agentd binding */
struct agentd_binding {
    bn_binding *binding;
    char pattern[1024];
    struct arg_values arg_vals;
    uint32_t flags;
};

typedef struct agentd_binding agentd_binding;

TYPED_ARRAY_HEADER_NEW_NONE(agentd_binding, struct agentd_binding *);

typedef struct _agentd_resp_data {
    xmlNodePtr data; /* base data node pointer */
    xmlNodePtr pnode;/* parent node pointer */
} agentd_resp_data;

/* The following structure is for mapping JUNOS strings to Tall Maple strings */
typedef struct agentd_string_map_s {
    const char *junos_str;
    const char *tm_str;
} agentd_string_map;

/* following enum is for agent flags */
/* These flags are used in pre-commit handling */
enum agent_flag
{
    af_regen_smaps_xml = 0, af_delete_smap, af_delete_smap_members, af_set_auth_order,
};

#define AF_REGEN_SMAPS_XML      ( 1 << af_regen_smaps_xml )
#define AF_DELETE_SMAP          ( 1 << af_delete_smap )
#define AF_DELETE_SMAP_MEMBERS  ( 1 << af_delete_smap_members )
#define AF_SET_AUTH_ORDER       ( 1 << af_set_auth_order )

/* Prototype for post-commit handler call back function */
typedef int (*binding_hdlr)(void * agentd_context, agentd_binding *binding, void *cb_arg);

typedef struct _agentd_config_detail {
    agentd_binding_array *running_config;
    char dist_id[MAX_DISTRIB_ID_LEN];
} agentd_config_detail;

/* Structure to hold details for post-commit handling, if any */
typedef struct _agentd_binding_hdlr {
    agentd_binding * abinding;
    void * cb_arg;       /* argument for call back function */
    binding_hdlr cb_func; /* post-commit call back function */
} agentd_binding_hdlr;

typedef struct agentd_context {
    int uds_server_fd;
    /* following data is related parsing, config diff */
    agentd_binding_array *running_config;
    agentd_binding_array *new_config;
    agentd_binding_array *config_diff;
    agentd_req_type req_type;
    /* message return code and return message */
    char ret_msg[512];
    int ret_code;
    /* operation commands and their response */
    char oper_cmd[1024];
    /* request and response structure */
    tstr_array *op_cmd_parts;
    bn_binding_array *reset_bindings;
    bn_binding_array *mod_bindings;
    bn_binding_array *del_bindings;
    /* pre-commands */
    agentd_binding *pre_cmds[AGENTD_MAX_PRE_CMDS];
    int num_pre_cmds;
    /* post commands */
    agentd_binding *post_cmds[AGENTD_MAX_POST_CMDS];
    int num_post_cmds;
    agentd_binding *handle_post_cmds[AGENTD_MAX_POST_CMDS];
    int num_handle_post_cmds;
    /* post-commit handlers  - take care of sending action requests, 
    system operations etc., after a configuration change */
    int num_post_commit_hndlrs;
    agentd_binding_hdlr post_commit_hndlr[AGENTD_MAX_POST_HANDLERS];
    uint32 conf_flags;
    xmlDocPtr runningConfigDoc;
    xmlXPathContextPtr xpathCtx;
    agentd_resp_data resp_data; /* contains mfc response "data" */
    /* check if rest-api is enabled */
    int rapi_enabled;
    char distrib_id[MAX_DISTRIB_ID_LEN];
    /* hash table for distrib-id config files */
    cp_hashtable *hash_table;
    int request_rejected;
} agentd_context_t;

#define AGENTD_UDS_ROOT "/vtmp"
#define AGENTD_UDS_PATH "/vtmp/ingester"
#define AGENTD_UDS_MASK 01777
#define AGENTD_SOCKET "/vtmp/ingester/uds-agentd-server"
#define UNIX_PATH_MAX 256
/* MFC currently has 4 login methods - local/radius/tacacs+/ldap */
#define MAX_MFC_AUTH_METHODS 4

int
agentd_convert_cfg(xmlNode * a_node, const char *base_name, const char *pattern,
    struct arg_values *arg_vals, int nargs, int ident_add,
    agentd_binding_array *array);

int agentd_binding_dump(int log_level, const char *prefix, const agentd_binding *binding);

int agentd_log_timestamp (int req_type, const char * msg);

int agentd_mgmt_save_config(agentd_context_t *context);


int agentd_save_pre_command(agentd_context_t *context,
	agentd_binding *abinding);

int agentd_save_post_handler (agentd_context_t * context,
    agentd_binding *abinding, void *cb_arg, int cb_arg_len, binding_hdlr cb_func);

const char * agentd_util_get_tm_str (agentd_string_map *str_map, const char * junos_str);

int
agentd_configure_login_method (agentd_context_t * context);

/************** Prototype of functions used for constructing XML response to mfcd ***************/
/* Creates the response document and sets the root element */
xmlDocPtr agentd_mfc_resp_create (agentd_context_t * context);

/* Adds "data" element to the response doc */
int agentd_mfc_resp_set_data (agentd_context_t * context, xmlDocPtr mfcRespDoc);

int agentd_mfc_resp_open_container (agentd_context_t * context, unsigned int node_name);
int agentd_mfc_resp_close_container (agentd_context_t * context, unsigned int node_name);

/* Adds sub-elements to "data" element */
int agentd_mfc_resp_add_element (agentd_context_t * context, unsigned int node_name,
                                        const char * node_content);

/***********************************************************************************************/
#endif /*_AGENTD_H_*/
