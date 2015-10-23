#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=common


SRCS= \
	addr_utils.c \
	addr_utils_ipv4u32.c \
	array.c \
	base64.c \
	backtrace.c \
	bsdstr.c \
	bswap.c \
	common.c \
	core_utils.c \
	dso.c \
	encoding.c \
	env_utils.c \
	errors.c \
	fd_mux.c \
	fdintrcb.c \
	file_utils.c \
	ftrans_curl.c \
	ftrans_scp.c \
	ftrans_sftp.c \
	ftrans_ssh.c \
	ftrans_tftp.c \
	ftrans_utils.c \
	hash_table.c \
	image_utils.c \
	hmac.c \
	keygen.c \
	lockfile.c \
	logging.c \
	login_utils.c \
	ltree.c \
	malloc_utils.c \
	md5c.c \
	mdreq_client.c \
	misc.c \
	node_enums.c \
	perf.c \
	proc_utils.c \
	proc_utils_platform.c \
	random.c \
	sha256c.c \
	signal_utils.c \
	smprintf.c \
	snmp_crypt.c \
	str_array.c \
	string_map.c \
	tbuffer.c \
	tdefaults.c \
	tdict.c \
	timezone.c \
	threading.c \
	tmalloc_hooks.c \
	tnet_utils.c \
	tpaths.c \
	tree.c \
	tstr_array.c \
	tstring.c \
	ttime.c \
	ttime_sys.c \
	type_conv.c \
	typed_arrays.c \
	unicode_utils.c \
	url_utils.c \
	tuuid.c \
	zonechange.c \
	tty_size.c \
	escape_utils.c \

ifneq (${PROD_FEATURE_WGET}, 0)
	SRCS+=\
		ftrans_wget.c \

endif


# Build version info: generated c file
SRCS+=	build_version.c \

CFLAGS=

LDADD=

ifneq (${PROD_FEATURE_I18N_SUPPORT}, 0)
	GETTEXT_PACKAGE=lib${LIB}
	DEFINES+=-DGETTEXT_DOMAIN=\"${GETTEXT_PACKAGE}\"
	SUBMAKEFILES+=Makefile.i18n
endif

ifeq (${DSO_LOG_VERBOSE},1)
	DEFINES+=-DDSO_LOG_VERBOSE=1
endif

# Build version info: dependency to create .c file
LIB_PRE_BUILD_TARGET=${PROD_BUILD_OUTPUT_DIR}/release/build_version.c

include ${PROD_TREE_ROOT}/src/mk/lib.mk


# Build version info: rule to create .c file
${PROD_BUILD_OUTPUT_DIR}/release/build_version.c:
	(cd ${PROD_SRC_ROOT}/release && ${MAKE} version)
	${MAKE} ${FULL_BUILD_DIR}/build_version.o

# Build version info: path to finding the generated build_version.c file
vpath %.c ${PROD_BUILD_OUTPUT_DIR}/release

