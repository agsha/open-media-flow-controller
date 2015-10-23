/*
 *
 * base64.h
 *
 *
 *
 */

#ifndef __BASE64_H_
#define __BASE64_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"

/* ------------------------------------------------------------------------- */
/** \file src/include/base64.h Base64 encode and decode of byte streams.
 * \ingroup lc
 */

/**
 * Base64 encode the given sequence of bytes.
 * \param bytes bytes to encode.
 * \param size number of bytes in the bytes parameter.
 * \param ret_bytes the resulting encoded bytes null terminated.
 */
int lc_base64_encode(const uint8 *bytes, uint32 size, char **ret_bytes);

/**
 * Base64 decode the given sequence of bytes.
 * \param bytes bytes to decode (null terminated).
 * \param ret_bytes the resulting decoded bytes null terminated.
 * \param ret_size the size of the decoded bytes.
 */
int lc_base64_decode(const char *bytes, uint8 **ret_bytes, uint32 *ret_size);


#ifdef __cplusplus
}
#endif

#endif /* __BASE64_H_ */
