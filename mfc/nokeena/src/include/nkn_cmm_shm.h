/*
 * nkn_cmm_shm.h - CMM shared memory definitions
 */
#ifndef NKN_CMM_SHM_H
#define NKN_CMM_SHM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/param.h>
#include <stdint.h>

/*
 *******************************************************************************
 * Generic definitions
 *******************************************************************************
 */
typedef char * cmm_segment_t;

#define CMM_SEG_PTR(_hdr, _n) \
	(cmm_segment_t *)(((char *)(_hdr))+(_hdr)->cmm_seg_offset + \
			  ((_hdr)->cmm_segment_size * (_n)))

typedef struct cmm_shm_hdr {
    uint32_t cmm_magic;
    uint32_t cmm_version;
    uint32_t cmm_segment_size;
    int32_t cmm_segments;
    uint32_t cmm_seg_offset;
    char cmm_bitmap[1]; // sizeof(char[roundup(MAX_SEGMENTS, NBBY)/NBBY])-1
} cmm_shm_hdr_t;

/*
 *******************************************************************************
 * CMM node status shared memory segment definitions
 *******************************************************************************
 */
#define CMM_SHM_KEY 0x204d4d44
#define CMM_SHM_SIZE (7 * 1024 * 1024) // roughly at max, (7168-1) nodes

#define CMM_SHM_MAGICNO CMM_SHM_KEY
#define CMM_SHM_VERSION 2

#define CMM_SEGMENT_SIZE 1024
#define CMM_MAX_HDR_SIZE CMM_SEGMENT_SIZE
#define CMM_SEGMENTS ((CMM_SHM_SIZE/CMM_SEGMENT_SIZE)-1)

/*
 * Note: 
 *   sizeof(cmm_shm_hdr_t) + 
 *   sizeof(char[roundup(CMM_SEGMENTS, NBBY)/NBBY])-1 <= CMM_MAX_HDR_SIZE 
 *   must be true.
 */

/*
 * cmm_segment_t is a sequence of {name}={value}'|' entries
 */
#define CMM_NM_NAMESPACE "namespace"
#define CMM_NM_NODE "node"

#define CMM_NM_STATE "state"
#define CMM_VA_STATE_ON "on"
#define CMM_VA_STATE_OFF "off"

#define CMM_NM_OP_SUCCESS "op-success"
#define CMM_NM_OP_TIMEOUT "op-timeout"
#define CMM_NM_OP_OTHER "op-other"
#define CMM_NM_OP_UNEXPECTED "op-unexp"
#define CMM_NM_OP_ONLINE "op-online"
#define CMM_NM_OP_OFFLINE "op-offline"

#define CMM_NM_LOOKUP_TIME "lookup"
#define CMM_NM_CONNECT_TIME "connect"
#define CMM_NM_PREXFER_TIME "prexfer"
#define CMM_NM_STARTXFER_TIME "startxfer"
#define CMM_NM_TOTAL_TIME "total"
#define CMM_NM_RUN_TIME "run"

#define CMM_NM_CURL_NO_CONNECT "curl-noconn"
#define CMM_NM_CURL_TIMEOUT "curl-timeout"
#define CMM_NM_CURL_SND_ERR "curl-snderr"
#define CMM_NM_CURL_RECV_ERR "curl-recverr"

#define CMM_NM_CFG_HEARTBEAT_URL "cfg-hearturl"
#define CMM_NM_CFG_HEARTBEAT_INTERVAL "cfg-heartinterval"
#define CMM_NM_CFG_CONNECT_TIMEOUT "cfg-conn-timeout"
#define CMM_NM_CFG_ALLOWED_FAILS "cfg-allowed-fails"
#define CMM_NM_CFG_READ_TIMEOUT "cfg-read-timeout"

/*
 *******************************************************************************
 * CMM node "load metric" shared memory segment definitions
 *******************************************************************************
 */
#define CMM_LD_SHM_KEY 0x205d5d54
#define CMM_LD_SHM_SIZE (4 * 1024 * 1024) // max (8192-1), supports CMM_SHM max

#define CMM_LD_SHM_MAGICNO CMM_LD_SHM_KEY
#define CMM_LD_SHM_VERSION 3

#define CMM_LD_SEGMENT_SIZE 512
#define CMM_LD_MAX_HDR_SIZE CMM_LD_SEGMENT_SIZE
#define CMM_LD_SEGMENTS ((CMM_LD_SHM_SIZE/CMM_LD_SEGMENT_SIZE)-1)

/*
 * cmm_segment_t is a sequence of cmm_loadmetric_entry_t(s).
 */
typedef uint64_t node_load_metric_t;

#define SIZEOF_NODE_HANDLE 128
#define SIZEOF_PAD (CMM_LD_SEGMENT_SIZE - sizeof(node_load_metric_t) - \
		    sizeof(uint64_t) - SIZEOF_NODE_HANDLE - sizeof(uint64_t))

typedef struct cmm_loadmetric_entry {
    union {
        struct cmm_loadmetric_data {
	    node_load_metric_t load_metric;
	    uint64_t incarnation;
	    char node_handle[SIZEOF_NODE_HANDLE];
	    uint64_t version;
	    char pad[SIZEOF_PAD];
	} loadmetric;
    	char data[CMM_LD_SEGMENT_SIZE];
    } u;
} cmm_loadmetric_entry_t;

/* 
 * Client access API:
 *
 *  1) Locate cmm_loadmetric_entry_t using node_handle.
 *
 *     for (n = 0; n < hdr->cmm_segments; n++) {
 *         if (isset(hdr->cmm_bitmap[n])) {
 *             lmp = (cmm_loadmetric_entry_t *)CMM_SEG_PTR(hdr, n);
 * 	       if (strcmp(node_handle, lmp->u.loadmetric.node_handle) == 0) {
 *	           // Match: Note data pointer, index and incarnation
 *		   break;
 *	       }
 *         }
 *     }
 *
 *  2) Get load metric
 *     if (isset(hdr->cmm_bitmap[index]) && 
 *         (incarnation == lmp->u.loadmetric.incarnation)) {
 *         return lmp->u.loadmetric.load_metric;
 *     }
 *
 */

/*
 * Note: 
 *   sizeof(cmm_shm_hdr_t) + 
 *   sizeof(char[roundup(CMM_LD_SEGMENTS, NBBY)/NBBY])-1 <= CMM_LD_MAX_HDR_SIZE 
 *   must be true.
 */

#ifdef __cplusplus
}
#endif

#endif /* NKN_CMM_SHM_H */

/*
 * End of nkn_cmm_shm.h
 */
