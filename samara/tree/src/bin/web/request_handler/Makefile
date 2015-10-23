#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=rh
PROG_INSTALL_DIR=/lib/web/handlers

SRCS= \
	rh_debug.c             \
	rh_main.c              \
	web_form_utils.c       \
	web_internal.c         \
	web_modules.c          \
	web_tcl.c              \
	web_tcl_cmds.c         \
	web_template.c	       \

SUBDIRS=                       \
	modules                \


CFLAGS= -I${PROD_TREE_ROOT}/src/lib/libgcl

LIB_TCL=-ltcl
ifeq (${PROD_TARGET_OS},FREEBSD)
	LIB_TCL=-L/usr/local/lib -ltcl84
endif

# XXX these should be done in a more compact way
LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libwebcli/libwebcli.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libweb/libweb.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcli/libcli.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtacl/libtacl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libclish/libclish.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libmdc_wrapper/libmdc_wrapper.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libgcl/libgcl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libevent/libevent.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcgi/libcgi.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libwebtmpl/libwebtmpl.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtcrypt/libtcrypt.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libcommon/libcommon.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${LIB_DIR}/libtc_st/libtc_st.a \
	${LIB_TCL} \
	-lcrypt \

# Graft point for customer-specific additions to build options for rh.
# e.g. a customer may want to make rh link with additional libraries
# by adding stuff to LDADD.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/bin/web/request_handler/Makefile.inc

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=${PROG}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

USES_MODULES=1

ifdef STATIC_MODULES
    CFLAGS+=-DSTATIC_MODULES
    LDADD+= \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/web/request_handler/modules/libweb_mods.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/bin/cli/modules/libcli_mods.a \

endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk

