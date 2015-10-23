#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=pam_ldap
LIB_BUILD_DIR=${LIB_DIR}/security/pam_ldap
LIB_INSTALL_DIR=lib/security
LIB_DYNAMIC=1
LDADD+= \
	-lresolv \
	-lldap \
	-llber \
	-lcrypt \
	-lcrypto \
	-lresolv \
	-lpam \
	-ldl \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/linux_common/pam/lib/libpam_common/libpam_common.a \

LIB_DYNAMIC_SONAME=
LIB_DYNAMIC_REALNAME=
LIB_DYNAMIC_PREFIX=

LICENSE_REQUIRES_REDIST=1

vpath %.c ../src

SRCS=   \
	pam_ldap.c \
	md5.c \
	resolve.c \
	dnsconfig.c \

INCLUDES= \
	-I../src \
	-I../../include \

DEFINES=\
	-D_GNU_SOURCE \
	-DHAVE_CONFIG_H \
	-DLDAP_REFERRALS \
	-DLDAP_DEPRECATED \


include ${PROD_TREE_ROOT}/src/mk/lib.mk
