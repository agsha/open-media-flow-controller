/*
 * nkn_urlfilter.h -- URL Filter interface
 */
#ifndef _NKN_URLFILTER_H_
#define _NKN_URLFILTER_H_

#include "mime_header.h"
#include "nkn_namespace.h"

namespace_uf_reject_t url_filter_lookup(const url_filter_config_t *cf,
                                        const mime_header_t *hdr);

#endif /* _NKN_URLFILTER_H_ */

/*
 * End of nkn_urlfilter.h
 */
