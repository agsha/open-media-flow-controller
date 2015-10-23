/*
 *
 * ltree.h
 *
 *
 *
 */

#ifndef __LTREE_H_
#define __LTREE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "tree.h"

/* ------------------------------------------------------------------------- */
/** \file ltree.h Layered tree data structure
 *
 * This API implements the "ltree" data structure, which is a layered
 * tree.  This is an abstraction implemented on top of the base tree
 * data structure.  Each layer is a tree, and the layers are arranged
 * in a stack.  Upper layers reflect diffs against the lower layers.
 * This allows easier tracking of changes to the tree, and makes it
 * possible to revert changes without having to make a backup copy of
 * the entire tree ahead of time.
 *
 * This API does not wrap aspects of the tree API that do not need
 * wrapping.  e.g. it still takes a tree_options for initialization,
 * and returns tree_node pointers for lookups.
 *
 * \ingroup lc_ds
 */

typedef struct ltree ltree;

typedef struct ltree_cont_cookie ltree_cont_cookie;

TYPED_ARRAY_HEADER_NEW_NONE(ltree_cont_cookie, struct ltree_cont_cookie *);

int ltree_cont_cookie_array_new(ltree_cont_cookie_array **ret_arr);

/**
 * Create a new ltree data structure.  The ltree starts out
 * with only a single layer.
 */
int ltree_new(ltree **ret_ltree, const tree_options *options);

/**
 * Free all of the memory associated with an ltree.
 */
int ltree_free(ltree **inout_ltree);

/**
 * Duplicate an ltree.
 */
int ltree_duplicate(const ltree *src, ltree **ret_dest);

/**
 * Add a new layer to the specified ltree.  Layers are arranged as a
 * stack, so this is like a "push" operation: the new layer is pushed
 * onto the stack, and automatically becomes the new active layer.
 * All modifications to the tree are made to the active layer (unless
 * the caller explicitly requests otherwise), whereas queries take
 * into account all of the layers, giving the active layer precedence.
 */
int ltree_push_new_layer(ltree *ltr);

/**
 * Pop the active layer off the ltree and delete it.  All of the
 * changes that were in that layer are lost, so this is like reverting
 * the state of the tree to what it was before this layer was added
 * with ltree_push_new_layer().
 */
int ltree_pop_layer(ltree *ltr);

/**
 * Merge the contents of two or more layers in the ltree.  The
 * contents of (low_layer_idx + 1) through (high_layer_idx) are merged
 * into (low_layer_idx), in effect making the changes from the higher
 * layers permananent in the lowest layer named.  This will not affect
 * the outcome of any queries (other than which of our internal trees
 * the nodes found are part of).
 *
 * \param ltr The ltree whose layers to merge.
 *
 * \param high_layer_idx Index of highest layer to take part in the merge.
 * Generally this should be at least 1 greater than low_layer_idx to do
 * anything interesting.  If it matches low_layer_idx, it's a no-op, and
 * if it's less than low_layer_idx, it's an error.  Pass -1 to use the
 * highest layer in the ltree, whatever its index is.
 *
 * \param low_layer_idx Index of lowest layer to take part in the merge.
 * Layers are numbered starting from 0 for the base layer (always present),
 * and going upwards from there.
 */
int ltree_merge_layers(ltree *ltr, int32 high_layer_idx, uint32 low_layer_idx);

/**
 * Merge the topmost layer down into the one beneath it.  This is simply
 * a specialized convenience wrapper around ltree_merge_layers().
 */
int ltree_merge_top_layer(ltree *ltr);

/**
 * Retrieve the number of layers in the specified ltree.  Note that
 * there is no separate way to query which is the active layer,
 * because the top layer is always active (i.e. the one with an index
 * one less than the number of layers).
 */
int ltree_get_num_layers(const ltree *ltr, int32 *ret_num_layers);

/**
 * Get a pointer to the tree for one of the layers in an ltree.  This
 * is a bit behind the abstraction, and there are not many legitimate
 * things a caller can do with this directly, though it could be
 * helpful for debugging or testing.
 *
 * \param ltr The ltree whose layer to fetch.
 *
 * \param layer_idx The index of the layer to fetch.  The base layer
 * is index 0, and an ltree always has one of those.  Each successive
 * call to ltree_push_new_layer() creates a new layer of one index
 * higher.
 *
 * \retval ret_layer The requested layer.  Do not modify this directly.
 */
int ltree_get_layer(const ltree *ltr, uint32 layer_idx, 
                    const tree **ret_layer);

/**
 * Set a node in the specified layer of the tree.  This is analogous
 * to tree_set_node_value_by_path(), except that you cannot specify a
 * starting node: we always start from the root.
 *
 * \param ltr The ltree to modify.
 *
 * \param path Array of relative names of tree nodes, which together 
 * specify a full path to the node to be set.
 *
 * \param value Value to be set on the node.
 *
 * \param create_node If the node did not already exist, and this is false,
 * an error is returned.
 *
 * \param layer_idx Index of layer to treat as the top layer.  Pass -1
 * to use the uppermost layer, which will be the normal approach.
 * If you pass a nonnegative number, we will pretend that any layers 
 * with a higher index do not exist.
 *
 * \retval ret_node The tree node that was either created or modified by
 * this set operation.
 */
int ltree_set_node_value_by_path(ltree *ltr, const tstr_array *path, 
                                 void *value, tbool create_node, 
                                 int32 layer_idx, tree_node **ret_node);

/**
 * Same as ltree_set_node_value_by_path(), except takes the path to the
 * node to set in the form of a single string.
 */
int ltree_set_node_value_by_path_str(ltree *ltr, const char *path_str, 
                                     void *value, tbool create_node, 
                                     int32 layer_idx, tree_node **ret_node);

/**
 * Delete a node from the ltree.  This is analogous to
 * tree_delete_node_by_path(), except that you cannot specify a
 * starting node: we always start from the root.
 *
 * \param ltr The ltree to modify.
 *
 * \param path Array of relative names of tree nodes, which together
 * specify a full path to the node to be deleted.  This must have at
 * least one name component in it: we do not support deleting the root
 * node of an ltree.
 *
 * \param delete_children If false, and node has children, the operation
 * will fail, and lc_err_has_children is returned.  It is not possible to
 * delete a node without deleting its children.
 *
 * \param layer_idx Index of layer to treat as the top layer, or -1 to
 * use the uppermost (normal).  If >= 0, we ignore any higher-numbered
 * layers.
 */
int ltree_delete_node_by_path(ltree *ltr, const tstr_array *path, 
                              tbool delete_children, int32 layer_idx);

/**
 * Same as ltree_delete_node_by_path(), except takes the path to the
 * node to set in the form of a single string.  The path must be
 * non-NULL, must be absolute, and must have at least one name
 * component in it: we do not support deleting the root node of an
 * ltree.
 */
int ltree_delete_node_by_path_str(ltree *ltr, const char *path_str, 
                                  tbool delete_children, int32 layer_idx);

/**
 * Look up a node in the ltree, given a full path to the desired node.
 * This is analogous to tree_get_node_by_path().
 *
 * \param ltr The ltree to search in.
 *
 * \param path The full path to the node, with one name component per 
 * entry in the array.
 *
 * \param return_partial If we cannot find the node requested, should 
 * we return the deepest matching node we could find on the way?
 * Even if no nodes match even the first component of the path, we'd
 * still return the root node of the tree.  If false, and we cannot find
 * the exact node requested, NULL is returned for the node.
 *
 * \param layer_idx Index of layer to treat as the top layer, or -1 to
 * use the uppermost (normal).  If >= 0, we ignore any higher-numbered
 * layers.
 *
 * \retval ret_found_node The node found.  If return_partial was false,
 * this node will have exactly the requested path, or NULL if it could
 * not be located.  Otherwise, it may have only a prefix of the requested
 * path.  Note that if we find the node but it is deleted, this counts
 * as not finding it, because keeping nodes around with the deleted flag
 * is an implementation detail, not the abstraction we are exposing.
 *
 * \retval ret_depth_found The depth of the node found, or -1 if we
 * are not returning a node.  The root node has depth 0, so this is 
 * effectively the number of components of the path that we were able
 * to match.  If return_partial was false, this will always be either 
 * -1 or match the length of the 'path' parameter.  But if return_partial
 * is true, this reflects how far down we were able to match, so may be
 * <= the length of 'path'.
 *
 * \retval ret_layer_idx_found The layer in which the node was found, or 
 * -1 if we are not returning a node.
 *
 * \return If return_partial was false, and we could not find an exact
 * match, returns lc_err_not_found.  Otherwise, returns 0 for success,
 * and nonzero for failure.
 */
int ltree_get_node_by_path(const ltree *ltr, const tstr_array *path, 
                           tbool return_partial, int32 layer_idx, 
                           tree_node **ret_found_node, int32 *ret_depth_found,
                           int32 *ret_layer_idx_found);

/**
 * Same as ltree_get_node_by_path(), except takes the path to the
 * node to set in the form of a single string.
 */
int ltree_get_node_by_path_str(const ltree *ltr, const char *path_str,
                               tbool return_partial, int32 layer_idx, 
                               tree_node **ret_found_node, 
                               int32 *ret_depth_found, int32 *ret_layer_found);

/**
 * Option flags for ltree_get_node_by_path_cont().
 */
typedef enum {
    
    lgnf_none = 0,

    /** 
     * Specify that we should verify that the continuation cookie is valid
     * for this path.  This means checking that the parent node stored in
     * the cookie matches the first n-1 parts (i.e. all but the last part)
     * of the path.  This is expensive and will sacrifice many of the
     * performance gains made by the cookie, so it is not advisable for
     * real code.  Real code should just be careful to only ever use a
     * valid cookie.  This is more for debugging, to help assure yourself
     * that the code is correct.
     *
     * XXXX/EMT: NOT YET IMPLEMENTED.
     */
    lgnf_sanity_check = 1 << 0,

    /*
     * XXXX/EMT: could also have an option for how aggressively we try to
     * avoid the binary search by relying on the index at which the last
     * child was found.  There are at least two possible behaviors:
     *
     *   1. (a) don't even try if the first child we look up is not at
     *      index 0; and (b) give up and fall back on binary search if,
     *      when using the continuation cookie, we fail to find the child
     *      being sought at the next index.  This was our original behavior,
     *      but that was problematic because of the possibility of dummy
     *      nodes getting in the way.  e.g. if you have:
     *        - /a/b/c
     *        - /a/d, /a/e, /a/f, /a/g, etc.
     *      then this approach will not get to use a cookie, because we
     *      are never asked to look up /a/b.  We start with /a/d, which
     *      is not the first child.  Similarly, if one of these occurred
     *      in the middle, that would throw us off as well.
     *
     *   2. Have faith that the nodes are all in order, but there may be 
     *      gaps.  So always start off doing the index thing; and if we 
     *      fail to find what we're looking for at the next index, linear
     *      search until we find it.  Only if we don't find it by the end
     *      of the array (so it's either before us, or doesn't exist),
     *      fall back on a binary search, and call it quits for this
     *      optimization for the lifetime of the cookie.  This is our
     *      behavior now.
     *
     * NOTE: this still favors flatter node designs.  One might think
     * that naming a node "/a/b/c" vs. "a/b_c" is a matter of whimsy
     * in some cases, but it will affect performance!
     */

} ltree_get_node_flags;

/** Bit field of ::ltree_get_node_flags ORed together */
typedef uint32 ltree_get_node_flags_bf;

/**
 * Like ltree_get_node_by_path(), except with support for "continuation
 * cookies" to enhance performance while querying in succession multiple
 * nodes that are known to be peers.
 *
 * There are two ways in which this can be called:
 *
 *   1. To get a new series of queries started, pass NULL for cont_cookie
 *      and pass all other arguments as normal.  Along with the answer to
 *      the first lookup, a continuation cookie will be returned.
 *
 *   2. To continue a series begun as per #1, pass back the continuation
 *      cookie returned to you earlier.  Pass the path as usual, except
 *      note that by using a continuation cookie you are asserting that
 *      this path is identical to the path from which the cookie was
 *      created, except for the last component having been replaced.
 *      Pass nothing (false and -1) for return_partial and layer_idx;
 *      the continuation cookie will cause the same values as before 
 *      to be reused.
 *
 * NOTE: it is possible to use this API incorrectly, by passing a
 * continuation cookie along with a path which is not valid for that
 * cookie.  i.e. it is not identical to the path for which the cookie was
 * constructed, except for the last part.  If this happens, it will NOT be
 * detected (unless the 'lgo_sanity_check' flag is set), but it can cause
 * incorrect behavior.
 *
 * NOTE: it is also important that the tree not change between when a
 * continuation cookie is created, and when it is used.  We hold a pointer
 * to the parent node of the set of peers being iterated over, and some
 * kinds of changes to the tree could make that a dangling pointer.
 *
 * min_peers_for_cont: the minimum number of peers at a given level
 * to require before creating a continuation cookie.  Since it imposes
 * some overhead to create a continuation cookie, and you only recoup
 * these losses by using the cookie to speed up future lookups, it will
 * definitely not make sense to use a cookie if there is only one child.
 * The minimum sensible number for this is at least 2; raising it further
 * is a matter of tuning.
 *
 * To skip using continuation cookies altogether, simply pass NULL for
 * ret_cont_cookie.
 *
 * XXX/EMT: continuation cookies are currently only generated for
 * single-layer ltrees, where layer_idx is -1 and return_partial is false.
 * This API can be safely used on an ltree with multiple layers, but it 
 * will never return a continuation cookie, so no performance gain will 
 * be observed.
 */
int ltree_get_node_by_path_cont(const ltree *ltr,
                                ltree_cont_cookie *cont_cookie,
                                uint32 min_peers_for_cont,
                                ltree_get_node_flags_bf flags,
                                const tstr_array *path, 
                                tbool return_partial, int32 layer_idx, 
                                tree_node **ret_found_node,
                                int32 *ret_depth_found,
                                int32 *ret_layer_idx_found,
                                ltree_cont_cookie **ret_cont_cookie);

/**
 * Free a continuation cookie returned by a previous call to
 * ltree_get_node_by_path_cont().  This will NULL out the caller's
 * pointer too.
 */
int ltree_cont_cookie_free(ltree_cont_cookie **inout_cont_cookie);

/**
 * Calculate the number of children a specified node in the tree has.
 * This is analogous to tree_get_node_num_children().
 *
 * \param ltr The ltree to query.
 *
 * \param node Node whose children to count.  Note that it does not 
 * matter which layer this node is in.  It is effectively only used
 * for its path, and the answer will be the total number of children
 * considering all appropriate layers.
 *
 * \param layer_idx Index of layer to treat as the top layer, or -1 to
 * use the uppermost (normal).  If >= 0, we ignore any higher-numbered
 * layers.  NOTE: if this is >= 0, the node specified must not be in
 * one of the layers we are ignoring.
 *
 * \retval ret_num_children Number of children the specified node has.
 */
int ltree_node_get_num_children(const ltree *ltr, const tree_node *node,
                                int32 layer_idx, uint32 *ret_num_children);

/**
 * Get the value of a specified tree node.  This is analogous to
 * tree_node_get_value(), which you could almost pass directly, except
 * that it takes a pointer to a tree (only for consistency with the
 * rest of the API).
 */
int ltree_node_get_value(const ltree *ltr, const tree_node *node, 
                         void **ret_value);

/**
 * Function to be called back on a set of nodes in an ltree, according
 * to the parameters specified in a call to ltree_foreach().
 *
 * Note that this function may not modify the tree, or the node it is
 * given.  Modifying the tree might mess up our traversal.  And
 * modifying the node might not have the intended effect, since if the
 * node you are passed is not in the upper layer, you'd be modifying a
 * node underneath the active layer.  That would not produce any
 * immediately wrong results, but if the active layer was later
 * deleted without being merged back, this change would not be undone.
 */
typedef int (*ltree_foreach_func)(const ltree *ltr, const tree *layer, 
                                  int32 layer_idx, const tree_node *node, 
                                  const char *name, const tstr_array *abspath,
                                  void *value, void *data);

/**
 * Call a specified function for each of a set of tree nodes.  This is
 * analogous to tree_foreach().  The function will be called in what
 * would be a pre-order traversal of a hypothetical tree that had all
 * of the layers merged together.  So the nodes you are called back
 * with may jump back and forth between layers, depending on where the
 * most up-to-date version of each node lives.
 *
 * \param ltr The ltree to iterate over.
 *
 * \param base_node The node to start iteration from.  Note that it does
 * not matter which layer this node is in; it is used solely to specify 
 * a path to iterate under.
 *
 * \param options Tree iterate options, which mainly control which nodes
 * are included in the iteration, and what parameters are passed to the
 * callback function.
 *
 * \param layer_idx Index of layer to treat as the top layer, or -1 to
 * use the uppermost (normal).  If >= 0, we ignore any higher-numbered
 * layers.
 *
 * \param foreach_func Function to call for each selected node.
 *
 * \param data Data to pass back to foreach_func on each call.
 */
int ltree_foreach(ltree *ltr, tree_node *base_node,
                  tree_foreach_options_bf options, int32 layer_idx,
                  ltree_foreach_func foreach_func, void *data);

/**
 * Function to be called back for every different node from
 * ltree_foreach_diff_...().  node1 et al. refer to
 * the old version (the tree represented by the lower layer and
 * below), while node2 et al. refer to the new version (the tree
 * represented by the upper layer and below).
 *
 * If a node exists in one tree but not the other, either
 * {node1, value1} or {node2, value2} may be NULLs.  The name
 * and abspath will always be provided for both.
 *
 * Note that because ltree_foreach_diff_adj_layers_path() takes only a
 * single root node for both subtrees, name1 and abspath1 will for now
 * always match name2 and abspath2.  These parameters are included
 * separately to possibly later allow different starting points.
 *
 * Node2_flags contains the flags found on the node in the upper
 * layer, if any.  If there is no directly corresponding node in the
 * upper layer (e.g. the node was deleted by a tnf_deleted flag on a 
 * parent), this will be 0.
 *
 * The 'maybe_duplicate' parameter tells you if you might be getting
 * called more than once for a given node name.  Usually you will not,
 * but you might in the case where (a) a node is deleted, and then 
 * (b) the node is recreated along with at least one of its previous
 * children.  This parameter will be 'false' for all callbacks in a
 * given iteration until the first one in which we know there might be
 * a duplicate.  From that callback forward, we will always pass 'true'
 * for the remainder of the iteration.  Callbacks may assume that they
 * will not be called for any node for the second time without it 
 * being accompanied by the 'maybe_duplicate' flag.
 */
typedef int (*ltree_foreach_diff_func)(ltree *ltr,
                                       const tree_node *node1,
                                       const char *name1, 
                                       const tstr_array *abspath1,
                                       void *value1,
                                       const tree_node *node2, 
                                       const char *name2,
                                       const tstr_array *abspath2,
                                       void *value2,
                                       tree_node_flags_bf node2_flags,
                                       tbool maybe_duplicate,
                                       void *cb_arg);

/**
 * Call a specified function for every node that might be different in
 * the trees represented by two adjacent layers in an ltree.  This is
 * similar to tree_foreach_diff(), except instead of working on two
 * independent trees, it works on the virtual trees represented by an
 * ltree at two different (and adjacent) layer points.  For example,
 * you could diff the tree represented by layers 2 and below against
 * the tree represented by layers 3 and below.  Essentially, this
 * means you are seeing the diffs represented by layer 3.
 *
 * For every node that exists in either virtual tree and might be
 * different, your callback will be called, and will be passed
 * whichever node(s) exist in each tree with that name.  If a node
 * exists in both trees, your function is only called once.
 *
 * By "might be", we mean only that we are not comparing the values to
 * see if they are actually different.  We call back for every node
 * that appears in the upper layer, and every node in the lower layer
 * that was underneath a node deleted by the upper layer.  Some of
 * these nodes might still happen to have the same values; that is up
 * to the caller to handle as desired.
 *
 * \param ltr The ltree to iterate over.
 *
 * \param path The path to the root node to start iterating from.  
 * The iteration must start from the same path in both trees.
 * Pass NULL to start from the root node.
 *
 * \param options Option flags for tree iteration.  As with 
 * tree_foreach_diff(), we accept a tree_foreach_options_bf here to allow for
 * future expansion, but currently only one combination of options is
 * supported: (tfo_subtree | tfo_include_self).  Any other sets of flags
 * will fail the entire call.
 *
 * \param layer_idx The layer index of the HIGHER layer to participate
 * in the diff.  Since you are seeing the differences of exactly one
 * layer, this will be diff'd against the tree represented by layer_idx-1
 * and lower.  Pass -1 to mean the highest layer (to be diff'd against 
 * the second-highest layer).
 *
 * \param cb_func The function to call back for each node.
 *
 * \param cb_arg Data to pass back to your function.
 */
int ltree_foreach_diff_adj_layers_path(ltree *ltr, const tstr_array *path,
                                       tree_foreach_options_bf options,
                                       int32 layer_idx, 
                                       ltree_foreach_diff_func cb_func,
                                       void *cb_arg);

/**
 * Dump a representation of the ltree to stdout, for debugging.
 */
int ltree_print(const ltree *ltr);

#ifdef __cplusplus
}
#endif

#endif /* __LTREE_H_ */
