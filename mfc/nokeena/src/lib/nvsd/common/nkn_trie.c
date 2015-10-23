/*
 *******************************************************************************
 * nkn_trie.c -- Patricia Tree interface
 *******************************************************************************
 */
#include "nkn_trie.h"
#include "cprops/collection.h"
#include "cprops/trie.h"

/*
 * nkn_trie_create() - Create Patricia Tree
 *	Returns: != 0 => Success
 */
nkn_trie_t nkn_trie_create(nkn_trie_copy_fn cfunc, nkn_trie_destruct_fn dfunc)
{
    return cp_trie_create_trie(
    	(COLLECTION_MODE_NOSYNC|COLLECTION_MODE_COPY|COLLECTION_MODE_DEEP), 
	cfunc, dfunc);
}

/*
 * nkn_trie_destroy() - Destroy Patricia Tree
 *	Returns: 0 => Success
 */
int nkn_trie_destroy(nkn_trie_t t)
{
    return cp_trie_destroy(t);
}

/*
 * nkn_trie_add() - Add node to Patricia Tree
 *	Returns: 0 => Success
 */
int nkn_trie_add(nkn_trie_t t, char *key, nkn_trie_node_t nd)
{
    return cp_trie_add(t, key, nd);
}

/*
 * nkn_trie_remove() - Remove node associated with key from the Patricia Tree
 *	Node removed via call to nkn_trie_destruct_fn
 *	Returns: >= 0 => Success, 0=Empty tree; 1=Deleted
 */
int nkn_trie_remove(nkn_trie_t t, char *key, nkn_trie_node_t *pnd)
{
    return cp_trie_remove(t, key, pnd);
}

/*
 * nkn_trie_prefix_match() - Find prefix matches in Patricia Tree
 *	Returns: 
 *		number of prefix mappings observed in while locating the
 *		longest prefix.
 *
 *		nkn_trie_node_t => node associated with longest prefix match
 */
int nkn_trie_prefix_match(nkn_trie_t t, char *key, nkn_trie_node_t *pnd)
{
    return cp_trie_prefix_match(t, key, pnd);
}

/*
 * nkn_trie_exact_match() - Find unique match
 *	Returns: !=0 => Associated node
 */
nkn_trie_node_t nkn_trie_exact_match(nkn_trie_t t, char *key)
{
    return cp_trie_exact_match(t, key);
}

/*
 * End of nkn_trie.c
 */
