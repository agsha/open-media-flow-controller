#include <stdint.h>

void nkn_locmgr_uri_tbl_print(void);
void nkn_locmgr_uri_tbl_init(void);
int nkn_locmgr_uri_tbl_insert(char *key, void *value);
void nkn_locmgr_uri_tbl_delete(char *key);
void *nkn_locmgr_uri_tbl_get(char *key);

void nkn_locmgr_file_tbl_print(void);
void nkn_locmgr_file_tbl_init(void);
int nkn_locmgr_file_tbl_insert(char *key, void *value);
int nkn_locmgr_file_tbl_delete(char *key);
void *nkn_locmgr_file_tbl_get(char *key);
void nkn_locmgr_uri_tbl_cleanup(void);


struct LM_file {
	uint64_t cur_offset;
	uint64_t cur_block;
};
