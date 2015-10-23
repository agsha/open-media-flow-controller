#include <stdio.h>

//mfp includes
#include "mfp_file_sess_mgr_api.h"
#include "mfp_pmf_parser_api.h"
#include "mfp_file_convert_api.h"
#include "mfp_live_sess_id.h"
#include "mfp_mgmt_sess_id_map.h"
#include "mfp_control_pmf_handler.h"

#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_stat.h"

extern file_id_t *file_state_cont;
extern uint32_t glob_mfp_fconv_tpool_enable;
extern uint32_t glob_mfp_max_sess_supported;
static int32_t attachPubContextToSessionId(mfp_publ_t *pub_ctx);

NKNCNT_DEF(mfp_file_num_active_sessions, AO_t,\
	   "", "total number of conversion sessions started")
NKNCNT_DEF(mfp_file_num_ctrl_sessions, AO_t,\
	   "", "total number of control sessions started")
NKNCNT_DEF(mfp_file_num_ses_collision, AO_t,\
	   "", "total number of session id collisions")
NKNCNT_DEF(mfp_file_tot_conv_sessions, AO_t,\
	   "", "total number of conversion sessions started")
NKNCNT_DEF(mfp_file_tot_err_sessions, AO_t,\
	   "", "total number of sessions that exited with errors")
/**
 * start the file processing pipeline by doing the following
 * a. adds to a thread pool for processing.
 * @param path - path to the PMF file
 * @return retunrs 0 on success and a negative integer on error 
 */
int32_t mfp_file_start_listener_session(char *path) 
{

    int32_t rv, sess_id;
    mfp_publ_t *pub;

    AO_fetch_and_add1(&glob_mfp_file_tot_conv_sessions);
	
    /* process and cleanup queue element */
    rv = readPublishContextFromFile(path,
				    &pub);
    if (rv) {
	DBG_MFPLOG ("0", ERROR, MOD_MFPFILE, "error parsing PMF file -"
		    " error code: %d" , -rv);
	AO_fetch_and_add1(&glob_mfp_file_tot_err_sessions);
	return rv;
    }

    /* add to the mgmt session map */
    if (pub->src_type != CONTROL_SRC) {
	/* attach the context to a session */
	sess_id = attachPubContextToSessionId(pub);
	if (sess_id < 0) {
	    rv = -E_MFP_INVALID_SESS;
	    DBG_MFPLOG ("0", ERROR, MOD_MFPFILE, "no free session id"
			" (total concurrent sessions possible %d) -"
			" error code: %d", 
			glob_mfp_max_sess_supported, rv);
	    /* no session id, calling the cleanup routine directly */
	    pub->delete_publish_ctx(pub);
	    AO_fetch_and_add1(&glob_mfp_file_tot_err_sessions);
	    return rv;
	}
	rv = mfp_mgmt_sess_tbl_add(sess_id, (char*)pub->name);
	if (rv){
	    rv = -E_MFP_INVALID_SESS;
	    printf("session id collision %d\n, pmf sess id %s", sess_id,
		   pub->name); 
	    DBG_MFPLOG (pub->name, ERROR, MOD_MFPFILE, "session id collision" 
			"error code:  %d", -rv);
	    AO_fetch_and_add1(&glob_mfp_file_num_ses_collision);
	    return rv;
	}
    } else {
	AO_fetch_and_add1(&glob_mfp_file_num_ctrl_sessions);
	AO_fetch_and_sub1(&glob_mfp_file_tot_conv_sessions);
	rv = handleControlPMF(pub, file_state_cont, FILE_SRC);
	pub->delete_publish_ctx(pub);
	return rv;
    }

    AO_fetch_and_add1(&glob_mfp_file_num_active_sessions);

    /* initiate a coversion */
    rv = mfp_convert_pmf(sess_id);

    if ((glob_mfp_fconv_tpool_enable) && (rv >= 0)) {
	    printf("Processing in progress using thread pool\n");
	    return rv;
    }
    // commented temp. till we fix pubSchemes storage url update in xml
    //file_state_cont->remove(file_state_cont, sess_id);
    return rv;
}

/** 
 * attach a publisher context to a global session id. the function
 * returns the new session id that the publisher context has been
 * attached to
 * @param pub_ctx - publisher context that needs to be attached
 * @return returns the session id (a positive non - zero) integer on
 * success. returns 0 otherwise
 */
static int32_t attachPubContextToSessionId(mfp_publ_t *pub_ctx) 
{
    return file_state_cont->insert(file_state_cont,
				   pub_ctx,
				   (ctxFree_fptr)\
				   pub_ctx->delete_publish_ctx); 
}
