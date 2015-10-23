/*
 *
 * src/bin/statsd/st_report.h
 *
 *
 *
 */

#ifndef __ST_REPORT_H_
#define __ST_REPORT_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "bnode.h"
#include "st_registration.h"

/* ------------------------------------------------------------------------- */
int st_reports_init(void);


/* ------------------------------------------------------------------------- */
int st_report_lookup_by_id(const char *id, st_report_reg **ret_reg);


/* ------------------------------------------------------------------------- */
/* Generate a report according to the instructions found in the 
 * specified binding array.
 */
int st_report_generate_from_bindings(const bn_binding_array *bindings,
                                     uint32 *ret_code, tstring **ret_msg,
                                     bn_binding **ret_resp_binding);

#ifdef __cplusplus
}
#endif

#endif /* __ST_REPORT_H_ */
