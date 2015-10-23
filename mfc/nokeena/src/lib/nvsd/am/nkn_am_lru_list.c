#include "nkn_am_api.h"
#include "nkn_am.h"
#include <stdint.h>
#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "nkn_defs.h"
#include "nkn_debug.h"
#define AM_LIST_OPTIMIZED 1

/* We will maintain N LRU lists which will be ordered by access times.
   When a new object arrives it will be moved to the head of the list.

   1. When a new hotness object arrives, its hotness is compared with
   the the most recently used item on each list.

   1.a If the hotness is lesser than the hotness of this item on the list,
   this object is moved into that list. NOTE:
   the object is moved into the list that has the closest hotness.

   1.b. If the hotness of the object is greater than the compared object,
        go to the next list.

   1.c If an object does not exist in the next list, put this object in
       that list.

   2. Popping an object: When a new object arrives and has been put into
   one of the lists, all the LRU objects of each list are compared. Their
   hotness is decayed over time and then the coldest items in this set
   is popped.
*/

GList *AM_hotness_lru_list_get_prev_obj(GList *lru_listp);
GList *AM_hotness_lru_list_get_next_obj(GList *lru_listp);

static GMutex *nkn_am_lru_list_tbl_mutex;
static GHashTable *nkn_am_lru_list_hash_tbl;
static void s_check_lru_list_loop(GList *headp, GList *tailp);
static nkn_am_prov_list_t *s_return_lru_list_by_key(char *key,
							GList **headpp,
							GList **tailpp);
static void
s_lru_list_delete_link(GList **headpp, GList **tailpp,
		   GList *newp);
static void
s_set_lru_list_ptr_by_key(char *key, GList **headpp, GList **tailpp);

uint64_t glob_am_new_prov_lru_list_entry = 0;
uint64_t glob_am_new_prov_lru_list_obj = 0;
uint64_t glob_am_new_prov_lru_list_obj_del = 0;
uint64_t glob_am_new_prov_lru_list_obj_updates = 0;
uint64_t glob_am_lru_list_insert_error = 0;
uint64_t glob_am_mem_recovery = 0;
uint64_t glob_am_lru_glib_mem = 0;

#define MAX_LRU_HOTNESS_LISTS 5
/* Keep track of the coldest values in each LRU list*/
//static AM_obj_t *s_coldest_ptr[MAX_LRU_HOTNESS_LISTS];

static char *
s_build_lru_list_key(AM_pk_t *pk, int in_list_index)
{
    char *key = NULL;
    int  len = 128;

    if((in_list_index < 0) || (in_list_index > MAX_LRU_HOTNESS_LISTS-1)) {
	return NULL;
    }

    /* Build the key from the PK */
    if((pk->provider_id > 0) && (pk->sub_provider_id >= 0)) {
        key = (char *) nkn_malloc_type (len, mod_am_lru_list_malloc_key);
        if(!key) {
            DBG_LOG_MEM_UNAVAILABLE(MOD_AM);
            return NULL;
        }
        snprintf(key, len-1, "%d_%d_%d", pk->provider_id,
		 pk->sub_provider_id, in_list_index);
        key[len-1] = '\0';
    } else if(pk->provider_id > 0) {
        key = (char *) nkn_malloc_type (len, mod_am_lru_list_malloc_key);
        if(!key) {
            DBG_LOG_MEM_UNAVAILABLE(MOD_AM);
            return NULL;
        }
        snprintf(key, len-1, "%d_%d", pk->provider_id, in_list_index);
        key[len-1] = '\0';
    } else {
	return NULL;
    }

    assert(key);
    return key;
}

/* This function returns the coldest item in each list.
   NOTE: The tail of each list is the least recently used item. */
static AM_obj_t *
s_am_return_lru_item(int index, nkn_provider_type_t ptype, int8_t sptype)
{
    AM_obj_t *pobj = NULL;
    AM_pk_t pk;
    char *key = NULL;
    GList *headp = NULL;
    GList *tailp = NULL;
    GList **headpp = &headp;
    GList **tailpp = &tailp;
    int   ret = -1;

    pk.provider_id = ptype;
    pk.sub_provider_id = sptype;
    pk.type = AM_OBJ_URI;

    key = s_build_lru_list_key(&pk, index);
    if(!key) {
	return NULL;
    }

    s_return_lru_list_by_key(key, headpp, tailpp);
    free(key);

    if(!(*tailpp)) {
	return NULL;
    } else {
	assert(*headpp);
	pobj = (AM_obj_t *)(*tailpp)->data;
	assert(pobj);
	ret = am_check_object(pobj);
	if(ret < 0) {
	    AM_hotness_lru_list_delete(pobj);
	    s_set_lru_list_ptr_by_key(key, headpp, tailpp);
	    free(key);
	    if(*headpp) {
		assert(*tailpp);
	    } else {
		assert(!(*tailpp));
	    }
	    glob_am_mem_recovery ++;
	    return NULL;
	}
    }
    return pobj;
}

/* This function returns the coldest item in each list.
   NOTE: The head of each list is the most recently used item */
static AM_obj_t *
s_am_return_mru_item(int index, nkn_provider_type_t ptype, int8_t sptype)
{
    AM_obj_t *pobj = NULL;
    AM_pk_t pk;
    char *key = NULL;
    GList *headp = NULL;
    GList *tailp = NULL;
    GList **headpp = &headp;
    GList **tailpp = &tailp;


    pk.provider_id = ptype;
    pk.sub_provider_id = sptype;
    pk.type = AM_OBJ_URI;

    key = s_build_lru_list_key(&pk, index);
    if(!key) {
	return NULL;
    }

    s_return_lru_list_by_key(key, headpp, tailpp);
    free(key);

    if(!(*headpp)) {
	return NULL;
    } else {
	assert(*tailpp);
	pobj = (AM_obj_t *)(*headpp)->data;
	assert(pobj);
    }
    return pobj;
}

/* This function compares the coldest items in each
   LRU list and finds the coldest of them all. This way,
   we are evicting only the absolute coldest item with
   regards to LRU as well as hotness. */
static AM_obj_t *
s_am_pop_coldest_lru_item(nkn_provider_type_t ptype, int8_t sptype)
{
    AM_obj_t *coldp = NULL;
    AM_obj_t *pobj = NULL;
    int32_t hotness1 = 0;
    int32_t hotness2 = 0;
    int i;
    int list_index = -1;

    for(i = 0; i < MAX_LRU_HOTNESS_LISTS; i++) {
	pobj = s_am_return_lru_item(i, ptype, sptype);
	if(!pobj) {
	    continue;
	}
	/* Decay the hotness for these LRU objects */
	//AM_decay_hotness(&pobj->hotness);
	if(!coldp) {
	    coldp = pobj;
	    list_index = 0;
	} else {
 	    hotness1 = am_decode_hotness(&pobj->hotness);
	    hotness2 = am_decode_hotness(&coldp->hotness);
	    if(hotness1 < hotness2) {
		/* Found a colder item */
		coldp = pobj;
		list_index = i;
	    }
	}
    }
    return coldp;
}

/* 1. Search through the lists. Compare the most recently used object (MRU)
      with the new object OR look for an empty list.
   2. If no object exists in the next list, put this object there.
   3. If an object exists whose hotness is greater than the new object,
      insert the new object into that list.
   4. Go on until no more lists are found where 2 and 3 are valid. Then
      put the new object in the highest list.
*/
static int
s_am_lru_find_insert_list(AM_obj_t *pobj)
{
    int      i;
    AM_obj_t *lrup = NULL;
    int      found = 0;
    int32_t hotness1 = 0;
    int32_t hotness2 = 0;

    /* Don't insert any objects that are not URI types because LRU
     does not make sense for them */
    if(pobj->pk.type != AM_OBJ_URI) {
	return -1;
    }

    hotness2 = am_decode_hotness(&pobj->hotness);
    for(i = pobj->lru_list_index; i < MAX_LRU_HOTNESS_LISTS-1; i++) {
	lrup = s_am_return_mru_item(i, pobj->pk.provider_id,
				    pobj->pk.sub_provider_id);
	/* If there are no objects in the next list, insert the
	   object there. */
	if(!lrup) {
	    /* Empty list. Insert new object. */
	    pobj->lru_list_index = i;
	    found = 1;
	    break;
	}
	//AM_decay_hotness(&lrup->hotness);
	hotness1 = am_decode_hotness(&lrup->hotness);
	if(hotness1 >= hotness2) {
	    /* Insert the new item. The new item's hotness is less than
	       the object on the list. */
	    pobj->lru_list_index = i;
	    found = 1;
	    break;
	}
    }
    if(found == 0) {
	/* This means that this object is the hottest object in all the
	 lists. Put it in the highest list. */
	pobj->lru_list_index = MAX_LRU_HOTNESS_LISTS-1;
    }
    return 0;
}


static nkn_am_prov_list_t *
s_return_lru_list_by_key(char *key, GList **headpp, GList **tailpp)
{
    nkn_am_prov_list_t *lru_list = NULL;

    lru_list = nkn_am_list_tbl_get(nkn_am_lru_list_hash_tbl,
			       nkn_am_lru_list_tbl_mutex, key);
    if(lru_list == NULL) {
	return NULL;
    }

    *headpp = lru_list->am_prov_list_head;
    *tailpp = lru_list->am_prov_list_tail;

    if(*headpp) {
	assert(*tailpp);
    }

    return lru_list;
}

static void
s_set_lru_list_ptr_by_key(char *key, GList **headpp, GList **tailpp)
{
    nkn_am_prov_list_t *lru_list = NULL;

    lru_list = nkn_am_list_tbl_get(nkn_am_lru_list_hash_tbl,
			       nkn_am_lru_list_tbl_mutex, key);
    if(lru_list == NULL) {
	return;
    }

    if(*headpp) {
	assert(*tailpp);
    }

    lru_list->am_prov_list_head = *headpp;
    lru_list->am_prov_list_tail = *tailpp;
    return;
}

/* This function inserts the new object at the head of the list. */
static GList *
s_lru_list_insert_head(GList **headpp, GList **tailpp, gpointer data,
		       GList *newp)
{
    if(!newp) {
	newp = g_list_alloc();
        glob_am_lru_glib_mem += 24;
	if(newp == NULL) {
	    return NULL;
	}
	newp->data = data;
	newp->next = NULL;
	newp->prev = NULL;
    }

    if(*headpp) {
	newp->next = (*headpp);
	(*headpp)->prev = newp;
    } else {
	/* New list*/
	*tailpp = newp;
    }
    *headpp = newp;
    assert(*tailpp);
    return newp;
}

static void
s_lru_list_delete_link(GList **headpp, GList **tailpp,
		   GList *newp)
{
    GList *linkp = newp;

    assert(*headpp);
    assert(*tailpp);

    if(*headpp) {
	assert(*tailpp);
    } else {
	assert(!(*tailpp));
    }

    s_check_lru_list_loop(*headpp, *tailpp);
    if (linkp) {
	if (linkp->prev) {
	    linkp->prev->next = linkp->next;
	}

	if (linkp->next) {
	    /* Tail should not be modified here. */
	    linkp->next->prev = linkp->prev;
	    if(!linkp->prev) {
		/* First element is getting deleted */
		*headpp = linkp->next;
	    }
	} else {
	    /* Last element is getting deleted */
	    if(linkp->prev) {
		*tailpp = linkp->prev;
	    } else {
		/* This one is the only elem in the lru_list */
		*tailpp = NULL;
		*headpp = NULL;
	    }
	}

	if(*headpp) {
	    assert(*tailpp);
	} else {
	    assert(!(*tailpp));
	}

	linkp->next = NULL;
	linkp->prev = NULL;
    }

    if(*headpp) {
	assert(*tailpp);
    } else {
	assert(!(*tailpp));
    }

    s_check_lru_list_loop(*headpp, *tailpp);
    return ;
}

static void
s_check_lru_list_loop(GList *headp, GList *tailp)
{

    return;
    GList *phead = headp;
    GList *thead;

    if(!headp)
	return;

    phead = headp;
    thead = headp;
    do {
	phead = phead->next;
	if(!thead->next)
	    return;
	thead = thead->next->next;
	if(!thead)
	    return;
	if(phead == thead)
	    assert(0);
    } while(phead->next && thead->next);

    phead = tailp;
    thead = tailp;
    do {
	phead = phead->prev;
	if(!thead->prev)
	    return;
	thead = thead->prev->prev;
	if(!thead)
	    return;
	if(phead == thead)
	    assert(0);
    } while(phead->prev && thead->prev);
}

int
am_check_object(AM_obj_t *objp)
{
    if(!objp->pk.name) {
	return -1;
    }

    /* Store only entries of valid cache provider, i.e no
       origin providers and others. */
    if((objp->pk.provider_id >= NKN_MM_max_pci_providers
		&& objp->pk.provider_id != RTSPMgr_provider
		&& objp->pk.provider_id != OriginMgr_provider) ||
       (objp->pk.provider_id < SolidStateMgr_provider)) {
	return -1;
    }

    /* We don't support sub-providers right now*/
    if(objp->pk.sub_provider_id) {
	objp->pk.sub_provider_id = 0;
    }
    return 0;
}

/* This function updates an element in the hotness
 * lru_list.
 */
void
AM_hotness_lru_list_update(AM_obj_t *objp)
{
    GList 			*headp2 = NULL;
    GList 			*tailp2 = NULL;
    GList 			**headpp2 = &headp2;
    GList 			**tailpp2 = &tailp2;
    GList 			*p_prov_lru_list;
    char 			*key = NULL;
    nkn_am_prov_list_t          *lru_list = NULL;
    int			        free_key = -1;
    int                         old_list_index = -1;
    int                         new_list_index = -1;
    int                         new = 0;
    int                         ret = -1;

    if(!objp) {
	return;
    }
    if(objp->pk.type != AM_OBJ_URI) {
	return;
    }

    ret = am_check_object(objp);
    if(ret < 0) {
	glob_am_lru_list_insert_error ++;
	return;
    }

    /* Provider lru_list */
    p_prov_lru_list = objp->prov_lru_list_ptr;

    /* Find the new list that this object will go to, if at all.
       If not, the object stays in the same list. */
    old_list_index = objp->lru_list_index;
    s_am_lru_find_insert_list(objp);
    new_list_index = objp->lru_list_index;

    if((objp->prov_lru_list_ptr) &&
       (old_list_index != new_list_index)) {
	objp->lru_list_index = old_list_index;
	/*Delete from the old list */
	AM_hotness_lru_list_delete(objp);
	objp->prov_lru_list_ptr = NULL;
    } else if(objp->prov_lru_list_ptr){
	/* The object is not going to move to a new list */
	return;
    }

    /* Insert the object into the new list */
    objp->lru_list_index = new_list_index;

    /* Insert into one of the lists */
    key = s_build_lru_list_key(&objp->pk, objp->lru_list_index);
    if(!key) {
	return;
    }
    lru_list = s_return_lru_list_by_key(key, headpp2, tailpp2);

    if(*headpp2 == NULL) {
	/* Allocating a new list if not already there. This should
	   happen very early and then should never happen again. */
	if(!lru_list) {
	    lru_list = (nkn_am_prov_list_t *)
		nkn_calloc_type (1, sizeof(nkn_am_prov_list_t),
                                 mod_am_lru_list);
	    if(!lru_list) {
		DBG_LOG_MEM_UNAVAILABLE(MOD_AM);
		if(key) {
		    free(key);
		}
		return;
	    }
	    glob_am_new_prov_lru_list_entry ++;
	    nkn_am_list_tbl_insert(nkn_am_lru_list_hash_tbl,
				   nkn_am_lru_list_tbl_mutex,
				   key, lru_list);
	    new = 1;
	} else {
            free_key = 1;
        }
    } else {
	free_key = 1;
    }

    /* Always insert the obj into the head of the list */
    glob_am_new_prov_lru_list_obj ++;
    p_prov_lru_list =
	s_lru_list_insert_head(headpp2, tailpp2,
			      (void *) objp, NULL);

    if(!p_prov_lru_list) {
	return;
    }
    if(new) {
	lru_list->am_prov_list_head = *headpp2;
	lru_list->am_prov_list_tail = *tailpp2;
    } else {
	s_set_lru_list_ptr_by_key(key, headpp2, tailpp2);
    }
    objp->prov_lru_list_ptr = p_prov_lru_list;

    assert((*tailpp2)->next == NULL);
    assert((*headpp2)->prev == NULL);

    if(free_key == 1) {
	free(key);
    }
    return;
}

/* This function deletes the link from the linked lru_list.
 */
int
AM_hotness_lru_list_delete(AM_obj_t *pobj)
{
    GList *headp2 		= NULL;
    GList *tailp2 		= NULL;
    GList **headpp2 	        = &headp2;
    GList **tailpp2 	        = &tailp2;
    GList *p_prov_lru_list 	= NULL;
    char  *key                  = NULL;

    p_prov_lru_list = pobj->prov_lru_list_ptr;
    if(p_prov_lru_list != NULL) {
	key = s_build_lru_list_key(&pobj->pk, pobj->lru_list_index);
	if(!key) {
	    return -1;
	}
	s_return_lru_list_by_key(key, headpp2, tailpp2);
	if(!headpp2) {
	    assert(0);
	    return 0;
	}
	s_lru_list_delete_link(headpp2, tailpp2, p_prov_lru_list);
        g_list_free1(p_prov_lru_list);
        glob_am_lru_glib_mem -= 24;
	s_set_lru_list_ptr_by_key(key, headpp2, tailpp2);
	free(key);
	if(*headpp2) {
	    assert(*tailpp2);
	} else {
	    assert(!(*tailpp2));
	}
	glob_am_new_prov_lru_list_obj_del ++;
    }
    return 0;
}

/* This function gets the coldest item in the LRU list given the
   provider type and sub provider type.
   NOTE: This function must be called with the mutex held.
*/
int
AM_hotness_lru_list_get_coldest_by_pk(AM_pk_t *pk, AM_obj_t *outp)
{
    AM_obj_t *pobj = NULL;

    pobj = s_am_pop_coldest_lru_item(pk->provider_id, pk->sub_provider_id);
    if(!pobj) {
	return -1;
    }
    if(!outp && !outp->pk.name) {
	return -1;
    }
    /* Copy the relevant things as we don't do ref counting */
    assert(strlen(pobj->pk.name) < MAX_URI_SIZE);
    strcpy(outp->pk.name, pobj->pk.name);
    outp->pk.provider_id = pobj->pk.provider_id;
    outp->pk.sub_provider_id = pobj->pk.sub_provider_id;
    outp->pk.type = pobj->pk.type;

    outp->hotness = pobj->hotness;
    outp->cacheable = pobj->cacheable;

    return 0;
}

static void
s_am_lru_list_destroy(gpointer data)
{
    GList *in_lru_list = (GList *) data;
    g_list_free(in_lru_list);
    in_lru_list = NULL;
}

void
AM_hotness_lru_list_init(void)
{
    nkn_am_list_tbl_init(&nkn_am_lru_list_hash_tbl,
			     &nkn_am_lru_list_tbl_mutex,
			     AM_tbl_destroy_key, s_am_lru_list_destroy);
}
