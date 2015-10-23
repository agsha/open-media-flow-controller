#ifndef XML_UTILS_H
#define  XML_UTILS_H

#include <syslog.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>


xmlNodePtr SearchNode (xmlNodePtr root, xmlChar * name);
void dumpNode(xmlNodePtr, xmlDocPtr);
xmlNodeSetPtr GetMatchingNodeSet(xmlNodePtr node, const char *xpathExpr,xmlXPathObjectPtr *retPath);
xmlNodePtr GetMatchingNode(xmlNodePtr, char *);
xmlNodeSetPtr getNodeSetUnderNode(xmlDocPtr,xmlNodePtr,xmlChar *,xmlXPathObjectPtr *retPath);
xmlNodeSetPtr getNodeSet (xmlDocPtr, xmlChar *);
char * GetAttributeValueEx(xmlNodePtr, char *);
char * GetAttributeValue(xmlNodePtr, char *);
void printNodeSet(xmlNodeSetPtr, xmlDocPtr);
xmlNode * find_element_by_name(xmlNode * node, const char *name);
#endif /*XML_UTILS_H*/
