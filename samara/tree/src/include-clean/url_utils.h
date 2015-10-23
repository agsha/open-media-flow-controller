/*
 *
 * url_utils.h
 *
 *
 *
 */

#ifndef __URL_UTILS_H_
#define __URL_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "ttypes.h"
#include "tstring.h"
#include "common.h"
#include "ftrans_ssh.h"

/*
 * NOTE: lft_url_protocol_prefix_map (in url_utils.c) must be kept
 * in sync.
 */
/*
 * XXX/SML: maybe rename this simply lft_protocol during file transfer
 * header consolidation.
 */
typedef enum {
    lftup_none = LFT_PROTOCOL_NONE,
    lftup_scp = LFT_PROTOCOL_SCP,  /* SSH Secure Copy Protocol   */
    lftup_sftp = LFT_PROTOCOL_SFTP, /* SSH File Transfer Protocol */
    lftup_http = LFT_PROTOCOL_HTTP,
    lftup_https = LFT_PROTOCOL_HTTPS,
    lftup_ftp = LFT_PROTOCOL_FTP,
    lftup_tftp = LFT_PROTOCOL_TFTP,
} lft_url_protocol;

extern const lc_enum_string_map lft_url_protocol_prefix_map[];


/* ------------------------------------------------------------------------- */
/** \file url_utils.h Parse various URLs into components 
 * \ingroup lc
 */

/**
 * Take an SCP URL and return the various components of it.
 * An scp URL takes the form:
 * 
 * scp://[username[:password]@]hostname[:port]/path/filename
 *
 * \param scp_url The URL to parse
 * \retval ret_user The extracted user name
 * \retval ret_password The extracted password
 * \retval ret_hostname The extracted hostname.  Note that if the hostname
 * in the URL was enclosed in square brackets (generally to permit an IPv6
 * address, since colons are otherwise disallowed in hostnames), they are
 * removed here.
 * \retval ret_port The extracted (sshd) service port.  Note that if
 * ':port' is not present, NULL will be returned, and if it is present but
 * empty, the empty string will be returned.  Also, since this function is
 * agnostic to service name resolution, it is up to the caller to interpret
 * the port string and convert to a numeric value as needed.  This may be
 * done by calling lu_url_service_port_str_to_port_num().
 * \retval ret_path The extracted path
 */

int lu_parse_scp_url(const char *scp_url, char **ret_user,
                     char **ret_password, char **ret_hostname,
                     char **ret_port, char **ret_path);


/**
 * Enhanced version of lu_parse_scp_url() which also returns the
 * offsets in the string at which the password was found.
 *
 * Note that if the password found was of zero length (there was an
 * '@' immediately following the ':'), password_offset_begin will point
 * to the '@' sign, and password_offset_end will point to the * ':',
 * reflecting a length of 0.
 */
int lu_parse_scp_url_offset(const char *scp_url, char **ret_user, 
                            char **ret_password, char **ret_hostname,
                            char **ret_port, char **ret_path,
                            int32 *ret_password_offset_begin,
                            int32 *ret_password_offset_end);

/**
 * Like lu_parse_scp_url(), but for SFTP URLs.
 */
int lu_parse_sftp_url(const char *sftp_url, char **ret_user,
                      char **ret_password, char **ret_hostname,
                      char **ret_port, char **ret_path);


/**
 * Like lu_parse_scp_url_offset(), but for SFTP URLs.
 */
int lu_parse_sftp_url_offset(const char *sftp_url, char **ret_user, 
                             char **ret_password, char **ret_hostname,
                             char **ret_port, char **ret_path,
                             int32 *ret_password_offset_begin,
                             int32 *ret_password_offset_end);


/**
 * Like lu_parse_scp_url() and lu_parse_sftp_url(),
 * but with an additional parameter to specify which URL protocol.
 */
int
lu_parse_ssh_pseudo_url(lft_ssh_protocol protocol,
                        const char *pseudo_url, char **ret_user,
                        char **ret_password, char **ret_hostname,
                        char **ret_port, char **ret_path);


/**
 * Like lu_parse_scp_url_offset() and lu_parse_sftp_url_offset(),
 * but with an additional parameter to specify which URL protocol.
 */
int
lu_parse_ssh_pseudo_url_offset(lft_ssh_protocol protocol,
                               const char *pseudo_url, char **ret_user,
                               char **ret_password, char **ret_hostname,
                               char **ret_port, char **ret_path,
                               int32 *ret_password_offset_begin,
                               int32 *ret_password_offset_end);


/**
 * Take a URL to be used with Curl (protocol http, https, or ftp),
 * and return its components.  If a password was found, also return
 * the beginning and ending offset of the password in the URL string.
 *
 * See comments in lu_parse_scp_url_offset() regarding zero-length
 * passwords.
 *
 * Note: CURL may not accept passwords in HTTP or HTTPS URLs, only FTP.
 * But a user might still enter a password (for whatever reason), and
 * we still want to detect it.  So we do not attempt to distinguish
 * between protocols.
 *
 * If a password has '@' or '/' in it, it needs to be escaped using
 * '%' followed by two hex digits representing the ASCII code of the
 * character desired.  That's "%40" for '@', and "%2f" for '/'.
 */
int lu_parse_curl_url_offset(const char *url, char **ret_username, 
                             char **ret_password, char **ret_hostname, 
                             char **ret_port, char **ret_path,
                             int32 *ret_password_offset_begin,
                             int32 *ret_password_offset_end);

/**
 * Take a TFTP URL and return the various components of it
 *
 * \param tftp_url The URL to parse
 * \param ret_hostname The extracted hostname
 * \param ret_path The extracted path
 */
int lu_parse_tftp_url(const char *tftp_url, char **ret_hostname,
                      char **ret_path);


/**
 * Take a URL of undetermined protocol, and extract a password from it, if
 * there is one.  If none is found, NULL is returned for the password
 * with no error.  If the URL is not for a supported protocol, or is
 * ill-formed, lc_err_bad_path will be returned.
 *
 * Iff a password is found, ret_start_offset and ret_end_offset are set to
 * the offsets in the url string where the password began, and ended,
 * respectively.
 *
 * See comments in lu_parse_scp_url_offset() regarding zero-length
 * passwords.
 */
int lu_parse_url_password(const char *url, char **ret_password,
                          int32 *ret_start_offset, int32 *ret_end_offset);


/**
 * Take a URL of undetermined protocol, find the password, if there is
 * one, and obfuscate it so that the URL may be displayed (e.g. in the
 * logs).  If none is found, the original URL is returned unchanged.
 *
 * If the URL is invalid, or for an unsupported protocol, the entire URL
 * string will be obfuscated, and no error is returned.  This is to help
 * prevent inadvertantly leaking a password.
 *
 * \param url The original URL string.
 *
 * \param ret_url_pw_obfuscated A copy of the URL with the password obfuscated.
 * The caller must free the copy.
 *
 * \param ret_changed Optional.  Return whether the URL string was altered by
 * obfuscation (i.e. whether it had a password field if the URL was valid).
 */
int lu_obfuscate_url_password(const char *url,
                              tstring **ret_url_pw_obfuscated,
                              tbool *ret_changed);


/**
 * Given a string that may contain a service port value, convert it to a port
 * number.  The port string may be NULL or empty, in which case 0 is returned
 * to signify that no port was specified and the either a default port number
 * should be used, or the port should be rejected, however this function does
 * not generate any error in this case.  If the port string contains a
 * non-numeric value, or a value out of uint16 range, an error is returned.
 *
 * \param port_str The input string which may contain a port number.
 *
 * \param ret_port_num Return variable for the port number if present,
 * 0 if not present or invalid.
 *
 * \param ret_err_msg Pointer to a message constant describing any input error.
 * If no error, points to the empty string.
 */
int
lu_url_service_port_str_to_port_num(const char *port_str, uint16 *ret_port_num,
                                    const char **ret_err_msg);


/**
 * Take a URL of undetermined protocol, check it against protocols
 * supported in the lft_url_protocol_prefix_map of protocols and return
 * lc_err_bad_type if the protocol is not supported.  In either case, the
 * schema prefix containing {protocol}://, and the url body follwing the
 * the schema may be returned optionally, and must be freed if requested.
 * If not a valid URL format, these will be returned as NULL.
 *
 * \param url The original URL string.
 *
 * \param ret_url_protocol The protocol enum if valid, else lftup_none.
 *
 * \param ret_url_protocol_prefix A copy of the URL schema string,
 * including the '://' schema delimiters.  The caller must free the copy.
 *
 * \param ret_url_body A copy of the URL body following the schema prefix.
 * The caller must free the copy.
 *
 */

int
lu_parse_url_prefix(const char *url,
                    lft_url_protocol *ret_url_protocol,
                    tstring **ret_url_protocol_prefix,
                    tstring **ret_url_body);

#ifdef __cplusplus
}
#endif

#endif /* __URL_UTILS_H_ */
