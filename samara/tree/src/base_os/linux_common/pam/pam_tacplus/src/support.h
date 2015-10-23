#include <security/pam_modules.h>

#ifndef CONF_FILE       
#define CONF_FILE       "/var/opt/tms/output/pam_tacplus_server.conf"
#endif /* CONF_FILE */

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 256
#endif /* BUFFER_SIZE */

/* support.c */
extern int _pam_parse(pam_handle_t *pamh, int argc, const char **argv, tacacs_conf_t *conf);

extern int initialize(pam_handle_t *pamh, tacacs_conf_t *conf, int accounting);

extern unsigned long _resolve_name(char *serv);

extern int tacacs_get_password(pam_handle_t *pamh, const char *prompt_msg,
			       int flags, int ctrl,
			       char **ret_password);

extern int pam_get_user_data(pam_handle_t *pamh, const char *prompt_msg,
                             int flags, int ctrl, char **ret_data);

extern int converse(pam_handle_t *pamh, int nargs,
		    struct pam_message **message,
		    struct pam_response **response);

extern void _pam_log(pam_handle_t *pamh, int err, const char *format, ...)
    __attribute__ ((format (printf, 3, 4)));


extern const char *_pam_get_terminal(pam_handle_t *pamh);

extern const char *_pam_get_rhost(pam_handle_t *pamh);

extern void release_config(tacacs_conf_t *conf);

