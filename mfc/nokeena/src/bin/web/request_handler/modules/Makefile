# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

ifdef STATIC_MODULES
    LIB=web_nokeena_mods
else
    SO_CLASS=web
endif

SRCS=                                   \
	web_config_form_namespace.c 	\
	web_config_form_ezconfig.c 	\
	web_servicelog.c 	\
	web_errorlog.c 	\
        web_config_form_diskcache.c \
        web_restart.c \
        web_config_form_nested.c \
        web_config_form_techsupport.c \
        web_config_form_action.c \
        web_nkn_logging.c \
        web_nkn_tcl.c \
	web_cachelog.c 	\
	web_tracelog.c 	\
	web_config_form_action_list.c \
	web_streamlog.c \
	web_fuselog.c 	\
	web_config_form_nsconfig.c 	\
	web_config_form_delivery.c 	\
	web_fmsedgelog.c 	\
	web_fmsaccesslog.c 	\
        web_sys_logging.c \
        web_config_form_servermap.c \
	web_publishlog.c 	\
	web_config_form_mfp.c 	\
        web_snapshot.c \
	web_config_form_ntp.c \


INCLUDES= -I${PROD_TREE_ROOT}/src/bin/web/request_handler \
	-I${PROD_TREE_ROOT}/src/include/bin/web/request_handler \

ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif
