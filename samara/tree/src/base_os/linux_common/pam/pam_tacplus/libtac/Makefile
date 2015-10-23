#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=tac
OBJ_PIC=1

vpath %.c ../src/libtac/lib

SRCS=   \
	acct_r.c \
	acct_s.c \
	attrib.c \
	authen_r.c \
	authen_s.c \
	author_r.c \
	author_s.c \
	connect.c \
	crypt.c \
	hdr_check.c \
	header.c \
	magic.c \
	md5.c \
	messages.c \
	version.c \
	xalloc.c \

LIB_BUILD_DIR=${ILIB_DIR}/base_os/linux_common/pam/pam_tacplus/libtac

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/linux_common/pam/pam_tacplus/src/libtac/include \
	-I${PROD_TREE_ROOT}/src/base_os/linux_common/pam/pam_tacplus/src/libtac/lib \
	-I${PROD_TREE_ROOT}/src/base_os/linux_common/pam/pam_tacplus/src

DEFINES= \
	-D_GNU_SOURCE \


#WARNINGS_OVERRIDE=1
#WARNINGS= \
#	-Werror \
#	-Wall \

ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

include ${PROD_TREE_ROOT}/src/mk/lib.mk
