/*
 * (C) Copyright 2008-2010 Ankeena Networks, Inc
 *
 * Definitions used commonly across the server.
 */
#ifndef NKN_DEFS_H
#define NKN_DEFS_H
#include <sys/time.h>
#include <stdint.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netinet/in.h>
#include "nkn_errno.h"
#include "nkn_memalloc.h"

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

/* Maximum network FDs that we can create/allocate in our application */
#define CACHE_YIELD	(30)		/* 30% connections are active */
#define MISC_FD_USERS	(5)		/* 5% overhead */
#define MAX_CLIENTS	(512*1024)
#define MAX_ORIGINS	(256*1024)	/* What if all clients are tunneled */
/*
 * Max possible fd is computed below:
 * Total =
 *	    (number of possible client conns)	+
 *	    (number of possible origin conns)	+
 *	    (number of internal ingest requests to origin) +
 *	    (what if DM keeps some fds open)	+
 *	    (what if we use some shmem which is file backed)
 */
#define MAX_GNM			(MAX_CLIENTS +				    \
				MAX_ORIGINS  +				    \
				(MAX_ORIGINS * (MISC_FD_USERS/100)))

/* Optimized for rounding up when Y is power of 2 */
#define ROUNDUP_PWR2(x, y) ( ((x) + (y) - 1) & ~((y)-1) )

/* Expiry time constants */
#define NKN_EXPIRY_UNDEFINED 0
#define NKN_EXPIRY_FOREVER (-1)

#define MAX_URI_HDR_HOST_SIZE 128
// /{name max 16}:XXXXXXXX (max 8) _YYYYYYYY (host max 128):XXXXX (port max 5)
#define MAX_URI_HDR_SIZE (1+16+1+8+1+MAX_URI_HDR_HOST_SIZE+1+5)
#define MAX_URI_SIZE (MAX_URI_HDR_SIZE+512)

/* Pseudo file descriptor macros */
#define PSEUDO_FD_BASE (MAX_GNM + 1024)

/* Maximum HTTP 302 redirects that we allow, to avoid an infinite loop.
 * Beyond this limit, hand over response to client and let the client
 * handle it.
 */
#define MAX_HTTP_REDIRECT   (5)

/*
 * PSEUDO_FD_BASE must be > MAX_GNM to avoid any clash in the fd
 * space. Check MAX_GNM
 */
#if (defined( MAX_GNM) && (MAX_GNM > 0))
#if (MAX_GNM >= PSEUDO_FD_BASE)
#error "Potential clash in fd space for MAX_GNM (nkn_defs.h) and PSEUDO_FD_BASE (nkn_defs.h). Resolve this first!"
#endif /* (MAX_GNM > PSEUDO_FD_BASE) */
#endif /* ifdef MAX_GNM */

#if (defined(MAX_GNM))
#  if ((MAX_GNM) > (1024*1024))
#  error "Cant handle 1048576 descriptors!! Reduce MAX_GNM."
#  elif ((MAX_GNM) > (1047552))
#  error "MAX_GNM overlaps PSEUDO_FD_BASE!!"
#  endif
#endif

#define IS_PSEUDO_FD(_fd) (((_fd) >= PSEUDO_FD_BASE) && ((_fd) > 0))

/*
 * nkn_attr_id_t definitions
 */
#define NKN_ATTR_MIME_HDR_ID   0xfff

/*
 * For less confusion,
 * Nokeena refers K, M, G to 1000 based
 * Nokeena refers Ki, Mi, Gi to 1024 based
 */
#define KBYTES	1000
#define KiBYTES	1024
#define MBYTES	(1000 * 1000)
#define MiBYTES	(1024 * 1024)
#define GBYTES	(1000 * 1000 * 1000)
#define GiBYTES	(1024 * 1024 * 1024)

/*
 * When -Wall option in compiler is enabled,
 * we should suppress any argument which is not used inside function
 * The following macro should work.
 */
#define UNUSED_ARGUMENT(x) (void)x

/*
 * MGMT constants
 */
#define NKN_MAX_NAMESPACE_LENGTH 16
/* limiting FQDN_LENGTH to 254 becasue of a limitation in VJX */
#define NKN_MAX_FQDN_LENGTH	254
#define NKN_MAX_VPLAYER_LENGTH	 64
#define NKN_MAX_NAMESPACES	 1025 /* max # of namespaces supported.
					Adding 1 for mfc_probe.
					user should be able to configure 256
					namespace.*/

/*
 * currently matching server-maps with maximum number of namespaces,
 * idealy it should be 2X of max namespaces supported
 */
#define NKN_MAX_SERVER_MAPS		 256
#define NKN_MAX_FILTER_MAPS		 256

/*
 * device-map related configuration
 */
#define NKN_MAX_DEVICE_MAPS	1

/*
 * Format: "/namespace:uuid"
 * 1: leading '/'
 * 16: NKN_MAX_NAMESPACE_LENGTH
 * 1: ':'
 * 8: strlen("12345678")	can only be 8 bytes
 */
#define NKN_MAX_UID_LENGTH \
    (1 + NKN_MAX_NAMESPACE_LENGTH + 1 + strlen("12345678"))


/*
 * NVSD object cache.
 * The following types are used to access the NVSD object cache.
 * The COD (Cache Object Descriptor) is an open handle to a cache object.
 * The COD incorporates the names of the object as well as a specific instance.
 * The UOL structure encapsulates a basic request spec.  It includes the COD,
 * the offset and length.
 */
typedef u_int64_t nkn_cod_t;            /* COD */
#define NKN_COD_NULL    0		/* Null value for COD */

// 80 bit long double to strut
typedef struct dtois {
	uint64_t  ui;
	uint16_t  us;
}dtois_t;

/*
 * Operations on buffers
 */
typedef void nkn_buffer_t;


/* Unique id for this instance of object */
typedef struct {
    union {
	uint8_t no_objv_c[16];
	uint64_t no_objv_l[2];
    } u_ver;
} nkn_objv_t;
#define objv_c u_ver.no_objv_c
#define objv_l u_ver.no_objv_l
typedef struct ip_addr {
    uint8_t    family;
    union {
        struct  in_addr v4;
        struct  in6_addr v6;
    } addr;
} ip_addr_t;

typedef struct dns_ip_addr {
    uint8_t    valid;
    ip_addr_t  ip;
} dns_ip_addr_t;

#define IPv4(x)  x.addr.v4.s_addr
#define IPv6(x)  x.addr.v6.s6_addr

#define NKN_OBJV_EQ(_p1, _p2) \
	(((_p1)->objv_l[0] == (_p2)->objv_l[0]) && \
	 ((_p1)->objv_l[1] == (_p2)->objv_l[1]))

#define NKN_OBJV_EMPTY(_p1) \
	(((_p1)->objv_l[0] == 0) && ((_p1)->objv_l[1] == 0))

typedef struct nkn_uol {
    nkn_cod_t cod;
    off_t offset;
    off_t length;
} nkn_uol_t;

typedef struct nkn_iovec {
    char  *iov_base;
    off_t iov_len;
} nkn_iovec_t;

/* The protocol being used for request/response */
typedef enum {
    UNKNOWN,			// internal request
    NKN_PROTO_HTTP,
    NKN_PROTO_FTP,
    NKN_PROTO_RTSP,
    NKN_PROTO_RTMP
} nkn_protocol_t;

typedef struct out_proto_data {
    union {
	uint64_t data[7];
	struct OM_data {
	    int32_t respcode;
	    int16_t remote_port;
	    int16_t pad;
	    int32_t flags;
	    int64_t content_length;
	    uint32_t incarn; // incarnation should go together with cp_sockfd.
	    int32_t cp_sockfd;
	    int32_t httpcon_sockfd;
	    ip_addr_t remote_ipv4v6;
	} OM;
	struct OM_resp_cache_index_data {
		unsigned char cksum[32];
	} OM_cache_idx_data;
	/** Add new defs here, insure def is <= sizeof(data) **/
    } u;
} out_proto_data_t;

/*
 * Various providers of content
 *
 * Give the lowest provider a value of 1, so physids will always be non-zero
 * NOTE: Do not change these numbers. Send out email before changing anything
 *	here.
 */
typedef enum {
    Unknown_provider = 0,
    SolidStateMgr_provider = 1,
    FlashMgr_provider = 2,
    BufferCache_provider = 3,
    SASDiskMgr_provider = 4,
    SAS2DiskMgr_provider = 5,
    SATADiskMgr_provider = 6,
    NKN_MM_max_pci_providers = 7,
    Peer_provider = 10,
    TFMMgr_provider = 11,
    Tunnel_provider = 12, /* not registered in MM, used for accesslog only */
    RTSPMgr_provider = 17,
    NKN_MM_origin_providers = 18,
    NFSMgr_provider = 19,
    OriginMgr_provider = 20, /* Origin Manager should always be LAST */
    NKN_MM_MAX_CACHE_PROVIDERS = 21,
} nkn_provider_type_t;

const char *nkn_provider_name(nkn_provider_type_t id);

typedef enum {
    NKN_DEL_REASON_CLI = 1,
    NKN_DEL_REASON_EXPIRED = 2,
    NKN_DEL_REASON_EXT_EVICTION = 3,
    NKN_DEL_REASON_DISK_FULL_INT_EVICTION = 4,
    NKN_DEL_REASON_ATTR2BIG = 5,
    NKN_DEL_REASON_SHARED = 6,
    NKN_DEL_REASON_REVAL_FAILED = 7,
    NKN_DEL_REASON_OBJECT_PARTIAL = 8,
    NKN_DEL_REASON_STRM_OBJECT_PARTIAL = 9,
    NKN_DEL_REASON_TIMED_INT_EVICTION = 10,
    NKN_DEL_REASON_MEM_FULL_INT_EVICTION = 11
} nkn_del_reason_t;

/* Helpful structure for representing byte ranges */
typedef struct byte_range {
    uint64_t start_offset;
    uint64_t end_offset;
} br_obj_t;

/* client data structure */
typedef struct nkn_client_data {
    nkn_protocol_t	proto;
    ip_addr_t		ipv4v6_address;
    unsigned short	port;
    uint8_t		client_ip_family;
#define NKN_CLIENT_CRAWL_REQ 1
    uint8_t		flags;
} nkn_client_data_t;

/* module type */
typedef enum {
    INVALID_MODULE = 0,
    SSP,
    NORMALIZER,
} nkn_module_t;

/* Handle to network object */
typedef struct net_fd_handle {
    uint32_t incarn;
    int32_t fd;
} net_fd_handle_t;

/*
 * Global variables that are kept updated by the timer using the time() system
 * call at a 1 second granularity.  This is a very cheap way to add
 * a coarse grained timestamp.
 */
extern int64_t nkn_cur_100_tms;
extern time_t nkn_cur_ts;
extern int nkn_system_inited;


/* Provider cookie to be used to identify the provider and its sub-components*/
typedef uint32_t provider_cookie_t;

/* This function encodes the provider and subprovider ids into one integer. */
extern provider_cookie_t AM_encode_provider_cookie(nkn_provider_type_t ptype,
						   uint8_t sptype,
						   uint16_t ssptype);
extern void AM_decode_provider_cookie(provider_cookie_t id,
				      nkn_provider_type_t *ptype,
				      uint8_t *sptype, uint16_t *ssptype);
extern char *nkn_get_datestr(int *datestr_len);

/*
 * The following two APIs are designed to mark a fd usage.
 *
 * nkn_mark_fd() should be called after fd is opened.
 * nkn_close_fd() should be called instead of close(fd) directly.
 *
 * If nkn_mark_fd() is called for a fd, nkn_close_fd() MUST be called to
 * cleanup signature.  Otherwise, the next usage of same fd will be asserted.
 *
 * At nkn_close_fd() time, if signature does not match, it will be asserted.
 */
typedef enum {
    FREE_FD = 0,
    NETWORK_FD,
    CLIENT_FD,
    SERVER_FD,
    OM_FD,
    DM2_FD,
    RTSP_FD,
    RTSP_UDP_FD,
    RTSP_OM_FD,
    RTSP_OM_UDP_FD,
} nkn_fd_type_t;

void nkn_mark_fd(int fd, nkn_fd_type_t type);
int nkn_close_fd(int fd, nkn_fd_type_t type);

typedef struct request_context {
    uint64_t opaque[2];
} request_context_t;


typedef struct system_cfg_chng_ctxt {
    int is_db_change;
} system_cfg_chng_ctxt_t;

/*
 * mgmtd HTTP /cmm-node-status.html object defines
 */
#define CMM_NODE_STATUS_HTML_VERSION	1

#define	    NETWORK_INIT	(1<<0)  //Not used as of now
#define	    PREREAD_INIT	(1<<1)

/* Transparent proxy - port-range functionality macros/defines */
#define D_TP_PORT_RANGE_MIN_DEF_STR "20000"
#define D_TP_PORT_RANGE_MIN_DEF 20000
#define D_TP_PORT_RANGE_MIN1_STR "1024"
#define D_TP_PORT_RANGE_MIN1 1024
#define D_TP_PORT_RANGE_MIN2_STR "65533"
#define D_TP_PORT_RANGE_MIN2 65533

#define D_TP_PORT_RANGE_MAX_DEF_STR "52768"
#define D_TP_PORT_RANGE_MAX_DEF 52768
#define D_TP_PORT_RANGE_MAX1_STR "1025"
#define D_TP_PORT_RANGE_MAX1 1025
#define D_TP_PORT_RANGE_MAX2_STR "65534"
#define D_TP_PORT_RANGE_MAX2 65534

/* HW TYPE */
#define HW_TYPE_UNKNOWN	    0
#define HW_TYPE_PACIFICA    1
#define HW_TYPE_OTHER	    2

/* MAX FQDNS supported per namespace */
#define NKN_MAX_FQDNS	    16

#endif /* NKN_DEFS_H */
