TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*")
    TRANSL_CUST(agentd_namespace_delete, NULL)
    TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*precedence*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/ns-precedence",
	    TYPE_UINT32,
	    ND_NORMAL,
	    1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*domainname*")                            // Correspoding Broker CMD
TRANSL_NUM_NDS(1)					     // Number of Nodes to be set
DEL_TRANSL_NDS("/nkn/nvsd/namespace/%s/fqdn-list/%s",       // Node to be set
	    TYPE_STR,                                        // Node data type
	    ND_NORMAL,				     // NODE type,This specifies what kinf of node, ND_HARDCODED = value to be set is harcoded and
	    1, NULL,
	    2,0,1,0)					     //is taken as the last argument which is * in this case
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*domainregex*")                            // Correspoding Broker CMD
TRANSL_NUM_NDS(1)					     // Number of Nodes to be set
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/domain/regex",      // Node to be set
	    TYPE_REGEX,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchuriregex*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/uri/regex",
	    TYPE_REGEX,
	    ND_NORMAL,
	    1, NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/type",
	    TYPE_UINT8,
	    ND_HARDCODED,
	    0,"2")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchheaderregex*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/header/regex",
	    TYPE_REGEX,
	    ND_NORMAL,
	    1, NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/type",
	    TYPE_UINT8,
	    ND_HARDCODED,
	    0,"4")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchquery-stringregex*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/query-string/regex",
	    TYPE_REGEX,
	    ND_NORMAL,
	    1, NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/type",
	    TYPE_UINT8,
	    ND_HARDCODED,
	    0,"6")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchuriuri-prefix*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/uri/uri_name",
	    TYPE_URI,
	    ND_NORMAL,
	    1, NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/type",
	    TYPE_UINT8,
	    ND_HARDCODED,
	    0,"1")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchvirtual-hostipv4-address*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/virtual-host/ip",
            TYPE_IPADDRV4,
            ND_NORMAL,
            1, NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/type",
            TYPE_UINT8,
            ND_HARDCODED,
            0,"7")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchvirtual-hostport*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/virtual-host/port",
            TYPE_UINT16,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*virtual-player*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/virtual_player",
	    TYPE_STR,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*accesslog-profile*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/accesslog",
            TYPE_STR,
	    ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchheadername*")
TRANSL_NUM_NDS(3)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/header/name",
	    TYPE_STR,
	    ND_NORMAL,
	    1,NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/header/value",
	    TYPE_STR,
	    ND_NORMAL,
	    2,NULL)

RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/type",
	    TYPE_UINT8,
	    ND_HARDCODED,
	    0,"3")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchheadername*value*")
TRANSL_NUM_NDS(3)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/header/name",
	    TYPE_STR,
	    ND_NORMAL,
	    1,NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/header/value",
	    TYPE_STR,
	    ND_NORMAL,
	    2,NULL)

RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/type",
	    TYPE_UINT8,
	    ND_HARDCODED,
	    0,"3")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverabsolute-url*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/http/absolute-url",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverfollowheadername*")
TRANSL_NUM_NDS(3)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/http/follow/header",
	    TYPE_STR,
	    ND_NORMAL,
	    1,NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"false")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/type",
            TYPE_UINT8,
            ND_HARDCODED,
            0,"3")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverfollowheaderuse-client-ip*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/ip_tproxy",
            TYPE_IPADDRV4,
            ND_HARDCODED,
            0,"1.1.1.1")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverfqdnfqdn-path*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/http/host/name",
            TYPE_STR,
            ND_NORMAL,
            1,NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/type",
            TYPE_UINT8,
            ND_HARDCODED,
            0,"1")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverfqdnport*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/http/host/port",
            TYPE_UINT16,
            ND_NORMAL,
            1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverfollowdest-ip*")
TRANSL_NUM_NDS(3)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/http/follow/dest-ip",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"true")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"false")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/type",
            TYPE_UINT8,
            ND_HARDCODED,
            0,"4")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-serverfollowdest-ipuse-client-ip*")
TRANSL_NUM_NDS(4)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/http/follow/dest-ip",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"true")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/type",
            TYPE_UINT8,
            ND_HARDCODED,
            0,"4")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/ip_tproxy",
            TYPE_IPADDRV4,
            ND_HARDCODED,
            0,"1.1.1.1")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-server/http/follow/use-client-ip",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchquery-stringname*")
/* Can be ignored as "value" is a mandatory setting for query-string and 
   resetting of nodes will be handled in "name*value*" pattern deletion.  */
TRANSL_CUST(agentd_custom_entry_ignore, NULL)
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchquery-stringname*value*")
TRANSL_NUM_NDS(3)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/query-string/name",
	    TYPE_STR,
	    ND_NORMAL,
	    1,NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/query-string/value",
	    TYPE_STR,
	    ND_NORMAL,
	    2,NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/type",
	    TYPE_UINT8,
	    ND_HARDCODED,
	    0,"5")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*status*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/status/active",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"false")
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*matchprecedence*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/match/precedence",
	    TYPE_UINT32,
	    ND_NORMAL,
	    1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*live-pub-point*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS("/nkn/nvsd/namespace/%s/pub-point/%s",
            TYPE_STR,
            ND_NORMAL,
            1,NULL,
            2,0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*live-pub-point*caching*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/namespace/%s/pub-point/%s/caching/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            0,"true",
            2,0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*live-pub-point*receive-mode*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/namespace/%s/pub-point/%s/mode",
            TYPE_STR,
            ND_NORMAL,
            2,NULL,
            2,0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*objectvalidity-begin-header*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/validity/start_header_name",
            TYPE_STR,
            ND_NORMAL,
            1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

/*************************************************************/
/****** HTTP INSTANCE - MEDIA CACHE COMMANDS *****************/
/*************************************************************/
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*media-cachediskcache-tiersasfree-block-threshold*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/mediacache/sas/free_block_thres",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*media-cachediskcache-tiersatafree-block-threshold*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/mediacache/sata/free_block_thres",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*media-cachediskcache-tierssdfree-block-threshold*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/mediacache/ssd/free_block_thres",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*media-cachediskcache-tiersasgroup-read*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/mediacache/sas/group_read_enable",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*media-cachediskcache-tiersatagroup-read*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/mediacache/sata/group_read_enable",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*media-cachediskcache-tierssdgroup-read*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/mediacache/ssd/group_read_enable",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*media-cachediskcache-tiersasread-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/mediacache/sas/readsize",
                TYPE_UINT32,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*media-cachediskcache-tiersataread-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/mediacache/sata/readsize",
                TYPE_UINT32,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*media-cachediskcache-tierssdread-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/mediacache/ssd/readsize",
                TYPE_UINT32,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

/*************************************************************/
/******    HTTP INSTANCE - CACHE PINNING commands       ******/
/*************************************************************/
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*pinned-objectcache-capacity*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/cache_size",
                TYPE_UINT32,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*pinned-objectmax-obj-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/max_obj_size",
                TYPE_UINT32,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*pinned-objectpin-header*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/pin_header_name",
                TYPE_STR,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*pinned-objectenable*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/cache_pin/enable",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*pinned-objectauto-pin*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/pinned_object/auto_pin",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

/*************************************************************/
/******    HTTP INSTANCE - Resource limit commands       ******/
/*************************************************************/
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*resource-limitcache-tiersascache-capacity*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/resource/cache/sas/capacity",
                TYPE_UINT64,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*resource-limitcache-tiersatacache-capacity*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/resource/cache/sata/capacity",
                TYPE_UINT64,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*resource-limitcache-tierssdcache-capacity*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/resource/cache/ssd/capacity",
                TYPE_UINT64,
                ND_NORMAL,
                1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

/*************************************************************/
/****DELIVERY PROTOCOL - HTTP                             ****/
/*************************************************************/
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-connectionconcurrent-sessionretry-after*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/resource-pool/limit/http/max-session/retry-after",
        TYPE_UINT32,
        ND_NORMAL,
        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*cache-inheritnamespace-UID*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/cache_inherit",
        TYPE_STR,
        ND_HARDCODED,
        0, "set")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/uid",
        TYPE_STR,
        ND_NORMAL,
        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-directiveno-cache*") 
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-directive/no-cache",
        TYPE_STR,
        ND_NORMAL,
        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END
/*v*/
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-agedefault-age*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-age/default",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END
/*v*/
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionsdate-header-modifydate-header-options*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/date-header/modify",
            TYPE_BOOL,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-fillaggressive*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-fill",
            TYPE_UINT32,
            ND_HARDCODED,
            0, "1")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-fillclient-driven*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-fill",
            TYPE_UINT32,
            ND_HARDCODED,
            0, "2")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-fillclient-drivenaggressive-threshold*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-fill",
            TYPE_UINT32,
            ND_HARDCODED,
            0, "2")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-fill/client-driven/aggressive_threshold",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

/*v - But there is a data type mismatch need to be take care*/
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscontent-storemediacache-age-threshold*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/content-store/media/cache-age-threshold",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END
/*v*/
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscontent-storemediaobject-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/content-store/media/object-size-threshold",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END
/*v*/

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscontent-storemediacache-tierhighestobject-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-ingest/size-threshold",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchbyte-rangeignore*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/byte-range/preserve",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END 
/*v - But the parameters returned is -1*/

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchbyte-rangepreserve*")/*Need to be verified later since the translation is not working fine*/
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/byte-range/preserve",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_NDS_EXT_VAL("/nkn/nvsd/origin_fetch/config/%s/byte-range/align",
            TYPE_BOOL,
            ND_NORMAL,
            1, NULL)
TRANSL_NDS_VAL_MAP_START(2)
TRANSL_NDS_VAL_MAP(0,"align","true")
TRANSL_NDS_VAL_MAP(1,"no-align","false")
TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionseod-on-close*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/detect_eod_on_close",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

/* Ignore deletion of header "name" and "value" as MFC always has the default name and value */
TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionsheader*")
TRANSL_CUST(agentd_custom_entry_ignore, NULL)
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionsheader*value*")
TRANSL_CUST(agentd_custom_entry_ignore, NULL)
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionsheader*cachedeny*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/header/set-cookie/cache",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionsheader*cachepermit*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/header/set-cookie/cache",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionsheader*cachepermitno-revalidation*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/header/set-cookie/cache",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/header/set-cookie/cache/no-revalidation",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-controlcustom-header*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-control/custom-header",
            TYPE_STR,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END
/*v */

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionsredirect-302redirect-options*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/redirect_302",
            TYPE_BOOL,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END
/*v - But data type mismatch*/

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionshost-header-inheritincoming-req*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC ("/nkn/nvsd/origin_fetch/config/%s/host-header/inherit",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionshost-header-set*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/host-header/header-value",
	    TYPE_STR,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionscache-revalidationpermituse-date-header*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/use-date-header",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionscache-revalidationpermitflush-caches*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/flush-intermediate-cache",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionscache-revalidationpermit*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-revalidate",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionscache-revalidationpermitmethodmethod-options*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/method",
            TYPE_STR,
            ND_HARDCODED,
            0, "head")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionscache-revalidationpermitmethodvalidate-header*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/validate-header/header-name",
            TYPE_STR,
            ND_NORMAL,
            1, 0)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-revalidate/validate-header",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestconnecttimeout*") 
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-request/connect/timeout",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestconnectretry-delay*") 
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-request/connect/retry-delay",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestreadretry-delay*") 
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-request/read/retry-delay",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestreadtimeout*") 
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-request/read/interval-timeout",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionscache-revalidationdeny*") 
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-revalidate",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionsx-forwarded-for*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-request/x-forwarded-for/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestactionssecure-fetch*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-request/secure",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactionquery-stringno-cache*")
/* Fix for PR 725551. no-cache is the default setting. Hence deletion can be ignored*/
TRANSL_CUST(agentd_custom_entry_ignore, NULL)
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactioncache-controlmax-ageage*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/client-request/cache-control/max_age",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/client-request/cache-control/action",
            TYPE_STR,
            ND_HARDCODED,
            0, "serve-from-origin")
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactionquery-stringcache*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/query/cache/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/query/strip",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactionquery-stringcacheexclude-query-string*")
TRANSL_CUST(agentd_handle_querystr_cache_reset, NULL)
TRANSL_END_CUST
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactioncookiecache*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cookie/cache/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactioncookieno-cache*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cookie/cache/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactioncache-hitrevalidate-always*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/revalidate/reval_type",
            TYPE_STR,
            ND_HARDCODED,
            0, "all_offline")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/revalidate/time_barrier",
            TYPE_STR,
            ND_HARDCODED,
            0, "2541441258")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactioncache-hitrevalidate-alwaysinline*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/revalidate/reval_type",
            TYPE_STR,
            ND_HARDCODED,
            0, "all_inline")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/revalidate/time_barrier",
            TYPE_STR,
            ND_HARDCODED,
            0, "2541441258")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requesttunnel-all*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/client-request/tunnel-all",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-responsedscp*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-response/dscp",
            TYPE_INT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*pre-stageftp-user*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/namespace/%s/prestage/user/%s_ftpuser/password",
            TYPE_STR,
            ND_NORMAL,
            2, NULL,
	2,0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*pre-stageftp-user*password*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS("/nkn/nvsd/namespace/%s/prestage/user/%s_ftpuser/password",
            TYPE_STR,
            ND_NORMAL,
            2, NULL,
	2,0,1,0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*cache-indexdomain-name*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-index/domain-name",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-object-sizemax-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/obj-thresh-max",
	    TYPE_UINT32,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-object-sizemin-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/obj-thresh-min",
	    TYPE_UINT32,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactioncache-indexurl-match*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/dynamic-uri/regex",
	    TYPE_REGEX,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactioncache-indexmap-to*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/dynamic-uri/map-string",
	    TYPE_STR,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactioncache-indexno-match-tunnel*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/dynamic-uri/no-match-tunnel",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionstunnel-overrideobject-expired*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/tunnel-override/response/obj-expired",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionstunnel-overridecache-control-no-transform*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/tunnel-override/response/cc-notrans",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionsobjectcorrelationvalidators-allignore*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/correlation/validatorsall/ignore",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionsobjectcorrelationetagignore*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/correlation/etag/ignore",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionsoffline-fetchfile-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/offline/fetch/size",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscache-ingesthotness-threshold*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-ingest/hotness-threshold",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionscontent-storemediauri-depth-threshold*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/content-store/media/uri-depth-threshold",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-fetchactionspacking-policy*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/ingest-policy",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*origin-requestinclude-headerrequest-ip-port*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/origin-request/include-orig-ip-port",
                  TYPE_BOOL,
                  ND_HARDCODED,
                  0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requesttunnel-overrideauth-header*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/tunnel-override/request/auth-header",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requesttunnel-overridecookie*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/tunnel-override/request/cookie-header",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requesttunnel-overridecache-control*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/tunnel-override/request/cache-control",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requesttunnel-overridequery-string*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/tunnel-override/request/query-string",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requesthandle-connect*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/client-request/method/connect/handle",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestseek-uriurl-match*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/seek-uri/enable",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/seek-uri/regex",
            TYPE_REGEX,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestseek-urimap-to*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/seek-uri/map-string",
            TYPE_STR,
            ND_NORMAL,
            1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestsecure*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/client-request/secure",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*client-requestactioncache-controlmax-ageserve-from-origin*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/client-request/cache-control/action",
            TYPE_STR,
            ND_HARDCODED,
            0, "serve-from-origin")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*cache-indexobject-includechecksum*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object/chksum-bytes",
                   TYPE_UINT32,
                   ND_NORMAL,
                   1, NULL)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-index/include-object",
                  TYPE_BOOL,
                  ND_HARDCODED,
                  0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*cache-indexheader-include*")
TRANSL_CUST (agentd_ns_cache_index_header_delete, NULL)
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*loggingaccessfilename*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/filename",
		TYPE_STR,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*loggingaccessrotatefilesize*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/max-filesize",
		TYPE_STR,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*loggingaccessrotateon-the-hour")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/on-the-hour",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*loggingaccessrotatetime-interval*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/time-interval",
		TYPE_STR,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*loggingaccessauto-copy*")
TRANSL_NUM_NDS(2)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/upload/url",
		TYPE_STR,
		ND_NORMAL,
		1, NULL)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/upload/pass",
		TYPE_STR,
		ND_NORMAL,
		2, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*loggingaccesssyslog-replicate")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/syslog",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*loggingaccessdisplay-format")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/format-display",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*loggingaccessformat*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/format",
		TYPE_STR,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*loggingaccessmax-fileid*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/accesslog/config/max-fileid",
		TYPE_STR,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*compressionalgorithm*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/compression/algorithm",
		TYPE_STR,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*compressionenable*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/compression/enable",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*compressionsize-rangemax-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/compression/obj_range_max",
		TYPE_UINT64,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*compressionsize-rangemin-size*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/object/compression/obj_range_min",
		TYPE_UINT64,
		ND_NORMAL,
		1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_DEL_TRANS_STR"servicehttpinstance*compressionfile-types*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS("/nkn/nvsd/origin_fetch/config/%s/object/compression/file_types/%s",
		TYPE_STR,
		ND_NORMAL,
		1, NULL,
		2, 0, 1, 0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(HTTP_INST_DEL_TRANS_STR"origin-fetchactionscache-controls-maxageignore*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-control/s-maxage-ignore",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(HTTP_INST_DEL_TRANS_STR"origin-fetchactionscache-ingestbyte-range-incapablewhole-object*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/cache-ingest/byte-range-incapable",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(HTTP_INST_DEL_TRANS_STR"origin-fetchactionscache-responses*")
TRANSL_NUM_NDS(1)
DEL_TRANSL_NDS("/nkn/nvsd/origin_fetch/config/%s/cache/response/%s",
		TYPE_UINT32,
		ND_NORMAL,
		1, NULL,
		2, 0, 1, 0)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(HTTP_INST_DEL_TRANS_STR"origin-fetchactionscache-responses*age*")
TRANSL_CUST(agentd_custom_entry_ignore, NULL)
TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(HTTP_INST_DEL_TRANS_STR"client-requestactioncache-hitrevalidate-async*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/origin_fetch/config/%s/serve-expired/enable",
		TYPE_BOOL,
		ND_HARDCODED,
		0, "false")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(HTTP_INST_DEL_TRANS_STR"distribution-id*")
TRANSL_NUM_NDS(1)
RESET_TRANSL_NDS_BASIC("/nkn/nvsd/namespace/%s/distrib-id",
		TYPE_STR,
		ND_HARDCODED,
		0, "")
TRANSL_END_NDS
TRANSL_ENTRY_END
