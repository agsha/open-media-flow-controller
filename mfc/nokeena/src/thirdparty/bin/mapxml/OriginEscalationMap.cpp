//
// OriginEscalationMap_handler() 
//
// Convert XML to internal representation for consumption by nvsd as
// defined by "origin_escalation_map_bin_t" (origin_escalation_map_bin.h).
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
#include "origin_escalation_map_bin.h"

extern int opt_verbose;

#if 0
////////////////////////////////////////////////////////////////////////////////
// Sample XML data
////////////////////////////////////////////////////////////////////////////////
<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE OriginEscalationMap SYSTEM "OriginEscalationMap.dtd">
<OriginEscalationMap>
    <Header>
        <Version>1.0</Version>
        <Application>MapXML</Application>
    </Header>
    <OriginEscalationMapEntry>
        <Origin>dest.xxx.com</Origin>
        <Port>80</Port>
        <Options>heartbeatpath=/hello.html,weight=1, http_response_failure_codes=404;500</Options>
    </OriginEscalationMapEntry>
    <OriginEscalationMapEntry>
        <Origin>dest.yyy.com</Origin>
        <Port>81</Port>
        <Options>heartbeatpath=/hello2.html,weight=2, http_response_failure_codes=404;500</Options>
    </OriginEscalationMapEntry>
    <OriginEscalationMapEntry>
        <Origin>dest.zzz.com</Origin>
        <Port>82</Port>
        <Options>heartbeatpath=/hello3.html,weight=3, http_response_failure_codes=404;500;503</Options>
    </OriginEscalationMapEntry>
</OriginEscalationMap>
////////////////////////////////////////////////////////////////////////////////
#endif

#define ROOT_TAG "OriginEscalationMap"

#define HEAD_TAG "Header"
#define HEAD_VERS_TAG "Version"
#define HEAD_APP_TAG "Application"

#define ENTRY_TAG "OriginEscalationMapEntry"
#define ENTRY_ORIGIN_TAG "Origin"
#define ENTRY_PORT_TAG "Port"
#define ENTRY_OPTIONS_TAG "Options"

#define ORIGINESCALATIONMAP_VERSION "1.0"
#define ORIGINESCALATIONMAP_APPLICATION "MapXML"

int OriginEscalationMap_handler(const char *input, const char *output)
{
    XMLNode PNode;
    XMLNode Node;

    int rv;
    int ret = 0;
    int entries;
    const char* version;
    const char* application;
    const char* origin;
    const char* port;
    const char* options;

    origin_escalation_map_bin_t* oem = 0;
    origin_escalationmap_t* originescalationmap;

    char* str_table = 0;
    int str_table_size = 0;
    int str_table_bytes_used = 0;
    istr_ix_t index;
    int bytes_to_alloc;

    PNode = XMLNode::openFileHelper(input, ROOT_TAG);
    Node = PNode.getChildNode(HEAD_TAG);

    version = Node.getChildNode(HEAD_VERS_TAG).getText();
    if (!version || strcmp(ORIGINESCALATIONMAP_VERSION, version)) {
    	printf("Invalid '%s' version=%s, expected version=%s\n", 
	       ROOT_TAG, version, ORIGINESCALATIONMAP_VERSION);
	ret = 1;
	goto exit;
    }

    application = Node.getChildNode(HEAD_APP_TAG).getText();
    if (!application || strcmp(ORIGINESCALATIONMAP_APPLICATION, application)) {
    	printf("Invalid '%s' application=%s, expected application=%s\n", 
	       ROOT_TAG, application, ORIGINESCALATIONMAP_APPLICATION);
	ret = 2;
	goto exit;
    }

    entries = PNode.nChildNode(ENTRY_TAG);
    if (!entries) {
    	printf("No entries for '%s'\n", ROOT_TAG);
	ret = 3;
	goto exit;
    }

    bytes_to_alloc = sizeof(origin_escalation_map_bin_t) +
    	(sizeof(origin_escalationmap_t) * (entries-1));
    oem = (origin_escalation_map_bin_t*)calloc(1, bytes_to_alloc);
    if (!oem) {
    	printf("calloc() failed, size=%d\n", bytes_to_alloc);
	ret = 4;
	goto exit;
    }
    originescalationmap = &oem->entry[0];

    for (int n = 0; n < entries; n++) {
        // <Origin>
    	origin = PNode.getChildNode(ENTRY_TAG, n).getChildNode(ENTRY_ORIGIN_TAG).getText();
	if (!origin) {
	    printf("Null origin, entry=%d\n", n);
	    ret = 5;
	    goto exit;
	}
	rv = add_str_table(&str_table, &str_table_size, &str_table_bytes_used, 
		      	   &index, origin);
	if (rv) {
	    printf("add_str_table() failed, rv=%d "
	    	   "str_table=%p str_table_size=%d "
	    	   "str_table_bytes_used=%d index=%d origin=%s\n",
		   rv, str_table, str_table_size, str_table_bytes_used, 
		   index, origin);
	    ret = 6;
	    goto exit;
	}
	originescalationmap[n].origin = index;

        // <Port>
    	port = PNode.getChildNode(ENTRY_TAG, n).getChildNode(ENTRY_PORT_TAG).getText();
	if (!port) {
	    printf("Null Port, entry=%d\n", n);
	    ret = 7;
	    goto exit;
	}
	originescalationmap[n].port = atoi(port);
	if (!originescalationmap[n].port) {
	    printf("Invalid port=%s, entry=%d\n", port, n);
	    ret = 8;
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
	    ret = 9;
	    goto exit;
	}
	originescalationmap[n].options = index;

	if (opt_verbose) {
	    printf("Entry[%d]: origin=%s port=%s options=[%s]\n",
	    	   n, origin, port, options);
	}
    }

    // Write data to output file

    oem->version = OREM_VERSION;
    oem->magicno = OREM_MAGICNO;
    oem->string_table_offset = sizeof(origin_escalation_map_bin_t) + 
    	(sizeof(origin_escalationmap_t) * (entries-1));
    oem->string_table_length = str_table_bytes_used;
    oem->num_entries = entries;

    rv = write_binary_file(output, 
			   (const char*)oem, oem->string_table_offset,
			   (const char*)str_table, oem->string_table_length);
    if (rv) {
    	printf("output_binary_file() failed, rv=%d "
	       "meta_data=%p meta_datalen=%d str_table=%p str_tablelen=%d\n",
	       rv, oem, oem->string_table_offset, 
	       str_table, oem->string_table_length);
	ret = 10;
	goto exit;
    }

    if (opt_verbose) {
    	printf("Output to [%s] complete, bytes_written=%d entries=%d "
	       "string_table_offset=%d 0x%x string_table_length=%d 0x%x\n",
	       output,
	       oem->string_table_offset + oem->string_table_length,
	       oem->num_entries,
	       oem->string_table_offset, oem->string_table_offset,
	       oem->string_table_length, oem->string_table_length);
    }

exit:

    if (oem) {
    	free(oem);
    }
    if (str_table) {
    	free(str_table);
    }
    return ret;
}

//
// End of OriginEscalationMap.cpp
//
