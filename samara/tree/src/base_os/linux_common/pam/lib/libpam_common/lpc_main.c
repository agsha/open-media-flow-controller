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

#include "pam_common.h"

#define CUSTOMER_H_REDIST_ONLY 1
#include "customer.h"

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/sha.h>
#include <inttypes.h>

typedef enum {
    taie_none              =  0,
    taie_version           =  1,
    taie_auth_method       =  2,
    taie_remote_username   =  3,
    taie_local_username    =  4,
    taie_extra_uparams_str =  5,
    taie_boot_id           =  6,
    taie_timestamp_read    =  7,
    taie_timestamp_format  =  8,
    taie_hmac_sha256       =  9,
    taie_encode_b32        = 10,
    taie_rhost             = 11,
    taie_tty               = 12,
    taie_hash_too_long     = 13,
    taie_too_long          = 14,
} lpc_trusted_auth_info_error;

const char *lpc_trusted_auth_info_version = "TAI1";

/* ------------------------------------------------------------------------- */
/* Copied from strlcpy() in bsdstr.c in libcommon.
 */
size_t
lpc_strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}


/* ------------------------------------------------------------------------- */
/* Copied from strlcat() in bsdstr.c in libcommon.
 */
size_t
lpc_strlcat(char *dst, const char *src, size_t siz)
{
    register char *d = dst;
    register const char *s = src;
    register size_t n = siz;
    size_t dlen;
    
    /* Find the end of dst and adjust bytes left but don't go past end */
    while (n-- != 0 && *d != '\0')
        d++;
    dlen = d - dst;
    n = siz - dlen;
    
    if (n == 0)
        return(dlen + strlen(s));
    while (*s != '\0') {
        if (n != 1) {
            *d++ = *s;
            n--;
        }
        s++;
    }
    *d = '\0';
    
    return(dlen + (s - src)); /* count does not include NUL */
}


/* ------------------------------------------------------------------------- */
int
lpc_purify_string(char *str, const char *descr, int max_len,
                  char bad_char, char good_char, char truncate_at_char,
                  int *ret_string_fixed)
{
    int err = 0;
    int slen = 0, orig_slen = 0, i = 0;
    int string_fixed_len = 0, string_fixed_char = 0;
    char *trunc_pos = NULL;

    if (str == NULL) {
        err = 1;
        syslog(LOG_WARNING, "lpc_purify_string(): NULL string passed");
        goto bail;
    }

    if (bad_char != '\0' &&
        (good_char == '\0' || good_char == bad_char)) {
        err = 1;
        syslog(LOG_WARNING, "lpc_purify_string(): invalid bad_char/good_char "
               "passed");
        goto bail;
    }

    slen = strlen(str);
    if (max_len >= 0 && slen > max_len) {
        orig_slen = slen;
        if (truncate_at_char) {
            while ((trunc_pos = strrchr(str, truncate_at_char)) != NULL) {
                *trunc_pos = '\0';
                slen = strlen(str);
                if (slen <= max_len) {
                    break;
                }
            }
            if (trunc_pos == NULL) {
                str[0] = '\0';
            }
        }
        else {
            str[max_len] = '\0';
            slen = max_len;
        }
        string_fixed_len = 1;
        syslog(LOG_WARNING, "%s truncated to meet length limits", descr);
    }

    if (bad_char == '\0') {
        goto bail;
    }

    for (i = 0; i < slen; ++i) {
        if (str[i] == bad_char) {
            str[i] = good_char;
            string_fixed_char = 1;
        }
    }

    if (string_fixed_char) {
        syslog(LOG_WARNING, "%s altered to fix bad characters "
               "(replaced some '%c' characters with '%c')",
               descr, bad_char, good_char);
    }

 bail:
    if (ret_string_fixed) {
        *ret_string_fixed = (string_fixed_len || string_fixed_char);
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
int
lpc_check_string(const char *str, const char *descr, int max_len,
                 char bad_char, int *ret_string_bad)
{
    int err = 0;
    int slen = 0, i = 0;
    int string_bad = 0;

    if (str == NULL) {
        /*
         * Not a problem, it'll just be treated as an empty string.
         */
        goto bail;
    }

    slen = strlen(str);
    if (max_len >= 0 && slen > max_len) {
        syslog(LOG_WARNING, "%s '%s' too long, wanted %d characters, got %d",
               descr, str, max_len, slen);
        string_bad = 1;
    }

    if (bad_char == '\0') {
        goto bail;
    }

    for (i = 0; i < slen; ++i) {
        if (str[i] == bad_char) {
            syslog(LOG_WARNING, "%s '%s' has bad '%c' characters in it",
                   descr, str, bad_char);
            string_bad = 1;
            break;
        }
    }

 bail:
    if (ret_string_bad) {
        *ret_string_bad = string_bad;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
/* Derived from lf_set_nonblocking().
 */
int
lpc_set_nonblocking(int fd, int is_non_blocking)
{
    int err = 0;
    int flags = 0, flags_to_set = O_NONBLOCK;

    if (fd == -1) {
        err = 1;
        goto bail;
    }

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        err = 1;
        goto bail;
    }

    if (is_non_blocking) {
        flags |= flags_to_set;
    }
    else {
        flags &= ~flags_to_set;
    }

    err = fcntl(fd, F_SETFL, flags);
    /* Let err fall through */

 bail:
    return(err);
}


/* ------------------------------------------------------------------------- */
/* This is derived from lc_hmac_sha256() from libcommon (a near copy
 * of it), so we don't have to link with libcommon.  The libcommon one
 * is calling our own implementations of the SHA256_...() functions
 * (which we copied into libcommon so its clients could avoid linking
 * with OpenSSL's libcrypto!), but in this case we'd rather link with
 * libcrypto, and so here we will call it directly.
 */
int
lpc_hmac_sha256(const uint8_t *key, uint32_t key_len,
                const uint8_t *data, uint32_t data_len,
                uint8_t **ret_digest, uint32_t *ret_digest_len)
{
    int err = 0;
    SHA256_CTX ctx;
    uint8_t key_inner[lpc_hmac_block_sha256_length];
    uint8_t key_outer[lpc_hmac_block_sha256_length];
    uint8_t shrunk_key[lpc_hmac_digest_sha256_length];
    uint8_t *digest = NULL;
    uint32_t i = 0;

    if (key == NULL || data == NULL ||
        key_len < lpc_hmac_digest_sha256_length) {
        err = PAM_ERR_GENERIC;
        goto bail;
    }

    digest = (uint8_t *)malloc(lpc_hmac_digest_sha256_length);
    if (digest == NULL) {
        err = PAM_ERR_GENERIC;
        goto bail;
    }

    if (key_len > lpc_hmac_block_sha256_length) {
        SHA256_CTX key_ctx;

        SHA256_Init(&key_ctx);
        SHA256_Update(&key_ctx, key, key_len);
        SHA256_Final(shrunk_key, &key_ctx);

        key = shrunk_key;
        key_len = lpc_hmac_digest_sha256_length;
    }

    memset(key_inner, 0, lpc_hmac_block_sha256_length);
    memset(key_outer, 0, lpc_hmac_block_sha256_length);

    memcpy(key_inner, key, key_len);
    memcpy(key_outer, key, key_len);

    for (i = 0; i < lpc_hmac_block_sha256_length; ++i) {
        key_inner[i] ^= 0x36;
        key_outer[i] ^= 0x5c;
    }

    SHA256_Init(&ctx);
    SHA256_Update(&ctx, key_inner, lpc_hmac_block_sha256_length);
    SHA256_Update(&ctx, data, data_len);
    SHA256_Final(digest, &ctx);

    SHA256_Init(&ctx);
    SHA256_Update(&ctx, key_outer, lpc_hmac_block_sha256_length);
    SHA256_Update(&ctx, digest, lpc_hmac_digest_sha256_length);
    SHA256_Final(digest, &ctx);

    *ret_digest = digest;
    digest = NULL;

    *ret_digest_len = lpc_hmac_digest_sha256_length;

 bail:
    if (digest) {
        free(digest);
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
/* This is derived from lce_encode_b32() from libcommon, but we didn't
 * want to link with libcommon.
 */
int
lpc_encode_b32(const uint8_t *bytes, uint32_t num_bytes, char **ret_str)
{
    int err = 0;
    char *str = NULL;
    int str_len = 0, str_size = 0;
    int curr_byte = 0;
    int curr_bit = 0; /* Bits numbered from 0-7, lsb-msb */
    int num_bits = 0; /* Number of bits to get from current byte */
    int num_bits_extra = 0, new_char_num_extra = 0;
    uint8_t new_char_num = 0;
    char new_char = 0;
    int char_idx = 0;

    if (bytes == NULL || ret_str == NULL) {
        err = PAM_ERR_GENERIC;
        goto bail;
    }

    /*
     * Compute number of Base-32 characters we'll need, rounding up by
     * pre-adding bits-per-byte minus one.
     */
    str_len = ((num_bytes * lpc_bits_per_byte) + (lpc_bits_per_b32_char - 1)) /
        lpc_bits_per_b32_char;
    str_size = str_len + 1; /* For NUL delimiter */

    str = malloc(str_size);
    if (str == NULL) {
        goto bail;
    }
    memset(str, 0, str_size); /* This sets NUL delimiter too */

    /* We'll be filling in the string from the end */
    char_idx = str_len;

    curr_byte = num_bytes - 1;
    curr_bit = 0;
    while (curr_byte >= 0) {
        /* Number of bits to take from the current byte */
        num_bits = lpc_min(lpc_bits_per_b32_char,
                           lpc_bits_per_byte - curr_bit);

        /* The bits we wanted, in the n lowest bits of new_char_num */
        new_char_num = (bytes[curr_byte] >> curr_bit) &
            lpc_bit_masks[num_bits];

        /* Adjust pointers */
        curr_bit += num_bits;
        if (curr_bit >= lpc_bits_per_byte) {
            /* bail_require(curr_bit == lpc_bits_per_byte); */
            curr_bit = 0;
            --curr_byte;
        }

        /* If there's more to fill the current char, get it */
        if (curr_byte >= 0 && num_bits < lpc_bits_per_b32_char) {
            num_bits_extra = lpc_bits_per_b32_char - num_bits;
            new_char_num_extra = (bytes[curr_byte] &
                                  lpc_bit_masks[num_bits_extra]);
            new_char_num |= (new_char_num_extra << num_bits);
            curr_bit += num_bits_extra; /* This will never wrap to next byte */
        }

        /* Now just encode the char and move on! */
        new_char = lpc_b32_num_to_char[new_char_num];

        if (--char_idx < 0) {
            err = PAM_ERR_GENERIC;
            /* 
             * We'll still return the string, which is already
             * NUL-delimited.  Just avoid falling off the beginning
             * of the buffer.
             */
            goto bail;
        }

        str[char_idx] = new_char;
    }

 bail:
    if (ret_str) {
        *ret_str = str;
    }
    return(err);
}


/* ------------------------------------------------------------------------- */
/* XXX/EMT: only reads a single block, assumes we'll get everything.
 * True enough for small files in /proc.
 */
static int
lpc_read_file_single_block(const char *path, char *ret_contents,
                           size_t contents_size)
{
    int err = 0;
    int fd = -1;
    ssize_t nread = 0;
    int numcont = 0;

    if (path == NULL) {
        err = 1;
        goto bail;
    }

    if (ret_contents == NULL || contents_size < 2) {
        err = 1;
        goto bail;
    }

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        err = 1;
        goto bail;
    }

    while (1) {
        errno = 0;
        nread = read(fd, ret_contents, contents_size - 1);
        if (nread < 0) {
            if (errno == EAGAIN && numcont < 100) {
                ++numcont;
                continue;
            }
            else if (errno == EINTR && numcont < 100) {
                ++numcont;
                continue;
            }
            else {
                err = 1;
                goto bail;
            }
        }
        else if ((size_t)nread > contents_size - 1) {
            err = 1;
            goto bail;
        }
        else {
            ret_contents[nread] = '\0';
            break;
        }
    }
    
 bail:
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
    return(err);
}


/* -------------------------------------------------------------------------- */
static void
lpc_tai_error(int code, int fatal)
{
    if (fatal) {
        syslog(LOG_WARNING, "Fatal error constructing TRUSTED_AUTH_INFO "
               "(code %d), bailing out", code);
    }
    else {
        syslog(LOG_WARNING, "Non-fatal error constructing TRUSTED_AUTH_INFO "
               "(code %d)", code);
    }
}


/* ------------------------------------------------------------------------- */
/* We build a string with fields separated by
 * lpc_trusted_auth_info_sep_char.  We don't want to have to worry about
 * escaping.  We also don't expect to ever need to, but we sanity check
 * it all anyway.  Our fields fall into four categories:
 * 
 *   A : must be correct, so if the separator character is found, we
 *       bail out and do not construct TRUSTED_AUTH_INFO.
 *
 *   B : certain it will never include the separator character, since
 *       we are constructing it ourselves.  Nevertheless, for sanity
 *       checking purposes, treat the same as (A).  In some cases, like
 *       the hash, it would ruin everything if we did tamper with it.
 *
 *   C : can replace separator character with a sanitizing character
 *       like '_'.  We complain, but proceed.
 *
 *   D : cannot be sanitized, but is also not critical for TRUSTED_AUTH_INFO.
 *       That is, if we see any problem with it, we just drop this one 
 *       field but proceed with the rest of it.
 *
 * The actual fields, and their types, are:
 *
 *   B   <VERSION>
 *   B   <AUTH_METHOD>
 *   A   <LOGIN_USER>
 *   A   <AUTH_USER>
 *   D   <EXTRA_UPARAMS>
 *   C   <RHOST>
 *   C   <TTY>
 *   C   <BOOT_ID>
 *   C   <TIMESTAMP>
 *   C   <NONCE>
 *   B   <HASH>
 */
int
lpc_get_trusted_auth_info(pam_handle_t *pamh,
                          lpc_auth_method auth_method,
                          const char *login_user,
                          const char *auth_user,
                          char *rhost,
                          char *tty,
                          const char *extra_uparams,
                          char *ret_trusted_auth_info,
                          int trusted_auth_info_size)
{
    int err = 0;
    int string_bad = 0;
    const char *auth_method_str = NULL;
    char boot_id[128], timestamp[128];
    uint64_t nonce = 0;
    char nonce_str[17]; /* 16 hex chars for 64-bit value, plus NUL term. */
    char *delim = NULL;
    char hash_key_padded[LPC_HMAC_DIGEST_SHA256_LENGTH + 1];
    int key_len = 0;
    char trusted_auth_info[LPC_TRUSTED_AUTH_INFO_SIZE];
    char env_buf[LPC_TRUSTED_AUTH_INFO_SIZE + 128];
    uint8_t *digest = NULL;
    uint32_t digest_len = 0;
    char *encoded_hash = NULL;
    size_t len = 0;

    /* ........................................................................
     * Version string
     */
    err = lpc_check_string(lpc_trusted_auth_info_version, "Version",
                           LPC_VERSION_MAX_LEN,
                           lpc_trusted_auth_info_sep_char, &string_bad);
    if (err || string_bad) {
        lpc_tai_error(taie_version, 1);
        err = 1;
        goto bail;
    }

    /* ........................................................................
     * Auth method
     *
     * NOTE: these strings want to be in sync with 
     * MD_AAA_LOCAL_AUTH_METHOD et al. in md_mod_commit.h.
     */
    switch (auth_method) {
    case lpcam_radius:
        auth_method_str = "radius";
        break;

    case lpcam_tacacs:
        auth_method_str = "tacacs+";
        break;

    case lpcam_ldap:
        auth_method_str = "ldap";
        break;

    case lpcam_local:
        auth_method_str = "local";
        break;
        
    default:
        lpc_tai_error(taie_auth_method, 1);
        syslog(LOG_WARNING, "Invalid auth method: %d", auth_method);
        err = 1;
        goto bail;
        break;
    }

    err = lpc_check_string(auth_method_str, "Auth method",
                           LPC_AUTH_METHOD_MAX_LEN,
                           lpc_trusted_auth_info_sep_char, &string_bad);
    if (err || string_bad) {
        lpc_tai_error(taie_auth_method, 1);
        syslog(LOG_WARNING, "Auth method with bad format: %s",
               auth_method_str);
        err = 1;
        goto bail;
    }

    /* ........................................................................
     * Remote and local usernames (LOGIN_USER and AUTH_USER).
     */
    err = lpc_check_string(login_user, "Remote username",
                           LPC_LOGIN_USER_MAX_LEN,
                           lpc_trusted_auth_info_sep_char, &string_bad);
    if (err || string_bad) {
        lpc_tai_error(taie_remote_username, 1);
        err = 1;
        goto bail;
    }

    err = lpc_check_string(auth_user, "Local username",
                           LPC_AUTH_USER_MAX_LEN,
                           lpc_trusted_auth_info_sep_char, &string_bad);
    if (err || string_bad) {
        lpc_tai_error(taie_local_username, 1);
        err = 1;
        goto bail;
    }

    /* ........................................................................
     * Extra user parameters.
     */
    err = lpc_check_string(extra_uparams, "Extra user parameters string",
                           LPC_EXTRA_UPARAMS_MAX_LEN,
                           lpc_trusted_auth_info_sep_char, &string_bad);
    if (err || string_bad) {
        syslog(LOG_NOTICE, "User %s/%s: extra user parameters string '%s' "
               "contained bad character '%c', ignoring entire string",
               login_user, auth_user, extra_uparams,
               lpc_trusted_auth_info_sep_char);
        extra_uparams = "";
    }

    /* ........................................................................
     * Validate lengths and contents of rhost and tty.  We can fix these
     * in place if anything looks wrong.
     */
    err = lpc_purify_string(rhost, "Remote host", LPC_RHOST_MAX_LEN,
                            lpc_trusted_auth_info_sep_char, '_', 0, NULL);
    if (err) {
        lpc_tai_error(taie_rhost, 1);
        goto bail;
    }

    err = lpc_purify_string(tty, "TTY", LPC_TTY_MAX_LEN,
                            lpc_trusted_auth_info_sep_char, '_', 0, NULL);
    if (err) {
        lpc_tai_error(taie_tty, 1);
        goto bail;
    }

    /* ........................................................................
     * Boot ID
     */
    memset(boot_id, 0, sizeof(boot_id));

    err = lpc_read_file_single_block("/proc/sys/kernel/random/boot_id",
                                     boot_id, sizeof(boot_id));
    if (err) {
        lpc_tai_error(taie_boot_id, 0);
        boot_id[0] = '\0';
        err = 0;
    }
    else {
        /*
         * Trim whitespace from the end; there's probably a CR or LF or
         * something...
         */
        while ((len = strlen(boot_id)) > 0) {
            if (isspace(boot_id[len - 1])) {
                boot_id[len - 1] = '\0';
            }
            else {
                break;
            }
        }
    }

    lpc_purify_string(boot_id, "Boot ID", LPC_BOOT_ID_MAX_LEN,
                      lpc_trusted_auth_info_sep_char, '_', 0, NULL);

    /* ........................................................................
     * Timestamp (uptime in seconds)
     */
    memset(timestamp, 0, sizeof(timestamp));

    err = lpc_read_file_single_block("/proc/uptime",
                                     timestamp, sizeof(timestamp));
    if (err) {
        lpc_tai_error(taie_timestamp_read, 0);
        timestamp[0] = '\0';
        err = 0;
    }
    else {
        /*
         * The file looks like "14793388.08 108965006.70\n".  We want to
         * exclude everything past and including the first ' '.
         */
        delim = strchr(timestamp, ' ');
        if (delim == NULL) {
            lpc_tai_error(taie_timestamp_format, 0);
            syslog(LOG_WARNING, "Bad timestamp: %s", timestamp);
            timestamp[0] = '\0';
        }
        else {
            *delim = '\0';
        }
    }

    lpc_purify_string(timestamp, "Timestamp", LPC_TIMESTAMP_MAX_LEN,
                      lpc_trusted_auth_info_sep_char, '_', 0, NULL);

    /* ........................................................................
     * Nonce
     */
    nonce = lpc_random_get_uint64();
    snprintf(nonce_str, sizeof(nonce_str), "%016" PRIx64, nonce);

    lpc_purify_string(nonce_str, "Nonce", LPC_NONCE_MAX_LEN, 
                      lpc_trusted_auth_info_sep_char, '_', 0, NULL);

    /* ........................................................................
     * Construct the TRUSTED_AUTH_INFO string (everything up to the hash).
     */
    snprintf(trusted_auth_info, sizeof(trusted_auth_info),
             "%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s%c%s",

             lpc_trusted_auth_info_version,
             lpc_trusted_auth_info_sep_char,

             auth_method_str,
             lpc_trusted_auth_info_sep_char,

             login_user ? login_user : "",
             lpc_trusted_auth_info_sep_char,

             auth_user ? auth_user : "",
             lpc_trusted_auth_info_sep_char,

             extra_uparams ? extra_uparams : "",
             lpc_trusted_auth_info_sep_char,

             rhost ? rhost : "",
             lpc_trusted_auth_info_sep_char,

             tty ? tty : "",
             lpc_trusted_auth_info_sep_char,

             boot_id,
             lpc_trusted_auth_info_sep_char,

             timestamp,
             lpc_trusted_auth_info_sep_char,

             nonce_str);

    /* ........................................................................
     * Now do the hash
     */
    key_len = strlen(AAA_TRUSTED_AUTH_INFO_SHARED_SECRET);
    memcpy(hash_key_padded, AAA_TRUSTED_AUTH_INFO_SHARED_SECRET,
           lpc_min(key_len, LPC_HMAC_DIGEST_SHA256_LENGTH));
    hash_key_padded[LPC_HMAC_DIGEST_SHA256_LENGTH] = '\0';
    if (key_len < LPC_HMAC_DIGEST_SHA256_LENGTH) {
        memset(hash_key_padded + key_len, ' ',
               LPC_HMAC_DIGEST_SHA256_LENGTH - key_len);
    }

    err = lpc_hmac_sha256(hash_key_padded,
                          strlen((const char *)hash_key_padded),
                          (const uint8_t *)trusted_auth_info,
                          strlen(trusted_auth_info), &digest, &digest_len);
    if (err) {
        lpc_tai_error(taie_hmac_sha256, 1);
        goto bail;
    }

    err = lpc_encode_b32(digest, digest_len, &encoded_hash);
    if (err) {
        lpc_tai_error(taie_encode_b32, 1);
        goto bail;
    }

    /*
     * Since we truncated our inputs above, and knew ahead of time how big
     * our encoded hash would be, we should never have too much data by
     * this point.  Sanity check that.
     */
    if (strlen(trusted_auth_info) + 1 + strlen(encoded_hash) >
        LPC_TRUSTED_AUTH_INFO_MAX_LEN) {
        lpc_tai_error(taie_hash_too_long, 1);
        err = 1;
        goto bail;
    }

    err = lpc_check_string(encoded_hash, "Encoded hash",
                           LPC_ENCODED_HASH_MAX_LEN,
                           lpc_trusted_auth_info_sep_char, &string_bad);
    if (err || string_bad) {
        lpc_tai_error(taie_encode_b32, 1);
        err = 1;
        goto bail;
    }

    lpc_strlcat(trusted_auth_info, lpc_trusted_auth_info_sep_str,
                sizeof(trusted_auth_info));
    lpc_strlcat(trusted_auth_info, encoded_hash,
                sizeof(trusted_auth_info));

    /* ..................................................................... */
    /* Return the value.
     */
    if (strlen(trusted_auth_info) > LPC_TRUSTED_AUTH_INFO_MAX_LEN) {
        lpc_tai_error(taie_too_long, 1);
        err = 1;
        goto bail;
    }

    lpc_strlcpy(ret_trusted_auth_info, trusted_auth_info,
                trusted_auth_info_size);

 bail:
    if (digest) {
        free(digest);
        digest = NULL;
    }
    if (encoded_hash) {
        free(encoded_hash);
        encoded_hash = NULL;
    }
    return(err);
}
