/*
 *
 * src/bin/web/xml_gw/xg_request.h
 *
 *
 *
 */

#ifndef __XG_REQUEST_H_
#define __XG_REQUEST_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif


#include "common.h"
#include "xg_main.h"
#include <libxml/parser.h>
#include <libxml/tree.h>

int xg_request_error(xg_request_state *xrs, uint32 code, 
                     const char *format, ...)
    __attribute__ ((format (printf, 3, 4)));

int xg_request_handler(xg_request_state *xrs);

#ifdef __cplusplus
}
#endif

#endif /* __XG_REQUEST_H_ */
