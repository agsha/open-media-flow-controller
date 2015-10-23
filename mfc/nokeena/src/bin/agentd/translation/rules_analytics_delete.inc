TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsanalyticscache-ingestsize-threshold*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/am/config/cache_ingest_size_threshold",
	    TYPE_UINT32,
	    ND_NORMAL,
	    0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsanalyticscache-promotion*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/am/config/cache_promotion_enabled",
            TYPE_BOOL,
            ND_HARDCODED,
            0,"true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsanalyticscache-promotionenable*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/am/config/cache_promotion_enabled",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsanalyticscache-promotiondisable*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/am/config/cache_promotion_enabled",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsanalyticscache-promotionhotness-threshold*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/am/config/cache_promotion/hotness_threshold",
	    TYPE_UINT32,
	    ND_NORMAL,
	    0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsanalyticsmemory-limit*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/am/config/memory_limit",
	    TYPE_UINT32,
	    ND_NORMAL,
	    0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsanalyticscache-ingestobject-timeout*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/am/config/cache_ingest_object_timeout",
	    TYPE_UINT32,
	    ND_NORMAL,
	    0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsanalyticscache-ingestqueue-depth*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/am/config/cache_ingest_queue_depth",
	    TYPE_UINT32,
	    ND_NORMAL,
	    0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsanalyticscache-ingestpolicy*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/am/config/cache_ingest_policy",
	    TYPE_UINT32,
	    ND_NORMAL,
	    0,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END
