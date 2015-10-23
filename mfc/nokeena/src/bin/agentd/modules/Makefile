# Filename: bin/agentd/modules/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=agentd_mods

ifdef STATIC_MODULES
    LIB=${NAME}
else
    SO_CLASS=agentd
endif

SRCS= agentd_accesslog_cmds.c \
	agentd_aaa_cmds.c \
	agentd_analytics_cmds.c \
	agentd_bgp_cmds.c \
	agentd_config_cmds.c \
	agentd_crawler_cmds.c \
        agentd_geodb_service_cmds.c \
	agentd_global_delivery_cmds.c \
	agentd_image_cmds.c \
	agentd_logging_cmds.c \
	agentd_media_cache_cmds.c \
	agentd_network_cmds.c \
	agentd_policy_engine_cmds.c \
	agentd_ram_cache_cmds.c \
	agentd_resource_pool_cmds.c \
	agentd_ssl_cmds.c  \
	agentd_ssh_cmds.c \
	agentd_system_cmds.c \
	agentd_snmp_cmds.c \
	agentd_stats_alarm_cmds.c \
	agentd_tacacs_server_cmds.c \
	agentd_virtual_player_cmds.c \
	agentd_backup_restore.c \
	agentd_get_stats.c \
	agentd_get_config.c \
 
ifeq "${BUILD_USE_SSL}" "static_openssl"
OPENSSL_INCLUDE=-I/usr/local/openssl/include
OPENSSL_LIBDIR=/usr/local/openssl/lib
OPENSSL_SSL_LIB=/usr/local/openssl/lib/libssl.a
OPENSSL_CRYPTO_LIB=/usr/local/openssl/lib/libcrypto.a
INCLUDES= \
 ${OPENSSL_INCLUDE}

LDFLAGS+= -L${OPENSSL_LIBDIR}
LDADD= \
 ${OPENSSL_SSL_LIB} \
 ${OPENSSL_CRYPTO_LIB} \
 ${BUILD_EXTRA_LDADD}
else
INCLUDES=
LDFLAGS+= -lssl
LDADD= ${BUILD_EXTRA_LDADD}
endif

AGENTD_INCLUDE=-I../
CFLAGS+= -Wl,-export-dynamic
CFLAGS += -Wall -Werror -Wno-unused ${AGENTD_INCLUDE} \
 -I/usr/include/libxml2 

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif
