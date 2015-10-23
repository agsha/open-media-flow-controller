//
// ClusterMap_handler() 
//
// Convert XML to internal representation for consumption by nvsd as
// defined by "cluster_map_bin_t" (cluster_map_bin.h).
//
// Copyright (c) 2014 by Juniper Networks, Inc
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "mapxml_utils.h"
#include "xmlparser/xmlParser.h"
#include "cluster_map_bin.h"

extern int opt_verbose;

#if 0
////////////////////////////////////////////////////////////////////////////////
// Sample XML data
////////////////////////////////////////////////////////////////////////////////
<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE ClusterMap SYSTEM "ClusterMap.dtd">
<ClusterMap>
    <Header>
        <Version>1.0</Version>
        <Application>MapXML</Application>
    </Header>
    <ClusterMapEntry>
        <Node>NodeName1</Node>
        <IP>172.19.172.52</IP>
        <Port>80</Port>
        <Options>heartbeatpath=/root/heartbeat.html</Options>
    </ClusterMapEntry>
    <ClusterMapEntry>
        <Node>NodeName2</Node>
        <IP>172.19.172.53</IP>
        <Port>81</Port>
        <Options>heartbeatpath=/root/heartbeat.html</Options>
    </ClusterMapEntry>
</ClusterMap>
////////////////////////////////////////////////////////////////////////////////
#endif

#define ROOT_TAG "ClusterMap"

#define HEAD_TAG "Header"
#define HEAD_VERS_TAG "Version"
#define HEAD_APP_TAG "Application"

#define ENTRY_TAG "ClusterMapEntry"
#define ENTRY_NODE_TAG "Node"
#define ENTRY_IP_TAG "IP"
#define ENTRY_PORT_TAG "Port"
#define ENTRY_OPTIONS_TAG "Options"

#define CLUSTERMAP_VERSION "1.0"
#define CLUSTERMAP_APPLICATION "MapXML"

int ClusterMap_handler(const char *input, const char *output)
{
    XMLNode PNode;
    XMLNode Node;

    int rv;
    int ret = 0;
    int entries;
    const char* version;
    const char* application;
    const char* node;
    const char* ip;
    const char* port;
    const char* options;

    cluster_map_bin_t* clmb = 0;
    clustermap_t* clustermap;

    char* str_table = 0;
    int str_table_size = 0;
    int str_table_bytes_used = 0;
    istr_ix_t index;
    int bytes_to_alloc;

    PNode = XMLNode::openFileHelper(input, ROOT_TAG);
    Node = PNode.getChildNode(HEAD_TAG);

    version = Node.getChildNode(HEAD_VERS_TAG).getText();
    if (!version || strcmp(CLUSTERMAP_VERSION, version)) {
    	printf("Invalid '%s' version=%s, expected version=%s\n", 
	       ROOT_TAG, version, CLUSTERMAP_VERSION);
	ret = 1;
	goto exit;
    }

    application = Node.getChildNode(HEAD_APP_TAG).getText();
    if (!application || strcmp(CLUSTERMAP_APPLICATION, application)) {
    	printf("Invalid '%s' application=%s, expected application=%s\n", 
	       ROOT_TAG, application, CLUSTERMAP_APPLICATION);
	ret = 2;
	goto exit;
    }

    entries = PNode.nChildNode(ENTRY_TAG);
    if (!entries) {
    	printf("No entries for '%s'\n", ROOT_TAG);
	ret = 3;
	goto exit;
    }

    bytes_to_alloc = sizeof(cluster_map_bin_t) +
    	(sizeof(clustermap_t) * (entries-1));
    clmb = (cluster_map_bin_t*)calloc(1, bytes_to_alloc);
    if (!clmb) {
    	printf("calloc() failed, size=%d\n", bytes_to_alloc);
	ret = 4;
	goto exit;
    }
    clustermap = &clmb->entry[0];

    for (int n = 0; n < entries; n++) {
        // <Node>
    	node = PNode.getChildNode(ENTRY_TAG, n).getChildNode(ENTRY_NODE_TAG).getText();
	if (!node) {
	    printf("Null node, entry=%d\n", n);
	    ret = 5;
	    goto exit;
	}
	rv = add_str_table(&str_table, &str_table_size, &str_table_bytes_used, 
		      	   &index, node);
	if (rv) {
	    printf("add_str_table() failed, rv=%d "
	    	   "str_table=%p str_table_size=%d "
	    	   "str_table_bytes_used=%d index=%d node=%s\n",
		   rv, str_table, str_table_size, str_table_bytes_used, 
		   index, node);
	    ret = 6;
	    goto exit;
	}
	clustermap[n].nodename = index;

        // <IP>
    	ip = PNode.getChildNode(ENTRY_TAG, n).getChildNode(ENTRY_IP_TAG).getText();
	if (!ip) {
	    printf("Null IP, entry=%d\n", n);
	    ret = 7;
	    goto exit;
	}
	rv = add_str_table(&str_table, &str_table_size, &str_table_bytes_used, 
		      	   &index, ip);
	if (rv) {
	    printf("add_str_table() failed, rv=%d "
	    	   "str_table=%p str_table_size=%d "
	    	   "str_table_bytes_used=%d index=%d ip=%s\n",
		   rv, str_table, str_table_size, str_table_bytes_used, 
		   index, ip);
	    ret = 8;
	    goto exit;
	}
	clustermap[n].ipaddr = index;

        // <Port>
    	port = PNode.getChildNode(ENTRY_TAG, n).getChildNode(ENTRY_PORT_TAG).getText();
	if (!port) {
	    printf("Null Port, entry=%d\n", n);
	    ret = 9;
	    goto exit;
	}
	clustermap[n].port = atoi(port);
	if (!clustermap[n].port) {
	    printf("Invalid port=%s, entry=%d\n", port, n);
	    ret = 10;
	    goto exit;
	}

        // <Options>
    	options = PNode.getChildNode(ENTRY_TAG, n).getChildNode(ENTRY_OPTIONS_TAG).getText();
	if (!options) {
	    options = "";
	}
	rv = add_str_table(&str_table, &str_table_size, &str_table_bytes_used, 
		      	   &index, options);
	if (rv) {
	    printf("add_str_table() failed, rv=%d "
	    	   "str_table=%p str_table_size=%d "
	    	   "str_table_bytes_used=%d index=%d options=%s\n",
		   rv, str_table, str_table_size, str_table_bytes_used, 
		   index, options);
	    ret = 11;
	    goto exit;
	}
	clustermap[n].options = index;

	if (opt_verbose) {
	    printf("Entry[%d]: node=%s ip=%s port=%s options=[%s]\n",
	    	   n, node, ip, port, options);
	}
    }

    // Write data to output file

    clmb->version = CLM_VERSION;
    clmb->magicno = CLM_MAGICNO;
    clmb->string_table_offset = sizeof(cluster_map_bin_t) + 
    	(sizeof(clustermap_t) * (entries-1));
    clmb->string_table_length = str_table_bytes_used;
    clmb->num_entries = entries;

    rv = write_binary_file(output, 
			   (const char*)clmb, clmb->string_table_offset,
			   (const char*)str_table, clmb->string_table_length);
    if (rv) {
    	printf("output_binary_file() failed, rv=%d "
	       "meta_data=%p meta_datalen=%d str_table=%p str_tablelen=%d\n",
	       rv, clmb, clmb->string_table_offset, 
	       str_table, clmb->string_table_length);
	ret = 12;
	goto exit;
    }

    if (opt_verbose) {
    	printf("Output to [%s] complete, bytes_written=%d entries=%d "
	       "string_table_offset=%d 0x%x string_table_length=%d 0x%x\n",
	       output,
	       clmb->string_table_offset + clmb->string_table_length,
	       clmb->num_entries,
	       clmb->string_table_offset, clmb->string_table_offset,
	       clmb->string_table_length, clmb->string_table_length);
    }

exit:

    if (clmb) {
    	free(clmb);
    }
    if (str_table) {
    	free(str_table);
    }
    return ret;
}

//
// End of ClusterMap.cpp
//
