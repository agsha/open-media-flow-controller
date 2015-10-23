TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsaccesslogprofile*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS_BASIC("/nkn/accesslog/config/profile/%s",
            TYPE_STR,
            ND_NORMAL,
            0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsaccesslogprofile*file-name*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/profile/%s/filename",
            TYPE_STR,
            ND_HARDCODED,
            0, "access.log")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsaccesslogprofile*max-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/profile/%s/max-filesize",
            TYPE_UINT16,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsaccesslogprofile*max-duration*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/profile/%s/time-interval",
            TYPE_UINT32,
            ND_NORMAL,
            0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsaccesslogprofile*max-files*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/profile/%s/max-fileid",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsaccesslogprofile*format*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/profile/%s/format",
            TYPE_STR,
            ND_HARDCODED,
            0, "%h - - %t %r %s %b")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsaccesslogprofile*filtersize*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/profile/%s/object/filter/size",
            TYPE_UINT32,
            ND_NORMAL,
            0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsaccesslogprofile*filterresponse-code*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS("/nkn/accesslog/config/profile/%s/object/filter/resp-code/%s",
            TYPE_UINT16,
            ND_NORMAL,
            1, NULL,
            2, 0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"service-elementsaccesslogprofile*upload-url*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/profile/%s/upload/url",
	    TYPE_STR,
	    ND_NORMAL,
	    1, NULL)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/profile/%s/upload/pass",
	    TYPE_STR,
	    ND_NORMAL,
	    2, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

