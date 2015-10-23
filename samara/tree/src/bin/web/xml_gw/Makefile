#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=xg
PROG_INSTALL_DIR=/lib/web/handlers

SRCS= \
	xg_main.c            \
	xg_web.c             \
	xg_xfile.c           \
	xg_xml.c             \
	xg_request.c         \
	xg_response.c        \

INCLUDES+= -I/usr/include/libxml2

CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libgcl

# Needed for libxml2
WARNINGS+= -Wno-missing-format-attribute

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

ifneq (${PROD_TARGET_OS},LINUX)
	INCLUDES+= -I/usr/local/include/libxml2
	LDADD+=-L/usr/local/lib
endif

LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libweb/libweb.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtacl/libtacl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcgi/libcgi.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	-lxml2 \
	-lpthread \
	-lz \
	-lm \


include ${PROD_TREE_ROOT}/src/mk/prog.mk

