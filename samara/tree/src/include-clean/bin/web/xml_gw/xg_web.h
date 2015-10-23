/*
 *
 * src/bin/web/xml_gw/xg_web.h
 *
 *
 *
 */

#ifndef __XG_WEB_H_
#define __XG_WEB_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


int xg_web_query_string_to_request_tree(xg_request_state *xrs, 
                                    tstring **ret_request_tree);

int xg_web_get_request(xg_request_state *xrs);

int xg_web_do_output(xg_request_state *xrs);

int xg_web_init(xg_request_state *xrs);

int xg_web_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __XG_WEB_H_ */
