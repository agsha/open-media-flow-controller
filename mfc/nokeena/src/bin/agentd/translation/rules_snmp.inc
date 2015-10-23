TRANSL_ENTRY(PREPEND_TRANS_STR"systemmonitoringsnmp-serverhost*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/snmp/trapsink/sink/%s",
	        TYPE_HOSTNAME,
		ND_NORMAL,
		0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemmonitoringsnmp-serverhost*enable*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/snmp/trapsink/sink/%s/enable",
    TYPE_BOOL,
    ND_HARDCODED,
    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemmonitoringsnmp-serverhost*disable*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/snmp/trapsink/sink/%s/enable",
    TYPE_BOOL,
    ND_HARDCODED,
    0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

/*Host trap version*/
TRANSL_ENTRY(PREPEND_TRANS_STR"systemmonitoringsnmp-serverhost*trapsversion*")
TRANSL_CUST(agentd_snmp_set_trap_version, "/snmp/trapsink/sink")
TRANSL_END_CUST
TRANSL_ENTRY_END

/*Host trap community*/
TRANSL_ENTRY(PREPEND_TRANS_STR"systemmonitoringsnmp-serverhost*trapscommunity-string*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/snmp/trapsink/sink/%s/community",
	        TYPE_STR,
		ND_NORMAL,
		1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END


/*server location*/
TRANSL_ENTRY(PREPEND_TRANS_STR"systemmonitoringsnmp-serverlocation*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/snmp/syslocation",
	        TYPE_STR,
		ND_NORMAL,
		0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

/*server contact details*/
TRANSL_ENTRY(PREPEND_TRANS_STR"systemmonitoringsnmp-serversyscontact*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/snmp/syscontact",
	        TYPE_STR,
		ND_NORMAL,
		0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END


/*interface*/
TRANSL_ENTRY(PREPEND_TRANS_STR"systemmonitoringsnmp-serverlisteninterface*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/snmp/listen/interface/%s",
	        TYPE_STR,
		ND_NORMAL,
		0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemmonitoringsnmp-serverlistendisable*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/snmp/listen/enable",
    TYPE_BOOL,
    ND_HARDCODED,
    0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END


