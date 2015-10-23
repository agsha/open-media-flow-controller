#include "xml_utils.h"

//xmlNode *
//find_element_by_name(xmlNode * a_node, const char *name);

/*
 * This method gives the value of an attribute given the node
 * and the attribute name
 * Example: <message name = "example1">
 * 		</message>
 * Now GetAttributeValue(messageNode, "name") will give the value
 * 'example1'.
 * xmlGetProp method needs freeing the returned string.
 * xmlFree(val) is to be used wherever GetAttributeValue is called.
 */
char *
GetAttributeValue(xmlNodePtr nodePtr, char *attrName) {
	char *val;
	val = (char *)xmlGetProp(nodePtr, (xmlChar *)attrName);
	if (val)
		return val;
	return NULL;
}

char *
GetAttributeValueEx(xmlNodePtr nodePtr, char *attrName) {
	char *val;
	static char _val[4096];
	val = (char *)xmlGetProp(nodePtr, (xmlChar *)attrName);
	if (val) {
		strncpy(_val, val, 4096);
		xmlFree(val);
		return &_val[0];
	}
	return NULL;
}

static int
registerNSNamespaces(xmlXPathContextPtr ctxt) {

	xmlXPathRegisterNs(ctxt, (xmlChar *)"config",
					   (xmlChar *)"http://xml.juniper.net/xnm/1.1/xnm");
	return 0;
}
/*
 * This method returns the list of nodes matching
 * the given xpath expression under the given DocPtr.
 */
xmlNodeSetPtr
getNodeSet (xmlDocPtr doc, xmlChar *xpath) {
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;
	context = xmlXPathNewContext(doc);
	registerNSNamespaces(context);
	result = xmlXPathEvalExpression(xpath, context);
	xmlXPathFreeContext(context);
	if (result && result->type == XPATH_NODESET && result->nodesetval &&
		result->nodesetval->nodeNr)
		return result->nodesetval;
	return NULL;
}

/*
 * This method which returns the list of nodes matching the
 * given xpath Expression under a 'specific' node with the given DocPtr.
 *
 *  The third argument is to return the path object which needs to
 *  be cleaned up the user of the API
 */
xmlNodeSetPtr
getNodeSetUnderNode(xmlDocPtr doc, xmlNodePtr cur, xmlChar *xpath,xmlXPathObjectPtr *retPath) {
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;

	/* create the context */
	context = xmlXPathNewContext(doc);
	context->node = cur;

	registerNSNamespaces(context);
	/* evaluate the expression */
	result = xmlXPathEvalExpression(xpath, context);

	/* now that we are done, free the context */
	xmlXPathFreeContext(context);

	if(result==NULL)
	    return NULL;

	if (result->type == XPATH_NODESET
	        && result->nodesetval
	        && result->nodesetval->nodeNr)
	{
	    *retPath = result;
		return result->nodesetval;

	}
	else
	{
	    xmlXPathFreeObject(result);
	}
	/* failure, return NULL */
	return NULL;
}

/*
 * This method retuns a single node in the xmlTree
 * mathcing the given xpath expression.
 * Input: The node under which to search in the xml tree, the xpath Expression.
 * Output: The matching node.
 *
 * TODO: Need to receive the xmlXPathObjectPtr *retPath (as 3rd arg) to return
 * the object that needs to be cleaned up by the caller.
 */
xmlNodePtr
GetMatchingNode(xmlNodePtr node, char *xpathExpr) {
	xmlNodeSetPtr nodeSet = NULL;
	xmlXPathObjectPtr *retPath=NULL;
	nodeSet = getNodeSetUnderNode(node->doc, node, (xmlChar *)xpathExpr,retPath);
	if (!nodeSet) return NULL;
	return nodeSet->nodeTab[0];
}

/*
 * This method returns the list of nodes in the xmlTree
 * matching the given xpath expression.
 *
 * Input: The node under which to search in the xml tree
 *  The xpath Expression.
 *
 * Output:  The matching nodeset is returned
 *          The third argument will to return the path object which needs to
 *          be cleaned up the user of the API
 *
 *
 */
xmlNodeSetPtr
GetMatchingNodeSet(xmlNodePtr node, const char *xpathExpr,xmlXPathObjectPtr *retPath) {
	xmlNodeSetPtr nodeSet = NULL;
	nodeSet = getNodeSetUnderNode(node->doc, node, (xmlChar *)xpathExpr,retPath);
	return nodeSet;
}

/*
 * This method prints the output of all the nodes in a list.
 * Used for debugging only.
 */
void
printNodeSet(xmlNodeSetPtr nodeSet, xmlDocPtr wsdlDoc) {
	int i;
	xmlNodePtr node = NULL;
	for (i=0; i < nodeSet->nodeNr; i++)
		{
		node = nodeSet->nodeTab[i];
		dumpNode(node, wsdlDoc);
	}
}

/*
 * This method dumps the output of the xmlNode in string format
 * This output includes all its attributes and its children
 * Input - The node to be printed and its DocPtr
 * Output - The string format of the output
 */
void dumpNode(xmlNodePtr node, xmlDocPtr docptr) {
	FILE *fp = NULL;
	fp = fopen("/var/tmp/a.xml", "w+");
	xmlIndentTreeOutput = 1;
#if 0
	xmlBufferPtr xmlbufPtr = NULL;
	xmlbufPtr = xmlBufferCreate();
	xmlNodeDump(xmlbufPtr, docptr, node, 1, 1);
	xmlBufferDump(fp, xmlbufPtr);
#endif
	xmlDocFormatDump(fp, docptr, 1);
	fclose(fp);
}


xmlNode *
find_element_by_name(xmlNode * a_node, const char *name)
{
	xmlNode *cur_node = NULL;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE &&
				(!xmlStrcmp(cur_node->name,(const xmlChar *) name))) {
			return cur_node;
		}
	}
	return NULL;
}
/* This function does both breadth and depth search from the input node
 * for a node whose name matches the i/p param. "name".
 * INPUT:
 * ~~~~~~
 * root - the starting node used to search for the specified node.
 * name - name of the node to search.
 *
 * OUTPUT:
 * ~~~~~~~
 * The matching node, if found. Otherwise, NULL.
 */
xmlNodePtr SearchNode (xmlNodePtr root, xmlChar * name)
{
	xmlNodePtr cur = root, res = NULL;
	if (root == NULL)
		return NULL;

	if ((xmlStrcasecmp (cur->name, name) == 0) && (xmlNodeIsText(cur) != 1))
		return cur;
	else
	{
		res = SearchNode (cur->next, name);
		if (res != NULL)
			return res;
	}

	return (SearchNode (root->children, name));
	return NULL;
}

