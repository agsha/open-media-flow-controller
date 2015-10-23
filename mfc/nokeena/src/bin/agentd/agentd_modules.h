#ifndef __AGENTD_MODULE_H_
#define __AGENTD_MODULE_H_

int agentd_load_modules(void);
int agentd_unload_modules(void);
int agentd_init_modules(void);
int agentd_deinit_modules(void);
int agentd_get_symbol(const char *module_name, const char *symbol_name,
               void **ret_symbol);

/*
int agentd_register_trans_cmds_array ( );
int agentd_register_trans_cmd ( );
*/
#endif /* __AGENTD_MODULE_H_ */
