#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

#
# This builds 'libdhcp', which is an amalgam of what the original DHCP
# package built into two libraries:
#   - libdhcp  ('common' directory)
#   - libomapi ('omapip' directory)
#
# It also includes tms_dhcp_utils.c, which is something we added to
# the 'common' directory.
#
# Note: libdst and libres are built separately, into libdhcpaux.
#

LIB=dhcp

vpath %.c ../../src/common
vpath %.c ../../src/omapip

#
# NOTE: the source filenames listed below were sorted alphabetically
# by full path under the 'src' subtree.  Then the directory name 
# was removed because of the vpath definitions above.  This should make it
# easier to regenerate the list in a consistent order for subsequent imports.
#
# We had to use paths to alloc.c and dispatch.c, rather than relying on
# vpath, because there is a copy of each in both 'common' and 'omapip'
# directories!
#

SRCS=   \
	../../src/common/alloc.c \
	bpf.c \
	comapi.c \
	conflex.c \
	ctrace.c \
	discover.c \
	../../src/common/dispatch.c \
	dlpi.c \
	dns.c \
	ethernet.c \
	execute.c \
	fddi.c \
	heap.c \
	icmp.c \
	inet.c \
	lpf.c \
	memory.c \
	nit.c \
	options.c \
	packet.c \
	parse.c \
	print.c \
	raw.c \
	resolv.c \
	socket.c \
	tables.c \
	tree.c \
	tr.c \
	upf.c \
	\
	tms_dhcp_utils.c \
	\
	../../src/omapip/alloc.c \
	array.c \
	auth.c \
	buffer.c \
	connection.c \
	convert.c \
	../../src/omapip/dispatch.c \
	errwarn.c \
	generic.c \
	handle.c \
	hash.c \
	inet_addr.c \
	iscprint.c \
	listener.c \
	message.c \
	mrtrace.c \
	protocol.c \
	result.c \
	support.c \
	toisc.c \
	trace.c \


LIB_BUILD_DIR=${ILIB_DIR}/base_os/linux_common/dhcp/lib/libdhcp

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/linux_common/dhcp/src \
	-I${PROD_TREE_ROOT}/src/base_os/linux_common/dhcp/src/includes \

DEFINES_OVERRIDE=1
DEFINES= \
	-DHAVE_CONFIG_H \
	-DLOCALSTATEDIR=\"/var\" \

WARNINGS_OVERRIDE=1
WARNINGS= \
	-Werror \
	-Wall \

include ${PROD_TREE_ROOT}/src/mk/lib.mk
