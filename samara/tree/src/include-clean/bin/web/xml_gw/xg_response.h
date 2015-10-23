/*
 *
 * src/bin/web/xml_gw/xg_response.h
 *
 *
 *
 */

#ifndef __XG_RESPONSE_H_
#define __XG_RESPONSE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

extern const tbool xg_opt_default_use_stylesheet;
extern const tbool xg_opt_default_include_namespace;
extern const xg_value_format xg_opt_default_binary_format;

int xg_response_make_response(xg_request_state *xrs);
int xg_response_make_error_response_simple(xg_request_state *xrs, 
                                           int32 code,
                                           const char *msg);
int xg_response_make_error_response(xg_request_state *xrs);


#ifdef __cplusplus
}
#endif

#endif /* __XG_RESPONSE_H_ */
