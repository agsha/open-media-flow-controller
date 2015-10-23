#include <rtspc_if.h>
#include <stdio.h>


int main(void ){
	rtsp_session_t *sess = NULL;

	rtsp_error_t rtsp_err = RTSP_E_FAILURE;
	rtspmsg_init(NULL);

	rtsp_err = rtspc_create_session("rtsp://192.168.10.11/sample_100kbit.mp4", &sess);
 	if(RTSP_E_SUCCESS != rtspc_init_session(sess)){
		rtspc_destroy_session(sess);
		return (-1);
	}

	sess->rw_state = RTSP_PROCESS_DATA;

	do {
		rtsp_err = rdwr_loop(sess);
	}while (rtsp_err == RTSP_E_SUCCESS || rtsp_err == RTSP_E_RETRY);

	rtspc_destroy_session(sess);
	return 0;
}

/* READ 
 * 1. Peek and Read Socket Till header End
 * 2. Parse Header Till Header End
 * 3. If Entity present - Read Entity till length required
 * 4. Parse Entity
 * Message Read and parse Done
 */

