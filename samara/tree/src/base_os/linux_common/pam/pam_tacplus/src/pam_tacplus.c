/*
 * pam_tacplus.c PAM interface for TACACS+ protocol. Copyright
 * 1998,1999,2000 by Pawel Krawczyk <kravietz@ceti.pl> See end of this
 * file for copyright information. See file `CHANGES' for revision
 * history. 
 */

#include <stdio.h>
#include <stdlib.h>		/* malloc */
#include <syslog.h>
#include <netdb.h>		/* gethostbyname */
#include <sys/socket.h>		/* in_addr */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>		/* va_ */
#include <signal.h>
#include <string.h>		/* strdup */
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <pwd.h>

#include <security/pam_ext.h>

#include "tacplus.h"
#include "libtac.h"
#include "pam_tacplus.h"
#include "support.h"
#include "xalloc.h"
#include "magic.h"
#include "customer.h"
#include "pam_common.h"

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
/*
 * #define PAM_SM_PASSWORD 
 */

#include <security/pam_modules.h>
#include <security/pam_modutil.h>

/*
 * support.c 
 */
extern char    *tac_service;
extern char    *tac_protocol;
extern unsigned long _getserveraddr(char *serv);

/* Backward compatibility */
#if defined(TACACS_LOCAL_USER_SERVICE) && defined(TACACS_AUTHORIZATION_SERVICE)
#warning TACACS_LOCAL_USER_SERVICE is now called TACACS_AUTHORIZATION_SERVICE
#endif

#if defined(TACACS_LOCAL_USER_SERVICE) && !defined(TACACS_AUTHORIZATION_SERVICE)
#define TACACS_AUTHORIZATION_SERVICE TACACS_LOCAL_USER_SERVICE
#endif

#ifndef TACACS_AUTHORIZATION_SERVICE
#define TACACS_AUTHORIZATION_SERVICE "tms-login"
#endif

#ifndef TACACS_AUTHORIZATION_PROTOCOL
#define TACACS_AUTHORIZATION_PROTOCOL "unknown"
#endif

/*
 * address of server discovered by pam_sm_authenticate 
 */
static tacacs_server_t *active_server = NULL;

/*
 * accounting task identifier 
 */
static short int task_id = 0;

void            _int_free(pam_handle_t * pamh, void *x, int error_status);
static int      do_template_user(pam_handle_t * pamh,
				 tacacs_conf_t * config, int ctrl);
static int do_tac_authen(pam_handle_t *pamh, tacacs_conf_t *config, int ctrl,
			 int flags, tacacs_server_t *server, const char *user,
			 char *pass, const char *tty, const char *rhost,
			 int *ret_pam_code);

PAM_EXTERN int  _pam_account(pam_handle_t * pamh, int argc,
			     const char **argv, int type);
int             _pam_send_account(pam_handle_t *pamh, tacacs_conf_t *conf,
				  tacacs_server_t * server, int ctrl,
				  int type, char *user, const char *tty,
				  const char *rhost);

/*
 * Callback function used to free the saved return value for pam_setcred. 
 */
void
_int_free(pam_handle_t * pamh, void *x, int error_status)
{
    free(x);
}

static int
do_template_user(pam_handle_t * pamh, tacacs_conf_t * config, int ctrl)
{
    int             retval;
    const char     *tmpuser,
                   *finaluser;
    char            ubuf[1024];

    retval = pam_get_item(pamh, PAM_USER, (const void **) &tmpuser);
    if (retval != PAM_SUCCESS) {
	return (retval);
    }

    if (tmpuser) {
	ubuf[0] = '\0';
	snprintf(ubuf, sizeof(ubuf), "LOGIN_USER=%s", tmpuser);
	pam_putenv(pamh, ubuf);
    }

    if (tmpuser && config->template_user) {
	if (ctrl & PAM_TAC_DEBUG) {
	    syslog(LOG_DEBUG, "%s: Trying template user: %s",
		   __FUNCTION__, config->template_user);
	}

	/*
	 * If the given user name doesn't exist in the local password
	 * database, change it to the value given in the "template_user"
	 * option.
	 */
	if (pam_modutil_getpwnam(pamh, tmpuser) == NULL) {
	    retval = pam_set_item(pamh, PAM_USER, config->template_user);
	    if (ctrl & PAM_TAC_DEBUG) {
		syslog(LOG_DEBUG, "Using template user");
	    }
	}
    }

    retval = pam_get_item(pamh, PAM_USER, (const void **) &finaluser);
    if (retval != PAM_SUCCESS) {
	return (retval);
    }

    if (finaluser) {
	ubuf[0] = '\0';
	snprintf(ubuf, sizeof(ubuf), "AUTH_USER=%s", finaluser);
	pam_putenv(pamh, ubuf);
    }

    return (retval);
}

static int
do_tac_authen(pam_handle_t *pamh, tacacs_conf_t *config, int ctrl,
	      int flags, tacacs_server_t *server, const char *user,
	      char *pass, const char *tty, const char *rhost,
	      int *ret_pam_code)
{
    int err = 0;
    const char *err_msg = NULL;
    int tac_status;
    char *user_msg = NULL;
    int free_user_msg = FALSE;
    int local_user = 0;

    *ret_pam_code = PAM_SUCCESS;

    local_user = (pam_modutil_getpwnam(pamh, user) != NULL);

    while (1) {
	if (user_msg) {
	    err = tac_authen_send(config, server, user_msg, NULL, NULL, rhost);
	}
	else {
	    err = tac_authen_send(config, server, user, pass, tty, rhost);
	}
	if (err) {
	    _pam_log(pamh, LOG_ERR, "error sending auth req to TACACS+ server");
	    *ret_pam_code = PAM_AUTHINFO_UNAVAIL;
	    goto bail;
	}

	/* XXX really should handle the start/continue info better */
	if (free_user_msg) {
	    if (user_msg) {
		free(user_msg);
	    }
	    free_user_msg = FALSE;
	}
	user_msg = NULL;

	reset_server(server, FALSE);

	err = tac_authen_read(config, server, &err_msg, &tac_status);
	if (err) {
            if (local_user || aaa_log_unknown_usernames_flag) {
                if (tac_status == TAC_PLUS_AUTHEN_STATUS_FAIL) {
                    _pam_log(pamh, LOG_INFO,
                             "authentication failure; user=%s", user);
                }
                else {
                    _pam_log(pamh, LOG_INFO,
                             "authentication failure; user=%s msg=\"%s\"",
                             user, err_msg);
                }
            }
            else {
                if (tac_status == TAC_PLUS_AUTHEN_STATUS_FAIL) {
                    _pam_log(pamh, LOG_DEBUG,
                             "authentication failure; user=%s", user);
                    _pam_log(pamh, LOG_INFO,
                             "authentication failure; user not recognized");
                }
                else {
                    _pam_log(pamh, LOG_DEBUG,
                             "authentication failure; user=%s msg=\"%s\"",
                             user, err_msg);
                    _pam_log(pamh, LOG_INFO,
                             "authentication failure; user not recognized; msg=\"%s\"",
                             err_msg);
                }
            }
	    *ret_pam_code = PAM_AUTH_ERR;
	    goto bail;
	}

	switch (tac_status) {
	case TAC_PLUS_AUTHEN_STATUS_PASS:
	    /*
	     * OK, we got authenticated; save the server that accepted 
	     * us for pam_sm_acct_mgmt and exit the loop 
	     */
	    *ret_pam_code = PAM_SUCCESS;

	    active_server = server;
	    if (config->template_user && !config->use_vendor_attr) {
		/*
		 * In this configuration we wouldn't listen to any
		 * local-user attributes even if they are returned
		 * from the server, but we're not even going to ask,
		 * so try to switch to the template user during the
		 * authentication phase. 
		 */
		*ret_pam_code = do_template_user(pamh, config, ctrl);
	    }
	    else {
		/*
		 * Go ahead and do the base level template user processing
		 * which will set LOGIN_USER and a preliminary AUTH_USER
		 * just in case the 'pam_sm_acct_mgmt' routine is not
		 * called in the SSHv1 case (meaning local attribute
		 * processing is not done for v1).
		 */
		*ret_pam_code = do_template_user(pamh, config, ctrl);
	    }

	    goto bail;

	case TAC_PLUS_AUTHEN_STATUS_GETUSER:
	case TAC_PLUS_AUTHEN_STATUS_GETPASS:
	    /* XXX check flags and see if noecho should be set */
	    if (tac_status == TAC_PLUS_AUTHEN_STATUS_GETUSER) {
		*ret_pam_code = pam_get_user(pamh, (const char **) &user_msg,
			      *server->server_msg ? server->server_msg : NULL);
	    }
	    else if (tac_status == TAC_PLUS_AUTHEN_STATUS_GETPASS) {
		*ret_pam_code = tacacs_get_password
		    (pamh, 
		     *server->server_msg ? server->server_msg : "Password: ",
		     flags, ctrl, &user_msg);
		free_user_msg = TRUE;
	    }

	    if (*ret_pam_code != PAM_SUCCESS) {
		err = -1;
		goto bail;
	    }
	    break;

	case TAC_PLUS_AUTHEN_STATUS_GETDATA:
	    *ret_pam_code = pam_get_user_data
		(pamh, *server->server_msg ? server->server_msg : "",
		 flags, ctrl, &user_msg);
	    free_user_msg = TRUE;

	    if (*ret_pam_code != PAM_SUCCESS) {
		err = -1;
		goto bail;
	    }
	    break;

	default:
	    /* shouldn't reach this case as it should have been
	     * an error from 'tac_authen_read' 
	     */
	    err = -1;
	    *ret_pam_code = PAM_AUTH_ERR;
	    goto bail;
	}
    }

 bail:
    if (config->sockfd != -1) {
        close(config->sockfd);
        config->sockfd = -1;
    }
    return(err);
}

/*
 * These calls to release_config() are to free the dynamically allocated
 * config structs of most of our callbacks as they are returning.  The
 * exception is pam_sm_authenticate(), who has its own long-lived struct
 * that it does not want freed for it.  So that it can share these macros
 * with everyone else, it has a dummy config struct named 'config', which
 * release_config() just ignores, and holds its real config under a 
 * different variable name.
 */

#undef PAM_FAIL_CHECK
#define PAM_FAIL_CHECK if (retval != PAM_SUCCESS) {	\
	int *pret = malloc( sizeof(int) );		\
	*pret = retval;					\
	pam_set_data( pamh, "tac_setcred_return"	\
	              , (void *) pret, _int_free );	\
        release_config(&config);                        \
	return retval; }

#undef PAM_FAIL
#define PAM_RETURN(x) { \
	int *pret = malloc( sizeof(int) );		\
	*pret = x;					\
	pam_set_data( pamh, "tac_setcred_return"	\
	              , (void *) pret, _int_free );	\
        release_config(&config);                        \
	return x; }

static void
tms_pam_tacplus_cleanup_simple(pam_handle_t *pamh, void *data,
                               int error_status)
{
    if (data != NULL) {
        free(data);
    }

    return;
}

static void
pam_tacplus_release_config(pam_handle_t *pamh, void *data,
                           int error_status)
{
    tacacs_conf_t *conf = data;
    release_config(conf);
    free(conf);
}

/*
 * authenticates user on remote TACACS+ server returns PAM_SUCCESS if the
 * supplied username and password pair is valid 
 */
PAM_EXTERN int
pam_sm_authenticate(pam_handle_t * pamh, int flags,
		    int argc, const char **argv)
{
    int             ctrl,
                    retval;
    const char     *user;
    char           *pass;
    const char     *tty;
    const char     *rhost = NULL;

    /*
     * Dummy config struct so our PAM_FAIL_CHECK and PAM_RETURN calls will 
     * compile.  They will not do anything (the 'initialized' field is zero);
     * our real config is in persist_config.
     */
    tacacs_conf_t   config = {NULL, 0, NULL, 0, -1, 0, 0};

    /*
     * Unlike our other callbacks like pam_sm_acct_mgmt(), we need our
     * config struct to continue living after we return.  This is because
     * the 'active_server' variable is set to point into it, and then
     * referenced from later callbacks.  So if we freed it, that would
     * become a dangling pointer.  So instead, we allocate this ourselves
     * and then hand it over to pam_set_data().  This is not so anyone
     * can call pam_get_data() on it (XXX/EMT though maybe other callbacks
     * should do that, rather than reinitializing their own config structs?).
     * It is so that PAM will call the destructor at the appropriate time,
     * when pam_end() is called.
     */
    tacacs_conf_t   *persist_config = NULL;

    tacacs_server_t *server;
    int             status = PAM_AUTH_ERR;
    int             connected = 0;

    user = tty = NULL;
    pass = NULL;

    persist_config = (tacacs_conf_t *)malloc(sizeof(*persist_config));
    if (persist_config == NULL) {
        _pam_log(pamh, LOG_ERR, "Could not allocate space for config");
        return PAM_AUTHINFO_UNAVAIL;
    }
    memset(persist_config, 0, sizeof(*persist_config));

    (void)pam_set_data(pamh, TMS_PAM_TACPLUS_GLOBAL_CONFIG,
                       (void *)persist_config,
                       pam_tacplus_release_config);

    ctrl = _pam_parse(pamh, argc, argv, persist_config);

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: called (pam_tacplus v%hu.%hu.%hu)",
	       __FUNCTION__, PAM_TAC_VMAJ, PAM_TAC_VMIN, PAM_TAC_VPAT);
    }

    /* Clear this out, in case it was set before. */
    active_server = NULL;
    
    /* We haven't done authentication yet */
    (void) pam_set_data (pamh, TMS_PAM_TACPLUS_AUTHENTICATE_SUCCESS,
                         NULL, NULL);

    retval = pam_get_user(pamh, &user, NULL);
    PAM_FAIL_CHECK;

    /*
     * check that they've entered something, and not too long, either 
     */
    if ((user == NULL) || (*user == '\0') || (strlen(user) > MAXPWNAM)) {
	_pam_log(pamh, LOG_ERR, "User name was NULL, or too long");
	PAM_RETURN(PAM_USER_UNKNOWN);
    }

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: user [%s] obtained", __FUNCTION__, user);
    }

    retval = initialize(pamh, persist_config, FALSE);
    PAM_FAIL_CHECK;

    /*
     * XXX uwzgledniac PAM_DISALLOW_NULL_AUTHTOK 
     */

    retval = tacacs_get_password(pamh, "Password: ", flags, ctrl, &pass);
    if (retval != PAM_SUCCESS || pass == NULL) {
	_pam_log(pamh, LOG_ERR, "unable to obtain password");
	PAM_RETURN(PAM_CRED_INSUFFICIENT);
    }

    /*
     * If there was a password pass it to the next layer 
     */
    retval = pam_set_item(pamh, PAM_AUTHTOK, pass);
    if (retval != PAM_SUCCESS) {
	_pam_log(pamh, LOG_ERR, "unable to set password");
	PAM_RETURN(PAM_CRED_INSUFFICIENT);
    }

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: pass [%s] obtained", __FUNCTION__, pass);
    }

    tty = _pam_get_terminal(pamh);

    if (!strncmp(tty, "/dev/", 5)) {
	tty += 5;
    }

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: tty [%s] obtained", __FUNCTION__, tty);
    }

    rhost = _pam_get_rhost(pamh);
    if (ctrl & PAM_TAC_DEBUG) {
        syslog(LOG_DEBUG, "%s: rhost obtained [%s]", __FUNCTION__, rhost);
    }

    server = persist_config->server;
    while (server) {

	if (ctrl & PAM_TAC_DEBUG) {
	    syslog(LOG_DEBUG, "%s: trying srv %s", __FUNCTION__,
		   server->hostname);
	}

        connected = 0;
	if (!tac_connect_single(persist_config, server)) {
	    _pam_log(pamh, LOG_ERR, "connection failed srv %s: %m",
		     server->hostname);
	    if (!server->next) {	/* XXX check if OK */
		/* last server tried */
		_pam_log(pamh, LOG_ERR, "no more servers to connect");
		PAM_RETURN(PAM_AUTHINFO_UNAVAIL);
	    }
	} else {
            connected = 1;
	    reset_server(server, FALSE);
	    do_tac_authen(pamh, persist_config, ctrl, flags, server,
                          user, pass, tty, rhost, &status);
	    if (status == PAM_SUCCESS) {
		/* this server works, note this allows that all kinds
		 * of errors in the process will casue us to simply 
		 * try another server (unless not by config) */
		break;
	    }
	}
        if (persist_config->sockfd != -1) {
            close(persist_config->sockfd);
            persist_config->sockfd = -1;
        }
	/*
	 * if we are here, this means that authentication failed on
	 * current server; break if we are not allowed to probe another
	 * one, continue otherwise 
	 */
	if (connected && !(ctrl & PAM_TAC_FIRSTHIT)) {
	    break;
	}
	server = server->next;
    }


    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: exit", __FUNCTION__);
    }
    bzero(pass, strlen(pass));
    free(pass);
    pass = NULL;

    if (status == PAM_SUCCESS) {
        /*
         * We've successfully done authentication, let our account
         * management know.
         */
        (void) pam_set_data (pamh, TMS_PAM_TACPLUS_AUTHENTICATE_SUCCESS,
                             (void *) strdup("1"), 
                             tms_pam_tacplus_cleanup_simple);
    }

    PAM_RETURN(status);
    return status;

}				/* pam_sm_authenticate */

/*
 * Return a value matching the return value of pam_sm_authenticate, for
 * greatest compatibility. 
 * (Always returning PAM_SUCCESS breaks other authentication modules;
 * always returning PAM_IGNORE breaks PAM when we're the only module.)
 */
PAM_EXTERN int
pam_sm_setcred(pam_handle_t * pamh, int flags, int argc, const char **argv)
{
    int             retval,
                   *pret;

    retval = PAM_SUCCESS;
    pret = &retval;
    pam_get_data(pamh, "tac_setcred_return", (const void **) &pret);
    return *pret;
}

/*
 * authorizes user on remote TACACS+ server, i.e. checks his permission to 
 * access requested service returns PAM_SUCCESS if the service is allowed 
 */
PAM_EXTERN int
pam_sm_acct_mgmt(pam_handle_t * pamh, int flags,
		 int argc, const char **argv)
{
    int             retval,
                    ctrl,
                    status = PAM_AUTH_ERR;
    const char      *tty;
    struct areply   arep;
    struct tac_attrib *attr = NULL;
 /* char           *rhostname; */
 /* u_long          rhost = 0; */
    tacacs_conf_t   config;
    const char     *verified_local_user,
                   *finaluser;
    char            buff[128];
    const char     *login_user;
    char           *sep;
    int             local_user_len;
    char            ubuf[1024];
    const char     *did_authen = NULL;
    char           *colon = NULL;
    char            extra_uparams[LPC_EXTRA_UPARAMS_SIZE];

    char            login_user_copy[LPC_LOGIN_USER_MAX_LEN + 1];
    char            auth_user_copy[LPC_AUTH_USER_MAX_LEN + 1];
    const char     *rhost_str = NULL;
    char            rhost_copy[LPC_RHOST_MAX_LEN + 1];
    char            tty_copy[LPC_TTY_MAX_LEN + 1];
    char            trusted_auth_info[LPC_TRUSTED_AUTH_INFO_SIZE];
    char            env_buf[LPC_TRUSTED_AUTH_INFO_SIZE + 128];

    login_user = tty = NULL;


    /*
     * this also obtains service name for authorization this should be
     * normally performed by pam_get_item(PAM_SERVICE) but since PAM
     * service names are incompatible TACACS+ we have to pass it via
     * command line argument until a better solution is found ;) 
     */
    ctrl = _pam_parse(pamh, argc, argv, &config);

    if (ctrl & PAM_TAC_DEBUG) {
	struct in_addr  addr;

	syslog(LOG_DEBUG, "%s: called (pam_tacplus v%hu.%hu.%hu)",
	       __FUNCTION__, PAM_TAC_VMAJ, PAM_TAC_VMIN, PAM_TAC_VPAT);

        if (active_server) {
            bcopy(&(active_server->ip), &addr.s_addr, sizeof(addr.s_addr));
            syslog(LOG_DEBUG, "%s: active server is [%s]", __FUNCTION__,
                   inet_ntoa(addr));
        }
    }

    /*
     * Checks if user has been successfully authenticated by TACACS+; we
     * cannot solely authorize user if they haven't been authenticated
     * or have been authenticated by a method other than TACACS+ .
     *
     */
    retval = pam_get_data(pamh, TMS_PAM_TACPLUS_AUTHENTICATE_SUCCESS,
                      (const void **) &did_authen);
    if (retval != PAM_SUCCESS || !did_authen || strcmp(did_authen, "1")) {
        _pam_log(pamh, LOG_DEBUG,
        "User was not authenticated by TACACS+, not doing account management");
	PAM_RETURN(PAM_USER_UNKNOWN);
    }

    /*
     * Get user name given at the prompt (set in do_template_user) 
     * We are assuming LOGIN_USER is what PAM_USER would have been
     * if not for the do_template_user processing
     */
    login_user = pam_getenv(pamh, "LOGIN_USER");
    
    if (!login_user) {
	/* This must have been set previously */
	_pam_log(pamh, LOG_ERR, "unable to obtain username");
	PAM_RETURN(PAM_USER_UNKNOWN);
    }

    /*
     * We want to refer to this later, but each call to pam_get_item()
     * may overwrite the static buffer that it has returned to us.
     * So keep our own copy.
     */
    lpc_strlcpy(login_user_copy, login_user, sizeof(login_user_copy));

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: username obtained [%s]", __FUNCTION__,
	       login_user);
    }

    retval = initialize(pamh, &config, FALSE);
    if (retval != PAM_SUCCESS) {
	_pam_log(pamh, LOG_ERR, "unable to read in server configuration");
	PAM_RETURN(PAM_AUTHINFO_UNAVAIL);
    }

    tty = _pam_get_terminal(pamh);

    if (!strncmp(tty, "/dev/", 5))
	tty += 5;

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: tty obtained [%s]", __FUNCTION__, tty);
    }

    /*
     * XXX temporarily disabled retval = pam_get_item(pamh, PAM_RHOST,
     * (const void **) &rhostname); 
     */
    /*
     * if this fails, this means the remote host name was not supplied by
     * the application, and there's no need to report error; if it was,
     * then we appreciate this :) 
     */
    /*
     * XXX this is lame 
     */
    /*
     * if(retval == PAM_SUCCESS && rhostname != NULL && *rhostname !=
     * '\0') { rhost = _resolve_name(rhostname); rhostname = NULL; 
     */
    /*
     * if _resolve_name succeded, rhost now contains IP address in binary
     * form, rhostname is prepared for storing its ASCII representation 
     */
    /*
     * if (ctrl & PAM_TAC_DEBUG) { syslog(LOG_DEBUG, "%s: rhost obtained
     * [%lx]", __FUNCTION__, rhost); } } 
     */

    rhost_str = _pam_get_rhost(pamh);
    if (ctrl & PAM_TAC_DEBUG) {
        syslog(LOG_DEBUG, "%s: rhost obtained [%s]", __FUNCTION__, rhost_str);
    }
    rhost_copy[0] = '\0';
    lpc_strlcpy(rhost_copy, rhost_str, sizeof(rhost_copy));

    /*
     * This should not be possible, due to our check above, but there
     * could always be some (non-obvious) error case.
     */
    if (!active_server) {
	_pam_log(pamh, LOG_ERR,
                 "could not find active TACACS+ server for authorization");
	PAM_RETURN(PAM_AUTH_ERR);
    }

    /*
     * XXX Original code would not proceed with the authorization request
     * if a service and protocol were not set. 
     */
#if 0
    if (tac_service && *tac_service != '\0') {
	tac_add_attrib(&attr, "service", tac_service);
    }
    if (tac_protocol && *tac_protocol != '\0') {
	tac_add_attrib(&attr, "protocol", tac_protocol);
    }
#endif

    tac_add_attrib(&attr, "service", TACACS_AUTHORIZATION_SERVICE);
    tac_add_attrib(&attr, "protocol", TACACS_AUTHORIZATION_PROTOCOL);

#if 0				/* XXX wouldn't be set ever because it's
				 * commented out above */
    if (rhost) {
	struct in_addr  addr;
	bcopy(&rhost, &addr.s_addr, sizeof(addr.s_addr));
	tac_add_attrib(&attr, "ip", inet_ntoa(addr));
    }
#endif

    if (!tac_connect_single(&config, active_server)) {
	_pam_log(pamh, LOG_ERR, "TACACS+ server unavailable");
	status = PAM_AUTH_ERR;
        tac_free_attrib(&attr);
	goto ErrExit;
    }

    reset_server(active_server, FALSE);
    retval = tac_author_send(&config, active_server, (char *) login_user,
			     tty, rhost_str, attr);

    /*
     * this is no longer needed 
     */
    tac_free_attrib(&attr);

    if (retval < 0) {
	_pam_log(pamh, LOG_ERR, "error getting authorization");
	status = PAM_AUTH_ERR;
	goto ErrExit;
    }

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: sent authorization request", __FUNCTION__);
    }

    tac_author_read(&config, active_server, &arep);

    if (arep.status != AUTHOR_STATUS_PASS_ADD &&
	arep.status != AUTHOR_STATUS_PASS_REPL) {
#ifdef TACACS_AUTHORIZATION_FAIL_IGNORE
        _pam_log(pamh, LOG_ERR, "Ignoring TACACS+ authorization failure for [%s]",
                 login_user);
        status = PAM_SUCCESS;
        goto ErrExit;
#endif
        _pam_log(pamh, LOG_ERR, "TACACS+ authorization failed for [%s]",
                 login_user);
        if (arep.msg && *arep.msg) {
            pam_prompt(pamh, PAM_ERROR_MSG, NULL, "%s", arep.msg);
        }
        status = PAM_PERM_DENIED;
        goto ErrExit;
    }

    if (ctrl & PAM_TAC_DEBUG) {
	syslog(LOG_DEBUG, "%s: user [%s] successfully authorized",
	       __FUNCTION__, login_user);
    }
	
    status = PAM_SUCCESS;

    /*
     * set PAM_RHOST if 'addr' attribute was returned from server
     * and look for the "local user" attribute.
     */
    local_user_len = strlen(TACACS_LOCAL_USER_NAME_ATTR);
    verified_local_user = NULL;
    extra_uparams[0] = '\0';

    attr = arep.attr;
    while (attr != NULL) {
	if (ctrl & PAM_TAC_DEBUG) {
	    syslog(LOG_DEBUG, "%s: attribute found = %s",
		   __FUNCTION__, attr->attr);
	}

	sep = index(attr->attr, '=');
	if (sep == NULL) {
	    sep = index(attr->attr, '*');
	}
	if (sep == NULL) {
	    /*
	     * XXX should there always be a separator character? 
	     */
	    _pam_log(pamh, LOG_WARNING, "%s: invalid attribute `%s',"
		   " no separator", __FUNCTION__, attr->attr);
	    break;
	}

	if (!strncmp(attr->attr, "addr", 4)) {
	    bcopy(++sep, buff, attr->attr_len - 5);
	    buff[attr->attr_len - 5] = '\0';

	    if (isdigit(*buff))
		retval = pam_set_item(pamh, PAM_RHOST, buff);
	    if (retval != PAM_SUCCESS)
		_pam_log(pamh, LOG_WARNING,
		       "%s: unable to set remote address for PAM",
		       __FUNCTION__);
	    else if (ctrl & PAM_TAC_DEBUG)
		syslog(LOG_DEBUG, "%s: set remote addr to `%s'",
		       __FUNCTION__, buff);
	}

	if (config.use_vendor_attr &&
	    !strncmp(attr->attr, TACACS_LOCAL_USER_NAME_ATTR,
		     local_user_len)) {
	    if (ctrl & PAM_TAC_DEBUG) {
		syslog(LOG_DEBUG, "%s: Trying to use local user "
		       "attribute", __FUNCTION__);
	    }

	    bcopy(++sep, ubuf, attr->attr_len - (local_user_len + 1));
	    ubuf[attr->attr_len - (local_user_len + 1)] = '\0';

            /*
             * At this point, ubuf contains what we got from the
             * TACACS_LOCAL_USER_NAME_ATTR attribute.  If extra roles
             * are enabled, it may have some after a ':' character.
             */
            colon = strchr(ubuf, ':');
            if (colon) {
                *colon = '\0';

                /*
                 * Make sure the string is a respectable length.  But don't
                 * try to fix lpc_trusted_auth_info_sep_char characters;
                 * we'd rather than the library caught those and omitted
                 * the entire user parameters string.
                 */
                lpc_purify_string(colon + 1, "Extra roles from TACACS+",
                                  LPC_EXTRA_UPARAMS_MAX_LEN, 0, 0, ',', NULL);

                lpc_strlcpy(extra_uparams, colon + 1, sizeof(extra_uparams));
            }

	    if (pam_modutil_getpwnam(pamh, ubuf)) {
		/*
		 * Local user from attribute exists on system, only try
		 * mapping if user from prompt is not local
		 */
		verified_local_user = ubuf;
		if (pam_modutil_getpwnam(pamh, login_user) == NULL) {
		    pam_set_item(pamh, PAM_USER, verified_local_user);
		    if (ctrl & PAM_TAC_DEBUG) {
			syslog(LOG_DEBUG,
			       "Using Local User = %s",
			       verified_local_user);
		    }
		}
	    }
	}

	attr = attr->next;
    }

    /*
     * Do not need to set PAM_USER to config.template_user here
     * if no local user attribute was encountered and was valid
     * anymore since it should have been set in pam_sm_authenticate
     * due to SSHv1 not calling this routine.
     */

    /*
     * PAM_USER may have changed if a valid local user attribute
     * was returned and the username at the prompt was not local.
     */
    retval = pam_get_item(pamh, PAM_USER, (const void **) &finaluser);
    if (retval != PAM_SUCCESS)
	goto ErrExit;

    if (finaluser) {
	ubuf[0] = '\0';
	snprintf(ubuf, sizeof(ubuf), "AUTH_USER=%s", finaluser);
	pam_putenv(pamh, ubuf);
        lpc_strlcpy(auth_user_copy, finaluser, sizeof(auth_user_copy));
    }
    else {
        auth_user_copy[0] = '\0';
    }

    retval = pam_get_item(pamh, PAM_TTY, (const void **) &tty);
    if (tty && retval == PAM_SUCCESS) {
        lpc_strlcpy(tty_copy, tty, sizeof(tty_copy));
    }
    else {
        tty_copy[0] = '\0';
    }

    retval = lpc_get_trusted_auth_info(pamh, lpcam_tacacs, login_user_copy,
                                       auth_user_copy, rhost_copy, tty_copy,
                                       extra_uparams, trusted_auth_info,
                                       sizeof(trusted_auth_info));
    if (retval == 0) {
        env_buf[0] = '\0';
        snprintf(env_buf, sizeof(env_buf), "TRUSTED_AUTH_INFO=%s",
                 trusted_auth_info);
        pam_putenv(pamh, env_buf);
    }
    else {
        pam_syslog(pamh, LOG_WARNING, "Unable to set TRUSTED_AUTH_INFO");
        retval = 0;
    }

    /* Complain if we messed up the "local only" case */
    if (config.template_user && !config.use_vendor_attr) {
        if ((!login_user || !finaluser) || 
            (strcmp(finaluser, login_user) &&
             strcmp(finaluser, config.template_user))) {
            _pam_log(pamh, LOG_ERR, 
                     "Unexpected user mapping from %s to %s",
                     login_user, finaluser);
        }
    }

    /* If the final username isn't one known locally, fail this. */
    if (!finaluser || !pam_modutil_getpwnam(pamh, finaluser)) {
        _pam_log(pamh, LOG_INFO,
                 "Could not find local user, permission denied; user=%s",
                 finaluser ? finaluser : ":NULL:");
        status = PAM_PERM_DENIED;
    }

    /*
     * free returned attributes 
     */
    if (arep.attr != NULL)
	tac_free_attrib(&arep.attr);

  ErrExit:

    if (config.sockfd != -1) {
        close(config.sockfd);
        config.sockfd = -1;
    }

    PAM_RETURN(status);
    return status;
}				/* pam_sm_acct_mgmt */

/*
 * sends START accounting request to the remote TACACS+ server returns PAM 
 * error only if the request was refused or there were problems connection 
 * to the server 
 */
/*
 * accounting packets may be directed to any TACACS+ server, independent
 * from those used for authentication and authorization; it may be also
 * directed to all specified servers 
 */
PAM_EXTERN int
pam_sm_open_session(pam_handle_t * pamh, int flags,
		    int argc, const char **argv)
{
    task_id = (short int) magic();
    return (_pam_account(pamh, argc, argv, TAC_PLUS_ACCT_FLAG_START));
}				/* pam_sm_open_session */

/*
 * sends STOP accounting request to the remote TACACS+ server returns PAM
 * error only if the request was refused or there were problems connection 
 * to the server 
 */
PAM_EXTERN int
pam_sm_close_session(pam_handle_t * pamh, int flags,
		     int argc, const char **argv)
{
    return (_pam_account(pamh, argc, argv, TAC_PLUS_ACCT_FLAG_STOP));

}				/* pam_sm_close_session */

PAM_EXTERN int
_pam_account(pam_handle_t * pamh, int argc, const char **argv, int type)
{
    int             retval;
    static int      ctrl;
    char           *user;
    const char     *tty;
    const char     *rhost;
    const char     *typemsg;
    tacacs_conf_t   config;
    tacacs_server_t *server;
    int             status = PAM_SESSION_ERR;

    user = NULL;
    tty = NULL;
    rhost = NULL;

    typemsg = (type == TAC_PLUS_ACCT_FLAG_START) ? "START" : "STOP";
    /*
     * debugging if(type == TAC_PLUS_ACCT_FLAG_STOP) sleep(60); 
     */

    /*
     * when we are sending STOP packet we no longer have uid 0 
     */
    /*
     * if(type == TAC_PLUS_ACCT_FLAG_START) 
     */
    ctrl = _pam_parse(pamh, argc, argv, &config);

    if (ctrl & PAM_TAC_DEBUG)
	syslog(LOG_DEBUG, "%s: [%s] called (pam_tacplus v%hu.%hu.%hu)",
	       __FUNCTION__, typemsg, PAM_TAC_VMAJ, PAM_TAC_VMIN,
	       PAM_TAC_VPAT);

    retval = pam_get_item(pamh, PAM_USER, (const void **) &user);
    if (retval != PAM_SUCCESS || user == NULL || *user == '\0') {
	_pam_log(pamh, LOG_ERR, "%s: unable to obtain username", __FUNCTION__);
	return PAM_SESSION_ERR;
    }

    if (ctrl & PAM_TAC_DEBUG)
	syslog(LOG_DEBUG, "%s: username [%s] obtained", __FUNCTION__,
	       user);

    retval = initialize(pamh, &config, FALSE);
    if (retval != PAM_SUCCESS) {
	_pam_log(pamh, LOG_ERR, "unable to read in server configuration");
	return PAM_AUTHINFO_UNAVAIL;
    }

    tty = _pam_get_terminal(pamh);

    if (!strncmp(tty, "/dev/", 5))
	tty += 5;

    if (ctrl & PAM_TAC_DEBUG)
	syslog(LOG_DEBUG, "%s: tty [%s] obtained", __FUNCTION__, tty);

    rhost = _pam_get_rhost(pamh);

    if (ctrl & PAM_TAC_DEBUG) {
        syslog(LOG_DEBUG, "%s: rhost obtained [%s]", __FUNCTION__, rhost);
    }

    /*
     * checks for specific data required by TACACS+, which should be
     * supplied in command line 
     */
    if (tac_service == NULL || *tac_service == '\0') {
	_pam_log(pamh, LOG_ERR, "TACACS+ service type not configured");
	return PAM_AUTH_ERR;
    }
    if (tac_protocol == NULL || *tac_protocol == '\0') {
	_pam_log(pamh, LOG_ERR, "TACACS+ protocol type not configured");
	return PAM_AUTH_ERR;
    }

    /*
     * when this module is called from within pppd or other application
     * dealing with serial lines, it is likely that we will get hit with
     * signal caused by modem hangup; this is important only for STOP
     * packets, it's relatively rare that modem hangs up on accounting
     * start 
     */
    if (type == TAC_PLUS_ACCT_FLAG_STOP) {
	signal(SIGALRM, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
    }

    if (!(ctrl & PAM_TAC_ACCT)) {
	/*
	 * normal mode, send packet to the first available server 
	 */

	status = PAM_SUCCESS;


	server = tac_connect(&config, config.server, TRUE);
	if (!server) {
	    _pam_log(pamh, LOG_ERR, "%s: error sending %s - no servers",
		     __FUNCTION__, typemsg);
	    status = PAM_SESSION_ERR;
	}
	if (ctrl & PAM_TAC_DEBUG)
	    syslog(LOG_DEBUG, "%s: connected with fd=%d", __FUNCTION__,
		   config.sockfd);

	reset_server(server, FALSE);
        retval = _pam_send_account(pamh, &config, server, ctrl, type, user,
                                   tty, rhost);
        if (retval < 0) {
	    _pam_log(pamh, LOG_ERR, "%s: error sending %s",
		     __FUNCTION__, typemsg);
	    status = PAM_SESSION_ERR;
	}

        if (config.sockfd != -1) {
            close(config.sockfd);
            config.sockfd = -1;
        }

	if (ctrl & PAM_TAC_DEBUG) {
	    syslog(LOG_DEBUG, "%s: [%s] for [%s] sent",
		   __FUNCTION__, typemsg, user);
	}

    } else {
	/*
	 * send packet to all servers specified 
	 */

	status = PAM_SESSION_ERR;

	server = config.server;
	while (server) {
	    if (!tac_connect_single(&config, server)) {
		_pam_log(pamh, LOG_WARNING, "%s: error sending %s (fd)",
			 __FUNCTION__, typemsg);
		server = server->next;
		continue;
	    }

	    if (ctrl & PAM_TAC_DEBUG)
		syslog(LOG_DEBUG, "%s: connected with fd=%d (srv %s)",
		       __FUNCTION__, config.sockfd, server->hostname);

	    reset_server(server, FALSE);
	    retval = _pam_send_account(pamh, &config, server,
				       ctrl, type, user, tty, rhost);


	    /*
	     * return code from function in this mode is status of the
	     * last server we tried to send packet to 
	     */
	    if (retval < 0)
		_pam_log(pamh, LOG_WARNING, "%s: error sending %s (acct)",
			 __FUNCTION__, typemsg);
	    else {
		status = PAM_SUCCESS;
		if (ctrl & PAM_TAC_DEBUG)
		    syslog(LOG_DEBUG, "%s: [%s] for [%s] sent",
			   __FUNCTION__, typemsg, user);
	    }

            if (config.sockfd != -1) {
                close(config.sockfd);
                config.sockfd = -1;
            }

	    server = server->next;
	}

    }				/* acct mode */

    if (type == TAC_PLUS_ACCT_FLAG_STOP) {
	signal(SIGALRM, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
    }

    return status;
}

int
_pam_send_account(pam_handle_t *pamh, tacacs_conf_t * conf, tacacs_server_t * server, int ctrl,
		  int type, char *user, const char *tty, const char *r_addr)
{
    char            buf[40];
    struct tac_attrib *attr;
    int             retval, status = -1;
    const char *err_msg;

    if (!server) {
        status = -1;
        goto ErrExit;
    }

    attr = (struct tac_attrib *) xcalloc(1, sizeof(struct tac_attrib));

    sprintf(buf, "%lu", time(0));
    tac_add_attrib(&attr,
		   (type ==
		    TAC_PLUS_ACCT_FLAG_START) ? "start_time" : "stop_time",
		   buf);
    sprintf(buf, "%hu", task_id);
    tac_add_attrib(&attr, "task_id", buf);
    tac_add_attrib(&attr, "service", tac_service);
    tac_add_attrib(&attr, "protocol", tac_protocol);
    /*
     * XXX this requires pppd to give us this data 
     */
    /*
     * tac_add_attrib(&attr, "addr", ip_ntoa(ho->hisaddr)); 
     */

    reset_server(server, FALSE);
    retval = tac_account_send(conf, server, type, user, tty, r_addr, attr);

    /*
     * this is no longer needed 
     */
    tac_free_attrib(&attr);

    if (retval < 0) {
	_pam_log(pamh, LOG_WARNING, "%s: send %s accounting failed (task %hu)",
		 __FUNCTION__,
		 (type == TAC_PLUS_ACCT_FLAG_START) ? "start" : "stop",
		 task_id);
	status = -1;
	goto ErrExit;
    }

    if (tac_account_read(conf, server, &err_msg)) {
	_pam_log(pamh, LOG_WARNING, "%s: accounting %s failed (task %hu)",
		 __FUNCTION__,
		 (type == TAC_PLUS_ACCT_FLAG_START) ? "start" : "stop",
		 task_id);
	status = -1;
	goto ErrExit;
    }

    status = 0;

  ErrExit:
    if (conf->sockfd != -1) {
        close(conf->sockfd);
        conf->sockfd = -1;
    }
    return status;
}

#ifdef PAM_SM_PASSWORD
/*
 * no-op function for future use 
 */
PAM_EXTERN int
pam_sm_chauthtok(pam_handle_t * pamh, int flags,
		 int argc, const char **argv)
{
    tacacs_conf_t   config;
    int             ctrl;

    ctrl = _pam_parse(pamh, argc, argv, &config);

    if (ctrl & PAM_TAC_DEBUG)
	syslog(LOG_DEBUG, "%s: called (pam_tacplus v%hu.%hu.%hu)",
	       __FUNCTION__, PAM_TAC_VMAJ, PAM_TAC_VMIN, PAM_TAC_VPAT);

    return PAM_SUCCESS;
}				/* pam_sm_chauthtok */
#endif


#ifdef PAM_STATIC

struct pam_module _pam_tacplus_modstruct {
    "pam_tacplus",
	pam_sm_authenticate,
	pam_sm_setcred,
	pam_sm_acct_mgmt, pam_sm_open_session, pam_sm_close_session,
#ifdef PAM_SM_PASSWORD
    pam_sm_chauthtok
#else
    NULL
#endif
};
#endif

/*
 * Copyright 1998 by Pawel Krawczyk <kravietz@ceti.com.pl>
 *                                              All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
