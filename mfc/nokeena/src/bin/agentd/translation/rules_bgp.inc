TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsrouterbgp*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/bgp/config/local_as/%s",
		TYPE_STR,
		ND_NORMAL,
		0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsrouterbgp*router-id*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/bgp/config/local_as/%s/router_id",
		TYPE_STR,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsrouterbgp*neighbor*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/nkn/bgp/config/local_as/%s/neighbor/%s",
		TYPE_STR,
		ND_NORMAL,
		1, NULL,
                2, 0, 1, 0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsrouterbgp*neighbor*remote-as*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/nkn/bgp/config/local_as/%s/neighbor/%s/remote_as",
		TYPE_STR,
		ND_NORMAL,
		2, NULL,
                2, 0, 1, 0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsrouterbgp*networkipaddress*")
TRANSL_CUST(agentd_bgp_set_network, "/nkn/bgp/config/local_as/%s/network/%s/%s")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsrouterbgp*redistribute-connected*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/bgp/config/local_as/%s/redist_conn",
	    TYPE_STR,
	    ND_HARDCODED,
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsrouterbgp*keepalive*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/bgp/config/local_as/%s/timer_ka",
	    TYPE_STR,
	    ND_NORMAL,
	    1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsrouterbgp*holdtime*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/bgp/config/local_as/%s/timer_hold",
	    TYPE_STR,
	    ND_NORMAL,
	    1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END
