/*
 *
 * web.h
 *
 *
 *
 */

#ifndef __WEB_H_
#define __WEB_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <time.h>
#include "common.h"
#include "ttime.h"
#include "tstring.h"
#include "str_array.h"
#include "tstr_array.h"
#include "gcl.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "md_client.h"
#include "libevent_wrapper.h"
#include "tacl.h"


/*=============================================================================
 * WEB INTERFACE
 */

/** \defgroup web Web public APIs */

/* ========================================================================= */
/** \file web.h Libweb client API
 * \ingroup web
 *
 * These functions are available to all that link with libweb.
 * Specifically, this means the Request Handler, and thus all 
 * Web plug-in modules.
 */

/*-----------------------------------------------------------------------------
 * GLOBALS
 */

extern md_client_context *web_mcc;

extern lew_context *web_lwc;


/*-----------------------------------------------------------------------------
 * CONSTANTS
 */

#if defined(PROD_FEATURE_DEV_ENV) && defined(PROD_TARGET_OS_FREEBSD)
#define WEB_DEFAULT_USER  "nobody"
#define WEB_DEFAULT_GROUP "nobody"
#else
#define WEB_DEFAULT_USER  "apache"
#define WEB_DEFAULT_GROUP "apache"
#endif

extern const char web_default_user[];
extern const char web_default_group[];

/* Custom variables used for our form response messages */
#define WEB_VALUE_ERROR    "v_error"
#define WEB_VALUE_SUCCESS  "v_success"
#define WEB_VALUE_MSG_ENCODING "v_msg_encoding"

typedef enum {
    wmeo_none = 0,  /* don't alter the reponse message */
    wmeo_html_body_escape,
    wmeo_html_body_escape_transcode_ws,
} web_msg_encoding_option;

#ifdef WEB_RESPONSE_MSG_ENCODING_DISABLE
static const uint32 web_msg_encoding_option_default = wmeo_none;
#else
/*
 * By default, we protect the against inadvertent HTML in the response, and
 * transcode white space into HTML line breaks and spaces.
 */
static const uint32 web_msg_encoding_option_default = wmeo_html_body_escape_transcode_ws;
#endif

static const lc_enum_string_map web_msg_encoding_option_map[] = {
    { wmeo_none, "none" },
    { wmeo_html_body_escape, "html_body_escape" },
    { wmeo_html_body_escape_transcode_ws, "html_body_escape_transcode_ws" },
    { 0, NULL }
};

extern const char web_value_error[];
extern const char web_value_success[];
extern const char web_value_msg_encoding[];

static const char web_newline_char = '\n';
static const char web_carriage_return_char = '\r';

#define WEB_HDR_LOCATION        "Location"
#define WEB_HDR_CONTENT_TYPE    "Content-type"
#define WEB_MIMETYPE_TEXT_HTML  "text/html"
#define WEB_MIMETYPE_TEXT_XML   "text/xml"
#define WEB_HTTP_PREFIX         "http://"
#define WEB_HTTPS_PREFIX        "https://"
#define WEB_HTTP_HOST           "HTTP_HOST"
#define WEB_HTTPS               "HTTPS"

extern const char web_hdr_location[];
extern const char web_hdr_content_type[];
extern const char web_mimetype_text_html[];
extern const char web_mimetype_text_xml[];
extern const char web_http_prefix[];
extern const char web_https_prefix[];
extern const char web_http_host[];
extern const char web_https[];

static const char web_hdr_sep_char = ':';
static const char web_url_sep_char = '/';
static const char web_cookie_sep_char = ';';

/* URL parameter to ignore last_activity update */
#define WSMD_NONINTERACTIVE "tms_noninteractive"
extern const char wsmd_noninteractive[];


/*-----------------------------------------------------------------------------
 * STRUCTURES
 */

/**
 * Defines the available parameter sources that can be used as args
 * to some of the functions in this header file.
 */
typedef enum {
    ps_query_string,   /* get/set data from only the query string */
    ps_post_data,      /* get/set data from only the post data */
    ps_either          /* get/set data from either */
} web_param_source;

/**
 * Defines the replacement policies when setting a parameter either in
 * the query string or the post data.
 * 
 * The following are some combinations:
 *
 *    src           policy     value  result
 *    ------------  ---------  -----  -----------------------------------------
 *    query_string  rp_all     yes    replace any existing values in query
 *    ''            ''         null   delete param from query
 *    ''            rp_none    yes    append a value to the param in query
 *    ''            ''         null   DO NOTHING
 *    post_data     rp_all     yes    replace any existing values in post
 *    ''            ''         null   delete param from post
 *    ''            rp_none    yes    append a value to the param in post
 *    ''            ''         null   DO NOTHING
 *    either        rp_all     yes    replace any existing values in query
 *                                    AND also in post
 *    ''            ''         null   delete param from query AND post
 *    ''            rp_none    yes    append a value to the param in query
 *                                    AND in post
 *    ''            ''         null   DO NOTHING
 *
 * In general usage, the ps_either src should probably not be used when
 * setting a request parameter.
 */
typedef enum {
    rp_all,
    rp_none
} web_replacement_policy;

/**
 * Defines the handling of POST data in web_init() and
 * web_load_post_data().  The POST data may be large, and so it may make
 * sense to avoid parsing or preserving the raw form of it in some
 * situations.
 */
typedef enum {
    wpdh_parsed,
    wpdh_raw,
    wpdh_parsed_and_raw
} web_post_data_handling;


typedef struct web_request_data web_request_data;

/**
 * Function for walking through parameter list.
 * \param src the source this particular param was found in.
 * \param name the name of the param.
 * \param value contains value if multi = false.
 * \param param parameter passed in when registering this callback.
 * \return 0 on success, error code on failure.
 */
typedef int (*web_param_iterator_func)(web_param_source src,
                                       const tstring *name,
                                       const tstring *value,
                                       void *param);

/**
 * Function for walking through cookie list.
 * \param name the name of the cookie.
 * \param value the value of the cookie.
 * \param param parameter passed in when registering this callback.
 * \return 0 on success, error code on failure.
 */
typedef int (*web_cookie_iterator_func)(const tstring *name,
                                        const tstring *value,
                                        void *param);

/*-----------------------------------------------------------------------------
 * API
 */

/**
 * Mark this request as having to redirect to the given URL.
 * If a redirect URL is set when the headers are output, note that
 * any buffered content will be deleted.
 * \param web_data per request state
 * \param url the url. (duped)
 * \return 0 on success, error code on failure.
 */
int web_redirect(web_request_data *web_data, const tstring *url);
int web_redirect_str(web_request_data *web_data, const char *url);

/**
 * Was the request redirected?
 * \param web_data per request state
 * \return true or false.
 */
tbool web_is_request_redirected(web_request_data *web_data);

/**
 * Add a raw header line to the response.
 * Raw header lines are not touched and are sent as is to the browser.
 * Raw header lines SHOULD NOT contain the newline as it will automatically
 * be added by the framework.
 * \param web_data per request state
 * \param header the header line.
 * \return 0 on success, error code on failure.
 */
int web_add_raw_header(web_request_data *web_data, const tstring *header);
int web_add_raw_header_str(web_request_data *web_data, const char *header);

/**
 * Get a raw header line from the pending response.
 * This is generally a bad thing to do, but sometimes certain
 * cases (such as wsmd sending back a fully-formed response 
 * header to set the session cookie) force us into it to get
 * the data we need.
 */
int web_get_raw_header(web_request_data *web_data, uint32 idx,
                       const tstring **ret_header);

/**
 * Add a header line to the response.
 * There is no guarantee on the order that the headers will be
 * sent to the browser.
 * Note that in order to provide header removal functionality,
 * there's a limitation that all header lines must have a unique
 * name (no duplicate names allowed). Adding a header with a
 * duplicate name will cause the old value to be overwritten.
 * Note that for cookies, you should use the web_set_cookie
 * function below.
 * \param web_data per request state
 * \param name the name of the header to add. (duped)
 * \param value the value of the header. (duped)
 * \return 0 on success, error code on failure.
 */
int web_add_header(web_request_data *web_data, const tstring *name,
                   const tstring *value);
int web_add_header_str(web_request_data *web_data, const char *name,
                       const char *value);

/**
 * Remove a header line from the response.
 * \param web_data per request state
 * \param name the name of the header to remove.
 * \return 0 on success, error code on failure.
 */
int web_remove_header(web_request_data *web_data, const tstring *name);
int web_remove_header_str(web_request_data *web_data, const char *name);

/**
 * Clears all special added lines from the response header.
 * \param web_data per request state
 * \return 0 on success, error core on failure.
 */
int web_clear_header(web_request_data *web_data);

/**
 * Enters stream mode. Essentially outputs the header and
 * outputs all buffered contents. After this function is called
 * all output (via web_print) goes directly to stdout.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_enter_stream_mode(web_request_data *web_data);

/**
 * Check whether or not the request is in stream mode.
 * \param web_data per request state
 * \return true or false.
 */
tbool web_is_in_stream_mode(web_request_data *web_data);

/**
 * Make the response be a multipart response. This can be used to
 * send multiple pages back to the browser.
 * \param web_data per request state
 * \result 0 on success, error code on failure.
 */
int web_enter_multipart_response_mode(web_request_data *web_data);

/**
 * Insert a multipart boundary into the response stream. Used to
 * delimit a new page.
 * \param web_data per request state
 * \result 0 on success, error code on failure.
 */
int web_output_multipart_boundary(web_request_data *web_data);

/**
 * This method is used to send content back to the browser.
 * It will correctly buffer content if the request has not
 * already entered stream mode.
 * Note that this function will do nothing if the request
 * has already entered stream mode AND the redirect URL
 * was set for this request.
 * \param web_data per request state
 * \param str the string.
 * \return 0 on success, error code on failure.
 */
int web_print(web_request_data *web_data, const tstring *str);
int web_print_str(web_request_data *web_data, const char *str);

/**
 * Get if the request was made over HTTP or HTTPS.
 * \param web_data per request state
 * \param ret_scheme "http" or "https"
 */
int web_request_url_scheme(web_request_data *web_data,
                           char **ret_scheme);

/**
 * Retrieve a URL parameter. This function is similar to the
 * web_get_request_param function below with a src of query_string but
 * it's used to access the var_????? series of query parameters. These
 * parameters are special in that they are automatically restored on a
 * call to web_url_construct. Please see web_url_construct for more info.
 * Do NOT modify or free the returned value.
 * \param web_data per request state
 * \param name the name of the param.
 * \param ret_value the returned value of the param or NULL if not found.
 * \return 0 on success, error code on failure.
 */
int web_url_get_param(web_request_data *web_data, const tstring *name,
                      const tstring **ret_value);
int web_url_get_param_str(web_request_data *web_data, const char *name,
                          const tstring **ret_value);

/**
 * Set a URL parameter. This function is similar to the web_set_request_param
 * function with a src of query_string but it's used to access the var_?????
 * series of query parameters. Please see web_url_construct for more info.
 * \param web_data per request state
 * \param name the name of the param. (duped).
 * \param value the value of the param (pass NULL to remove param). (duped).
 * \return 0 on success, error code on failure.
 */
int web_url_set_param(web_request_data *web_data, const tstring *name,
                      const tstring *value);
int web_url_set_param_str(web_request_data *web_data, const char *name,
                          const char *value);

/**
 * Clears the known url parameters from the internally held list.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_url_clear_params(web_request_data *web_data);

/**
 * Web url construction options.
 */
typedef enum {
    /**
     * Do not perform any html context encoding.
     */
    wuco_none = 0,

    /**
     * Encode '&' characters as the string "&amp;" for HTML URI/URL context.
     * This flag should be set when the string output of a URL construction
     * function will be emitted from an HTML web page (web TCL template).
     * Do not use this flag for web redirects, as those are emitted by the
     * web core as HTTP Location headers that do not understand HTML entity
     * encoding.  Do use this for HTML context correctness to ensure that
     * browsers will not misinterpret some component data as an HTML entity.
     * For HTML context, this is required for w3 compliance.
     */
    wuco_html_esc_amp_encode = 1 << 1,

    /**
     * Escape data in a name/value component from HTML context during
     * URI/URL construction.  Data elements are escaped using hex
     * percent-encoding as described in RFC 3986, section 2.1 (pct-encoded =
     * "%" HEXDIG HEXDIG).  This prevents data from being interpreted as
     * HTML.  Note that with functions that accept pre-constructed
     * name=value pairs, the name and value are escaped independently.  The
     * first '=' character is used to parse out the name, and everything
     * that follows it considered to be value data (including any '='
     * characters) and is hex-encoded.  The '=' character itself is not
     * escaped.  This option is mutually exclusive with
     * wuco_html_esc_singular.
     */
    wuco_html_esc_name_and_value = 1 << 2,

    /**
     * Escape a singular data component from HTML context during URI/URL
     * construction.  This is similar to wuco_html_esc_name_and_value,
     * except that no parsing is performed, and all characters in the
     * component are encoded (including any '=' character).  This option is
     * mutually exclusive with wuco_html_esc_name_and_value.
     */
    wuco_html_esc_singular = 1 << 3,

} web_url_construct_options;

/* Bit field for web_url_construct_options */
typedef uint32 web_url_construct_options_bf;

/**
 * @name URL construction functions (name/value pairs in single strings)
 *
 * These functions may be used to construct a URL from the given set of name
 * value pairs as well as the known URL parameters set/get using the
 * web_url_set_param and web_url_get_param functions. So as an example:
 *
 * <pre>
 *    web_url_set_param("a", "1");
 *    web_url_set_param("b", "2");
 *    web_url_construct_str(wdata, &ret_url, opts,
 *                          "action=blah", "template=foo", NULL);
 * </pre>
 *
 * will construct the url:
 *
 * <pre>
 *    /admin/launch?script=rh&action=blah&template=foo&var_a=1&var_b=2
 * </pre>
 *
 * Note these functions may also operate on strings that do no contain both 
 * name and value, and that wuco_html_esc_singular escaping option may be
 * used if escaping a string that is not a name=value pair (no '=').
 */
/*@{*/

/**
 * \param web_data Per-request state
 * \param opts URL construction options.
 * \param ... Parameters of type 'tstring *' where each string is of the
 * form 'name=value'.  NULL must be passed for the last parameter, as a
 * delimiter.
 * \retval ret_url The constructed url.  This is dynamically allocated
 * and the caller is responsible for freeing it.
 */
int web_url_construct(web_request_data *web_data, tstring **ret_url,
                      web_url_construct_options_bf opts, ...);

/**
 * \param web_data Per-request state
 * \param opts URL construction options.
 * \param ... Parameters of type 'const char *' where each string is of the
 * form 'name=value'.  NULL must be passed for the last parameter, as a
 * delimiter.
 * \retval ret_url The constructed url.  This is dynamically allocated
 * and the caller is responsible for freeing it.
 */
int web_url_construct_str(web_request_data *web_data, tstring **ret_url, 
                          web_url_construct_options_bf opts, ...);

/**
 * \param web_data Per-request state
 * \param opts URL construction options.
 * \param args Tstring array of args in the form 'name=value'
 * \retval ret_url The constructed url.  This is dynamically allocated
 * and the caller is responsible for freeing it.
 */
int web_url_construct_array(web_request_data *web_data, tstring **ret_url,
                            web_url_construct_options_bf opts,
                            const tstr_array *args);

/**
 * \param web_data Per-request state
 * \param opts URL construction options.
 * \param args String array of args in the form 'name=value'
 * \retval ret_url The constructed url.  This is dynamically allocated
 * and the caller is responsible for freeing it.
 */
int web_url_construct_array_str(web_request_data *web_data, tstring **ret_url,
                                web_url_construct_options_bf opts,
                                const str_array *args);

/*@}*/


/**
 * @name URL construction functions (name/value pairs in separate strings)
 *
 * See the comments for the web_url_construct non-paired versions.
 * These functions differ in that the variable arguments are NOT of
 * the form 'name=value'. Instead, the name and value are interleaved.
 * So for example:
 *
 *    web_url_construct_paired_str(wdata, &ret_url, opts,
 *                                 "a", "1", "b", "2", NULL);
 *
 * Would result in:
 *
 *    /admin/launch?script=rh&a=1&b=2
 *
 * The first version of this function expects interleaved tstring *s.
 * The second version (_str) expects normal c strings.
 */

/**
 * \param web_data per request state
 * \param opts URL Construction options, see web_url_construct_options.
 * \param ... variable interleaved name/value pairs, as 'const tstring *'.
 * \retval ret_url the constructed url.
 */
int web_url_construct_paired(web_request_data *web_data, 
                             tstring **ret_url,
                             web_url_construct_options_bf opts, ...);

/**
 * \param web_data per request state
 * \param opts URL Construction options, see web_url_construct_options.
 * \param ... variable interleaved name/value pairs, as 'const char *'.
 * \retval ret_url the constructed url.
 */
int web_url_construct_paired_str(web_request_data *web_data,
                                 tstring **ret_url,
                                 web_url_construct_options_bf opts, ...);

/**
 * \param web_data per request state
 * \param opts URL Construction options, see web_url_construct_options.
 * \param args Tstring array of interleaved name/value pairs.
 * \retval ret_url the constructed url.
 */
int web_url_construct_paired_array(web_request_data *web_data,
                                   tstring **ret_url,
                                   web_url_construct_options_bf opts,
                                   const tstr_array *args);

/**
 * \param web_data per request state
 * \param opts URL Construction options, see web_url_construct_options.
 * \param args String array of interleaved name/value pairs.
 * \retval ret_url the constructed url.
 */
int web_url_construct_paired_array_str(web_request_data *web_data, 
                                       tstring **ret_url,
                                       web_url_construct_options_bf opts,
                                       const str_array *args);

/**
 * Gets the original filename associated with a multipart file form field.
 * The original filename is the one supplied by the user from his local
 * machine, and may (will always?) include the full path of the file,
 * not just the filename. If the filename cannot be found (some browsers
 * don't seem to
 * supply it) then the tmp filename (just the filename only, not the full
 * path) will be returned. This function cannot differentiate between a
 * file form field and any other form field therefore if a non-file form
 * field name is given, then it will be returned exactly the same. If the
 * field is not found at all, NULL will be returned.
 *
 * CAUTION: since the path comes from the system running the browser, it
 * may follow different naming conventions from what we are used to.
 * For example, if the file was uploaded from Windows, the parts of the
 * filename may be delimited with backslashes instead of forward slashes.
 * Some of the APIs in file_utils.h are not equipped to deal with this.
 *
 * \param web_data per request state
 * \param name the name of the file field.
 * \param ret_name the original name of the file.
 * \return 0 on success, error code on failure.
 */
int web_get_original_filename(web_request_data *web_data, const tstring *name,
                              const tstring **ret_name);
int web_get_original_filename_str(web_request_data *web_data,
                                  const char *name, const tstring **ret_name);

/**
 * Check if the given request parameter exists.
 * \param web_data per request state
 * \param name the name of the parameter to find.
 * \param src the location to look for the parameter.
 * \return true if it exists, false if it does not.
 */
tbool web_request_param_exists(web_request_data *web_data,
                               const tstring *name, web_param_source src);
tbool web_request_param_exists_str(web_request_data *web_data, 
                                   const char *name, web_param_source src);

/**
 * Retrieve a request parameter.
 * If the requested parameter has multiple values, only the first value
 * found is returned. There are no guarantees that any ordering will be
 * preserved when this call is made therefore if you want to use a single
 * value from a parameter that has multiple values, you should use the mult
 * call below. The returned value will be NULL if the param is not found.
 * The returned value SHOULD NOT be modified or freed by the caller.
 * \param web_data per request state
 * \param name the name of the parameter to find.
 * \param src the location to look for the parameter.
 * \param ret_value the pointer that will contain the location of the value.
 * \return 0 on success, error code on failure.
 */
int web_get_request_param(web_request_data *web_data, const tstring *name,
                          web_param_source src,
                          const tstring **ret_value);
int web_get_request_param_str(web_request_data *web_data, const char *name,
                              web_param_source src,
                              const tstring **ret_value);

/**
 * Retrieve a request parameter that has multiple values.
 * The returned value will be NULL if the param is not found.
 * The returned value SHOULD NOT be modified by the caller.
 * \param web_data per request state
 * \param name the name of the parameter to find.
 * \param src the location to look for the parameter.
 * \param ret_values the pointer that will contain the location of the values.
 * \return 0 on success, error code on failure.
 */
int web_get_request_param_mult(web_request_data *web_data, const tstring *name,
                               web_param_source src,
                               const tstr_array **ret_values);
int web_get_request_param_mult_str(web_request_data *web_data,
                                   const char *name, web_param_source src,
                                   const tstr_array **ret_values);

/**
 * Read the value from a checkbox form field.
 *
 * HTML's plan is that if a checkbox is checked, the value sent up is 
 * the word "on".  If it's not, no value is sent up at all.  So it's
 * a rather simple check.  (We do validate that only the word "on" is
 * ever sent, as a sanity check.)  We mainly have this function to 
 * prevent code from mistakenly trying to read a boolean true/false
 * out of this field, or something (see bug 12545).
 */
int web_get_checkbox_tbool(web_request_data *web_data, const char *name,
                           web_param_source src, tbool *ret_checked);


/**
 * Walk through the params inside the given src. The given function
 * will be called for each parameter in the given src.
 * \param web_data per request state
 * \param src the param source.
 * \param func the iteration function.
 * \param param a param to pass to the function.
 * \return 0 on success, error code on failure.
 */
int web_foreach_request_param(web_request_data *web_data,
                              web_param_source src,
                              web_param_iterator_func func,
                              void *param);

/**
 * Dumps the names and values of all GET and POST parameters to
 * the logs at level NOTICE.  Intended for debugging purposes.
 */
int web_dump_params(web_request_data *web_data);

/**
 * Set a request parameter.
 * A value of NULL will remove the value if rp is rp_all otherwise,
 * nothing will happen. A src of ps_either combined with rp_none will
 * add the given value to params it finds in all sources.
 * \param web_data per request state
 * \param name the name of the parameter to set. (duped)
 * \param src the location in which to set the parameter.
 * \param value the value to set (duped).
 * \param rp the replacement policy.
 * \return 0 on success, error code on failure.
 */
int web_set_request_param(web_request_data *web_data,
                          const tstring *name, web_param_source src,
                          const tstring *value, web_replacement_policy rp);
int web_set_request_param_str(web_request_data *web_data,
                              const char *name, web_param_source src,
                              const char *value, web_replacement_policy rp);

/**
 * Clear out all post data.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_clear_post_data(web_request_data *web_data);

/**
 * Clear out all post data only if there is no error response code pending.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_maybe_clear_post_data(web_request_data *web_data);

/**
 * Gets a named cookie.
 * The returned value will be NULL if the variable was not found.
 * The returned value SHOULD NOT be modified or freed.
 * \param web_data per request state
 * \param name the name of the cookie.
 * \param ret_value pointer to the pointer that will hold the value.
 * \return 0 on success, error code on failure.
 */
int web_get_cookie(web_request_data *web_data, const tstring *name,
                   const tstring **ret_value);
int web_get_cookie_str(web_request_data *web_data, const char *name,
                       const tstring **ret_value);

/**
 * Walk through all the cookies. For each cookie, the given func
 * will be called.
 * \param web_data per request state
 * \param func the iteration function.
 * \param param a param to pass to the function.
 * \return 0 on success, error code on failure.
 */
int web_foreach_cookie(web_request_data *web_data,
                       web_cookie_iterator_func func, void *param);

/**
 * Sets a named cookie.
 * If has_expiration is FALSE, then the expiration_date field is NOT
 * honored and no expiration date will be set on the cookie. According
 * to the spec, this means that the cookie will expire when the session
 * ends (ie. the user closes the browser).
 * Passing a NULL for the value will remove the cookie. Even if you pass
 * NULL, you should correctly fill in the contents of the domain and path
 * in order for the browser to correctly identify the cookie to kill. Note
 * that by passing a NULL for the value, you can just pass false for the
 * has_expiration param since the time will be auto-calculated for you.
 * The given value will automatically be escaped for you.
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
 * \param web_data per request state
 * \param name the name of the cookie. (duped)
 * \param value the value of the cookie. (duped)
 * \param has_expiration whether or not the expiration_date is valid.
 * \param expiration_date the GMT expiration date in seconds.
 * \param path the valid path for this cookie. (duped)
 * \param domain the valid domain for this cookie. (duped)
 * \param http_only whether to prevent client-side script access.
 * \param secure_only whether a secure protocol is required (e.g. SSL/TLS)
 * \return 0 on success, error code on failure.
 */
int web_set_cookie(web_request_data *web_data, const tstring *name,
                   const tstring *value, tbool has_expiration,
                   lt_time_sec expiration_date,
                   const tstring *path, const tstring *domain,
                   tbool http_only, tbool secure_only);
int web_set_cookie_str(web_request_data *web_data, const char *name,
                       const char *value, tbool has_expiration,
                       lt_time_sec expiration_date,
                       const char *path, const char *domain,
                       tbool http_only, tbool secure_only);

/**
 * Gets a CGI variable.
 * The returned value will be NULL if the variable was not found.
 * The returned value SHOULD NOT be modified or freed.
 * \param web_data per request state
 * \param name the name of the variable.
 * \param ret_value pointer to the pointer that will hold the value.
 * \return 0 on success, error code on failure.
 */
int web_get_cgi_var(web_request_data *web_data, const tstring *name,
                    const tstring **ret_value);
int web_get_cgi_var_str(web_request_data *web_data, const char *name,
                        const tstring **ret_value);

/**
 * Sets a CGI variable.
 * Pass a NULL value to remove a custom variable.
 * \param web_data per request state
 * \param name the name of the variable (duped).
 * \param value the value of the variable (duped).
 * \return 0 on success, error code on failure.
 */
int web_set_cgi_var(web_request_data *web_data, 
                    const tstring *name, const tstring *value);
int web_set_cgi_var_str(web_request_data *web_data, const char *name,
                        const char *value);

/**
 * Gets the value of a custom variable.
 * Custom variables only live during the duration of a single request.
 * They are used to pass data between various modules handling a
 * single request.
 * \param web_data per request state
 * \param name the name of the variable.
 * \param ret_value pointer to a pointer that will hold the value.
 * \return 0 on success, error code on failure.
 */
int web_get_custom_var(web_request_data *web_data, const tstring *name,
                       const tstring **ret_value);
int web_get_custom_var_str(web_request_data *web_data, const char *name,
                           const tstring **ret_value);

/**
 * Sets a custom variable.
 * Custom variables only live during the duration of a single request.
 * They are used to pass data between various modules handling a
 * single request.
 * Pass a NULL value to remove a custom variable.
 * \param web_data per request state
 * \param name the name of the variable (duped).
 * \param value the value of the variable (duped).
 * \return 0 on success, error code on failure.
 */
int web_set_custom_var(web_request_data *web_data, const tstring *name,
                       const tstring *value);
int web_set_custom_var_str(web_request_data *web_data, const char *name,
                           const char *value);

/**
 * Gets the value of a session variable.
 * Session variables exist as long as the session is in existence.
 * \param web_data per request state
 * \param name the name of the variable.
 * \param ret_value pointer to a pointer that will hold the value.
 * \return 0 on success, error code on failure.
 */
int web_get_session_var(web_request_data *web_data, const tstring *name,
                        tstring **ret_value);
int web_get_session_var_str(web_request_data *web_data, const char *name,
                            tstring **ret_value);

/**
 * Sets a session variable.
 * Session variables exist as long as the session is in existence.
 * Pass a NULL value to remove a custom variable.
 * \param web_data per request state
 * \param name the name of the variable (duped).
 * \param value the value of the variable (duped).
 * \return 0 on success, error code on failure.
 */
int web_set_session_var(web_request_data *web_data, const tstring *name,
                        const tstring *value);
int web_set_session_var_str(web_request_data *web_data, const char *name,
                            const char *value);

/*-----------------------------------------------------------------------------
 * WEB DATA ESCAPING FUNCTIONS
 */

typedef enum {
    /** Default:  escape only the chars: \verbatim   <>&"'   \endverbatim */
    weeo_none = 0,

    /** Don't escape single or double quotes */
    weeo_except_quotes = 1 << 0,

    /** Don't escape single quotes */
    weeo_except_single_quotes = 1 << 1,

    /**
     * Don't escape RFC 3986 reserved characters (URI delimiters):
     * \verbatim    :/?#[]@!$&'()*+,;=    \endverbatim
     * 
     */
    weeo_except_reserved_uri = 1 << 2,

    /** Include forward slash in the default escape set */
    weeo_escape_forward_slash = 1 << 3,

    /** Include ASCII-7 control characters other than \\r \\n and \\t */
    weeo_escape_control_chars = 1 << 4,

    /**
     * Escape all characters except for those known to be safe in all HTML
     * contexts, and not explicitly on the exception list.  With no other
     * options, this will escape all characters other than ._-~ and
     * alpha-numeric.
     */
    weeo_escape_by_default = 1 << 5,
} web_esc_escape_options;

static const lc_enum_string_map web_esc_escape_options_map[] = {
    { weeo_none, "none" },
    { weeo_except_quotes, "except_quotes" },
    { weeo_except_single_quotes, "except_single_quotes" },
    { weeo_except_reserved_uri, "except_reserved_uri" },
    { weeo_escape_forward_slash, "escape_forward_slash" },
    { weeo_escape_control_chars, "escape_control_chars" },
    { weeo_escape_by_default, "escape_by_default" },
    { 0, NULL }
};

/**
 * Encode HTML special characters within an HTML entity string as HTML
 * '&' escape sequences.  This function should be applied anywhere dynamic
 * (and therefore untrusted) data elements are inserted into an HTML
 * element using the TCL interpreter (or any other interpreter).  This
 * function applies in most anywhere in the HTML body, including in tag
 * attributes, with the exception of URIs that are used for attribute values.
 * For URI's, the components of the URI must be escaped independently
 * The following special characters that would otherwise be interpreted as
 * valid HTML quoting characters are converted to their HTML symbols for
 * literals: '<' to "&lt;", '>' to "&gt;", '&' to "&amp;", the double quote
 * character to "&quot", and single quote to "&#x27;".  All other characters
 * are left unaltered.  If an illegal character (non-ASCII-7) needs to be
 * escaped, the sequence "&#xfffd;" will be emitted, which is the value for the
 * unicode character U+FFFD (the "replacement character"). This normally
 * displays as a white question-mark inside a black diamond.
 *
 * \param input the input string containing the entity to be escaped
 * \param  ret_output an escaped version of the input string
 * \return 0 on success, error code on failure.
 */
int web_esc_html_body_escape(const char *input, tstring **ret_output);

/**
 * Same as web_esc_html_body_escape(), but appends inout_output.
 */
int web_esc_html_body_escape_append_str(tstring *inout_output,
                                        const char *input);

/**
 * Same as web_esc_html_body_escape(), but alters the original string.
 */
int web_esc_html_body_escape_ts(tstring *inout_str);

/**
 * Encode HTML special characters within a an HTML entity string as HTML
 * escape sequences with options.  This function is the similar to
 * web_esc_html_body_escape(), but it allows certain default behaviors to be
 * overriden with the opts argument.  See web_esc_escape_options for details.
 *
 * All characters not considered part of the safe set, depending on
 * web_esc_escape_options, are converted either to &{entity} symbolic
 * form, or to hex character code escape sequences of the form:
 *     \verbatim &#xHH; \endverbatim
 * where HH is the hex value for the character.
 *
 * If an illegal character (non-ASCII-7) needs to be escaped, the sequence
 * "&#xfffd;" will be emitted, which is the value for the unicode character
 * U+FFFD (the "replacement character"). This normally displays as a white
 * question-mark inside a black diamond.
 *
 * \param input the input string containing the entity to be escaped
 * \param ret_output an escaped version of the input string
 * \param opts bitwise-and of web_esc_escape_options
 * \return 0 on success, error code on failure.
 */
int web_esc_html_escape_ex(const char *input, tstring **ret_output,
                           web_esc_escape_options opts);

/**
 * Same as web_esc_html_escape_ex(), but appends inout_output.
 */
int web_esc_html_escape_ex_append_str(tstring *inout_output,
                                      const char *input,
                                      web_esc_escape_options opts);

#ifndef TMSLIBS_DISABLE_WEB_OLD_ESCAPING
/**
 * web_url_escape() and web_url_unescape() are deprecated wrappers for URL /
 * URI escaping.  See TMSLIBS_DISABLE_WEB_OLD_ESCAPING in customer.mk if you
 * wish to omit these and other deprecated web APIs from the build.  If you
 * require CGI space+ encoding (e.g. for application/x-www-form-urlencoded
 * documents, please use lcgi_url_escape() / lcgi_url_unescape() instead.
 */

/**
 * DEPRECATED:  see web_esc_uri_component_escape().
 *
 * Encode the given url string in old CGI URL format.
 * Note that unlike web_esc_uri_component_escape(), spaces are encoded as '+'
 * characters.
 *
 * \param inout_url the URL component to be encoded.
 * \return 0 on success, error code on failure.
 */
int web_url_escape(tstring *inout_url);

/**
 * DEPRECATED:  see web_esc_uri_unescape().
 *
 * Decodes the given url string encoded in CGI URL format.
 * Note that unlike web_esc_uri_unescape(), '+' characters are decoded as
 * spaces.
 *
 * \param inout_url the full URL, or URL component to be decoded.
 * \return 0 on success, error code on failure.
 */
int web_url_unescape(tstring *inout_url);
#endif

/**
 * Escapes the given URI component string using percent-encoding format %HH
 * where HH is the hex value of the character (see RFC 3986, section 2.2).
 * All characters are percent-encoded except for unreserved characters -._~
 * and alphanumeric.  This should NOT be used on an assembled URI string,
 * as reserved characters would be encoded and loose their special meaning.
 * Note that this differs slightly from the older CGI percent-encoding
 * format still supported by most browsers and implemented in
 * web_url_escape().
 *
 * \param input the unescaped URI.
 * \param ret_output a new escaped tstring copy of the input
 * \return 0 on success, error code on failure.
 */
int web_esc_uri_component_escape(const char *input, tstring **ret_output);

/**
 * Unescapes a URI or URI component string encoded per RFC 3986, section 2.2,
 * This function essentially reverses all percent-encoding.  Note that this
 * differs from web_url_unescape() in that it will not decode '+'
 * characters as spaces.
 *
 * \param input the escaped URI.
 * \param ret_output a new unescaped tstring copy of the input
 * \return 0 on success, error code on failure.
 */
int web_esc_uri_unescape(const char *input, tstring **ret_output);

/**
 * Same as web_esc_uri_component_escape(), but operates on the existing string
 *
 * \param inout_str the component string to be escaped
 */
int web_esc_uri_component_escape_ts(tstring *inout_str);

/**
 * Same as web_esc_uri_unescape(), but operates on the existing string
 *
 * \param inout_str the component string to be unescaped
 */
int web_esc_uri_unescape_ts(tstring *inout_str);

/**
 * Same as web_esc_uri_component_escape(), but appends the component string to
 * the specified uri.
 *
 * \param uri the uri to be appended
 * \param component the component to be escaped and appended to the uri
 */
int web_esc_uri_component_escape_append(tstring *uri, const char *component);

/**
 * Same as web_esc_uri_component_escape_append(), but appends a preformed
 * string containing a name and value pair delimited by a name / value
 * separator character, such as '='.  Each component in the pair is
 * escaped, but the separator character is assumed to be a URI special
 * character, and is not escaped.  If the separator character is not found,
 * the string is presumed to contain a nameless value, and is escaped in
 * its entirety.  Note that it is acceptable for the separator character to
 * appear more than once in the string.  Only the first occurence is taken
 * to be the name / value delimiter, and any subsequent instances are
 * presumed to be part of the value, so these are escaped.
 *
 * \param uri the uri to be appended
 * \param component_nvp the nvp string to be escaped and appended to the uri
 * \param nvp_sep the delimiter that terminates the name substring.  The
 * value is everything that follows the first such delimiter, or the entire
 * string if the separator character is not found.
 */
int web_esc_uri_component_escape_append_nvp(tstring *uri,
                                            const char *component_nvp,
                                            char nvp_sep);

#ifndef TMSLIBS_DISABLE_WEB_OLD_ESCAPING
/**
 * Convert HTML special characters into entities.
 *
 * Note:  Use of this is been deprecated due to XSS vulnerability.
 * Whereas this function escapes only '<', '>', and '&', its successor
 * function, web_esc_html_body_escape(), also includes single and double
 * quotes, which could otherwise be used to terminate a quoted HTML data entity
 * and insert arbitrary text in HTML context.
 */
int web_escape_entities(const char *input, tstring **ret_output);
#endif

/**
 * For those who still need to support the deprecated behavior of
 * web_escape_entities(), this API provides equivalent escaping, and may be
 * used under a build that has TMSLIBS_DISABLE_WEB_OLD_ESCAPING defined in
 * your customer.mk file.  This is also the equivalent of calling
 * web_esc_html_escape_ex() with the weeo_except_quotes flag.  While not
 * recommended from a security perspective for most escaping contexts, it's
 * provided in case there is a legitimate need not to escape single or
 * double qoute characters in some context.
 */
int web_esc_html_body_except_quotes(const char *input,
                                    tstring **ret_output);

/**
 * Hex encode (\\xHH) Javascript values that contain untrusted data.  All
 * data values injected into Javascript should enclosed in either single or
 * double quotes, but even data within quoted strings must be protected
 * from escaping from Javascript context.  If your web module injects any
 * pre-formed Javascript into the the request handler output, all variable
 * data appearing inside quoted values should be escaped with this function
 * to prevent the data from being interpreted as Javascript or HTML.
 *
 * \param input the input string containing Javascript value to be escaped
 * \param ret_output a new escaped tstring copy of the input
 */
int web_esc_javascript_value_escape(const char *input,
                                    tstring **ret_output);


/**
 * Hex encode (HH) arbitrary text that may contain untrusted data.  Each
 * character in the input string is encoded as a two character ASCII hex
 * string.  This function is particularly useful when user data is used to
 * form an identifier, attribute or variable name in HTML.  In particular,
 * HTML4 standards require identifier string character compliance with SGML
 * standard ISO8879, so web attributes like CDATA, NAME, ID, etc. are
 * restricted to set of valid characters.  If you hex-encode a user data
 * value, it's guaranteed to contain only alphanumeric values, so it can be
 * safely used as part or all of an identifier name in HTML.  A web request
 * handler module that needs the original value may web_esc_hex_decode() it
 * in its supporting action handler.
 *
 * \param input the input text string to be encoded
 * \param ret_output a new hex-encoded tstring copy of the input
 */
int web_esc_hex_encode(const char *input, tstring **ret_output);

/**
 * Hex dencode (HH) text that was encoded using web_esc_hex_encode().
 *
 * \param input the input text string to be decoded
 * \param ret_output a new decoded tstring copy of the input
 */
int web_esc_hex_decode(const char *input, tstring **ret_output);

/*-----------------------------------------------------------------------------
 * WEB USER, AUTH, CAPABILITIES, SESSION, RESPONSE, AND OTHER SUPPORT FUNCTIONS
 */

/**
 * Attempts to authenticate a user based on the given user_id and password.
 * \param web_data per request state
 * \param service_name the PAM service name to use for authentication.
 *        Leave as NULL for the default (wsmd).
 * \param user_id the user id.
 * \param password the password.
 * \param remote_addr the remote address being logged in from
 * \param ret_success whether or not the auth was successful.
 * \param ret_msg contains message when ret_success is false.
 * \return 0 on success, error code on failure.
 */
int web_auth_user(web_request_data *web_data,
                  const tstring *service_name,
                  const tstring *user_id, const tstring *password,
                  const tstring *remote_addr,
                  tbool *ret_success, tstring **ret_msg);

/**
 * Check if the current session is still valid.
 * \param web_data per request state
 * \param update_activity should the last activity time of the user
 * being checked be updated?
 * \param ret_success holds true or false.
 * \return 0 on success, error code on failure.
 */
int web_validate_user(web_request_data *web_data, tbool update_activity,
                      tbool *ret_success);

/**
 * Update current session's inactivity time, unless we were told not to.
 * Check if the tms_noninteractive parameter was specified in the URL.
 * If it was not, OR if 'force' is 'true', then update it.
 *
 * This function internally uses wsmd's authentication (cookie) validation
 * action, which updates the last_activity field of the session.
 *
 * \param web_data per request state
 * \param force force the update even if URL specifies tms_noninteractive
 * \retval ret_success whether the update was successful or not
 * \retval ret_updated whether last activity time was updated or not
 * \return 0 on success, error code on failure.
 */
int web_maybe_update_activity(web_request_data *web_data, tbool force,
                              tbool *ret_success, tbool *ret_updated);

/**
 * Get the current session's session ID from wsmd.  This is a number
 * unique to the login session.
 *
 * \param web_data per request state
 * \retval ret_session_id The session ID (which would fit in a uint16),
 * or -1 if the cookie was not valid.
 */
int web_get_session_id(web_request_data *web_data, int32 *ret_session_id);

/**
 * Logout the user.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_deauth_user(web_request_data *web_data);

/**
 * Ask wsmd what is the value of the 'config_confirmed' flag for this session.
 * Also, determine if we even care about this flag (i.e. are we a CMC 
 * client under management of a CMC server, with the "confirm config edits"
 * flag set?)
 *
 * NOTE: if ret_config_need_confirm comes back false, then we won't try
 * to figure out ret_config_confirmed -- it will always come back false,
 * regardless of what state wsmd is holding.
 */
int web_config_confirmed_get(web_request_data *web_data, 
                             tbool *ret_config_need_confirm,
                             tbool *ret_config_confirmed);

/**
 * Tell wsmd to change the value of the 'config_confirmed' flag for this 
 * session.
 */
int web_config_confirmed_set(web_request_data *web_data, 
                             tbool config_confirmed);

/**
 * Send a message to the mgmtd. This is the lower level function. You
 * should try to use the functions in md_client.h.
 * \param web_data per request state
 * \param req the request to send.
 * \param ret_resp the response received.
 * \return 0 on success, error code on failure.
 */
int web_send_mgmt_msg(web_request_data *web_data, bn_request *req,
                      const bn_response **ret_resp);

/**
 * Set the result of a mgmt message.
 * This function looks at the code and sets either RB-SUCCESS or
 * RB-ERROR to display the message.
 *
 * Note that if you embed HTML directives in your reponse, you'll want to call:
 *   web_set_msg_encoding_opt(web_data, * wmeo_none)
 * to disable automatice HTML encoding of your response.
 *
 *
 * \param web_data per request state
 * \param code the code.
 * \param msg the msg.
 * \return 0 on success, error code on failure.
 */
int web_set_msg_result(web_request_data *web_data, uint32 code,
                       const tstring *msg);
int web_set_msg_result_str(web_request_data *web_data, uint32 code,
                           const char *msg);
int web_set_msg_result_str_fmt(web_request_data *web_data, uint32 code,
                               const char *msg_fmt, ...)
     __attribute__ ((format (printf, 3, 4)));

/**
 * Set the desired HTML result messsage encoding option for web module form
 * response message text appended through calls to web_set_msg_result*().  This
 * function sets a template flag for the requested encoding option.  See
 * web_msg_encoding_option for a full list of options.
 *
 * Actual encoding of the mesasge is handled by the tms-layout.tem tag function
 * <TMS-MSG> just prior to response emission.  The <TMS-MSG> tag is invoked
 * automatically (see login.tem) after each request has been handled.  Note
 * that if you have made any alterations to the TMS logic for this tag, you are
 * responsible for support of this.  This function sets the variable name
 * defined by WEB_VALUE_MSG_ENCODING to the specified encoding_opt value.
 *
 * This function is normally invoked only when the default encoding behavior,
 * defined above by web_msg_encoding_option_default, does not apply.  Under the
 * default build, the enocding default is wmeo_html_body_escape_transcode_ws.
 * Although not recommended, encoding may be disabled (wmeo_none) by default
 * (see WEB_RESPONSE_MSG_ENCODING_DISABLE in customer.h for more information).
 *
 * Normal response text may contain accidental (or intentional) HTML encoding.
 * It may also contain spaces and newlines that would be ignored by the HTML.
 * The wmeo_html_body_escape_transcode_ws option ensures that response text:
 *
 * A) is not interpreted as HTML, and
 * B) renders embedded spaces and newlines as <br> and &nbsp; respectively
 * 
 * Web modules that generate reponses contianing intentionally embedded HTML
 * should set the encoding_opt to wmeo_none to preserve the HTML embedded in
 * the response message.  The module must then take responsibility to escape
 * any untrusted data (anything derrived from a variable should be suspect),
 * and to perform whitespace transcoding if desired.
 *
 * \param web_data per request state
 * \param encoding_opt A web_msg_encoding_option flag value
 * \return 0 on success, error code on failure.
 */
int web_set_msg_encoding_opt(web_request_data *web_data,
                             const web_msg_encoding_option encoding_opt);

/**
 * Return the string value of the web result message encoding requested for
 * the web results response message.  See web_msg_encoding_option_map for
 * valid values.
 *
 * \param web_data per request state
 * \param ret_encoding_opt_str A string set to the value of the encoding 
 * currently requested, or to NULL if no encoding directive has been set yet.
 * \return 0 on success, error code on failure.
 */
int web_get_msg_encoding_opt_str(web_request_data *web_data,
                                 const tstring **ret_encoding_opt_str);

/**
 * Get the current message result.
 * \param web_data per request state
 * \param ret_code the result code or NULL if not needed.
 * \param ret_msg the result msg or NULL if not needed.
 * \return 0 on success, error code on failure.
 */
int web_get_msg_result(web_request_data *web_data, uint32 *ret_code,
                       const tstring **ret_msg);

/**
 * Returns 'true' if there is an error pending in this context (or if
 * the context is NULL), or 'false' if not.
 */
tbool web_result_has_error(web_request_data *web_data);

/**
 * Make sure that any errors and/or messages in the provided parameters
 * are reported in the Web UI's response.
 */
int web_report_results(web_request_data *web_data, 
                       int new_err, uint32 new_code, const char *new_msg);

/**
 * A convenience wrapper around web_report_results(), which takes the
 * message as a tstring which is freed by the function before
 * returning.
 */
int web_report_results_takeover(web_request_data *web_data, int new_err, 
                                uint32 new_code, tstring **inout_new_msg);

/**
 * Report any pending web results to the progress tracking mechanism
 * under a specified operation ID, and end that operation.  This should
 * be called from the 'bail' clause of the same function that began the
 * operation, to ensure that all errors that happen during the function
 * are tracked under the operation, and that the operation is ended.
 */
int web_report_results_progress(web_request_data *web_data,
                                md_client_context *mcc, const char *oper_id);

#ifdef PROD_FEATURE_CAPABS
/**
 * Check whether or not a user has a set of capabilities.
 * \param web_data per request state
 * \param caps the array of capabilities.
 * \param apply_filter Should we apply any filters on these capabilities?
 * (e.g. withhold set capabilities for CMC clients under management of a 
 * CMC server)
 * \param ret_code the result code: 0 if the user has all listed capabilities;
 * nonzero if the user is missing one or more of the capabilities.
 * \param ret_msg the return msg.
 * \return 0 on success, error code on failure.
 */
int web_has_capabilities(web_request_data *web_data,
                         const tstr_array *caps,
                         tbool apply_filter,
                         uint32 *ret_code, tstring **ret_msg);

/**
 * Return the set of capabilities that a user has.
 * \param web_data per request state
 * \param apply_filter Should we apply any filters on these capabilities?
 * (e.g. withhold set capabilities for CMC clients under management of a 
 * CMC server)
 * \retval ret_caps An array of capability strings, like "set_basic",
 * the user holds.
 * \return 0 on success, error code on failure.
 */
int web_get_capabilities(web_request_data *web_data,
                         tbool apply_filter,
                         tstr_array **ret_caps);
#endif /* PROD_FEATURE_CAPABS */

#ifdef PROD_FEATURE_ACLS
/**
 * Return the information needed to determine authorization for a user
 * under the ACL regime.
 *
 * \param web_data Per-request state
 * \retval ret_username The authenticated user's local username
 * \retval ret_auth_groups The set of authorization groups the user
 * belongs to.
 * \retval ret_oper_mask A mask showing which operations are permitted
 * for this user in general.  Normally this will be all of them (access
 * to operations is still limited based on context by the auth groups);
 * but in rare cases, certain operations like Sets may be forbidden,
 * in which case this mask would have 0 bits in the positions for those
 * operations.
 */
int web_get_acl_auth(web_request_data *web_data, tstring **ret_username, 
                     uint32_array **ret_auth_groups,
                     ta_oper_bf *ret_oper_mask);
#endif /* PROD_FEATURE_ACLS */

/**
 * Send a raw file to the user. This function sends output directly
 * to standard out bypassing the rest of the system.
 *
 * NOTE: the caller of this function MUST return war_override_output
 * to override other output (such as a template) which would otherwise
 * be served after the file, which can disrupt some browsers (see bug 12410).
 *
 * \param file_path the path to the file.
 * \param mime_type the mime type or NULL to auto-determine it.
 * \return 0 on success, error code on failure.
 */
int web_send_raw_file_str(const char *file_path, const char *mime_type);

/**
 * Same as web_send_raw_file_str(), except that if filename is
 * non-NULL, we also send a Content-Disposition header in the
 * response, telling the browser what filename to use if this file is
 * saved to disk, and a Content-Length header in case the browser
 * wants to display a progress bar.  The Content-Length number will be
 * derived automatically by checking the size of the file in the
 * filesystem.
 *
 * NOTE: the caller of this function MUST return war_override_output
 * to override other output.
 *
 * The 'flags' parameter is to permit future expansion, but is not
 * currently used for anything.
 */
int web_send_raw_file_with_fileinfo(const char *file_path,
                                    const char *mime_type,
                                    const char *filename,
                                    uint32 flags);

/**
 * NOTE: the caller of this function MUST return war_override_output
 * to override other output.
 */
int web_send_raw_file(const tstring *file_path, const tstring *mime_type);

/**
 * Send the raw output of a program to the user. This function sends output
 * directly to standard out bypassing the rest of the system.
 *
 * NOTE: the caller of this function MUST return war_override_output
 * to override other output.
 * 
 * IMPORTANT: note that ownership of prog_path and args WILL BE TAKEN OVER
 * by this function. Do NOT free these two items once you pass them in. The
 * exception is the prog_path argument in the _str version of this function.
 *
 * \param prog_path the path to the program.
 * \param args the program arguments.
 * \param mime_type the mime type of the output. Pass NULL for text/plain.
 * \return 0 on success, error code on failure.
 */
int web_send_raw_process_output(tstring *prog_path, tstr_array *args,
                                const char *mime_type);
int web_send_raw_process_output_str(const char *prog_path, tstr_array *args,
                                    const char *mime_type);


/*-----------------------------------------------------------------------------
 * WEB CORE
 */

/**
 * Initialize the web core.
 * Reads in form and query string parameters, etc...
 * \param post_fd fd to read POST data from
 * \param post_handling
 * \retval ret_web_data a pointer to per-request state
 * \return 0 on success, error code on failure.
 */
int web_init(web_request_data **ret_web_data, int post_fd, 
             web_post_data_handling post_handling);

/**
 * Shutdown the web core.
 * Deallocates any memory allocated during the initialization process.
 * \param inout_web_data per request state to free
 * \return 0 on success, error code on failure.
 */
int web_deinit(web_request_data **inout_web_data);

/**
 * Used to output the ending boundary for multipart responses.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_output_multipart_end_boundary(web_request_data *web_data);

/**
 * This function is designed to be non-intrusive and safe.  It is not designed
 * to strip all superfluous web_data whitespace, but only to strip useless
 * whitespace after any cgi header and prior to the substance of the HTML body.
 *
 * Use it to strip extraneous whitespace preceding actual HTML content from web
 * cgi output prior to sending it.  This is targetted at developers who may
 * view the raw source.  The end user will not see this whitespace since it is
 * non-functional in nature.  We do not strip anything after the
 * "<!DOCTYPE HTML" as this content might contain literal whitespace, e.g.
 * inside a &lt;pre&gt; tag.  Neither do we strip after the first non-white
 * character that we encounter, as we might not understand its purpose.
 * Output that does not contain a "<!DOCTYPE HTML" tag prefix will also not be
 * stripped.
 *
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_output_buffer_trim_pre_html_whitespace(web_request_data *web_data);

/**
 * Calling this function outputs any buffered contents.
 * It also generates the end boundary if we're in a multipart response.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_output_buffered_response(web_request_data *web_data);

/**
 * If called with true, this toggles the system into not rendering
 * anything including buffered data.
 * \param web_data per request state
 * \param overridden true or false.
 */
void web_output_overridden(web_request_data *web_data, tbool overridden);

/**
 * Has the output been overridden.
 * \param web_data per request state
 * \return true or false.
 */
tbool web_is_output_overridden(web_request_data *web_data);

/**
 * Member function to returned configured type converter
 * \param web_data per request state
 * \return type converter
 */
bn_type_converter *web_data_get_type_converter(web_request_data *web_data);

/**
 * Member function to return result code
 * \param web_data per request state
 * \return result code
 */
uint32 web_data_get_result_code(web_request_data *web_data);

/**
 * Member function to set result code
 * \param web_data per request state
 * \param code code to set
 */
int web_data_set_result_code(web_request_data *web_data, uint32 code);

/**
 * Member function to set login url
 * \param web_data per request state
 * \param login_url URL to redirect to if no authentication
 */
int web_data_set_login_url(web_request_data *web_data, char *login_url);

/*-----------------------------------------------------------------------------
 * WEB ACCESSORS
 */

/**
 * Load the cookie data into the global request structure.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_load_cookie_data(web_request_data *web_data);

/**
 * Load the query string data into the global request structure.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_load_query_string_data(web_request_data *web_data);

/**
 * Load the post data into the global request structure.
 * \param web_data per request state
 * \param post_fd fd to read POST data from
 * \param post_handling if we should provide parsed and raw POST data
 * \return 0 on success, error code on failure.
 */
int web_load_post_data(web_request_data *web_data, 
                       int post_fd,
                       web_post_data_handling post_handling);

/**
 * Get the raw post data.  This can only be called after the post data
 * has been read using web_load_post_data().
 */
int
web_get_raw_post_data(web_request_data *web_data, const tbuf **ret_raw_data);


/*-----------------------------------------------------------------------------
 * WEB SESSION INIT / DEINIT
 */

/**
 * Initialize the connection to the session manager.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_init_sessions(web_request_data *web_data);

/**
 * Deinitialize the connection to the session manager.
 * \param web_data per request state
 * \return 0 on success, error code on failure.
 */
int web_deinit_sessions(web_request_data *web_data);

/*-----------------------------------------------------------------------------
 * MISCELLANEOUS
 */

#ifdef __cplusplus
}
#endif

#endif /* __WEB_H_ */
