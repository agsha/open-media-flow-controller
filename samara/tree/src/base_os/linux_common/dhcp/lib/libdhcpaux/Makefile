#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

#
# This builds 'libdhcpaux', which is an amalgam of what the original DHCP
# package built into two libraries:
#   - libdst   ('dst' directory)
#   - libres   ('minires' directory)
#
# This is separate from libdhcp because it needs a couple of additional
# preprocessor constants (MINIRES_LIB and HMAC_MD5) to be #defined
# on the gcc command line.
# 

LIB=dhcpaux

vpath %.c ../../src/dst
vpath %.c ../../src/minires

#
# NOTE: the source filenames listed below were sorted alphabetically
# by full path under the 'src' subtree.  Then the directory name 
# was removed because of the vpath definitions above.  This should make it
# easier to regenerate the list in a consistent order for subsequent imports.
#

SRCS=   \
	base64.c \
	dst_api.c \
	dst_support.c \
	hmac_link.c \
	md5_dgst.c \
	prandom.c \
	\
	ns_date.c \
	ns_name.c \
	ns_parse.c \
	ns_samedomain.c \
	ns_sign.c \
	ns_verify.c \
	res_comp.c \
	res_findzonecut.c \
	res_init.c \
	res_mkquery.c \
	res_mkupdate.c \
	res_query.c \
	res_send.c \
	res_sendsigned.c \
	res_update.c \


LIB_BUILD_DIR=${ILIB_DIR}/base_os/linux_common/dhcp/lib/libdhcpaux

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/linux_common/dhcp/src \
	-I${PROD_TREE_ROOT}/src/base_os/linux_common/dhcp/src/includes \

DEFINES_OVERRIDE=1
DEFINES= \
	-DHAVE_CONFIG_H \
	-DLOCALSTATEDIR=\"/var\" \
	-DMINIRES_LIB \
	-DHMAC_MD5 \

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Werror \
	-Wall \

include ${PROD_TREE_ROOT}/src/mk/lib.mk
