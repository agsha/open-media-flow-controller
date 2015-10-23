/*
 *
 * snmp_crypt.h
 *
 *
 *
 */

#ifndef __SNMP_CRYPT_H_
#define __SNMP_CRYPT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "tstring.h"

/**
 * Hash the specified password using the specified hash algorithm,
 * and return the result in a form palatable to snmpd.conf
 * (i.e. as a hex string).
 *
 * NOTE: this function works by forking an external utility and 
 * capturing its output.
 *
 * \param password The plaintext password to be hashed.
 * \param hash_type The type of hash to use, represented as a string.
 * The valid values here are "md5" and "sha", for MD5 and SHA1,
 * respectively.
 * \retval ret_key The hashed password.
 */
int lc_snmp_encrypt_password(const char *password, const char *hash_type,
                             tstring **ret_key);

int lc_snmp_transform_key(const char *key, const char *hash_type,
                          const char *engine_id, tstring **ret_loc_key);

#ifdef __cplusplus
}
#endif

#endif /* __SNMP_CRYPT_H_ */
