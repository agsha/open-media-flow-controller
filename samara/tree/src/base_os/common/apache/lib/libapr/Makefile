#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=apr

vpath %.c ../../src/ext-srclib/apr/atomic/unix
vpath %.c ../../src/ext-srclib/apr/dso/unix
vpath %.c ../../src/ext-srclib/apr/file_io/unix
vpath %.c ../../src/ext-srclib/apr/locks/unix
vpath %.c ../../src/ext-srclib/apr/memory/unix
vpath %.c ../../src/ext-srclib/apr/misc/unix
vpath %.c ../../src/ext-srclib/apr/mmap/unix
vpath %.c ../../src/ext-srclib/apr/network_io/unix
vpath %.c ../../src/ext-srclib/apr/passwd
vpath %.c ../../src/ext-srclib/apr/poll/unix
vpath %.c ../../src/ext-srclib/apr/random/unix
vpath %.c ../../src/ext-srclib/apr/shmem/unix
vpath %.c ../../src/ext-srclib/apr/strings
vpath %.c ../../src/ext-srclib/apr/support/unix
vpath %.c ../../src/ext-srclib/apr/tables
vpath %.c ../../src/ext-srclib/apr/threadproc/unix
vpath %.c ../../src/ext-srclib/apr/time/unix
vpath %.c ../../src/ext-srclib/apr/user/unix


#
# NOTE: the source filenames listed below were sorted alphabetically
# by full path under the 'apr' subtree.  e.g. first those for 
# 'apr/atomic/unix', then for 'dso/unix', etc.  Then the directory name 
# was removed because of the vpath definitions above.  This should make it
# easier to regenerate the list in a consistent order for subsequent imports.
#

SRCS=   \
	builtins.c \
	mutex.c \
	dso.c \
	buffer.c \
	copy.c \
	dir.c \
	fileacc.c \
	filedup.c \
	filepath.c \
	filepath_util.c \
	filestat.c \
	flock.c \
	fullrw.c \
	mktemp.c \
	open.c \
	pipe.c \
	readwrite.c \
	seek.c \
	tempdir.c \
	global_mutex.c \
	proc_mutex.c \
	thread_cond.c \
	thread_mutex.c \
	thread_rwlock.c \
	apr_pools.c \
	charset.c \
	env.c \
	errorcodes.c \
	getopt.c \
	otherchild.c \
	rand.c \
	start.c \
	version.c \
	common.c \
	mmap.c \
	inet_ntop.c \
	inet_pton.c \
	multicast.c \
	sendrecv.c \
	sockaddr.c \
	sockets.c \
	sockopt.c \
	apr_getpass.c \
	epoll.c \
	kqueue.c \
	poll.c \
	port.c \
	select.c \
	apr_random.c \
	sha2_glue.c \
	sha2.c \
	shm.c \
	apr_cpystrn.c \
	apr_fnmatch.c \
	apr_snprintf.c \
	apr_strings.c \
	apr_strnatcmp.c \
	apr_strtok.c \
	waitio.c \
	apr_hash.c \
	apr_tables.c \
	proc.c \
	procsup.c \
	signals.c \
	thread.c \
	threadpriv.c \
	time.c \
	timestr.c \
	groupinfo.c \
	userinfo.c \


ifeq (${PROD_TARGET_ARCH_LC},ppc)
	SRCS+=ppc.c
else
	SRCS+=ia32.c
endif


LIB_BUILD_DIR=${ILIB_DIR}/base_os/common/apache/lib/libapr

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/ext-srclib/apr/include \
	-I${PROD_TREE_ROOT}/src/base_os/common/apache/src/ext-srclib/apr/include/arch/unix \

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
