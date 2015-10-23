/*
 *
 * Filename:  ssld_mgmt.h
 * Date:      2010/09/23
 * Author:    Thilak Raj S
 *
 * (C) Copyright 2009-10 Juniper  Networks, Inc.
 * All rights reserved.
 *
 *
 */
#ifndef SSLD_MGMT_H
#define SSLD_MGMT_H



#define NKN_MAX_SSL_KEYS         64
#define NKN_MAX_SSL_KEY_STR     16

#define NKN_MAX_SSL_CSR         64
#define NKN_MAX_SSL_CSR_STR     16


#define NKN_MAX_SSL_CERTS      256
#define NKN_MAX_SSL_HOST       256
#define NKN_MAX_SSL_CERT_STR    16
#define NKN_MAX_SSL_VHOST_STR   63
#define NKN_SSL_ALLOWED_CHARS   "/\\*:|`\"?"

#define NKN_SSLD_DELIVERY_MAX_PORTS   64
#define NKN_SSLD_DELIVERY_MAX_INTF    16


typedef struct ssl_cert_node_data_st {
	char *name;
	char *cert_name;
	char *passphrase;
} ssl_cert_node_data_t;

typedef struct ssl_key_node_data_st {
	char *name;
	char *key_name;
	char *passphrase;
} ssl_key_node_data_t;

typedef struct ssl_vhost_node_data_st {
	char *name;
	ssl_cert_node_data_t *p_ssl_cert;
	ssl_key_node_data_t *p_ssl_key;
	char *cipher;
} ssl_vhost_node_data_t;

typedef struct ssl_global_data_st {
    char *cipher;
    char *cert;
    char *key;
} ssl_global_data_t;


#endif // SSLD_MGMT_H
