/*
 *******************************************************************************
 * nkn_trie.h -- Patricia Tree interface
 *******************************************************************************
 */

#ifndef _NKN_TRIE_H
#define _NKN_TRIE_H

typedef void * nkn_trie_t;
typedef void * nkn_trie_node_t;

typedef void *(*nkn_trie_copy_fn)(nkn_trie_node_t nd);
typedef void (*nkn_trie_destruct_fn)(nkn_trie_node_t nd);

/*
 * nkn_trie_create() - Create Patricia Tree
 *	Returns: != 0 => Success
 */
nkn_trie_t nkn_trie_create(nkn_trie_copy_fn cnf, nkn_trie_destruct_fn df);

/*
 * nkn_trie_destroy() - Destroy Patricia Tree
 *	Returns: 0 => Success
 */
int nkn_trie_destroy(nkn_trie_t t);

/*
 * nkn_trie_add() - Add node to Patricia Tree
 *	Returns: 0 => Success
 */
int nkn_trie_add(nkn_trie_t t, char *key, nkn_trie_node_t nd);

/*
 * nkn_trie_remove() - Remove node associated with key from the Patricia Tree
 *	Node removed via call *nkn_trie_destruct_fn
 *	Returns: >= 0 => Success, 0=Empty tree; 1=Deleted
 */
int nkn_trie_remove(nkn_trie_t t, char *key, nkn_trie_node_t *pnd);

/*
 * nkn_trie_prefix_match() - Find prefix matches in Patricia Tree
 *	Returns: 
 *		number of prefix mappings observed in while locating the
 *		longest prefix.
 *
 *		nkn_trie_node_t => node associated with longest prefix match
 */
int nkn_trie_prefix_match(nkn_trie_t t, char *key, nkn_trie_node_t *pnd);

/*
 * nkn_trie_exact_match() - Find unique match
 *	Returns: !=0 => Associated node
 */
nkn_trie_node_t nkn_trie_exact_match(nkn_trie_t t, char *key);

#endif  /* _NKN_TRIE_H */

/*
 * End of nkn_trie.h
 */
