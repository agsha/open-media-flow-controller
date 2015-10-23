/*
 * Move Networks Server Side Player
 *
 */

#include <string.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <alloca.h>

#include "ssp.h"
#include "ssp_authentication.h"
#include "ssp_queryparams.h"
#include "http_header.h"
#include "nkn_errno.h"
#include "nkn_debug.h"
#include "nkn_common_config.h"
#include "nkn_stat.h"
#include "nkn_ssp_players.h"



////////////////////////////////////////////////////////
// Move Networks Server Side Player (Move SSP Type 2) //
////////////////////////////////////////////////////////

int
moveSSP(con_t *con, const char *uriData, int uriLen, const ssp_config_t *ssp_cfg)
{
    char moveProfile[3];
    const int PROFTAGLEN = 14;
    uint64_t assuredRate = 0;
    uint64_t afrlimit=0;
    int remapLen=0;
    int rv, i=0;

    if (findStr(uriData, ".qss", uriLen) != -1) {

	// Check for presence of any query params and ignore for the moment, since Move does not provide any query params to process
	// Fix for Bug #1318: Move VirtualPlayer needs to accept "?" at end of URI
	if ( (remapLen = findStr(uriData, "?", uriLen)) != -1 ) { // If ? is present, then strip it out to find the correct profile
	    uriLen = remapLen;
	}

	// Move URI format
	// /vod/BBB87026/pda_201_bzzzzzzzz_episode_1813604/CDB061FD0712D142A39EF3ED8757E68E_070000002B.qss
	if ( uriLen < (PROFTAGLEN+1) ) {
	    DBG_LOG(WARNING, MOD_SSP, "Requested URI %s is not conformant with customer specified format", uriData);
	    return -NKN_SSP_BAD_URL;
	}

	if ( strncmp( &uriData[uriLen - (PROFTAGLEN+1)], "_", 1) !=0 ) {
	    DBG_LOG(WARNING, MOD_SSP, "Requested URI %s is not conformant with customer specified format", uriData);
	    return -NKN_SSP_BAD_URL;
	}

	strncpy(moveProfile, &uriData[uriLen - PROFTAGLEN], 2);
	moveProfile[2]='\0';

	// Obtain specified con max b/w value
	if (ssp_cfg->con_max_bandwidth > 0) { // CMBvp > 0
	    afrlimit = ssp_cfg->con_max_bandwidth;
	    con->max_bandwidth = ssp_cfg->con_max_bandwidth;
	} else { // if no limit is specified, then client bw is the limit, n/w bw is not used when VP is enabled
	    afrlimit = con->max_client_bw;
	}

	if (ssp_cfg->rate_map.enable) { // AFR to be set by SSP

	    while (i < ssp_cfg->rate_map.entries) {

		if ( !strncmp (moveProfile, ssp_cfg->rate_map.profile_list[i].matchstr, strlen(moveProfile)) ) { // match found

		    // Set the Assured Bit Rate for this segment of video (Hex to Dec)
		    assuredRate = ssp_cfg->rate_map.profile_list[i].rate;

		    // SSP rate check before forwarding the request
		    // We need to take the max_allowed_bw from interface structure, not from con structure.
		    // It is because this max_allowed_bw could be changed from socket accpeted to this time ssp is getting called.
		    // -- Max
		    if (assuredRate <= (uint64_t) MIN(afrlimit, (unsigned)con->pns->max_allowed_bw) ) {
			con_set_afr(con, assuredRate);
			DBG_LOG(MSG, MOD_SSP, "AFR Set: %ld Bytes/sec, Profile: %s", con->min_afr, moveProfile);
			if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
			    DBG_TRACE("AFR Set: %ld Bytes/sec, Profile: %s", con->min_afr, moveProfile);
			}
			rv = SSP_SEND_DATA_OTW;
		    }
		    else {
			// Reject connection, since requested rate > available bandwidth
			DBG_LOG(WARNING, MOD_SSP, "AFR Failed: %ld B/W unavailable for Profile: %s or AFR > Max con bw", assuredRate, moveProfile);
			if (CHECK_HTTP_FLAG(&con->http, HRF_TRACE_REQUEST)) {
			    DBG_TRACE("AFR Failed. %ld B/W unavailable for profile %s or AFR > Max con bw", assuredRate, moveProfile);
			}
			rv =  -NKN_SSP_AFR_INSUFF;
		    }

		    goto exit;
		}

		i++;
	    }//while

	    // No match, hence close connection
	    DBG_LOG(WARNING, MOD_SSP, "Requested profile %s not supported per config", moveProfile);
	    return -NKN_SSP_BAD_URL;

	}
	else { // AFR not requested to be set
	    rv =  SSP_SEND_DATA_OTW;
	}

    }
    else { // This will handle .qmx and other extensions in the future
	if (ssp_cfg->con_max_bandwidth > 0) { // CMBvp > 0
            con->max_bandwidth = ssp_cfg->con_max_bandwidth;
	}
	rv = SSP_SEND_DATA_OTW;
    }

 exit:
    return rv;
}


