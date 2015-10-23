#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=pam_disabled
LIB_BUILD_DIR=${LIB_DIR}/security/pam_disabled
LIB_INSTALL_DIR=lib/security
LIB_DYNAMIC=1

LIB_DYNAMIC_SONAME=
LIB_DYNAMIC_REALNAME=
LIB_DYNAMIC_PREFIX=

vpath %.c ../src

SRCS=   \
	pam_disabled.c \

INCLUDES=\
	-I../src \


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
	-Wl,--as-needed \
	-Wl,-soname -Wl,pam_disabled.so \


include ${PROD_TREE_ROOT}/src/mk/lib.mk
