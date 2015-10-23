/*
 *
 * common.h
 *
 *
 *
 */

#ifndef __COMMON_H_
#define __COMMON_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/** \defgroup lc LibCommon: miscellaneous */

/*
 * This can be used to help find bad lc_enum_to_string() invocations.
 * The difficulty is that it also makes it impossible for
 * bn_type_to_str() to work, and that is used all over the place...
 *
 * This would be nicer to put into customer.h, but header file dependency
 * issues make this difficult.
 */
/* #define lc_disallow_enum_to_string */


/* ------------------------------------------------------------------------- */
/** \file src/include/common.h Common data types, conversion functions
 * and other general functions.
 * \ingroup lc
 *
 * TMS atomic data types are defined here in addition to the string map to
 * enum functions.
 *
 * There are various macros and functions that are generally useful
 * for wrapping library and system calls to make the "more safe".
 */

/* Allow us to see if our headers get incorrectly included */
#ifndef TMS_NON_REDIST_HEADERS_INCLUDED
#define TMS_NON_REDIST_HEADERS_INCLUDED 1
#endif

#if defined(NON_REDIST_HEADERS_PROHIBITED) && \
    (defined(TMS_NON_REDIST_HEADERS_INCLUDED) || defined(NON_REDIST_HEADERS_INCLUDED))
#error Non-redistributable header file included when prohibited
#endif

#include "tinttypes.h"
#include "ttypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <netinet/in.h>

/* ------------------------------------------------------------------------- */
/** Library-wide initialization.
 *
 * Calling this is optional, and in fact unnecessary, for
 * single-threaded application.  But it is mandatory for multithreaded
 * applications.  It should be called after lc_log_init() (and the
 * bail_error() on the result from that), but before any other usage
 * of libcommon.
 */
int lc_init(void);


/*
 * When we want to print a number into a string buffer, how large 
 * should that buffer be?  This is comfortably larger than any
 * 128-bit integer or float.
 */
#define NUM_STR_SIZE 50


/* ----------------------------------------------------------------------------
 * Enum / string maps
 *
 * To use these functions, an array of lc_enum_string_map structures
 * is required.  Each one establishes a mapping between an enum value
 * and a string.  NOTE: the array must be terminated with an instance
 * of the structure with a NULL lesm_string, so the conversion
 * functions know when to stop scanning.
 *
 * The lc_signed_enum_string_map is provided separately because gcc
 * chooses the type of enums based on the values declared in them.
 * This prevents us from having a single mechanism that works for
 * all enums (unless we were to throw a negative value into all
 * enums to coerce them to be signed, which would obviously suck).
 */
typedef struct lc_enum_string_map {
    uint32 lesm_enum;
    const char *lesm_string;
} lc_enum_string_map;

typedef struct lc_signed_enum_string_map {
    int32 lsesm_enum;
    const char *lsesm_string;
} lc_signed_enum_string_map;

/**
 * Convert an enum to a string using a map.  If no mapping can be
 * found, log an error and return a generic string ("ERROR-NO-MAP")
 * saying no mapping could be found.
 *
 * NOTE: use of this function is not recommended.  It is a potential
 * source of unattributable error messages -- if no mapping can be
 * found, it logs an error, but then returns nothing that the caller
 * can programmatically distinguish as an error, so it cannot log
 * anything, and the problem can be hard to track down.  Please use
 * lc_enum_to_string_quiet_err() or lc_enum_to_string_quiet_null()
 * instead.
 *
 * \param map The enum to string mapping
 * \param enum_value Which value from the string map to return
 */
#ifndef lc_disallow_enum_to_string
const char *lc_enum_to_string(const lc_enum_string_map map[],
                              uint32 enum_value);
#endif

/**
 * Analog of lc_enum_to_string, except don't log anything if no mapping
 * is found.
 *
 * NOTE: use of this function is not recommended.  It returns nothing that
 * the caller can programmatically distinguish as an error, so bugs are
 * less likely to be noticed.  Please use lc_enum_to_string_quiet_err() or
 * lc_enum_to_string_quiet_null() instead.
 *
 * \param map The enum to string mapping
 * \param enum_value Which value from the string map to return
 */
#ifndef lc_disallow_enum_to_string
const char *lc_enum_to_string_quiet(const lc_enum_string_map map[],
                                    uint32 enum_value);
#endif

/**
 * Analog of lc_enum_to_string_quiet(), except return the string through
 * a parameter so we can also return an error code.  Some callers want
 * to detect the error, but still want a placeholder string to use
 * anyway, rather than a NULL.
 *
 * \param map The enum to string mapping
 * \param enum_value Which value from the string map to return
 * \retval ret_string Mapped string, or "ERROR-NO-MAP" if not found.
 */
int lc_enum_to_string_quiet_err(const lc_enum_string_map map[],
                                uint32 enum_value,
                                const char **ret_string);

/**
 * Analog of lc_enum_to_string, except don't log anything and return
 * NULL instead of a string saying no mapping could be found.
 *
 * \param map The enum to string mapping
 * \param enum_value Which value from the string map to return
 */
const char *lc_enum_to_string_quiet_null(const lc_enum_string_map map[],
                                         uint32 enum_value);

/**
 * Returns the value of the lowest numbered bit set in the bitfield.
 *
 * \param bitfield_value The value of the bit field to be tested
 * \return The value of the lowest numbered bit set in bitfield_value.
 */
uint32 lc_bitfield_first(uint32 bitfield_value);

/**
 * Render a bit field into a string.  This is meant to address
 * the case where the possible entries for the bit field are
 * defined in an enum.  ::cli_command_flags is an example.
 *
 * \param map Enum to string mapping.  This is just like the one you
 * would use for lc_enum_to_string() et al.  Note that this is used for
 * two purposes: (a) to know what bits to look for in the bit field,
 * and (b) to know what string to map them onto if they are found.
 * Note that if this map has a mapping for zero (usually the empty,
 * or "none" enum), it will NOT be included in the result.
 *
 * \param separator A separator string to be inserted between the
 * string representations of each bit.  If NULL is passed "," is
 * used by default.
 *
 * \param flags Option flags to this function.  Currently unused:
 * caller must pass 0.
 *
 * \param bitfield_value The value of the bit field to be rendered.
 *
 * \retval ret_string The string rendering of this bit field.  If the
 * bit field does not have any of the bits listed in the map, the
 * empty string will be returned.
 *
 * \retval ret_num_found The number of bits found to be set in the 
 * bit field.
 */
int lc_bitfield_to_string(const lc_enum_string_map map[],
                          const char *separator, uint32 flags,
                          uint32 bitfield_value, char **ret_string,
                          uint32 *ret_num_found);

/**
 * Analog of lc_enum_to_string, except works on a signed enum map.
 *
 * \param map The enum to string mapping
 * \param enum_value Which value from the string map to return
 */
const char *lc_signed_enum_to_string(const lc_signed_enum_string_map map[],
                                     int32 enum_value);

/**
 * Analog of lc_enum_to_string, except don't log anything and return
 * NULL instead of a string saying no mapping could be found.
 *
 * \param map The enum to string mapping
 * \param enum_value Which value from the string map to return
 */
const char *lc_signed_enum_to_string_quiet_null
    (const lc_signed_enum_string_map map[], int32 enum_value);

/**
 * Convert a string to an enum using a map.  If no mapping can be
 * found, 0 is returned in ret_enum, and the function returns
 * lc_err_bad_type.
 *
 * \param map The enum to string mapping
 * \param string_value The string to map
 *
 * \retval ret_enum The value of the enum associated with the string
 */
int lc_string_to_enum(const lc_enum_string_map map[],
                      const char *string_value, uint32 *ret_enum);

/**
 * Convert a string to an enum using a map, but ignores the case in string
 * comparisons.  If no mapping can be found, 0 is returned in ret_enum, and
 * the function returns lc_err_bad_type.
 *
 * \param map The enum to string mapping
 * \param string_value The string to map
 *
 * \retval ret_enum The value of the enum associated with the string
 */
int lc_string_to_enum_case(const lc_enum_string_map map[],
                           const char *string_value,
                           uint32 *ret_enum);

/**
 * Convert a string to an enum using a signed enum map.  If no mapping
 * can be found, 0 is returned in ret_enum, and the function returns
 * lc_err_bad_type.  See also lc_string_to_enum , which is more common.
 *
 * \param map The enum to string mapping
 * \param string_value The string to map
 *
 * \retval ret_enum The value of the enum associated with the string
 */
int lc_string_to_signed_enum(const lc_signed_enum_string_map map[],
                             const char *string_value, int32 *ret_enum);


/**
 * Convert a string which represents a comma separated list of "flags"
 * into a logically ORd flag bit mask.
 *
 * \param flag_str A comma separated list (in a string)
 * \param flag_map The mapping of enums to strings
 * \retval ret_flags The ORd bit mask
 * \retval ret_flags_ok Indication of success
 */
int lc_extract_flags(const char *flag_str, 
                     const lc_enum_string_map flag_map[],
                     uint32 *ret_flags, tbool *ret_flags_ok);


/**
 * Convert a string which represents a comma separated list of "flags"
 * into a logically ORd flag bit mask.  Similar to lc_extract_flags(),
 * but with extra options.  These options may be useful if a caller
 * is trying to extract flags combined from multiple string maps.
 *
 * \param flag_str A comma separated list (in a string)
 * \param flag_map The mapping of enums to strings
 * \param ignore_missing If flags with no mapping cause an error
 * \param count_zeros If the flag counts include flags equal to 0
 * \retval ret_flags The ORd bit mask
 * \retval ret_num_flags_extracted Count of flags extracted from the string
 * \retval ret_num_flags The total number of flags in the string
 * \retval ret_flags_ok Indication of success
 */
int lc_extract_flags_ex(const char *flag_str,
                        const lc_enum_string_map flag_map[],
                        tbool ignore_missing,
                        tbool count_zeros,
                        uint32 *ret_flags,
                        uint32 *ret_num_flags_extracted,
                        uint32 *ret_num_flags,
                        tbool *ret_flags_ok);

/**
 * Obfuscate the characters of a string.  This function is its own inverse.
 * Note that this is not a security function.
 */
int
lc_obfuscate(char *inout_str);

/**
 * Convert a string into an equivalent tbool value.
 *
 * \param str The string to convert
 * \retval ret_value The value of the converted tbool
 */
int lc_str_to_bool(const char *str, tbool *ret_value);

/**
 * Convert a tbool into an equivalent string
 *
 * \param value The value of the tbool
 * \return The equivalent string
 */
const char *lc_bool_to_str(tbool value);

/**
 * Convert a "friendly string" representation of a boolean 
 * to a boolean.
 *
 * In English, we take anything starting with a 'y' (case insensitive)
 * to be true, and anything starting with an 'n' to be false.  But
 * this can be a tricky topic in other locales where the first
 * character of the words for "yes" and "no" could be a UTF-8
 * character that cannot be entered in the CLI or Wizard; or where the
 * two words might start with the same letter.
 *
 * Our solution is to have two pairs of localizable yes/no strings: 
 * one at full length ("yes" and "no"); and one where each is only
 * a single character long ("y" and "n").  We will accept either a
 * full match against the full strings, or a first-character match
 * against the single-character strings.  The localizer must be 
 * careful to make sure that at least the single character strings
 * can be entered on the keyboard.
 */
int lc_friendly_str_to_bool(const char *str, tbool *ret_value);

/**
 * Convert a boolean to a "friendly string", which is "yes" or "no"
 * in English.
 */
const char *lc_bool_to_friendly_str(tbool value);

/**
 * Convert a boolean to a string containing only a single character.
 * The intent is that if we are asking the user a yes/no question and
 * their answer does not exactly match one of the strings from
 * lc_bool_to_friendly_str(), we'll compare its first character against
 * these characters.  Therefore, they MUST be ASCII-7.
 */
const char *lc_bool_to_friendly_char(tbool value);

/**
 * Return a (potentially localized) string that prompts the user to
 * enter 'yes' or 'no'.  This is a function rather than just an inline
 * string so that all of the yes/no strings are localized in a single
 * string catalog (the one for libcommon), rather than scattered
 * throughout the various components that use them.  Standardization
 * is important here, since if there is any inconsistency, the prompt
 * could end up telling the user to enter different strings than the
 * system would accept.
 */
const char *lc_friendly_bool_prompt(void);


/**
 * Dump the contents of argv to the logs, mainly for debugging.
 */
int lc_dump_argv(int log_level, const char *prefix, int argc, char *argv[]);


/* ----------------------------------------------------------------------------
 * Miscellaneous useful macros
 */

#include "errors.h"

#include "tcompiler.h"

/* 
 * XXX: If you change this, remember to change the copy in tstring.h
 */
/**
 * A wrapper macro to 'free' which only calls 'free' if the variable
 * is not NULL and sets it to NULL after the call.
 */
#define safe_free(var)                                                       \
    do {                                                                     \
        if (var) {                                                           \
            free(var);                                                       \
            var = NULL;                                                      \
        }                                                                    \
    } while(0)

/**
 * A wrapper to 'strdup' which returns NULL if passed NULL (instead of
 * call 'strdup').
 *
 * \param str The string to duplicate
 */
char *safe_strdup(const char *str);

/**
 * A wrapper to 'strcmp' which handles gracefully either string being
 * NULL.
 *
 * \param str1 One string to compare
 * \param str2 The other string to compare
 */
int safe_strcmp(const char *str1, const char *str2);

/**
 * A wrapper to 'strlen' which gracefully handles the string being NULL.
 */
size_t safe_strlen(const char *str);

/**
 * A wrapper to 'close' which closes the given fd (if it is not -1), and
 * sets the caller's copy of the fd to -1.
 */
int safe_close(int *inout_fd);

#ifndef __cplusplus
#define max(a,b) ((a)>(b) ? (a) : (b))
#define min(a,b) ((a)<(b) ? (a) : (b))
#endif

/**
 * Given any two numbers of the same type, compare them, and return one
 * of: -1, 0, or 1 , if the first number is less than, equal to, or
 * greater than, the second number.
 */
#define lc_numeric_cmp(x,y) ({                                          \
            __typeof__(x) _lnc_x = (x);                                 \
            __typeof__(y) _lnc_y = (y);                                 \
            (void) (&_lnc_x == &_lnc_y);                                \
            _lnc_x == _lnc_y ? 0 : ((_lnc_x < _lnc_y) ? -1 : 1); })

/**
 * Convert a hex digit character into a decimal number, or -1 if the
 * character is not a valid hex digit.
 */
#define lc_hex_char_to_dec(x) (((x)>='0'&&(x)<='9') ? (x)-'0' :               \
                               (((x)>='A'&&(x)<='F') ? (x)-'A'+10 :           \
                                (((x)>='a'&&(x)<='f') ? (x)-'a'+10 : -1)))

/**
 * Convert a decimal number in the range [0..15] into a lowercase 
 * hex digit, or '\\0' if the input is out of range.
 */
#define lc_dec_to_hex_char(x) (((x)>=0&&(x)<=9) ? (x)+'0' :                   \
                               ((x)>=10&&(x)<=15) ? (x)-10+'a' : '\0')

/**
 * Given a value, set it to be the min or max (depending) of a range
 * if it falls outside of that range.
 */
#define clamp(n,min,max) (((n)<(min)) ? (min) : (((n)>(max)) ? (max) : (n)))

/**
 * Given a value, and two other values serving as a range, return 'true'
 * if the first value is between the other two (inclusive), and 'false'
 * otherwise.
 */
#define lc_between(n,min,max) ((((n) >= (min)) && ((n) <= (max))) ?           \
                               true : false)

/* NOTE: smprintf() and friends are in smprintf.c NOT in common.c */

/* ------------------------------------------------------------------------- */
/** Render a string treating the arguments as printf() would, but return
 * the result in a dynamically allocated string.  Returns NULL if there
 * was an error.
 *
 * \param format The format string
 * \param ... variable args of various types relating to the format strings.
 */
char *smprintf(const char *format, ...)
     __attribute__ ((format (printf, 1, 2)));

/** Like smprintf() except the arguments come as a va_list */
char *vsmprintf(const char *format, va_list ap)
     __attribute__ ((format (printf, 1, 0)));

/** Like smprintf() except returns length of string created */
int snmprintf(char **ret_result, int32 *ret_result_len, 
              const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));

/** Like vsmprintf() except returns length of string created */
int vsnmprintf(char **ret_result, int32 *ret_result_len, const char *format,
               va_list ap)
     __attribute__ ((format (printf, 3, 0)));



/* ------------------------------------------------------------------------- */
/** Returns true if the first string is a prefix of the second one,
 * false if not.  If ignore_case is false, this works fine on UTF-8
 * strings as well; if you want to ignore case in the comparison,
 * use the tstring functions like ts_has_prefix_str().
 *
 * \param small The substring
 * \param large The string to look in
 * \param ignore_case Controls the matching
 */
tbool lc_is_prefix(const char *small, const char *large, tbool ignore_case);

/* ------------------------------------------------------------------------- */
/** Returns 0 if the first string is a prefix of the second one.
 * Otherwise, a number <0 if the first string is less than the second;
 * and a number >0 if the first string is greater than the second.
 */
int32 lc_cmp_prefix(const char *small, const char *large, tbool ignore_case);

/* ------------------------------------------------------------------------- */
/** Returns true if the first string is a suffix of the second one,
 * false if not.  If ignore_case is false, this works fine on UTF-8
 * strings as well; if you want to ignore case in the comparison, use
 * the tstring functions like ts_has_prefix_str().
 *
 * \param small The substring
 * \param large The string to look in
 * \param ignore_case Controls the matching
 */
tbool lc_is_suffix(const char *small, const char *large, tbool ignore_case);

/* ------------------------------------------------------------------------- */
/** Like memcmp() except the comparison is not case sensitive.
 * \param s1 One data element to compare
 * \param s2 The other data element to compare to
 * \param n How much to compare
 */
int memcasecmp(const void *s1, const void *s2, size_t n);

/* ------------------------------------------------------------------------- */
/** Like strcmp() except the comparison is not case sensitive.
 * Not named strcasecmp() to avoid conflict on systems that have it.
 * \param s1 First string to compare
 * \param s2 Second string to compare
 * \retval -1 if first string is less; 1 if first string is greater;
 * 0 if they are equal.
 */
int strcmpi(const char *s1, const char *s2);


/* ------------------------------------------------------------------------- */
/** Unlimit core dump size, disabling both the hard limit and soft
 * limit.  Unnecessary for anyone managed by PM, but a good idea for
 * anyone else.
 */
int lc_unlimit_coredump_size(void);

/* ------------------------------------------------------------------------- */
/** Set the soft core dump size limit to the hard limit.  This is the best
 * a non-root user can hope to do.
 */
int lc_unlimit_coredump_size_nonroot(void);

/* ------------------------------------------------------------------------- */
/** Set maximum number of file descriptors the process may have open.
 *
 * Note: if this returns an error, errno may be set with a more
 * detailed error code.
 */
int lc_set_max_fd_limit(uint32 max_limit);

/* Pick up strlcpy() and friends */
#include "bsdstr.h"

/* ------------------------------------------------------------------------- */
/** Copy src to character array ret_dst_arr.  At most sizeof(ret_dst_arr)-1
 * characters will be copied.  Always NUL terminates.  Returns 0 on success,
 * or lc_err_no_buffer_space if truncation occurred.  
 * \retval ret_dst_arr Destination array, must be char[] 
 * \param src Source string
 */
#define safe_strlcpy(ret_dst_arr, src)                                  \
    (COMPILE_FAIL_NOT_CHAR_ARRAY(ret_dst_arr),                          \
     (strlcpy(ret_dst_arr, src, sizeof(ret_dst_arr)) >=                 \
      sizeof(ret_dst_arr)) ?                                            \
     lc_err_no_buffer_space : 0)

/* ------------------------------------------------------------------------- */
/** Append src to character array ret_dst_arr .  At most
 * sizeof(ret_dst_arr)-1 characters will be copied.  Always NUL terminates.
 * Returns 0 on success, or lc_err_no_buffer_space if truncation occurred.
 * \retval ret_dst_arr Destination array, must be char[]
 * \param src Source string
 */
#define safe_strlcat(ret_dst_arr, src)                                  \
    (COMPILE_FAIL_NOT_CHAR_ARRAY(ret_dst_arr),                          \
     (strlcat(ret_dst_arr, src, sizeof(ret_dst_arr)) >=                 \
      sizeof(ret_dst_arr)) ?                                            \
     lc_err_no_buffer_space : 0)


#ifdef PROD_TARGET_OS_SUNOS
/* ------------------------------------------------------------------------- */
char *strcasestr(char *haystack, char *needle);

#define strerror_r(e,b,l) (strlcpy(b,strerror(e),l), strerror(e))

#endif

/* ------------------------------------------------------------------------- */
/** Returns the number of elements in a simple C array.  Will not let
 *  itself be called on a pointer or non-array type, including "arrays"
 *  that are function parameters, so it is always safe to use if it
 *  compiles.
 */
#define SAFE_ARRAY_SIZE(a) (COMPILE_FAIL_NOT_ARRAY_NUM(a), \
                            sizeof(a) / sizeof((a)[0]))

/* ------------------------------------------------------------------------- */
/** Development environment detection.  Returns true this is a production
 * environment, false if this is a development environment.
 */
tbool lc_devel_is_production(void);

/* ------------------------------------------------------------------------- */
/** Leak build versioning information.  This makes information about the
 * build present in any valgrind or other memory leak detector output,
 * which allows a developer to tell which build and sources were used
 * for the binary in question.
 *
 * \param keep_ref If a reference should be kept to the leaked memory.
 * A referenced leak will be free'd on any subsequent call with
 * 'keep_ref' true, and a new referenced leak will potentially be made
 * (0 for 'leak_size' means "unleak" with no new leak).  If 'keep_ref'
 * is false, the leak is a new, unreferenced leak, and there's no way to
 * "unleak" it.  
 * \param leak_size The size of the memory buffer to leak, or 0 for no
 * leak.
 */
int lc_leak_build_version_info(tbool keep_ref, unsigned long leak_size);

/* ------------------------------------------------------------------------- */
/* Miscellaneous string/type conversion
 */

#define GNI_MAXHOST 1025 /* Max hostname len from getnameinfo() */
#define GNI_MAXSERV 32   /* Max service len from getnameinfo() */

#include "addr_utils.h"
#include "addr_utils_ipv4u32.h"

static const char lc_macaddr_delim_char = ':';
static const char lc_macaddr_delim_char_alt = '.';

/**
 * Convert a string to a MAC address.
 *
 * \param str Source string.  Four formats are accepted:
 * "11:22:33:44:55:66", "1122:3344:5566", "11.22.33.44.55.66", or
 * "1122.3344.5566".
 *
 * \retval ret_macaddr802 Derived MAC address
 */
int lc_str_to_macaddr802(const char *str, uint8 ret_macaddr802[6]);

/**
 * Convert a MAC address to a string.  The format used is
 * "11:22:33:AA:BB:CC".
 */
int lc_macaddr802_to_str(const uint8 macaddr802[6], char **ret_str);

/**
 * Return the name of the primary interface for the system.
 */
const char *lc_if_primary_name(void);

/** Prefix used for all virtual interface names */
#define md_virt_ifname_prefix   "vif"

/** Prefix used for all virtual bridge names */
#define md_virt_vbridge_prefix  "virbr"

/** 
 * This function checks to see if an interface name is acceptable.  
 * Allows this to be extended by a customer-specific mechanism.
 *
 * \param ifname Candidate interface name
 * \retval ret_ok Indication of acceptability
 */
int lc_ifname_validate(const char *ifname, tbool *ret_ok);

/** 
 * This function checks to see if an interface alias index is acceptable.  
 * Allows this to be extended by a customer-specific mechanism.
 *
 * \param aindex Candidate alias index
 * \retval ret_ok Indication of acceptability
 */
int lc_ifalias_index_validate(const char *aindex, tbool *ret_ok);

/* ------------------------------------------------------------------------- */
/** Access function for password error message.
 */
const char *lc_get_password_invalid_error_msg(void);

/** 
 * This function checks to see if a password is acceptable.  
 * Normally it would decline a password based on its being too
 * easy to guess for some reason.  For now, it imposes no 
 * restrictions of its own, but it allows this to be extended by
 * a customer-specific mechanism.
 *
 * \param password Candidate password
 * \retval ret_ok Indication of acceptability
 */
int lc_password_validate(const char *password, tbool *ret_ok);

/* ------------------------------------------------------------------------- */
/** Check an email address for validity.
 */
/* XXX/EMT: there are other tests we could apply here.  For now, we 
 * just ensure that it doesn't start with a '-', to prevent rogue
 * addresses from masquerading as other command-line options.
 */
int lc_email_addr_validate(const char *addr, tbool *ret_ok);

/* ------------------------------------------------------------------------- */
/** Get window size.  Returns -1 if we cannot get an answer.
 */
int lc_window_size_get(int32 *ret_rows, int32 *ret_cols);

/* ------------------------------------------------------------------------- */
/** Set window size.  Pass -1 for either dimension to leave that one
 * unchanged (and presumably change the other).
 */
int lc_window_size_set(int32 rows, int32 cols);

/* ------------------------------------------------------------------------- */
/** Get the terminal window size from the tty directly.
 * Useful in serial consoles, where other mechanisms fail.
 *
 * Supported platforms: Linux (mostly all flavors)
 * Supported terminals: VT100
 *
 * NOTE: Not tested for portability on other systems or terminals.
 */
int lc_tty_size_get(int *ret_rows, int *ret_cols);

/* ------------------------------------------------------------------------- */
/* Other miscellaneous definitions
 */

/**
 * Arbitrary number to be used by convention as exit code by processes 
 * who do not want their exit to be considered a failure, but simply
 * be relaunched.
 */
#define pm_expected_exit_code      211
#define pm_expected_exit_code_str  "211"

/* ----------------------------------------------------------------------------
 * Other common headers everyone needs.  Put these at the end because 
 * they may have dependencies on stuff earlier in this file.
 */
#include "i18n.h"
#include "bail.h"

/* For dmalloc memory debugging */
#ifdef DMALLOC
#include "dmalloc.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __COMMON_H_ */
