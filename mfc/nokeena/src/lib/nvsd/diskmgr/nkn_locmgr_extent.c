#include <stdlib.h>
#include <stdint.h>
#include <glib.h>
#include <assert.h>
#include "nkn_locmgr_extent.h"
#include "nkn_locmgr_uri.h"
#include "nkn_locmgr_physid.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static GMutex *nkn_lm_extent_q_mutex = NULL;
static GQueue *nkn_lm_extent_free_q = NULL;
struct LM_extent *nkn_glob_lm_extent_array[NKN_SYSTEM_MAX_EXTENTS];

static int  s_lm_extent_prealloc_mem (void);
static nkn_lm_extent_id_t s_lm_extent_get_free_id(void);
static void s_lm_extent_insert(nkn_lm_extent_id_t in_extent_id,
              struct LM_extent *in_extent);
#if 0
static void s_lm_extent_cleanup(nkn_lm_extent_id_t in_extent_id);
#endif

int 	glob_lm_extent_create_count = 0;
int 	glob_lm_extent_delete_count = 0;
int 	glob_lm_extent_delete_func_call_count = 0;

void
nkn_lm_extent_dump_counters(void)
{
	printf("\nglob_lm_extent_create_count = %d",
	       glob_lm_extent_create_count);
	printf("\nglob_lm_extent_delete_count = %d",
	       glob_lm_extent_delete_count);
	printf("\nglob_lm_extent_delete_func_call_count = %d",
		glob_lm_extent_delete_func_call_count);
}

static void
s_lm_extent_insert(nkn_lm_extent_id_t in_extent_id,
			       struct LM_extent *in_extent)
{
	/* The caller function obtains the lock */
        assert(in_extent);
        nkn_glob_lm_extent_array[in_extent_id] = in_extent;
}

static int
s_lm_extent_prealloc_mem(void)
{
	LM_extent_t *ext;
	int i;

	for (i=0; i < NKN_SYSTEM_MAX_EXTENTS; i++) {
		ext = (LM_extent_t *) nkn_calloc_type(1, sizeof(LM_extent_t),
						mod_dm_LM_extent_t);
		if (ext == NULL) {
			printf("\n%s(): Error getting extent mem",
			       __FUNCTION__);
			return -1;
		}

		ext->extent_id = i;
		s_lm_extent_insert(ext->extent_id, ext);
		g_queue_push_head(nkn_lm_extent_free_q, ext);
	}
	return 0;
}

static nkn_lm_extent_id_t
s_lm_extent_get_free_id(void)
{
	LM_extent_t *ext;

	/* The caller function obtains the lock */
	ext = g_queue_pop_tail(nkn_lm_extent_free_q);
	if(ext != NULL) {
		return ext->extent_id;
	} else {
		return -1;
	}
}

#if 0
static void s_lm_extent_cleanup(nkn_lm_extent_id_t in_extent_id)
{
	int i;

        for(i=0; i < NKN_SYSTEM_MAX_EXTENTS; i++) {
                if(nkn_glob_lm_extent_array[in_extent_id] != NULL) {
                        free(nkn_glob_lm_extent_array[in_extent_id]);
                        nkn_glob_lm_extent_array[in_extent_id] = NULL;
                }
        }
}
#endif

int
nkn_lm_extent_init(void)
{
	int ret_val = -1;

        nkn_lm_extent_free_q = g_queue_new();
        assert(nkn_lm_extent_free_q);

        ret_val = s_lm_extent_prealloc_mem();

        assert(nkn_lm_extent_q_mutex == NULL);
        nkn_lm_extent_q_mutex = g_mutex_new();

        return ret_val;
}

struct LM_extent *
nkn_lm_extent_get(nkn_lm_extent_id_t in_extent_id)
{
	assert((in_extent_id > 0) && (in_extent_id < NKN_SYSTEM_MAX_EXTENTS));
	return (nkn_glob_lm_extent_array[in_extent_id]);
}

nkn_lm_extent_id_t
nkn_lm_extent_create_new(char     *physid,
			 uint32_t content_offset,
			 uint32_t content_len)
{
        struct LM_extent *ext = NULL;
        nkn_lm_extent_id_t eid;

        g_mutex_lock(nkn_lm_extent_q_mutex);

        eid = s_lm_extent_get_free_id();
        if((int)eid < 0) {
                assert(0);
                return -1;
        }

        ext = nkn_lm_extent_get(eid);
        if(ext == NULL) {
                assert(0);
                return -1;
        }

	memcpy(ext->physid, physid, strlen(physid));
	ext->uol.offset = content_offset;
	ext->uol.length = content_len;
	ext->inUse = TRUE;

	glob_lm_extent_create_count ++;
	g_mutex_unlock(nkn_lm_extent_q_mutex);

	return 0;
}

void
nkn_lm_extent_free(nkn_lm_extent_id_t extent_id)
{
        struct LM_extent *ext = NULL;

	glob_lm_extent_delete_func_call_count ++;

        g_mutex_lock(nkn_lm_extent_q_mutex);
        ext = nkn_glob_lm_extent_array[extent_id];
        if(ext != NULL) {
                ext->inUse = FALSE;
		glob_lm_extent_delete_count ++;
                g_queue_push_head(nkn_lm_extent_free_q, ext);
        }
        g_mutex_unlock(nkn_lm_extent_q_mutex);
}

/*
	Both get functions need to be protected if the lists are
	going to be worked on. Some other thread could add to or delete
	this list.
*/
//void nkn_lm_get_extents_by_uri(char *in_uriname, GList **out_extent_list)
GList *
nkn_lm_get_extents_by_uri(char *in_uriname)
{
	GList *out_extent_list;

	assert(in_uriname);
	out_extent_list = (GList *)nkn_locmgr_uri_tbl_get(in_uriname);
	return out_extent_list;
}

void
nkn_lm_get_extents_by_physid(char *in_physid,
			     GList **out_extent_list)
{
	assert(in_physid);
        *out_extent_list = (GList *)nkn_locmgr_physid_tbl_get(in_physid);
}

void
nkn_lm_set_extent_offset(char *in_uri,
			 struct LM_extent *in_extent,
			 uint64_t in_uri_offset)
{
#if 0
	GList *extent_list = NULL;
	GList *ptr_list = NULL;
#endif

	in_extent->uol.offset = in_uri_offset;
#if 0
	extent_list = (GList *) nkn_locmgr_uri_tbl_get(in_uri);
	if(extent_list == NULL) {
		new_list = 1;
		in_extent->uol.offset = 0;
	}
	else {
		ptr_list = g_list_last(extent_list);
		assert(ptr_list);
		ptr_ext = (struct LM_extent *) (ptr_list->data);
		assert(ptr_ext);
		in_extent->uol.offset =
		    ptr_ext->uol.offset + in_extent->uol.length;
	}
#endif
}



/*
	- This function inserts an extent into a list given a uri as key.
	- The string has to be a uri or a physid string.
	- This function has to be called under locks.
*/
int
nkn_lm_extent_insert_by_uri(char *in_string,
			    struct LM_extent *in_extent)
{
	GList *extent_list = NULL;
#if 0
	GList *ptr_list = NULL;
#endif
	int new_list = 0;
	int ret_val = -1;
#if 0
	struct LM_extent *ptr_ext = NULL;
#endif

	extent_list = (GList *) nkn_locmgr_uri_tbl_get(in_string);
	if(extent_list == NULL) {
		new_list = 1;
	}

	extent_list = g_list_append(extent_list, in_extent);
	if (new_list == 1) {
		ret_val = nkn_locmgr_uri_tbl_insert(in_string, extent_list);
		if (ret_val < 0) {
			return -1;
		}
	}

	assert(extent_list);
	return 0;
}

/*
        - This function inserts an extent into a list given a string as key.
        - The string has to be a physid string.
        - This function has to be called under locks.
*/
int
nkn_lm_extent_insert_by_physid(char		*in_string,
			       struct LM_extent *in_extent)
{
        GList *extent_list = NULL;
        int new_list = 0;
        int ret_val = -1;

        extent_list = (GList *) nkn_locmgr_physid_tbl_get(in_string);
        if(extent_list == NULL) {
                new_list = 1;
        }

        extent_list = g_list_append(extent_list, in_extent);
        if (new_list == 1) {
                ret_val = nkn_locmgr_physid_tbl_insert(in_string, extent_list);
                if (ret_val < 0) {
                        return -1;
                }
        }
        assert(extent_list);
        return 0;
}


/*
        - This function deletes all extents given a uri string.
	- This function needs to be called with a lock so that delete
	  will not trod on some other get.
*/
int
nkn_lm_extent_delete_all_by_uri(char *in_string)
{
        GList *extent_list = NULL;
        int list_found = 0;
        int ret_val = 0;

        extent_list = (GList *) nkn_locmgr_uri_tbl_get(in_string);
        if(extent_list != NULL) {
		list_found = 1;
        }

        if (list_found == 1) {
                nkn_locmgr_uri_tbl_delete(in_string);
        }
	return ret_val;
}

/*
        - This function deletes all extents given a physid string.
        - This function needs to be called with a lock so that delete
          will not trod on some other get.
*/
int
nkn_lm_extent_delete_all_by_physid(char *in_string)
{
        GList *extent_list = NULL;
        int list_found = 0;
        int ret_val = 0;

        extent_list = (GList *) nkn_locmgr_physid_tbl_get(in_string);
        if(extent_list != NULL) {
                list_found = 1;
        }

        if (list_found == 1) {
                nkn_locmgr_physid_tbl_delete(in_string);
        }
        return ret_val;
}
