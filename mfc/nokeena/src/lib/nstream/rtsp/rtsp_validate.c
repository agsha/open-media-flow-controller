
#include <rtsp.h>
#include <ctype.h>

extern int32_t rtsp_get_valid_host_len(const char *host, int32_t host_len){

	const char *scan = NULL;
	const char *end = NULL;
	const char STATE_DOT = 0x01;
	const char  STATE_DASH = 0x02 ;
	const char  STATE_OTHER = 0x04 ;
	int32_t sym_state = 0x00;

	if(!host || host_len <=0){
		return (RTSP_E_INVALID_ARG);
	}

	scan = host;
	end = host + host_len;

	if(*scan == '-')
		return (scan - host);

	/* TODO  need to validate with proper spec
	 * Numbers allowed in start of name
	 * hname :: <name>*["." name]
	 * name :: <let or digit>[*[<let or digit or hyphen]<let or digit>]
	 * */

	while(scan < end){
		if(isalnum(*scan)){
			sym_state = 0x00;
		} else if (*scan == '-') {
			if(sym_state & STATE_DOT){
				break;
			}
			sym_state = STATE_DASH;
		} else if (*scan == '.') {
			if(sym_state & (STATE_DOT|STATE_DASH)){
				break;
			}
			sym_state = STATE_DOT;
		} else {
			sym_state = STATE_OTHER;
			break;
		}
		scan++;
	}

	if (sym_state){
		/* Host cannot be in this states below
		 * a .-
		 * b -.
		 * c ..
		 * */
		return (-1);
	} 

	return(scan - host);
}

extern rtsp_error_t rtsp_validate_host(const char *host, int32_t host_len){
	if(host_len == rtsp_get_valid_host_len(host, host_len))
		return (RTSP_E_SUCCESS);
	return (RTSP_E_FAILURE); 
}
