#ifndef __LOGD_PUB__H
#define __LOGD_PUB__H

#include <stdint.h>
#include <sys/socket.h>

#define LOG_UDS_ROOT "/vtmp/nknlogd"
#define LOG_UDS_PATH "/vtmp/nknlogd/accesslog"

#define LOGD_PORT		7958
#define DEF_LOG_IP		0x0100007F

#define TYPE_ACCESSLOG  1
#define TYPE_ERRORLOG   2
#define TYPE_TRACELOG   3
#define TYPE_CACHELOG   4
#define TYPE_STREAMLOG  5
#define TYPE_FUSELOG    6
#define TYPE_FMSACCESSLOG 7
#define TYPE_FMSEDGELOG 8
#define TYPE_SYSLOG     9
#define TYPE_MFPLOG     10
#define TYPE_CRAWLLOG   11
#define TYPE_CBLOG   	12
/*! add before TYPE_MAXLOG and change the #define value accordingly */
#define TYPE_MAXLOG     12


/*
 * nknlogd network protocol
 */
#define SIG_SEND_ACCESSLOG 	"nkn_send_accesslog_1_0"
#define SIG_SEND_DEBUGLOG 	"nkn_send_debuglog_1_0"
#define SIG_SEND_STREAMLOG 	"nkn_send_streamlog_1_0"
#define SIG_REQ_ACCESSLOG 	"nkn_req_accesslog_1_0"
#define SIG_REQ_DEBUGLOG 	"nkn_req_debuglog_1_0"
#define SIG_REQ_STREAMLOG 	"nkn_req_streamlog_1_0"
#define SIG_REQ_FUSELOG		"nkn_req_fuselog_1_0"
#define SIG_REQ_FMSACCESSLOG	"nkn_req_fmsaccesslog_1_0"
#define SIG_REQ_FMSEDGELOG	"nkn_req_fmsedgelog_1_0"
#define SIG_REQ_MFPLOG		"nkn_req_mfplog_1_0"

/* Nokeena Log Protocol */
typedef struct NLP_client_hello_t {
	char junk[32];	// Just raise the bar for attack.
	char sig[32];
} NLP_client_hello;

typedef struct NLP_server_hello_t {
	char junk[32];	// Just raise the bar for attack.
	int answer;
	int numoflog;	// number of logs
	int lenofdata;	// Total length of data excluding header.
} NLP_server_hello;

typedef struct NLP_DEBUGLOG_CHANNEL_t {
	int32_t channelid;
	int32_t level;
	uint64_t mod;
	int32_t type;
	int32_t reserved;
} NLP_debuglog_channel;

typedef struct NLP_ACCESSLOG_CHANNEL_t {
	int32_t channelid;
	int32_t num_of_hdr_fields;
	int32_t len;	// excluding the size of the header.
} NLP_accesslog_channel;

typedef struct NLP_STREAMLOG_CHANNEL_t {
	int32_t channelid;
	int32_t num_of_hdr_fields;
	int32_t len;	// excluding the size of the header.
} NLP_streamlog_channel;

typedef struct NLP_data_t {
	int32_t channelid;
	uint32_t len;
} NLP_data;

#endif // __LOGD_PUB__H

