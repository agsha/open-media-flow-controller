#ifndef __NKN_STREAMLOG__
#define __NKN_STREAMLOG__

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> /* only the defines on windows */
#include <errno.h>
#include <time.h>

//#ifdef INCLUDE_GCL
#include "signal_utils.h"
#include "libevent_wrapper.h"

#define MAX_SL_FORMAT_LENGTH	256

extern lew_context *sl_lew;
//extern md_client_context *sl_mcc;
extern tbool sl_exiting;

//#endif // INCLUDE_GCL

typedef struct {
	char key;
	enum {
		FORMAT_X_TIMESTAMP,       //x-localtime
		FORMAT_S_IP,		//s-ip 
		FORMAT_R_IP,		//r-ip Origin server ipaddress, (dst_ip)
		FORMAT_C_IP,		//c-ip Client ip address, (src_ip)
		//FORMAT_BYTES_DELIVERED,	//Total bytes delivered, (out_bytes)
		FORMAT_TRANSACTION_CODE,//transaction or reply code, (status)
		FORMAT_CS_URI,		//Streaming URL access, (urlSuffix)
		FORMAT_CS_URI_STEM, 	//Streaming URL stem only, (urlPreSuffix)
		FORMAT_CLIENT_ID,	//unique client side streaming id
		FORMAT_STREAM_ID,	//unique server side streaming id, (fOurSessionId)

		FORMAT_EMD_TIME,	//GMT time when transaction ended, (end_ts)
		FORMAT_CLIP_START,	//#of milisec into the stream, when client started receiving stream, (start_ts)
		FORMAT_CLIP_END,	//#of milisec into the stream, when the client stop receiving stream, (end_ts)
		FORMAT_PACKETS,		//Total number of packets delivered to the client
		FORMAT_CS_STATUS,	//HTTP responce code returned to the client
		FORMAT_CON_PROTOCOL,	//Protocol used during connection, RTSP
		FORMAT_TRA_PROTOCOL,	//Transport protocol used during the connection, TCP
		FORMAT_ACTION, 		//Action performed, SPLIT, PROXY, CACHE, CONNECTING
		FORMAT_DROPPED_PACKETS,	//#pf packets dropped 
		FORMAT_PLAY_TIME,	//duration of time the client received the content

		FORMAT_PRODUCT,		//Streaming product used to create and stream content
		FORMAT_RECENT_PACKET, 	//packets resent to the client
		FORMAT_C_PLAYERVERSION,	//client media player version
		FORMAT_C_OS,		//client os
		FORMAT_C_OSVERSION,	//client os version
		FORMAT_C_CPU,		//Client CPU type
		FORMAT_FILELENGTH,	//length of the sream in secs
		FORMAT_AVG_BANDWIDTH,	//Avg bandwidth in bytes per sec
		FORMAT_CS_USER_AGENT,	//player info used in the header
		FORMAT_E_TIMESTAMP,	//number of sec since Jan 1, 1970, when transaction ended

		FORMAT_S_BYTES_IN,	//bytes received from client
		FORMAT_S_BYTES_OUT,	//bytes sent to client
		FORMAT_S_NAMESPACE_NAME,//Namespae name
 
		FORMAT_S_CACHEHIT,
		FORMAT_S_SERVER_PORT,

		FORMAT_S_PERCENT,
		FORMAT_S_UNSET,
		FORMAT_S_STRING,	// Special format, defined by configure
	}type;
	//unsigned int type;
}sl_format_mapping;

extern int 
sl_log_basic(const char *__restrict fmt, ...) 
    __attribute__ ((__format__ (printf, 1, 2)));

extern int 
sl_log_debug(const char *__restrict fmt, ...) 
    __attribute__ ((__format__ (printf, 1, 2)));

extern int sl_config_done;


#endif // __NKN_STREAMLOG__
