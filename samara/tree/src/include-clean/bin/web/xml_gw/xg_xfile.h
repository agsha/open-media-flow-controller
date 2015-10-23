/*
 *
 * src/bin/web/xml_gw/xg_xfile.h
 *
 *
 *
 */

#ifndef __XG_XFILE_H_
#define __XG_XFILE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

int xg_xfile_init(xg_request_state *xrs);

int xg_xfile_deinit(xg_request_state *xrs);

int xg_xfile_get_request(xg_request_state *xrs);

int xg_xfile_do_output(xg_request_state *xrs);


#ifdef __cplusplus
}
#endif

#endif /* __XG_XFILE_H_ */
