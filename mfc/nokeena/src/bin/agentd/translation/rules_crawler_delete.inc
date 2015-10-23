TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS_BASIC("/nkn/crawler/config/profile/%s",
		TYPE_STR,
		ND_NORMAL,
		0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*origin-serverurl*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/crawler/config/profile/%s/url",
		TYPE_STR,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*schedulestart*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/crawler/config/profile/%s/schedule/start",
		TYPE_DATETIME_SEC,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*schedulestop*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/crawler/config/profile/%s/schedule/stop",
                TYPE_DATETIME_SEC,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*schedulerefresh-interval*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/crawler/config/profile/%s/schedule/refresh",
		TYPE_UINT32,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*link-depth*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/crawler/config/profile/%s/link_depth",
		TYPE_UINT8,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*accept-file-extension*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS("/nkn/crawler/config/profile/%s/file_extension/%s",
		TYPE_STR,
		ND_NORMAL,
		1, NULL,
		2,0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*accept-file-extension*skip-preload*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/crawler/config/profile/%s/file_extension/%s/skip_preload",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true",
                2,0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*active*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/crawler/config/profile/%s/status",
                TYPE_UINT8,
                ND_HARDCODED,
                0, "1")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*actionauto-generate*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/crawler/config/profile/%s/auto_generate",
               TYPE_UINT8,
               ND_HARDCODED,
               0, "1")
DEL_TRANSL_NDS("/nkn/crawler/config/profile/%s/auto_generate_dest/%s",
                TYPE_STR,
                ND_NORMAL,
                1, NULL,
                2,0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementscrawler*actionauto-generate*source-file-type*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS("/nkn/crawler/config/profile/%s/auto_generate_src/%s",
                TYPE_STR,
                ND_NORMAL,
                2, NULL,
                2,0,2,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

