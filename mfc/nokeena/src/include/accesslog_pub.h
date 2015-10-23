#ifndef __ACCESSLOG__H__
#define __ACCESSLOG__H__

#if 0
#define AL_MAGIC  "NAL accesslog_ver_1.0"

typedef struct accesslog_req {
	char magic[32];

	int tot_size;			// include this structure itself
	uint32_t num_of_hdr_fields;
	/* followed by num of HTTP header fields */
} accesslog_req_t;
#define accesslog_req_s sizeof(struct accesslog_req)

#define ALR_STATUS_SUCCESS		1
#define ALR_STATUS_UNKNOWN_ERROR	-1
#define ALR_STATUS_VERSION_NOTSUPPORTED	-2
#define ALR_STATUS_INTERNAL_ERROR	-3
#define ALR_STATUS_SOCKET_ERROR		-4

typedef struct accesslog_res {
	int32_t status;
} accesslog_res_t;
#define accesslog_res_s sizeof(struct accesslog_res)
#endif // 0

#define ALF_HTTP_10                     0x0000000000000001
#define ALF_HTTP_11                     0x0000000000000002
#define ALF_CONNECTION_CLOSE            0x0000000000000004
#define ALF_CONNECTION_KEEP_ALIVE       0x0000000000000008
#define ALF_METHOD_GET                  0x0000000000000010
#define ALF_BYTE_RANGE                  0x0000000000000020
#define ALF_TRANSCODE_CHUNKED           0x0000000000000040
#define ALF_METHOD_HEAD			0x0000000000004000
#define ALF_METHOD_POST			0x0000000000008000
#define ALF_METHOD_CONNECT		0x0000000000010000
#define ALF_METHOD_OPTIONS		0x0000000000020000
#define ALF_SENT_FROM_TUNNEL		0x0000000000040000
#define ALF_RETURN_REVAL_CACHE		0x0000000000080000
#define ALF_METHOD_PUT              	0x0000000000100000
#define ALF_METHOD_DELETE              	0x0000000000200000
#define ALF_METHOD_TRACE              	0x0000000000400000
#define ALF_HTTP_09                     0x0000000000800000
#define ALF_SECURED_CONNECTION		0x0000000001000000
#define ALF_RETURN_OBJ_GZIP		0x0000000004000000
#define ALF_RETURN_OBJ_DEFLATE		0x0000000008000000
#define ALF_EXP_OBJ_DEL			0x0000000010000000

/*
 * For RTSP flag
 */
#define SLF_METHOD_OPTIONS		0x0000000000000001
#define SLF_METHOD_DESCRIBE		0x0000000000000002
#define SLF_METHOD_PLAY			0x0000000000000004
#define SLF_METHOD_SETUP		0x0000000000000008
#define SLF_METHOD_PAUSE		0x0000000000000010
#define SLF_METHOD_GET_PARAMETER	0x0000000000000020
#define SLF_METHOD_TEARDOWN		0x0000000000000040
#define SLF_METHOD_BROADCAST_PAUSE	0x0000000000000080
#define SLF_METHOD_BAD			0x0000000000000100
#define SLF_METHOD_ANNOUNCE		0x0000000000000200
#define SLF_METHOD_RECORD		0x0000000000000400
#define SLF_METHOD_REDIRECT		0x0000000000000800
#define SLF_METHOD_SET_PARAM		0x0000000000001000
#define SLF_METHOD_SENT_DESCRIBE_RESP	0x0000000000002000
#define SLF_METHOD_SENT_SETUP_RESP	0x0000000000004000
#define SLF_METHOD_SENT_PLAY_RESP	0x0000000000008000
#define SLF_METHOD_SENT_TEARDOWN_RESP	0x0000000000010000
#define SLF_REVALIDATE_CACHE		0x0000000000020000

#define SLF_SMODE_RTP_UDP		0x0100000000000000
#define SLF_SMODE_RTP_TCP		0x0200000000000000
#define SLF_SMODE_RAW_UDP		0x0400000000000000
#define SLF_SMODE_RAW_TCP		0x0800000000000000
#include "nkn_defs.h"

typedef struct accesslog_item {
	int		tot_size;	// depreciated
	/*
	 * Socket information
	 */
	uint16_t	reserved;
	uint16_t	dst_port;
	ip_addr_t	src_ipv4v6;
	ip_addr_t	dst_ipv4v6;

	/*
	 * HTTP transaction information
	 */
	struct timeval	start_ts;	// HTTP begin timestamp 
	struct timeval	end_ts;		// HTTP end timestamp
	struct timeval	ttfb_ts;	// time to first byte
	uint64_t	flag;
	uint64_t	in_bytes;
	uint64_t	out_bytes;
	uint16_t	status;
	uint16_t	reserved2;
	uint32_t	subcode;
	uint32_t	resp_hdr_size;

	/*
	 * Variable name/value pair related infromation
	 */
	uint32_t	num_of_hdr_fields;
	/* followed by num of HTTP header fields */
} accesslog_item_t;

#define accesslog_item_s sizeof(struct accesslog_item)
#define SET_ALITEM_FLAG(x, f) (x)->flag |= (f)
#define CHECK_ALITEM_FLAG(x, f) ((x)->flag & (f))

//Add Streaming log support
#define streamlog_item_t accesslog_item_t
#define streamlog_item_s sizeof(struct accesslog_item)

void dpi_accesslog_write(char *uri, int urilen, char *host, int hostlen,
			 uint64_t method, uint32_t ip_src, uint32_t ip_dst,
			 uint64_t sub_errcode);

#endif // __ACCESSLOG__H__

