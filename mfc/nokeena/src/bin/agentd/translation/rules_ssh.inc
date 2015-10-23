TRANSL_ENTRY(PREPEND_TRANS_STR"systemsshclientuser*authorized-keysshv2*")
TRANSL_CUST(agentd_add_ssh_client_authkey, "/ssh/server/username/%s/auth-key/sshv2")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemsshclientuser*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/ssh/server/username/%s",
        TYPE_STR,
        ND_NORMAL,
        0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemsshserverdisable*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/ssh/server/enable",
	TYPE_BOOL,
	ND_HARDCODED,
	0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemsshserverlistendisable*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/ssh/server/listen/enable",
	TYPE_BOOL,
	ND_HARDCODED,
	0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"systemsshserverlisteninterface*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/ssh/server/listen/interface/%s",
		TYPE_STR,
		ND_NORMAL,
		0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END
