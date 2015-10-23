#include <stdint.h> 

void nkn_locmgr_physid_tbl_print(void);
void nkn_locmgr_physid_tbl_init(void);
int nkn_locmgr_physid_tbl_insert(char *key, void *value);
void nkn_locmgr_physid_tbl_delete(char *key);
void *nkn_locmgr_physid_tbl_get(char *key);
void nkn_locmgr_physid_tbl_cleanup(void);
