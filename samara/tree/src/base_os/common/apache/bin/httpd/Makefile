#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=httpd
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/usr/sbin
#PROG_STRIP=1

vpath %.c ../../src
vpath %.c ../../src/os/unix
vpath %.c ../../src/server
vpath %.c ../../src/server/mpm/prefork

#
# NOTE: the source filenames listed below were sorted alphabetically
# by full path under the 'src' subtree.  e.g. first those for 'src' itself,
# then for 'src/os/unix', etc.  Then the directory name was removed because
# of the vpath definitions above.  This should make it easier to regenerate
# the list in a consistent order for subsequent imports.
#

SRCS=	\
	modules.c \
	unixd.c \
	buildmark.c \
	config.c \
	connection.c \
	core_filters.c \
	core.c \
	eoc_bucket.c \
	error_bucket.c \
	exports.c \
	listen.c \
	log.c \
	main.c \
	mpm_common.c \
	protocol.c \
	provider.c \
	request.c \
	scoreboard.c \
	util_cfgtree.c \
	util_charset.c \
	util_debug.c \
	util_ebcdic.c \
	util_filter.c \
	util_md5.c \
	util.c \
	util_pcre.c \
	util_script.c \
	util_time.c \
	util_xml.c \
	vhost.c \
	prefork.c \

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/ext-srclib/apr/include \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/ext-srclib/apr-util/include \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/modules/http \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/modules/mappers \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/include \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/os/unix \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/server/mpm/prefork \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/modules/filters \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/modules/proxy \

DEFINES= \
	-D_GNU_SOURCE  \
	-D_SVID_SOURCE \
	-DHAVE_CONFIG_H \
	-DLINUX=2 \
	-DHAVE_SYSLOG \
	-D_BSD_SOURCE \
	-pthread \

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Wall \

LDADD=  \
	-lssl \
	-lcrypto \
	-lexpat \
	-lm \
	-lcrypt \
	-lpthread \
	-lz \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/apache/lib/libmodules/libmodules.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/apache/lib/libaprutil/libaprutil.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/apache/lib/libapr/libapr.a \
	${PROD_BUILD_OUTPUT_DIR}/${INSTALL_TREE}/${ILIB_DIR}/base_os/common/apache/lib/libpcre/libpcre.a \

ifneq (${PROD_TARGET_OS},FREEBSD)
# XXX/EMT: do we need libdl for something?  Apache links without it...
LDADD+= \
	-lrt \
	-ldl \

else
LDADD+= \
	-L/usr/local/lib \

endif

# PowerPC does not support selinux
ifeq (${PROD_TARGET_ARCH_FAMILY},X86)
LDADD+= \
	-lselinux \

endif

ifeq (${PROD_TARGET_OS},LINUX)
    LDFLAGS+= \
	-Wl,--export-dynamic

endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk
