/*
 *******************************************************************************
 * nkn_trie_stubs.c 
 *	nkn_trie.c is linked into libnkn_common.a which is shared by 
 *	multiple binaries.   Stubs are added to break the dependency 
 *	on libcprops.so which is only required by nvsd.
 *******************************************************************************
 */
#include <assert.h>
#include "cprops/collection.h"
#include "cprops/trie.h"

#define UNUSED_ARGUMENT(x) (void)x

cp_trie *cp_trie_create_trie(int mode, cp_copy_fn cfn, cp_destructor_fn dfn)
{
    UNUSED_ARGUMENT(mode);
    UNUSED_ARGUMENT(cfn);
    UNUSED_ARGUMENT(dfn);
    assert(!"cp_trie_create_trie()");
    return 0;
}

int cp_trie_destroy(cp_trie *t)
{
    UNUSED_ARGUMENT(t);
    assert(!"cp_trie_destroy()");
    return 1;
}

int cp_trie_add(cp_trie *t, char *key, void *nd)
{
    UNUSED_ARGUMENT(t);
    UNUSED_ARGUMENT(key);
    UNUSED_ARGUMENT(nd);
    assert(!"cp_trie_add()");
    return 1;
}


int cp_trie_prefix_match(cp_trie *t, char *key, void **pnd)
{
    UNUSED_ARGUMENT(t);
    UNUSED_ARGUMENT(key);
    UNUSED_ARGUMENT(pnd);
    assert(!"cp_trie_prefix_match()");
    return 0;
}

void *cp_trie_exact_match(cp_trie *t, char *key)
{
    UNUSED_ARGUMENT(t);
    UNUSED_ARGUMENT(key);
    assert(!"cp_trie_exact_match()");
    return 0;
}

/*
 * End of nkn_trie_stubs.c
 */
