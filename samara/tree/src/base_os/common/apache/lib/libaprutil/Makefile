#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=aprutil

vpath %.c ../../src/ext-srclib/apr-util/buckets/
vpath %.c ../../src/ext-srclib/apr-util/crypto/
vpath %.c ../../src/ext-srclib/apr-util/dbm/
vpath %.c ../../src/ext-srclib/apr-util/dbm/sdbm/
vpath %.c ../../src/ext-srclib/apr-util/encoding/
vpath %.c ../../src/ext-srclib/apr-util/hooks/
vpath %.c ../../src/ext-srclib/apr-util/ldap/
vpath %.c ../../src/ext-srclib/apr-util/memcache/
vpath %.c ../../src/ext-srclib/apr-util/misc/
vpath %.c ../../src/ext-srclib/apr-util/strmatch/
vpath %.c ../../src/ext-srclib/apr-util/uri/
vpath %.c ../../src/ext-srclib/apr-util/xlate/
vpath %.c ../../src/ext-srclib/apr-util/xml/

#
# NOTE: the source filenames listed below were sorted alphabetically
# by full path under the 'apr-util' subtree.  e.g. first those for 
# 'apr-util/buckets', then for 'apr-util/crypto', etc.  Then the directory
# name was removed because of the vpath definitions above.  This should 
# make it easier to regenerate the list in a consistent order for subsequent
# imports.
#

SRCS=   \
	apr_brigade.c \
	apr_buckets_alloc.c \
	apr_buckets_eos.c \
	apr_buckets_file.c \
	apr_buckets_flush.c \
	apr_buckets_heap.c \
	apr_buckets_mmap.c \
	apr_buckets.c \
	apr_buckets_pipe.c \
	apr_buckets_pool.c \
	apr_buckets_refcount.c \
	apr_buckets_simple.c \
	apr_buckets_socket.c \
	apr_md4.c \
	apr_md5.c \
	apr_sha1.c \
	getuuid.c \
	uuid.c \
	apr_dbm.c \
	apr_dbm_sdbm.c \
	sdbm_hash.c \
	sdbm_lock.c \
	sdbm.c \
	sdbm_pair.c \
	apr_base64.c \
	apr_hooks.c \
	apr_ldap_stub.c \
	apr_ldap_url.c \
	apr_memcache.c \
	apr_date.c \
	apr_queue.c \
	apr_reslist.c \
	apr_rmm.c \
	apr_thread_pool.c \
	apu_dso.c \
	apu_version.c \
	apr_strmatch.c \
	apr_uri.c \
	xlate.c \
	apr_xml.c \

LIB_BUILD_DIR=${ILIB_DIR}/base_os/common/apache/lib/libaprutil

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/ext-srclib/apr-util/include \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/ext-srclib/apr-util/include/private \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/ext-srclib/apr/include \

DEFINES= \
	-D_GNU_SOURCE  \
	-D_SVID_SOURCE \
	-DHAVE_CONFIG_H \
	-DLINUX=2 \
	-D_BSD_SOURCE \

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Werror \
	-Wall \

include ${PROD_TREE_ROOT}/src/mk/lib.mk
