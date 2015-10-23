#ifndef MFC_AGENTD_TRANSLATION_TABLE_H
#define MFC_AGENTD_TRANSLATION_TABLE_H

//#include <h/hashtable.h>
//#include <h/hashtable_itr.h>
#include <string.h>
#include "bnode.h"
//#include <mfcd_config.h>
//#include "mfc_agent.h"



#define MAX_MGMT_NDS	5


/* TRANSL_NDS_FLAGS*/
#define      POST_PROCESS           0x0000001
#define      SUBOP_DEL              0x0000010

/* SUBOP Flags */
#define    SUBTREE                  0x0000001
#define    INCLUDE_SELF             0x0000010
#define    NO_CONFIG		    0x0000100
#define    NO_STATE                 0x0001000

#define ND_TBL_SIZE sizeof(mfc_agentd_nd_trans_tbl)/sizeof(mfc_agentd_nd_trans_tbl_t)
#define CNTR_TBL_SIZE sizeof(mfc_agentd_cntr_tbl)/sizeof(mfc_agentd_cntr_tbl_t)
typedef struct translation_obj {
    char *xml_data;     // can we have xml_data also as doc pointer?
    char *cli_data;
    //      xmlDocPtr *xg_cust_data;
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
#if 1
typedef int (*cust_func_t)(agentd_binding *br_cmd, void *cb_arg,
	agentd_context_t *context, char **cli_buff);


typedef struct cust_obj {
    cust_func_t func;
    void *arg;
} cust_obj_t;
#endif

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


typedef struct value_str_mapping {
    int n_vals;
    value_map_t val_map[5];
} val_str_mapping_t;

typedef struct nd_str_mapping {
    int n_vals;
    value_map_t val_map[5];
} nd_str_mapping_t;

typedef struct mgmt_nd_data {
    const char *nd_name;
    nd_dt_type_t nd_dt_type;
    nd_type_t	nd_type;
    nd_fmt_str_mapping_t nd_fmt_str_mapping;
    val_str_mapping_t val_str_mapping;
    nd_str_mapping_t nd_str_mapping;
    int  val_index;
    const char *value;
    subop_t subop;
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




int mfc_agentd_trans_tbl_init(void);

int
mfc_agentd_trans_tbl_destroy(void);

int 
mfc_agentd_trans_tbl_get_trans_entry(char *cmd, trans_entry_t **trans_entry);

/* cnt TBL */
int mfc_agentd_cntr_tbl_init(void);
int mfc_agentd_cntr_tbl_get_cntr(char *cntr ,mfc_agentd_cntr_elem_t **cntr_elem);
#if 0
int mfc_agentd_form_cust_cmd(cmd_obj_t *br_cmd_obj,
			    cust_obj_t *cust_obj,
			    agentd_context_t *context, char **cli_buff);
//int mfc_agentd_translate(broker_cmd_obj_t *br_cmd, tm_cmd_obj_t *tm_cmd);
int mfc_agentd_form_TM_cmd(cmd_obj_t *br_cd, tm_cmd_obj_t *tm_cmd,
	char **ret_buffer);
#endif


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


#define TRANSL_NDS(node_name, node_dt_type, node_type, \
		     value_index,val, nargs,           \
		idx1,idx2,idx3)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
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
    .subop = 0                                         \
},



#define TRANSL_NDS_BASIC(node_name, node_dt_type, node_type, \
		     value_index,val)                        \
{                                                      \
  .nd_name = node_name,                                \
   .nd_dt_type = node_dt_type,			       \
   .nd_type =	node_type,                             \
    .val_index = value_index,                          \
    .value = val,                                      \
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
    .subop = 0,

#define TRANSL_NDS_ND_MAP_START(num_nds)                   \
.nd_str_mapping = {                                  \
    .n_vals = num_nds,

#define TRANSL_NDS_ND_MAP(idx,br_val,tm_nd)              \
    .val_map[idx] = {br_val,tm_nd},

#define TRANSL_NDS_ND_MAP_END                 }  }


#define TRANSL_CMD(tm_cmds)                            \
    .trans_entry_type = CMD_ENTRY,		       \
    .trans_item = {                                    \
	.tm_cmd = {                                    \
	.cli_cmd = tm_cmds,                            \
	.translator_func = 0,                          \
	.misc_cmd = 0,



#define TRANSL_END_NDS     } } }

#define TRANSL_END_CMDS } }

#define TRANSL_ENTRY_END  } }  ,



#endif
