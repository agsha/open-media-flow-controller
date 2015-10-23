/*
 *
 * tcrypt.h
 *
 *
 *
 */

#ifndef __TCRYPT_H_
#define __TCRYPT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "tstring.h"

/* ------------------------------------------------------------------------- */
/** \file tcrypt.h Generate or validate an encrypted password
 * \ingroup lc
 */

typedef enum {
    /*
     * Use the default algorithm, which may be set in customer.h as
     * ltc_default_password_algo .  Note that MD5 is generally
     * considered less of a good security choice than SHA512, and should
     * not be used unless necessary.
     */
    lpa_default = 0,
    lpa_md5 = 1,
    lpa_sha256 = 5,
    lpa_sha512 = 6
} ltc_password_algo;

static const lc_enum_string_map ltc_password_algo_map[] = {
    { lpa_default, "default" },
    { lpa_md5, "md5" },
    { lpa_sha256, "sha256" },
    { lpa_sha512, "sha512" },
    { 0, NULL }
};

/* If no password algorithm is specified, what one will we use? */
#ifndef ltc_default_password_algo
#ifdef PROD_TARGET_ARCH_FAMILY_X86
#define ltc_default_password_algo lpa_sha512
#else
#define ltc_default_password_algo lpa_md5
#endif
#endif

#ifndef ltc_default_password_rounds
#define ltc_default_password_rounds ltc_password_rounds_default
#endif

/*
 * Used for the 'rounds' parameter of ltc_encrypt_password_ex(), to
 * request the build-time default, or the algorithm-specific default,
 * for the number of rounds to iterate the hash.  Has no impact on
 * lpa_md5 .
 */
#define ltc_password_rounds_default (0)
#define ltc_password_rounds_algo_default ((uint32) -1)

/**
 * Generate an encrypted password using MD5, SHA256, or SHA512.
 *
 * \param password The plaintext to encrypt
 * \param algo The password algorithm to use.  The default number of
 * rounds is used.
 * \retval ret_result The encrypted password
 */
int ltc_encrypt_password(const char *password,
                         ltc_password_algo algo,
                         char **ret_result);

/**
 * Generate an encrypted password using MD5, SHA256, or SHA512, and
 * specify a number of rounds to use.
 *
 * \param password The plaintext to encrypt
 * \param algo The password algorithm to use
 * \param rounds The number of rounds to use for SHA256 and SHA512
 * hashing.  0 (ltc_password_rounds_default) means to take the
 * build-time default (ltc_default_password_rounds from customer.h) , -1
 * (ltc_password_rounds_algo_default) means to use the algorithm default
 * (5000 for SHA256 and SHA512).  The max rounds is 999999999.  Ignored
 * for MD5.  Unless there is some good reason, pass 0
 * (ltc_password_rounds_default) or use ltc_encrypt_password().
 * \retval ret_result The encrypted password
 */
int ltc_encrypt_password_ex(const char *password,
                            ltc_password_algo algo,
                            uint32 rounds,
                            char **ret_result);

/**
 * Checks a plaintext password against a hashed password, to see if the
 * hash came from the same plaintext.  This works on hashes that could
 * have been generated from ltc_encrypt_password() .
 *
 * \param password Plaintext password to check.
 * \param hash Hashed password.
 * \retval ret_ok Set to true if hashing the plaintext password with the
 * salt found in the hashed password results in a match with the hashed
 * password; false otherwise.
 */
int ltc_validate_password(const char *password, const char *hash,
                          tbool *ret_ok);

#ifdef __cplusplus
}
#endif

#endif /* __TCRYPT_H_ */
