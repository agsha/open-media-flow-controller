PR: 
# Filename: bin/mgmtd/modules/Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk
FULL_BUILD_DIR=${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${FILES_INSTALL_DIR}

NAME=md_nkn_mods

ifdef STATIC_MODULES
    LIB=${NAME}
else
    SO_CLASS=md
endif

SRCS=                           \
	md_nvsd.c		\
	md_nvsd_network.c	\
	md_nvsd_http.c		\
	md_nvsd_diskcache.c     \
	md_nvsd_buffermgr.c     \
	md_nvsd_mediamgr.c      \
	md_nvsd_scheduler.c     \
	md_nvsd_fmgr.c          \
	md_nvsd_am.c		\
	md_oomgr.c              \
	md_nkn_cmm.c		\
	md_nvsd_pm.c		\
	md_acclog_pm.c	        \
	md_errlog_pm.c		\
	md_oomgr_pm.c		\
	md_nkn_cmm_pm.c		\
	md_vpemgr_pm.c		\
	md_cache_evictd_pm.c	\
	md_bgpd_pm.c	\
	md_zebra_pm.c	\
	md_nsmartd_pm.c	\
	md_cache_meta_evictd_pm.c	\
	md_cache_evictd_ssd_pm.c	\
	md_cache_evictd_sas_pm.c	\
	md_cache_evictd_sata_pm.c	\
	md_disk_auto_enable_pm.c	\
	md_bgp.c		\
	md_acclog.c		\
	md_errlog.c		\
	md_nkn_stats.c		\
	md_ftpd_passwd.c	\
	md_ftpd_pm.c		\
	md_nvsd_mm.c		\
	md_nvsd_namespace.c	\
	md_nvsd_uri.c		\
	md_nvsd_virtual_player.c	\
	md_nvsd_origin_fetch.c	\
	md_nvsd_interface.c	\
	md_pre_stage.c		\
	md_mfdlog.c		\
	md_nvsd_server_map.c	\
	md_nvsd_cluster.c	\
	md_mfd_license.c	\
	md_dashboard_pm.c	\
	md_nvsd_tfm.c		\
	md_nkn_events.c		\
	md_nknlogd_pm.c		\
	md_nkn_debug.c		\
	md_nkn_bonding.c	\
	md_nvsd_rtstream.c	\
	md_nvsd_pub_point.c 	\
	md_tracelog.c		\
	md_cachelog.c		\
	md_streamlog.c		\
	md_fuselog.c		\
	md_fmsaccesslog.c	\
	md_fmsedgelog.c		\
	md_nkn_mgmt_if.c	\
	md_gmgmthd_pm.c		\
	md_drop_cache_pm.c	\
	md_nvsd_policy_engine.c \
	md_nvsd_mfp.c \
	md_nvsd_resource_mgr.c \
	md_publishlog.c \
	md_file_mfpd_pm.c	\
	md_live_mfpd_pm.c	\
	md_nvsd_pre_fetch.c     \
	md_nfs_mount.c \
	md_nkn_services.c \
	md_l4proxy_pm.c	\
	md_tmd_pm.c	\
	md_ucfltd_pm.c	\
        md_l4proxy.c       \
	md_nkncnt_pm.c	\
	md_adnsd_pm.c	\
	md_ssld_pm.c	\
	md_named.c		\
	md_ssld_cfg.c \
	md_ssld_delivery.c \
	md_irqpin_ixgbed_pm.c \
	md_ingester_pm.c \
	md_geodbd_pm.c	\
	md_geodbd.c	\
	md_watchdog_pm.c \
	md_watchdog.c \
	md_ssld_stats.c \
	md_agentd_pm.c \
	md_apply_first.c \
	md_apply_last.c \
	md_ssh_tunnel_pm.c \
	md_lfd_pm.c \
	md_crawler_cfg.c \
	md_crawler_pm.c \
	md_nknexecd_pm.c \
	md_nkn_monitor.c \
	md_logexport_passwd.c \
	md_logexport.c \
	md_ssh.c \
	md_nkn_cb_pm.c \
	md_if_config.c \
	md_jce_pm.c \
	md_loganalyzer_pbr_pm.c	\
	md_url_filter.c \
	md_cfg_files.c \
	md_device_map.c \
	md_log_analyzer.c \
	md_jpsd_pm.c \
	md_jpsd_tdf.c \
	md_junos_re.c \
	md_pacifica_resiliency_pm.c \

ifeq "${BUILD_USE_SSL}" "static_openssl"
OPENSSL_INCLUDE=-I/usr/local/openssl/include
OPENSSL_LIBDIR=/usr/local/openssl/lib
OPENSSL_SSL_LIB=/usr/local/openssl/lib/libssl.a
else
OPENSSL_INCLUDE=
endif

INCLUDES=\
 ${OPENSSL_INCLUDE} \
 -I${PROD_TREE_ROOT}/src/include     \
 -I${PROD_TREE_ROOT}/src/lib/libevent     \
 -I${PROD_TREE_ROOT}/src/bin/mgmtd     \
 -I${PROD_TREE_ROOT}/src/lib/libgd     \
 -I${PROD_TREE_ROOT}/src/include/bin/mgmtd     \
 -I${PROD_TREE_ROOT}/src/include/lib/libgd     \


ifeq (${BUILD_GCOV}, 1)
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

ifeq "${BUILD_USE_SSL}" "static_openssl"
LDFLAGS += -L${OPENSSL_LIBDIR} -ldl -lpthread -lcprops

LDADD = \
 ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncnt/libnkncnt.a \
 ${OPENSSL_SSL_LIB} \
 ${BUILD_EXTRA_LDADD}
else
LDFLAGS += -lssl -lpthread -lcprops
LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncnt/libnkncnt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/sslcnt/libsslcnt.a \
	${BUILD_EXTRA_LDADD}
endif

ifdef STATIC_MODULES
    include ${PROD_TREE_ROOT}/src/mk/lib.mk
else
    include ${PROD_TREE_ROOT}/src/mk/so.mk
endif
