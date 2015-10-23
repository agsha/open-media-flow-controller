#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=modules


vpath %.c ../../src/modules/aaa
vpath %.c ../../src/modules/filters
vpath %.c ../../src/modules/generators
vpath %.c ../../src/modules/http
vpath %.c ../../src/modules/loggers
vpath %.c ../../src/modules/mappers
vpath %.c ../../src/modules/metadata
vpath %.c ../../src/modules/ssl
vpath %.c ../../src/modules/proxy

#
# XXX/EMT: as with the apr and apr-util libraries, we would have liked to
# have the filenames here sorted first by path, to make it easier to 
# regenerate the list in the same order after an update.  But here, we
# have tried to preserve the ordering from before, to make the diffs
# more clear.
#

SRCS=   \
	mod_auth_basic.c \
	mod_authn_default.c \
	mod_authn_file.c \
	mod_authz_default.c \
	mod_authz_groupfile.c \
	mod_authz_host.c \
	mod_authz_user.c \
	mod_filter.c \
	byterange_filter.c \
	chunk_filter.c \
	mod_log_config.c \
	mod_env.c \
	mod_headers.c \
	mod_deflate.c \
	mod_version.c \
	mod_ssl.c \
	ssl_engine_config.c \
	ssl_engine_dh.c \
	ssl_engine_init.c \
	ssl_engine_io.c \
	ssl_engine_kernel.c \
	ssl_engine_log.c \
	ssl_engine_mutex.c \
	ssl_engine_pphrase.c \
	ssl_engine_rand.c \
	ssl_engine_vars.c \
	ssl_expr.c \
	ssl_expr_eval.c \
	ssl_expr_parse.c \
	ssl_expr_scan.c \
	ssl_scache.c \
	ssl_scache_dbm.c \
	ssl_scache_dc.c \
	ssl_scache_shmcb.c \
	ssl_util.c \
	ssl_util_ssl.c \
	http_core.c \
	http_etag.c \
	http_filters.c \
	http_protocol.c \
	http_request.c \
	mod_mime.c \
	mod_autoindex.c \
	mod_cgi.c \
	mod_dir.c \
	mod_actions.c \
	mod_alias.c \
	mod_so.c \
	mod_include.c \
	mod_setenvif.c \
	mod_asis.c \
	mod_rewrite.c \
	mod_status.c \
	mod_info.c \
	mod_proxy.c \
	proxy_util.c \
	mod_proxy_http.c \


LIB_BUILD_DIR=${ILIB_DIR}/base_os/common/apache/lib/libmodules

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/ext-srclib/apr/include \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/ext-srclib/apr-util/include \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/modules/http \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/modules/ssl \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/modules/generators \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/include \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/os/unix \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/server/mpm/prefork \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/modules/filters \

DEFINES= \
	-D_GNU_SOURCE  \
	-D_SVID_SOURCE \
	-DHAVE_CONFIG_H \
	-DLINUX=2 \
	-D_BSD_SOURCE \
	-D_REENTRANT \

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Werror \
	-Wall \
	-Wno-unused \

# Graft point for customer-specific additions, generally to add Apache modules.
-include ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT_LC}/src/base_os/common/apache/lib/libmodules/Makefile.inc

include ${PROD_TREE_ROOT}/src/mk/lib.mk
