#define GENERIC      "0"
#define YAHOO        "3"
#define YOUTUBE      "5"
#define UNDEFINED_VP "7"

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s",
            TYPE_STR,
            ND_NORMAL,
            0, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*type*")
       TRANSL_NUM_NDS(1)
       TRANSL_NDS_EXT_VAL("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
           TYPE_UINT32,                                             // Node data type
           ND_NORMAL,                                       // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
           1, NULL)
           TRANSL_NDS_VAL_MAP_START(3)
		TRANSL_NDS_VAL_MAP(0,"generic","0")
		TRANSL_NDS_VAL_MAP(1,"yahoo","3")
		TRANSL_NDS_VAL_MAP(2,"youtube","5")
	    TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END// Specify the index of args from broker cmd which is the value to be set for the node,if type node type

                                            // End ZTraqnslation TBL entry
TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegeneric*")
    TRANSL_NUM_NDS(3)
    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",
        TYPE_UINT32,
        ND_HARDCODED,
        0, GENERIC)
    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/flv-type",      // Node to be set
        TYPE_STR,
        ND_HARDCODED,
        0, "byte-offset")
    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/mp4-type",      // Node to be set
        TYPE_STR,
        ND_HARDCODED,
        0, "time-msec")
    TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutube*")
    TRANSL_NUM_NDS(5)
    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",
        TYPE_UINT32,
        ND_HARDCODED,
        0, YOUTUBE)
    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/flv-type",      // Node to be set
        TYPE_STR,
        ND_HARDCODED,
        0, "time-msec")
    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/mp4-type",      // Node to be set
        TYPE_STR,
        ND_HARDCODED,
        0, "time-msec")
    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/uri_query",      // Node to be set
        TYPE_STR,
        ND_HARDCODED,
        0, "begin")
    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
        TYPE_BOOL,
        ND_HARDCODED,
        0, "true")

    TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlassured-flowquery-string-parmquery-string*")
	TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	TYPE_UINT32,					     // Node data type
	ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	0, GENERIC)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/query/str",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		     TYPE_UINT32,                                           // Node data type
		     ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		     0, "2")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/rate_control/query/str")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
    	TYPE_BOOL,
	ND_HARDCODED,
	0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlassured-flowquery-string-parmrate-units*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_EXT_VAL ("/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
 TRANSL_NDS_VAL_MAP_START(4)
        TRANSL_NDS_VAL_MAP(0,"Kbits-per-sec","Kbps")
        TRANSL_NDS_VAL_MAP(1,"KBytes-per-sec","KBps")
        TRANSL_NDS_VAL_MAP(2,"Mbits-per-sec","Mbps")
        TRANSL_NDS_VAL_MAP(3,"MBytes-per-sec","MBps")
 TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlassured-flowquery-string-parmdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlassured-flowauto*")
TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
			TYPE_UINT32,					     // Node data type
			ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
			0, GENERIC)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",      // Node to be set
			TYPE_BOOL,					     // Node data type
			ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
			0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
    	TYPE_BOOL,
	ND_HARDCODED,
	0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlmax-bit-rateauto*")
TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
                 TYPE_UINT32,                                         // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, GENERIC)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",      // Node to be set
                 TYPE_BOOL,                                           // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, "true")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
                 TYPE_UINT32,                                           // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, "1")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
		TYPE_LNK,
  		ND_DYNAMIC,
	        0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
	        TYPE_BOOL,
	        ND_HARDCODED,
	        0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlassured-flowautodelivery-overhead*")
TRANSL_NUM_NDS_ADV(6, POST_PROCESS)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
                 TYPE_UINT32,                                         // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, GENERIC)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",      // Node to be set
                 TYPE_BOOL,                                           // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, "true")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",      // Node to be set
                 TYPE_STR,                                           // Node data type
                 ND_NORMAL,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
	        TYPE_LNK,
       		ND_DYNAMIC,
  	     	0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
	        TYPE_BOOL,
       		ND_HARDCODED,
       		0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlassured-flowstaticrate*")
	TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, GENERIC)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
    	TYPE_BOOL,
	ND_HARDCODED,
	0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry
/* Dont know who sets the assured_flow/link ,/valid, /active nodes*/

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlassured-flowstaticdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
        TYPE_STR,
        ND_NORMAL,
        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlmax-bit-ratequery-string-parmquery-string*")
        TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
        TYPE_UINT32,                 
        ND_HARDCODED,                
        0, GENERIC)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/query/str",  
            TYPE_STR,       
            ND_NORMAL,                     
            1, NULL)
    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",   
                     TYPE_UINT32,                         
                     ND_HARDCODED,                        
                     0, "1")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
        TYPE_LNK,
        ND_DYNAMIC,
        0, "/nkn/nvsd/virtual_player/config/%s/rate_control/query/str")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
        TYPE_BOOL,
        ND_HARDCODED,
        0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END   

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlmax-bit-ratequery-string-parmrate-units*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_EXT_VAL ("/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
 TRANSL_NDS_VAL_MAP_START(4)
        TRANSL_NDS_VAL_MAP(0,"Kbits-per-sec","Kbps")
        TRANSL_NDS_VAL_MAP(1,"KBytes-per-sec","KBps")
        TRANSL_NDS_VAL_MAP(2,"Mbits-per-sec","Mbps")
        TRANSL_NDS_VAL_MAP(3,"MBytes-per-sec","MBps")
 TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlmax-bit-ratequery-string-parmdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlmax-bit-rateautodelivery-overhead*")
TRANSL_NUM_NDS_ADV(6, POST_PROCESS)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
                 TYPE_UINT32,                               
                 ND_HARDCODED,                              
                 0, GENERIC)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect", 
                 TYPE_BOOL,                                    
                 ND_HARDCODED,                                 
                 0, "true")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst", 
                 TYPE_STR,       
                 ND_NORMAL,      
                 1, NULL)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme", 
                TYPE_UINT32,        
                ND_HARDCODED,       
                0, "1")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
                TYPE_LNK,
                ND_DYNAMIC,
                0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlmax-bit-ratestaticrate*")
        TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",  
            TYPE_UINT32,                                             
            ND_HARDCODED,                                            
            0, GENERIC)          
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate",
            TYPE_UINT32,     
            ND_NORMAL,  
            1, NULL)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",
                TYPE_UINT32,                            
                ND_HARDCODED,                           
                0, "1")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
        TYPE_LNK,
        ND_DYNAMIC,
        0, "/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
        TYPE_BOOL,
        ND_HARDCODED,
        0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                  


TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericrate-controlmax-bit-ratestaticdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
        TYPE_STR,
        ND_NORMAL,
        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericfast-startsize*")
	TRANSL_NUM_NDS_ADV(4,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, GENERIC)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/size",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/active",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/fast_start/size")

TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericfast-startquery-string*")
	TRANSL_NUM_NDS_ADV(4,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
	    TYPE_UINT32,					   
	    ND_HARDCODED,					  
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/uri_query",
	    TYPE_STR,
	    ND_NORMAL,
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/active",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/fast_start/uri_query")

TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericfast-starttime*")
        TRANSL_NUM_NDS_ADV(4,POST_PROCESS)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",
            TYPE_UINT32,
            ND_HARDCODED,
            0, GENERIC)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/time",
            TYPE_UINT32,
            ND_NORMAL,
            1, NULL)
            TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/active",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/valid",
        TYPE_LNK,
        ND_DYNAMIC,
        0, "/nkn/nvsd/virtual_player/config/%s/fast_start/time")

TRANSL_END_NDS
TRANSL_ENTRY_END 
 
TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericseek-configquery-stringseek-length*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, GENERIC)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/length/uri_query",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericseek-configquery-stringseek-config*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, GENERIC)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/uri_query",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericseek-configseek-flv-type*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, GENERIC)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/flv-type",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericseek-configseek-mp4-type*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, GENERIC)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/mp4-type",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericseek-configtunnel-mode*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",
	    TYPE_UINT32,					  
	    ND_HARDCODED,					 
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",
	    TYPE_BOOL,				
	    ND_HARDCODED,		
	    0, "true")
	TRANSL_NDS_EXT_VAL("/nkn/nvsd/virtual_player/config/%s/seek/enable_tunnel",
		    TYPE_BOOL,
		    ND_NORMAL,
		    1, NULL)
	TRANSL_NDS_VAL_MAP_START(2)
	TRANSL_NDS_VAL_MAP(0,"enable","true")
	TRANSL_NDS_VAL_MAP(1,"disable","false")
	TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END 
/* Node pending - seek/valid seek/active*/

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*type*signalsquery-stringsession-id*")
	TRANSL_NUM_NDS(1)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/signals/session_id/uri_query",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    2, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*type*signalsquery-stringstate*")
	TRANSL_NUM_NDS(1)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/signals/state/uri_query",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    2, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*type*signalsquery-stringprofile*")
	TRANSL_NUM_NDS(1)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/signals/profile/uri_query",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    2, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typesmoothstream-pubfragment-tag*")
	TRANSL_NUM_NDS(2)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, UNDEFINED_VP)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fragment-tag",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typesmoothstream-pubquality-tag*")
	TRANSL_NUM_NDS(2)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, UNDEFINED_VP)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/quality-tag",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

/*Avoid conflict between smmothstream-pub / flashstream-pub fragment tags*/
TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeflashstream-pubfragment-tag*")
	TRANSL_NUM_NDS(2)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, UNDEFINED_VP)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fragment-tag",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeflashstream-pubsegmentquery-stringfragment-delimiter*")
	TRANSL_NUM_NDS(2)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, UNDEFINED_VP)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/segment-delimiter-tag",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeflashstream-pubsegmentquery-stringtag*")
	TRANSL_NUM_NDS(2)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, UNDEFINED_VP)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/segment-tag",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry


TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericfull-downloadmatch-queryname*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
	    TYPE_UINT32,					   
	    ND_HARDCODED,					  
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/full_download/active", 
	    TYPE_BOOL,					    
	    ND_HARDCODED,				
	    0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/full_download/uri_query",
	    TYPE_STR,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericfull-downloadmatch-queryvalue*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
	    TYPE_UINT32,					   
	    ND_HARDCODED,					  
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/full_download/active", 
	    TYPE_BOOL,					    
	    ND_HARDCODED,				
	    0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/full_download/match",
	    TYPE_STR,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericfull-downloadmatch-headername*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",     
	    TYPE_UINT32,					     
	    ND_HARDCODED,					    
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/full_download/active", 
	    TYPE_BOOL,					    
	    ND_HARDCODED,				
	    0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/full_download/uri_hdr",
	    TYPE_STR,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericfull-downloadmatch-headervalue*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",     
	    TYPE_UINT32,					     
	    ND_HARDCODED,					    
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/full_download/active", 
	    TYPE_BOOL,					    
	    ND_HARDCODED,				
	    0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/full_download/match",
	    TYPE_STR,
	    ND_NORMAL,
	    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericfull-downloadalways*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, GENERIC)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/full_download/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/full_download/always",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenericconnectionmax-bandwidth*")
	TRANSL_NUM_NDS_ADV(2,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, GENERIC)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/max_session_rate",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry


TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenerichash-verifyquery-stringhash-expiry-time*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",    
	    TYPE_UINT32,					     
	    ND_HARDCODED,					   
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/active",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/expiry_time/epochtime",
	    TYPE_STR,
	    ND_NORMAL,
	    1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenerichash-verifyquery-stringhash-name*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",
	    TYPE_UINT32,					  
	    ND_HARDCODED,					 
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/active",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/match/uri_query",
	    TYPE_STR,
	    ND_NORMAL,
	    1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenerichash-verifyshared-secretsecret-string*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",   
	    TYPE_UINT32,					     
	    ND_HARDCODED,					    
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/secret/value",
	    TYPE_STR,
	    ND_NORMAL,
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/active",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenerichash-verifyshared-secretmode*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",   
	    TYPE_UINT32,					     
	    ND_HARDCODED,					    
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/secret/type",
	    TYPE_STR,
	    ND_NORMAL,
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/active",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"true")
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenerichash-verifyurl-type*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      
	    TYPE_UINT32,					    
	    ND_HARDCODED,					   
	    0, GENERIC)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/url_type",
	    TYPE_STR,
	    ND_NORMAL,
	    1,NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/active",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0,"true")

TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typegenerichash-verifydigest*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, GENERIC)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/hash_verify/digest",
	    TYPE_STR,
	    ND_NORMAL,
	    1,NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END


/*=======================================================================================================


Yahoo Translation

=========================================================================================================*/
TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoo*")
   TRANSL_CUST(ns_virtual_player_yahoo_set_default, "")
   TRANSL_END_CUST
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlassured-flowquery-string-parmquery-string*")
	TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	TYPE_UINT32,					     // Node data type
	ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	0, YAHOO)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/query/str",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/rate_control/query/str")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
			TYPE_BOOL,
			ND_HARDCODED,
			0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlassured-flowquery-string-parmrate-units*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_EXT_VAL ("/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
 TRANSL_NDS_VAL_MAP_START(4)
        TRANSL_NDS_VAL_MAP(0,"Kbits-per-sec","Kbps")
        TRANSL_NDS_VAL_MAP(1,"KBytes-per-sec","KBps")
        TRANSL_NDS_VAL_MAP(2,"Mbits-per-sec","Mbps")
        TRANSL_NDS_VAL_MAP(3,"MBytes-per-sec","MBps")
 TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlassured-flowquery-string-parmdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlmax-bit-rateauto*")
TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",    
                 TYPE_UINT32,                                 
                 ND_HARDCODED,                               
                 0, YAHOO)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",  
                 TYPE_BOOL,                            
                 ND_HARDCODED,                        
                 0, "true")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme", 
                 TYPE_UINT32,                                       
                 ND_HARDCODED,                                     
                 0, "1")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
                TYPE_LNK,
                ND_DYNAMIC,
                0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlassured-flowautodelivery-overhead*")
TRANSL_NUM_NDS_ADV(6, POST_PROCESS)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
                 TYPE_UINT32,                                         // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, YAHOO)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",      // Node to be set
                 TYPE_BOOL,                                           // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, "true")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",      // Node to be set
                 TYPE_STR,                                           // Node data type
                 ND_NORMAL,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 1, NULL)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
                TYPE_LNK,
                ND_DYNAMIC,
                0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlassured-flowauto*")
	TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
			TYPE_UINT32,					     // Node data type
			ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
			0, YAHOO)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",      // Node to be set
			TYPE_BOOL,					     // Node data type
			ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
			0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
			TYPE_BOOL,
			ND_HARDCODED,
			0, "true")
	TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlassured-flowstaticrate*")
	TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YAHOO)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
			TYPE_BOOL,
			ND_HARDCODED,
			0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlassured-flowstaticdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
	TYPE_STR,
	ND_NORMAL,
	1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlmax-bit-ratequery-string-parmquery-string*")
        TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      
        TYPE_UINT32,                   
        ND_HARDCODED,                 
        0, YAHOO)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/query/str",   
            TYPE_STR,                          
            ND_NORMAL,                        
            1, NULL)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",
                TYPE_UINT32,                                           
                ND_HARDCODED,                                   
                0, "1")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
        TYPE_LNK,
        ND_DYNAMIC,
        0, "/nkn/nvsd/virtual_player/config/%s/rate_control/query/str")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                        TYPE_BOOL,
                        ND_HARDCODED,
                        0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlmax-bit-ratequery-string-parmrate-units*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_EXT_VAL ("/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
 TRANSL_NDS_VAL_MAP_START(4)
        TRANSL_NDS_VAL_MAP(0,"Kbits-per-sec","Kbps")
        TRANSL_NDS_VAL_MAP(1,"KBytes-per-sec","KBps")
        TRANSL_NDS_VAL_MAP(2,"Mbits-per-sec","Mbps")
        TRANSL_NDS_VAL_MAP(3,"MBytes-per-sec","MBps")
 TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlmax-bit-ratequery-string-parmdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlmax-bit-rateautodelivery-overhead*")
TRANSL_NUM_NDS_ADV(6, POST_PROCESS)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",    
                 TYPE_UINT32,                                 
                 ND_HARDCODED,                               
                 0, YAHOO)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",   
                 TYPE_BOOL,                         
                 ND_HARDCODED,                     
                 0, "true")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",  
                 TYPE_STR,                                       
                 ND_NORMAL,                                     
                 1, NULL)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
                TYPE_LNK,
                ND_DYNAMIC,
                0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme", 
                TYPE_UINT32,                  
                ND_HARDCODED,                
                0, "1")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlmax-bit-ratestaticrate*")
        TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",  
            TYPE_UINT32,                                            
            ND_HARDCODED,                                          
            0, YAHOO)                     
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate",    
            TYPE_UINT32,                
            ND_NORMAL,                 
            1, NULL)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",    
                TYPE_UINT32,               
                ND_HARDCODED,             
                0, "1")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
        TYPE_LNK,
        ND_DYNAMIC,
        0, "/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                        TYPE_BOOL,
                        ND_HARDCODED,
                        0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoorate-controlmax-bit-ratestaticdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
        TYPE_STR,
        ND_NORMAL,
        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooseek-configquery-stringseek-length*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YAHOO)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/length/uri_query",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooseek-configquery-stringseek-config*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YAHOO)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/uri_query",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooseek-configseek-flv-type*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YAHOO)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/flv-type",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooseek-configseek-mp4-type*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YAHOO)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/mp4-type",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooseek-configtunnel-mode*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",  
	    TYPE_UINT32,					    
	    ND_HARDCODED,					   
	    0, YAHOO)                                        
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active", 
	    TYPE_BOOL,					
	    ND_HARDCODED,			
	    0, "true")
	TRANSL_NDS_EXT_VAL("/nkn/nvsd/virtual_player/config/%s/seek/enable_tunnel",
		    TYPE_BOOL,
		    ND_NORMAL,
		    1, NULL)
	TRANSL_NDS_VAL_MAP_START(2)
	TRANSL_NDS_VAL_MAP(0,"enable","true")
	TRANSL_NDS_VAL_MAP(1,"disable","false")
	TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoohealth-probequery-string*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",   
	    TYPE_UINT32,					     
	    ND_HARDCODED,					    
	    0, YAHOO)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/health_probe/uri_query", 
	    TYPE_STR,					     
	    ND_NORMAL,					    
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/health_probe/active",  
	    TYPE_BOOL,					     
	    ND_HARDCODED,				
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END 

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahoohealth-probequery-match*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",   
	    TYPE_UINT32,					     
	    ND_HARDCODED,					    
	    0, YAHOO)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/health_probe/match",
	    TYPE_STR,					     
	    ND_NORMAL,					    
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/health_probe/active",  
	    TYPE_BOOL,					     
	    ND_HARDCODED,				
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END 


/* Req-Auth */
TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooreq-authdigest*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",   
	    TYPE_UINT32,					     
	    ND_HARDCODED,					    
	    0, YAHOO)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/digest", 
	    TYPE_STR,					    
	    ND_NORMAL,					   
	    1, NULL)
    	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/active",  
    		TYPE_BOOL,					    
    		ND_HARDCODED,				
    		0, "true")
	TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooreq-authshared-secret*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",   
	    TYPE_UINT32,					     
	    ND_HARDCODED,					    
	    0, YAHOO)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/secret/value", 
	    TYPE_STR,					    
	    ND_NORMAL,					   
	    1, NULL)
    	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/active",  
    		TYPE_BOOL,					    
    		ND_HARDCODED,				
    		0, "true")
	TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooreq-authtime-interval*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",   
	    TYPE_UINT32,					     
	    ND_HARDCODED,					    
	    0, YAHOO)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/time_interval",
	    TYPE_UINT32,					     
	    ND_NORMAL,					    
	    1, NULL)
    	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/active",  
    		TYPE_BOOL,					    
    		ND_HARDCODED,				
    		0, "true")
	TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooreq-authquery-stringstream-id*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
	    TYPE_UINT32,					   
	    ND_HARDCODED,					  
	    0, YAHOO)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/stream_id/uri_query", 
	    TYPE_STR,					     
	    ND_NORMAL,					    
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/active", 
	    TYPE_BOOL,					   
	    ND_HARDCODED,				
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooreq-authquery-stringauth-id*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
	    TYPE_UINT32,					   
	    ND_HARDCODED,					  
	    0, YAHOO)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/auth_id/uri_query",
	    TYPE_STR,					     
	    ND_NORMAL,					    
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/active", 
	    TYPE_BOOL,					   
	    ND_HARDCODED,				
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooreq-authquery-stringhash-name*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
	    TYPE_UINT32,					   
	    ND_HARDCODED,					  
	    0, YAHOO)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/match/uri_query", 
	    TYPE_STR,					    
	    ND_NORMAL,					   
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/req_auth/active", 
	    TYPE_BOOL,					   
	    ND_HARDCODED,				
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END


/*=======================================================================================================


Youtube Translation

=========================================================================================================*/


TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlassured-flowquery-string-parmquery-string*")
	TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	TYPE_UINT32,					     // Node data type
	ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	0, YOUTUBE)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/query/str",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/rate_control/query/str")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
			TYPE_BOOL,
			ND_HARDCODED,
			0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlassured-flowquery-string-parmrate-units*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_EXT_VAL ("/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
 TRANSL_NDS_VAL_MAP_START(4)
        TRANSL_NDS_VAL_MAP(0,"Kbits-per-sec","Kbps")
        TRANSL_NDS_VAL_MAP(1,"KBytes-per-sec","KBps")
        TRANSL_NDS_VAL_MAP(2,"Mbits-per-sec","Mbps")
        TRANSL_NDS_VAL_MAP(3,"MBytes-per-sec","MBps")
 TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlassured-flowquery-string-parmdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
	TYPE_STR,
	ND_NORMAL,
	1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlmax-bit-rateauto*")
TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
                 TYPE_UINT32,                                         // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, YOUTUBE)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",      // Node to be set
                 TYPE_BOOL,                                           // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, "true")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
                 TYPE_UINT32,                                           // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, "1")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
                TYPE_LNK,
                ND_DYNAMIC,
                0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlassured-flowautodelivery-overhead*")
TRANSL_NUM_NDS_ADV(6, POST_PROCESS)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
                 TYPE_UINT32,                                         // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, YOUTUBE)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",      // Node to be set
                 TYPE_BOOL,                                           // Node data type
                 ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 0, "true")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",      // Node to be set
                 TYPE_STR,                                           // Node data type
                 ND_NORMAL,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
                 1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
                TYPE_LNK,
                ND_DYNAMIC,
                0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlassured-flowauto*")
TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
			TYPE_UINT32,					     // Node data type
			ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
			0, YOUTUBE)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",      // Node to be set
			TYPE_BOOL,					     // Node data type
			ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
			0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
			TYPE_BOOL,
			ND_HARDCODED,
			0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END


TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlassured-flowstaticrate*")
	TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YOUTUBE)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",      // Node to be set
		TYPE_UINT32,                                           // Node data type
		ND_HARDCODED,                                        // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
		0, "2")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
			TYPE_BOOL,
			ND_HARDCODED,
			0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlassured-flowstaticdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
	TYPE_STR,
	ND_NORMAL,
	1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlmax-bit-ratequery-string-parmquery-string*")
        TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",  
        TYPE_UINT32,                
        ND_HARDCODED,               
        0, YOUTUBE)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/query/str",
            TYPE_STR,               
            ND_NORMAL,              
            1, NULL)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme", 
                TYPE_UINT32,                      
                ND_HARDCODED,                     
                0, "1")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
        TYPE_LNK,
        ND_DYNAMIC,
        0, "/nkn/nvsd/virtual_player/config/%s/rate_control/query/str")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                        TYPE_BOOL,
                        ND_HARDCODED,
                        0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlmax-bit-ratequery-string-parmrate-units*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_EXT_VAL ("/nkn/nvsd/virtual_player/config/%s/rate_control/query/rate",
    TYPE_STR,
    ND_NORMAL,
    1, NULL)
 TRANSL_NDS_VAL_MAP_START(4)
        TRANSL_NDS_VAL_MAP(0,"Kbits-per-sec","Kbps")
        TRANSL_NDS_VAL_MAP(1,"KBytes-per-sec","KBps")
        TRANSL_NDS_VAL_MAP(2,"Mbits-per-sec","Mbps")
        TRANSL_NDS_VAL_MAP(3,"MBytes-per-sec","MBps")
 TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlmax-bit-ratequery-string-parmdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC ("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
        TYPE_STR,
        ND_NORMAL,
        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlmax-bit-rateautodelivery-overhead*")
TRANSL_NUM_NDS_ADV(6, POST_PROCESS)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
                 TYPE_UINT32,                               
                 ND_HARDCODED,                              
                 0, YOUTUBE)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect",  
                 TYPE_BOOL,                  
                 ND_HARDCODED,               
                 0, "true")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst", 
                 TYPE_STR,            
                 ND_NORMAL,           
                 1, NULL)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme", 
                TYPE_UINT32,  
                ND_HARDCODED, 
                0, "1")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
                TYPE_LNK,
                ND_DYNAMIC,
                0, "/nkn/nvsd/virtual_player/config/%s/rate_control/auto_detect")
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                TYPE_BOOL,
                ND_HARDCODED,
                0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlmax-bit-ratestaticrate*")
        TRANSL_NUM_NDS_ADV(5, POST_PROCESS)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
            TYPE_UINT32,                                
            ND_HARDCODED,                               
            0, YOUTUBE)                                 
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate", 
            TYPE_UINT32,                    
            ND_NORMAL,                      
            1, NULL)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/scheme",  
                TYPE_UINT32,            
                ND_HARDCODED,           
                0, "1")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/valid",
        TYPE_LNK,
        ND_DYNAMIC,
        0, "/nkn/nvsd/virtual_player/config/%s/rate_control/static/rate")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/active",
                        TYPE_BOOL,
                        ND_HARDCODED,
                        0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutuberate-controlmax-bit-ratestaticdelivery-overhead*")
TRANSL_NUM_NDS(1)
TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/rate_control/burst",
        TYPE_STR,
        ND_NORMAL,
        1, NULL)
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubefast-startsize*")
	TRANSL_NUM_NDS_ADV(4,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YOUTUBE)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/size",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/active",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/fast_start/size")

TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubefast-startquery-string*")
	TRANSL_NUM_NDS_ADV(4,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YOUTUBE)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/uri_query",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/active",
	    TYPE_BOOL,
	    ND_HARDCODED,
	    0, "true")
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/valid",
    	TYPE_LNK,
	ND_DYNAMIC,
	0, "/nkn/nvsd/virtual_player/config/%s/fast_start/uri_query")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubefast-starttime*")
        TRANSL_NUM_NDS_ADV(4,POST_PROCESS)
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",
            TYPE_UINT32,    
            ND_HARDCODED,
            0, YOUTUBE) 
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/time",
            TYPE_UINT32, 
            ND_NORMAL,        
            1, NULL)
            TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/active",
            TYPE_BOOL,
            ND_HARDCODED,
            0, "true")
        TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/fast_start/valid",
        TYPE_LNK,
        ND_DYNAMIC,
        0, "/nkn/nvsd/virtual_player/config/%s/fast_start/time")

TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubeseek-configquery-stringseek-length*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YOUTUBE)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/length/uri_query",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubeseek-configquery-stringseek-config*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YOUTUBE)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/uri_query",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubeseek-configseek-flv-type*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YOUTUBE)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/flv-type",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubeseek-configseek-mp4-type*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",      // Node to be set
	    TYPE_UINT32,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, YOUTUBE)                                         // Specify the index of args from broker cmd which is the value to be set for the node,if type node type
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/mp4-type",      // Node to be set
	    TYPE_STR,					     // Node data type
	    ND_NORMAL,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    1, NULL)
	    TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",      // Node to be set
	    TYPE_BOOL,					     // Node data type
	    ND_HARDCODED,					     // NODE type,This specifies what kinf of node, ND_NORMAL = will get values from broker
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END                                             // End ZTraqnslation TBL entry

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubeseek-configtunnel-mode*")
	TRANSL_NUM_NDS_ADV(3,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type", 
	    TYPE_UINT32,					   
	    ND_HARDCODED,					  
	    0, YOUTUBE)                                        
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/seek/active",
	    TYPE_BOOL,					    
	    ND_HARDCODED,				
	    0, "true")
	TRANSL_NDS_EXT_VAL("/nkn/nvsd/virtual_player/config/%s/seek/enable_tunnel",
		    TYPE_BOOL,
		    ND_NORMAL,
		    1, NULL)
	TRANSL_NDS_VAL_MAP_START(2)
	TRANSL_NDS_VAL_MAP(0,"enable","true")
	TRANSL_NDS_VAL_MAP(1,"disable","false")
	TRANSL_NDS_VAL_MAP_END
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubecache-namevideo-id*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",
	    TYPE_UINT32,					  
	    ND_HARDCODED,					 
	    0, YOUTUBE)                                         
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/video_id/uri_query",
	    TYPE_STR,					    
	    ND_NORMAL,					   
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/video_id/active",  
	    TYPE_BOOL,				
	    ND_HARDCODED,		
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubecache-nameformat-tag*")
	TRANSL_NUM_NDS_ADV(3, POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",
	    TYPE_UINT32,					  
	    ND_HARDCODED,					 
	    0, YOUTUBE)                                         
      	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/video_id/format_tag",  
	    TYPE_STR,					     
	    ND_NORMAL,					    
	    1, NULL)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/video_id/active",  
	    TYPE_BOOL,				
	    ND_HARDCODED,		
	    0, "true")
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyoutubeconnectionmax-bandwidth*")
	TRANSL_NUM_NDS_ADV(2,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",
	    TYPE_UINT32,					  
	    ND_HARDCODED,		
	    0, YOUTUBE)                
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/max_session_rate",    
	    TYPE_UINT32,					    
	    ND_NORMAL,					     
	    1, NULL)                                        
TRANSL_END_NDS
TRANSL_ENTRY_END

TRANSL_ENTRY(PREPEND_TRANS_STR"service-elementsvirtual-player*typeyahooconnectionmax-bandwidth*")
	TRANSL_NUM_NDS_ADV(2,POST_PROCESS)
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/type",
	    TYPE_UINT32,					  
	    ND_HARDCODED,		
	    0, YAHOO)                
	TRANSL_NDS_BASIC("/nkn/nvsd/virtual_player/config/%s/max_session_rate",    
	    TYPE_UINT32,					    
	    ND_NORMAL,					     
	    1, NULL)                                        
TRANSL_END_NDS
TRANSL_ENTRY_END                                           
