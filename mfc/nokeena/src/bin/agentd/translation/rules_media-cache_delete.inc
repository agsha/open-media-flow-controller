TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheevict-thread-frequency*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/evict_thread_freq",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachefree-block-thresholdsas*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/free-block/threshold/internal/sas",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachefree-block-thresholdsata*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/free-block/threshold/internal/sata",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachefree-block-thresholdssd*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/free-block/threshold/internal/ssd",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheblock-sharing-thresholdsas*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/sas",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheblock-sharing-thresholdsata*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/sata",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheblock-sharing-thresholdssd*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/ssd",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachedisk-globalsrate-limit*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/cmd/rate_limit", /* DONT reset to default value of true. Set it to false */
                 TYPE_BOOL,
                 ND_HARDCODED,
                 0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachedisk-globalsdictionarypre-load*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/pre-load/enable",  /* DONT reset to default value of true. Set it to false */
                  TYPE_BOOL,
                  ND_HARDCODED,
                  0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachedisk-globalsgroup-readsata*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/nkn/nvsd/diskcache/config/sata/group-read/enable",  /* DONT reset to default value of true. Set it to false */
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false",
            0,0,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachedisk-globalsgroup-readsas*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/nkn/nvsd/diskcache/config/sas/group-read/enable",  /* DONT reset to default value of true. Set it to false */
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false",
            0,0,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachedisk-globalsgroup-readssd*")
TRANSL_NUM_NDS(1)
TRANSL_NDS("/nkn/nvsd/diskcache/config/ssd/group-read/enable",  /* DONT reset to default value of true. Set it to false */
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false",
            0,0,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachecache-tieradmission-thresholdsas*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/diskcache/config/sas_adm_thres",
	    TYPE_UINT32,
	    ND_NORMAL,
	    1,NULL,
	    0,0,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachecache-tieradmission-thresholdsata*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/diskcache/config/sata_adm_thres",
	    TYPE_UINT32,
	    ND_NORMAL,
	    1,NULL,
	    0,0,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cachecache-tieradmission-thresholdssd*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/diskcache/config/ssd_adm_thres",
	    TYPE_UINT32,
	    ND_NORMAL,
	    1,NULL,
	    0,0,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheevict-thread-frequency*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/evict_thread_freq",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachefree-block-thresholdsas*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/free-block/threshold/internal/sas",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachefree-block-thresholdsata*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/free-block/threshold/internal/sata",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachefree-block-thresholdssd*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/free-block/threshold/internal/ssd",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheblock-sharing-thresholdsas*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/sas",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheblock-sharing-thresholdsata*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/sata",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheblock-sharing-thresholdssd*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/block-sharing/threshold/internal/ssd",
                  TYPE_INT8,
                  ND_NORMAL,
                  1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachedisk-globalsrate-limit*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/cmd/rate_limit",
                 TYPE_BOOL,
                 ND_HARDCODED,
                 0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachedisk-globalsdictionarypre-load*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/diskcache/config/pre-load/enable",
                  TYPE_BOOL,
                  ND_HARDCODED,
                  0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachedisk-globalsgroup-readsata*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/diskcache/config/sata/group-read/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true",
	    0, 0, 0, 0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachedisk-globalsgroup-readsas*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/diskcache/config/sas/group-read/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true",
	    0, 0, 0, 0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachedisk-globalsgroup-readssd*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/diskcache/config/ssd/group-read/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true",
	    0, 0, 0, 0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachecache-tieradmission-thresholdsas*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/diskcache/config/sas_adm_thres",
	    TYPE_UINT32,
	    ND_NORMAL,
	    1,NULL,
	    0,0,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachecache-tieradmission-thresholdsata*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/diskcache/config/sata_adm_thres",
	    TYPE_UINT32,
	    ND_NORMAL,
	    1,NULL,
	    0,0,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cachecache-tieradmission-thresholdssd*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/diskcache/config/ssd_adm_thres",
	    TYPE_UINT32,
	    ND_NORMAL,
	    1,NULL,
	    0,0,0,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheinternal-evictionsaswatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/sas/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheinternal-evictionsatawatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/sata/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheinternal-evictionssdwatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/ssd/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheinternal-evictionsaswatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/sas/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheinternal-evictionsatawatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/sata/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheinternal-evictionssdwatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/ssd/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheinternal-evictionsaswatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/sas/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheinternal-evictionsatawatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/sata/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheinternal-evictionssdwatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/ssd/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheinternal-evictionsaswatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/sas/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheinternal-evictionsatawatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/sata/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheinternal-evictionssdwatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/internal/ssd/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheexternal-evictionsaswatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/sas/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheexternal-evictionsatawatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/sata/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheexternal-evictionssdwatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/ssd/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheexternal-evictionsaswatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/sas/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheexternal-evictionsatawatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/sata/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typevxa-standalonemedia-cacheexternal-evictionssdwatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/ssd/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheexternal-evictionsaswatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/sas/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheexternal-evictionsatawatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/sata/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheexternal-evictionssdwatermarkhigh*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/ssd/high",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheexternal-evictionsaswatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/sas/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheexternal-evictionsatawatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/sata/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"member-node*typeblademedia-cacheexternal-evictionssdwatermarklow*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/diskcache/config/watermark/external/ssd/low",
                        TYPE_UINT16,
                        ND_NORMAL,
                        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

