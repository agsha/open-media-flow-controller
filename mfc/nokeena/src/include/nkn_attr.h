#ifndef NKN_ATTR_H
#define NKN_ATTR_H
/*
 * (C) Copyright 2008 Nokeena Networks, Inc
 *
 * Definitions for the NVSD atttribute cache
 *
 * Each object in the cache can have associated attributes.  Some of these
 * attributes are visible to the cache and media providers.  The rest are
 * opaque and application/protocol specific.  Each opaque attribute has an
 * ID and length.
 */
#include <stdint.h>
#include <time.h>
#include "nkn_defs.h"

#define NKN_MAX_ATTR_V1_SIZE   512	// max size of nkn_attr_v1_t obj
#define NKN_DEFAULT_ATTR_SIZE  (4*1024) // default size of attributes
#define NKN_ATTR_HEADER_LEN    ((int64_t)(&((nkn_attr_t *)0)->na_blob))

typedef struct nkn_attr_id {
    union {
	struct nkn_attribute_id {
	    uint16_t protocol_id:4;
	    uint16_t id:12;
        } attr;
	uint16_t attr_id;
    } u;
} nkn_attr_id_t;

typedef uint8_t nkn_attr_len_v1_t;
/*
 * 16 bits is enough for all practical purposes, but 20 bits will guarantee
 * that all future values will fit.
 */
typedef struct nkn_attr_entry_data_s {
    uint32_t	len:20;
    uint32_t	pad:12;
} nkn_attr_entry_data_t;
#define NKN_ATTR_LEN_MAX 0x000fffff	// 20 bits is enough

typedef struct {
    nkn_attr_id_t     id;
    nkn_attr_len_v1_t len;
    uint8_t	      unused;
    // char value[];		// variable length value
} nkn_attr_entry_v1_t;

typedef struct {
    nkn_attr_entry_data_t ae_data;
    nkn_attr_id_t	  id;
    uint8_t		  unused[2];
} nkn_attr_entry_t;
#define attr_entry_len ae_data.len

/*
 * In Version 1, an attribute object remained untouched on disk as it was
 * deleted.  The notion of whether the attribute was active or deleted was
 * stored outside the nkn_attr_t structure.  With V2, we retain this notion.
 * We considered putting external information, like a URI's basename, into
 * nkn_attr_t, but this started to muddle the definition of nkn_attr_t because
 * the HTTP layer would need to provide the URI's basename to the nkn_attr_init
 * function.  The notion of a basename is something that only DM2 needs and only
 * DM2 could generate from a URI name.
 */
#define NKN_ATTR_MAGIC		0xCAFEBABE
#define NKN_ATTR_V1_MAGIC	0xCAFEBABE
#define NKN_ATTR_V1_MAGIC_OLD	0xDEADBEEF
#define NKN_ATTR_V1_VERSION	1
#define NKN_ATTR_VERSION	2

/*
 * The header of version 1 will be 64 bytes, so we have a pad(unused field).
 * After the header, this structure has a blob until the end of the
 * attribute.
 *
 * The disk manager will have no idea what is in the blob.  Any attribute
 * which the disk manager must interpret must be before the blob.
 *
 * total_attrsize is the size of the padded attribute and should be a
 * multiple of DEV_BSIZE=512.  total_attrsize while perhaps redundant is
 * useful should we have different sized nkn_attr_t objects.  While objects
 * made by nkn_attr_init will be NKN_MAX_ATTR_SIZE, ones coming off disk
 * may be a different size for various reasons.
 */
typedef struct nkn_attr_v1 {
    uint64_t	checksum;	// Can not move
    uint32_t	magic;		// 0xCAFEBABE - can not move
    uint8_t	version;	// version == 1 - can not move
    uint8_t	entries;	// # entries in blob
    uint16_t	total_attrsize;	// total size of this attr object on disk
    time_t	cache_expiry;	// when data should be tossed
    time_t	cache_time0;	// when data can be served
    uint64_t	content_length;	// total data length of object
    uint8_t	flags;
    uint8_t	unused1;
    uint16_t	attrsize;	// used size of attr object
    char	unused2[12];
    uint64_t	hotval;		// opaque value determined by AM
    uint8_t	blob[448];
} nkn_attr_v1_t;


/*
 * sizeof(nkn_attr_priv_t) == 16
 */
union nkn_attr_priv {
    struct {
	uint8_t	 v_type:5;
	uint8_t	 v_subtype:3;
	uint8_t  v_unused[1];
	uint16_t v_video_bit_rate;     //kbps; max 65 Mbps
	uint16_t v_audio_bit_rate:13;  //kbps; max 8 Mbps
	uint16_t v_reserved:3;         //reserved
	uint16_t v_video_codec_type:5; //max 32 video codecs
	uint16_t v_audio_codec_type:5; //max 32 audio codecs
	uint16_t v_cont_type:6;        //max 64 container typ
	uint16_t v_duration;
	uint8_t v_padding[6];
    } vpe;
    struct {
	uint8_t f_type:5;
	uint8_t f_subtype:3;
	uint8_t f_padding[15];
    } full;
};
typedef union nkn_attr_priv nkn_attr_priv_t;

#define NKN_ATTR_VPE	0x1

#define priv_type		na_private.full.f_type
#define vpe_video_bitrate	na_private.vpe.v_video_bit_rate
#define vpe_audio_bitrate	na_private.vpe.v_audio_bit_rate
#define vpe_duration		na_private.vpe.v_duration
#define vpe_video_codec_type	na_private.vpe.v_video_codec_type
#define vpe_audio_codec_type	na_private.vpe.v_audio_codec_type
#define vpe_cont_type		na_private.vpe.v_cont_type

/*
 * The header of version 2 will be 256 bytes.  To ease readability, use a pad
 * unused field).
 *
 * - Fields should be 64-bit aligned.
 * - blob_attrsize includes the header.  It is the useful part of the entire
 *   attribute.
 *
 */
typedef struct nkn_attr {
    uint64_t	    na_checksum;	// Can not move
    uint32_t	    na_magic;		// 0xCAFEBABE - can not move
    uint8_t	    na_version;		// structure version == 2 - can not move
    uint8_t	    na_entries;		// # entries in blob
    uint8_t	    na_flags;		// 8 bits of flags
    uint8_t	    na_flags2;		// align to 64 bits
    uint64_t	    content_length;	// total data length of object
    nkn_objv_t	    obj_version;	// 128b: probably ETAG
    time_t	    cache_expiry;	// 64b: when data should be tossed
    time_t	    cache_time0;	// 64b: when data can be served
    /* cache_create = origin_cache_create + cache_correctedage */
    time_t	    origin_cache_create;// 64b: when data 1st hit cache - ctime
    time_t	    cache_reval_time;	// 64b: last time data was revalidated
    uint64_t	    hotval;		// opaque value determined by AM
    uint64_t	    inodenum;
    uint32_t	    total_attrsize;	// total size of attr object
    uint32_t	    blob_attrsize;	// used size of attr object (the blob)
    mode_t	    mode;		// 32b: for FUSE
    uint16_t	    encoding_type;      // encoding type
    uint16_t	    na_respcode;	// response code
    uint32_t	    hit_count;          // hit count
    uint32_t	    na_unused1;		// 
    nkn_attr_priv_t na_private;		// 128b: private data
    uint64_t	    na_padding2[3];	// Save if na_private needs to grow
    int64_t	    start_offset;
    int64_t	    end_offset;
    time_t	    cache_correctedage;	// 64b: corrected age from remote cache/origin 
    AO_t	    cached_bytes_delivered; // Bytes delivered from Cache 
    char	    na_unused2[72];	// pad out to 256 bytes
    char	    na_blob[0];		// should start 256 bytes into structure
} nkn_attr_t;

// Attribute flags
#define NKN_OBJ_STREAMING          0x01	// objects w/o content length
#define NKN_OBJ_FORCEREVAL         0x02	// sync providers during force-reval
#define NKN_OBJ_FORCEREVAL_ALWAYS  0x04 // sync all during force-reval always
#define NKN_OBJ_CACHE_NOUPDATE	   0x08 // update the cache and providers
#define NKN_OBJ_VOID_2_0_X	   0x10 // Conflicts with 2.0.X checkin
#define NKN_PROXY_REVAL            0x20 // object has to be revalidated
#define NKN_OBJ_CACHE_PIN	   0x40 // object can not be evicted from cache
#define NKN_OBJ_COMPRESSED	   0x80 // Object is compressed internally gzip
// na_flags2 defines
#define NKN_OBJ_INVALID		   0x01
#define NKN_OBJ_NEGCACHE	   0x02
#define NKN_OBJ_VARY		   0x04 // Object has vary header and content varies based on header

/* Operations for opaque attributes */

// Initialize the opaque attributes to a null set
void nkn_attr_init(nkn_attr_t *ap, int maxlen);

// Delete all blob entries but retain C-struct data
void nkn_attr_reset_blob(nkn_attr_t *ap, int zero_used_blob);

// Set a new attribute.  Duplicates are not detected.  Returns error if there
// is not enough space to add the attribute.
int nkn_attr_set(nkn_attr_t *ap, nkn_attr_id_t id, uint32_t len, void *value);

// Get an attribute value.  Returns a pointer to the value in the attributes
// and sets the length.  Returns NULL is the attribute does not exist.
void *nkn_attr_get(nkn_attr_t *ap, nkn_attr_id_t id, uint32_t *len);


// Get the nth attribute value, where nth is 0 based.
// Returns 0 on success with the pointers set for id, value and length.
int nkn_attr_get_nth(nkn_attr_t *ap, int nth, nkn_attr_id_t *id, void **data,
		     uint32_t *len);

// Copy an nkn_attr_t object, ensuring sector alignment for write.  Optionally,
// zero the unused space.  Returns 0 on success with a new nkn_attr_t object
// in ap_dst.
int nkn_attr_copy(nkn_attr_t *ap_src, nkn_attr_t **ap_dst,
		  nkn_obj_type_t objtype);

// Copy an attribute object to a user supplied buffer
void nkn_attr_docopy(nkn_attr_t *ap_src, nkn_attr_t *ap_dst, uint32_t dstsize);

// Mark that the object is streaming in
void nkn_attr_set_streaming(nkn_attr_t *ap);

// Mark that the object has stopped streaming (ie.. reached its end)
void nkn_attr_reset_streaming(nkn_attr_t *ap);

// Check if the object is still streaming in
// Returns 1 if streaming else 0
int nkn_attr_is_streaming(nkn_attr_t *ap);

// Update the content length for the object
void nkn_attr_update_content_len(nkn_attr_t *ap, int len);

// Indicate that an object is to be pinned in cache
void nkn_attr_set_cache_pin(nkn_attr_t *ap);

// Reset the indication of cache pinning
void nkn_attr_reset_cache_pin(nkn_attr_t *ap);

// Return 1 if object is pinned (or to be pinned) in the cache
int nkn_attr_is_cache_pin(const nkn_attr_t *ap);

// Indicate that an object is to be pinned in cache
void nkn_attr_set_negcache(nkn_attr_t *ap);

// Reset the indication of cache pinning
void nkn_attr_reset_negcache(nkn_attr_t *ap);

// Return 1 if object is pinned (or to be pinned) in the cache
int nkn_attr_is_negcache(const nkn_attr_t *ap);

// Set/Reset/Check VARY header handling flag
void nkn_attr_set_vary(nkn_attr_t *ap);
void nkn_attr_reset_vary(nkn_attr_t *ap);
int nkn_attr_is_vary(const nkn_attr_t *ap);



#endif	/* NKN_ATTR_H */
