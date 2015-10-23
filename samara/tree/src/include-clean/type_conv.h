/*
 *
 * type_conv.h
 *
 *
 *
 */

#ifndef __TYPE_CONV_H_
#define __TYPE_CONV_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "tstring.h"
#include "tbuffer.h"

/**
 * \file src/include/type_conv.h Functions to convert between various
 * data types and representations.
 * \ingroup lc
 */

/*
 * XXX/EMT: there are various other functions that should go in
 * here, mainly from common.h.
 */

/**
 * Convert binary data into a user-readable string containing the data
 * represented as hex values.  The length is given at the start of the
 * string.
 *
 * \param buf The buffer of binary data to be rendered.
 * \retval ret_str The user-readable string produced.
 */
int lc_format_binary(const tbuf *buf, tstring **ret_str);

/**
 * Flags for lc_format_binary_buf_ex().
 */
typedef enum {
    lfbf_none = 0,

    /** Don't attempt to render as a character */
    lfbf_no_chars = 1 << 0,

} lc_format_binary_flags;

/** Bit field of ::lc_format_binary_flags ORed together. */
typedef uint32 lc_format_binary_flags_bf;

/**
 * Convert binary data into a user-readable string containing the data
 * represented as hex values.
 */
int lc_format_binary_buf_ex(const uint8 *buf, uint32 buf_len, 
                            lc_format_binary_flags_bf flags, 
                            tstring **ret_str);

/**
 * Convert binary data into a user-readable string containing the data
 * represented as base64 encoded values (like base64binary).
 *
 * \param buf The buffer of binary data to be rendered.
 * \retval ret_str The user-readable string produced.
 */

int lc_format_binary_base64(const tbuf *buf, tstring **ret_str);

/**
 * Option flags for lc_string_to_hex().
 */
typedef enum {
    sho_none = 0,

    /** Prefix the string with "0x" to indicate a hexademical # follows */
    sho_prefix = 1 << 0,

    /** Include the NUL terminating character in the string (as "00") */
    sho_null_term = 1 << 1,

    /** Use uppercase characters for the letters A-F */
    sho_uppercase = 1 << 2,

    /** Separate each byte with a space */
    sho_space_per_byte = 1 << 3,

} lc_string_to_hex_options;

/* Bit field of ::lc_string_to_hex_options ORed together */
typedef uint32 lc_string_to_hex_options_bf;

/**
 * Convert a string into a hex string which represents the ASCII bytes
 * from the first string.  By default, we use lowercase characters for
 * the letters a-f.  This is similar to what lc_format_binary() does,
 * but with a different output format.
 *
 * e.g. "Hello" would become "48656c6c6f" with default parameters;
 * "0x48656c6c6f" with sho_prefix set; "48656c6c6f00" with
 * sho_null_term set; and "48656C6C6F" with sho_uppercase set.
 *
 * \param str String to be converted.
 * \param opts A bit field of #lc_string_to_hex_options ORed together.
 * \retval ret_hex_str The converted string.
 */
int lc_string_to_hex(const char *str, lc_string_to_hex_options_bf opts,
                     tstring **ret_hex_str);

/**
 * Convert a buffer into a hex string.  Same as lc_string_to_hex(),
 * except it takes its input as a tbuf so you are not limited to 
 * a series of bytes that is string-legal.  Note that while the
 * sho_null_term flag is still honored, it doesn't make much sense
 * in this context, since there could also be NUL characters in
 * the original tbuffer.
 */
int lc_buffer_to_hex(const tbuf *buf, lc_string_to_hex_options_bf opts,
                     tstring **ret_hex_str);

/**
 * Convert a string, which supposedly holds a hexadecimal representation
 * of a series of bytes, into a tbuffer.
 * \param hex_str String to convert.  A leading "0x" is permitted but
 * not required, and will be ignored if present.  Hex digits 'a'-'f' may
 * be either capital or lowercase.  Since we are extracting full bytes,
 * there must be an even number of hex digits.
 * \retval ret_buffer The buffer produced from the hex characters.
 * If any invalid characters were found in the string, a buffer will
 * still be returned, containing any converted characters that came
 * before the invalid ones, if any.  If there were no valid characters
 * before the first invalid one was encountered, the buffer will be
 * empty.
 * \return Zero for success; nonzero for failure.  If any invalid
 * characters are found, lc_err_bad_type is returned, even though
 * the return buffer is also partially populated.
 */
int lc_hex_to_buffer(const char *hex_str, tbuf **ret_buffer);

/**
 * Convert a string, which supposedly holds a hexadecimal representation of
 * a series of bytes, into a tstring containing a valid hex string padded
 * with zeros if padding is required to meet the specified hex digit_count.
 *
 * \param hex_str String to convert.  A leading "0x" is permitted but
 * not required, and will be perserved if present, but ignored in the 
 * digit_count.  Hex digits 'a'-'f' may be either capital or lowercase.
 * \param digit_count The number of total number of hex digits expected  
 * if the number is to be padded.  The "0x" prefix, if present, does not
 * count as part of the number.  Use 0 if no padding is desired.
 * \retval ret_ts The tstring produced from the hex characters.
 * If any invalid characters were found in the string, a tstring will
 * still be returned, containing any converted characters that came
 * before the invalid ones, if any.  If there were no valid characters
 * before the first invalid one was encountered, the tstring will be
 * empty.
 * \return Zero for success; nonzero for failure.  If any invalid
 * characters are found, lc_err_bad_type is returned, even though
 * the return tstring may be partially populated.
 */
int lc_hex_str_to_padded_str(const char *hex_str, uint32 digit_count,
                             tstring **ret_ts);

#define lc_is_uppercase_hex_digit(x) ((((x) >= '0') && ((x) <= '9')) ||       \
                                      (((x) >= 'A') && ((x) <= 'F')))

#define lc_is_lowercase_hex_digit(x) ((((x) >= '0') && ((x) <= '9')) ||       \
                                      (((x) >= 'a') && ((x) <= 'f')))

#define lc_is_hex_digit(x) (lc_is_uppercase_hex_digit(x) ||                   \
                            lc_is_lowercase_hex_digit(x))

int lc_oid_to_string(uint32 oid[], uint32 oid_len, tbool oid_abs,
                     char **ret_oid_str);

int lc_string_to_oid(const char *oid_str, uint32 oid_max_len,
                     uint32 ret_oid[], uint32 *ret_oid_len,
                     tbool *ret_oid_abs);


/* ========================================================================= */
/** @name Converting strings to unsigned integers
 *
 * These functions should be used in place of the strtoul() and strtoull()
 * functions for several reasons, as they:
 *
 *   - Employ known storage sizes to avoid platform-specific size differences
 *     that can lead to unexpected results.
 *
 *   - Avoid demotion of the return value to a variable of smaller storage size
 *     that will result in silent loss of the real value (this issue is not 
 *     caught by the compler).
 *
 *   - Avoid a known issue where the strtoul() and strtoull() functions 
 *     silently ignore signed strings.
 *
 *   - Provide convenience, standardization, code reuse, and avoidance of 
 *     errors.
 *
 *   - Support variations that allow alternative range limitations to be 
 *     imposed.
 *
 * In general, the parameters and return value are as follows:
 *
 *   - str: Pointer to a string expected to contain an unsigned integer.
 *
 *   - ret_num: Pointer to a storage variable of the specified unsigned
 *     integer size to recieve the converted value if valid.
 *
 *   - min (_range variants only): The minimum valid value.
 *
 *   - max (_range variants only): The maximum valid value.
 *
 *   - flags (_range variants only): Reserved for future use.  The caller
 *     must pass 0 here.
 *
 *   - base (_range_base variants only): The base with which to interpret
 *     the number.  This is subject to the rules and limitations of the
 *     'base' parameter to strtol().
 *
 *   - Return value: Zero for success, or lc_err_bad_type if the
 *     string does not contain a valid unsigned integer value, and
 *     lc_err_range if it does but the value is not within range for the
 *     storage class or the range specified.
 */

/*@{*/

int lc_str_to_uint8(const char *str, uint8 *ret_num);
int lc_str_to_uint16(const char *str, uint16 *ret_num);
int lc_str_to_uint32(const char *str, uint32 *ret_num);
int lc_str_to_uint64(const char *str, uint64 *ret_num);

int lc_str_to_uint8_range(const char *str,
                          uint8 min, uint8 max,
                          uint32 flags,
                          uint8 *ret_num);
int lc_str_to_uint16_range(const char *str,
                           uint16 min, uint16 max,
                           uint32 flags,
                           uint16 *ret_num);
int lc_str_to_uint32_range(const char *str,
                           uint32 min, uint32 max,
                           uint32 flags,
                           uint32 *ret_num);
int lc_str_to_uint64_range(const char *str,
                           uint64 min, uint64 max,
                           uint32 flags,
                           uint64 *ret_num);

int lc_str_to_uint32_range_base(const char *str,
                                uint32 min, uint32 max,
                                uint32 flags,
                                uint16 base,
                                uint32 *ret_num);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* __TYPE_CONV_H_ */
