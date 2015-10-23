/*
 * Filename :   rules_origin_map.inc
 * Date:        08 Dec 2011
 * Author:      Vijayekkumaran M
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*")
TRANSL_CUST(agentd_origin_map_set_server_map_nodes, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*precedence*")
TRANSL_CUST(agentd_custom_entry_ignore, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashhash-input*")
TRANSL_NUM_NDS(2)
TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/type",
                   TYPE_UINT8,
                   ND_HARDCODED,
                   0,"10")
TRANSL_NDS_EXT_VAL("/nkn/nvsd/namespace/%s/cluster-hash",
			TYPE_UINT32,
	                ND_NORMAL,
	                2, NULL)
	TRANSL_NDS_VAL_MAP_START(2)
	    TRANSL_NDS_VAL_MAP(0,"base-url","1")
	    TRANSL_NDS_VAL_MAP(1,"complete-url","2")
	TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashmembers*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/format-type",
			TYPE_UINT32,
			ND_HARDCODED,
			0, "2",
                        2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashmembers*ipaddress*")
TRANSL_CUST(agentd_origin_map_set, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashmembers*port*")
TRANSL_CUST(agentd_origin_map_set, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashmembers*heart-beat-path*")
TRANSL_CUST(agentd_origin_map_set, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashnode-monitoringconnect-timeout*")
TRANSL_NUM_NDS(1)					     // Number of Nodes to be set
TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/connect-timeout",      // Node to be set
				TYPE_UINT32,
        	                ND_NORMAL,
        	                2,NULL,
        	                2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashnode-monitoringheartbeat-interval*")
TRANSL_NUM_NDS(1)					     // Number of Nodes to be set
TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/interval",      // Node to be set
		TYPE_UINT32,
		ND_NORMAL,
		2,NULL,
		2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashnode-monitoringread-timeout*")
TRANSL_NUM_NDS(1)					     // Number of Nodes to be set
TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/read-timeout",      // Node to be set
		TYPE_UINT32,
		ND_NORMAL,
		2,NULL,
		2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashnode-monitoringallowed-fails*")
TRANSL_NUM_NDS(1)					     // Number of Nodes to be set
TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/allowed-fails",      // Node to be set
		TYPE_UINT32,
        	ND_NORMAL,
        	2,NULL,
        	2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationnode-monitoringconnect-timeout*")
TRANSL_NUM_NDS(1)					     // Number of Nodes to be set
TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/connect-timeout",      // Node to be set
		TYPE_UINT32,
                ND_NORMAL,
        	2,NULL,
        	2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationnode-monitoringheartbeat-interval*")
TRANSL_NUM_NDS(1)					     // Number of Nodes to be set
TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/interval",      // Node to be set
		TYPE_UINT32,
		ND_NORMAL,
		2,NULL,
		2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationnode-monitoringread-timeout*")
TRANSL_NUM_NDS(1)					     // Number of Nodes to be set
TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/read-timeout",      // Node to be set
		TYPE_UINT32,
		ND_NORMAL,
		2,NULL,
		2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationnode-monitoringallowed-fails*")
TRANSL_NUM_NDS(1)					     // Number of Nodes to be set
TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/allowed-fails",      // Node to be set
		TYPE_UINT32,
        	ND_NORMAL,
        	2,NULL,
        	2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/format-type",
                        TYPE_UINT32,
                        ND_HARDCODED,
                        0, "3",
                        2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*ipaddress*")
TRANSL_CUST(agentd_origin_map_set, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*port*")
TRANSL_CUST(agentd_origin_map_set, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*heart-beat-path*")
TRANSL_CUST(agentd_origin_map_set, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*order*")
TRANSL_CUST(agentd_origin_map_set, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*http-failure-codes*")
TRANSL_CUST(agentd_origin_map_set, "")
TRANSL_END_CUST
TRANSL_ENTRY_END
