/*
 *
 * tuuid.h
 *
 *
 *
 */

#ifndef __TUUID_H_
#define __TUUID_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "tstring.h"


/* ------------------------------------------------------------------------- */
/** \file tuuid.h Routines for working with UUIDs.
 * \ingroup lc
 */

struct lc_uuid {
    uint32 lu_time_low;
    uint16 lu_time_mid;
    uint16 lu_time_hi_and_version;
    uint8  lu_clock_seq_hi_and_reserved;
    uint8  lu_clock_seq_low;
    uint8  lu_node[6];
} __attribute__ ((packed));

typedef struct lc_uuid lc_uuid;

#define lc_uuid_length_bits 128
static const uint32 lc_uuid_length_bytes = lc_uuid_length_bits / 8;

typedef enum {
    luv_none = 0,
    luv_hash_md5 = 3,
    luv_hash_sha1 = 5,
} lc_uuid_version;


/* ------------------------------------------------------------------------- */
/* Generate a UUID string from a hash.  This function requies a hash
 * of at least 128 bits.  It transforms this into a UUID following the
 * rules of RFC 4122, and then renders it to ASCII.
 *
 * \param digest Pointer to the beginning of the digest (hash result)
 * to use as input for the UUID.
 *
 * \param digest_length Length of the digest available, in bytes.
 * This must be at least lc_uuid_length_bytes; and anything beyond 
 * that length will be ignored.
 *
 * \param uuid_ver UUID version number, from RFC 4122.  Callers should
 * pass luv_hash_md5 here if the hash provided is MD5; or
 * luv_hash_sha1 if it is SHA1.
 *
 * \param flags Reserved for future use; for now, must pass 0.
 *
 * \retval ret_uuid_str The UUID string.
 */
int lc_uuid_hash_to_uuid_tstr(const uint8 *digest, uint32 digest_length,
                              lc_uuid_version uuid_ver, uint32 flags,
                              tstring **ret_uuid_str);


#ifdef __cplusplus
}
#endif

#endif /* __TUUID_H_ */
