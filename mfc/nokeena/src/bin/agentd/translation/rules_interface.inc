TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladefpc*")
        TRANSL_CUST(agentd_custom_entry_ignore, "/net/interface/config")
        TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladepic*")
        TRANSL_CUST(agentd_custom_entry_ignore, "/net/interface/config")
        TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY("deletemember-node*typebladefpc*")
        TRANSL_CUST(agentd_custom_entry_ignore, "/net/interface/config")
        TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY("deletemember-node*typebladepic*")
        TRANSL_CUST(agentd_custom_entry_ignore, "/net/interface/config")
        TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*")
TRANSL_CUST(agentd_configure_interface, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*")
TRANSL_CUST(agentd_configure_interface, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*dhcp*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/addr/ipv4/dhcp",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true",
	    1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*dhcp*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/addr/ipv4/dhcp",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true",
	    1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*mtu*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/mtu",
            TYPE_UINT16,
            ND_NORMAL,
            2,NULL,
	    1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*mtu*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/mtu",
            TYPE_UINT16,
            ND_NORMAL,
            2,NULL,
            1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*speed*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/type/ethernet/speed",
            TYPE_STR,
            ND_NORMAL,
            2,NULL,
	    1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*speed*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/type/ethernet/speed",
            TYPE_STR,
            ND_NORMAL,
            2,NULL,
            1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*duplex*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/type/ethernet/duplex",
            TYPE_STR,
            ND_NORMAL,
            2,NULL, 
	    1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*duplex*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/type/ethernet/duplex",
            TYPE_STR,
            ND_NORMAL,
            2,NULL,
            1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*no-arp*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/arp",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false",
            1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*no-arp*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/arp",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false",
            1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*shutdown*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            1, "false",
            1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*shutdown*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            1, "false",
            1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*alias*no-arp*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s:%s/arp",
	TYPE_BOOL,
	ND_HARDCODED,
	0, "false",
	2,1,2,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*alias*no-arp*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s:%s/arp",
        TYPE_BOOL,
        ND_HARDCODED,
        0, "false",
        2,1,2,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*ipv6disable*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/addr/ipv6/enable",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "false",
		1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*ipv6disable*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/net/interface/config/%s/addr/ipv6/enable",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "false",
		1,1,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typevxa-standaloneinterface*ipv6address*")
TRANSL_CUST(agentd_set_ipv6_addr, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"member-node*typebladeinterface*ipv6address*")
TRANSL_CUST(agentd_set_ipv6_addr, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

