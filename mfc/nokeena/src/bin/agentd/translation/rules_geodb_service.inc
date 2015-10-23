TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*client-requestgeo-servicegeo-service-provider*")
TRANSL_NUM_NDS(2)
TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/geo-service/service",
               TYPE_STR,
               ND_NORMAL,
               1, NULL)
TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/geo-service/api-url",
               TYPE_STR,
               ND_HARDCODED,
               0, "")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*client-requestgeo-servicefailover-mode-bypass*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/geo-service/failover-mode",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"servicehttpinstance*client-requestgeo-servicelookup-timeout*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/geo-service/lookup-timeout",
                TYPE_UINT32,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END
