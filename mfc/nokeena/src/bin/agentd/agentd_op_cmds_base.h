#ifndef AGENTD_OP_CMD_BASE_H
#define  AGENTD_OP_CMD_BASE_H

#include "juniper-media-flow-controller_odl.h"

#define POS_CLUSTER_NAME 2
#define POS_SERVICE_NAME 4
#define POS_RESTART_SERVICE_NAME 4
#define POS_STOP_SERVICE_NAME 4

#define POS_MFC_CLUSTER_NAME 2
#define POS_MFC_MEMBER_NODE_NAME (POS_MFC_CLUSTER_NAME+2 )

/* Operation command table constants and structure definitions */

#define MAX_OP_TBL_ENTRIES 500
#define MAX_PARAMS	8
#define MAX_XML_TAGS	5
#define DUMMY_WC_ENTRY  { {0,0,0,0,0,0,0,0} }
#define OP_ENTRY_NULL  { .func = NULL }

/* schema version of MFC, used by MFA.
 * this version should be updated when ever there a config change
 * or RPC changes */
#define AGENTD_SCHEMA_VERSION	    "3"
#define AGENTD_MIN_SCHEMA_VERSION   "3"

typedef int (*opercmd_execution_callback)(agentd_context_t *context, void *data);

/*Max of 8 wc position can be set
 * A value of 0 means no more wildcard entry is present
 * if command is namespace * counters, then the array index 1 has wildcard
 * so we can take the value here itself.
 */
typedef struct wc_position {
    int index[MAX_PARAMS];
}wc_position_t;

typedef struct oper_table_entry {
    char pattern[1024];
    wc_position_t wc_loc;
    opercmd_execution_callback func;
    void *callback_data;
}oper_table_entry_t;

/*
 *  XML output macros
 *
 *  OP_EMIT_TAG_START(context, tag)is used to write a start tag to the output <tag>
 *
 *  OP_EMIT_TAG_END(context, tag)is used to write a end tag to the output </tag>
 *  OP_EMIT_TAG_VALUE(context, tag,val) is used to output the tagged value to the output <tag> val </tag>
 *
 */

#define OP_EMIT_TAG_START(__context, __tag)                     \
    err = agentd_mfc_resp_open_container (__context, __tag); \
    if (err) { \
        set_ret_code (__context, 1); \
        set_ret_msg (__context, "Internal error occurred while constructing response.Please check MFC logs"); \
        bail_error (err); \
    }

#define OP_EMIT_TAG_END(__context, __tag)   \
    err = agentd_mfc_resp_close_container (__context, __tag); \
    if (err) { \
        set_ret_code (__context, 1); \
        set_ret_msg (__context, "Internal error occurred while constructing response.Please check MFC logs"); \
        bail_error (err); \
    }

#define OP_EMIT_TAG_VALUE(__context, __tag,__value)   \
    err = agentd_mfc_resp_add_element (__context, __tag, __value); \
    if (err) { \
        set_ret_code (__context, 1); \
        set_ret_msg (__context, "Internal error occurred while constructing response.Please check MFC logs"); \
        bail_error (err); \
    }

int agentd_register_op_cmds_array (oper_table_entry_t * op_arr);

int agentd_register_op_cmd (oper_table_entry_t* op_entry);

int agentd_update_op_table_size (void);

int agentd_dummy_testing(int);

/* Prototype of operational command callback functions */
int agentd_op_req_revalidate_contents (agentd_context_t *context, void *data);
int agentd_op_req_restart_process (agentd_context_t *context, void *data);
int agentd_op_req_stop_process (agentd_context_t *context, void *data);


int get_system_version      (agentd_context_t *context, void *data);
int get_namespace_counters  (agentd_context_t *context, void *data);
int get_interface_details   (agentd_context_t *context, void *data);

int agentd_op_req_restart_nvsd  (agentd_context_t *context, void *data);

int agentd_op_req_purge_contents    (agentd_context_t *context, void *data);
int agentd_show_interface_details   (agentd_context_t *context, void *data);
int agentd_show_bond_details        (agentd_context_t *context, void *data);

int agentd_show_service_info (agentd_context_t *context, void *data);
int agentd_show_running_config (agentd_context_t *context, void *data);
int agentd_show_config_version(agentd_context_t *context, void *data);
int agentd_show_schema_version(agentd_context_t *context, void *data);

#endif  /*AGENTD_OP_CMD_BASE_H */
