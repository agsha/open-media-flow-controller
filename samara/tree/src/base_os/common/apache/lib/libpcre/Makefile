#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=pcre

vpath %.c ../../src/ext-srclib/pcre

SRCS=   \
	pcre_chartables.c \
	pcre_compile.c \
	pcre_config.c \
	pcre_dfa_exec.c \
	pcre_exec.c \
	pcre_fullinfo.c \
	pcre_get.c \
	pcre_globals.c \
	pcre_info.c \
	pcre_maketables.c \
	pcre_newline.c \
	pcre_ord2utf8.c \
	pcreposix.c \
	pcre_refcount.c \
	pcre_study.c \
	pcre_tables.c \
	pcre_try_flipped.c \
	pcre_ucd.c \
	pcre_valid_utf8.c \
	pcre_version.c \
	pcre_xclass.c

LIB_BUILD_DIR=${ILIB_DIR}/base_os/common/apache/lib/libpcre

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/ \

DEFINES= \
	-D_GNU_SOURCE  \
	-D_SVID_SOURCE \
	-DHAVE_CONFIG_H \
	-DLINUX=2 \
	-D_BSD_SOURCE \
	-D_REENTRANT \
	-DAP_HAVE_DESIGNATED_INITIALIZER \

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Werror \
	-Wall \
	-Wno-unused \

include ${PROD_TREE_ROOT}/src/mk/lib.mk
