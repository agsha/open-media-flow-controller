TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsresource-pool*")
    TRANSL_CUST(agentd_rp_delete, NULL)
    TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsresource-pool*instance-listinstance*")
    TRANSL_CUST(agentd_rp_delete_instance, NULL)
    TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsresource-pool*clientsession*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/resource_mgr/config/%s/client_sessions",
	    TYPE_UINT64,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsresource-pool*clientbandwidth*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/resource_mgr/config/%s/max_bandwidth",
	    TYPE_UINT64,
	    ND_NORMAL,
	    1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

