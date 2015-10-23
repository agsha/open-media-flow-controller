/*
 * (C) Copyright 2008 Nokeena Networks, Inc
 *
 * Definitions used commonly across the server.
 */
#ifndef SSL_DEFS_H
#define SSL_DEFS_H
#include <limits.h>
#include <sys/time.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <atomic_ops.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <linux/netfilter_ipv4.h>
#include "openssl/e_os2.h"
#include "openssl/lhash.h"
#include "openssl/bn.h"
#include "openssl/x509.h"
#include "openssl/err.h"
#include "openssl/ssl.h"
#include "nkn_defs.h"

#ifndef TRUE
#define FALSE    (0)
#define TRUE    (!FALSE)
#endif

#define SSL_DEFAULT_IDLE_TIMEOUT 60
#define CA_CERT_PATH "/config/nkn/ca"

extern int ssl_idle_timeout;

/* This is a context that we pass to callbacks */
typedef struct tlsextctx_st {
   const char * servername;
   BIO * biodebug;
   int extension_error;
} tlsextctx;

/* Structure passed to cert status callback */

typedef struct tlsextstatusctx_st {
   /* Default responder to use */
   char *host, *path, *port;
   int use_ssl;
   int timeout;
   BIO *err;
   int verbose;
} tlsextstatusctx;

static pthread_mutex_t cipher_lock;

static tlsextstatusctx tlscstatp = {NULL, NULL, NULL, 0, -1, NULL, 0};

void nkn_ssl_interfacelist_callback(char * ssl_interface_list_str);
void ssl_lib_init(void);

void log_ssl_stack_errors(void) ;

#ifdef HTTP2_SUPPORT
int http2_init(void);
#endif

#endif /* SSL_DEFS_H */
