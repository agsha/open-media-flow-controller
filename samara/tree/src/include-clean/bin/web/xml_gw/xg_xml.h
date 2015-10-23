/*
 *
 * src/bin/web/xml_gw/xg_xml.h
 *
 *
 *
 */

#ifndef __XG_XML_H_
#define __XG_XML_H_


#ifdef __cplusplus
extern "C" {
#endif
#if 0 /* for emacs */
}
#endif

#include <libxml/parser.h>
#include <libxml/tree.h>
#include "xg_main.h"

char *xg_get_child_string_full(xmlNode *parent_node, tbool trim_spaces);

char *xg_get_child_string(xmlNode *parent_node);

/* Returns NULL for attribs that do not exist */
char *xg_get_attrib_string_full(xmlNode *node,
                                const char *attrib_name,
                                tbool trim_spaces);

char *xg_get_attrib_string(xmlNode *node,
                           const char *attrib_name);

int xg_set_attrib_string(xmlNode *node,
                         const char *attrib_name,
                         const char *attrib_value);

int xg_append_text_node_sprintf(xmlNode *parent, xmlNode **ret_new_node,
                                const char *name, const char *format, ...)
    __attribute__ ((format (printf, 4, 5)));

int xg_append_text_node_str(xmlNode *parent, xmlNode **ret_new_node,
                        const char *name, const char *value);

int xg_xml_init(void);

int xg_xml_deinit(void);


#ifdef __cplusplus
}
#endif

#endif /* __XG_XML_H_ */
