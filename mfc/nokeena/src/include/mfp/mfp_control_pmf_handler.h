#ifndef MFP_CONTROL_PMF_HANDLER_H
#define MFP_CONTROL_PMF_HANDLER_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h> 

#include "mfp_publ_context.h"
#include "mfp_mgmt_sess_id_map.h"
#include "mfp_live_sess_id.h"
#include "nkn_debug.h"

typedef enum {
	CRESP_SESS_ACTIVE,
	CRESP_SESS_INACTIVE,
	CRESP_SESS_NOT_FOUND,
	CRESP_SESS_ACT_UNSUPP,
} ctrl_resp_code_t;


extern uint32_t ctrl_resp_status[];

// Possible responses for control PMF
extern char const* ctrl_resp_status_str[];


int32_t handleControlPMF(mfp_publ_t const *pub, live_id_t *state_cont,
			 SOURCE_TYPE stype); 


#endif

