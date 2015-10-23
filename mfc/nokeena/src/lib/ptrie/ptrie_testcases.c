#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <assert.h>
#include <poll.h>
#include <stdarg.h>
#include <pthread.h>

#include "ptrie/persistent_trie.h"

// Application specific definitions

typedef struct ocrp_fh_user_data { // fh_user_data_t overlay
    union {
    	struct d_struct {
	    long ocrp_incarnation;
	    long ocrp_seqno;
	    long ocrp_version;
    	} d;
    	fh_user_data_t fhud;
    } u;
} ocrp_fh_user_data_t;

typedef struct ocrp_app_data { // app_data_t overlay
    long data[8];
} ocrp_app_data_t;

#define CKP_FILE_1 "./CKP-1"
#define CKP_FILE_2 "./CKP-2"

void copy_app_data(const app_data_t *src, app_data_t *dest)
{
    ocrp_app_data_t *ap_src = (ocrp_app_data_t *) src;
    ocrp_app_data_t *ap_dest = (ocrp_app_data_t *) dest;

    *ap_dest = *ap_src;
}

void destruct_app_data(app_data_t *d)
{
    // Free deep data references, appdata_t is freed by caller
}

int mylogfunc(ptrie_log_level_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("[Level=%d]:", level);
    vprintf(fmt, ap);
}

void *my_malloc(size_t size)
{
    return malloc(size);
}

void *my_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void *my_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void my_free(void *ptr)
{
    return free(ptr);
}

int tst1(ptrie_context_t *ctx)
{
    const fh_user_data_t *pfh;

    ocrp_fh_user_data_t ocrp_fh_ud;
    ocrp_fh_user_data_t *pocrp_fh_ud;
    ocrp_app_data_t ocrp_ad;
    ocrp_app_data_t *pocrp_ad;
    int rv;

    memset(&ocrp_fh_ud, 0, sizeof(ocrp_fh_ud));
    memset(&ocrp_ad, 0, sizeof(ocrp_ad));

    // Start tst1.1
    // Update side
    ////////////////////////////////////////////////////////////////////////////
    rv = ptrie_begin_xaction(ctx);
    assert(rv == 0);

    ocrp_ad.data[0] = 1;
    rv = ptrie_add(ctx, "/a/b/c/1", (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    ocrp_ad.data[0] = 2;
    rv = ptrie_add(ctx, "/a/b/c/2", (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    ocrp_ad.data[0] = 3;
    rv = ptrie_add(ctx, "/a/b/c/prefix_3/", (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    ocrp_ad.data[0] = 4;
    rv = ptrie_add(ctx, "/a/b/c/prefix_4/", (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    ocrp_fh_ud.u.d.ocrp_incarnation = 1234;
    ocrp_fh_ud.u.d.ocrp_seqno = 1;
    ocrp_fh_ud.u.d.ocrp_version = 100;

    rv = ptrie_end_xaction(ctx, 1, (fh_user_data_t *) &ocrp_fh_ud);
    assert(rv == 0);

    pfh = ptrie_get_fh_data(ctx);
    assert(pfh != 0);
    pocrp_fh_ud = (ocrp_fh_user_data_t *)pfh;
    assert(pocrp_fh_ud->u.d.ocrp_incarnation == 
	   ocrp_fh_ud.u.d.ocrp_incarnation);
    assert(pocrp_fh_ud->u.d.ocrp_seqno == ocrp_fh_ud.u.d.ocrp_seqno);
    assert(pocrp_fh_ud->u.d.ocrp_version == ocrp_fh_ud.u.d.ocrp_version);

    // Reader side
    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_exact_match(ctx, "/a/b/c/1", (app_data_t *)&ocrp_ad);
    assert(rv == 0);
    assert(ocrp_ad.data[0] == 1);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_exact_match(ctx, "/a/b/c/2", (app_data_t *)&ocrp_ad);
    assert(rv == 0);
    assert(ocrp_ad.data[0] == 2);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_prefix_match(ctx, "/a/b/c/prefix_3/", 
    				(app_data_t *)&ocrp_ad);
    assert(rv == 1);
    assert(ocrp_ad.data[0] == 3);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_prefix_match(ctx, "/a/b/c/prefix_4/", 
    				(app_data_t *)&ocrp_ad);
    assert(rv == 1);
    assert(ocrp_ad.data[0] == 4);
    // End tst1.1
    ////////////////////////////////////////////////////////////////////////////


    // Start tst1.2
    // Update side
    ////////////////////////////////////////////////////////////////////////////
    rv = ptrie_begin_xaction(ctx);
    assert(rv == 0);

    pocrp_ad = (ocrp_app_data_t *)ptrie_tr_exact_match(ctx, "/a/b/c/1");
    assert(pocrp_ad != 0);
    assert(pocrp_ad->data[0] == 1);

    ocrp_ad = *pocrp_ad;
    ocrp_ad.data[0] += 100;
    rv = ptrie_update_appdata(ctx, "/a/b/c/1", (app_data_t *)pocrp_ad, 
    				  (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    pocrp_ad = (ocrp_app_data_t *)ptrie_tr_exact_match(ctx, "/a/b/c/2");
    assert(pocrp_ad != 0);
    assert(pocrp_ad->data[0] == 2);

    ocrp_ad = *pocrp_ad;
    ocrp_ad.data[0] += 200;
    rv = ptrie_update_appdata(ctx, "/a/b/c/2", (app_data_t *) pocrp_ad, 
				  (app_data_t *)&ocrp_ad);
    assert(rv == 0);


    rv = ptrie_tr_prefix_match(ctx, "/a/b/c/prefix_3/", 
				   ((app_data_t **)&pocrp_ad));
    assert(rv == 1);
    assert(pocrp_ad->data[0] == 3);

    ocrp_ad = *pocrp_ad;
    ocrp_ad.data[0] += 300;
    rv = ptrie_update_appdata(ctx, "/a/b/c/prefix_3/",
    				  (app_data_t *) pocrp_ad, 
				  (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    rv = ptrie_tr_prefix_match(ctx, "/a/b/c/prefix_4/", 
				   ((app_data_t **)&pocrp_ad));
    assert(rv == 1);
    assert(pocrp_ad->data[0] == 4);

    ocrp_ad = *pocrp_ad;
    ocrp_ad.data[0] += 400;
    rv = ptrie_update_appdata(ctx, "/a/b/c/prefix_4/",
    				  (app_data_t *) pocrp_ad, 
				  (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    ocrp_fh_ud.u.d.ocrp_seqno++;
    rv = ptrie_end_xaction(ctx, 1, (fh_user_data_t *) &ocrp_fh_ud);
    assert(rv == 0);

    pfh = ptrie_get_fh_data(ctx);
    assert(pfh != 0);
    pocrp_fh_ud = (ocrp_fh_user_data_t *)pfh;
    assert(pocrp_fh_ud->u.d.ocrp_incarnation == 
	   ocrp_fh_ud.u.d.ocrp_incarnation);
    assert(pocrp_fh_ud->u.d.ocrp_seqno == ocrp_fh_ud.u.d.ocrp_seqno);
    assert(pocrp_fh_ud->u.d.ocrp_version == ocrp_fh_ud.u.d.ocrp_version);

    // Reader side
    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_exact_match(ctx, "/a/b/c/1", (app_data_t *)&ocrp_ad);
    assert(rv == 0);
    assert(ocrp_ad.data[0] == 101);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_exact_match(ctx, "/a/b/c/2", (app_data_t *)&ocrp_ad);
    assert(rv == 0);
    assert(ocrp_ad.data[0] == 202);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_prefix_match(ctx, "/a/b/c/prefix_3/", (app_data_t*)&ocrp_ad);
    assert(rv == 1);
    assert(ocrp_ad.data[0] == 303);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_prefix_match(ctx, "/a/b/c/prefix_4/", (app_data_t*)&ocrp_ad);
    assert(rv == 1);
    assert(ocrp_ad.data[0] == 404);

    // End tst1.2
    ////////////////////////////////////////////////////////////////////////////


    // Start tst1.3
    // Update side
    ////////////////////////////////////////////////////////////////////////////
    rv = ptrie_begin_xaction(ctx);
    assert(rv == 0);

    rv = ptrie_remove(ctx, "/a/b/c/1");
    assert(rv == 0);

    rv = ptrie_remove(ctx, "/a/b/c/2");
    assert(rv == 0);

    rv = ptrie_remove(ctx, "/a/b/c/prefix_3/");
    assert(rv == 0);

    rv = ptrie_remove(ctx, "/a/b/c/prefix_4/");
    assert(rv == 0);

    ocrp_fh_ud.u.d.ocrp_seqno++;
    rv = ptrie_end_xaction(ctx, 1, (fh_user_data_t *) &ocrp_fh_ud);
    assert(rv == 0);

    pfh = ptrie_get_fh_data(ctx);
    assert(pfh != 0);
    pocrp_fh_ud = (ocrp_fh_user_data_t *)pfh;
    assert(pocrp_fh_ud->u.d.ocrp_incarnation == 
    	   ocrp_fh_ud.u.d.ocrp_incarnation);
    assert(pocrp_fh_ud->u.d.ocrp_seqno == ocrp_fh_ud.u.d.ocrp_seqno);
    assert(pocrp_fh_ud->u.d.ocrp_version == ocrp_fh_ud.u.d.ocrp_version);

    // Reader side
    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_exact_match(ctx, "/a/b/c/1", (app_data_t *)&ocrp_ad);
    assert(rv != 0);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_exact_match(ctx, "/a/b/c/2", (app_data_t *)&ocrp_ad);
    assert(rv != 0);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_prefix_match(ctx, "/a/b/c/prefix_3/", (app_data_t*)&ocrp_ad);
    assert(rv == 0);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_prefix_match(ctx, "/a/b/c/prefix_4/", (app_data_t*)&ocrp_ad);
    assert(rv == 0);
    // End tst1.3
    ////////////////////////////////////////////////////////////////////////////

    // Start tst1.4
    // Update side
    ////////////////////////////////////////////////////////////////////////////
    rv = ptrie_begin_xaction(ctx);
    assert(rv == 0);

    ocrp_ad.data[0] = 1000;
    rv = ptrie_add(ctx, "/R/a/b/c/1", (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    ocrp_ad.data[0] = 2000;
    rv = ptrie_add(ctx, "/R/a/b/c/2", (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    ocrp_ad.data[0] = 3000;
    rv = ptrie_add(ctx, "/R/a/b/c/prefix_3/", (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    ocrp_ad.data[0] = 4000;
    rv = ptrie_add(ctx, "/R/a/b/c/prefix_4/", (app_data_t *)&ocrp_ad);
    assert(rv == 0);

    ocrp_fh_ud.u.d.ocrp_seqno++;
    rv = ptrie_end_xaction(ctx, 1, (fh_user_data_t *) &ocrp_fh_ud);
    assert(rv == 0);

    pfh = ptrie_get_fh_data(ctx);
    assert(pfh != 0);
    pocrp_fh_ud = (ocrp_fh_user_data_t *)pfh;
    assert(pocrp_fh_ud->u.d.ocrp_incarnation == 
	   ocrp_fh_ud.u.d.ocrp_incarnation);
    assert(pocrp_fh_ud->u.d.ocrp_seqno == ocrp_fh_ud.u.d.ocrp_seqno);
    assert(pocrp_fh_ud->u.d.ocrp_version == ocrp_fh_ud.u.d.ocrp_version);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_exact_match(ctx, "/R/a/b/c/1", (app_data_t *)&ocrp_ad);
    assert(rv == 0);
    assert(ocrp_ad.data[0] == 1000);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_exact_match(ctx, "/R/a/b/c/2", (app_data_t *)&ocrp_ad);
    assert(rv == 0);
    assert(ocrp_ad.data[0] == 2000);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_prefix_match(ctx, "/R/a/b/c/prefix_3/", 
    				(app_data_t *)&ocrp_ad);
    assert(rv == 1);
    assert(ocrp_ad.data[0] == 3000);

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    rv = ptrie_prefix_match(ctx, "/R/a/b/c/prefix_4/", 
    				(app_data_t *)&ocrp_ad);
    assert(rv == 1);
    assert(ocrp_ad.data[0] == 4000);
    // End tst1.4
    ////////////////////////////////////////////////////////////////////////////


    return 0;
}

void val2tststr(long val, char *buf, int maxbufsz)
{
    long dval = val;
    int write_bytes = 0;
    int rv;

    rv = snprintf(&buf[write_bytes], (maxbufsz-write_bytes), 
		  "[%ld]", val);
    write_bytes += rv;
    assert((write_bytes+1 < maxbufsz));

    do {
    	dval /= 10;
    	rv = snprintf(&buf[write_bytes], (maxbufsz-write_bytes), 
		      "/%ld", dval);
	write_bytes += rv;
	assert((write_bytes+1 < maxbufsz));
    } while (dval);

    rv = snprintf(&buf[write_bytes], (maxbufsz-write_bytes), 
		  "/%ld", val);
    write_bytes += rv;
    assert((write_bytes+1 < maxbufsz));
}

int tst_commit_xaction(ptrie_context_t *ctx, int commit, 
		       ocrp_fh_user_data_t *ocrp_fh_ud)
{
    const fh_user_data_t *pfh;
    ocrp_fh_user_data_t *pocrp_fh_ud;
    int rv;

    if (commit) {
    	ocrp_fh_ud->u.d.ocrp_seqno++;
    }
    rv = ptrie_end_xaction(ctx, commit, (fh_user_data_t *) ocrp_fh_ud);
    assert(rv == 0);

    pfh = ptrie_get_fh_data(ctx);
    assert(pfh != 0);
    pocrp_fh_ud = (ocrp_fh_user_data_t *)pfh;
    assert(pocrp_fh_ud->u.d.ocrp_incarnation == 
    	   ocrp_fh_ud->u.d.ocrp_incarnation);
    assert(pocrp_fh_ud->u.d.ocrp_seqno == ocrp_fh_ud->u.d.ocrp_seqno);
    assert(pocrp_fh_ud->u.d.ocrp_version == ocrp_fh_ud->u.d.ocrp_version);

    return 0;
}

int tst2(ptrie_context_t *ctx)
{
    long n;
    const fh_user_data_t *pfh;
    ocrp_fh_user_data_t ocrp_fh_ud;
    ocrp_fh_user_data_t *pocrp_fh_ud;
    ocrp_app_data_t ocrp_ad;
    char key[4096];
    int rv;

    memset(&ocrp_fh_ud, 0, sizeof(ocrp_fh_ud));
    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    ocrp_fh_ud.u.d.ocrp_incarnation = 1000;
    ocrp_fh_ud.u.d.ocrp_seqno = 1000;
    ocrp_fh_ud.u.d.ocrp_version = 100;

    rv = ptrie_begin_xaction(ctx);
    assert(rv == 0);

    // Add prefixes
    for (n = 0; n < 1000000; n++) {
    	val2tststr(n, key, sizeof(key));
    	ocrp_ad.data[0] = 0xfffff + n;
    	rv = ptrie_add(ctx, key, (app_data_t *)&ocrp_ad);
	if (rv < 0) {
	    rv = tst_commit_xaction(ctx, 1, &ocrp_fh_ud);
	    assert(rv == 0);
	    rv = ptrie_begin_xaction(ctx);
	    assert(rv == 0);
	    rv = ptrie_add(ctx, key, (app_data_t *)&ocrp_ad);
	    assert(rv == 0);
	} else {
	    assert(rv == 0);
	}
    }
    rv = tst_commit_xaction(ctx, 1, &ocrp_fh_ud);
    assert(rv == 0);

    // Verify prefixes
    for (n = 0; n < 1000000; n++) {
    	val2tststr(n, key, sizeof(key));
    	rv = ptrie_prefix_match(ctx, key, (app_data_t *)&ocrp_ad);
	if (rv > 0) {
	    assert(ocrp_ad.data[0] == (0xfffff + n));
	} else {
	    assert(!"Invalid rv");
	}
    }


    printf("tst2 - Delete/add test\n");
    rv = ptrie_begin_xaction(ctx);
    assert(rv == 0);

    printf("tst2 - Delete/add test - Delete\n");
    for (n = 0; n < 500000; n++) {
    	val2tststr(n, key, sizeof(key));
    	rv = ptrie_remove(ctx, key);
	if (rv < 0) {
	    rv = tst_commit_xaction(ctx, 1, &ocrp_fh_ud);
	    assert(rv == 0);
	    rv = ptrie_begin_xaction(ctx);
	    assert(rv == 0);
	    rv = ptrie_remove(ctx, key);
	    assert(rv == 0);
	} else {
	    assert(rv == 0);
	}
    }

    printf("tst2 - Delete/add test - Add\n");
    for (n = 0; n < 500000; n++) {
    	val2tststr(n, key, sizeof(key));
    	ocrp_ad.data[0] = 0xfffff + n;
    	rv = ptrie_add(ctx, key, (app_data_t *)&ocrp_ad);
	if (rv < 0) {
	    rv = tst_commit_xaction(ctx, 1, &ocrp_fh_ud);
	    assert(rv == 0);
	    rv = ptrie_begin_xaction(ctx);
	    assert(rv == 0);
	    rv = ptrie_add(ctx, key, (app_data_t *)&ocrp_ad);
	    assert(rv == 0);
	} else {
	    assert(rv == 0);
	}
    }

    printf("tst2 - Delete/add test - Commit\n");
    rv = tst_commit_xaction(ctx, 1, &ocrp_fh_ud);
    assert(rv == 0);

    printf("tst2 - Delete/add test - Verify\n");
    for (n = 0; n < 500000; n++) {
    	val2tststr(n, key, sizeof(key));
    	rv = ptrie_prefix_match(ctx, key, (app_data_t *)&ocrp_ad);
	if (rv > 0) {
	    assert(ocrp_ad.data[0] == (0xfffff + n));
	} else {
	    assert(!"Invalid rv");
	}
    }

    return 0;
}

int tst3(ptrie_context_t *ctx)
{
    long n;
    const fh_user_data_t *pfh;
    ocrp_fh_user_data_t ocrp_fh_ud;
    ocrp_fh_user_data_t *pocrp_fh_ud;
    ocrp_app_data_t ocrp_ad;
    ocrp_app_data_t *pocrp_ad;
    ocrp_app_data_t new_ad;
    char key[4096];
    int rv;

    memset(&ocrp_fh_ud, 0, sizeof(ocrp_fh_ud));
    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    ocrp_fh_ud.u.d.ocrp_incarnation = 999;
    ocrp_fh_ud.u.d.ocrp_seqno = 2000;
    ocrp_fh_ud.u.d.ocrp_version = 200;

    rv = ptrie_begin_xaction(ctx);
    assert(rv == 0);

    printf("tst3 - Reset/add test - Reset\n");
    rv = ptrie_reset(ctx);
    assert(rv == 0);

    printf("tst3 - Delete/add test - Add\n");
    for (n = 500000-1; n >= 0; n--) {
    	val2tststr(n, key, sizeof(key));
    	ocrp_ad.data[0] = 0xfffff + n;
    	rv = ptrie_add(ctx, key, (app_data_t *)&ocrp_ad);
	if (rv < 0) {
	    rv = tst_commit_xaction(ctx, 1, &ocrp_fh_ud);
	    assert(rv == 0);
	    rv = ptrie_begin_xaction(ctx);
	    assert(rv == 0);
	    rv = ptrie_add(ctx, key, (app_data_t *)&ocrp_ad);
	    assert(rv == 0);
	} else {
	    assert(rv == 0);
	}
    }

    rv = tst_commit_xaction(ctx, 1, &ocrp_fh_ud);
    assert(rv == 0);

    printf("tst3 - Reset/add test - Verify\n");
    for (n = 0; n < 500000; n++) {
    	val2tststr(n, key, sizeof(key));
    	rv = ptrie_prefix_match(ctx, key, (app_data_t *)&ocrp_ad);
	if (rv > 0) {
	    assert(ocrp_ad.data[0] == (0xfffff + n));
	} else {
	    assert(!"Invalid rv");
	}
    }

    rv = ptrie_begin_xaction(ctx);
    assert(rv == 0);

    printf("tst3 - Update test - Update\n");
    for (n = 500000-1; n >= 0; n--) {
    	val2tststr(n, key, sizeof(key));

	pocrp_ad = (ocrp_app_data_t *)ptrie_tr_exact_match(ctx, key);
	assert(pocrp_ad != 0);
	new_ad = *pocrp_ad;
	new_ad.data[0] = 0xfffff + n + n;

	rv = ptrie_update_appdata(ctx, key, (app_data_t *)pocrp_ad,
				      (app_data_t *)&new_ad);
	if (rv < 0) {
	    rv = tst_commit_xaction(ctx, 1, &ocrp_fh_ud);
	    assert(rv == 0);
	    rv = ptrie_begin_xaction(ctx);
	    assert(rv == 0);

	    pocrp_ad = (ocrp_app_data_t *)ptrie_tr_exact_match(ctx, key);
	    assert(pocrp_ad != 0);
	    new_ad = *pocrp_ad;
	    new_ad.data[0] = 0xfffff + n + n;

	    rv = ptrie_update_appdata(ctx, key, (app_data_t *)pocrp_ad,
					  (app_data_t *)&new_ad);
	    assert(rv == 0);
	} else {
	    assert(rv == 0);
	}
    }

    rv = tst_commit_xaction(ctx, 1, &ocrp_fh_ud);
    assert(rv == 0);

    printf("tst3 - Update test - Verify\n");
    for (n = 0; n < 500000; n++) {
    	val2tststr(n, key, sizeof(key));
    	rv = ptrie_prefix_match(ctx, key, (app_data_t *)&ocrp_ad);
	if (rv > 0) {
	    assert(ocrp_ad.data[0] == (0xfffff + n + n));
	} else {
	    assert(!"Invalid rv");
	}
    }

    printf("tst3 - Commit Reset Abort test - Reset Abort\n");
    rv = ptrie_begin_xaction(ctx);
    rv = ptrie_reset(ctx);
    assert(rv == 0);

    rv = tst_commit_xaction(ctx, 0, &ocrp_fh_ud); // Abort xaction
    assert(rv == 0);

    printf("tst3 - Commit Reset Abort test - Verify Current\n");
    for (n = 0; n < 500000; n++) {
    	val2tststr(n, key, sizeof(key));
    	rv = ptrie_prefix_match(ctx, key, (app_data_t *)&ocrp_ad);
	if (rv > 0) {
	    assert(ocrp_ad.data[0] == (0xfffff + n + n));
	} else {
	    assert(!"Invalid rv");
	}
    }

    rv = ptrie_begin_xaction(ctx);
    assert(rv == 0);

    printf("tst3 - Commit Reset Abort test - Verify Shadow\n");
    for (n = 0; n < 500000; n++) {
    	val2tststr(n, key, sizeof(key));
    	rv = ptrie_tr_prefix_match(ctx, key, (app_data_t **)&pocrp_ad);
	if (rv > 0) {
	    assert(pocrp_ad->data[0] == (0xfffff + n + n));
	} else {
	    assert(!"Invalid rv");
	}
    }

    rv = tst_commit_xaction(ctx, 0, &ocrp_fh_ud); // Abort xaction
    assert(rv == 0);

    return 0;
}

AO_t reader_action;

typedef struct reader_data 
{
    ptrie_context_t *ctx;
    int threadnum;
} reader_data_t;

void *reader_thread(void *arg)
{
    char key[1024];
    reader_data_t *rd;
    ptrie_context_t *ctx;
    pthread_t mytid;
    long action;
    long prefix_hits = 0;
    long hits = 0;
    ocrp_app_data_t ad;
    int rv;
    int last_action = 0;


    rd = (reader_data_t *) arg;
    ctx = (ptrie_context_t *)rd->ctx;
    mytid = pthread_self();

    snprintf(key, sizeof(key), "/{0x%lx}/", mytid);
    printf("Thread #%d [0x%lx] Started\n", rd->threadnum, mytid);

    while (1) {
    	action = AO_load(&reader_action);
	switch (action) {
	case 1:
	    if (last_action != 1) {
	    	prefix_hits = 0;
	    	hits = 0;
	    }
	    last_action = 1;
	    break;
	case 2:
	    if ((last_action != 2) && (hits || prefix_hits)) {
	    	printf("*** #%d [0x%x] prefix_hits: %ld hits: %ld\n",
		       rd->threadnum, mytid, prefix_hits, hits);
		hits = 0;
		prefix_hits = 0;
	    }
	    last_action = 2;
	    break;
	default:
	    break;
	}

	rv = ptrie_prefix_match(ctx, key, (app_data_t *)&ad);
	if (rv > 1) {
	    prefix_hits++;
	}

	rv = ptrie_exact_match(ctx, key, (app_data_t *)&ad);
	if (!rv) {
	    assert(ad.data[0] == mytid);
	    hits++;
	}
    }

}

void *read_only_thread(void *arg)
{
    char key[1024];
    reader_data_t *rd;
    ptrie_context_t *ctx;
    pthread_t mytid;
    long prefix_hits = 0;
    long hits = 0;
    long lookups = 0;
    ocrp_app_data_t ad;
    int rv;
    int last_action = 0;
    time_t last_ct;
    time_t ct;


    rd = (reader_data_t *) arg;
    ctx = (ptrie_context_t *)rd->ctx;
    mytid = pthread_self();

    snprintf(key, sizeof(key), "/{0x%lx}/", mytid);
    printf("Thread #%d [0x%lx] Started\n", rd->threadnum, mytid);
    last_ct = time(0);

    while (1) {
        ct = time(0);

	if ((ct - last_ct) > 1) {
	    printf("*** #%d [0x%x] prefix_hits: %ld hits: %ld lookups=%ld\n",
		   rd->threadnum, mytid, prefix_hits, hits, lookups);
	    prefix_hits = 0;
	    hits = 0;
	    lookups = 0;
	    last_ct = ct;
	}

	lookups++;
	rv = ptrie_prefix_match(ctx, key, (app_data_t *)&ad);
	if (rv > 1) {
	    prefix_hits++;
	}

	lookups++;
	rv = ptrie_exact_match(ctx, key, (app_data_t *)&ad);
	if (!rv) {
	    assert(ad.data[0] == mytid);
	    hits++;
	}
    }

}

int concurrency_reader_test(ptrie_context_t *ctx)
{
#define RDR_THDS 16
    int rv;
    int n;
    int passcnt = 1;

    reader_data_t reader_data[RDR_THDS];
    pthread_t reader_tid[RDR_THDS];

    char key[1024];
    ocrp_app_data_t ocrp_ad;
    ocrp_fh_user_data_t ocrp_fh_ud;

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    memset(&ocrp_fh_ud, 0, sizeof(ocrp_fh_ud));

    for (n = 0; n < RDR_THDS; n++) {
    	reader_data[n].ctx = ctx;
    	reader_data[n].threadnum = n;
    	rv = pthread_create(&reader_tid[n], 0, read_only_thread, 
			    &reader_data[n]);
	assert(rv == 0);
    }
    sleep(3);

    rv = ptrie_begin_xaction(ctx);
    assert(rv == 0);

    for (n = 0; n < RDR_THDS; n++) {
    	snprintf(key, sizeof(key), "/{0x%lx}/", reader_tid[n]);

	ocrp_ad.data[0] = reader_tid[n];
	rv = ptrie_add(ctx, key, (app_data_t *)&ocrp_ad);
	assert(rv == 0);
    }
    ocrp_fh_ud.u.d.ocrp_seqno++;
    rv = ptrie_end_xaction(ctx, 1, (fh_user_data_t *) &ocrp_fh_ud);
    assert(rv == 0);

    while (1) {
    	sleep(1);
    }
    return 0;
}

int concurrency_test(ptrie_context_t *ctx)
{
#define RDR_THDS 16
    int rv;
    int n;
    int passcnt = 1;

    reader_data_t reader_data[RDR_THDS];
    pthread_t reader_tid[RDR_THDS];

    char key[1024];
    ocrp_app_data_t ocrp_ad;
    ocrp_fh_user_data_t ocrp_fh_ud;

    memset(&ocrp_ad, 0, sizeof(ocrp_ad));
    memset(&ocrp_fh_ud, 0, sizeof(ocrp_fh_ud));

    for (n = 0; n < RDR_THDS; n++) {
    	reader_data[n].ctx = ctx;
    	reader_data[n].threadnum = n;
    	rv = pthread_create(&reader_tid[n], 0, reader_thread, &reader_data[n]);
	assert(rv == 0);
    }
    sleep(3);

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    printf("****  Pass: %d\n\n", passcnt++);

    for (n = 0; n < RDR_THDS; n++) {
        AO_store(&reader_action, 1);
	sleep(1);

    	snprintf(key, sizeof(key), "/{0x%lx}/", reader_tid[n]);

	rv = ptrie_begin_xaction(ctx);
	assert(rv == 0);

	ocrp_ad.data[0] = reader_tid[n];
	rv = ptrie_add(ctx, key, (app_data_t *)&ocrp_ad);
	assert(rv == 0);

    	ocrp_fh_ud.u.d.ocrp_seqno++;
    	rv = ptrie_end_xaction(ctx, 1, (fh_user_data_t *) &ocrp_fh_ud);
	assert(rv == 0);

	sleep(1);
        AO_store(&reader_action, 2);

	rv = ptrie_begin_xaction(ctx);
	assert(rv == 0);

	rv = ptrie_remove(ctx, key);
	assert(rv == 0);

    	ocrp_fh_ud.u.d.ocrp_seqno++;
    	rv = ptrie_end_xaction(ctx, 1, (fh_user_data_t *) &ocrp_fh_ud);
	assert(rv == 0);
    }

    ///////////////////////////////////////////////////////////////////////////
    } // End while
    ///////////////////////////////////////////////////////////////////////////

    return 0;
}

main(int argc, char *argv[])
{
    ptrie_context_t *ctx;
    int rv;
    int tests_to_run = 0;
    ptrie_config_t cfg;

    if (argc > 1) {
    	tests_to_run = atoi(argv[1]);
    } else {
    	tests_to_run = 0; // Run all
    }

    assert(sizeof(file_header_t) == DEV_BSIZE);
    assert(sizeof(ocrp_fh_user_data_t) <= sizeof(fh_user_data_t));
    assert(sizeof(ocrp_app_data_t) <= sizeof(app_data_t));

    memset(&cfg, 0, sizeof(cfg));
    cfg.interface_version = PTRIE_INTF_VERSION;
    cfg.proc_logfunc = mylogfunc;
    cfg.proc_malloc = my_malloc;
    cfg.proc_calloc = my_calloc;
    cfg.proc_realloc = my_realloc;
    cfg.proc_free = my_free;
    rv = ptrie_init(&cfg);
    assert(rv == 0);

    ctx = new_ptrie_context(copy_app_data, destruct_app_data);
    assert(ctx != 0);

    rv = ptrie_recover_from_ckpt(ctx, CKP_FILE_1, CKP_FILE_2);
    assert(rv == 0);

    if (tests_to_run < 0) {
    	if (tests_to_run == -1) {
	    concurrency_test(ctx);
	} else if (tests_to_run == -2) {
	    concurrency_reader_test(ctx);
	}
    	goto exit;
    }


    if (!tests_to_run || (tests_to_run >= 1)) {
    	printf("Start tst1()\n");
    	rv = tst1(ctx);
    	assert(rv == 0);
    	printf("End tst1()\n");

        if (tests_to_run == 1) {
	    goto exit;
	}
    }

    if (!tests_to_run || (tests_to_run >= 2)) {
    	printf("Start tst2()\n");
    	rv = tst2(ctx);
    	assert(rv == 0);
    	printf("End tst2()\n");

        if (tests_to_run == 2) {
	    goto exit;
	}
    }

    if (!tests_to_run || (tests_to_run >= 3)) {
    	printf("Start tst3()\n");
    	rv = tst3(ctx);
    	assert(rv == 0);
    	printf("End tst3()\n");
        if (tests_to_run == 3) {
	    goto exit;
	}
    }

exit:
    delete_ptrie_context(ctx);
}
