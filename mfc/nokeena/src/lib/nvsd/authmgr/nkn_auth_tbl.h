#ifndef _NKN_AM_TBL_API_H
#define _NKN_AM_TBL_API_H


#include <sys/queue.h>
#include "nkn_authmgr.h"

typedef struct auth_loc_tbl_t
{
	auth_msg_t auth_msg_loc;    
	TAILQ_ENTRY(auth_loc_tbl_t) p_tbl_entry_p;
} auth_loc_tbl_t;


void auth_htbl_destroy(void);
void auth_htbl_list_init(void);
auth_loc_tbl_t* auth_create_loc_tbl_obj(auth_msg_t *data);
void auth_free_loc_tbl_obj(auth_msg_t* obj);
int auth_update_local(auth_msg_t *data);
int auth_remove_local(unsigned char *remkey);
int auth_fetch_local(auth_msg_t* data);
int auth_htbl_sz(void);
int auth_get_auth_from_file(auth_msg_t*);

#endif /* _NKN_AM_TBL_API_H */
