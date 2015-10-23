/*
 * Filename :   adns_mgmt.h
 * Date:        23 May 2011
 * Author:      Kiran Desai
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

#ifndef  __AGENTD_MGMT__H
#define  __AGENTD_MGMT__H

#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <nkn_mgmt_defs.h>
#include "md_client.h"
#include "mdc_wrapper.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "signal_utils.h"
#include "libevent_wrapper.h"
#include "random.h"
#include "dso.h"
#include "agentd.h"
#include "xml_utils.h"
#include "nkn_defs.h"

#define MAX_MGMT_NDS	6
#define MAX_TMP_BUF_LEN 128
#define MAX_IGNORE_ITEMS 5

/*Error code*/
#define  TRANS_LKUP_FAILED 1051

/* TRANSL_NDS_FLAGS*/
#define      POST_PROCESS           0x0000001
#define      SUBOP_DEL              0x0000010

/* SUBOP Flags */
#define    SUBTREE                  0x0000001
#define    INCLUDE_SELF             0x0000010
#define    NO_CONFIG		    0x0000100
#define    NO_STATE                 0x0001000

typedef char net_buf_t[100]; /* Used for storing network prefix and mask */

#define PREPEND_TRANS_STR "mfc-cluster*"
#define HTTP_INST_TRANS_STR  PREPEND_TRANS_STR"servicehttpinstance*"

#define PREPEND_DEL_TRANS_STR "deletemfc-cluster*"
#define HTTP_INST_DEL_TRANS_STR  PREPEND_DEL_TRANS_STR"servicehttpinstance*"

#define ND_TBL_SIZE sizeof(mfc_agentd_nd_trans_tbl)/sizeof(mfc_agentd_nd_trans_tbl_t)
#define CNTR_TBL_SIZE sizeof(mfc_agentd_cntr_tbl)/sizeof(mfc_agentd_cntr_tbl_t)

typedef struct translation_obj {
    char *xml_data;     // can we have xml_data also as doc pointer?
    char *cli_data;
} translation_obj_t;

typedef enum subop {
    SUBOP_DELETE = bsso_delete,
    SUBOP_MODIFY = bsso_modify,
    SUBOP_CREATE = bsso_create,
    SUBOP_RESET = bsso_reset
} subop_t;

typedef enum trans_entry_type {
    ND_ENTRY = 0 ,
    CMD_ENTRY,
    CUST_ENTRY,
    CNTR_ENTRY
} trans_entry_type_t;

typedef struct mfc_agentd_tm_binding {
    const char *name;
    const char *type;
    const char *val;
} mfc_agentd_tm_binding_t;

typedef int (*cust_func_t)(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);


typedef struct cust_obj {
    cust_func_t func;
    void *arg;
} cust_obj_t;

typedef struct mfc_agentd_tm_action_nd {
    const char *nd_name;
    int n_bindings;
    mfc_agentd_tm_binding_t binding[7];
} mfc_agentd_tm_action_nd_t;

typedef enum tm_subop {
    GET,
    ITERATE
} tm_subop_t;

typedef struct mfc_agentd_tm_query_rsp {
    char *name;
    char *type;
    char *value;
} mfc_agentd_tm_query_rsp_t;

typedef struct mfc_agentd_tm_query_obj {
	const char *node_name;
	tm_subop_t tm_subop;
	int flags;
} mfc_agentd_tm_query_obj_t;

typedef struct vp_item {
    const char *vp_name;
    const char *vp_type;
} vp_item_t;

typedef enum nd_dt_type{
	TYPE_STR = bt_string,
	TYPE_BOOL = bt_bool,
	TYPE_URI = bt_uri,
	TYPE_REGEX = bt_regex,
	TYPE_INT8 = bt_int8,
	TYPE_INT16 = bt_int16,
	TYPE_INT32 = bt_int32,
	TYPE_UINT8 = bt_uint8,
	TYPE_UINT16 = bt_uint16,
	TYPE_UINT32 = bt_uint32,
	TYPE_UINT64 = bt_uint64,
	TYPE_LNK = bt_link,
	TYPE_IPADDRV4 = bt_ipv4addr,
	TYPE_IPADDRV6 = bt_ipv6addr,
	TYPE_DATETIME_SEC = bt_datetime_sec,
	TYPE_HOSTNAME = bt_hostname,
	TYPE_INETADDRZ = bt_inetaddrz,

	TYPE_MAX
}nd_dt_type_t;

typedef enum nd_type {
	ND_NORMAL,
	ND_HARDCODED,
	ND_DYNAMIC
}nd_type_t;

typedef enum cntr_type {
	CNTR_STATIC,
	CNTR_DYNAMIC
}cntr_type_t;

typedef struct nd_fmt_str_mapping {
	int n_args;
	int arg_index[3];
}nd_fmt_str_mapping_t;

typedef struct value_map {
	const char *map;
	const char *val;
} value_map_t;

typedef enum swap_type {
    VAL = 0,
    ND
}swap_type_t;

typedef enum _arith_op {
    OP_NONE = 0,
    ADD,
    SUB,
    MULT,
    DIV
}arith_op_t;

typedef struct value_str_mapping {
    int n_vals;
    value_map_t val_map[5];
} val_str_mapping_t;

typedef struct nd_str_mapping {
    int n_vals;
    value_map_t val_map[5];
} nd_str_mapping_t;

/* Mapping of the argument index and value to be ignored 
   while handling the translation pattern */
typedef struct ign_idx_val {
    int arg_index;
    const char *val;
} ign_idx_val_t;

typedef struct ign_val_lst {
    int n_vals;
    ign_idx_val_t ign_val[MAX_IGNORE_ITEMS];
} ign_val_lst_t;

typedef struct mgmt_nd_data {
    const char *nd_name;
    nd_dt_type_t nd_dt_type;
    nd_type_t	nd_type;
    nd_fmt_str_mapping_t nd_fmt_str_mapping;
    val_str_mapping_t val_str_mapping;
    nd_str_mapping_t nd_str_mapping;
    ign_val_lst_t ign_val_mapping;
    int  val_index;
    const char *value;
    subop_t subop;
    arith_op_t op;
    int operand; 
}mgmt_nd_data_t;

typedef struct mgmt_nd_lst {
    int nnds;
    int flags; //Used to determine if post processing is needed
    mgmt_nd_data_t mgmt_nd_data[MAX_MGMT_NDS];
} mgmt_nd_lst_t;

typedef char* (*translator_func_t)(void *br_cmd,void *tm_cmd);

typedef struct tm_cmd_obj {
	char *cli_cmd;
	//char *arg_values[BROKER_CMD_MAX_VAL];// will not need this
	translator_func_t translator_func;  //Should come in handy for custom/complex scenarios
	char *misc_cmd;                     //Will be used when a default cmd/or a key needs to hit whenever
}tm_cmd_obj_t;




typedef union trans_item {
    mgmt_nd_lst_t    mgmt_nd_data_lst;
    tm_cmd_obj_t     tm_cmd;
    cust_obj_t       cust_obj;
}trans_item_t;

typedef struct trans_entry {
    trans_entry_type_t  trans_entry_type;
    trans_item_t	trans_item;
} trans_entry_t;

typedef struct mfc_agentd_nd_trans_tbl {
   const char *cmd;
   trans_entry_t      trans_entry;
   // mgmt_nd_lst_t mgmt_nd_data_lst;
}mfc_agentd_nd_trans_tbl_t;


typedef struct mfc_agentd_cntr_elem {
    char *nd_name;
    nd_fmt_str_mapping_t nd_fmt_str_mapping;
    cntr_type_t cntr_type;
} mfc_agentd_cntr_elem_t;

/* mFC agentd Counter TBl */
typedef struct mfc_agentd_cntr_tbl {
	const char* cntr_name;
	mfc_agentd_cntr_elem_t mfc_agentd_cntr_elem;
	/* Might have to extend this for more dynamic counters */
}mfc_agentd_cntr_tbl_t;

/*structure to hold the cntr that are preparred and that are to be looked upon*/
typedef struct mfc_agentd_cntr_item {
	char* cntr_name;
	char* nd_name;
	cntr_type_t cntr_type; //may not be needed as of now
	unsigned long int cntr_val;
}mfc_agentd_cntr_item_t;

/* Structure to hold data from the user */
typedef struct mfc_agentd_cntrs {
	char *name;
	cntr_type_t cntr_type;
	const char *args;

}mfc_agentd_cntrs_t;

/* Structure and enum definition used in Junos ODL header & source files */

typedef struct xml_tag_s {
    const char *xt_name;
    unsigned short xt_type;
    unsigned short xt_flags;
} xml_tag_t;

/* Values for xt_type */
#define XTT_UNKNOWN     0       /* Unknown type */
#define XTT_INTEGER     1       /* %d */
#define XTT_UNSIGNED    2       /* %u */
#define XTT_STRING      3       /* %s */
#define XTT_INT64       4       /* %qd */
#define XTT_UINT64      5       /* %qu */

/* Values for xt_flags: */
#define XTF_LEVEL       (1<<0)  /* <foo>...</foo> */
#define XTF_DATA        (1<<1)  /* <foo>value</foo> */
#define XTF_EMPTY       (1<<2)  /* <foo/> */
#define XTF_OUTPUT      (1<<3)  /* Make output if not xml_on_board */
#define XTF_NEWLINE     (1<<4)  /* Make newline if not xml_on_board */
#define XTF_COLON       (1<<5)  /* Make a ': ' after the tag/value */
#define XTF_ATTR        (1<<6)  /* foo is an attribute */

#define NULL_XML_TAG_ENTRY \
NULL, 0, 0
/***************************************************************************/
int agentd_register_cmds_array (mfc_agentd_nd_trans_tbl_t * );

int agentd_register_cmd (mfc_agentd_nd_trans_tbl_t  *);

int mfc_agentd_trans_tbl_init(void);

int
mfc_agentd_trans_tbl_destroy(void);

int
mfc_agentd_trans_tbl_get_trans_entry(char *cmd, trans_entry_t **trans_entry);

/* cnt TBL */
int mfc_agentd_cntr_tbl_init(void);
int mfc_agentd_cntr_tbl_get_cntr(char *cntr ,mfc_agentd_cntr_elem_t **cntr_elem);

#define TRANSL_ENTRY_NULL {.cmd = NULL}

#define TRANSL_ENTRY(br_cmd)     \
{                                \
    .cmd = br_cmd ,              \
    .trans_entry =  {

#define TRANSL_NUM_NDS(num)       \
.trans_entry_type = ND_ENTRY,    \
    .trans_item = {             \
    .mgmt_nd_data_lst = {        \
	.nnds = num,             \
	.flags = 0,              \
	.mgmt_nd_data = {

#define TRANSL_NUM_NDS_ADV(num, n_flags)       \
.trans_entry_type = ND_ENTRY,    \
    .trans_item = {             \
    .mgmt_nd_data_lst = {        \
	.nnds = num,             \
	.flags = n_flags,              \
	.mgmt_nd_data = {


#define RESET_TRANSL_NDS(node_name, node_dt_type, node_type, \
		     value_index,val, nargs,           \
		idx1,idx2,idx3)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
	.n_args = nargs,                               \
	.arg_index[0]= idx1,                           \
	.arg_index[1]= idx2,                           \
	.arg_index[2]= idx3,                           \
    },                                                  \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = SUBOP_RESET                               \
},
#define DEL_TRANSL_NDS(node_name, node_dt_type, node_type, \
		     value_index,val, nargs,           \
		idx1,idx2,idx3)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
	.n_args = nargs,                               \
	.arg_index[0]= idx1,                           \
	.arg_index[1]= idx2,                           \
	.arg_index[2]= idx3,                           \
    },                                                  \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = SUBOP_DELETE                              \
},

#define TRANSL_NDS(node_name, node_dt_type, node_type, \
		     value_index,val, nargs,           \
		idx1,idx2,idx3)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
	.n_args = nargs,                               \
	.arg_index[0]= idx1,                           \
	.arg_index[1]= idx2,                           \
	.arg_index[2]= idx3,                           \
    },                                                  \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = 0                                         \
},



#define RESET_TRANSL_NDS_BASIC(node_name, node_dt_type, node_type, \
		     value_index,val)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
	.n_args = 1,                                   \
	.arg_index[0]= 0,                              \
	.arg_index[1]= 0,                              \
	.arg_index[2]= 0,                              \
    },                                                  \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = SUBOP_RESET				\
},

#define DEL_TRANSL_NDS_BASIC(node_name, node_dt_type, node_type, \
		     value_index,val)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
	.n_args = 1,                                   \
	.arg_index[0]= 0,                              \
	.arg_index[1]= 0,                              \
	.arg_index[2]= 0,                              \
    },                                                  \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = SUBOP_DELETE                              \
},

#define TRANSL_NDS_BASIC(node_name, node_dt_type, node_type, \
		     value_index,val)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
	.n_args = 1,                                   \
	.arg_index[0]= 0,                              \
	.arg_index[1]= 0,                              \
	.arg_index[2]= 0,                              \
    },                                                  \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = 0                                         \
},

#define TRANSL_NDS_EXT_VAL(node_name, node_dt_type, node_type, \
		     value_index,val)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
	.n_args = 1,                                   \
	.arg_index[0]= 0,                              \
	.arg_index[1]= 0,                              \
	.arg_index[2]= 0,                              \
    },                                                 \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = 0,

#define TRANSL_NDS_EXT_VAL_ADV(node_name, node_dt_type, node_type, \
                     value_index,val,nargs, \
			idx1,idx2,idx3)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,                         \
   .nd_type =   node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
        .n_args = nargs,                                   \
        .arg_index[0]= idx1,                              \
        .arg_index[1]= idx2,                              \
        .arg_index[2]= idx3,                              \
    },                                                 \
    .nd_str_mapping = {                                \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = 0,


#define TRANSL_NDS_VAL_MAP_START(num_val)                   \
.val_str_mapping = {                                  \
    .n_vals = num_val,

#define TRANSL_NDS_VAL_MAP(idx,br_val,tm_val)              \
    .val_map[idx] = {br_val,tm_val},

#define TRANSL_NDS_VAL_MAP_END                 }  }


#define TRANSL_NDS_EXT_ND(node_name, node_dt_type, node_type, \
		     value_index,val)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
	.n_args = 1,                                   \
	.arg_index[0]= 0,                              \
	.arg_index[1]= 0,                              \
	.arg_index[2]= 0,                              \
    },                                                 \
    .val_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = 0,

#define TRANSL_NDS_ND_MAP_START(num_nds)                   \
.nd_str_mapping = {                                  \
    .n_vals = num_nds,

#define TRANSL_NDS_ND_MAP(idx,br_val,tm_nd)              \
    .val_map[idx] = {br_val,tm_nd},

#define TRANSL_NDS_ND_MAP_END                 }  }


#define TRANSL_NDS_ADD_VAL(node_name, node_dt_type, node_type, \
		     value_index,val,op_value)                 \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = ADD,                                         \
    .operand = op_value,                               \
    .nd_fmt_str_mapping = {                                \
	.n_args = 1,                                   \
	.arg_index[0]= 0,                              \
	.arg_index[1]= 0,                              \
	.arg_index[2]= 0,                              \
    },                                                  \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = 0                                         \
},

#define TRANSL_NDS_SUB_VAL(node_name, node_dt_type, node_type, \
		     value_index,val,op_value)                 \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = SUB,                                         \
    .operand = op_value,                               \
    .nd_fmt_str_mapping = {                                \
	.n_args = 1,                                   \
	.arg_index[0]= 0,                              \
	.arg_index[1]= 0,                              \
	.arg_index[2]= 0,                              \
    },                                                  \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = 0                                         \
},

#define TRANSL_NDS_MULT_VAL(node_name, node_dt_type, node_type, \
		     value_index,val,op_value)                 \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = MULT,                                        \
    .operand = op_value,                               \
    .nd_fmt_str_mapping = {                                \
	.n_args = 1,                                   \
	.arg_index[0]= 0,                              \
	.arg_index[1]= 0,                              \
	.arg_index[2]= 0,                              \
    },                                                  \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = 0                                         \
},

#define TRANSL_NDS_DIV_VAL(node_name, node_dt_type, node_type, \
		     value_index,val,op_value)                 \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = DIV,                                         \
    .operand = op_value,                               \
    .nd_fmt_str_mapping = {                                \
	.n_args = 1,                                   \
	.arg_index[0]= 0,                              \
	.arg_index[1]= 0,                              \
	.arg_index[2]= 0,                              \
    },                                                  \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                 \
    .ign_val_mapping = {                                \
	.n_vals = 0,                                   \
	.ign_val[0] = {0,0},                           \
	.ign_val[1] = {0,0},                           \
	.ign_val[2] = {0,0},                           \
	.ign_val[3] = {0,0},                           \
	.ign_val[4] = {0,0},                           \
    },                                                 \
    .subop = 0                                         \
},

#define TRANSL_NDS_IGN_VAL(node_name, node_dt_type, node_type, \
                     value_index,val)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,                         \
   .nd_type =   node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
        .n_args = 1,                                   \
        .arg_index[0]= 0,                              \
        .arg_index[1]= 0,                              \
        .arg_index[2]= 0,                              \
    },                                                 \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                 \
    .subop = 0,

#define RESET_TRANSL_NDS_IGN_VAL(node_name, node_dt_type, node_type, \
                     value_index,val)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,                         \
   .nd_type =   node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
        .n_args = 1,                                   \
        .arg_index[0]= 0,                              \
        .arg_index[1]= 0,                              \
        .arg_index[2]= 0,                              \
    },                                                 \
    .val_str_mapping = {                               \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                 \
    .subop = SUBOP_RESET,

#define DEL_TRANSL_NDS_IGN_VAL(node_name, node_dt_type, node_type, \
                     value_index,val)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,                         \
   .nd_type =   node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
        .n_args = 1,                                   \
        .arg_index[0]= 0,                              \
        .arg_index[1]= 0,                              \
        .arg_index[2]= 0,                              \
    },                                                 \
    .val_str_mapping = {                               \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                 \
    .subop = SUBOP_DELETE,

#define TRANSL_NDS_IGN_VAL_ADV(node_name, node_dt_type, node_type, \
                     value_index,val,nargs, \
                        idx1,idx2,idx3)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,                         \
   .nd_type =   node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
        .n_args = nargs,                                   \
        .arg_index[0]= idx1,                              \
        .arg_index[1]= idx2,                              \
        .arg_index[2]= idx3,                              \
    },                                                 \
    .val_str_mapping = {                               \
	.n_vals = 0,                                   \
	.val_map[0] = {0,0},                           \
	.val_map[1] = {0,0},                           \
	.val_map[2] = {0,0},                           \
	.val_map[3] = {0,0},                           \
	.val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                 \
    .subop = 0,

#define RESET_TRANSL_NDS_IGN_VAL_ADV(node_name, node_dt_type, node_type, \
                     value_index,val,nargs, \
                        idx1,idx2,idx3)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,                         \
   .nd_type =   node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
        .n_args = nargs,                                   \
        .arg_index[0]= idx1,                              \
        .arg_index[1]= idx2,                              \
        .arg_index[2]= idx3,                              \
    },                                                 \
    .val_str_mapping = {                               \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                 \
    .subop = SUBOP_RESET,

#define DEL_TRANSL_NDS_IGN_VAL_ADV(node_name, node_dt_type, node_type, \
                     value_index,val,nargs, \
                        idx1,idx2,idx3)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,                         \
   .nd_type =   node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
    .op = OP_NONE,                                     \
    .operand = 0,                                      \
    .nd_fmt_str_mapping = {                                \
        .n_args = nargs,                                   \
        .arg_index[0]= idx1,                              \
        .arg_index[1]= idx2,                              \
        .arg_index[2]= idx3,                              \
    },                                                 \
    .val_str_mapping = {                               \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                  \
    .nd_str_mapping = {                                \
        .n_vals = 0,                                   \
        .val_map[0] = {0,0},                           \
        .val_map[1] = {0,0},                           \
        .val_map[2] = {0,0},                           \
        .val_map[3] = {0,0},                           \
        .val_map[4] = {0,0},                           \
    },                                                 \
    .subop = SUBOP_DELETE,

#define TRANSL_NDS_IGN_MAP_START(num_val)                   \
.ign_val_mapping = {                                  \
    .n_vals = num_val,

#define TRANSL_NDS_IGN_MAP(idx,br_val,tm_val)              \
    .ign_val[idx] = {br_val,tm_val},

#define TRANSL_NDS_IGN_MAP_END                 }  }

#define TRANSL_CMD(tm_cmds)                            \
    .trans_entry_type = CMD_ENTRY,		       \
    .trans_item = {                                    \
	.tm_cmd = {                                    \
	.cli_cmd = tm_cmds,                            \
	.translator_func = 0,                          \
	.misc_cmd = 0,


#define TRANSL_CUST(callback, data)                            \
    .trans_entry_type = CUST_ENTRY,		       \
    .trans_item = {                                    \
	.cust_obj = {                                    \
	.func = callback,                            \
	.arg = (void *)data,


#define TRANSL_END_NDS     } } }

#define TRANSL_END_CMDS } }

#define TRANSL_END_CUST } }

#define TRANSL_ENTRY_END  } }  ,



///////////////////////////////////////////////////////////////////////////////
extern int  exit_req;
///////////////////////////////////////////////////////////////////////////////


int agentd_mgmt_handle_event_request(gcl_session *session,
                              bn_request  **inout_request,
                              void        *arg);
int agentd_config_handle_set_request(
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);
int agentd_module_cfg_handle_change(
        bn_binding_array *old_bindings, bn_binding_array *new_bindings);

int agentd_cfg_handle_change(
        const bn_binding_array *arr,
        uint32 idx,
        bn_binding *binding,
         void *data);

int    agentd_deinit(void);


int agentd_mgmt_init(agentd_context_t *context);
int    agentd_mgmt_initiate_exit(void);




int agentd_handle_cfg_request(agentd_context_t *context, agentd_binding_array *change_list);
int agentd_send_request_to_mgmtd(bn_request *req_msg, trans_entry_t *node_details);

int set_ret_msg(agentd_context_t *context, const char *message);
int set_ret_code(agentd_context_t *context, int ret_code);
extern int agentd_fill_request_msg(agentd_context_t  *context,
	agentd_binding *cmd_obj, mgmt_nd_lst_t *mgmt_nd_list, int *ret_code);

int agentd_process_oper_request(agentd_context_t *context, int *ret_code);

int
ns_prestage_ftp_password(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);
int
ns_get_server_map (const char *ns_name, void *arg,
	agentd_context_t *context, mfc_agentd_tm_query_rsp_t **query_resp, int *num_nodes);

int
namespace_delete(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_del_proto_origin_req_add_hdr(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_del_proto_client_rsp_add_hdr(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_del_proto_cache_age_content_type(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int ns_del_proto_rm_header(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);
int
ns_del_proto_cache_age_rm_content_type(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
agentd_interface_ip_addr(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int
agentd_del_alias(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int
agentd_configure_interface(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int
agentd_delete_interface(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int
agentd_bond_interface_params(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int
agentd_delete_bonded_interface(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int
agentd_delete_bond_interface_params(agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

//delete_ns_del_proto_cache_age_content_type(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_om_instance(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_om_node_mon_read_timeout(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);
int
ns_om_node_mon_connect_timeout(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_om_node_mon_hb_interval(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_om_node_mon_fails(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_om_hash_input(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_om_member(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_om_instance_dummy(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_om_delete_all(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_org_http_ser_map(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_del_proto_client_rsp_del_hdr(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
ns_del_proto_client_cache_age_del_hdr(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
mfc_agentd_del_all_objs(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);

int
mfc_agentd_del_all_var_objs(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);


int agentd_custom_entry_ignore(agentd_context_t *context, agentd_binding *binding, void *cb_arg, char **cli_buff);
int
agentd_append_req_msg(agentd_context_t *context,const char *name,
	subop_t subop, nd_dt_type_t type, const char *value);

int
agentd_handle_querystr_cache_reset (agentd_context_t *context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int
agentd_ns_cache_index_header_add (agentd_context_t * context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

int
agentd_ns_cache_index_header_delete(agentd_context_t * context, agentd_binding *br_cmd, void *cb_arg, char **cli_buff);

typedef struct {
    const char	*child_name;
    const char *binding_name;
    uint32_array *indices;
    uint32 node_idx;
} ns_header_match_context;

int
agentd_ns_mdc_array_get_matching_indices(md_client_context *mcc,
	const char *root_name,
	const char *db_name,
	const char *bn_name,
	const char *value,
	uint32 node_index,
	int32 rev_before,
	int32 *ret_rev_after,
	uint32_array **ret_indices,
	uint32 *ret_code,
	tstring **ret_msg);

int
agentd_ns_get_last_index(const char *root_name, const char *db_name,
	const char *value, uint32 *lst_idx,  uint32 *ret_code, tstring **ret_msg);

int agentd_set_ns_domain_name (agentd_context_t *context,
        agentd_binding *abinding, void *cb_arg, char **cli_buff);

int agentd_namespace_delete(agentd_context_t *context, 
	agentd_binding *abinding, void *cb_arg, char **cli_buff);

int agentd_set_ipv6_addr (agentd_context_t *context,
	agentd_binding *abinding, void *cb_arg, char **cli_buff);

int agentd_del_ipv6_addr (agentd_context_t *context,
	agentd_binding *abinding, void *cb_arg, char **cli_buff);

/* This function converts JUNOS date time format to TM cli date time format */
int agentd_convert_junos_time_fmt (const char *junos_time, str_value_t * out_tm_date_time);

/* This function outputs the value of a binding in tstring format for the given binding node from the given prequeried binding array */
/* Note: Memory allocated for 'target_tstr' should be freed by the calling function */
int agentd_binding_array_get_value_tstr_fmt (const bn_binding_array *binding_arr,
          tstring ** target_tstr, const char *format, ...) __attribute__((format(printf,3,4)));

/* This function outputs the uint8 value of binding given the binding node and the prequeried binding array */
int agentd_binding_array_get_value_uint8_fmt (const bn_binding_array *binding_arr,
          uint8* target_val, const char *format, ...) __attribute__((format(printf,3,4)));

int agentd_binding_array_get_value_uint32_fmt (const bn_binding_array *binding_arr,
          uint32* target_val, const char *format, ...) __attribute__((format(printf,3,4)));

/* This function outputs the uint8 value of a binding given the binding node */
int agentd_mdc_get_value_uint8_fmt (uint8 *target_val, const char *format, ...)
                  __attribute__((format(printf,2,3)));

/* This function outputs the tstr value of a binding given the binding node */
int agentd_mdc_get_value_tstr_fmt(tstring **target_val, const char *format,...)
                __attribute__((format(printf,2,3)));;


/******************************************************************************
 ** Typedefs
 *****************************************************************************/

typedef enum {
    mdm_by_children,
    mdm_by_index
} agentd_delete_method;

typedef struct agentd_array_delete_context {
    agentd_delete_method madc_method;
    const bn_binding_array *madc_match_criteria;
    const uint32_array *madc_match_indices;
    tbool madc_shift_to_cover;
    bn_request *madc_request;
    uint32 madc_num_deleted;
    uint32 madc_lst_del_idx;
} agentd_array_delete_context;

typedef struct agentd_array_move_context {
    uint32 mamc_start_index;
    uint32 mamc_end_index;
    uint32 mamc_last_index;
    bn_request *mamc_request;
} agentd_array_move_context;

typedef struct agentd_array_match_context {
    const bn_binding_array *mamc_match_criteria;
    uint32_array *mamc_matched_indices;
} agentd_array_match_context;

typedef uint32 agentd_array_flags_bf;

/******************************************************************************
 ** Prototypes
 *****************************************************************************/

int
agentd_array_cleanup(void);
int agentd_array_append(md_client_context *context, agentd_array_move_context *ctxt, bn_binding_array * barr,
	const char *root_name, const bn_binding_array *children,
	uint32 *ret_code, tstring **ret_msg);

int agentd_array_set(md_client_context *context, agentd_array_move_context *ctxt, bn_binding_array * barr,
	const char *root_name, const bn_binding_array *children,
	uint32 *ret_code, tstring **ret_msg);


int agentd_xml_node_exists(agentd_context_t *context, const char *pattern, int *ispresent);

#endif /*__AGENTD_MGMT__H*/

