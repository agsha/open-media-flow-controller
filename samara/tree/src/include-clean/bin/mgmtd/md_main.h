/*
 *
 * src/bin/mgmtd/md_main.h
 *
 *
 *
 */

#ifndef __MD_MAIN_H_
#define __MD_MAIN_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "typed_arrays.h"
#include "libevent_wrapper.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "md_crypt.h"
#include "mdb_db.h"
#include "md_mod_reg.h"
#include "md_mod_reg_internal.h"
#include "proc_utils.h"

/* ================================================== */
/* Mamangement Daemon Global */
/* ================================================== */


extern md_module_array *md_mod_array;
extern mdb_reg_state *md_reg_state;
extern lew_context *md_lwc;
extern gcl_handle *md_gclh;
extern tbool md_commit_mode_one_shot;
extern tbool md_exiting;
extern int32 md_db_revision_id;
extern int32 md_db_commit_count;
extern int32 md_request_count;
extern mdb_db *md_initial_db;
extern int md_syslog_facility;

extern md_crypt_context *md_global_crypt_context;

typedef struct md_event_state {
    struct event *mes_ev;
    struct timeval mes_tv;
} md_event_state;
TYPED_ARRAY_HEADER_NEW_NONE(md_event_state, md_event_state *);
int md_event_state_array_new(md_event_state_array **ret_arr);
extern md_event_state_array *md_module_event_state_array;


/* Per-gcl session mgmtd specific state */
typedef struct md_session_state {
    int_ptr mss_session_id;       /* The key */
    gcl_session *mss_session;
    tbool mss_session_closed;     /* Closed session, but may have child */

    pid_t mss_async_child_pid;    /* -1 if not waiting for a child */

    bn_ipv4addr mss_remote_addr;  /* Where user logged in from, 0 if IPv6 */
    bn_inetaddrz mss_remote_inetaddrz; /* Where is user logged in from? */
    tstring *mss_remote_hostname; /* Remote hostname, if specified */
    tstring *mss_client_descr;    /* User-friendly description of peer */
    tbool mss_login_done;         /* Have we received a login action? */
    tstring *mss_username_local;  /* Local username, if specified.  GCL session
                                   * has only remote username, */
    tstring *mss_line;            /* Line (TTY without "/dev/") */
    tstring *mss_auth_method;     /* Auth method used for login */
    lt_dur_ms mss_orig_auth_uptime_ms; /* Uptime stamp from PAM (validated) */

    /*
     * Extra roles from auth server.  These are validated at the time of
     * login, to be either built-in roles or user-defined roles.
     * User-defined roles may be deleted at runtime, and would then be
     * removed from this list.
     */
    tstr_array *mss_extra_roles;

    /*
     * Did we receive and process a TRUSTED_AUTH_INFO string in a login
     * action?  If so, the following fields were validated and are now
     * more trusted than they would have been otherwise:
     *   - mss_remote_addr, mss_remote_inetaddrz, mss_remote_hostname
     *   - mss_line
     *   - mss_username_local
     *   - (The remote username held by the GCL)
     *
     * Furthermore, the following fields are populated where they would
     * have been empty before:
     *   - mss_auth_method
     *   - mss_extra_roles
     *   - mss_orig_auth_uptime_ms
     */
    tbool mss_trusted;
} md_session_state;

TYPED_ARRAY_HEADER_NEW_NONE(md_session_state, md_session_state *);
extern md_session_state_array *md_session_states;
int md_session_state_lookup_by_id(int_ptr gcl_sess_id, 
                                  md_session_state **ret_session_state,
                                  uint32 *ret_session_idx);

int md_db_open(mdb_db **ret_db, mdb_reg_state *reg_state, const char *db_name);

/* Does a test commit of a db.  This does not do an apply. */
int md_db_commit_test(const mdb_db *db, uint16 *ret_commit_code,
                      tstring **ret_return_msg);

/**
 * How to handle saves, upgrade fallback backups, etc. when opening a
 * config db.
 */
typedef enum {
    /**
     * On saves, should creating a '.bak' file be disabled for this db?
     */
    mdof_save_backup_disable = 1 << 0,

    /** 
     * Should we disable saving off a fallback (hashed name) copy of the
     * source db when an upgrade is done?  Note, if this flag is
     * specified, mdof_upgrade_save_disable likely should be too, as
     * otherwise any upgrade will happen in place, without a way for an
     * older version of the system to find or read the given config db.
     */
    mdof_upgrade_write_fallback_disable = 1 << 1,

    /**
     * Should we disable saving back to the source db after an upgrade
     * is done?
     */
    mdof_upgrade_save_disable = 1 << 2,

    /**
     * Should we disable fallback entirely?  If this is set, opening a
     * too-new database will fail, even if a fallback exists.
     */
    mdof_fallback_disable = 1 << 3,

    /** 
     * Should we disable saving off a fallback (hashed name) copy of the
     * source db when a fallback is done? 
     */
    mdof_fallback_write_fallback_disable = 1 << 4,

} md_db_open_flags;

/* Bit field of ::bn_query_node_flags ORed together */
typedef uint32 md_db_open_flags_bf;

int md_db_open_full(mdb_db **ret_db, mdb_reg_state *reg_state, 
                    const char *db_name, 
                    md_db_open_flags_bf open_flags);

tbool md_db_valid_db_name(const char *db_name);
int md_db_upgrade_module(mdb_db *inout_db, md_module *mod, 
                         int32 current_db_ver);
int md_db_versioned_name(const char *db_name, 
                         const char *product_name, const char *version_hash,
                          tstring **ret_curr_path, tstring **ret_ver_path);
int md_db_set_module_version(mdb_db *inout_db, const char *module_name, 
                             uint32 module_version, 
                             const char *module_root_node);

int md_login_logout_event_create(gcl_session *sess,
                                 md_session_state *sess_state, 
                                 const char *event_name, bn_request **ret_req);

#ifndef MD_EXT_MSG_LOG_LEVEL
#define MD_EXT_MSG_LOG_LEVEL LOG_INFO
#endif

extern const char mgmtd_process_name[];


#ifdef __cplusplus
}
#endif

#endif /* __MD_MAIN_H_ */
