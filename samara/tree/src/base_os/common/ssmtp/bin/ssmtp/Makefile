#  Makefile
#
# 
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=ssmtp
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/sbin
PROG_STRIP=1

LICENSE_REQUIRES_REDIST=1

vpath %.c ../../src
vpath %.c ${PROD_TREE_ROOT}/src/lib/libcommon
vpath %.c ${PROD_TREE_ROOT}/src/lib/libcommon/BIN_DIST

SRCS=	\
	ssmtp.c \
	arpadate.c \
	base64.c \
	escape_utils.c \

INCLUDES= \
	-I../../src

DEFINES= \
	-D_BSD_SOURCE         \
	-DSTDC_HEADERS=1      \
	-DHAVE_LIMITS_H=1     \
	-DHAVE_STRINGS_H=1    \
	-DHAVE_SYSLOG_H=1     \
	-DHAVE_UNISTD_H=1     \
	-DHAVE_LIBNSL=1       \
	-DRETSIGTYPE=void     \
	-DHAVE_VPRINTF=1      \
	-DHAVE_GETHOSTNAME=1  \
	-DHAVE_SOCKET=1       \
	-DHAVE_STRDUP=1       \
	-DHAVE_STRSTR=1       \
	-DREWRITE_DOMAIN=1    \
	-DSSMTPCONFDIR=\"/etc\" \
	-DHASTO_OPTION=1      \
	-DINET6=1             \
	-DHAVE_SSL=1          \
	-DCONFIGURATION_FILE=\"/etc/ssmtp.conf\"          \
	-DREVALIASES_FILE=\"/etc/revaliases\"

WARNINGS_OVERRIDE=1

LDADD=  \
        -lssl \
        -lcrypto \

ifeq ($(call CC_VERSION_ATLEAST,40000,1,0),1)
	WARNINGS+= \
		-Wno-pointer-sign \

endif

include ${PROD_TREE_ROOT}/src/mk/prog.mk
