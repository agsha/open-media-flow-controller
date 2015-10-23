/*
 *
 * txml.h
 *
 *
 *
 */

#ifndef __TXML_H_
#define __TXML_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

/** \defgroup ltxml LibTXML: XML Utilities for use with libxml2 */

/* ------------------------------------------------------------------------- */
/** \file src/include/txml.h XML utilities.
 * \ingroup ltxml
 *
 * This is a wrapper for the libxml2 API.  The intention is that you
 * never have to call functions from libxml2, but you do need to use
 * its data types (only the ones returned from the libtxml API).
 *
 * The functions in this library all start with "txm_".  Conventions
 * would suggest "ltx_", but that was already taken by libtexpr.
 *
 * IMPORTANT NOTE: this library does not entirely follow our standard
 * conventions of memory ownership.  It returns non-const pointers to
 * dynamically allocated objects which you must NOT free, as some of
 * them are already owned by something else.  The rule is this: xmlDoc
 * objects returned to you ARE for you to free, using txm_doc_free().
 * xmlNode objects are NOT for you to free, as they are always created
 * in the context of an xmlDoc, which owns them.  There is no libtxml
 * API to free an xmlNode, so as long as you stick to only using this
 * API, and do not call directly into libxml2 or call free(), you'll
 * be fine.
 */

#include <libxml/parser.h>
#include <libxml/tree.h>
#include "common.h"
#include "tstring.h"

typedef struct txm_context txm_context;

/** @name General */
/*@{*/

/**
 * Initialize the library.  Call this once per process before making
 * any other calls to this API.  If the process is multithreaded, call
 * it from the main thread before launching any other threads.
 */
int txm_init(void);

/**
 * Denitialize the library.  No calls to the library are permitted
 * afterwards, unless txm_init() is called again.
 */
int txm_deinit(void);

/**
 * Get a context pointer.  Call this once per thread which will be 
 * making calls to this library -- each thread must have its own
 * separate context.
 *
 * \retval ret_context The newly-created context.
 */
int txm_context_new(txm_context **ret_context);

/**
 * Free a context.  Call this on every context gotten from
 * txm_get_context(), once you are done with it.
 *
 * \param inout_context The context to be freed.  This pointer will be
 * set to NULL.
 */
int txm_context_free(txm_context **inout_context);

/*@}*/

/** @name Creating and modifying xmlDocs */
/*@{*/

/**
 * Create a new XML document with a root node.  Ownership of the
 * document is given to the caller, which must free it with
 * txm_doc_free() when finished.  Ownership of the root node is 
 * retained by the document, but the pointer is returned so the 
 * caller can add properties, children, etc.
 *
 * \param context Libtxml context pointer.
 *
 * \retval ret_doc The newly-created document, to be owned by the caller.
 *
 * \retval ret_root_node The root node of the document, not to be
 * owned by the caller.
 *
 * \param xml_version XML version string.  For now, must be "1.0".
 *
 * \param root_node_name Name of root node to create.
 */
int txm_doc_new(txm_context *context, xmlDoc **ret_doc,
                xmlNode **ret_root_node, const char *xml_version,
                const char *root_node_name);

/**
 * Free an XML document.
 *
 * \param context Libtxml context pointer.
 *
 * \param inout_doc Document to be freed.  This pointer will be set to
 * NULL.
 */
int txm_doc_free(txm_context *context, xmlDoc **inout_doc);

/**
 * Create a new node, and add it as a child of an existing node.
 *
 * \param context Libtxml context pointer.
 *
 * \retval ret_child_node The newly-created child node.  Ownership is 
 * NOT assigned to the caller; it resides with the document containing
 * the specified parent node.  It is returned to the caller so he can
 * add properties, child nodes, etc.
 *
 * \param parent_node Existing node which will be the parent of the
 * newly-created node.
 *
 * \param child_name Name of the new child node to create.
 *
 * \param child_content Text content of the child node, if any.
 * May be NULL to skip content.
 */
int txm_node_add_child_str(txm_context *context, xmlNode **ret_child_node,
                           xmlNode *parent_node, const char *child_name, 
                           const char *child_content);

/**
 * Same as txm_node_add_child_str(), except the content is taken as a 
 * printf format string and variable argument list.
 */
int txm_node_add_child_sprintf(txm_context *context, xmlNode **ret_child_node,
                               xmlNode *parent_node, const char *child_name,
                               const char *child_content_fmt, ...)
     __attribute__ ((format (printf, 5, 6)));

/**
 * Add a property to a node.
 */
int txm_node_add_property_str(txm_context *context, xmlNode *node, 
                              const char *prop_name, const char *prop_value);

/**
 * Same as txm_node_add_property_str(), except the content is taken as a 
 * printf format string and variable argument list.
 */
int txm_node_add_property_sprintf(txm_context *context, xmlNode *node, 
                                  const char *prop_name,
                                  const char *prop_value_fmt, ...)
     __attribute__ ((format (printf, 4, 5)));

/** A bit field of xmlParserOption ORed together */
typedef uint32 xmlParserOption_bf;

/**
 * Parse an XML string, and convert into a xmlDoc structure.
 */
int txm_doc_parse(txm_context *context, xmlDoc **ret_doc, const char *str,
                  xmlParserOption_bf flags);

/*@}*/

/** @name Reading xmlDocs */
/*@{*/

/**
 * Get the root node of an XML document.
 */
int txm_doc_get_root(txm_context *context, xmlNode **ret_root_node,
                     xmlDoc *doc);

/**
 * Search the children of the specified parent node for a child node
 * with the specified name.  Return the first one we find, if any.
 */
int txm_node_get_first_matching_child(txm_context *context,
                                      xmlNode **ret_child_node,
                                      xmlNode *parent_node,
                                      const char *child_name);

/**
 * Selects the next sibling of this node which shares the same name,
 * if any.  Normally to be used as a followup to 
 * txm_node_get_first_matching_child(), if multiple matching nodes
 * are suspected.
 */
int txm_node_get_next_matching_sibling(txm_context *context,
                                       xmlNode **ret_sib_node,
                                       xmlNode *node);

/**
 * Options for txm_node_get_content().
 *
 * None are defined at present, but this is available for future
 * expansion.
 */
typedef enum {
    tncf_none = 0,
} txm_node_get_content_flags;

/** 
 * A bit field of ::txm_node_get_content_flags ORed together.
 */
typedef uint32 txm_node_get_content_flags_bf;

/**
 * Get the content of a node.
 *
 * \param context Libtxml context pointer.
 *
 * \retval ret_content The content retrieved from the node.  Even if 
 * there wasn't any, you still get back an empty string.  We do not 
 * return NULL except in case of error.
 *
 * \param node The node whose content to get.
 *
 * \param flags Option flags.
 */
int txm_node_get_content(txm_context *context, tstring **ret_content,
                         xmlNode *node, txm_node_get_content_flags_bf flags);

/**
 * Get the value of a property on a node, if it exists.
 *
 * \param context Libtxml context pointer.
 *
 * \retval ret_value The value of the property, if it was found.
 * If the specified property was not found on the node, NULL is returned.
 *
 * \param node The node to search for the property.
 *
 * \param prop_name The name of the property to search for.
 */
int txm_node_get_property(txm_context *context, tstring **ret_value,
                          xmlNode *node, const char *prop_name);

/**
 * Get either the content or a property of a node in a document.
 * This is a helper function that tries to make it easier to extract
 * content, but does not provide any unique capabilities.
 *
 * \param context Libtxml context pointer.
 *
 * \retval ret_data The string fetched from the document.  This will
 * be either the content of a node, or a property, depending on the
 * prop_name parameter.  If the empty string is returned, this means
 * the node was found (and the property was found, if prop_name was
 * specified), but was empty.  If NULL is returned, this means the
 * node was not found (or the property was not found, if prop_name
 * was specified).
 *
 * \param root The root node from which to start searching.
 *
 * \param path The path relative to the root node of the node to fetch.
 * This is in the same format as a relative mgmtd node name, with name
 * parts separated by '/' characters, using '\' for escaping.  It is
 * always relative because it is relative to the root node (which may
 * or may not be the root node of the XML document).  Note that this
 * may NOT include property names.  If this path is NULL, the root node
 * is used, and so the function acts just like a single call to 
 * txm_node_get_content() or txm_node_get_property().
 *
 * \param prop_name Name of property to fetch, if any.  If this is
 * NULL, ret_data will get the content of the node named.  If this is
 * non-NULL, it specifies the name of a property whose value should be
 * fetched from the node named.
 */
int txm_node_get_data(txm_context *context, tstring **ret_data,
                      xmlNode *root, const char *path,
                      const char *prop_name);

/**
 * Option flags for txm_doc_render().
 */
typedef enum {
    tdrf_none = 0,

    /**
     * Formatting option: should whitespace be added to make document
     * more readable?  (Note: this will probably make it LESS readable
     * in syslog, since it removes line breaks.)
     */
    tdrf_add_whitespace = 1 << 0,

} txm_doc_render_flags;

/** 
 * A bit field of ::txm_doc_render_flags ORed together.
 */
typedef uint32 txm_doc_render_flags_bf;

/**
 * Render an XML document into a string.
 */
int txm_doc_render(txm_context *context, tstring **ret_str, xmlDoc *doc, 
                   txm_doc_render_flags_bf flags);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif /* __TXML_H_ */
