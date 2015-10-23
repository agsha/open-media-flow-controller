#ifndef __NVSD_MGMT_lib__H
#define __NVSD_MGMT_lib__H
extern int mgmt_lib_mdc_foreach_binding_prequeried(const bn_binding_array *bindings,
		const char *binding_spec, const char *db_name,
		mdc_foreach_binding_func callback, void *callback_data);



//TODO
typedef int (*cfg_handle_change_func) (const bn_binding_array *arr, uint32 idx,
		bn_binding *binding, void *data);
// Should use the above callback later

//typedef int (cfg_query_func) (const bn_binding_array *arr);

typedef struct mgmt_query_data_st {
	const char *query_nd;
	cfg_handle_change_func chg_func;
} mgmt_query_data_t;


/* Define all mgmt nodes prefix that are to be queried or
   for which change event should be handled
 */
typedef enum nd_prefix_en {
	MGMT_PREFIX_START,
	/* Define all nodes here */
	MGMT_PREFIX_END,
} nd_prefix_t;

#endif
