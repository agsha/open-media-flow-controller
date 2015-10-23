/*
 *
 * cgi.h
 *
 *
 *
 */

#ifndef __CGI_H_
#define __CGI_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <stdio.h>
#include "common.h"
#include "tstring.h"
#include "tstr_array.h"
#include "hash_table.h"
#include "tbuffer.h"
#include "ttime.h"

/*=============================================================================
 * LIBCGI
 */

/* ========================================================================= */
/** \file cgi.h CGI API
 * \ingroup web
 *
 * Not to be confused with the LPGL'd LIBCGI. This is an internal TMS CGI
 * library.
 *
 * Note that in most cases, this library will not be used. Module
 * developers should use the web interfaces provided in web.h. This library
 * exists so that we can share CGI utility methods with programs that do
 * not require the full support provided in web.h.
 */

typedef struct lcgi_context lcgi_context;

/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

#define LCGI_SUFFIX_ORIGINAL "_original"
#define LCGI_HDR_SET_COOKIE "Set-Cookie"
#define LCGI_PARAM_EXPIRES "expires"
#define LCGI_PARAM_PATH "path"
#define LCGI_PARAM_DOMAIN "domain"
#define LCGI_PARAM_HTTP_ONLY "HttpOnly"
#define LCGI_PARAM_SECURE_ONLY "Secure"
#define LCGI_HDR_SEP_CHAR ':'
#define LCGI_COOKIE_SEP_CHAR ';'

extern const char lcgi_hdr_set_cookie[];
extern const char lcgi_param_expires[];
extern const char lcgi_param_path[];
extern const char lcgi_param_domain[];
extern const char lcgi_param_http_only[];
extern const char lcgi_param_secure_only[];
extern const char lcgi_hdr_sep_char;
extern const char lcgi_cookie_sep_char;


/*-----------------------------------------------------------------------------
 * API
 */

/**
 * Must be called before any lcgi functions are used.
 * ret_context will be NULL on error.
 * \param ret_context the cgi context.
 * \return 0 on success, error code on failure.
 */
int lcgi_init(lcgi_context **ret_context);

/**
 * Must be called before binary termination if lcgi_initialize()
 * was called.
 * \param context the cgi context.
 * \return 0 on success, error code on failure.
 */
int lcgi_deinit(lcgi_context **context);

/**
 * Unescape the given url, optionally also converting plus to space.
 * The resulting string may be shorter than the original since unescaped
 * characters take up less room than the escaped ones.
 * On error, the original string is NOT guaranteed to be the same as
 * it was before this call.
 * \param inout_url the url to unescape and will contain the unescaped result.
 * \param convert_plus should we also convert '+' plus characters to
 * ' ' space characters?
 * \return 0 on success, error code on failure.
 */
int lcgi_url_unescape_ex(tstring *inout_url, tbool convert_plus);

/**
 * This is a backward-compatibility wrapper for lcgi_url_unescape_ex().
 * We always pass 'true' for convert_plus.
 */
int lcgi_url_unescape(tstring *inout_url);

/**
 * Escape the given url.
 * This routine takes care of plus to space conversion as well.
 * The resulting string may be larger than the original since escaped
 * characters take up more room than the unescaped ones.
 * On error, the original string is NOT guaranteed to be the same as
 * it was before this call.
 * \param inout_url the url to escape and will contain the escaped result.
 * \return 0 on success, error code on failure.
 */
int lcgi_url_escape(tstring *inout_url);

/**
 * Utility method to update the cookie environment variable with the
 * given name value pair.  Preserves other cookies that are already in the
 * environment variable.  Note that special characters in the name or
 * value are acceptable, and will be URL-escaped automatically, so unescaped
 * strings are expected here.
 * \param name the name of the cookie to update
 * \param value the new value for the cookie
 * \param skip_if_no_cookie If 'true' and there is not already a 
 * cookie present in the environment (HTTP_COOKIE), we will do
 * nothing.  Otherwise, we will always set the specified cookie.
 * \return 0 on success, error code on failure.
 */
int lcgi_update_cookie_env(const tstring *name, const tstring *value,
                           tbool skip_if_no_cookie);
int lcgi_update_cookie_env_str(const char *name, const char *value,
                               tbool skip_if_no_cookie);

/**
 * Parse the cookies in the web server's cookie environment (HTTP_COOKIE) and
 * return a hash table containing the name (tstring *) and value (tstring *).
 * An empty table will be returned if no cookies were found. On error, the
 * ret_table will contain NULL.  The caller is responsible for freeing the
 * returned table.  Cookies in the table are in plain (url-unescaped) text.
 * \param ctx the cgi context (currently unused).
 * \param ret_table will return a newly allocated table containing data.
 * \return 0 on success, error code on failure.
 */
int lcgi_load_cookies(lcgi_context *ctx, ht_table **ret_table);

/**
 * Parse the next quoted string, beginning at start_idx, and return a
 * dequoted copy of that string, along with the index of the next input
 * character following the quoted string, which may be used for continued
 * parsing of the remainder of the input string (if any).  A quoted string
 * is any string of characters enclosed in a pair of quote characters.
 * Note that within the quoted string the backslash character is parsed as
 * an escape character, so it can be used to escape a quote character, or
 * another backslash character (a quoted-pair, as described in section
 * 2.2 of RFC 2616), although it will be taken to escape any single
 * character that follows.  If start_idx does not point to a quote
 * character, any intervening characters will be skipped over until the
 * first open quote is found.
 *
 * \param quoted the input string to be parsed
 * \param start_idx the character offset at which to start parsing
 * \retval ret_next_idx character offset of the next character following
 * the quoted string in the input (i.e. the character after any closing
 * quote), or the last character parsed if an error occured.  If the last
 * character in the string has been parsed, ret_next_idx will point to the
 * null terminator at the end of the string.
 * \retval ret_dequoted
 * \return 0 on success, error code on failure.  Returns lc_err_not_found
 * if a quoted string is not found, and lc_err_parse_error if a quoted
 * string is not closed, or if an escape character is not followed by
 * another character.
 */
int lcgi_get_dequoted_string(const char *quoted, uint32 start_idx,
                             uint32 *ret_next_idx,
                             tstring **ret_dequoted);

/**
 * Parse raw cookies one at a time from an input cookie string encoded per
 * RFCs 2109, 2616 and 2965, with a few differences to tolerate formatting
 * non-compliance seen on some servers and tolerated by many browsers.  In
 * particular, note the following differences:
 *
 *  1. Linear white space between tokens is limited to the space and
 *  horizontal tab characters.  It does not include the [CRLF] as defined
 *  in RFC 2616.  Thus, unquoted line termination characters may appear in
 *  a cookie VALUE.
 *
 *  2. The cookie separator character is a semicolon rather than a comma,
 *  as this function is expected to work with semicolon delimited cookies
 *  provided by the web server in the HTTP_COOKIE environment variable.
 *
 *  3. No special restrictions are placed on the characters in the cookie
 *  NAME or VALUE, beyond the requirements that the '=' character, if not
 *  quoted, is interpreted as the delimiter between a cookie's attribute
 *  NAME and its VALUE, an unquoted semicolon always terminates a cookie,
 *  and a cookie NAME or VALUE may not contain unqouted linear white space.
 *  Note that this means that '=' characters may appear unquoted in the
 *  VALUE (although this would not be good practice as it can be
 *  confusing).
 *
 *  4. The cookie NAME is optional rather than required, as some web
 *  servers may require nameless cookies.
 *
 *  So the general syntax for cookies input data is of the form:
 *
 *      [[ [NAME] = ] VALUE] [; [[[NAME] = ] VALUE ]...
 *
 *  where the NAME is the optional cookie name, VALUE is either a quoted value
 *  or a contiguous string of any characters other than linear white space or
 *  semicolon, and where the linear whitespace above is optional.
 *
 * The cookie name and value are returned without any decoding beyond what
 * is described above, namely without decoding URL escaping.  The caller is
 * responsible for calling lcgi_url_unescape(), depending on whether
 * subsequent operations on the cookie name and value will require internal
 * or external cookie formatting.
 *
 * \param cookie_str the input string to parse (e.g. from HTTP_COOKIES)
 * \param start_idx the character offset at which to start parsing input
 * \param ret_next_idx the index of the next character in the input that
 * needs to be parsed.  Unless an error occurred, this points to the first
 * character of the next cookie, or to the input string's null termination
 * character of there are no more cookies.
 * \param ret_name name of the cookie (if any) found after start_idx
 * \param ret_value value of the cookie (if any) found after start_idx
 * \param ret_more_cookies boolean indicating whether more cookies are
 * expected in the input string.  Returns false only if the end of the string
 * has been reached.
 * \return 0 on success, error code on failure.  Returns lc_err_parse_error
 * if the cookie format is invalid.
 */
int lcgi_get_next_cookie(const char *cookie_str,
                         uint32 start_idx,
                         uint32 *ret_next_idx,
                         tstring **ret_name,
                         tstring **ret_value,
                         tbool *ret_more_cookies);

/**
 * Parse the query string and return a hash table containing
 * the name (tstring *) and value (tstr_array *). An empty table will be
 * returned even if no query params are found. On error, the ret_table
 * will contain NULL.
 * The caller is responsible for freeing the returned table.
 * \param ctx the cgi context.
 * \param ret_table will return a newly allocated table containing data.
 * \return 0 on success, error code on failure.
 */
int lcgi_load_query_string_data(lcgi_context *ctx, ht_table **ret_table);

/**
 * Read HTTP POST input from the specified file stream.  It can be
 * returned in one or both of two forms: raw, or in a hash table
 * containing the name (tstring *) and  value (tstr_array *).
 * An empty table will be returned even if no post data is found. 
 * On error, the ret_table will contain NULL.
 * The caller is responsible for freeing the returned table.
 * \param ctx the cgi context.
 * \param fd the file descriptor to load the post data from.
 * \param ret_raw_data will return a string containing the raw post data.
 * \param ret_table will return a newly allocated table containing data.
 * \return 0 on success, error code on failure.
 */
int lcgi_load_post_data(lcgi_context *ctx, int fd, tbuf **ret_raw_data,
                        ht_table **ret_table);

/**
 * Insert the given name and value into the given table.
 * The name will be duped and then inserted into the table. The
 * value will be duped and then stored inside a tstr_array which will
 * then be inserted into the table. If NULL is specified for the value,
 * an empty tstr_array will be inserted into the table. If you set
 * clear_values to true, then all existing values for the given name
 * inside the table will be deleted first before the new value is
 * inserted.
 * This is just a convenience method for manipulating the tables returned
 * by the lcgi_load_query_string_data and lcgi_load_post_data functions.
 * This function should NOT be used with the table returned by
 * lcgi_load_cookies since that table contains tstring/tstring pairs instead
 * of tstring/tstr_array pairs.
 * \param table the table to insert the name/value into.
 * \param name the name of the param (will be duped).
 * \param value the value of the param (will be duped) or NULL.
 * \param clear_values whether to clear all values first before appending.
 * \return 0 on success, error code on failure.
 */
int lcgi_insert_param_value(ht_table *table, const tstring *name,
                            const tstring *value, tbool clear_values);

/**
 * Generates a restricted (browser-only) set cookie HTTP header.
 * 
 * Security notes:  1) Cookies are always vulnerable to XSS attacks unless
 * the http_only flag is set to true.  Any browser that supports this flag
 * should prevent client-side scripts from accessing the cookie, which makes
 * the cookie data less vulnerable to malicious theft in a cross-site
 * scripting attack.  The HttpOnly flag is supported by most modern
 * browsers, and is ignored by those that do not support it, so it is safe
 * to use.  Set it to false only if cookie access is required by a
 * legitimate application script.  2) Cookies that contain sensitive data
 * (e.g. a session auth token), are vulnerable to snooping.  Setting the
 * http_only flag tells the client browser not to transmit a cookie unless the
 * session uses a secure protocol (typically https).  However, if you set this
 * flag, note that this breaks cookie transmission for http users, so you
 * should make sure that either http is disabled, or that http sessions do
 * not use this flag.
 *
 * \param name the name.
 * \param value the value or NULL.
 * \param exp_time expiration time or 0 for no expiration.
 * \param path the path or NULL.
 * \param domain the domain or NULL.
 * \param ret_cookie_hdr the new cookie hdr.
 * \param http_only whether to prevent client-side script access.
 * \param secure_only whether a secure protocol is required (e.g. SSL/TLS)
 * \return 0 on success, error code on failure.
 */
int lcgi_make_set_cookie_hdr(const char *name,
                             const char *value,
                             lt_time_sec exp_time,
                             const char *path,
                             const char *domain,
                             tbool http_only,
                             tbool secure_only,
                             char **ret_cookie_hdr);

#ifdef __cplusplus
}
#endif

#endif /* __CGI_H_ */
