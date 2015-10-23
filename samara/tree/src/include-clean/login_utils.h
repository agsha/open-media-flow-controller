/*
 *
 * login_utils.h
 *
 *
 *
 */

#ifndef __LOGIN_UTILS_H_
#define __LOGIN_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/*
 * Copyright (c) 2000 Andre Lucas.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 ** loginrec.h:  platform-independent login recording and lastlog retrieval
 **/

#include "common.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

/**
 ** you should use the login_* calls to work around platform dependencies
 **/

/*
 * login_netinfo structure
 */

union login_netinfo {
	struct sockaddr sa;
	struct sockaddr_in sa_in;
	struct sockaddr_storage sa_storage;
};

/*
 *   * logininfo structure  *
 */
/* types - different to utmp.h 'type' macros */
/* (though set to the same value as linux, openbsd and others...) */
#define LTYPE_FAILED_LOGIN 6
#define LTYPE_LOGIN    7
#define LTYPE_LOGOUT   8

/* string lengths - set very long */
#define LINFO_PROGSIZE 64
#define LINFO_LINESIZE 64
#define LINFO_NAMESIZE 128
#define LINFO_HOSTSIZE 256

typedef struct logininfo {
	char       progname[LINFO_PROGSIZE];     /* name of program (for PAM) */
	int        progname_null;
	short int  type;                         /* type of login (LTYPE_*) */
	int        pid;                          /* PID of login process */
	int        uid;                          /* UID of this user */
	char       line[LINFO_LINESIZE];         /* tty/pty name */
	char       username[LINFO_NAMESIZE];     /* login username */
	char       hostname[LINFO_HOSTSIZE];     /* remote hostname */
	/* 'exit_status' structure components */
	int        exit;                        /* process exit status */
	int        termination;                 /* process termination status */
	/* struct timeval (sys/time.h) isn't always available, if it isn't we'll
	 * use time_t's value as tv_sec and set tv_usec to 0
	 */
	unsigned int tv_sec;
	unsigned int tv_usec;
	union login_netinfo hostaddr;       /* caller's host address(es) */
} logininfo;

/*
 * The main API routines to use 
 */

void lu_record_login(pid_t, const char *, const char *, uid_t,
		     const char *, struct sockaddr *, socklen_t);

void lu_record_logout(pid_t, const char *, const char *);

/*
 * Records that the user has failed to log in.
 */
void lu_record_failed_login(pid_t pid, const char *tty,
                            const char *user, uid_t uid,
                            const char *host,
                            struct sockaddr *addr, socklen_t addrlen);

int lu_rewind_user_login_file(void);

int lu_close_user_login_file(void);

int lu_get_user_entry(tbool rewind_file, logininfo **ret_logininfo);

#ifdef __cplusplus
}
#endif

#endif /* __LOGIN_UTILS_H_ */
