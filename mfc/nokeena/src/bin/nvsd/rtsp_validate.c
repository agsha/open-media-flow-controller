#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/queue.h>
#define USE_SIGNALS 1
#include <time.h> // for "strftime()" and "gmtime()"
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic_ops.h>


#define USE_SPRINTF

// NKN generic defs
#include "nvsd_mgmt.h"
#include "nkn_debug.h"
#include "nkn_defs.h"
#include "nkn_cfg_params.h"
#include "nkn_sched_api.h"
#include "rtsp_server.h"
#include "rtsp_header.h"

rtsp_parse_status_t rtsp_validate_protocol(rtsp_cb_t *prtsp)
{
    rtsp_parse_status_t status = RPS_OK;
    assert (prtsp);
    /* Do Validation of RTSP params */

    unsigned int hdr_attr = 0;
    int hdr_cnt = 0;
    const char *hdr_str = NULL;
    int hdr_len = 0;

    /* URL Length 
     * FIXME - Limited to 256 due to hard coding on underlying
     * functions/modules
     * */
    if ((0 == mime_hdr_get_known(&prtsp->hdr, RTSP_HDR_X_URL,
				 &hdr_str, &hdr_len, &hdr_attr,
				 &hdr_cnt))) {
	if(hdr_len > RTSP_MAX_URL_LEN){
	    RTSP_SET_ERROR(prtsp,RTSP_STATUS_414 ,NKN_RTSP_PVE_URL_1);
	    return (RPS_ERROR);
	}
    }
    return (status);
}

rtsp_parse_status_t rtsp_check_mfd_limitation(rtsp_cb_t *prtsp)
{
	rtsp_parse_status_t status = RPS_OK;
	assert (prtsp);
	/* Do MFD Specific Validations */

	if (rtsp_relay_only_enable) {
		return RPS_ERROR;
	}

	return (status);
}
