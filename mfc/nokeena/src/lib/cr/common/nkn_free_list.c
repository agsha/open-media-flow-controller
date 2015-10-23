/**
 * @file   nkn_free_list.c
 * @author Sunil Mukundan <smukundan@cmbu-build03>
 * @date   Wed Feb 29 02:08:20 2012
 * 
 * @brief  
 * 
 * 
 */
#include "nkn_free_list.h"

#ifdef FREE_LIST_UT
uint64_t UT_FL_CP_FLAG = 0x0000000000000000;
#define _UT_FL_CP_01 || (UT_FL_CP_FLAG & 0x01)
#else
#define _UT_FL_CP_01  
#endif

static int32_t obj_list_cleanup(struct tag_obj_list_ctx *ctx);
static int32_t obj_list_get_free(obj_list_ctx_t *ctx, void **out);
static int32_t obj_list_return_free(obj_list_ctx_t *ctx, void *p);

int32_t
obj_list_create(int32_t num_objs, obj_create_fnp create_obj, 
		obj_delete_fnp delete_obj, obj_list_ctx_t **out)
{
    int32_t i = 0, err = 0;
    void *p = NULL;
    obj_list_t *free_list = NULL;
    obj_list_ctx_t *ctx = NULL;
    obj_list_elm_t *elm = NULL;

    assert(out);
    assert(create_obj);
    *out = NULL;
    
    ctx = (obj_list_ctx_t *)
	nkn_calloc_type(1, sizeof(obj_list_ctx_t), 100);
    if (!ctx _UT_FL_CP_01) {
	err = -ENOMEM;
	goto error;
    }
    ctx->obj_delete = delete_obj;
    ctx->delete = obj_list_cleanup;
    ctx->pop_free = obj_list_get_free;
    ctx->push_free = obj_list_return_free;
    ctx->tot_elm = num_objs;
    ctx->num_free_elm = num_objs;

    pthread_mutex_init(&ctx->lock, NULL);

    free_list = (obj_list_t *)
	nkn_calloc_type(1, sizeof(obj_list_t), 100);
    if (!free_list) {
	err = -ENOMEM;
	goto error;
    }
    ctx->free_list = free_list;

    TAILQ_INIT(free_list);
    for(i = 0; i < num_objs; i++) {
	p = create_obj();
	if (!p) {
	    err = -ENOMEM;
	    goto error;
	}
	elm = (obj_list_elm_t *)
	    nkn_calloc_type(1, sizeof(obj_list_elm_t), 100);
	if (!elm) {
	    err = -ENOMEM;
	    goto error;
	}
	elm->obj = p;
	TAILQ_INSERT_TAIL(free_list, elm, entries);
    }

    *out = ctx;
    return err; 

 error:
    /* three possibile control flows can lead here
     * 1. mem alloc for ctx failed.
     * 2. during element allocation ctx is created and a some elements
     * are added to the list but failure occured before all elements
     * were added. partial clean up of addede elements needs to happen
     * in this case
     * 3. UT test forces it with a mem alloc failure, here the actual
     * alloc succeeded but was forced here via a UT control path flag
     * in this case, ctx needs to be freed
     */
    if (ctx) { 		    /* case 1 */
	if (ctx->delete) {  /* case 2 */
	    ctx->delete((struct tag_obj_list_ctx*)ctx);
	} else { 	    /* case 3 */
	    free(ctx);
	}
    }
    return err;
}

static int32_t
obj_list_cleanup(struct tag_obj_list_ctx *ctx)
{
    obj_list_t *free_list = NULL;
    obj_list_elm_t *elm = NULL;

    if (!ctx) {
	return 0;
    }

    free_list = ctx->free_list;
    pthread_mutex_lock(&ctx->lock);
    if (free_list) {
	while ((elm = TAILQ_FIRST(free_list))) {
	    TAILQ_REMOVE(free_list, elm, entries);
	    if (elm) {
		if (elm->obj) {
		    if (ctx->obj_delete) {
			ctx->obj_delete(elm->obj);
		    } else {
			free(elm->obj);
		    }
		}
		free(elm);
	    }
	}
	free(free_list);
    }
    pthread_mutex_unlock(&ctx->lock);

    free(ctx);

    return 0;
}

static int32_t
obj_list_get_free(obj_list_ctx_t *ctx, void **out)
{
    obj_list_t *free_list = NULL;
    obj_list_elm_t *elm = NULL;
    int32_t err = 0;

    assert(ctx);
    assert(out);

    *out = NULL;
    pthread_mutex_lock(&ctx->lock);
    free_list = ctx->free_list;   
    if (TAILQ_EMPTY(free_list)) {
	err = -E_OBJ_LIST_EMPTY;
	goto error;
    }
    elm = TAILQ_FIRST(free_list);
    TAILQ_REMOVE(free_list, elm, entries);
    AO_fetch_and_sub1(&ctx->num_free_elm);
    *out = elm->obj;

 error:
    free(elm);
    pthread_mutex_unlock(&ctx->lock);
    return err;
    
}

static int32_t
obj_list_return_free(obj_list_ctx_t *ctx, void *p)
{
    obj_list_t *free_list = NULL;
    obj_list_elm_t *elm = NULL;
    int32_t err = 0;

    assert(ctx);
    assert(p);

    pthread_mutex_lock(&ctx->lock);
    free_list = ctx->free_list;   

    elm = (obj_list_elm_t *)
	nkn_calloc_type(1, sizeof(obj_list_elm_t), 100);
    if (!elm) {
	err = -ENOMEM;
	goto error;
	}
    elm->obj = p;
    AO_fetch_and_add1(&ctx->num_free_elm);
    TAILQ_INSERT_TAIL(free_list, elm, entries);

 error:
    pthread_mutex_unlock(&ctx->lock);
    return err;
}


/* UNIT TEST SUITE:
 * compile with Makefile.ut using 'make -f Makefile.ut
 * requires libnkn_memalloc; either use LDD to sepcify path to this
 * library explicitly or run this binary in MF'X' products
 */

#ifdef FREE_LIST_UT

#include "nkn_mem_counter_def.h"
#include "cr_utils.h"
#include "math.h"

int nkn_mon_add(const char *name, const char *instance, void *obj,
		uint32_t size)
{   
    return 0;
}

void *create_elm(void) 
{
    return malloc(1);
}

void *create_elm1(void)
{
    static int32_t num = 0;
    
    if (num < 100) {
	num++;
	return malloc(1);
    } else {
	return NULL;
    }
	       
}

int32_t
UT_FL_TEST01(void)
{
    obj_list_ctx_t *ctx = NULL;
    int32_t err = 0, num_elm_pop = 0;
    obj_list_elm_t *p = NULL, *p1 = NULL;
    obj_list_t head;
    
    TAILQ_INIT(&head);
    err = obj_list_create(100000, create_elm, NULL, &ctx);
    if (err) {
	return -1;
    }
    
    do {
	err = ctx->pop_free(ctx, (void **)&p);
	if (err == -E_OBJ_LIST_EMPTY) {
	    break;
	} else if (err) {
	    return -1;
	}
	
	p1 = (obj_list_elm_t*)
	    calloc(1, sizeof(obj_list_elm_t));
	p1->obj = p;
	TAILQ_INSERT_TAIL(&head, p1, entries);
	num_elm_pop++;
    } while(p);
    if (ctx->tot_elm != num_elm_pop) {
	return -1;
    }

    p1 = NULL;
    while((p1 = TAILQ_FIRST(&head))) {
	TAILQ_REMOVE(&head, p1, entries);
	ctx->push_free(ctx, p1->obj);
	free(p1);
	num_elm_pop--;
    } 
    
    if (num_elm_pop) {
	return -1;
    }
    
    if (ctx->num_free_elm != ctx->tot_elm) {
	return -1;
    }
    
    ctx->delete(ctx);
    return 0;
}

/* forces memory allocation failure during context creation */
int32_t
UT_FL_CP01(void)
{
    int32_t err = 0;
    obj_list_ctx_t *ctx = NULL;
    
    UT_FL_CP_FLAG |= 0x01;
    err = obj_list_create(100000, create_elm, NULL, &ctx); 
    if (err != -ENOMEM) {
	return -1;
    }
    UT_FL_CP_FLAG &= ~(0x01);
    return 0;
}

/* forces case where element creation fails after N elements (100 in
 * this case). this forces the code into the control path where
 * partially allocated lists are cleaned up correctly
 */
int32_t
UT_FL_CP02(void)
{
    int32_t err = 0;
    obj_list_ctx_t *ctx = NULL;
    
    err = obj_list_create(100000, create_elm1, NULL, &ctx); 
    if (err != -ENOMEM) {
	return -1;
    }
    
    return 0;
}

int32_t
UT_GEO_DIST01()
{
    conjugate_graticule_t cg1 = {13.0810, 80.2740}; //chennai
    conjugate_graticule_t cg2 = {10.8000, 79.1500}; //tanjore
    conjugate_graticule_t cg3 = {-34.6036, -58.3817};//buenos aires
    double d = 0;
    int32_t err = 0;

    /* two places in the same hemisphere */
    d = compute_geo_distance(&cg1, &cg2); //should be roughly between
					  //275 & 285
    if (floor(d) < 275 || 
	floor(d) > 285) {
	err = -1;
    }

    /* two places in different hemispheres */
    d = compute_geo_distance(&cg1, &cg3); //should be roughly between
					  //15220 & 15230
    if (floor(d) < 15220 || 
	floor(d) > 15230) {
	err = -1;
    }

    return err;
}

int32_t
UT_GEO_DIST02(void)
{
    conjugate_graticule_t cg1 = {0, 0};
    conjugate_graticule_t cg2 = {0, 0};
    double d = 0.0;
    int32_t err = 0;
    
    d = compute_geo_distance(&cg1, &cg2);
    if (floor(d) != 0) {
	err = -1;
    }

    return err;
}

int32_t
UT_LABEL_REV01(void)
{
    char *str = "www.google.com", *out = NULL;
    
    out = domain_label_rev(str);
    
    free(out);
    return 0;
    
}

int32_t
main(int argc, char *argv[])
{

    /* free list unit tests */
    UT_FL_TEST01();
    UT_FL_CP01();
    UT_FL_CP02();

    /* geo distance unit tests */
    assert(UT_GEO_DIST01() == 0);
    assert(UT_GEO_DIST02() == 0);

    /* label revers UT */
    UT_LABEL_REV01();

    return 0;
    
}
#endif
