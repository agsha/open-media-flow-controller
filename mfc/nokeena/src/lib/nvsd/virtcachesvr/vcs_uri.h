/*============================================================================
 *
 *
 * Purpose: This file defines the vcs_uri functions.
 *
 * Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
 *
 *
 *============================================================================*/
#ifndef __VCS_URI_H__
#define __VCS_URI_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "vcs_dbg.h"
#include "nkn_defs.h"

static inline char *vcs_uri(uint32_t *uri_len) 
{
    return (char *)(uri_len+1);
}

static inline char *vcs_uriput(uint32_t *uri_len, void *p)
{
    char *from = p;
    char *to = vcs_uri(uri_len);
    uint32_t len = *uri_len;

    if (len > 0) {
	memcpy(to, from, len);
    }
    to[len] = 0;
    return to;
}

#endif /* __VCS_URI_H__ */
