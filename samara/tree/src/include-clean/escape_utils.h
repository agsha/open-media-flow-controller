/*
 *
 * escape_utils.h
 *
 *
 *
 */

#ifndef __ESCAPE_UTILS_H_
#define __ESCAPE_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


/* ========================================================================= */
/** \file src/include/escape_utils.h Utilities for handling characters that
 * need to be escaped within strings; designed to be able to be linked
 * into third-party binaries (as well as libcommon).
 * \ingroup lc
 *
 * These general purpose set of escape character handling functions are
 * intended to aid in situations where parsers do not deal properly with
 * certain special characters in legitimate strings (e.g. white space,
 * or special parsing delimiters).  This can easily happen with simple
 * parsers that use scanf() or strtok() to break apart a single line
 * with multiple items.
 *
 * The escape and unescape function pair provide for the encoding and
 * decoding of strings using escape sequences, so that strings which
 * would otherwise contain characters used for parsing may be avoided,
 * and parsers will treat the encoded string as a single token.
 *
 * Alternately, they can be used for the kind of escaping where special
 * characters are preceded with a designated escape character (often a
 * backslash), but not otherwise transformed.
 *
 * These functions can be employed at the point where parseable data is
 * about to be generated using lc_escape_str(), and again right after
 * the data has been parsed, using lc_unescape_str() to restore it to
 * its orginal value.
 *
 * These functions are linked into libcommon, but the source file
 * escape_utils.c is designed to have no other dependencies on Samara
 * infrastructure or shared libraries, so it is able to be linked into
 * third-party code which needs to access the same functionality.
 */

/*
 * The following definitions provide commonly used character sets.
 */
#define ESC_CLIST_LOWER_LETTERS   "abcdefghijklmnopqrstuvwxyz"
#define ESC_CLIST_UPPER_LETTERS   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define ESC_CLIST_LETTERS   ESC_CLIST_LOWER_LETTERS ESC_CLIST_UPPER_LETTERS
#define ESC_CLIST_NUMBERS         "0123456789"
#define ESC_CLIST_LOWER_ALPHANUM  ESC_CLIST_LOWER_LETTERS ESC_CLIST_NUMBERS
#define ESC_CLIST_UPPER_ALPHANUM  ESC_CLIST_UPPER_LETTERS ESC_CLIST_NUMBERS
#define ESC_CLIST_ALPHANUM        ESC_CLIST_LETTERS ESC_CLIST_NUMBERS
/*
 * The following definition matches standard POSIX / C character set used by
 * scanf() and isspace()
 */
#define ESC_CLIST_WHITESPACE      " \t\n\r\f\v"


/**
 * Escaping option flags for lc_escape_str().
 * Flags may be ORed together unless noted as mutually exclusive.
 */
typedef enum {
    /**
     * No special options.  Use the default escaping behavior.
     */
    lecf_defaults = 0,

    /**
     * Indicate that the character set provided is a comprehensive list
     * of all characters that should not be escaped.  All other
     * character values will be escaped.  This option is mutually
     * exclusive with lecf_escaped_char_set.
     */
    lecf_non_escaped_char_set = 0 << 0, /* default */

    /**
     * Indicate that the character set provided is a comprehensive list
     * of all characters that need to be escaped.  All other character
     * values will reamain unchanged.  This option is mutually exclusive
     * with lecf_non_escaped_char_set.
     */
    lecf_escaped_char_set = 1 << 0,

    /**
     * By default, when a character is escaped, it is replaced with the
     * escape character followed by a hex representation of the ASCII
     * value of that character.  With this flag, the transformation to
     * hex is skipped; instead, characters needing escaping are simply
     * preceded with the escape character.
     */
    lecf_no_hex_transform = 1 << 1,

} lc_escape_control_flags;


/**
 * Unescaping option flags for lc_unescape_str().
 */
typedef enum {
    /**
     * No special options.  Use the default unescaping behavior.
     */
    lucf_defaults = 0,

    /**
     * Allow an (unescaped) escape character NOT followed by hex digit(s)
     * to be interpreted as plain text and included in the returned output
     * string.  Normally, an error would be returned in this case.
     */
    lucf_allow_escape_char_without_hex_digits = 1 << 0,

    /**
     * Allow an (unescaped) escape character followed by only one hex digit
     * to be interpreted as a single hex digit byte value (nibble) and
     * included in the returned output string decoded .  Normally, an error
     * would be returned in this case.
     */
    lucf_allow_escape_char_with_single_hex_digit = 1 << 1,

    /**
     * The unescaping counterpart to ::lecf_no_hex_transform.  Instead of
     * expecting to find a two-character hex ASCII code after the escaping
     * character, we simply expect to find the original character itself.
     *
     * Note that this flag is mutually exclusive with
     * ::lucf_allow_escape_char_without_hex_digits and
     * ::lucf_allow_escape_char_with_single_hex_digit, since those 
     * are relevant only to the other sort of escaping involving hex digits.
     */
    lucf_no_hex_transform = 1 << 2,

} lc_unescape_control_flags;

typedef unsigned long int lc_escape_control_flags_bf;

typedef unsigned long int lc_unescape_control_flags_bf;


/* Escape utility return codes */
typedef enum {
    /* Positive values reserved for char position of problem in input string */
    leuec_err_no_error = 0,
    leuec_err_null_input = -1,
    leuec_err_invalid_param = -2,
    leuec_err_assertion_failed = -3,
    leuec_err_escape_char_in_exclusion_set = -4,
    leuec_err_no_buffer_space = -5,

    leuec_err_mem_alloc = -99
} lc_escape_util_error_codes;


/**
 * Generates a new string from instr that contains escape sequences for
 * all "unsafe" characters that are not allowed to be in the string.  An
 * escape sequence is defined as the escape_char followed by a two hex
 * digits containing the ASCII value of the character that has been
 * escaped.  The escape character will itself be escaped automatically.
 *
 * If ::lecf_no_hex_transform is used, instead of an escape sequence,
 * we simply use the escape character followed by the original character.
 *
 * \param instr The string to be replicated into an escaped character
 * string sequence.
 *
 * \param escape_char The escape character to be used as the escape
 * sequence prefix for unsafe characters.  The escape character may be
 * any value other than zero, since zero is a string terminator.
 *
 * \param options Option flags defined above in lc_escape_control_flags.
 * The lecf_non_escaped_char_set and lecf_escaped_char_set fields are
 * mutually exclusive.  These indicate, respectively, whether the
 * char_set provided contains the complete list of "safe" characters
 * that should not be escaped, or alternatively, the blacklist set of
 * all unsafe characters that need to be escaped.  By default
 * (e.g. lecf_defaults), the character set is assumed to be a the safe
 * character set.
 *
 * \param char_set Pointer to a string containing a complete list of
 * characters that either should or should not be escaped, depending on
 * the option_flags.  The default is that it is a list of characters
 * that should not be escaped.  Note that the escape character itself
 * will always be escaped in the output string, so if the set contains
 * characters to be escaped, it is OK to omit the escape character from
 * the set.  In contrast, if this set contains safe characters, it MUST
 * NOT contain the escape character itself, or an error will be
 * returned.
 *
 * \retval ret_escaped_str The resulting escaped character string.  This
 * memory is allocated during the call and must be freed by the caller.
 *
 * \return Returns 0 if successful, nonzero on failure due to anything
 * invalid about the input, or due to malloc failure.  On failure, any
 * allocated memory is automatically freed prior to return.
 */
int lc_escape_str(const char *instr,
                  int escape_char,
                  lc_escape_control_flags_bf options,
                  const char *char_set,
                  char **ret_escaped_str);


/**
 * Identical to lc_escape_str, except that the return value is the
 * allocated escaped string, or NULL if an error occured.
 */
char *lc_escape_str_quick(const char *instr,
                          int escape_char,
                          lc_escape_control_flags_bf options,
                          const char *char_set);


/**
 * Generates a new, plain-text string from the escaped string instr,
 * where instr is a string containing any mix of plain characters and
 * escape sequences of the form "EXX", where E is the specifed
 * escape_char, and XX is a two digit upper-case hex value that
 * specifies an ASCII character that has been escaped.  This function
 * reverses the escaping performed by lc_escape_str().
 *
 * If ::lucf_no_hex_transform is used, instead of an escape sequence,
 * we take the next character after the escape character as a literal.
 *
 * \param instr The string containing a mixture of escape sequences and
 * plain text to be replicated into the restored, plain-text character
 * string.
 *
 * \param escape_char The escape character original used to create the
 * escaped string sequence.
 *
 * \param options Option flags defined above in
 * lc_unescape_control_flags.  There are no special options implemented
 * yet, so this parameter is currently ignored, however use
 * lucf_defaults to protect against future changes.
 *
 * \retval ret_unescaped_str The resulting unescaped character string.
 * This memory is allocated during the call and must be freed by the
 * caller.
 *
 * \return Returns 0 if successful, nonzero on failure due to anything
 * invalid about the input, or due to malloc failure.  On failure, any
 * allocated memory is automatically freed prior to return.
 */
int lc_unescape_str(const char *instr,
                    int escape_char,
                    lc_unescape_control_flags_bf options,
                    char **ret_unescaped_str);


/**
 * Identical to lc_unescape_str, except that the return value is the
 * allocated escaped string, or NULL if an error occured.
 */
char *lc_unescape_str_quick(const char *instr,
                            int escape_char,
                            lc_unescape_control_flags_bf options);

#ifdef __cplusplus
}
#endif

#endif /* __ESCAPE_UTILS_H_ */
