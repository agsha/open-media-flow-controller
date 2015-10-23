//=============================================================================
//
//
// Purpose: This file defines the various data structures for the VirtCache 
//          server. 
//
// Copyright (c) 2002-2012 Juniper Networks. All rights reserved.
//
//
//=============================================================================

#ifndef __NKN_VIRT_CACHE_SERVER__
#define __NKN_VIRT_CACHE_SERVER__

#pragma once

//-----------------------------------------------------------------------------
// Forward Declarations & Include Files
//-----------------------------------------------------------------------------
#include <stdint.h>
#include <string.h>


// FIXME: remove argument and return value.
int virt_cache_server_init(void);
void virt_cache_server_exit(void);

// test only
void virt_cache_server_wait(void);

#endif // __NKN_VIRT_CACHE_SERVER__
