#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=pam_tallybyname
LIB_BUILD_DIR=${LIB_DIR}/security/pam_tallybyname
LIB_INSTALL_DIR=lib/security
LIB_DYNAMIC=1

LIB_DYNAMIC_SONAME=
LIB_DYNAMIC_REALNAME=
LIB_DYNAMIC_PREFIX=

vpath %.c ../src

SRCS=   \
	pam_tallybyname_mod.c \
	pam_tallybyname_common.c \
	sqlite_utils.c \

INCLUDES=\
	-I../src \
	-I../../include \

DEFINES=\
	-D_GNU_SOURCE \

# NOTE: stack-protector does not appear to work on PPC.
STACK_PROTECTOR=
ifeq (${PROD_TARGET_ARCH_FAMILY},X86)

STACK_PROTECTOR=\
	-fstack-protector --param=ssp-buffer-size=4 \

endif

CFLAGS+=\
	-g -Wp,-D_FORTIFY_SOURCE=2 \
	-fexceptions ${STACK_PROTECTOR} \
	-fasynchronous-unwind-tables \


LDADD+=\
	-lpam \
	-lcrypt \
	-lcrypto \
	-lsqlite3 \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/linux_common/pam/lib/libpam_common/libpam_common.a \
	-Wl,--as-needed \
	-Wl,-soname -Wl,pam_tallybyname.so \

include ${PROD_TREE_ROOT}/src/mk/lib.mk
