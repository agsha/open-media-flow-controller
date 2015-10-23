#ifndef _MFP_LIVE_SESS_MGR_
#define _MFP_LIVE_SESS_MGR_

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct tag_mfp_live_sess_mgr_cont {
	int32_t err;
	mfp_publ_t *pub;
    }mfp_live_sess_mgr_cont_t;
    
    /**
     * schedules a live listener session. Does this following sequence
     * of operations
     * 1. if the scheduling is set to now then we trigger a session
     * immediately 
     * 2. if this is a scehduled event then we sceduled a delayed
     * event; further if the duration is set then we create a pseudo
     * STOP Control PMF and the scehdule a delayed event for the
     * Control PMF 
     * @param path - path to the PMF file
     * @return returns 0 on success and a non-zero negative number on
     * error 
     */
    int32_t mfp_live_schedule_live_session(char *path);
    
#ifdef __cplusplus
}
#endif

#endif //_MFP_LIVE_SESS_MGR_
