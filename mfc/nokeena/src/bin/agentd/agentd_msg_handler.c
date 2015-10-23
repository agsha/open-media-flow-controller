#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "common.h"
//#include "include/agentd_translation_table.h"
#include "agentd_mgmt.h"
#include "nkn_mgmt_defs.h"
#include "md_client.h"
#include "mdc_wrapper.h"
extern int jnpr_log_level;

static int
mfc_agentd_xml_form_mgmt_nd(const char *mgmt_nd_fmt_str, nd_fmt_str_mapping_t nd_fmt_str_mapping ,
	agentd_binding *cmd_obj, char *tm_node);


/* This function performs the arithmetic operation "op" on the input parameters "br_val" & "operand"
   and returns the result of operation in "out_val" */
static int manipulate_value (const char *br_val, arith_op_t op, int operand, char * out_val) {
/* TODO : 1. Need to hande error / sanity check for value overflow
	  2. Use strtoul() instead of atoi() to handle uint values
*/
    int val = 0;
    int err = 0;

    val =  atoi(br_val);
    lc_log_debug (LOG_DEBUG, "value initial = %d\n", val);
    switch (op) {
        case ADD:
            val += operand;
            break;
        case SUB:
            val -= operand;
            break;
        case MULT:
            val *= operand;
            break;
        case DIV:
            if (operand) {
		val /= operand;
	    } else {
                lc_log_basic (jnpr_log_level, "Divide by zero. Value not changed");
            }
            break;
        default:
            lc_log_debug (LOG_DEBUG, "Invalid operator");
            err = -1;
            break;
    }
    snprintf (out_val, sizeof(agentd_value_t), "%d", val);
    lc_log_debug (LOG_DEBUG, "value after manipulation = %s\n", out_val);
    return err;
}

static const char* swap_node_vals(const char *br_val, mgmt_nd_data_t *mgmt_nd, swap_type_t type)
{
	int i = 0;
	//bail_null_error(br_val, "Null value recieved");

	if(type == VAL) {

		for(i = 0; i < mgmt_nd->val_str_mapping.n_vals ; i++) {
			//bail_null_error(mgmt_nd->val_str_mapping.val_map[i].map, "Null value recieved");
			if(strcmp(br_val, mgmt_nd->val_str_mapping.val_map[i].map) == 0) {
				return mgmt_nd->val_str_mapping.val_map[i].val;
			}
		}
	} else if (type == ND) {
		for(i = 0; i < mgmt_nd->nd_str_mapping.n_vals ; i++) {
			//bail_null_error(mgmt_nd->nd_str_mapping.val_map[i].map, "Null value recieved");
			if(strcmp(br_val, mgmt_nd->nd_str_mapping.val_map[i].map) == 0) {
				return mgmt_nd->nd_str_mapping.val_map[i].val;
			}
		}
	}
	return NULL;
}

int
agentd_fill_request_msg(agentd_context_t  *context,
	agentd_binding *cmd_obj, mgmt_nd_lst_t *mgmt_nd_list, int *ret_code)
{
    int i = 0;
    int ign_idx = 0, ign_arg_idx = 0, ign_pattern = 0;
    node_name_t tm_node = {0};
    const char *tm_nd_fmt_str = NULL;
    const char *tm_val = NULL;
    bn_set_subop subop = bsso_modify;
    int err = 0;
    bn_binding *binding = NULL;
    bn_binding_array *barr = NULL;
    agentd_value_t out_val = {0};

    /* UNUSED Variable */
    context = context;

    //	bail_null_error(context, "The agentd context message is NULL");
    //	bail_null_error(nd_lst_tag, "node list is null");
    //	bail_null_error(mgmt_nd_list, "mgmt nd list is null");

    for(i = 0; i < min(mgmt_nd_list->nnds,MAX_MGMT_NDS); i++) {
	mgmt_nd_data_t *mgmt_nd = &mgmt_nd_list->mgmt_nd_data[i];
	assert(mgmt_nd != NULL);

	if(mgmt_nd->subop == SUBOP_DELETE) {
	    subop = bsso_delete;
            barr = context->del_bindings;
	} else if(mgmt_nd->subop == SUBOP_RESET) {
	    subop = bsso_reset;
            barr = context->reset_bindings;
	} else {
            barr = context->mod_bindings;
        }

	//TODO: Add subop here
	//Possible subops for set-request are create,modify/delete
	//How to set bsso_create, bsso_modify ?

        /* Check if the pattern should be ignored or translated */ 
        if (mgmt_nd->ign_val_mapping.n_vals) {
            lc_log_debug (LOG_DEBUG, "mgmt_nd->ign_val_mapping.n_vals=> %d",
                                         mgmt_nd->ign_val_mapping.n_vals);
            ign_pattern = 0;
            for (ign_idx = 0; (ign_idx<mgmt_nd->ign_val_mapping.n_vals && ign_idx<MAX_IGNORE_ITEMS); ign_idx++) {
                ign_arg_idx = mgmt_nd->ign_val_mapping.ign_val[ign_idx].arg_index;
                if (!mgmt_nd->ign_val_mapping.ign_val[ign_idx].val) continue;
                lc_log_debug (LOG_DEBUG, "Ignore index: %d, ignore string: %s, string in pattern: %s",
                                            ign_arg_idx,
                                            mgmt_nd->ign_val_mapping.ign_val[ign_idx].val,
                                            cmd_obj->arg_vals.val_args[ign_arg_idx + 1]);
                if (strcmp ( mgmt_nd->ign_val_mapping.ign_val[ign_idx].val, 
                             cmd_obj->arg_vals.val_args[ign_arg_idx + 1]) == 0) {
                    /* pattern contains the argument to be ignored */
                    ign_pattern = 1;
                    break; 
                }
            }
            if (ign_pattern) continue; /* Goto the translation entry in the list */
        }

	if(mgmt_nd->nd_type == ND_NORMAL) {

	    lc_log_debug(LOG_DEBUG, "mgmt_nd->nd_str_mapping.n_vals=> %d, val_index=> %d",
		    mgmt_nd->nd_str_mapping.n_vals, mgmt_nd->val_index );
	    if(mgmt_nd->nd_str_mapping.n_vals) {
		tm_nd_fmt_str = swap_node_vals(cmd_obj->arg_vals.val_args[mgmt_nd->val_index + 1],
			mgmt_nd,
			ND);
	    } else {
		tm_nd_fmt_str= mgmt_nd->nd_name;
	    }

	    lc_log_debug(LOG_DEBUG, "tm_nd_fmt_str=> %s", tm_nd_fmt_str);
	    mfc_agentd_xml_form_mgmt_nd(tm_nd_fmt_str,
		    mgmt_nd->nd_fmt_str_mapping,
		    cmd_obj ,tm_node);

	    lc_log_debug(LOG_DEBUG, "tm_nd_fmt_str=> %s", tm_node);
	    /*Assuming tm_node has the node value*/

	    /* For a normal node,the vaule is taken from the cmd_obj args */
	    /* if the value type differs from broker to TM Node
	     * Do a trick with val_str_mapping
	     */
	    if(mgmt_nd->val_str_mapping.n_vals) {
		tm_val =  swap_node_vals(cmd_obj->arg_vals.val_args[mgmt_nd->val_index +1 ], mgmt_nd,
			VAL);
	    } else {
		tm_val = cmd_obj->arg_vals.val_args[mgmt_nd->val_index + 1];
	    }

	    bail_null(tm_val);

	    lc_log_debug(LOG_DEBUG, "node=> %s, type=> %d, val=> %s",
		    tm_node,  mgmt_nd->nd_dt_type,tm_val );

            /* If the value has to be manipulated, then apply the arithmetic operation
               and get the value to store in TM node */
            if (mgmt_nd->op) {
                err = manipulate_value(tm_val, mgmt_nd->op, mgmt_nd->operand, out_val);
                bail_error (err);
                tm_val = out_val;
            }

	    err = bn_binding_new_str_autoinval(
		    &binding, tm_node, ba_value, mgmt_nd->nd_dt_type, 0,
		    tm_val);
	    bail_error(err);

	    err = bn_binding_array_append_takeover (barr, &binding); 
	    bail_error(err);
	} else if(mgmt_nd->nd_type == ND_HARDCODED) {
	 //   snprintf(tm_node, sizeof(tm_node), mgmt_nd->nd_name, cmd_obj->arg_vals.val_args[0 + 1]);
	    mfc_agentd_xml_form_mgmt_nd(mgmt_nd->nd_name,
		    mgmt_nd->nd_fmt_str_mapping,
		    cmd_obj ,tm_node);

	   // lc_log_debug(LOG_DEBUG, "tm_nd_fmt_str=> %s", tm_node);

	    lc_log_debug(LOG_DEBUG, "node=> %s, type=> %d, val=> %s",
		    tm_node,  mgmt_nd->nd_dt_type, mgmt_nd->value);

	    /* For a hard coded node,The value are defined in the translation table*/
	    err = bn_binding_new_str_autoinval(
		    &binding, tm_node, ba_value, mgmt_nd->nd_dt_type, 0,
		    mgmt_nd->value);
	    bail_error(err);

            err = bn_binding_array_append_takeover (barr, &binding);
            bail_error (err);
	} else if(mgmt_nd->nd_type == ND_DYNAMIC) {
	    node_name_t tm_value = {0};
	    mfc_agentd_xml_form_mgmt_nd(mgmt_nd->nd_name,mgmt_nd->nd_fmt_str_mapping ,
		    cmd_obj, tm_node);

	    mfc_agentd_xml_form_mgmt_nd(mgmt_nd->value,mgmt_nd->nd_fmt_str_mapping ,
		    cmd_obj, tm_value);

	    lc_log_debug(LOG_DEBUG, "node=> %s, type=> %d, val=> %s",
		    tm_node,  mgmt_nd->nd_dt_type,tm_value );

	    err = bn_binding_new_str_autoinval(
		    &binding, tm_node, ba_value, mgmt_nd->nd_dt_type, 0,
		    tm_value);
	    bail_error(err);
            err = bn_binding_array_append_takeover (barr, &binding);
            bail_error (err);
	}
    }
bail:
    return err;
}

static int
mfc_agentd_xml_form_mgmt_nd(const char *mgmt_nd_fmt_str, nd_fmt_str_mapping_t nd_fmt_str_mapping ,
		agentd_binding *cmd_obj,char *tm_node)
{
	switch(nd_fmt_str_mapping.n_args)
	{
		case 0:
			snprintf(tm_node, sizeof(node_name_t), "%s", mgmt_nd_fmt_str);
			break;
		case 1:
			snprintf(tm_node, sizeof(node_name_t), mgmt_nd_fmt_str,
				cmd_obj->arg_vals.val_args[nd_fmt_str_mapping.arg_index[0] + 1 ]);
			break;
		case 2:
			snprintf(tm_node, sizeof(node_name_t), mgmt_nd_fmt_str,
				cmd_obj->arg_vals.val_args[nd_fmt_str_mapping.arg_index[0] + 1],
				cmd_obj->arg_vals.val_args[nd_fmt_str_mapping.arg_index[1] + 1]);
			break;
		case 3:
			snprintf(tm_node, sizeof(node_name_t), mgmt_nd_fmt_str,
				cmd_obj->arg_vals.val_args[nd_fmt_str_mapping.arg_index[0] + 1],
				cmd_obj->arg_vals.val_args[nd_fmt_str_mapping.arg_index[1] + 1],
				cmd_obj->arg_vals.val_args[nd_fmt_str_mapping.arg_index[2] + 1]);
			break;
		case 4:
			lc_log_debug(LOG_NOTICE, "hitting case 4, must not happen");
			break;
	}
	return 0;
}
