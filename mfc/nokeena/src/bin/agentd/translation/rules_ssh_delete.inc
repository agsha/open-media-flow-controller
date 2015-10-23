TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemsshclientuser*")
TRANSL_CUST (agentd_custom_entry_ignore, "")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemsshclientuser*authorized-keysshv2*")
TRANSL_CUST(agentd_delete_ssh_client_authkey, "/ssh/server/username/%s/auth-key/sshv2")
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemsshserverdisable*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/ssh/server/enable",
        TYPE_BOOL,
        ND_HARDCODED,
        0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemsshserverlistendisable*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/ssh/server/listen/enable",
        TYPE_BOOL,
        ND_HARDCODED,
        0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"systemsshserverlisteninterface*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS_BASIC("/ssh/server/listen/interface/%s",
                TYPE_STR,
                ND_NORMAL,
                0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

