/*
 * dpi_urlfilter.h -- DPI interface to URL Filter
 */
#ifndef _DPI_URLFILTER_H_
#define _DPI_URLFILTER_H_

#include "nkn_namespace.h"
#include "nkn_http.h"

namespace_uf_reject_t 
DPI_urlfilter_lookup(uint64_t http_method /* HRF_METHOD_XXX defs nkn_http.h */,
		     const char *host, int hostlen,
		     const char *uri_abs_path, int uri_abs_pathlen);

#endif /* _DPI_URLFILTER_H_ */
/*
 * End of dpi_urlfilter.h
 */
