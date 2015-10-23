# Filename: bin/cli/modules/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk

NAME=cli_mfd_mods

ifdef STATIC_MODULES
    LIB=${NAME}
else
    SO_CLASS=cli
endif

SRCS= cli_nkn_cmds.c \
      cli_nkn_al_cmds.c \
	  cli_nvsd_http_cmds.c \
	  cli_nvsd_access_ctrl_cmds.c \
	  cli_nvsd_am_cmds.c \
	  cli_nvsd_logging_cmds.c \
	  cli_nvsd_network_cmds.c \
	  cli_nvsd_ramcache_cmds.c \
	  cli_nvsd_diskcache_cmds.c \
	  cli_nvsd_namespace_cmds.c \
	  cli_nvsd_pre_stage_cmds.c \
	  cli_nvsd_stats_cmds.c \
	  cli_nkn_accesslog_cmds.c \
	  cli_nkn_crawllog_cmds.c \
	  cli_disabled_cmds.c \
	  cli_nkn_mfdlog_cmds.c \
	  cli_nvsd_virtual_player_cmds.c \
	  cli_nvsd_delivery_cmds.c \
	  cli_nvsd_server_map_cmds.c \
	  cli_nvsd_tfm_cmds.c \
	  cli_nkn_if_cmds.c \
	  cli_nkn_mgmt_if_cmds.c \
	  cli_nvsd_debug_cmds.c \
	  cli_nvsd_rtstream_cmds.c \
	  cli_nvsd_pub_point_cmds.c \
	  cli_nvsd_rtstream_media_cmds.c \
	  cli_nkn_errorlog_cmds.c \
	  cli_nkn_tracelog_cmds.c \
	  cli_nkn_cachelog_cmds.c \
	  cli_nkn_streamlog_cmds.c \
	  cli_nkn_fuselog_cmds.c \
	  cli_nkn_fmsaccesslog_cmds.c \
	  cli_nkn_fmsedgelog_cmds.c \
	  cli_nkn_file_cmds.c \
	  cli_nvsd_policy_engine_cmds.c \
	  cli_nvsd_cluster_cmds.c \
	  cli_nkn_publishlog_cmds.c \
	  cli_nvsd_resource_mgr_cmds.c \
	  cli_nvsd_pre_fetch_cmds.c \
	  cli_nvsd_mfp.c \
	  cli_adnsd_cmds.c \
	  cli_geodbd_cmds.c \
          cli_l4proxy_cmds.c \
	  cli_ssld_cfg_cmds.c \
	  cli_ssld_delivery_cmds.c \
	  cli_persistent_store.c \
	  cli_bgp_cmds.c \
	  cli_systune_cmds.c \
	  cli_watchdog_cmds.c \
	  cli_crawler_cmds.c \
	  cli_interface_cmds.c \
	  cli_nkn_logexport_cmds.c \
	  cli_mgmt_route_cmds.c \
	  cli_url_filter_cmds.c \
	  cli_config_files_cmds.c \
	  cli_log_analyzer_cmds.c \
	  cli_device_map_cmds.c \
	  cli_jpsd_tdf_cmds.c \


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

CFLAGS+= -Wl,-export-dynamic
CFLAGS += -Wall -Werror -Wno-unused

ifeq (${BUILD_GCOV}, 1)
        CFLAGS += -fprofile-arcs -ftest-coverage
endif


ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif
