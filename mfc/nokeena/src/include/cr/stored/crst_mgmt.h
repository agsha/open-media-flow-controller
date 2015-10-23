#ifndef _CRST_STORE_
#define _CRST_STORE_

#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>

#include "md_client.h"
#include "mdc_wrapper.h"
#include "random.h"
#include "libevent_wrapper.h"
#include "nkn_mgmt_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0 //keeps emacs happy
}
#endif

void * crst_mgmt_init(void *);

int nkn_cr_stored_cfg_query(const bn_binding_array *bindings, 
			    uint32_t i);

int nkn_cr_stored_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
		const bn_binding *binding,
		const tstring *bndgs_name,
		const tstr_array *bndgs_name_components,
		const tstring *bndgs_name_last_part,
		const tstring *b_value,
		void *data);

int nkn_cr_stored_delete_cfg_handle_change(const bn_binding_array *arr, uint32 idx,
		const bn_binding *binding,
		const tstring *bndgs_name,
		const tstr_array *bndgs_name_components,
		const tstring *bndgs_name_last_part,
		const tstring *b_value,
		void *data);

int
nkn_cr_stored_module_cfg_handle_change (bn_binding_array *old_bindings,
					 bn_binding_array *new_bindings);


int cr_mgmt_lib_mdc_foreach_binding_prequeried(const bn_binding_array *bindings,
					    const char *binding_spec, const char *db_name,
					    mdc_foreach_binding_func callback, void *callback_data);


#ifdef __cplusplus
}
#endif

#endif //_CRST_STORE_
