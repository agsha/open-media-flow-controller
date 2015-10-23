/*
 *
 * hmac.h
 *
 *
 *
 */

#ifndef __HMAC_H_
#define __HMAC_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"

static const uint32 lc_hmac_block_md5_length = 64;
static const uint32 lc_hmac_digest_md5_length = 16;

static const uint32 lc_hmac_block_sha256_length = 64;
static const uint32 lc_hmac_digest_sha256_length = 32;


/* ------------------------------------------------------------------------- */
/** \file hmac.h Generate an HMAC for the given data.
 * \ingroup lc
 */

/**
 * Common prototype for HMAC hash functions.
 */
typedef int (*lc_hmac_func)(const uint8 *key, uint32 key_len,
                            const uint8 *data, uint32 data_len,
                            uint8 **ret_digest, uint32 *ret_digest_len);

/**
 * Generate an HMAC for the given data using MD5.
 * \param key the random key to use for this hash.
 * \param key_len the length of the key (recommend 16-64).
 * \param data the data to hash.
 * \param data_len the length of the data.
 * \param ret_digest pointer to hold the resulting digest.
 * \param ret_digest_len will hold digest length.
 * \return 0 on success, error code on failure.
 */
int lc_hmac_md5(const uint8 *key, uint32 key_len,
                const uint8 *data, uint32 data_len,
                uint8 **ret_digest, uint32 *ret_digest_len);


/**
 * Generate an HMAC for the given data using SHA256.
 */
int
lc_hmac_sha256(const uint8 *key, uint32 key_len,
               const uint8 *data, uint32 data_len,
               uint8 **ret_digest, uint32 *ret_digest_len);

#ifdef __cplusplus
}
#endif

#endif /* __HMAC_H_ */
