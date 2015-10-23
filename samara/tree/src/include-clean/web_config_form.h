/*
 *
 * web_config_form.h
 *
 *
 *
 */

#ifndef __WEB_CONFIG_FORM_H__
#define __WEB_CONFIG_FORM_H__


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/** \file web_config_form.h Web config form library functions
 * \ingroup web
 */

#include "common.h"
#include "tstring.h"
#include "bnode.h"
#include "bnode_proto.h"
#include "web.h"

extern const char web_dname_prefix[];
extern const char web_name_prefix[];
extern const char web_rname_prefix[];
extern const char web_type_prefix[];
extern const char web_ctype_prefix[];
extern const char web_typeflags_prefix[];
extern const char web_field_prefix[];
extern const char web_value_prefix[];
extern const char web_required_prefix[];
extern const char web_multiplier_prefix[];
extern const char web_obfuscated_prefix[];
extern const char web_disabled_prefix[];

extern const char web_btn_apply[];
extern const char web_btn_save[];
extern const char web_btn_ok[];
extern const char web_btn_cancel[];
extern const char web_btn_reset[];

extern const char web_tbool_true_str[];
extern const char web_bool_false_str[];

extern const char web_field_btn_dest[];

/*
 * For custom form handlers, which may want to fall back to generic
 * CFG-FORM handler on some inputs.
 */
extern const char wcf_config_form_action_name[];

/**
 * For the given type in string form, return the type.
 * \param web_data per-request web context data returned from web_init().
 * \param str_type the type in string form.
 * \param ret_type the type.
 * \return 0 on success, error code on failure.
 */
int web_get_type_for_str(web_request_data *web_data, const char *str_type,
                         bn_type *ret_type);

/**
 * For the given form field id, return the type.
 * \param web_data per-request web context data returned from web_init().
 * \param id the id.
 * \param ret_type the type.
 * \return 0 on success, error code on failure.
 */
int web_get_type_for_form_field(web_request_data *web_data, const tstring *id,
                                bn_type *ret_type);
int web_get_type_for_form_field_str(web_request_data *web_data, const char *id,
                                    bn_type *ret_type);

/**
 * Get the display name of the given form field.
 * \param web_data per-request web context data returned from web_init().
 * \param id the id.
 * \param ret_name the display name.
 * \return 0 on success, error code on failure.
 */
int web_get_field_display_name(web_request_data *web_data,
                               const tstring *id, const tstring **ret_name);
int web_get_field_display_name_str(web_request_data *web_data,
                                   const char *id, const tstring **ret_name);

/**
 * Check if the given form field is a required value.
 * \param web_data per-request web context data returned from web_init().
 * \param id the id.
 * \param ret_required true or false.
 * \return 0 on success, error code on failure.
 */
int web_is_required_form_field(web_request_data *web_data, const tstring *id,
                               tbool *ret_required);
int web_is_required_form_field_str(web_request_data *web_data, const char *id,
                                   tbool *ret_required);

/**
 * Helper for processing form fields that use the tms-cfg-form template.
 * \param web_data per-request web context data returned from web_init().
 * \param id the id to process.
 * \param req the request to tack bindings onto.
 * \param ret_code NULL or a pointer if you want a result code.
 * \param ret_msg NULL or a pointer if you want an error message.
 * \return 0 on success, error code on failure.
 */
int web_process_form_field(web_request_data *web_data,
                           const tstring *id, bn_request *req,
                           uint32 *ret_code, tstring **ret_msg);
int web_process_form_field_str(web_request_data *web_data, const char *id,
                               bn_request *req,
                               uint32 *ret_code, tstring **ret_msg);

/**
 * Helper for processing form fields that use the tms-cfg-form template.
 * This is different from web_process_form_field in that the GCL request
 * is handled internally.
 * \param web_data per-request web context data returned from web_init().
 * \param id the id to process.
 * \param ret_code NULL or a pointer if you want a result code.
 * \param ret_msg NULL or a pointer if you want an error message.
 * \return 0 on success, error code on failure.
 */
int web_handle_form_field(web_request_data *web_data, const tstring *id,
                          uint32 *ret_code, tstring **ret_msg);
int web_handle_form_field_str(web_request_data *web_data, const char *id,
                              uint32 *ret_code, tstring **ret_msg);

/**
 * Check the given string against the given node type and see if
 * a conversion can be done.
 * \param web_data per-request web context data returned from web_init().
 * \param id identifier used when generating an error message.
 * \param value the value to check.
 * \param type the type to check against.
 * \param type_flags type flags modifying the type to check against, if any.
 * \param ret_code NULL or a pointer if you want a result code.
 * \param ret_msg NULL or a pointer if you want an error message.
 * \return 0 on success, error code on failure.
 */
int web_type_check_value(web_request_data *web_data, const tstring *id,
                         const tstring *value, bn_type type, uint32 type_flags,
                         uint32 *ret_code, tstring **ret_msg);
int web_type_check_value_str(web_request_data *web_data,
                             const char *id, const char *value, bn_type type,
                             uint32 type_flags, uint32 *ret_code,
                             tstring **ret_msg);

/**
 * Check the data for the given form id to see if the data can
 * be converted to the type for that id.
 * \param web_data per-request web context data returned from web_init().
 * \param id the id to check.
 * \param ret_code NULL or a pointer if you want a result code.
 * \param ret_msg NULL or a pointer if you want an error message.
 * \return 0 on success, error code on failure.
 */
int web_type_check_form_field(web_request_data *web_data,
                              const tstring *id, uint32 *ret_code,
                              tstring **ret_msg);
int web_type_check_form_field_str(web_request_data *web_data, const char *id,
                                  uint32 *ret_code, tstring **ret_msg);

/**
 * Helper for processing form buttons that use the tms-cfg-form template.
 * \param web_data per-request web context data returned from web_init().
 * \return 0 on success, error code on failure.
 */
int web_process_form_buttons(web_request_data *web_data);

/**
 * Helper function for getting a trimmed form value.
 * \param web_data per-request web context data returned from web_init().
 * \param name the name of the parameter.
 * \param ret_value the value.
 * \return 0 on success, error code on failure.
 */
int web_get_trimmed_value(web_request_data *web_data, const tstring *name,
                          tstring **ret_value);
int web_get_trimmed_value_str(web_request_data *web_data, const char *name,
                              tstring **ret_value);

/**
 * Helper function for getting a trimmed form value.
 * \param web_data per-request web context data returned from web_init().
 * \param id the id of the field.
 * \param ret_value the value.
 * \return 0 on success, error code on failure.
 */
int web_get_trimmed_form_field(web_request_data *web_data, const tstring *id,
                               tstring **ret_value);
int web_get_trimmed_form_field_str(web_request_data *web_data, const char *id,
                                   tstring **ret_value);

/**
 * Helper function for getting a trimmed form value which is type checked
 * and also checked to see if it's empty (for strings).
 * \param web_data per-request web context data returned from web_init().
 * \param id the id of the field.
 * \param ret_value the value.
 * \param ret_code the return code.
 * \param ret_msg the return msg.
 * \return 0 on success, error code on failure.
 */
int web_get_checked_form_field(web_request_data *web_data,
                               const tstring *id, tstring **ret_value,
                               uint32 *ret_code, tstring **ret_msg);
int web_get_checked_form_field_str(web_request_data *web_data,
                                   const char *id, tstring **ret_value,
                                   uint32 *ret_code, tstring **ret_msg);

#ifdef __cplusplus
}
#endif

#endif /* __WEB_CONFIG_FORM_H_ */
