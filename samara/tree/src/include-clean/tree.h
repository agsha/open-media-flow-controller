/*
 *
 * tree.h
 *
 *
 *
 */

#ifndef __TREE_H_
#define __TREE_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include "common.h"
#include "tstr_array.h"

/* ------------------------------------------------------------------------- */
/** \file tree.h N-ary tree data structure.
 * \ingroup lc_ds
 *
 * Each node has a name, which is a char *, and a value, which is a
 * void *.  The names of all sibling nodes must be unique; i.e. no two
 * nodes with the same parent may have equal names.  The name is not
 * prefixed with the names of all of the node's ancestors; it is just
 * the portion of the node's identity that uniquifies it from its
 * siblings.  A name may not be NULL, nor may it be the empty string.
 * No assumptions are made by the library about the encoding of the
 * name besides that it is NULL-terminated; it may be ASCII, UTF-8, etc.
 *
 * An "absolute path" is the set of names that uniquifies a node in
 * the tree; it is the names of all of the node's ancestors back to
 * but not including the root of the tree.  A "relative path" is the
 * set of names that uniquifies a node in a subtree; it is the names
 * of all of the node's ancestors back to but not including the root
 * of the subtree in question.
 *
 * A path, whether relative or absolute, may be specified either as a
 * single string or as a tstr_array.  The tree data structure can only
 * directly operate on a tstr_array, where each string within is a
 * single name in the path.  For convenience, APIs are provided for
 * working with single-string paths, whose format is entirely
 * determined by the caller.  The caller provides callbacks that can
 * convert from a tstr_array to a single-string format and vice-versa.
 * Generally the caller will use the tstring library to tokenize and
 * join strings.
 *
 * The root node is special in that it cannot be deleted, and its name
 * cannot be set.  If queried, its name will appear to be the empty
 * string, and its path will contain no strings.  The root node can be
 * assigned a value.
 *
 * Except where otherwise noted, all functions return an int error
 * code among those defined in common.h: zero for success, and nonzero
 * for other failures.
 */

/** Opaque tree data type */
typedef struct tree tree;

/** Opaque tree node data type */
typedef struct tree_node tree_node;

/* ------------------------------------------------------------------------- */
/** An opaque data structure that helps optimize certain tree
 * operations which would otherwise be performed less efficiently.
 * A context goes with a particular tree node; thus the operations
 * they can optimize are those to which an existing node of the tree
 * is an argument.  A context pointer is never given to the caller
 * directly; the caller is given a handle, which identifies a tree
 * context the tree data structure keeps track of.  This is done to
 * protect against programming errors where the caller thinks a tree
 * context is still valid but in fact it has been deleted; with a
 * handle, we may simply return an error, rather than causing more
 * serious problems by trying to access memory that has been freed.
 *
 * A context can be provided to API functions which take a tree node
 * as an argument, except those which simply query the tree node
 * directly and do not affect or query any other part of the tree.
 * A context can be returned from any API function which returns a
 * tree node, whether or not it takes a starting tree node as an
 * argument.  API functions of either or both types (i.e. those which
 * accept and/or return a pointer to a node) take a pointer to a
 * tree_context_handle.  There are three possibilities for what to
 * pass for this parameter:
 *
 * \li NULL.  Do not use contexts.  The request will be fulfilled 
 *     correctly, but may be slower than if contexts were used,
 *     depending on circumstances.
 *
 * \li A pointer to a tree_context_handle which is set to
 *     tree_context_none.  The caller is not providing a context,
 *     so perform this operation without the benefit of a context;
 *     but if the API function also returns another node (i.e. it is
 *     a function to find one node given another), create a context
 *     for use in subsequent calls and return it.
 *
 * \li A pointer to a tree_context_handle containing a handle that was
 *     returned by a previous call.  Caller is providing a context, so
 *     use it to perform this operation; and if the API function also
 *     returns another node, update it to reflect the new result node.
 *
 * In either of the latter two cases, the context returned will
 * pertain to the node returned, and can only be passed back on
 * requests which are relative to that node.  Passing a context with a
 * request relative to a different node will result in an error.
 *
 * Note: contexts may be affected by changes to the tree.  The tree
 * data structure keeps track of all of the contexts it has handed
 * out, and automatically updates them to reflect changes made to the
 * tree.  If the node the context pertains to is deleted, the context
 * will be invalidated, and using it again will result in an error.
 *
 * It is the caller's responsibility to free contexts using
 * tree_context_free().  Not freeing old contexts not only leaks
 * memory but also exacts a performance penalty during tree
 * modifications, as the tree attempts to keep all of the active
 * contexts up to date.  Note that contexts are freed automatically
 * when the node they pertain to is deleted.
 */
typedef int tree_context_handle;

/** An invalid, unassigned tree handle */
#define tree_context_none ((tree_context_handle)0)

/* ----------------------------------------------------------------------------
 * The following six function type declarations are for functions that
 * can be specified as properties of a tree when it is created, using
 * a tree_options data structure.  They allow the caller to override
 * the default behavior of the tree library in certain contexts.
 */

/* ------------------------------------------------------------------------- */
/** Function to compare two tree nodes.  This determines the order in
 * which siblings are returned by tree_node_get_next() and
 * tree_node_get_prev(), as well as the order in which nodes are
 * passed to foreach functions.  See the comment for
 * tree_node_get_next() for details.
 *
 * \param name1 Name of first node to compare
 * \param value1 Value of first node to compare
 * \param name2 Name of second node to compare
 * \param value2 Value of second node to compare
 *
 * \return A negative number, zero, or a positive number if the
 * node identified by name1 and value1 is less than, equal to, or
 * greater than the node identified by name2 and value2, respectively.
 *
 * The default comparison function just does a standard ASCII string
 * compare on the names, and ignores the values.
 */
typedef int (*tree_node_compare_func)(const char *name1, const void *value1,
                                      const char *name2, const void *value2);

/* ------------------------------------------------------------------------- */
/** Function to duplicate a tree value.  Called by tree_duplicate() on
 * all non-NULL node values in the source tree.  It is legal to pass
 * NULL for the duplicate function, but then any calls to
 * tree_duplicate() will fail.
 *
 * Note that this function is only used on the node value.  The name is
 * always duplicated in the same manner by the library, using strdup().
 *
 * \param src_value The value to be duplicated.
 *
 * \retval ret_dest_value The newly-created duplicate of the value.
 *
 * The default duplicate function just returns the pointer given.
 */
typedef int (*tree_value_dup_func)(const void *src_value,
                                   void **ret_dest_value);

/* ------------------------------------------------------------------------- */
/** Function to be called on the value when a new node is added to the
 * tree.  Will be called when a node with a NULL value is being set,
 * so should be able to handle this gracefully.
 *
 * Note that this function is only used on the node value.  The name is
 * duplicated using strdup() when a node is added.
 *
 * \param src_value The value provided by the caller to set in the tree.
 *
 * \retval ret_dest_value The value that should be added to the tree.
 * The two most common choices are to use the duplicate function as
 * the set function, so the tree will make a copy of any values as
 * they are added; or the default, so the tree will take ownership of
 * any values added.  More complex examples may involve incrementing a
 * reference count, etc.
 *
 * The default set function just returns the pointer given. 
 */
typedef int (*tree_value_set_func)(const void *src_value,
                                   void **ret_dest_value);

/* ------------------------------------------------------------------------- */
/** Function to free a tree value.  This is called when a node is deleted
 * from the tree.  It also will be called if a value is overwritten by
 * another one being added to the tree under the same name.
 *
 * Note that this function is only used on the node value.  The name is
 * always freed by the library using free().
 *
 * \param value The value to be freed.
 *
 * The default free function does not do anything.
 */
typedef void (*tree_value_free_func)(void *value);

/* ------------------------------------------------------------------------- */
/** Function to convert a path as a tstr_array to single-string
 * representation.  This is called when the caller requests a
 * single-string representation of the path to a node, such as with
 * tree_node_get_path().
 *
 * \param names The path to be escaped.
 *
 * \param abs_path Indicates whether the path represented by
 * the strings is absolute (relative to the root of the tree), or not
 * (relative to some other node).  This does need to be reflected in
 * the resulting path string, as paths converted in the other
 * direction need to set this boolean, which is used by the framework
 * to sanity-check requests (e.g. to catch cases like an absolute path
 * being given along with a pointer to a non-root node which it is to
 * be relative to).  A common approach is for an absolute path to be
 * prefixed with the delimiter used to separate names in a path, and
 * for the relative path to have no prefix.
 *
 * \retval ret_path The escaped string in a newly-allocated block of
 * memory.  The tree library takes ownership of this memory.
 *
 * There is no default escaping function.  If none is provided by the
 * caller, the functions that work with single string paths which
 * require it will return an error.
 */
typedef int (*tree_path_escape_func)(const tstr_array *names, tbool abs_path,
                                     char **ret_path);

/* ------------------------------------------------------------------------- */
/** Function to convert a path as a single string into a tstr_array.
 * This is called when the caller provides a single-string
 * representation of a path to an API function that requires a path,
 * such as tree_get_node_by_path().  It is the inverse of
 * tree_path_escape_func.
 *
 * \param path The path to be unescaped.
 *
 * \retval ret_names The unescaped path in tstr_array form.  The tree
 * library takes ownership of this data structure and will free it
 * when it is no longer needed.
 *
 * \retval ret_abs_path Indicates whether or not the path was
 * absolute.  The boolean is used by the framework to sanity-check
 * requests.
 * 
 * There is no default unescaping function.  If none is provided by
 * the caller, the functions that work with single string paths which
 * require it will return an error.
 */
typedef int (*tree_path_unescape_func)(const char *path,
                                       tstr_array **ret_names,
                                       tbool *ret_abs_path);

/* ------------------------------------------------------------------------- */
/** Specify behavior when the caller tries to insert a node with the
 * same name as one of its new siblings.  As explained above, two
 * sibling nodes cannot share the same name.  This also applies to a
 * node being renamed, or to the root of a tree which is being grafted
 * onto another tree.
 *
 * The default behavior, if not overridden in the options structure,
 * is tdp_overwrite_old.
 */
typedef enum {
    /**
     * No policy has been specified.
     */
    tdp_none = 0,

    /**
     * The node already in the tree should be deleted first, then the
     * new one is added; if the old one had children, they are
     * deleted too.
     */
    tdp_delete_old,

    /**
     * The new node should replace the old node, but that any
     * children of the old node should not be disturbed.
     */
    tdp_overwrite_old,
    
    /**
     * The requested operation should not be performed, the tree and
     * any other arguments provided should remain unmodified, and the
     * error code lc_err_exists should be returned.
     */
    tdp_fail_new,
    tdp_last
} tree_dup_policy;

/* ------------------------------------------------------------------------- */
/** Options for a newly-created tree.  These are passed in a structure,
 * rather than directly as parameters to tree_new(), to allow new
 * options to be added without a change to the API.
 */
typedef struct tree_options {
    /** Function to compare two elements */
    tree_node_compare_func   to_node_compare_func;

    /** Function to duplicate an element */
    tree_value_dup_func      to_value_dup_func;

    /** Function to prepare an element to be added to the tree */
    tree_value_set_func      to_value_set_func;

    /** Function to deallocate an element */
    tree_value_free_func     to_value_free_func;

    /** Function to convert array of node names into single string node path */
    tree_path_escape_func    to_path_escape_func;

    /** Function to convert single string node path into array of node names */
    tree_path_unescape_func  to_path_unescape_func;

    /** Policy on handling insertion of duplicate elements */
    tree_dup_policy          to_dup_policy;
} tree_options;

/* ========================================================================= */
/** @name Core API
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** Populate a provided options structure with the default values.
 * If you do not want only the default values, call this first and
 * then overwrite whichever values you want to change.
 *
 * \param ret_options Structure to populate with default options.
 */
int tree_options_get_defaults(tree_options *ret_options);

/* ------------------------------------------------------------------------- */
/** Create a new tree with the options provided.  The tree initially
 * has only a root node with a NULL value.
 *
 * \param options Tree behavior options.  If all of the default
 * options are acceptable, NULL may be passed.  Otherwise, a
 * fully-populated options structure must be passed.  DO NOT fill in
 * an empty options structure if you want to override all of the
 * defaults.  If more options are added, these will then be passed in
 * unset, potentially causing mysterious failures and defeating the
 * purpose of the option structure.  ALWAYS call
 * tree_options_get_defaults() and overwrite whatever you want to,
 * unless you want only the defaults.
 * \n\n
 * The compare and set functions are required; setting them to NULL
 * will result in the tree_new() call failing.  The duplicate, free,
 * escape, and unescape functions may be NULL.  If the duplicate
 * function is NULL, a call to tree_duplicate() will fail.  If the
 * free function is NULL, no action will be taken to free tree values
 * as they are deleted.  If the escape or unescape functions are NULL,
 * any API calls that require escaping or unescaping will fail.
 *
 * \retval ret_tr The newly-created tree.
 */
int tree_new(tree **ret_tr, const tree_options *options);

/* ------------------------------------------------------------------------- */
/** Free an entire tree.  Calls the free function on the values of each of
 * the tree nodes, then frees everything else associated with the tree.
 *
 * \param tr Tree to be freed.
 */
int tree_free(tree **tr);

/* ------------------------------------------------------------------------- */
/** Populate the provided tree_options data structure with the options
 * from the specified tree.
 */
int tree_get_options(const tree *tr, tree_options *ret_opts);

/* ------------------------------------------------------------------------- */
/** Set the specified tree's options to match those in the provided
 * structure.
 *
 * (NOT YET IMPLEMENTED)
 */
#if 0
int tree_set_options(tree *tr, tree_options *opts);
#endif

/* ------------------------------------------------------------------------- */
/** Reset the tree back to the same state it was in when it was first
 * created.  This deletes all nodes except for the root node, freeing
 * values with the provided free function, and also clears the value
 * of the root node.  Also invalidates and deletes all tree contexts.
 *
 * \param tr Tree to be emptied.
 */
int tree_empty(tree *tr);

#if 0
/* ------------------------------------------------------------------------- */
/** Compacts the tree's data structures to make it take up as little
 * memory as possible.  Due to the way it progressively grows, under
 * some circumstances a tree may take up more memory than is necessary
 * to hold all of its data.  If no more values are going to be added
 * to the tree for a long time, compacting it will help conserve
 * memory.
 *
 * \param tr Tree to be compacted.
 *
 * NOT YET IMPLEMENTED.
 */
int tree_compact(tree *tr);
#endif

/* ------------------------------------------------------------------------- */
/** Make an identical copy of the source tree and return it in the
 * destination tree.  The duplicate function is used to duplicate each
 * of the node values.
 *
 * \param src Tree to be duplicated.
 *
 * \retval ret_dest A newly-created duplicate copy of the tree provided.
 */
int tree_duplicate(const tree *src, tree **ret_dest);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the root node of the tree.  This is also the
 * first node in the lexical ordering of the tree, regardless of what
 * comparison function was provided.
 */
int tree_get_root_node(const tree *tr, tree_node **ret_root_node,
                       tree_context_handle *ret_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the lexically last node in the tree.
 */
int tree_get_last_node(const tree *tr, tree_node **ret_last_node,
                       tree_context_handle *ret_context);

/* ------------------------------------------------------------------------- */
/* NOTE: for all of the functions below which search for nodes, if no
 * matching node is found: NULL is returned for the node pointer; the
 * context passed in, if any, is unchanged (i.e. it still pertains to
 * the node originally passed in); and lc_err_not_found is returned.
 */

/* ------------------------------------------------------------------------- */
/** Return a pointer to the lexically next node in the tree.
 * The lexical ordering of the tree nodes is a preorder traversal,
 * with siblings arranged in ascending order according to the tree's
 * compare function.
 */
int tree_node_get_next(const tree *tr, const tree_node *node,
                       tree_node **ret_next_node,
                       tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the lexically previous node in the tree.  This
 * returns nodes in exactly the opposite order as
 * tree_node_get_next().
 */
int tree_node_get_prev(const tree *tr, const tree_node *node,
                       tree_node **ret_prev_node,
                       tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the next sibling of the specified node.
 * The siblings are returned in ascending order sorted according to
 * the compare function provided when the tree was created.
 */
int tree_node_get_next_sibling(const tree *tr, const tree_node *node,
                               tree_node **ret_next_sibling_node,
                               tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the previous sibling of the specified node.
 * This returnes nodes in exactly the opposite order as
 * tree_node_get_next_sibling().
 */
int tree_node_get_prev_sibling(const tree *tr, const tree_node *node,
                               tree_node **ret_prev_sibling_node,
                               tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Find a child of the specified node with the specified name.  If the
 * node is a leaf, or has no matching children, NULL is returned.
 */
int tree_node_get_child_by_name(const tree *tr, const tree_node *parent_node,
                                const char *name, tree_node **ret_child_node,
                                tree_context_handle *inout_context);

int tree_node_get_child_by_name_ts(const tree *tr,
                                   const tree_node *parent_node,
                                   const tstring *name_ts,
                                   tree_node **ret_child_node,
                                   tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the first child of the specified node.  If the node
 * is a leaf, NULL is returned.
 */
int tree_node_get_first_child(const tree *tr, const tree_node *parent_node,
                              tree_node **ret_child_node,
                              tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the last child of the specified node.  If the node
 * is a leaf, NULL is returned.
 */
int tree_node_get_last_child(const tree *tr, const tree_node *parent_node,
                             tree_node **ret_child_node,
                             tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the nth child of the specified node.  If the
 * node does not have at least n + 1 children, lc_err_not_found
 * is returned, and the returned child node is set to NULL.
 */
int tree_node_get_nth_child(const tree *tr, const tree_node *parent_node,
                            uint32 n, tree_node **ret_child_node,
                            tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the parent of the specified node.  If the node is
 * the root node, it has no parent so NULL is returned.
 */
int tree_node_get_parent(const tree *tr, const tree_node *node,
                         tree_node **ret_parent_node,
                         tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return the name of the specified node.  The caller does not own the
 * memory passed back and should not free or modify it.
 */
int tree_node_get_name(const tree *tr, const tree_node *node,
                       const char **ret_name);

/* ------------------------------------------------------------------------- */
/** Return the value of the specified node.  The caller does not own
 * the memory passed back and should not free it, though its contents
 * may be modified as desired.
 */
int tree_node_get_value(const tree *tr, const tree_node *node, 
                        void **ret_value);

/* ------------------------------------------------------------------------- */
/** Return the value of the specified node, and take over ownership of it.
 * The tree node will be set to have a NULL value.
 */
int tree_node_get_value_takeover(tree *tr, tree_node *node,
                                 void **ret_value);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the tree that the specified node is part of.
 * Note that this does not copy the tree, so even though the pointer
 * is not const, it does not represent a passing of ownership to the
 * caller.
 */
int tree_node_get_tree(const tree_node *node, tree **ret_tree);

/* ------------------------------------------------------------------------- */
/** Return the number of immediate children the node has.
 */
int tree_node_get_num_children(const tree *tr, const tree_node *node,
                               uint32 *ret_num_children);

/* ------------------------------------------------------------------------- */
/** Explicitly request a context for a specified node.  The context
 * returned can be used to accelerate future queries based off of this
 * node.  Note that there is no overall speed advantage to doing this
 * first, rather than just passing in a pointer to an empty context
 * handle for the query and letting the context be determined at that
 * time.  This function is provided for convenience to reduce special
 * cases in the caller's code if the presence of a context is always
 * assumed.
 */
int tree_node_get_context(const tree *tr, const tree_node *node,
                          tree_context_handle *ret_context);

/* ------------------------------------------------------------------------- */
/** Create a node with the specified name and value and add it as a
 * child of the specified node.  The name is copied using strdup() (or
 * ts_get_str_dup() if appropriate), and the result of calling the set
 * function on the value is added to the node in lieu of the value
 * itself.  If ret_new_node is non-NULL, the newly-created child node
 * is returned in it.
 */
int tree_node_insert_child(tree *tr, tree_node *parent_node, const char *name,
                           void *value, tree_node **ret_new_node);

int tree_node_insert_child_ts(tree *tr, tree_node *parent_node,
                              const tstring *name_ts,
                              void *value, tree_node **ret_new_node);

/* ------------------------------------------------------------------------- */
/** Change the name of the specified node.  The name provided is copied
 * using strdup().
 */
int tree_node_set_name(tree *tr, tree_node *node, const char *name);

/* ------------------------------------------------------------------------- */
/** Change the value of the specified node.  The result of calling the
 * set function on the value is added to the node in lieu of the value
 * itself.
 */
int tree_node_set_value(tree *tr, tree_node *node, void *value);

/* ------------------------------------------------------------------------- */
/** Delete the specified node, calling the tree's value free function
 * on the value.  The context passed in is invalidated.  If
 * delete_children is false and the node has children,
 * lc_err_has_children is returned.  Otherwise, any children the node
 * has are deleted as well.
 *
 * Deletion of the root node is a special case.  The root node itself
 * cannot be deleted, but if you call tree_node_delete() with the root
 * node, with delete_children set to true, the result will be
 * equivalent to calling tree_empty().
 */
int tree_node_delete(tree *tr, tree_node **node, tbool delete_children,
                     tree_context_handle *context);

/* ------------------------------------------------------------------------- */
/** Remove the specified node from the tree, but instead of freeing its
 * value, return it to the caller.  If delete_children is false and
 * the node has children, lc_err_has_children is returned.  Otherwise,
 * any children the node has are deleted.  The node itself is deleted
 * and the pointer set to NULL.
 *
 * It is illegal to remove the root node of the tree, and attempting
 * this will result in an error.
 */
int tree_node_remove(tree *tr, tree_node **node, void **ret_value,
                     tbool delete_children, tree_context_handle *context);

/* ------------------------------------------------------------------------- */
/** Within the specified tree, move the specified node from its present
 * location to being a child of the specified new parent node.
 * Any children of the node being moved are preserved, as are any
 * existing children of the new parent (unless one of them has the
 * same name as the node being grafted, in which case the duplicate
 * policy is followed).
 *
 * In the case where we are overwriting an existing node which had the
 * same name as us, the pointer to the node being moved may be
 * changed.
 */
int tree_node_graft(tree *tr, tree_node **inout_node, tree_node *new_parent);

/* ------------------------------------------------------------------------- */
/** First, graft all of the specified node's children onto it's parent
 * (i.e. as peers of the specified node).  Then, remove the specified
 * node and return its value as with tree_node_remove().  If ret_value
 * is NULL, the value is deleted instead.  inout_node is set to NULL 
 * after the operation is completed.
 */
int tree_node_remove_save_children(tree *tr, tree_node **inout_node,
                                   void **ret_value);

/* ------------------------------------------------------------------------- */
/** Graft the 'subsumed' tree onto the 'main' tree.  The subsumed
 * tree's root will be added as a child of the specified node, and
 * given 'root_name' as a name.  All of the options registered on the
 * subsumed tree are lost; the nodes from that tree are now subject to
 * the options registered on the main tree.
 *
 * All of the nodes from the subsumed tree are saved in this manner,
 * though the empty 'shell' of a tree left over after the nodes are
 * transferred out is then deleted, and the subsumed_tree pointer is
 * set to NULL.
 */
int tree_graft(tree *main_tree, tree **subsumed_tree, tree_node *parent_node,
               const char *root_name);

/* ------------------------------------------------------------------------- */
/** Invalidate and free the specified tree context.
 */
int tree_context_free(tree *tr, tree_context_handle *context);

/*@}*/

/* ========================================================================= */
/** @name Convenience API
 *
 * This section contains functions which are not privvy to the internals
 * of the tree data structure, and are defined exclusively in terms of
 * the external tree API.
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** Return the total number of nodes in the tree, including the root.
 */
int tree_get_num_nodes(const tree *tr, uint32 *ret_num_nodes);

/* ------------------------------------------------------------------------- */
/** Return the total number of descendants of the specified node,
 * not including the node itself.
 */
int tree_node_get_num_descendants(const tree *tr, const tree_node *node,
                                  uint32 *ret_num_descendants);

/* ------------------------------------------------------------------------- */
/** Return the depth of the node in the tree.  The root node is considered
 * to have depth 0.
 */
int tree_node_get_depth(const tree *tr, const tree_node *node,
                        uint32 *ret_depth);

/* ------------------------------------------------------------------------- */
/** Return a path to the specified node in the form of an tstr_array of
 * names.  The array returned is dynamically allocated and should be
 * freed by the caller using tstr_array_free().
 *
 * Note that the array is constructed with the option to not copy
 * strings when they are added, which would be interesting in the rare
 * case that the caller intends to add more elements to the array.
 *
 * If a subtree root node is provided, the path returned is relative
 * to that node.  If the node specified is not a descendant of the
 * subtree root, an error is returned.  If NULL is specified for the
 * subtree root, the path returned is an absolute path.
 */
int tree_node_get_path(const tree *tr, const tree_node *node, 
                       const tree_node *subtree_root_node,
                       tstr_array **ret_path);

/* ------------------------------------------------------------------------- */
/** Return a path to the specified node in the form of a single
 * dynamically-allocated string.  The string is formed by first
 * getting a path using tree_node_get_path() and then calling the path
 * escaping function provided during tree creation.
 *
 * Note that naming the root of the tree as the subtree root has
 * different behavior than passing NULL; in the former case, the path
 * returned is a relative path (according to however the client
 * chooses to encode this in the string), while in the latter case the
 * path returned is absolute.
 */
int tree_node_get_path_str(const tree *tr, const tree_node *node,
                           const tree_node *subtree_root_node,
                           char **ret_path_str);


/* ------------------------------------------------------------------------- */
/** Return a pointer to the deepest node along the path to the specified path.
 * If there is no such ancestor, return NULL.
 */
int tree_get_node_ancestor_by_path(const tree *tr, const tree_node *base_node,
                           const tstr_array *path, 
                           tree_node **ret_ancestor_node,
                           tree_context_handle *inout_context);

int tree_get_node_ancestor_by_path_str(const tree *tr, 
                               const tree_node *base_node,
                               const char *path_str,
                               tree_node **ret_ancestor_node,
                               tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return an array of pointers to node values, one for each level of the 
 * specified path.  'ret_path_depth' is the number of elements in 'path',
 * and 'ret_match_depth' is the number of node values we were able to
 * match from the tree.  This is a wildcarded match, where the tree node
 * name component can match either the path name component or the wildcard
 * name.
 */
int tree_get_node_value_array_by_path_wildcarded(const tree *tr, 
                                                 const tree_node *base_node,
                                                 const tstr_array *path, 
                                                 const char *wc_name,
                                                 tree_node **ret_found_node,
                                                 array **ret_node_value_array, 
                                                 int32 *ret_path_depth,
                                                 int32 *ret_match_depth,
                                                 tree_context_handle *
                                                 inout_context);

int tree_get_node_value_array_by_path_str_wildcarded(const tree *tr, 
                                                     const tree_node *
                                                     base_node,
                                                     const char *path_str, 
                                                     const char *wc_name,
                                                     tree_node **
                                                     ret_found_node,
                                                     array **
                                                     ret_node_value_array, 
                                                     int32 *ret_path_depth, 
                                                     int32 *ret_match_depth,
                                                     tree_context_handle *
                                                     inout_context);


/* ------------------------------------------------------------------------- */
/** Return a pointer to the deepest matched node value for the specified
 * path.  Similar to tree_get_node_value_array_by_path_wildcarded(), but
 * only returns the deepest matched value, instead of all values along
 * the path.
 *
 * \param tr Tree to be searched.
 *
 * \param base_node If NULL, path must be absolute, and searching starts
 * at the root of the tree.  Otherwise, searching starts at this node,
 * and the path is treated as relative.
 *
 * \param path The path to do the search for.
 *
 * \param wc_name The name that corresponds to the wildcard.  Typically
 * this is "*" . The tree node name component can match either the path
 * name component or this wildcard name.
 *
 * \retval ret_found_node Set if there is a full match to the node we
 * looked up.
 *
 * \retval ret_node_best_value Set to the value of the deepest node we
 * matched along the full path.
 *
 * \retval ret_path_depth The number of elements in 'path' .
 *
 * \retval ret_match_depth The number of node name parts we were able to
 * match from the tree.
 *
 * \param inout_context An opaque data structure to help optimize
 * certain tree operations.  See above for more information on contexts.
 *
 */
int tree_get_node_best_value_by_path_wildcarded(const tree *tr, 
                                       const tree_node *base_node,
                                       const tstr_array *path, 
                                       const char *wc_name,
                                       tree_node **ret_found_node,
                                       void **ret_node_best_value, 
                                       int32 *ret_path_depth, 
                                       int32 *ret_match_depth,
                                       tree_context_handle *
                                       inout_context);

int tree_get_node_best_value_by_path_str_wildcarded(const tree *tr, 
                                       const tree_node *base_node,
                                       const char *path_str,
                                       const char *wc_name,
                                       tree_node **ret_found_node,
                                       void **ret_node_best_value, 
                                       int32 *ret_path_depth, 
                                       int32 *ret_match_depth,
                                       tree_context_handle *
                                       inout_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to a node by following a specified path.  This is a
 * wildcarded match, where the tree node name component can match either the
 * path name component or the wildcard name.
 */

int tree_get_node_by_path_wildcarded(const tree *tr, 
                                     const tree_node *base_node,
                                     const tstr_array *path, 
                                     const char *wc_name,
                                     tree_node **ret_found_node,
                                     tree_context_handle *inout_context);

int tree_get_node_by_path_str_wildcarded(const tree *tr, 
                                         const tree_node *base_node,
                                         const char *path_str,
                                         const char *wc_name,
                                         tree_node **ret_found_node,
                                         tree_context_handle *inout_context);

/*@}*/

/* ------------------------------------------------------------------------- */
/** @name Selecting nodes by path
 *
 * All of the ..._by_path() functions below perform operations on
 * nodes whose name and location in the tree are specified by a
 * provided path and an optional base node.  The name of the node
 * will always be the last component of the path.
 *
 * If base_node is NULL, the path provided must be absolute.  If
 * base_node is a node in the tree, the path provided is treated as
 * relative to that node.
 *
 * The implementation works natively with the path specified as a
 * tstr_array, with each string being the name of one node in the
 * path.  The ..._str() variants on these functions use the unescaping
 * function provided at tree creation to convert the single-string
 * representation of a path into a tstr_array, which is then
 * passed on to the base variant of the same function, and then
 * freed before returning the answer to the caller.
 *
 * The context handle pointers passed to these functions are 'inout'
 * instead of 'ret' because the caller is assumed not to have a valid
 * context for the node specified by the path.  If he did, he would
 * also have a pointer to the node, and would not be using the
 * less-efficient ..._by_path() variant.
 */

/*@{*/

/* ------------------------------------------------------------------------- */
/** Return a pointer to the lexically next node after the one specified
 * by a path.  Note that the base node does not limit the scope of the
 * traversal -- the node returned may be outside the subtree below
 * the base node.  The base node is only used in conjunction with the
 * path to identify the node from which to start.
 */
int tree_node_get_next_by_path(const tree *tr, const tree_node *base_node,
                               const tstr_array *path,
                               tree_node **ret_next_node,
                               tree_context_handle *inout_context);

/**
 * Same as tree_node_get_next_by_path() except the path comes 
 * as a string instead of a tstr_array broken down by path component.
 */
int tree_node_get_next_by_path_str(const tree *tr, const tree_node *base_node,
                                   const char *path_str,
                                   tree_node **ret_next_node,
                                   tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the lexically previous node before the one
 * specified by a path.  Note that the base node does not limit the
 * scope of the traversal -- the node returned may be outside the
 * subtree below the base node.  The base node is only used in
 * conjunction with the path to identify the node from which to start.
 */
int tree_node_get_prev_by_path(const tree *tr, const tree_node *base_node,
                               const tstr_array *path,
                               tree_node **ret_prev_node,
                               tree_context_handle *inout_context);

/**
 * Same as tree_node_get_prev_by_path() except the path comes 
 * as a string instead of a tstr_array broken down by path component.
 */
int tree_node_get_prev_by_path_str(const tree *tr, const tree_node *base_node,
                                   const char *path_str,
                                   tree_node **ret_prev_node,
                                   tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the next sibling of the node specified by the
 * provided path.
 */
int tree_node_get_next_sibling_by_path(const tree *tr, const tree_node *node,
                                       const tstr_array *path,
                                       tree_node **ret_next_node,
                                       tree_context_handle *ret_context);

/**
 * Same as tree_node_get_next_sibling_by_path() except the path comes 
 * as a string instead of a tstr_array broken down by path component.
 */
int tree_node_get_next_sibling_by_path_str(const tree *tr,
                                           const tree_node *node,
                                           const char *path_str,
                                           tree_node **ret_next_node,
                                           tree_context_handle *ret_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to the previous sibling of the node specified by the
 * provided path.
 */
int tree_node_get_prev_sibling_by_path(const tree *tr, const tree_node *node,
                                       const tstr_array *path,
                                       tree_node **ret_next_node,
                                       tree_context_handle *ret_context);

/**
 * Same as tree_node_get_prev_sibling_by_path() except the path comes 
 * as a string instead of a tstr_array broken down by path component.
 */
int tree_node_get_prev_sibling_by_path_str(const tree *tr,
                                           const tree_node *node,
                                           const char *path_str,
                                           tree_node **ret_next_node,
                                           tree_context_handle *ret_context);

/* ------------------------------------------------------------------------- */
/** Return a pointer to a node by following a specified path.  
 */
int tree_get_node_by_path(const tree *tr, const tree_node *base_node,
                          const tstr_array *path, tree_node **ret_found_node,
                          tree_context_handle *inout_context);

/**
 * Same as tree_get_node_by_path() except the path comes 
 * as a string instead of a tstr_array broken down by path component.
 */
int tree_get_node_by_path_str(const tree *tr, const tree_node *base_node,
                              const char *path_str, tree_node **ret_found_node,
                              tree_context_handle *inout_context);

/* ------------------------------------------------------------------------- */
/** Set the value of the node with the specified path, after freeing
 * any value it previously had.  The path is relative to the base node,
 * or absolute if base_node is NULL.
 *
 * The 'create_node' argument determines what happens if a node with
 * the specified path did not already exist.  If it is set to true,
 * the node is created using tree_insert_node_by_path, also creating
 * any necessary ancestors.  Otherwise, an error is returned.
 *
 * A pointer to the node whose value was set is returned in ret_node.
 */
int tree_set_node_value_by_path(tree *tr, tree_node *base_node,
                                const tstr_array *path, void *value,
                                tbool create_node, tree_node **ret_node);

/**
 * Same as tree_set_node_value_by_path() except the path comes 
 * as a string instead of a tstr_array broken down by path component.
 */
int tree_set_node_value_by_path_str(tree *tr, tree_node *base_node, 
                                    const char *path_str, void *value,
                                    tbool create_node, tree_node **ret_node);

/* ------------------------------------------------------------------------- */
/** Insert a node into the tree at a location specified by a path.
 * Note that unlike tree_node_insert_child(), no name is provided
 * because the new node's name is included in the path.  If the
 * interior nodes necessary to insert the node do not exist yet:
 * if create_path is true, they are created with NULL values;
 * otherwise an error is returned.  If ret_new_node is non-NULL, the
 * newly-created child node is returned in it.
 *
 * Note: if the duplicate policy is tdp_overwrite_old, or if the node did
 * not already exist, this is the same as tree_set_node_value_by_path()
 * with create_node set to true.
 */
int tree_insert_node_by_path(tree *tr, tree_node *base_node,
                             const tstr_array *path, void *value,
                             tbool create_path, tree_node **ret_new_node);

/**
 * Same as tree_insert_node_by_path() except the path comes 
 * as a string instead of a tstr_array broken down by path component.
 */
int tree_insert_node_by_path_str(tree *tr, tree_node *base_node,
                                 const char *path_str, void *value,
                                 tbool create_path, tree_node **ret_new_node);

/* ------------------------------------------------------------------------- */
/** Delete a node with the specified path.  If delete_children is false
 * and the node has children, lc_err_has_children is returned;
 * otherwise all of its children, if any, are deleted.
 */
int tree_delete_node_by_path(tree *tr, tree_node *base_node,
                             const tstr_array *path, tbool delete_children);

/**
 * Same as tree_delete_node_by_path() except the path comes 
 * as a string instead of a tstr_array broken down by path component.
 */
int tree_delete_node_by_path_str(tree *tr, tree_node *base_node,
                                 const char *path_str, tbool delete_children);

/* ------------------------------------------------------------------------- */
/** Remove a node with the specified path, as described in
 * tree_node_remove().
 */
int tree_remove_node_by_path(tree *tr, tree_node *base_node,
                             const tstr_array *path, void **ret_value,
                             tbool delete_children);

/**
 * Same as tree_remove_node_by_path() except the path comes 
 * as a string instead of a tstr_array broken down by path component.
 */
int tree_remove_node_by_path_str(tree *tr, tree_node *base_node,
                                 const char *path_str, void **ret_value,
                                 tbool delete_children);

#if 0
/* ------------------------------------------------------------------------- */
/** Graft the 'subsumed' tree onto the 'main' tree, as described in
 * tree_graft() above.  The root node of the subsumed tree will be
 * added at ther specified path.  As with tree_insert_node_by_path(),
 * if interior nodes in the path do not exist, they are created with
 * NULL values if create_path is true, or an error is returned if it
 * is false.
 *
 * NOT YET IMPLEMENTED.
 */
int tree_graft_by_path(tree *main_tree, tree **subsumed_tree,
                       const tstr_array *root_path, tbool create_path);

/**
 * Same as tree_graft_by_path() except the path comes 
 * as a string instead of a tstr_array broken down by path component.
 *
 * NOT YET IMPLEMENTED.
 */
int tree_graft_by_path_str(tree *main_tree, tree **subsumed_tree,
                           const char *root_path_str, tbool create_path);
#endif

/*@}*/

/* ------------------------------------------------------------------------- */
/** Function to be called on a set of nodes in a tree, according to the
 * parameters specified to tree_foreach() or tree_foreach_delete().
 *
 * The abspath array-of-strings representation of the node's absolute
 * path may not be provided, depending on the parameters passed to
 * tree_foreach().  If it are not provided, it will be NULL.  This
 * option exists because the library is in a position to compute the
 * path more efficiently than the callback function; as it iterates
 * down a list of siblings, it only needs to change the last component
 * in the path, rather than tracing all the way back to the tree root.
 *
 * Its return value may be any of the following:
 *   - lc_err_no_error
 *   - lc_err_foreach_delete (only valid if called from tree_foreach_delete();
 *     if returned from tree_foreach(), treated as lc_err_foreach_halt_err)
 *   - lc_err_foreach_halt_err
 *   - lc_err_foreach_halt_ok
 *   - anything else: a nonfatal error.  Iteration continues, but an error
 *     is returned from tree_foreach() when it is finished.
 */
typedef int (*tree_foreach_func)(tree *tr, tree_node *node,
                                 const char *name, const tstr_array *abspath,
                                 void *value, void *data,
                                 tree_context_handle context);

/* ------------------------------------------------------------------------- */
/** Options to specify how to iterate over a tree.  The options desired
 * should be ORed together.
 */
typedef enum {
    /** No options */
    tfo_none           = 0,

    /**
     * Specifies whether to only traverse the immediate children of 
     * the base node (if not set), or to traverse all descendants of 
     * the base node (if set).
     */
    tfo_subtree        = 1 << 0,

    /**
     * Specifies whether or not the base node itself is included in 
     * the traversal.
     */
    tfo_include_self   = 1 << 1,

    /**
     * Specifies whether we should only return leaf nodes.
     */
    tfo_leaf_only      = 1 << 2,

    /**
     * Specifies whether the 'name' parameter is just the name component,
     * or the full path.
     */
    tfo_name_full_path = 1 << 3,

    /**
     * Specifies whether or not the abspath parameter for the foreach
     * callback is calculated and passed by the library.  If it is not
     * set, NULL will be passed instead.  Note that this parameter is
     * a string array, while the absolute path provided by
     * tfo_name_full_path is a string.
     */
    tfo_provide_path   = 1 << 4,

    /**
     * By default, specifying NULL for a node name means to use the
     * root node.  But specifying this flag means that NULL really
     * means NULL.  This is nonsensical for vanilla tree_foreach(),
     * since a subtree root node is needed.  But it can be useful to
     * use for one (not both!) of the subtree roots for 
     * tree_foreach_diff().
     */
    tfo_null_not_root  = 1 << 5,

} tree_foreach_options;

/* Bit field of ::tree_foreach_options ORed together */
typedef uint32 tree_foreach_options_bf;

/* ------------------------------------------------------------------------- */
/** Call the specified callback function once for each of a set of tree
 * nodes, using a preorder traversal.  The arguments are as follows:
 *   - base_node determines the root of the subtree whose nodes to
 *     traverse.  If NULL is specified, the root of the entire tree
 *     is used.
 *   - data is passed to the foreach_func with every call.
 *
 * The return value may be lc_err_foreach_halt_err, meaning one of the
 * foreach functions halted the traversal with an error;
 * lc_err_foreach_halt_ok, meaning one of the foreach functions halted
 * the traversal with no error; lc_err_no_error, meaning the traversal
 * completed without incident; or anything else, meaning that one or
 * more nonfatal errors occurred during the traversal, or a different
 * fatal error occurred that was not returned from a foreach function.
 */
int tree_foreach(tree *tr, tree_node *base_node,
                 tree_foreach_options_bf options,
                 tree_foreach_func foreach_func, void *data);

/* ------------------------------------------------------------------------- */
/** Works identically to tree_foreach(), except that tree_foreach_delete_me
 * is a valid return value from the foreach function, and will cause the
 * current node to be deleted.
 *
 * If a node in the tree is deleted in this manner, all of its children
 * will also be deleted.  Since a preorder traversal is used, if the
 * subtree option is specified this will prevent any of its children
 * from being passed to the foreach function.
 */
int tree_foreach_delete(tree *tr, tree_node *base_node,
                        tree_foreach_options_bf options,
                        tree_foreach_func foreach_func, void *data);



/* ------------------------------------------------------------------------- */
/** A function to be called once for every unique absolute path that
 * occurs in either of two trees passed to tree_foreach_diff().
 *
 * The two nodes we are called with will never have different names or
 * absolute paths.  Either one will be NULL, or both will match.
 * If one is NULL, that indicates that the corresponding tree does
 * not contain a node with the same absolute path as the other node
 * passed.
 *
 * Note that the special return values that can be returned by a 
 * tree_foreach_func are not honored here.  If you return nonzero,
 * the infrastructure will complain, but continue with the iteration.
 *
 * NOTE: currently the abspath1 and abspath2 parameters will always be
 * NULL, as expected since we do not support the tfo_name_full_path
 * flag for tree_foreach_diff().
 */
typedef int (*tree_foreach_diff_func)(tree *tr1, 
                                      tree_node *node1,
                                      const char *name1,
                                      const tstr_array *abspath1,
                                      void *value1,
                                      tree *tr2, 
                                      tree_node *node2,
                                      const char *name2, 
                                      const tstr_array *abspath2,
                                      void *value2,
                                      void *data, tree_context_handle context);

/* ------------------------------------------------------------------------- */
/** Walk two trees side-by-side, calling a provided function once for
 * every unique node path that occurs in either tree.  What makes this
 * function more interesting than calling tree_foreach() twice in
 * succession is that if an absolute path is shared by nodes in both
 * trees, the callback will be called only once with both nodes.  We
 * guarantee that the function will be called with names in the order
 * they would occur in a preorder traversal of a hypothetical tree
 * that had all of these names in it.
 *
 * For example, consider the following two trees:
 *
 * Tree 1: (root)--+--a----+--e
 *                 |       |--f
 *                 |
 *                 +--j
 *                 +--k
 *
 *
 * Tree 2: (root)--+--a----+--d1
 *                 |       |--d2
 *                 |       |--e---2
 *                 |       |--g
 *                 +--b----+--h
 *                 |       |--i
 *                 |--k
 *                 |--l
 *
 * The calls to our function would be:
 *
 *    Node from tree 1    Node from tree 2
 *    ----------------    ----------------
 *          /a                  /a
 *          NULL                /a/d1
 *          NULL                /a/d2
 *          /a/e                /a/e
 *          NULL                /a/e/2
 *          /a/f                NULL
 *          NULL                /a/g
 *          NULL                /b
 *          NULL                /b/h
 *          NULL                /b/i
 *          /j                  NULL
 *          /k                  /k
 *          NULL                /l
 *
 * We accept a tree_foreach_options_bf here to allow for future expansion,
 * but currently only two combinations of options are supported:
 * (tfo_subtree | tfo_include_self), with or without tfo_null_not_root.
 * Any other sets of flags will fail the entire call.
 *
 * NOTE: the two trees must have the same compare function (used for
 * purposes of determining node ordering).
 *
 * \param t1 First tree to use in comparison.
 *
 * \param base_node1 Node in t1 to use as subtree root for comparison.
 * By default NULL here means to use the tree root, although if
 * ::tfo_null_not_root is specified, that means to take the NULL literally
 * That would mean that there is no subtree in this tree to compare to,
 * so effectively the foreach_diff_func would be called for every node
 * in t2, just as it would for tree_foreach().
 *
 * \param t2 Second tree to use in comparison.
 *
 * \param base_node2 Node in t2 to use as subtree root for comparison.
 * Ditto notes above for base_node1.
 *
 * \param options Bit field of ::tree_foreach_options ORed together.
 *
 * \param foreach_diff_func Callback function to call.
 *
 * \param data Data to pass to foreach_diff_func.
 */
int tree_foreach_diff(tree *t1, tree_node *base_node1,
                      tree *t2, tree_node *base_node2,
                      tree_foreach_options_bf options,
                      tree_foreach_diff_func foreach_diff_func, void *data);



/* ---------------------------------------------------------------------- */
/** Debugging function to print all the tree node names 
 */
int tree_print(const tree *tr, const char *prefix);

TYPED_ARRAY_HEADER_NEW_NONE(tree, tree *);

/**
 * Create a new array of trees.  Note that the array has no comparison
 * function registered with it (as we have no defined way to compare
 * trees), so any sorting of the array will be meaningless.
 */
int tree_array_new(tree_array **ret_tarr);

/* ---------------------------------------------------------------------- */
/** Flags that can be set on a tree node.  Since an end caller could
 * generally put whatever information they wanted in the tree value,
 * this is intended mainly for use by wrapper APIs such as ltree, who
 * need to store additional per-node information without disturbing
 * the value.
 */
typedef enum {
    tnf_none = 0,

    /**
     * Can be used to indicate that this node has been deleted,
     * without actually deleting it.
     */
    tnf_deleted = 1 << 0,

    /**
     * Can be used to represent that this node has been modified from
     * its original, sort of like a "dirty" bit.
     */
    tnf_modified = 1 << 1,

    /**
     * Can be used to indicate that this node is a real node, as
     * opposed to being here only to be an ancestor to a "deleted"
     * node.  See comment at top of ltree.c.
     */
    tnf_exists = 1 << 2,

} tree_node_flags;

typedef uint32 tree_node_flags_bf;


/* ---------------------------------------------------------------------- */
/** Set the flags on this node.  This replaces all previous flags.
 */
int tree_node_set_flags(tree_node *node, tree_node_flags_bf new_flags);

/* ---------------------------------------------------------------------- */
/** Get all of the flags on this node.  All nodes start with all flags
 * cleared, until they are explicitly set with tree_node_set_flags().
 */
int tree_node_get_flags(const tree_node *node, tree_node_flags_bf *ret_flags);

#ifdef __cplusplus
}
#endif

#endif /* __TREE_H_ */
