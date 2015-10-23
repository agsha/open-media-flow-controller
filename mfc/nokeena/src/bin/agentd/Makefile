# Makefile

include ${PROD_TREE_ROOT}/src/mk/common.mk


SUBDIRS= agentd_cgi modules

PROD_INSTALL_PREFIX=/opt/nkn
PROG_INSTALL_DIR=sbin
PROG=agentd

SRCS=                   \
	agentd_main.c   \
	agentd_mgmt.c	\
	agentd_cli.c    \
	agentd_modules.c \
	agentd_msg_handler.c \
	juniper-media-flow-controller_odl.c \
	agentd_op_cmds_base.c \
	agentd_op_cmds_req.c \
	agentd_op_cmds_show.c \
	agentd_op_cmds_show_interfaces.c \
	agentd_copied_code.c \
	agentd_custom_handling.c \
	agentd_array.c \
        agentd_conf_omap.c \
        xml_utils.c

#WARNINGS_ARE_ERRORS=1

CFLAGS= \
 -Wall  -Werror -Wno-missing-format-attribute \
 -Wno-unused \
 -fPIC \
 -I/usr/lib64/glib-2.0/include \
 -I/usr/include/glib-2.0 \
 -I/usr/lib/glib-2.0/include \
 -I/usr/include/libxml2 \
 -I${PROD_TREE_ROOT}/src/lib/libgcl \
 -I${PROD_TREE_ROOT}/src/include \
 ${BUILD_EXTRA_CFLAGS}

ifeq (${BUILD_GCOV}, 1)
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

CFLAGS += \
 -DNKN_BUILD_ID=\"${NKN_BUILD_ID}\" \
 -DNKN_BUILD_PROD_RELEASE=\"${NKN_BUILD_PROD_RELEASE}\" \
 -DNKN_BUILD_NUMBER=\"${NKN_BUILD_NUMBER}\" \
 -DNKN_BUILD_SCM_INFO_SHORT=\"${NKN_BUILD_SCM_INFO_SHORT}\" \
 -DNKN_BUILD_SCM_INFO=\"${NKN_BUILD_SCM_INFO}\" \
 -DEXTRA_CFLAGS_DEF=\"${BUILD_EXTRA_CFLAGS}\" \

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

THIRDPARTYLIBS= \

LDFLAGS += -lpthread `pkg-config --cflags --libs glib-2.0 gthread-2.0` `xml2-config --libs` -lm -lelf -ldl -lssl -luuid -L/usr/lib64/python2.6/ -lpython2.6 -lcares ${THIRDPARTYLIBS} -lxml2 -lcrypt -lcprops

# XXX these should be done in a more compact way
LDADD= \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtcrypt/libtcrypt.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libmgmtlog/libmgmtlog.so.1.0.0 \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/libcli/libnkncli.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcli/libcli.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libclish/libclish.a \
       ${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtacl/libtacl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nvsd/memalloc/libnvsd_memalloc.so.1.0.0 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncnt/libnkncnt.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncnt_client/libnkncnt_client.a\
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${CUST_LIB_DIR}/nkncounters/libnkn_counters.a \
       ${BUILD_EXTRA_LDADD}

USES_MODULES=1

include ${PROD_TREE_ROOT}/src/mk/prog.mk

