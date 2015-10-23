#ifndef __ERRORLOG__H__
#define __ERRORLOG__H__

/*
 * Handshake between errorlog client and nvsd
 */

/* request */

#define EL_MAGIC  "NEL errorlog_ver_1.0"

typedef struct errorlog_req {
	char magic[32];
} errorlog_req_t;
#define errorlog_req_s sizeof(struct errorlog_req)

#define ELR_STATUS_SUCCESS		1
#define ELR_STATUS_UNKNOWN_ERROR	-1
#define ELR_STATUS_VERSION_NOTSUPPORTED	-2
#define ELR_STATUS_INTERNEL_ERROR	-3
#define ELR_STATUS_SOCKET_ERROR		-4
#define ELR_STATUS_STOP_TRYING		-5

/* response */

typedef struct errorlog_res {
	int32_t status;
} errorlog_res_t;
#define errorlog_res_s sizeof(struct errorlog_res)

#endif // __ERRORLOG__H__

