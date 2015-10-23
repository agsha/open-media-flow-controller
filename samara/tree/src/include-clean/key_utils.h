/*
 *
 * key_utils.h
 *
 *
 *
 */

#ifndef __KEY_UTILS_H_
#define __KEY_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "tstring.h"
#include "typed_arrays.h"
#include "tstr_array.h"

/* ========================================================================= */
/** \file key_utils.h Functions to help in launching and
 * manipulating processes.
 * \ingroup lc
 */

/* ------------------------------------------------------------------------- */
/** Generate public/private key-pair of type possibly for a given user string
 * \param key_type Input string
 * \param user_str Input string
 * \retval ret_pub_key_data Created public key
 * \retval ret_pub_key_len Length of created public key
 * \retval ret_priv_key_data Created private key
 * \retval ret_priv_key_len Length of created private key
 */
int lc_ssh_util_keygen(const char *key_type, const char *user_str,
                       char **ret_pub_key_data,
                       uint32 *ret_pub_key_len, char **ret_priv_key_data,
                       uint32 *ret_priv_key_len);

/* ------------------------------------------------------------------------- */
/** Get the attributes of all ssh hostkey keys in hostkey entry file
 * identified by key_path
 * \param key_path Path to the file containing ssh host keys
 * \retval ret_fps Optional array of finger prints of the keys
 * \retval ret_hosts Optional array of hosts
 * \retval ret_key_lengths Optional array of key lengths (key modulus in bits)
 */
int lc_ssh_util_keygen_entries_get_attrs(const char *key_path,
                                         tstr_array **ret_fps,
                                         tstr_array **ret_hosts,
                                         uint32_array **ret_key_lengths);


/* ------------------------------------------------------------------------- */
/** Get ssh finger print of given key identified by path
 * (DEPRECATED:  use lc_ssh_util_keygen_entries_get_attrs() instead, and pass
 * NULL if you don't want the key_length attribute)
 * \param key_path Path to the file containing ssh host keys
 * \retval ret_fps Array of finger prints of the keys
 * \retval ret_hosts Optional array of hosts
 */
int lc_ssh_util_keygen_finger_print(const char *key_path,
                                    tstr_array **ret_fps,
                                    tstr_array **ret_hosts);


/* ------------------------------------------------------------------------- */
/** Take a hostkey entry and break it up into its 3 components
 * \param host_entry Input host entry
 * \retval host_str host name buffer to fill in
 * \param host_str_size size of buffer
 * \retval ip_str Optional IP address if comma tuple of hostname
 * \param ip_str_size size of buffer
 * \retval ret_key_type_str Optional key type
 * \retval ret_key_str Optional key
 */
int lc_ssh_util_parse_hostkey_entry(const tstring *host_entry, char *host_str,
                                    uint32 host_str_size, char *ip_str,
                                    int32 ip_str_size,
                                    char **ret_key_type_str,
                                    char **ret_key_str);

#ifdef __cplusplus
}
#endif

#endif /* __KEY_UTILS_H_ */
