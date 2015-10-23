//
// HostOriginMap_handler() 
//
// Convert XML to internal representation for consumption by nvsd as
// defined by "host_origin_map_bin_t" (host_origin_map_bin.h).
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
#include "host_origin_map_bin.h"

extern int opt_verbose;

#if 0
////////////////////////////////////////////////////////////////////////////////
// Sample XML data
////////////////////////////////////////////////////////////////////////////////
<?xml version="1.0" encoding="ISO-8859-1"?> 
<!DOCTYPE HostOriginMap SYSTEM "HostOriginMap.dtd">
<HostOriginMap>
    <Header>
	<Version>1.0</Version>
	<Application>MapXML</Application>
    </Header>
    <HostOriginEntry>
	<Host>host1.xxx.yyy.com</Host>
	<Origin>host1.com</Origin>
	<Port>80</Port>
    </HostOriginEntry>
    <HostOriginEntry>
	<Host>host2.aaa.bbb.com</Host>
	<Origin>host2.com</Origin>
	<Port>81</Port>
    </HostOriginEntry>
    <HostOriginEntry>
	<Host>host3.ccc.ddd.com</Host>
	<Origin>host3.com</Origin>
	<Port>82</Port>
    </HostOriginEntry>
</HostOriginMap> 
////////////////////////////////////////////////////////////////////////////////
#endif

#define ROOT_TAG "HostOriginMap"

#define HEAD_TAG "Header"
#define HEAD_VERS_TAG "Version"
#define HEAD_APP_TAG "Application"

#define ENTRY_TAG "HostOriginEntry"
#define ENTRY_HOST_TAG "Host"
#define ENTRY_ORIGIN_TAG "Origin"
#define ENTRY_PORT_TAG "Port"
#define ENTRY_OPTIONS_TAG "Options"

#define HOSTORIGINMAP_VERSION "1.0"
#define HOSTORIGINMAP_APPLICATION "MapXML"

int HostOriginMap_handler(const char *input, const char *output)
{
    XMLNode PNode;
    XMLNode Node;

    int rv;
    int ret = 0;
    int entries;
    const char* version;
    const char* application;
    const char* host;
    const char* origin_host;
    const char* port;
    const char* options;

    host_origin_map_bin_t* hom = 0;
    host2origin_t* h2origin;

    char* str_table = 0;
    int str_table_size = 0;
    int str_table_bytes_used = 0;
    istr_ix_t index;

    PNode = XMLNode::openFileHelper(input, ROOT_TAG);
    Node = PNode.getChildNode(HEAD_TAG);

    version = Node.getChildNode(HEAD_VERS_TAG).getText();
    if (!version || strcmp(HOSTORIGINMAP_VERSION, version)) {
    	printf("Invalid '%s' version=%s, expected version=%s\n", 
	       ROOT_TAG, version, HOSTORIGINMAP_VERSION);
	ret = 1;
	goto exit;
    }

    application = Node.getChildNode(HEAD_APP_TAG).getText();
    if (!application || strcmp(HOSTORIGINMAP_APPLICATION, application)) {
    	printf("Invalid '%s' application=%s, expected application=%s\n", 
	       ROOT_TAG, application, HOSTORIGINMAP_APPLICATION);
	ret = 2;
	goto exit;
    }

    entries = PNode.nChildNode(ENTRY_TAG);
    if (!entries) {
    	printf("No entries for '%s'\n", ROOT_TAG);
	ret = 3;
	goto exit;
    }

    hom = (host_origin_map_bin_t*) 
    	calloc(1, sizeof(host_origin_map_bin_t) + 
    	       (sizeof(host2origin_t) * (entries-1)));
    if (!hom) {
    	printf("calloc() failed, size=%d\n", sizeof(host2origin_t) * entries);
	ret = 4;
	goto exit;
    }
    h2origin = &hom->entry[0];

    for (int n = 0; n < entries; n++) {
	// <Host> 
    	host = PNode.getChildNode(ENTRY_TAG, n).getChildNode(ENTRY_HOST_TAG).getText();
	if (!host) {
	    printf("Null host, entry=%d\n", n);
	    ret = 5;
	    goto exit;
	}
	rv = add_str_table(&str_table, &str_table_size, &str_table_bytes_used, 
		      	   &index, host);
	if (rv) {
	    printf("add_str_table() failed, rv=%d "
	    	   "str_table=%p str_table_size=%d "
	    	   "str_table_bytes_used=%d index=%d host=%s\n",
		   rv, str_table, str_table_size, str_table_bytes_used, 
		   index, host);
	    ret = 6;
	    goto exit;
	}
	h2origin[n].host = index;

	// <Origin> 
    	origin_host = PNode.getChildNode(ENTRY_TAG, n).getChildNode(ENTRY_ORIGIN_TAG).getText();
	if (!origin_host) {
	    printf("Null origin_host, entry=%d\n", n);
	    ret = 7;
	    goto exit;
	}
	rv = add_str_table(&str_table, &str_table_size, &str_table_bytes_used, 
		      	   &index, origin_host);
	if (rv) {
	    printf("add_str_table() failed, rv=%d "
	    	   "str_table=%p str_table_size=%d "
	    	   "str_table_bytes_used=%d index=%d origin_host=%s\n",
		   rv, str_table, str_table_size, str_table_bytes_used, 
		   index, origin_host);
	    ret= 8;
	    goto exit;
	}
	h2origin[n].origin_host = index;

	// <Port> 
    	port = PNode.getChildNode(ENTRY_TAG, n).getChildNode(ENTRY_PORT_TAG).getText();
	if (!port) {
	    printf("Null port, entry=%d\n", n);
	    ret = 9;
	    goto exit;
	}
	h2origin[n].origin_port = atoi(port);
	if (!h2origin[n].origin_port) {
	    printf("Invalid port=%s, entry=%d\n", port, n);
	    ret = 10;
	    goto exit;
	}

	// <Options> 
	options = "";
    	h2origin[n].options = 0;

	if (opt_verbose) {
	    printf("Entry[%d]: host=%s origin_host=%s origin_port=%s "
	    	   "options=%s\n", n, host, origin_host, port, options);
	}
    }

    // Write data to output file

    hom->version = HOM_VERSION;
    hom->magicno = HOM_MAGICNO;
    hom->string_table_offset = sizeof(host_origin_map_bin_t) +
    	(sizeof(host2origin_t) * (entries-1));
    hom->string_table_length = str_table_bytes_used;
    hom->num_entries = entries;

    rv = write_binary_file(output, 
			   (const char*)hom, hom->string_table_offset,
			   (const char*)str_table, hom->string_table_length);
    if (rv) {
    	printf("output_binary_file() failed, rv=%d "
	       "meta_data=%p meta_datalen=%d str_table=%p str_tablelen=%d\n",
	       rv, hom, hom->string_table_offset, 
	       str_table, hom->string_table_length);
	ret = 11;
	goto exit;
    }

    if (opt_verbose) {
    	printf("Output to [%s] complete, bytes_written=%d entries=%d "
	       "string_table_offset=%d 0x%x string_table_length=%d 0x%x\n",
	       output,
	       hom->string_table_offset + hom->string_table_length, 
	       hom->num_entries,
	       hom->string_table_offset, hom->string_table_offset,
	       hom->string_table_length, hom->string_table_length);
    }

exit:

    if (hom) {
    	free(hom);
    }
    if (str_table) {
    	free(str_table);
    }
    return ret;
}

//
// End of HostOriginMap.cpp
//
