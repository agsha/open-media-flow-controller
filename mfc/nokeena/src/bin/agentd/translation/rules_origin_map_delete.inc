/*
 * Filename :   rules_origin_map.inc
 * Date:        08 Dec 2011
 * Author:      Vijayekkumaran M
 *
 * (C) Copyright 2011, Juniper Networks, Inc.
 * All rights reserved.
 *
 */
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*")
TRANSL_CUST(agentd_origin_map_delete,"")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*precedence*")
TRANSL_CUST(agentd_custom_entry_ignore, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashhash-input*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/cluster-hash",
		TYPE_UINT32,
	        ND_NORMAL,
	        1, NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/type",
                   TYPE_UINT8,
                   ND_HARDCODED,
                   0,"10")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashmembers*")
TRANSL_CUST(agentd_origin_map_members_deletion, "") //TODO need to check if this is fine
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashmembers*ipaddress*")
TRANSL_CUST(agentd_origin_map_members_deletion, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashmembers*port*")
TRANSL_CUST(agentd_origin_map_members_deletion, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashmembers*heart-beat-path*")
TRANSL_CUST(agentd_origin_map_members_deletion, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashnode-monitoringconnect-timeout*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/connect-timeout",
			TYPE_UINT32,
       	                ND_NORMAL,
       	                2,NULL,
                        2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashnode-monitoringheartbeat-interval*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/interval",
			TYPE_UINT32,
			ND_NORMAL,
			2,NULL,
                        2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashnode-monitoringread-timeout*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/read-timeout",
			TYPE_UINT32,
			ND_NORMAL,
			2,NULL,
                        2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*consistent-hashnode-monitoringallowed-fails*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/allowed-fails",
			TYPE_UINT32,
        	        ND_NORMAL,
        	        2,NULL,
                        2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationnode-monitoringconnect-timeout*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/connect-timeout",
			TYPE_UINT32,
        	        ND_NORMAL,
        	        2,NULL,
                        2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationnode-monitoringheartbeat-interval*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/interval",
			TYPE_UINT32,
			ND_NORMAL,
			2,NULL,
                        2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationnode-monitoringread-timeout*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/read-timeout",
			TYPE_UINT32,
			ND_NORMAL,
			2,NULL,
                        2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationnode-monitoringallowed-fails*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/server-map/config/%s_%s/node-monitoring/heartbeat/allowed-fails",
			TYPE_UINT32,
                        ND_NORMAL,
        		2,NULL,
                        2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*")
TRANSL_CUST(agentd_origin_map_members_deletion, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*ipaddress*")
TRANSL_CUST(agentd_origin_map_members_deletion, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*port*")
TRANSL_CUST(agentd_origin_map_members_deletion, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*heart-beat-path*")
TRANSL_CUST(agentd_origin_map_members_deletion, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*order*")
TRANSL_CUST(agentd_origin_map_members_deletion, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverorigin-map*origin-escalationmembers*http-failure-codes*")
TRANSL_CUST(agentd_origin_map_members_deletion, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

