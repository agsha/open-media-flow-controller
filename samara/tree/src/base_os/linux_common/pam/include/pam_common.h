/*
 *
 * (C) Copyright 2013-2015 Juniper Networks, Inc. All Rights reserved.
 * (C) Copyright 2002-2013 Tall Maple Systems, Inc.
 * All rights reserved.
 * The contents of this file are subject to the terms of the MIT License.
 * You may not use this file except in compliance with the License.
 * See the file License.txt at the top of this source tree.
 * You can obtain a copy of the license at https://opensource.org/licenses/MIT
 *
 */

#ifndef __PAM_COMMON_H_
#define __PAM_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <sys/types.h>
#include <security/pam_modutil.h>

/*
 * Chosen to be negative so we don't overlap with SQLite's error codes.
 */
#define PAM_ERR_GENERIC -1

/*
 * These need to match lc_hmac_block_sha256_length and
 * lc_hmac_digest_sha256_length since those are what mgmtd uses
 * to validate these hashes.
 */
static const uint32_t lpc_hmac_block_sha256_length = 64;
static const uint32_t lpc_hmac_digest_sha256_length = 32;

static const int lpc_bits_per_b32_char = 5;
static const int lpc_bits_per_byte = 8;
static const uint8_t lpc_bit_masks[] = {0x00, 0x01, 0x03, 0x07,
                                        0x0f, 0x1f, 0x3f, 0x7f, 0xff};

static const int lpc_hash_trunc_bits = 256;
#define LPC_HMAC_DIGEST_SHA256_LENGTH 32

#define LPC_TRUSTED_AUTH_INFO_SIZE    1024
#define LPC_EXTRA_UPARAMS_SIZE         512

#define LPC_TRUSTED_AUTH_INFO_MAX_LEN   (LPC_TRUSTED_AUTH_INFO_SIZE - 1)
#define LPC_EXTRA_UPARAMS_MAX_LEN       (LPC_EXTRA_UPARAMS_SIZE - 1)

/*
 * Copied from b32_num_to_char in encoding.c.
 */
static const char lpc_b32_num_to_char[] =
    "0123456789ABCDEFGHJKLMNPQRTUVWXY";

/*
 * Separator character for TRUSTED_AUTH_INFO.
 *
 * Note some of the other special characters which may occur in the
 * fields of TRUSTED_AUTH_INFO, and so would have been unsuitable for
 * the separator character:
 *   '+' may be in auth method
 *   '-' may be in usernames, rhost, boot ID
 *   '.' may be in usernames, rhost
 *   '=' may be in extra user parameters
 *   ',' may be in extra user parameters
 *   ':' may be in rhost (at least for IPv6 addresses)
 *   '%' may be in rhost (at least for IPv6 addresses with zone ID)
 *   '/' may be present in TTY
 */
static const char lpc_trusted_auth_info_sep_char = '|';
static const char lpc_trusted_auth_info_sep_str[] = "|";

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t lpc_strlcpy(char *dst, const char *src, size_t siz);

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t lpc_strlcat(char *dst, const char *src, size_t siz);

/*
 * Check a string to make sure it is acceptable.  If not, make it so.
 * We can check and enforce two constraints:
 *
 *   1. Maximum number of characters.  If the string is too long,
 *      we truncate.  If there is no maximum length to be enforced,
 *      pass -1 for max_len.
 *
 *   2. That there are no occurrences of a specific character in the
 *      string.  (The plan is that we are going to embed this string
 *      into a larger string, with this character as a separator, with
 *      no escaping.)  If the character is found, it is replaced with
 *      good_char.  If there is no bad character to avoided, pass 0
 *      for bad_char.
 *
 * \param str String to be checked and fixed.
 * \param descr Description of what the string contains, to be used in the
 * log message if we have to fix it.
 * \param max_len Maximum permitted length of the string, or -1 for no limit.
 * \param bad_char Which character is illegal in this string, or '\0' for
 * no illegal character.
 * \param good_char If we find bad_char, what char should we replace it 
 * with?  If bad_char is '\0', this may be '\0'.  Otherwise, this may not
 * be '\0', nor may it match bad_char.
 * \param truncate_at_char If we have to truncate the string to keep it
 * within the specified maximum length, are there any limitations on where
 * we can truncate it?  If this is '\0', we can truncate anywhere.  Otherwise,
 * we will only truncate at this character (or at zero length if there is no
 * suitable place to truncate).  This is useful e.g. for lists of items where
 * we want only whole items.
 * \retval ret_string_fixed Returns 0 if the string was fine and therefore
 * not modified; 1 if the string needed to be modified and we did so.
 */
int lpc_purify_string(char *str, const char *descr, int max_len,
                      char bad_char, char good_char, char truncate_at_char,
                      int *ret_string_fixed);

/*
 * Like lpc_purify_string, except read-only: just check, but don't change 
 * anything.  If a problem is found, log a warning and return 1 in
 * 'ret_string_bad'.
 */
int lpc_check_string(const char *str, const char *descr, int max_len,
                     char bad_char, int *ret_string_bad);

#define lpc_min(x,y) ( (x)<(y)?(x):(y) )

int lpc_hmac_sha256(const uint8_t *key, uint32_t key_len,
                    const uint8_t *data, uint32_t data_len,
                    uint8_t **ret_digest, uint32_t *ret_digest_len);

int lpc_encode_b32(const uint8_t *bytes, uint32_t num_bytes, char **ret_str);

typedef enum {
    lpcam_none = 0,
    lpcam_radius,
    lpcam_tacacs,
    lpcam_ldap,
    lpcam_local
} lpc_auth_method;

int lpc_set_nonblocking(int fd, int is_non_blocking);

uint64_t lpc_random_get_uint64(void);

/*
 * Construct a string which is to be set into the TRUSTED_AUTH_INFO
 * environment variable.  We just return the string and let our caller
 * call pam_putenv().  (We don't want to require everyone linking with
 * this library to link with libpam, e.g. the pam_tallybyname binary.)
 *
 * The format is a set of fields separated by
 * lpc_trusted_auth_info_sep_char characters.  We ensure that no field
 * contains this character in its value.  Some fields may be empty, which
 * does not change the format; there is still a placeholder (i.e. two or
 * more separator characters may be next to each other).
 *
 * The fields are:
 *
 * <VERSION> is "TAI1" for now.  If the format changes, this number should be
 * increased.  This is just a precaution, but should not be necessary as
 * it should always be the same version of mgmtd and the PAM modules running
 * at any given time.
 *
 * <AUTH_METHOD> is a string describing the authentication method.
 * It can be "radius", "tacacs+", "ldap", or "local" (the same set of 
 * values that are accepted for /aaa/auth_method/ * /name).  These
 * strings are defined as md_aaa_*_auth_method in mdmod_common.h.
 *
 * <LOGIN_USER> is whatever string we set into the LOGIN_USER env var.
 * This is the username originally given by the user; also known as the
 * remote username in the case of remote authentication.
 *
 * <AUTH_USER> is whatever string we set into the AUTH_USER env var.
 * This is the local username as whom the user is going to operate.
 *
 * <EXTRA_UPARAMS> is a comma-delimited list of user parameters which we
 * got from the auth server (provided we are configured to accept it).
 *
 * <RHOST> is the contents of the PAM_RHOST item, the string representing
 * the remote host address from which the user is logging in.  This item
 * is given to PAM by the client overseeing the authentication.
 *
 * <TTY> is the contents of the PAM_TTY item, the string representing the
 * TTY on which the user is logging in.  This item is given to PAM by the
 * client overseeing the authentication.
 *
 * <BOOT_ID> is the contents of /proc/sys/kernel/random/boot_id.
 * The TRUSTED_AUTH_INFO will not be honored if its boot ID does not
 * match the current one at the time it is presented.
 *
 * <TIMESTAMP> is the first number in /proc/uptime, which is a number of
 * seconds since the system was booted, typically (always?) with two
 * digits of precision after the decimal point.  This is used for
 * informational purposes, but not to validate the TRUSTED_AUTH_INFO,
 * since in some cases there may be a legitimate delay between its
 * generation and its use.
 *
 * <NONCE> is a 64-bit random number in lowercase ASCII hex (therefore
 * 16 characters).
 *
 * <HASH> is an HMAC SHA256 hash, rendered as Base32.  It is calculated
 * on the portion of the TRUSTED_AUTH_INFO string up to but NOT including
 * the separator character preceding the hash.  It is keyed using
 * AAA_TRUSTED_AUTH_INFO_SHARED_SECRET from customer.h.
 *
 * The whole string is limited to 1023 (LPC_TRUSTED_AUTH_INFO_MAX_LEN)
 * characters.  Breakdown:
 *   -  10 for colons to separate the 11 fields
 *   -   5 for version
 *   -  15 for auth method
 *   -  63 for login_user
 *   -  31 for auth_user
 *   -  63 for rhost
 *   -  31 for TTY
 *   - 512 for extra user parameters (see below)
 *   -  36 for boot ID
 *   -  20 for timestamp (may be shorter)
 *   -  16 for the nonce
 *   -  52 for the HMAC hash in base32
 *   - 177 reserved for future use
 *
 * All the other space requirements add up to 342 characters, leaving a
 * maximum of 681 characters for the list of extra user parameters (like
 * ACL roles).  We'll limit it to 512 characters now, to leave some room
 * (169 characters) for future expansion of the remainder of the value.
 *
 * Note that rhost and tty are non-const because we check for bad
 * characters and fix any that we find in place.  The other strings are
 * checked too, but if we find any bad characters in them, we just fail.
 */
int lpc_get_trusted_auth_info(pam_handle_t *pamh,
                              lpc_auth_method auth_method,
                              const char *login_user,
                              const char *auth_user,
                              char *rhost,
                              char *tty,
                              const char *extra_uparams,
                              char *ret_trusted_auth_info,
                              int trusted_auth_info_size);

#define LPC_VERSION_MAX_LEN        5
#define LPC_AUTH_METHOD_MAX_LEN   15
#define LPC_LOGIN_USER_MAX_LEN    63
#define LPC_AUTH_USER_MAX_LEN     31
#define LPC_RHOST_MAX_LEN         63
#define LPC_TTY_MAX_LEN           31
#define LPC_BOOT_ID_MAX_LEN       36
#define LPC_TIMESTAMP_MAX_LEN     20
#define LPC_NONCE_MAX_LEN         16
#define LPC_ENCODED_HASH_MAX_LEN  52

#ifdef __cplusplus
}
#endif

#endif /* __PAM_COMMON_H_ */
