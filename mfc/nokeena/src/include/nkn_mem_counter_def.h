/*****************************************************************************
*
*	File : nkn_mem_counter_def.h
*
*	Description : Header file that defines the coutners for the memory allocator
*
*	Created By : Ramanand Narayanan (ramanand@nokeena.com)
*
*	Created On : 11th June, 2009
*
*	Modified On : 
*
*	Copyright (c) Nokeena Inc., 2008-09
*
*****************************************************************************/
#ifndef _NKN_MEM_COUNTER_DEF_H
#define _NKN_MEM_COUNTER_DEF_H

#include "nkn_memalloc.h"

// Counters for various types of objects allocated - required by the
// nkn_memalloc library
#undef OBJ_TYPE
#define OBJ_TYPE(_type) u_int64_t glob_malloc_ ## _type; u_int64_t glob_malloc_glib_ ## _type; u_int64_t glob_malloc_nlib_ ## _type; u_int32_t glob_malloc_objsz_ ## _type;
#include "nkn_mem_object_types.h"
#undef OBJ_TYPE

object_data_t obj_type_data[] = {
#undef OBJ_TYPE
#define OBJ_TYPE(_type) { _type, &glob_malloc_objsz_ ## _type, #_type, &glob_malloc_ ## _type, &glob_malloc_glib_ ## _type, &glob_malloc_nlib_ ## _type },
#include "nkn_mem_object_types.h"
#undef OBJ_TYPE
};

nkn_obj_type_t nkn_malloc_max_mem_types = max_mem_types;

/*
 *******************************************************************************
 * Allocate glob_xxx call counters
 *******************************************************************************
 */
AO_t glob_malloc_calls;
AO_t glob_free_calls;
AO_t glob_realloc_calls;
AO_t glob_calloc_calls;
AO_t glob_cfree_calls;
AO_t glob_memalign_calls;
AO_t glob_valloc_calls;
AO_t glob_pvalloc_calls;
AO_t glob_posix_memalign_calls;
AO_t glob_strdup_calls;

AO_t glob_nkn_malloc_calls;
AO_t glob_nkn_realloc_calls;
AO_t glob_nkn_calloc_calls;
AO_t glob_nkn_memalign_calls;
AO_t glob_nkn_valloc_calls;
AO_t glob_nkn_pvalloc_calls;
AO_t glob_nkn_posix_memalign_calls;
AO_t glob_nkn_strdup_calls;

AO_t glob_nkn_memalloc_bad_free;
AO_t glob_nkn_memalloc_bad_realloc;

#endif /* _NKN_MEM_COUNTER_DEF_H */

/*
 * End of nkn_mem_counter_def.h
 */
