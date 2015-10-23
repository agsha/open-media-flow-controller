TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsresource-pool*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/resource_mgr/config/%s",
	    TYPE_STR,
	    ND_NORMAL,
	    0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsresource-pool*instance-listinstance*")
TRANSL_NUM_NDS(2)
TRANSL_NDS("/nkn/nvsd/resource_mgr/config/%s/namespace/%s",
	    TYPE_STR,
	    ND_NORMAL,
            1,NULL,
	    2,0,1,0)
TRANSL_NDS("/nkn/nvsd/namespace/%s/resource_pool",
	    TYPE_STR,
	    ND_NORMAL,
	    0,NULL,
            1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsresource-pool*clientsession*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/resource_mgr/config/%s/client_sessions",
	    TYPE_UINT64,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsresource-pool*clientbandwidth*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/resource_mgr/config/%s/max_bandwidth",
	    TYPE_UINT64,
	    ND_NORMAL,
	    1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

