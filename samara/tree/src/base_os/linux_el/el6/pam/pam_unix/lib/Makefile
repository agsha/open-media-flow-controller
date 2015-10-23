#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=pam_unix
LIB_BUILD_DIR=${LIB_DIR}/security/pam_unix
LIB_INSTALL_DIR=lib/security
LIB_DYNAMIC=1

LIB_DYNAMIC_SONAME=
LIB_DYNAMIC_REALNAME=
LIB_DYNAMIC_PREFIX=

vpath %.c ../src

LICENSE_REQUIRES_REDIST=1

SRCS=   \
	bigcrypt.c \
	pam_unix_acct.c \
	pam_unix_auth.c \
	pam_unix_passwd.c \
	pam_unix_sess.c \
	support.c \
	yppasswd_xdr.c \
	md5_good.c \
	md5_broken.c \


INCLUDES=\
	-I../src \
	-I../../../../../linux_common/pam/include \


DEFINES=\
	-DHAVE_CONFIG_H \
	-DCHKPWD_HELPER=\"/sbin/unix_chkpwd\" \
	-DUPDATE_HELPER=\"/sbin/unix_update\" \
	-DPATH_RANDOMDEV=\"/dev/urandom\" \

##	-DUSE_CRACKLIB \

# XXX-PPC: SELinux is not currently supported on PowerPC

# NOTE: SELinux is only supported on x86 (i386 or x86_64)
# NOTE: ../../src/config.h must match this
# NOTE: stack-protector does not appear to work on PPC.
STACK_PROTECTOR=
ifeq (${PROD_TARGET_ARCH_FAMILY},X86)
DEFINES+=\
	-DWITH_SELINUX \

STACK_PROTECTOR=\
	-fstack-protector --param=ssp-buffer-size=4 \

endif

CFLAGS+=\
	-O2 -g -Wp,-D_FORTIFY_SOURCE=2 \
	-fexceptions ${STACK_PROTECTOR} \
	-fasynchronous-unwind-tables \


SELINUX_LIBS=
ifeq (${PROD_TARGET_ARCH_FAMILY},X86)

SELINUX_LIBS+= -lselinux

endif

LDADD+=\
	-lpam \
	-lcrypt \
	-lcrypto \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/linux_common/pam/lib/libpam_common/libpam_common.a \
	${SELINUX_LIBS} \
	-lnsl \
	-Wl,--as-needed \
	-Wl,-O1 \
	-Wl,--version-script=../src/modules.map \
	-Wl,-soname -Wl,pam_unix.so \

##	-lcrack \


include ${PROD_TREE_ROOT}/src/mk/lib.mk
