/*
 * Copyright 1997-2000 by Pawel Krawczyk <kravietz@ceti.pl>
 *
 * See http://www.ceti.com.pl/~kravietz/progs/tacacs.html
 * for details.
 *
 * messages.c  Various messages returned to user.
 */

const char *system_err_msg =
    "Authentication error, please contact administrator.";
const char *protocol_err_msg = "Protocol error.";
const char *author_ok_msg = "Service granted.";
const char *author_fail_msg = "Service not allowed.";
const char *author_err_msg = "Protocol error.";
const char *no_server_err_msg = "No server specified";
const char *hdr_chk_err_msg = "Header check failed";
const char *auth_fail_msg = "TACACS+ authentication failed";
const char *bad_login_msg = "Login incorrect";
const char *unexpected_status_msg = "Unexpected message status";
