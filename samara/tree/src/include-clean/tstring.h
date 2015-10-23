/*
 *
 * tstring.h
 *
 *
 *
 */

#ifndef __TSTRING_H_
#define __TSTRING_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/* ------------------------------------------------------------------------- */
/** \file tstring.h String manipulation library for ASCII or UTF-8 strings.
 * \ingroup lc_ds
 *
 * This library module provides fast string manipulation routines for use
 * with both UTF8 and 7 bit ASCII strings (ASCII-7).
 *
 * It automatically grows allocated buffers, and avoids length scans by
 * storing the string length internally.
 *
 * UTF-8 string usage notes:
 *
 *   - Strings are created as ASCII (not UTF-8) by default,
 *     unless (a) PROD_FEATURE_I18N support is enabled, AND
 *     (b) you explicitly set them or create them as UTF-8.
 *
 *   - Some functions below take a character as an integer, including
 *     ts_trim(), ts_append_char(), ts_put_char(), ts_insert_char(), 
 *     ts_remove_char(), ts_tokenize_str(), ts_join_tokens(), ts_find_char(),
 *     ts_rfind_char(), and ts_has_prefix_char().  If the string is
 *     ASCII-7, any 8-bit character is accepted here: [0x00-0xFF].
 *     If the string is marked as UTF-8, only 7-bit ASCII [0x00-0x7F]
 *     is accepted, since anything beyond that range would be either 
 *     part of a multibyte character, or invalid.
 *
 *   - Some functions below operate on string fragments: a specified
 *     range of characters from a C string, including ts_append_str_frag
 *     and ts_put_str_frag.  These strings are assumed to be ASCII-7
 *     strings, as reflected in the parameter name a7_str.  There are
 *     separate versions of these strings, ..._utf8_str_frag(), which
 *     take UTF-8 C strings.  Note that there are not separate UTF-8
 *     forms of functions which operate on entire C strings or byte
 *     ranges, since ASCII-7 and UTF-8 are treated identically there.
 *
 *   - Functions which take a UTF-8 character as const char *uch,
 *     such as ts_append_char_utf8(), do not assume that there is
 *     a NUL delimiter after the UTF-8 character.  uch may be any
 *     UTF-8 string, and we will just take the first character of it.
 */

/**
 * \struct tstring
 * The Tall Maple string wrapper.
 */

/*
 */

#include "common.h"

typedef struct tstring tstring;

#include "tstr_array.h"

#define utf8_char_size_max 6

/* ========================================================================= */
/** @name Create, copy, clear, delete */
/*@{*/

int ts_new(tstring **ret_ts);

int ts_new_utf8(tstring **ret_ts);

int ts_new_size(tstring **ret_ts, uint32 initial_size);

int ts_new_size_utf8(tstring **ret_ts, uint32 initial_size);

int ts_new_str(tstring **ret_ts, const char *str);

int ts_new_str_utf8(tstring **ret_ts, const char *str);

int ts_new_sprintf(tstring **ret_ts, const char *format, ...)
     __attribute__ ((format (printf, 2, 3)));

int ts_new_sprintf_utf8(tstring **ret_ts, const char *format, ...)
     __attribute__ ((format (printf, 2, 3)));

/** 
 * Create a new string by taking over an existing ASCII-7 string.  
 * \retval ret_ts The newly-created tstring
 * \param inout_str A reference to the ASCII-7 string to be taken over.
 * Note that your pointer will be set to NULL by this function,
 * to keep memory ownership clear.
 * \param str_len The length of the string you are passing, or -1 if you
 * don't know so we should call strlen().  Since this is an
 * ASCII-7 string, the length is both the number of characters
 * and length in bytes.
 * \param str_size The size of the dynamic memory block allocated for
 * the string, or -1 if you don't know so we should assume it is
 * only exactly large enough to hold its current contents.
 */
int ts_new_str_takeover(tstring **ret_ts, char **inout_str, 
                        int32 str_len, int32 str_size);

/**
 * Same as ts_new_str_takeover() except we assume the string given to
 * us is UTF-8.
 * \retval ret_ts The newly-created tstring
 * \param inout_str A reference to the UTF-8 string to be taken over.
 * \param str_len The length of the string in bytes, or -1.
 * This is not used for the number of characters: that will be
 * computed by scanning the string.
 * \param str_size The size of the dynamic memory block allocated
 * for the string, or -1.
 */
int ts_new_str_takeover_utf8(tstring **ret_ts, char **inout_str, 
                             int32 str_len, int32 str_size);

/** Tell us whether a string is UTF-8 */
int ts_get_utf8(const tstring *ts, tbool *ret_is_utf8);

/** By default tstrings are ASCII-7, and UTF 8 functions will fail. */
int ts_set_utf8(tstring *ts, tbool is_utf8);

int ts_free(tstring **inout_ts);

/* 
 * XXX: If you change this, remember to change the copy in common.h
 */
/** Make sure we don't mistakenly free tstring's with safe_free() */
#ifndef TS_IN_TSTRING
#undef safe_free

#define safe_free(var) \
    do {                                                                \
        (COMPILE_FAIL_IS_TYPE(var, tstring *));                         \
        if (var) {                                                      \
            free(var);                                                  \
            var = NULL;                                                 \
        }                                                               \
    } while(0)
#endif

/**
 * Clear out the string inside a tstring.  The UTF8 flag is not
 * disturbed.
 */
int ts_clear(tstring *ts);

/**
 * Clear out the string inside a tstring, but do not resize (shrink) the
 * tstring.  This might be useful if you are about to reuse the tstring.
 * The UTF8 flag is not disturbed.
 */
int ts_clear_no_resize(tstring *ts);

/**
 * Create a new tstring initialized to the specified value; or if the
 * pointer already points to a tstring, free its contents and replace
 * them with the specified string.  i.e. this is like ts_new_str()
 * except it doesn't leak memory if there was already a string there.
 *
 * Note that even if the old string contents was longer than the new
 * contents, none of the old remains.
 */
int ts_new_or_replace_str(tstring **ts, const char *str);

/**
 * If the string didn't exist, create it with the new main string.
 * If it did exist, append the separator, and then the new main string.
 *
 * \param inout_ts Pointer to the tstring pointer.  If the underlying
 * tstring is NULL, this will be initialized with a new one; otherwise,
 * we'll just append to the existing one.
 *
 * \param separator_str If *inout_ts existed, this will be appended to
 * it before new_str is.  NULL is treated the same as the empty string.
 *
 * \param new_str_fmt A format string, allowing a new string to be
 * constructed as with sprintf.  The result of this is always appended
 * to *inout_ts, after it is either created, or has separator_str
 * appended to it.
 */
int ts_new_or_append_sprintf(tstring **inout_ts, const char *separator_str,
                             const char *new_str_fmt, ...)
    __attribute__ ((format (printf, 3, 4)));

/**
 * Like ts_new_or_replace_str(), except requires that the input
 * tstring not be NULL, and thus never changes the pointer.
 */
int ts_replace_str(tstring *ts, const char *str);

/** Free's the tstring, but returns the char * it holds untouched. */
int ts_detach(tstring **inout_ts, char **ret_string, 
              int32 *ret_string_bytes);

/**
 * Duplicate a tstring.  If the input string is NULL, return a 
 * new, empty string.
 */
int ts_dup(const tstring *in_ts, tstring **ret_ts);

/**
 * Like ts_dup(), except that if the input string is NULL, then
 * NULL is returned instead of a new empty string.
 */
int ts_dup_maybe(const tstring *in_ts, tstring **ret_ts);

int ts_dup_str(const tstring *ts, char **ret_duplicate,
               int32 *ret_byte_length);

/*@}*/

/* ========================================================================= */
/** @name Reading strings */
/*@{*/

/**
 * Returns a pointer to the underlying char *, which MUST NOT
 * be changed, as the library knows the length, and also may have parsed it
 * in the utf8 case.
 */
const char *ts_str(const tstring *ts);

/** Same as ts_str() except does not log a complaint if string is NULL */
const char *ts_str_maybe(const tstring *ts);

/** Same as ts_str_maybe() except returns empty string "" if string is NULL */
const char *ts_str_maybe_empty(const tstring *ts);

/**
 * Same as ts_str() except advances the pointer by ts_char_start
 * characters (this is more interesting for UTF-8 strings).
 * \param ts The tstring to read
 * \param ts_char_start Number of characters offset from beginning of 
 * string that we should return a pointer to.
 * \return Pointer to the string, offset by ts_char_start chars,
 * or NULL if ts_char_start was greater than the total number of
 * characters in the string.
 */
const char *ts_str_nth(const tstring *ts, uint32 ts_char_start);


/* XXX see about adding a ret_YYY for get_buffer and friends for # chars used*/

/**
 * Copy the contents of a tstring into a provided buffer, and
 * NUL-terminate it.  If the buffer is too small to hold the entire
 * string, the string is truncated, but the result will still be
 * NUL-terminated.  This is like strlcpy() for tstrings.
 */
int ts_get_buffer(const tstring *ts, char *ret_buf, uint32 buf_size_bytes);

int ts_substr(const tstring *ts, uint32 ts_char_start,
              int32 ts_char_count, tstring **ret_ts);

#if 0
int ts_substr_str(const tstring *ts, uint32 ts_char_start,
                  int32 ts_char_count, char **ret_str);
int ts_substr_buffer(const tstring *ts, uint32 ts_char_start,
                     int32 ts_char_count, 
                     char *ret_buf, uint32 buf_size_bytes);
#endif

/** The number of bytes used */
int32 ts_length(const tstring *ts);

/** The number of chars used */
int32 ts_num_chars(const tstring *ts);

/** The number of bytes allocated */
int32 ts_size(const tstring *ts);

/**
 * Shortcut to determining if a tstring is empty.  Returns true if ts
 * is non-NULL and has at least one character in it.  This is like
 * calling ts_num_chars(), except that complains if the tstring is
 * NULL, and we do not.  Not having to pretest for NULL means only one
 * reference to the tstring instead of two, which matters if it's a
 * long variable name, etc.
 */
tbool ts_nonempty(const tstring *ts);

/**
 * If the tstring is UTF-8, the requested character will be returned
 * anyway as long as it is a single-byte (ASCII-7) character.  If it
 * is a multi-byte character, -1 is returned.
 *
 * If the specified char offset is out of range for the string,
 * 0 is returned.
 */
int ts_get_char(const tstring *ts, uint32 char_offset);

/**
 * Get a character out of the specified string, whether it is 
 * ASCII-7 or UTF-8, and put it in the provided buffer.  
 * As a convenience to the caller, the byte immediately 
 * following the character is set to 0.
 *
 * The buffer provided must be at least utf8_char_size_max + 1
 * bytes long (to leave room for the delimiter).  We don't
 * mess around with allowing it to be smaller if the character
 * you get happens to be smaller.
 */
int ts_get_char_utf8(const tstring *ts, uint32 char_offset, 
                     char *ret_uchar_buf, uint32 char_buf_size);

int ts_get_char_utf8_byte_offset(const tstring *ts, uint32 byte_offset, 
                                 char *ret_uchar_buf, uint32 char_buf_size,
                                 uint32 *ret_char_size);

/* XXX */
#if 0
int ts_get_array_chars(tstring *ts, uint32 char_offset, int32 num_chars,
                       str_array *ret_char_array);
#endif

/*@}*/

/* ========================================================================= */
/** @name Trimming strings */
/*@{*/

/** Trim leading and trailing number of characters */
int ts_trim_num_chars(tstring *ts, uint32 num_head_chars,
                      uint32 num_tail_chars);

/** Trim leading and trailing characters */
int ts_trim(tstring *ts, int head_ch, int tail_ch);
int ts_trim_head(tstring *ts, int ch);
int ts_trim_tail(tstring *ts, int ch);
int ts_trim_whitespace(tstring *ts);

int ts_trim_utf8(tstring *ts, const char *head_uch, const char *tail_uch);
int ts_trim_head_utf8(tstring *ts, const char *uch);
int ts_trim_tail_utf8(tstring *ts, const char *uch);

/*@}*/

/* ========================================================================= */
/** @name Appending to a string */
/*@{*/

int ts_append(tstring *ts, const tstring *tstr);

int ts_append_str(tstring *ts, const char *str);

/**
 * Append a buffer to a string.  The buffer need not be NUL terminated
 * if you provide its length.
 * \param ts Tstring to append to
 * \param buf Buffer to append from
 * \param buf_num_bytes The number of bytes from the buffer to append,
 * not counting any terminating NUL character.  A NUL terminator 
 * will be added after the append.  If -1 is passed, we will call
 * strlen() to determine the length of the buffer so in that case it
 * must be NUL-terminated.
 */
int ts_append_buffer(tstring *ts, const char *buf, int32 buf_num_bytes);

/** str_num_chars can be -1 to say, all the rest of the string */
int ts_append_str_frag(tstring *ts, const char *a7_str, uint32 str_start_char,
                       int32 str_num_chars);
int ts_append_str_frag_utf8(tstring *ts, const char *utf8_str,
                            uint32 str_start_char, int32 str_num_chars);
int ts_append_tstr_frag(tstring *ts, const tstring *tstr, 
                        uint32 tstr_start_char, int32 tstr_num_chars);
int ts_append_char(tstring *ts, int ch);
int ts_append_chars(tstring *ts, int ch, uint32 num_chars);
int ts_append_char_utf8(tstring *ts, const char *uch);

/**
 * Append as many instances of 'ch' as necessary to pad the length of
 * the string to 'tot_num_chars' characters.  'min_pad_num_chars'
 * tells the minimum number of characters we should append, even if 
 * it would leave the string longer than tot_num_chars.
 */
int ts_append_chars_pad_to_length(tstring *ts, int ch, uint32 tot_num_chars,
                                  uint32 min_pad_num_chars);

int ts_append_sprintf(tstring *ts, const char *format, ...)
     __attribute__ ((format (printf, 2, 3)));

/*@}*/

/* ========================================================================= */
/** @name Overwriting parts of a string */
/*@{*/

/** Put something into a tstring.  Overwrites at the point given */
int ts_put(tstring *ts, uint32 ts_char_offset, const tstring *tstr);

/** Note: if the ts_char_offset is off the end, the put / insert fails */
int ts_put_str(tstring *ts, uint32 ts_char_offset, const char *str);

/** If buf_num_bytes == -1, means do strlen(buf), else may be un-NUL term. */
int ts_put_buffer(tstring *ts, uint32 ts_char_offset, const char *buf,
                  int32 buf_num_bytes);

/** str_num_chars can be -1 to say, all the rest of the string */
int ts_put_str_frag(tstring *ts, uint32 ts_char_offset, 
                    const char *a7_str, 
                    uint32 str_char_offset, int32 str_num_chars);
int ts_put_str_frag_utf8(tstring *ts, uint32 ts_char_offset, 
                         const char *utf8_str, 
                         uint32 str_char_offset, int32 str_num_chars);
int ts_put_tstr_frag(tstring *ts, uint32 ts_char_offset, const tstring *tstr, 
                     uint32 tstr_start_char, int32 tstr_num_chars);
int ts_put_char(tstring *ts, uint32 ts_char_offset, int ch);
int ts_put_chars(tstring *ts, uint32 ts_char_offset, int ch,
                 uint32 num_chars);
int ts_put_char_utf8(tstring *ts, uint32 char_offset, const char *uch);

int ts_put_sprintf(tstring *ts, uint32 ts_char_offset, 
                   const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));

/** makes ts have 'str' at specified start, with nothing after it */
int ts_copy_str(tstring *ts, uint32 ts_char_offset, const char *str);

/*@}*/

/* ========================================================================= */
/** @name Inserting into the middle of a string */
/*@{*/

/** Insert something into a tstring.  Moves down existing characters */
int ts_insert(tstring *ts, uint32 ts_char_offset, const tstring *tstr);

int ts_insert_str(tstring *ts, uint32 ts_char_offset, const char *str);
int ts_insert_str_frag(tstring *ts, uint32 ts_char_offset, 
                       const char *a7_str, 
                       uint32 str_char_offset, int32 str_num_chars);
int ts_insert_str_frag_utf8(tstring *ts, uint32 ts_char_offset, 
                            const char *utf8_str, 
                            uint32 str_char_offset, int32 str_num_chars);
int ts_insert_buffer(tstring *ts, uint32 ts_char_offset, 
                     const char *buf, int32 buf_num_bytes);
int ts_insert_tstr_frag(tstring *ts, uint32 ts_char_offset,
                        const tstring *tstr, 
                        uint32 tstr_start_char, int32 tstr_num_chars);
int ts_insert_char(tstring *ts, uint32 ts_char_offset, int ch);
int ts_insert_chars(tstring *ts, uint32 ts_char_offset, int ch,
                    uint32 num_chars);
int ts_insert_char_utf8(tstring *ts, uint32 ts_char_offset,
                        const char *uch);

int ts_insert_sprintf(tstring *ts, uint32 ts_char_offset,
                      const char *format, ...)
     __attribute__ ((format (printf, 3, 4)));

/*@}*/

/** @name Dealing with upper and lower case */
/*@{*/

/**
 * Transform a tstring to all lowercase.
 *
 * NOTE: any character which would enlarge to take more bytes
 * than it did before as a result of this transformation will be left
 * unchanged.
 *
 * NOTE: we don't support Unicode's "title case" here.
 */
int ts_trans_lowercase(tstring *inout_ts);

/** Same as ts_trans_lowercase(), except transforms to uppercase. */
int ts_trans_uppercase(tstring *inout_ts);

typedef int32 (*ts_trans_map_func)(const int32 in_char);

/*
 * XXX/EMT: I18N: need a ts_trans_map_chars_utf8() which can do UTF-8
 * character mapping.
 */

/** Test if an entire string is lowercase. */
tbool ts_is_lowercase(const tstring *ts);

/** Test if an entire string is uppercase */
tbool ts_is_uppercase(const tstring *ts);

/** Test if a single character in a string is lowercase */
tbool ts_char_is_lowercase(const tstring *ts, uint32 char_offset);

/** Test if a single character in a string is uppercase */
tbool ts_char_is_uppercase(const tstring *ts, uint32 char_offset);

/*@}*/

/** @name Removing parts of strings */
/*@{*/

/**
 * Removes characters from ts, moving characters up if
 * they're still left.
 *
 * \param ts String from which to cut characters
 * \param ts_char_start Index of first character to cut
 * \param ts_char_count Number of characters to cut.  Can be -1 to cut 
 * to the end of the string.
 */
int ts_cut(tstring *ts, uint32 ts_char_start, int32 ts_char_count);

/*@}*/

/** @name Character-based transformations */
/*@{*/

/**
 * Remove all instances of a specified character from a tstring.
 */
int ts_remove_char(tstring *ts, int ch);
int ts_remove_char_utf8(tstring *ts, const char *uch);

/**
 * Replace all instances of a specified character with another character.
 */
int ts_replace_char(tstring *ts, int old_ch, int new_ch);
int ts_replace_char_utf8(tstring *ts, const char *old_uch,
                         const char *new_uch);

/**
 * Perform a transformation on a tstring by calling a function to
 * map each character.
 *
 * Note: this function will allow itself to be called on UTF-8 strings,
 * but in that case the mapping function will only be called on ASCII-7
 * characters (<0x80).  If the string is not UTF-8, the mapping 
 * function will be called for everything.
 */
int ts_trans_map_chars(tstring *inout_ts, ts_trans_map_func trans_func);

/*@}*/

/*
 * Macros to tell us about bytes in a string
 */

/**
 * Tell if a given byte represents an ASCII-7 character.
 */
#define ts_is_ascii_7(b) ((uint8)(b) < 0x80)

/**
 * Tell if a given byte is the first byte of a multi-byte character.
 */
#define ts_is_first_byte_of_multibyte_char(b) ((uint8)(b) >= 0xC0 &&          \
                                            (uint8)(b) <= 0xFD)

/**
 * Tell if a given byte is part of a multi-byte character but is not
 * the first byte.
 */
#define ts_is_nonfirst_byte_of_multibyte_char(b) ((uint8)(b) >= 0x80 &&       \
                                               (uint8)(b) <= 0xBF)

/**
 * Tell if a given byte is the first byte of any character, either
 * ASCII-7 or UTF-8.
 */
#define ts_is_first_byte_of_char(b) (ts_is_ascii_7(b) ||                      \
                                     ts_is_first_byte_of_multibyte_char(b))

/* ========================================================================= */
/** @name Tokenization */
/*@{*/

/**
 * Flags to be passed to ts_tokenize() and its brethren.
 */
typedef enum {
    ttf_none = 0,

    /**
     * Do not strip quoting characters while tokenizing.
     * By default we do.
     * 'c "a b" d'-> {'c','"a b"','d'}
     * NOT 'c "a b" d'-> {'c', 'a b', 'd'}
     */
    ttf_no_strip_quoting_chars =     0x01,

    /**
     * Can only be used with ttf_no_strip_quoting_chars.
     * If the last token is missing a close quote, do not append
     * the missing quotation mark.  By default we do.
     * '"a b' -> {'"a b'}
     * NOT '"a b' -> {'"a b"'}
     */
    ttf_no_append_missing_quoting =  0x02,

    /**
     * Do not ignore duplicate separators.  By default we do.
     * 'a  b' -> {'a', '', 'b'}
     * NOT 'a  b' -> {'a', 'b'}
     */
    ttf_no_ignore_dup_separator =    0x04,

    /**
     * Ignore leading separators.  By default we treat them as
     * indicating another token (or more, if ttf_no_ignore_dup_separator 
     * is specified) at the beginning.
     * ' a b' -> {'a', 'b'}
     * NOT ' a b' -> {'', 'a', 'b'}
     */
    ttf_ignore_leading_separator =   0x08,

    /**
     * Ignore trailing separators.  By default we treat them as
     * indicating another token (or more, if ttf_no_ignore_dup_separator 
     * is specified) at the end.
     * 'a b ' -> {'a', 'b'}
     * NOT 'a b ' -> {'a', 'b', ' '}
     */
    ttf_ignore_trailing_separator =  0x10,

    /**
     * If the string to be tokenized is empty, return a NULL array,
     * instead of an array with a single empty element.  By default
     * we produce the array with a single empty element.
     * '' -> NULL
     * NOT '' -> {''}
     */
    ttf_single_empty_token_null   =  0x20

} ts_tokenize_flags;

/**
 * Flags to be passed to ts_join_tokens() and its brethren.
 */
typedef enum {
    ttj_none = 0,

    /**
     * Make sure the string produced has a leading separator.
     * By default we do not.
     * {'a','b',c'} -> ' a b c'
     * NOT {'a','b',c'} -> 'a b c'
     */
    ttj_ensure_leading_separator =  0x0100,

    /**
     * Make sure the string produced has a trailing separator.
     * By default we do not.
     * {'a','b',c'} -> 'a b c '
     * NOT {'a','b',c'} -> 'a b c'
     */
    ttj_ensure_trailing_separator = 0x0200,

    /*
     * XXX/EMT: find out what the default behavior is. 
     */
    ttj_ignore_empty_tokens =       0x0400,  /* {'a','','b'} -> 'a b' */

#if 0
    ttj_has_unstripped_quoting =    0x0800,  /* {'"a b"','c'} -> '"a\ b" c' */
                                         /* NOT {'"a b"','c'} -> '\"a\ b\" c'*/
    /* Can only be set with ttj_has_unstripped_quoting */
    ttj_convert_unstripped_quoting =0x1000, /* {'"a b"','c'} -> 'a\ b c' */
                                        /* NOT {'"a b"','c'} -> '"a\ b" c' */
    ttj_prefer_quoting_to_escaping =0x2000,  /* {'a b', 'c'} -> '"a b" c' */
                                         /* NOT {'a b', 'c'} -> 'a\ b c' */
#endif
} ts_join_flags;

/**
 * Tokenize a string: break it up into component parts, using a
 * specified separator character.
 *
 * In tokenizing there are three Special characters:
 * 
 *  1) The Escape character      (commonly '\')
 *  2) The Separator character   (commonly ' ')
 *  3) The Quoting character     (commonly '"')
 * 
 *  The tokenizer takes an input string, and breaks it into an array of
 *  tokens, each of which was separated by the Separator.  The Escape
 *  character is used to include literal instances of the any of the three
 *  Special characters inside a token.  The Quoting character can be used to
 *  avoid the need to Escape Separator characters between two Quoting
 *  characters.
 *
 * This suite of functions works on UTF-8 strings, but the three special
 * characters must be ASCII-7.
 *
 * \param ts
 * \param separator_char
 * \param escape_char
 * \param quoting_char
 * \param tokenize_flags A bit field of ::ts_tokenize_flags ORed together.
 * \retval ret_tokens The tokens produced from the input string.
 */
int ts_tokenize(const tstring *ts, 
                int separator_char, int escape_char, int quoting_char,
                uint32 tokenize_flags,
                tstr_array **ret_tokens);

/**
 * Like ts_tokenize() except it takes the string to be tokenized as
 * a const char * instead of a tstring.  The string must be ASCII-7.
 */
int ts_tokenize_str(const char *str,
                    int separator_char, int escape_char, int quoting_char,
                    uint32 tokenize_flags,
                    tstr_array **ret_tokens);

/**
 * Like ts_tokenize_str(), except the string to be tokenized is in UTF-8.
 */
int ts_tokenize_str_utf8(const char *str, 
                         int separator_char, int escape_char, int quoting_char,
                         uint32 token_flags,
                         tstr_array **ret_tokens);

#if 0
/*
 * These are Not Yet Implemented.
 */
int ts_tokenize_offsets(const tstring *ts, 
                    int separator_char, int escape_char, int quoting_char,
                    uint32 tokenize_flags,
                    tstr_array **ret_tokens, array **ret_int32_offset_array);

int ts_tokenize_offsets_str(const char *str,
                    int separator_char, int escape_char, int quoting_char,
                    uint32 tokenize_flags,
                    tstr_array **ret_tokens, array **ret_int32_offset_array);
#endif

/**
 * Join a set of tokens back into a single string.  This is the 
 * opposite of tk_tokenize().
 *
 * When joining tokens, the array of tokens may be a mixture of ASCII-7
 * and UTF-8 strings, making it unclear how the result should be tagged.
 * So the caller must specify explicitly.  ts_join_tokens() will produce
 * a string marked as ASCII-7 regardless of the UTF-8ness of the tokens.
 * and ts_join_tokens_utf8() will produce a UTF-8 string.
 */
int ts_join_tokens(const tstr_array *tokens,
                   int separator_char, int escape_char, int quoting_char,
                   uint32 join_flags,
                   tstring **ret_tstr);

/**
 * Same as ts_join_tokens() except that the string it creates will be
 * marked as a UTF-8 string.
 */
int ts_join_tokens_utf8(const tstr_array *tokens,
                        int separator_char, int escape_char, int quoting_char,
                        uint32 join_flags,
                        tstring **ret_tstr);


/**
 * Given a previously joined token string, append a single new token.
 * For example, given "/a/b" and "c" return "/a/b/c" .
 *
 * The following notes are only interesting if you want to understand
 * what happens in various corner cases, where base is empty or "/"
 * and/or append is "".
 *
 * When base is the empty string and ttj_ensure_leading_separator is not
 * specified, this function will result in base="append", and NOT
 * base="separator_char append".  This is different than what would
 * happen if an array form of joining (using ts_join_tokens()) was used.
 * Note that this means appending an empty string to an empty string
 * yields an empty string (with no ttj_ensure_leading_separator), not a
 * separator.  Callers desiring different behavior may want to either
 * initialize base to the separator_char, or always use
 * ttj_ensure_leading_separator.
 *
 * Related, if append is the empty string, and base is exactly the
 * separator_char, the result is the separator_char.  If base is more
 * than 1 character, or the 1 character it has is not the
 * separator_char, appending the empty string appends exactly the
 * separator_char .
 *
 * Some examples (with no flags):
 *
 *     "/a" + "b"   -> "/a/b"
 *     "/a/b" + "c" -> "/a/b/c"
 *     "a" + "b"    -> "a/b"
 *     "/"  + "a"   -> "/a"
 *     "" + "a"     -> "a"
 *     "" + ""      -> ""
 *     "a" + ""     -> "a/"
 *     "a/" + "b"   -> "a//b"
 *     "/"  + ""    -> "/"
 *     "/a" + ""    -> "/a/"
 *     "/a/" + "b"  -> "/a//b"
 *     "/a/" + ""   -> "/a//"
 *
 */
int ts_join_append_str(tstring *base,
                       const char *append,
                       int separator_char, int escape_char, int quoting_char,
                       uint32 join_flags);

/*@}*/

/* ========================================================================= */
/** @name Finding things in a string */
/*@{*/

/**
 * Find in a tstring.  Returns -1 in ret_char_offset if not found.
 */
int ts_find(const tstring *ts, uint32 ts_start_char_offset, 
            const tstring *tstr, int32 *ret_char_offset);
int ts_find_str(const tstring *ts, uint32 ts_start_char_offset, 
                const char *str, int32 *ret_char_offset);

#if 0
int ts_find_str_frag(const tstring *ts, uint32 ts_start_char_offset, 
                     const char *a7_str, uint32 str_start_char,
                     int32 str_num_chars, int32 *ret_char_offset);
int ts_find_str_frag_utf8(const tstring *ts, uint32 ts_start_char_offset, 
                          const char *utf8_str, uint32 str_start_char,
                          int32 str_num_chars, int32 *ret_char_offset);
int ts_find_buffer(const tstring *ts, uint32 ts_start_char_offset, 
                   const char *buf, uint32 buf_byte_count, 
                   int32 *ret_char_offset);
int ts_find_tstr_frag(const tstring *ts, uint32 ts_start_char_offset, 
                      const tstring *tstr, 
                      uint32 tstr_start_char, int32 tstr_num_chars, 
                      int32 *ret_char_offset);
#endif

int ts_find_char(const tstring *ts, uint32 ts_start_char_offset, 
                 int ch, int32 *ret_char_offset);

#if 0
int ts_find_char_utf8(const tstring *ts, uint32 ts_start_char_offset,
                      const char *uch, int32 *ret_char_offset);
#endif

/**
 * Find the first character in the specified string that does not
 * match one of the ASCII-7 characters in the provided string
 * (really a character list).
 *
 * \param ts The string to search
 *
 * \param allowed_a7_chars_str A list of all ASCII-7 characters allowed
 * to be in the string.  Note that if the string is UTF-8, any non-ASCII-7
 * characters in it will be considered invalid, since they are not 
 * represented in this list.
 *
 * \param ts_start_char_offset The character (not byte) offset of the
 * first character to start searching at.  0 to search entire string.
 *
 * \retval ret_bad_char_offset Returns the character offset of the 
 * first such character if one is found, or -1 if none are found.
 */
int ts_find_first_invalid_char(const tstring *ts,
                               const char *allowed_a7_chars_str,
                               uint32 ts_start_char_offset,
                               int64 *ret_bad_char_offset);

/**
 * Find the first character in the specified string that matches
 * one of the blacklisted ASCII-7 characters in the provided string
 * (really a character list).  Any other char value is considered valid,
 * including values from 128 to 255.
 *
 * \param ts The string to search
 *
 * \param disallowed_a7_chars_str A blacklist of all ASCII-7 characters not
 * allowed to be in the string.  Note that if the string is UTF-8, this
 * function will merely skip over non-ASCII-7 characters in ts, as they cannot
 * be blacklisted.  Therefore, any non-ASCII-7 characters injected into the
 * disallowed_a7_chars_str blacklist will effectively be ignored, and this
 * function will not complain about them.
 *
 * \param ts_start_char_offset The character (not byte) offset of the
 * first character to start searching at.  0 to search entire string.
 *
 * \retval ret_bad_char_offset The offset at which the first invalid
 * character was found, or -1 if none was found.
 */
int ts_find_first_invalid_char_blacklisted(const tstring *ts,
                                           const char *disallowed_a7_chars_str,
                                           uint32 ts_start_char_offset,
                                           int64 *ret_bad_char_offset);

/* Reverse find */

/**
 * Reverse-find: search starting from the end of the string.
 *
 * \param ts String to search
 * \param ts_start_char_offset the offset from the beginning of the big string
 * to start searching for the little string.  It is the first character
 * that will be tried as the beginning of the little string, and will
 * then work backwards from there.  If it is -1, the search will start
 * from the end.
 * \param tstr String to search for
 * \retval ret_char_offset The offset from the beginning of the big string
 * where the next occurrence of the little string begins, or -1 if
 * it was not found.
 */
int ts_rfind(const tstring *ts, int32 ts_start_char_offset, 
             const tstring *tstr, int32 *ret_char_offset);

int ts_rfind_str(const tstring *ts, int32 ts_start_char_offset, 
                 const char *str, int32 *ret_char_offset);

#if 0
int ts_rfind_str_frag(const tstring *ts, int32 ts_start_char_offset, 
                      const char *a7_str, uint32 str_start_char,
                      int32 str_num_chars, int32 *ret_char_offset);
int ts_rfind_str_frag_utf8(const tstring *ts, int32 ts_start_char_offset, 
                           const char *utf8_str, uint32 str_start_char,
                           int32 str_num_chars, int32 *ret_char_offset);
int ts_rfind_buffer(const tstring *ts, int32 ts_start_char_offset, 
                    const char *buf, uint32 buf_byte_count, 
                    int32 *ret_char_offset);
int ts_rfind_tstr_frag(const tstring *ts, int32 ts_start_char_offset, 
                       const tstring *tstr,
                       uint32 tstr_start_char, int32 tstr_num_chars, 
                       int32 *ret_char_offset);
#endif

int ts_rfind_char(const tstring *ts, int32 ts_start_char_offset, 
                  int ch, int32 *ret_char_offset);

#if 0
int ts_rfind_char_utf8(const tstring *ts, int32 ts_start_char_offset,
                       const char *uch, int32 *ret_char_offset);
#endif

/*@}*/

/* ========================================================================= */
/** @name Comparing strings */
/*@{*/

/** Are two tstrings equal or not */
tbool ts_equal(const tstring *ts, const tstring *tstr);

/** Compare for equality, ignoring case */
tbool ts_equal_case(const tstring *ts, const tstring *tstr);

/* Can ignore case in compare */
tbool ts_equal_tstr(const tstring *ts, const tstring *tstr, tbool ignore_case);
tbool ts_equal_tstr_frag(const tstring *ts,
                        const tstring *tstr,
                        uint32 tstr_start_char, int32 tstr_num_chars, 
                        tbool ignore_case); 
tbool ts_equal_str(const tstring *ts, const char *str, tbool ignore_case);

#if 0
tbool ts_equal_str_frag(const tstring *ts, 
                       const char *a7_str, uint32 str_start_char,
                       int32 str_num_chars,
                       tbool ignore_case);
tbool ts_equal_str_frag_utf8(const tstring *ts, 
                             const char *utf8_str, uint32 str_start_char,
                             int32 str_num_chars,
                             tbool ignore_case);
tbool ts_equal_buffer(const tstring *ts,
                     const char *buf, uint32 buf_byte_count,
                     tbool ignore_case);
tbool ts_equal_char(const tstring *ts, int ch, tbool ignore_case); 
tbool ts_equal_offset_char(const tstring *ts, uint32 ts_offset, int ch,
                          tbool ignore_case); 
tbool ts_equal_char_utf8(const tstring *ts,
                        const char *uch, tbool ignore_case); 
#endif


/* Compare like strcmp: first is less, equal, greater second -> neg, 0, pos */
int32 ts_cmp(const tstring *ts, const tstring *tstr);
int32 ts_cmp_case(const tstring *ts, const tstring *tstr);

/* Like ts_cmp(), except consider NULL and the empty string equivalent */
int32 ts_cmp_empty(const tstring *ts, const tstring *tstr);

int32 ts_cmp_tstr(const tstring *ts, const tstring *tstr, tbool ignore_case);

#if 0
int32 ts_cmp_tstr_frag(const tstring *ts,
                       const tstring *tstr,
                       uint32 tstr_start_char, int32 tstr_num_chars, 
                       tbool ignore_case); 
#endif

int32 ts_cmp_str(const tstring *ts, const char *str, tbool ignore_case);

#if 0
int32 ts_cmp_str_frag(const tstring *ts, 
                      const char *a7_str, uint32 str_start_char,
                      int32 str_num_chars, tbool ignore_case);
int32 ts_cmp_str_frag_utf8(const tstring *ts, 
                           const char *utf8_str, uint32 str_start_char,
                           int32 str_num_chars, tbool ignore_case);
int32 ts_cmp_buffer(const tstring *ts, 
                    const char *buf, uint32 buf_byte_count, tbool ignore_case);
int32 ts_cmp_char(const tstring *ts, int ch, tbool ignore_case);
int32 ts_cmp_offset_char(const tstring *ts, uint32 ts_offset, int ch,
                          tbool ignore_case); 
int32 ts_cmp_char_utf8(const tstring *ts, const char *uch, tbool ignore_case);
#endif

/* In the future, there could be functions which compare using a function */

/**
 * Prefix: does the tstring in the first argument have as a prefix
 * whatever string is specified by the rest of the params?
 * Note that this is the reverse of the order used by lc_is_prefix().
 */
tbool ts_has_prefix_str(const tstring *ts, const char *str, tbool ignore_case);
#if 0
tbool ts_has_prefix_buffer(const tstring *ts, uint32 ts_start_offset, 
                           const char *buf, uint32 buf_byte_count,
                           tbool ignore_case);
tbool ts_has_prefix_tstr(const tstring *ts, const tstring *tstr,
                         tbool ignore_case);
#endif
tbool ts_has_prefix_char(const tstring *ts, int ch, tbool ignore_case);

#if 0
tbool ts_has_prefix_char_utf8(const tstring *ts, const char *uch,
                              tbool ignore_case);
#endif

/**
 * Check if 'ts' has 'pfx' as a prefix.  Return 0 if so.  If not, return
 * a number <0 if ts is less than pfx; or a number >0 if ts is greater 
 * than pfx.
 */
int32 ts_cmp_prefix_str(const tstring *ts, const char *pfx,
                        tbool ignore_case);

/**
 * Suffix: does the tstring in the first argument have as a suffix
 * whatever string is specified by the rest of the params?
 * Note that this is the reverse of the order used by lc_is_suffix().
 */
tbool ts_has_suffix_str(const tstring *ts, const char *str, tbool ignore_case);

#if 0
tbool ts_has_suffix_buffer(const tstring *ts, 
                           const char *buf, uint32 buf_byte_count,
                           tbool ignore_case);
tbool ts_has_suffix_tstr(const tstring *ts, const tstring *tstr,
                         tbool ignore_case);
tbool ts_has_suffix_char(const tstring *ts, int ch, tbool ignore_case);
tbool ts_has_suffix_char_utf8(const tstring *ts, const char *uch,
                              tbool ignore_case);
#endif

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* __TSTRING_H_ */
