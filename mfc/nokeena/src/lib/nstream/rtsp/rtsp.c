#include <rtsp.h>
#include <rtsp_table.h>
#include <rtsp_local.h>


rtsp_error_t rtspmsg_init( char * configuration ){

	rtsp_error_t err = RTSP_E_FAILURE;

	/* Ensure Init sequence is proper */

	err = rtsp_table_init();
	if(err != RTSP_E_SUCCESS){
		return (err);
	}

	err = rtsp_chdr_cb_init();
	if(err != RTSP_E_SUCCESS){
		return (err);
	}

	err = rtsp_phdr_cb_init();
	if(err != RTSP_E_SUCCESS){
		return (err);
	}

	return err;
}

rtsp_error_t rtspmsg_exit(void ){
	/* TODO - cleanup */
	return (RTSP_E_SUCCESS);
}
