/*
 * Copyright 1997,1998,1999,2000 by Pawel Krawczyk <kravietz@ceti.pl>
 *
 * See http://www.ceti.com.pl/~kravietz/progs/tacacs.html
 * for details.
 *
 */

#ifndef _AUTH_TAC_H
#define _AUTH_TAC_H

#if defined(DEBUGTAC) && !defined(TACDEBUG)
#define TACDEBUG(x)	syslog x;
#else
#define TACDEBUG(x)
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include <netinet/in.h>

typedef struct tacacs_server_t {
    struct tacacs_server_t *next;
    struct in_addr ip;
    u_short port;
    char *hostname;
    char *secret;
    int timeout;
    u_char seq_no;
    u_char auth_type;
    u_char service;

    /* Fields to hold possible responses back from the server */
    char *server_msg;
    char *data;
} tacacs_server_t;

typedef struct tacacs_conf_t {
    tacacs_server_t *server;
    int retries;
    const char *template_user;
    int use_vendor_attr;
    int sockfd;
    int debug;
    int initialized;
} tacacs_conf_t;

/* version.c */
extern int tac_ver_major;
extern int tac_ver_minor;
extern int tac_ver_patch;

/* header.c */
extern u_int session_id;

/* connect.c */
extern tacacs_server_t *tac_connect(tacacs_conf_t *conf,
				    tacacs_server_t *server, int loop);
extern tacacs_server_t *tac_connect_single(tacacs_conf_t *conf,
					   tacacs_server_t *server);
extern void reset_server(tacacs_server_t *server, int clear_seq);

extern int tac_authen_send(tacacs_conf_t *conf, tacacs_server_t *server,
			   const char *user, char *pass, const char *tty,
			   const char *r_addr);
extern  int tac_authen_read(tacacs_conf_t *conf, tacacs_server_t *server,
			    const char **ret_err_msg, int *ret_status);
extern HDR *_tac_req_header(u_char type, u_char seq_no);
extern u_char protocol_version(int msg_type, int var, int type);
extern void _tac_crypt(char *secret, u_char *buf, HDR *th, int length);
extern u_char *_tac_md5_pad(char *secret, int len, HDR *hdr);
extern void tac_add_attrib(struct tac_attrib **attr, const char *name,
                           const char *value);
extern void tac_free_attrib(struct tac_attrib **attr);
extern int tac_account_send(tacacs_conf_t *conf, tacacs_server_t *server,
			    int type, char *user, const char *tty, const char *rem_addr,
			    struct tac_attrib *attr);
extern int tac_account_read(tacacs_conf_t *conf, tacacs_server_t *server,
			    const char **ret_err_msg);
extern int _tac_check_header(HDR *th, int type, tacacs_server_t *server,
			     const char **ret_err_msg);
extern int tac_author_send(tacacs_conf_t *conf, tacacs_server_t *server,
			   char *username, const char *tty, const char *r_addr,
                           struct tac_attrib *attr);
extern void tac_author_read(tacacs_conf_t *conf, tacacs_server_t *server,
			    struct areply *arep);

#endif

