#include "mfp_control_pmf_handler.h"
#include "mfp_pmf_processor.h"

#include "nkn_memalloc.h"

static int32_t buildCtrlRespPMF(char const *pub_name, 
				int32_t status_flag,
				int32_t last_known_err,
				char** resp, int32_t* resp_size); 

static int32_t buildStatusListResponse(mfp_publ_t const *pub,
				       live_id_t *state_cont,
				       char **resp,
				       int32_t *resp_len);

static void sendControlPMF(char const *pub_name, char const *resp,
			   int32_t resp_size);
static int32_t handleConfigAction(const mfp_publ_t *pub);

// Control PMF output directory
const char *const control_pmf_out = "/nkn/vpe/stat";

const char *pmf_ctrl_status_str[] = {"INIT", "SYNC", "RUNNING",
				     "IDLE", "STOPPED", "EEXIST",
				     "REMOVED"}; 
const char *sess_list_resp_name[] = {"live_session_status_list",
				     "file_session_status_list"};

uint32_t ctrl_resp_status[] = {200, 400, 401, 500};
extern pmf_proc_mgr_t *g_pmgr;

int32_t handleControlPMF(mfp_publ_t const *pub, live_id_t* state_cont,
			 SOURCE_TYPE stype) 
{
	int32_t status_flag = 0, last_known_err = 0, sess_flag = 0;
	mgmt_sess_state_t *sess_obj;
	char *resp = NULL, *ctrl_resp_name = NULL;
	int32_t resp_len, rc, cleanup_sess_obj_flag;

	/* init */
	sess_obj = NULL;
	rc = 0;
	cleanup_sess_obj_flag = 0;

	ctrl_resp_name = (char*)pub->name;
	if (pub->op == PUB_SESS_STATE_STATUS_LIST) {
		ctrl_resp_name = (char*)sess_list_resp_name[stype];
		rc = buildStatusListResponse(pub, state_cont, 
					     &resp, &resp_len);
		if (rc) {
			return rc;
		}
		rc = 1;
		goto send_ctrl_pmf;
	}

	if (pub->op == PUB_SESS_STATE_CONFIG) {
	    handleConfigAction(pub);
	}

	// lookup using session name and retrieve sess id
	sess_obj = mfp_mgmt_sess_tbl_find(
			(char*)pub->name);
	if (!sess_obj) {
		DBG_MFPLOG (pub->name, ERROR, MOD_MFPLIVE,
				"Control PMF Session does not exist");
		status_flag = PUB_SESS_STATE_EEXIST;
		last_known_err = 0;
		goto build_resp;
	}

	if (pub->op == PUB_SESS_STATE_STOP) {
		/* handle STOP action
		 * 1. Check if the session is active; if inactive mark the
		 * session to be removed in the PMF response and exit
		 * 2.set the pub->op for the session to STOP
		 */
		DBG_MFPLOG (pub->name, MSG, MOD_MFPLIVE,
				"Received Control PMF with action: STOP");
		/* check if the session is active */
		if (sess_obj->sess_id == -1) {
			sess_obj->status = PUB_SESS_STATE_REMOVE;
			status_flag = PUB_SESS_STATE_REMOVE;
			last_known_err = 0;
			goto build_resp;
		}
		// acquire the session context
		uint32_t sess_id = sess_obj->sess_id;
		mfp_publ_t* sess_pub = state_cont->acquire_ctx(state_cont, sess_id);
		sess_flag = state_cont->get_flag(state_cont, sess_obj->sess_id);
		/* get the sess flag for the current session (this is
		   specifically for live */
		if (sess_flag != -1) { 
			/* session is active - mark for deletion */
			DBG_MFPLOG (pub->name, MSG, MOD_MFPLIVE,
					"Mark the session as inactive");
			state_cont->mark_inactive_ctx(state_cont,
					sess_obj->sess_id);
			// NOTE: Remove/Clean only from TimeoutHandler
			sess_pub->op = PUB_SESS_STATE_STOP;
			last_known_err = sess_obj->err;
			DBG_MFPLOG (pub->name, MSG, MOD_MFPLIVE,
					"Session State: STOPPED");
			status_flag = PUB_SESS_STATE_REMOVE;
			//AO_fetch_and_sub1(&glob_mfp_live_num_pmfs_cntr);

			// Unlink sess name in the mgmt sess tbl
			mfp_mgmt_sess_unlink((char*)pub->name,
					status_flag, 0);
		} else if (sess_flag == -1 ) {
			status_flag = PUB_SESS_STATE_REMOVE;
			last_known_err = sess_obj->err;
		}
		state_cont->release_ctx(state_cont, sess_id);
	} else if (pub->op == PUB_SESS_STATE_STATUS) {
		/* handle STATUS */
		if (sess_obj->status == -1) {
			DBG_MFPLOG (pub->name, ERROR, MOD_MFPLIVE,
					"Session already completed/idle");
			status_flag = PUB_SESS_STATE_REMOVE;
			last_known_err = sess_obj->err;
		} else {
			status_flag = sess_obj->status;
			last_known_err = sess_obj->err;
		}
	} else if (pub->op == PUB_SESS_STATE_REMOVE) { 
		if (sess_obj->sess_id == -1 &&
				sess_obj->status == PUB_SESS_STATE_REMOVE) {
			mfp_mgmt_sess_tbl_remove((char *)pub->name);
			cleanup_sess_obj_flag = 1;
			status_flag = PUB_SESS_STATE_REMOVED;
			last_known_err = sess_obj->err;
			DBG_MFPLOG (pub->name, MSG, MOD_MFPLIVE,
					"Session Removed from Session Table");

		} else {
			DBG_MFPLOG (pub->name, ERROR, MOD_MFPLIVE,
					"Control PMF Session does not exist or"
					" Session has not been stopped with STOP"
					" control PMF; current session state is %s",
					pmf_ctrl_status_str[sess_obj->status]);
		}
	} else {
		DBG_MFPLOG (pub->name, ERROR, MOD_MFPLIVE,
				"Control PMF Session does not exist");
		status_flag = PUB_SESS_STATE_EEXIST;
		last_known_err = 0;
	}

build_resp:
	DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "Session Status %d", status_flag);

	// prepare control PMF
	rc = buildCtrlRespPMF((char*)pub->name, status_flag,
			last_known_err,
			&resp, &resp_len);
	DBG_MFPLOG (pub->name, MSG, MOD_MFPFILE, "Control PMF resonse"
			" size %d", resp_len);

send_ctrl_pmf:
	if (rc > 0) {
		// send control PMF
		sendControlPMF((char*)ctrl_resp_name, resp, resp_len);
	}
	if (resp != NULL) free(resp);
	if (cleanup_sess_obj_flag) mfp_cleanup_mgmt_sess_state(sess_obj);
	return rc;
}


static int32_t buildCtrlRespPMF(char const *pub_name, 
				int32_t status_flag,
				int32_t last_known_err,
				char** resp, int32_t* resp_size) 
{
    char *buf, *ptr;
    const size_t buf_size = 40*1024;
    size_t rv;
    const char *xml_type = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    const char *pmf_open_root_tag_fmt = 
	"<PubManifest version=\"1.0\" name=\"%s\">\n";
    const char *pmf_control_resp_tag_fmt = 
	"<ControlResponse>\n<Status>%s</Status>\n <Error>%d</Error>\n </ControlResponse>\n</PubManifest>"; 
	
    rv = 0;
    ptr = buf = (char*)malloc(buf_size);
    if (!buf)
	return -E_MFP_PMF_NO_MEM;
    
    *resp = buf;
    *resp_size = 0;
    *resp_size += snprintf(ptr, strlen(xml_type) + 2, "%s\n",
			   xml_type);
    ptr += (*resp_size);
    
    rv = snprintf(ptr, strlen(pmf_open_root_tag_fmt) +
		  strlen(pub_name) + 1, 
		  pmf_open_root_tag_fmt, pub_name);
    *resp_size += rv;
    ptr += rv;

    rv = snprintf(ptr, strlen(pmf_control_resp_tag_fmt) +
		  strlen(pmf_ctrl_status_str[status_flag]) + 1,
		  pmf_control_resp_tag_fmt,
		  pmf_ctrl_status_str[status_flag], last_known_err);

    *resp_size += rv;

    return 1;
}


static void sendControlPMF(char const *pub_name, char const *resp,
			   int32_t resp_size) 
{
    int32_t rv = mkdir(control_pmf_out, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (rv && errno != EEXIST) {
	DBG_MFPLOG ("0", MSG, MOD_MFPFILE, "error writing" 
		    " control PMF response, err no %d", 
		    E_MFP_PMF_INVALID_FILE);
	return;
    }
    char out_file_name[256];
    snprintf(out_file_name, 256, "/nkn/vpe/stat/%s.xml", 
	     (char *)pub_name);
    FILE *fp = fopen(out_file_name, "w");
    if (!fp) {
	DBG_MFPLOG ("0", ERROR, MOD_MFPFILE, "error writing" 
		    " control PMF response, err no %d", 
		    E_MFP_PMF_INVALID_FILE);
	return;
    }
    //printf("resp1 : %d resp2: %d\n", (int32_t)strlen(resp), resp_size);
    rv = fwrite(resp, 1, resp_size, fp);
    if (rv == 0)
	perror("fwrite: ");

    //printf("Control PMF write response bytes: %d\n", rv);
    fclose(fp);

}

static int32_t buildStatusListResponse(mfp_publ_t const *pub,
				       live_id_t *state_cont,
				       char **resp,
				       int32_t *resp_size)
{
    char *ptr, *buf;
    const size_t buf_size = 40*1024;
    size_t rv;
    const char *xml_type = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    const char *pmf_open_root_tag_str = 
	"<PubManifest version=\"1.0\" name=\"\">\n";
    const char *pmf_control_resp_tagopen_str = 
	"<ControlResponse>\n<SessionList>\n";
    const char *pmf_sess_state_fmt = "<sess name=\"%s\" error=\"%d\" status=\"%s\"/>\n";
    int32_t i, status_flag, error_flag = 0;
    mgmt_sess_state_t *sess_obj;

    sess_obj = NULL;
    *resp = buf = ptr = \
	nkn_malloc_type(buf_size, mod_mfp_pmf_resp_data);
				  
    if (!buf) {
	return -E_MFP_NO_MEM;
    }

    *resp_size = 0;
    *resp_size += snprintf(ptr, strlen(xml_type) + 2, "%s\n",
			   xml_type);
    ptr += (*resp_size);
    
    rv = snprintf(ptr, strlen(pmf_open_root_tag_str) + 
		  strlen(pmf_control_resp_tagopen_str) + 1, "%s%s",
		  pmf_open_root_tag_str, pmf_control_resp_tagopen_str);
    *resp_size += rv;
    ptr += rv;

    for (i = 0; i < (int32_t)pub->n_sess_names; i++) {
	sess_obj = mfp_mgmt_sess_tbl_find(
					  (char*)pub->sess_name_list[i]);
	if (!sess_obj) {
	    status_flag = PUB_SESS_STATE_EEXIST;
	} else {
	    if (sess_obj->sess_id != -1) {
		/* read status from the pub context */
		mfp_publ_t* sess_pub = state_cont->acquire_ctx(state_cont,
							       sess_obj->sess_id);
		int32_t sess_flag = -1;
		sess_flag = state_cont->get_flag(state_cont,
						 sess_obj->sess_id);
		status_flag = EEXIST;
		if (sess_flag != -1) {
		    status_flag =  sess_pub->op;
		}
		state_cont->release_ctx(state_cont, sess_obj->sess_id);
	    } else {
		/* read status from the session obj */
	    status_flag =  sess_obj->status;
	}
	    error_flag = sess_obj->err;
	}


	rv = snprintf(ptr, buf_size,
		      pmf_sess_state_fmt, pub->sess_name_list[i],
		      error_flag,
		      pmf_ctrl_status_str[status_flag]);
	*resp_size += rv;
	ptr += rv;

    }

    rv =snprintf(ptr, buf_size, "%s",
		 "</SessionList>\n</ControlResponse>\n</PubManifest>");
    *resp_size += rv;

    return 0;    
}

static int32_t 
handleConfigAction(const mfp_publ_t *pub)
{
    dir_watch_th_obj_t *dwto = NULL;
    char *watch_dir_name = NULL;
    int32_t err = 0;
    pthread_t tid;

    switch (pub->cfg.type) {
	case MFP_CFG_ADD_WATCHDIR:
	    err = create_dir_watch_obj(pub->cfg.val, g_pmgr, &dwto);
	    if (err) {
		goto error;
	    }
	    tid = start_dir_watch(dwto);
	    break;
	case MFP_CFG_DEL_WATCHDIR:
	    err = stop_dir_watch(pub->cfg.val);
	    if (err) {
		goto error;
	    }
	    break;
	default:
	    err = -E_MFP_PMF_UNSUPPORTED_CTRL_ACTION;
	    goto error;
    }
    return err;

 error:
    if (watch_dir_name) free(watch_dir_name);
    return err;
}
