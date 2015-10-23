#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=tvirtd

SRCS= \
	vi_action.c         \
	vi_config.c         \
	vi_event.c          \
	vi_install.c        \
	vi_licensing.c      \
	vi_main.c           \
	vi_mon.c            \
	vi_scan.c           \
	vi_shared.c         \
	vi_state.c          \
	vi_structs.c        \
	vi_worker.c         \
	vi_wrappers.c       \
	vi_xml.c            \

CFLAGS=

INCLUDES+= -I/usr/include/libxml2

LDADD= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtxml/libtxml.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_mt/libtc_mt.a \
	-lvirt \
	-lxml2 \
	-lpthread \


include ${PROD_TREE_ROOT}/src/mk/prog.mk
