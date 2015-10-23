/*
 *
 * src/bin/mgmtd/md_audit.h
 *
 *
 *
 */

#ifndef __MD_AUDIT_H_
#define __MD_AUDIT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "tstring.h"
#include "mdb_db.h"
#include "mdb_changes.h"
#include "bnode.h"

/**
 * \file src/bin/mgmtd/md_audit.h API for Action Audit Logging Functions.
 * This module contains public helper functions that may be used in conjunction
 * with action audit logging.
 * \ingroup mgmtd
 */


/**
 * Line type used by an mdm_action_audit_func callback handler when calling 
 * md_audit_action_add_line() or md_audit_action_add_line_fmt() to append a
 * line to the Action Audit Log.
 */
typedef enum {
    maalt_none = 0,

    /**
     * Specifies that this line contains an overall description of the action,
     * such as the one provided to the callback in the maad_action_descr field
     * of the audit_directives structure passed to the callback function,
     * which was provided during node registration.
     */
    maalt_description,

    /**
     * Specifies that this line contains an action binding parameter
     * description, such as the one provided in the mra_param_audit_descr of
     * field in the action_parameters list passed to the callback function,
     * which was provided during node registration.
     */
    maalt_param,
} md_audit_action_line_type;


/**
 * Line flags used by an mdm_action_audit_func callback handler when calling 
 * md_audit_action_add_line() or md_audit_action_add_line_fmt() to append a
 * line to the Action Audit Log.  (Placeholder for future features).
 */
typedef enum {
    /* No special line handling flags */
    maalf_none = 0,
} md_audit_action_line_flags;

/** Bit field of ::md_audit_action_line_flags ORed together */
typedef uint32 md_audit_action_line_flags_bf;


typedef struct mdm_action_audit_context mdm_action_audit_context;


/**
 * Function used to a generate prinf-sytle formatted audit log message line
 * from within a Custom Action Node Audit Callback Function
 * mdm_action_audit_func.
 *
 * For a pre-formatted line, use md_audit_action_add_line(), which has the same
 * parameters except for the line parameter vs. line_fmt and '...' parameters.
 *
 * A Standard Action Audit Message Prefix will be added to each line as
 * described under the mdm_action_audit_func type.
 *
 * \param ctxt The opaque variable supplied by the infrastructure in the call
 * to your mdm_action_audit_func.
 *
 * \param line_type The type of audit information contained in this log line.
 *
 * \param line_flags (Future) Special directives used in line output handling.
 *
 * \param line_fmt The line to be appended to the audit log, containing
 * printf-style format string characters.
 *
 * \param ... The variable arguments list of format string variable values.
 */
int md_audit_action_add_line_fmt(mdm_action_audit_context *ctxt,
                                 md_audit_action_line_type line_type,
                                 md_audit_action_line_flags_bf line_flags,
                                 const char *line_fmt, ...)
                                 __attribute__ ((format (printf, 4, 5)));

/**
 * Function used to generate a pre-formatted audit log message line from
 * within a Custom Action Node Audit Callback Function mdm_action_audit_func.
 *
 * Parameters are identical to md_audit_action_add_line_fmt(), except for the
 * line parameter, which is a pre-formatted string literal.
 */
int md_audit_action_add_line(mdm_action_audit_context *ctxt,
                             md_audit_action_line_type line_type,
                             md_audit_action_line_flags_bf line_flags,
                             const char *line);


/**
 * These flags specify options that may affect the display of attribute values
 * in audit log messages.  Many of the flags below within a related group are
 * mutually exclusive, so setting more than one will generate an error message.
 * Unless otherwise noted below, flags with the same prefix should be
 * considered mutually exclusive, e.g. you must choose only one of
 * the mavdf_bool_* settings.
 */
typedef enum {
    /**
     * Select all default directives for display:
     * - mavdf_bool_yes_no
     * - mavdf_binary_bytes_hex_and_ascii
     */
    mavdf_none = 0,

    /*
     * For boolean attributes, display _("yes") or _("no")
     */
    mavdf_bool_yes_no = 1 << 0,  /* default */

    /* For boolean attributes, display _("true") or _("false") */
    mavdf_bool_true_false = 1 << 1,

    /**
     * For boolean attributes, display _("enabled") or _("disabled")
     */
    mavdf_bool_enabled_disabled = 1 << 2,

    /**
     * For a binary value display the entire string in hex with ASCII char
     * displayable values included wherever possible (see lc_format_binary).
     */
    mavdf_binary_bytes_hex_and_ascii = 1 << 3,  /* default */

    /**
     * For a binary value display only a comment that it is binary rather than
     * the binary string.
     */
    mavdf_binary_comment_only = 1 << 4,

} md_audit_value_display_flags; 

typedef uint32 md_audit_value_display_flags_bf;


/**
 * For general audit logging purposes, render a bn_attrib in a format suitable
 * for display in the logs.  This function may be used within an
 * mdm_action_audit_func to prepare values in the same fashion as is done for
 * Automatic Action Auditing, however this function may be suitable for other
 * kinds of attribute auditing as well (e.g. events and configuration).
 *
 * In particular:
 * - Binary values are displayed in the form returned by lc_format_binary()
 * - Time values are rendered in a pretty display format
 * - String types are quoted with ""
 * - Character types are quoted with ''
 * - The resulting value is optionally with a suffix if supplied
 * - The value is truncated with indication of truncation if requested
 *
 * Since this function is not aware of your module's registration node, be sure
 * invoke internationalization macros on your special display strings as
 * needed prior to passing on to this function.  For example, you might pass
 * something like like I_(my_units_suffix) to value_suffix.
 *
 * \param commit Opaque context structure for this commit action.
 *
 * \param attrib The attribute to be displayed as a loggable string
 *
 * \param display_flags Flags that control the manner in which the value will
 * be rendered, such as how to display boolean and binary values.  See
 * md_audit_value_display_flags for details.
 *
 * \param value_suffix An optional suffix to append to the value, such as a
 * unit of measure.
 *
 * \param max_len The maximum displayable length to render before truncating
 * the value with ellipses, e.g. ..." the end of a long string... (truncated)".
 * If 0, then the string will not be truncated.  Else max_len must be at least
 * 32 characters to allow ample room for the truncation text, which might
 * be subject to change and is subject to internationalization, but will never
 * be more 32 characters, so 32 or greater must be used.
 *
 * \retval ret_value The value returned in log displayable string format.
 *
 */

int md_audit_log_render_value_as_str(md_commit *commit,
                                     const  bn_attrib *attrib,
                                     md_audit_value_display_flags_bf
                                        display_flags,
                                     const char *value_suffix,
                                     uint32 max_len,
                                     tstring **ret_value);

/**
 * This is an audit function (matching the function signature defined
 * by ::mdm_node_audit_func) which can be assigned to mrn_audit_func
 * in your node registration.  It is to be used on a uint32 node which
 * contains a gid which represents a capability level.
 *
 * NOTE: this callback is only appropriate if PROD_FEATURE_CAPABS is
 * enabled.  It should not be used under PROD_FEATURE_ACLs.
 */
int md_audit_gid_capab(md_commit *commit, const mdb_db *old_db,
                       const mdb_db *new_db, 
                       const mdb_db_change_array *changes, 
                       const mdb_db_change *change, int32 max_messages_left, 
                       void *audit_cb_arg, void *arg, 
                       tstr_array *ret_messages);

#ifdef __cplusplus
}
#endif

#endif /* __MD_AUDIT_H_ */
