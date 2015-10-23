/*
 *
 * md_email_utils.h
 *
 *
 *
 */

#ifndef __MD_EMAIL_UTILS_H_
#define __MD_EMAIL_UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/**
 * \file src/include/md_email_utils.h Utilities for sending emails
 * from mgmtd modules.
 * \ingroup mgmtd
 */

#include "common.h"
#include "mdb_db.h"
#include "proc_utils.h"

#define md_email_node_root    "/email"
#define md_email_node_mailhub "/email/client/mailhub"
#define md_email_node_mailhub_port "/email/client/mailhub_port"
#define md_email_node_domain  "/email/client/rewrite_domain"
#define md_email_node_enable  "/email/client/enable"

#define md_email_node_auth_enable   "/email/client/auth/enable"
#define md_email_node_auth_username "/email/client/auth/username"
#define md_email_node_auth_password "/email/client/auth/password"
#define md_email_node_auth_method   "/email/client/auth/method"
#define md_email_node_auth_old      "/email/client/auth/old_auth"
#define md_email_node_ssl_mode      "/email/client/ssl/mode"
#define md_email_node_ssl_cert_vrfy "/email/client/ssl/cert_verify"
#define md_email_node_ssl_calist    "/email/client/ssl/ca_certs/list_name"

#define md_email_node_auto_auth_enable   "/email/autosupport/auth/enable"
#define md_email_node_auto_auth_username "/email/autosupport/auth/username"
#define md_email_node_auto_auth_password "/email/autosupport/auth/password"
#define md_email_node_auto_auth_method   "/email/autosupport/auth/method"
#define md_email_node_auto_auth_old      "/email/autosupport/auth/old_auth"
#define md_email_node_auto_ssl_mode      "/email/autosupport/ssl/mode"
#define md_email_node_auto_ssl_cert_vrfy "/email/autosupport/ssl/cert_verify"
#define md_email_node_auto_ssl_calist    "/email/autosupport/ssl/ca_certs/" \
                                         "list_name"

#define md_email_monitor_domain  "/email/state/rewrite_domain"

#include "event_notifs.h"

typedef enum {
    medo_none =          0,
    medo_detail_only,
    medo_summary_only,
    medo_all    
} md_email_detail_options;

/**
 * Option flags which an event handler can set, to tweak how the email
 * is composed and sent.  These all default to off.
 */
typedef enum {
    mesf_none = 0,

    /**
     * Do not prepend the standard subject prefix ("System event on
     * &lt;hostname&gt;", or whatever we get from md_email_*_subject_prefix)
     * to the subject.
     */
    mesf_suppress_common_subject = 1 << 0,

    /**
     * Do not prepend the standard body header (various system
     * information) to the body of the email.
     */
    mesf_suppress_common_body_header = 1 << 1,

    /**
     * Do not append the standard body trailer (currently, just
     * a line saying "==== Done.") to the body of the email.
     */
    mesf_suppress_common_body_trailer = 1 << 2,

} md_email_send_flags;

typedef uint32 md_email_send_flags_bf;

typedef struct md_email_attachment {
    /** Mandatory: full path to file to attach    */
    char *mea_path;

    /** If NULL, application/octet-stream is used */
    const char *mea_mime_type;

    /** Include attachment in detailed emails?    */
    tbool mea_detail;

    /** Include attachment in summary emails?     */
    tbool mea_summary;

    /** Delete this file after attaching it?      */
    tbool mea_delete;

    /*
     * XXX/EMT: currently only one MIME type per email is supported
     * by makemail.sh, so we will only honor the first one specified.
     */
} md_email_attachment;

/**
 * Create a new structure to hold information about an email
 * attachment.  Note that the default is to include the attachment
 * in every kind of email, and to delete it when we are done sending
 * the email.  You must fill in mea_path; changing the rest of the 
 * fields is optional.
 */
int md_email_attachment_new(md_email_attachment **ret_mea);
void md_email_attachment_free_for_array(void *elem);

TYPED_ARRAY_HEADER_NEW_NONE(md_email_attachment, struct md_email_attachment *);
int md_email_attachment_array_new(md_email_attachment_array **ret_arr);

typedef struct md_email_attachment_wrapper {
    int meaw_refcount;
    md_email_attachment_array *meaw_attachments;
} md_email_attachment_wrapper;

/**
 * Create a new md_email_attachment_wrapper object. If create_new_array
 * parameter is set to true, a new empty md_email_attachment_array will
 * be created and meaw_attachments member variable will point to it. Otherwise
 * meaw_attachments will be NULL.
 *
 * The meaw_refcount will be set to 1.
 */
int md_email_attachment_wrapper_new(md_email_attachment_wrapper **ret_meaw,
        tbool create_new_array);

/**
 * Create a new md_email_attachment_wrapper object. meaw_attachments
 * member variable will be set to the pointer specified by attachments.
 *
 * The meaw_refcount will be set to 1.
 */
int md_email_attachment_wrapper_new_takeover(md_email_attachment_wrapper
        **ret_meaw, md_email_attachment_array **attachments);

/**
 * Increment the reference count of md_email_attachment_wrapper object.
 */
int md_email_attachment_wrapper_ref_inc(md_email_attachment_wrapper *meaw);

/**
 * Decrement the reference count of md_email_attachment_wrapper object.
 * If reference count becomes zero, the object is freed from memory.
 * *inout_meaw pointer will ALWAYS be set to NULL.
 */
int md_email_attachment_wrapper_ref_dec(md_email_attachment_wrapper
        **inout_meaw);

typedef struct md_email_params {
    md_commit *               mep_commit;
    const mdb_db *            mep_db;

    /**
     * Is this email going to autosupport?  If so, the next three
     * fields (mep_ev_classes, mep_detail_options, and mep_is_failure)
     * are ignored.
     */
    tbool                     mep_autosupport;

    /**
     * This is a bit field of ::en_event_class.  This specifies which
     * kind(s) of event this is, so we can figure out which recipients
     * to send it to.  Generally, only one bit should be set, as an
     * event is either a failure or informational.  But both could be 
     * set, for example, if we were sending a test email and wanted to
     * hit all of the recipients.
     */
    uint32                    mep_ev_classes;

    /**
     * Specifies whether we should send to recipients who want detail,
     * summaries, or both.
     */
    md_email_detail_options   mep_detail_options;

    /**
     * This field is ignored.  (XXX/EMT: should it be taken out?)
     */
    tbool                     mep_is_failure;

    const char *              mep_subject;
    const char *              mep_body;
    const bn_binding_array *  mep_bindings;

    md_email_send_flags_bf    mep_flags;

    md_email_attachment_array *mep_attachments;
} md_email_params;

typedef struct md_email_finalize_state {
    lc_launch_result *mefs_launch_result;
    md_email_attachment_wrapper *mefs_attachments;
} md_email_finalize_state;

/* ------------------------------------------------------------------------- */
/**
 */
int md_email_params_defaults(md_email_params **ret_mep, md_commit *commit,
                             const mdb_db *db);


/* ------------------------------------------------------------------------- */
/**
 */
int md_email_params_free(md_email_params **inout_mep);


/* ------------------------------------------------------------------------- */
/**
 */
int md_email_check_config(md_commit *commit, const mdb_db *db, 
                          const en_event_notif *event, tbool *ret_ok);

/* ------------------------------------------------------------------------- */
/** Get a list of recipient names that want to receive the specified
 * class(es) of events.
 *
 * The autosupport field tells whether or not we are interested in the
 * autosupport recipients.  If it is set, the eecs field is ignored.
 * If it is not set, the eecs field is a bit field containing one or
 * more flags from the en_event_class enum.
 *
 * The flags field is a bit field containing one or more flags from
 * the md_email_detail_options enum.
 */
int
md_email_get_recip_names(md_commit *commit, const mdb_db *db,
                         tbool autosupport, uint32 eecs, 
                         md_email_detail_options detail_opts,
                         tstr_array **ret_recips);

/* ------------------------------------------------------------------------- */
/**
 */
int md_email_send_email(const md_email_params *mep,
                        tstring **ret_errors);


/* ------------------------------------------------------------------------- */
/**
 */
int md_email_send_email_non_blocking(const md_email_params *mep,
                                     lc_launch_result **ret_launch_result,
                                     md_email_finalize_state **ret_mefs_state);


/* ------------------------------------------------------------------------- */
/**
 */
int md_email_send_email_non_blocking_complete(lc_launch_result **
                                              inout_launch_result,
                                              md_email_finalize_state **
                                              inout_mefs_state,
                                              int *ret_return_code,
                                              tstring **ret_output);


/* ------------------------------------------------------------------------- */
/** Generate the body of a notification email for an event generated by
 * a stats alarm.  The value that triggered the alarm is extracted from
 * the bindings array and embedded into the provided format string using
 * a call to a printf() variant.
 *
 * \param bindings The array of bindings present in the notification
 * event, presumably with one named "data_value" which will be extracted
 * and used in the message generated.
 *
 * \param fmt_string A printf-style format string containing exactly one
 * '%s' argument, where we should place the value extracted.  Since this
 * whole string will be used as a format string, any other '%' characters
 * should be escaped as "%%".
 *
 * \retval ret_body The generated email body.
 * 
 */
int
md_email_get_basic_body(const bn_binding_array *bindings, 
                        const char *fmt_string, char **ret_body);


int md_email_is_for_autosupport(md_commit *commit, const mdb_db *db,
                                const char *event_name, tbool *ret_autosupp);

#ifdef __cplusplus
}
#endif

#endif /* __MD_EMAIL_UTILS_H_ */
