#ifndef _MGMT_SESS_ID_MAP_
#define _MGMT_SESS_ID_MAP_

#include <stdio.h>
#include <inttypes.h>

#ifdef __cplusplus 
extern "C" {
#endif
    typedef struct _tag_mgmt_sess_state {
	int32_t sess_id;
	int32_t status;
	int32_t err;
	pthread_mutex_t lock;
    }mgmt_sess_state_t;

    int32_t mfp_mgmt_sess_tbl_init(void);
    int32_t mfp_mgmt_sess_tbl_add(int32_t sess_id, char *mgmt_sess_id);
    mgmt_sess_state_t* mfp_mgmt_sess_tbl_find(char *mgmt_sess_id);
    int32_t mfp_mgmt_sess_tbl_remove(char *mgmt_sess_id);
    int32_t mfp_mgmt_sess_unlink(char *mgmt_sess_id, int32_t status,
				 int32_t err); 
    mgmt_sess_state_t * mfp_mgmt_sess_tbl_acq(char *mgmt_sess_id);
    int32_t mfp_mgmt_sess_tbl_rel(char *mgmt_sess_id);
    int32_t mfp_mgmt_sess_replace(mgmt_sess_state_t *sess_obj,
				  int32_t sess_id);
    void mfp_cleanup_mgmt_sess_state(mgmt_sess_state_t *p);
#ifdef __cplusplus
}
#endif

#endif //_MGMT_SESS_ID_MAP_
