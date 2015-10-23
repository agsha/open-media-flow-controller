include ${PROD_TREE_ROOT}/src/mk/common.mk

S=../../..

#
#
OBJS = \
	ip_input.o \
	ip_icmp.o \
	ip_output.o \
	ip_options.o \
	kern_mbuf.o \
	uipc_mbuf.o \
	uipc_mbuf2.o \
	in_cksum.o \
	raw_ip.o \
	in_pcb.o \
	tcp_hostcache.o \
	tcp_output.o \
	tcp_sack.o \
	tcp_timer.o \
	tcp_usrreq.o \
	tcp_input.o \
	tcp_reass.o \
	tcp_syncache.o \
	tcp_timewait.o \
	tcp_subr.o \
	strcpy.o \
	strcat.o \
	strlen.o \
	strlcpy.o \
	strncmp.o \
	strtoq.o \
	uipc_socket.o \
	subr_prf.o \
	in_proto.o \
	inet_ntoa.o \
	uipc_sockbuf.o \
	udp_usrreq.o \
	igmp.o \
	arc4random.o \
	md5c.o \
	nkns_in.o \
	nkns_sysctl.o \
	subr_msgbuf.o \
	nkns_init_main.o \
	in_mcast.o \
	route.o \
	bcd.o \
	nkns_osl.o \
	nkns_api.o \
	nkns_debug.o \
	uipc_domain.o \
	subr_unit.o \
	init_main.o \
	uipc_syscalls.o \
	sys_socket.o \
	kern_timeout.o \
	nkns_umacore.o \
	nkns_mem.o

#
SOURCES = \
	$S/netinet/ip_input.c \
	$S/netinet/ip_icmp.c \
	$S/netinet/ip_output.c \
	$S/netinet/ip_options.c \
	$S/kern/kern_mbuf.c \
	$S/kern/uipc_mbuf.c \
	$S/kern/uipc_mbuf2.c \
	$S/i386/i386/in_cksum.c \
	$S/i386/i386/support.s \
	$S/netinet/raw_ip.c \
	$S/netinet/in_pcb.c \
	$S/netinet/tcp_hostcache.c \
	$S/netinet/tcp_output.c \
	$S/netinet/tcp_sack.c \
	$S/netinet/tcp_timer.c \
	$S/netinet/tcp_usrreq.c \
	$S/netinet/tcp_input.c \
	$S/netinet/tcp_reass.c \
	$S/netinet/tcp_syncache.c \
	$S/netinet/tcp_timewait.c \
	$S/netinet/tcp_subr.c \
	$S/libkern/strcpy.c \
	$S/libkern/strcat.c \
	$S/libkern/strlen.c \
	$S/libkern/strlcpy.c \
	$S/libkern/strncmp.o \
	$S/libkern/strtoq.c \
	$S/kern/uipc_socket.c \
	$S/kern/subr_prf.c \
	$S/netinet/in_proto.c \
	$S/libkern/inet_ntoa.c \
	$S/kern/uipc_sockbuf.c \
	$S/netinet/udp_usrreq.c \
	$S/netinet/igmp.c \
	$S/libkern/arc4random.c \
	$S/kern/md5c.c \
	$S/nknsshim/nkns_in.c \
	$S/nknshim/nkns_sysctl.c \
	$S/kern/subr_msgbuf.c \
	$S/nknshim/nkns_init_main.c \
	$S/netinet/in_mcast.c \
	$S/net/route.c \
	$S/libkern/bcd.c \
	$S/nknshim/nkns_osl.c \
	$S/nknshim/nkns_api.c \
	$S/nknshim/nkns_debug.c \
	$S/kern/uipc_domain.c \
	$S/kern/subr_unit.c \
	$S/kern/init_main.c \
	$S/kern/uipc_syscalls.c \
	$S/kern/sys_socket.c \
	$S/nknshim/nkns_umacore.c \
	$S/kern/kern_timeout.c \
	$S/nknshim/nkns_mem.c

# redefine inbuilt macros and define new ones
CC   = cc -c # Use gcc as the default C compiler

CFLAGS = -g -fPIC -O2 -pipe -fno-strict-aliasing  -std=c99  -Wall -Wredundant-decls -Wnested-externs -Wstrict-prototypes  -Wmissing-prototypes -Wpointer-arith -Winline -Wcast-qual  -Wundef -nostdinc  -I. -I../../.. -I../../../contrib/altq -D_KERNEL -DHAVE_KERNEL_OPTION_HEADERS -include opt_global.h -fno-common -finline-limit=8000 --param inline-unit-growth=100 --param large-function-growth=1000   -mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-sse3 -ffreestanding -Werror -rdynamic -DVERBOSE_SYSINIT -DZERO_COPY_SOCKETS 

# -DZERO_COPY_SOCKETS

SFLAGS = -fPIC -x assembler-with-cpp -DLOCORE -O2 -pipe -fno-strict-aliasing  -std=c99  -Wall -Wredundant-decls -Wnested-externs -Wstrict-prototypes  -Wmissing-prototypes -Wpointer-arith -Winline -Wcast-qual  -Wundef -nostdinc  -I. -I../../.. -I../../../contrib/altq -D_KERNEL -DHAVE_KERNEL_OPTION_HEADERS -include opt_global.h -fno-common -finline-limit=8000 --param inline-unit-growth=100 --param large-function-growth=1000   -mno-mmx -mno-3dnow -mno-sse -mno-sse2 -mno-sse3 -ffreestanding -Werror -DZERO_COPY_SOCKETS 

BSDLD   = gcc -shared # Use gcc as the default C compiler
LIBS =        # Add any libraries required

SO_DIR=${PROD_INSTALL_OUTPUT_DIR}/image/lib/nkn
SO_TARGET=${SO_DIR}/libstack.so.1.0.1

# Default target
all: libstack

# Actual target and dependencies
libstack: $(OBJS)
	@mkdir -p $(SO_DIR)
	$(BSDLD)  -Wl,-soname,libstack.so.1 -o $(SO_TARGET) $(OBJS) 
	@echo "compilation done"

test: test.o
	gcc -o test test.o

# Target deleting unwanted files
clean:
	\rm -f *.o *~ libstack.so.1.0.1 core.*

#
#
#

ip_input.o: $S/netinet/ip_input.c
	$(CC) $(CFLAGS) $<


ip_icmp.o: $S/netinet/ip_icmp.c
	$(CC) $(CFLAGS) $<

ip_output.o: $S/netinet/ip_output.c 
	$(CC) $(CFLAGS) $<

ip_options.o: $S/netinet/ip_options.c 
	$(CC) $(CFLAGS) $<

kern_mbuf.o: $S/kern/kern_mbuf.c 
	$(CC) $(CFLAGS) $<

uipc_mbuf.o: $S/kern/uipc_mbuf.c 
	$(CC) $(CFLAGS) $<

uipc_mbuf2.o: $S/kern/uipc_mbuf2.c 
	$(CC) $(CFLAGS) $<

in_cksum.o: $S/i386/i386/in_cksum.c 
	$(CC) $(CFLAGS) $<

support.o: $S/i386/i386/support.s 
	$(CC) $(SFLAGS) $<

raw_ip.o: $S/netinet/raw_ip.c 
	$(CC) $(CFLAGS) $<

in_pcb.o: $S/netinet/in_pcb.c 
	$(CC) $(CFLAGS) $<

tcp_hostcache.o: $S/netinet/tcp_hostcache.c 
	$(CC) $(CFLAGS) $<

tcp_output.o: $S/netinet/tcp_output.c 
	$(CC) $(CFLAGS) $<

tcp_sack.o: $S/netinet/tcp_sack.c 
	$(CC) $(CFLAGS) $<

tcp_timer.o: $S/netinet/tcp_timer.c 
	$(CC) $(CFLAGS) $<

tcp_usrreq.o: $S/netinet/tcp_usrreq.c 
	$(CC) $(CFLAGS) $<

tcp_input.o: $S/netinet/tcp_input.c 
	$(CC) $(CFLAGS) $<

tcp_reass.o: $S/netinet/tcp_reass.c 
	$(CC) $(CFLAGS) $<

tcp_syncache.o: $S/netinet/tcp_syncache.c 
	$(CC) $(CFLAGS) $<

tcp_timewait.o: $S/netinet/tcp_timewait.c 
	$(CC) $(CFLAGS) $<

tcp_subr.o: $S/netinet/tcp_subr.c 
	$(CC) $(CFLAGS) $<

strcpy.o: $S/libkern/strcpy.c 
	$(CC) $(CFLAGS) $<

strcat.o: $S/libkern/strcat.c 
	$(CC) $(CFLAGS) $<

strlen.o: $S/libkern/strlen.c 
	$(CC) $(CFLAGS) $<

strlcpy.o: $S/libkern/strlcpy.c 
	$(CC) $(CFLAGS) $<

strncmp.o: $S/libkern/strncmp.c 
	$(CC) $(CFLAGS) $<

strtoq.o: $S/libkern/strtoq.c 
	$(CC) $(CFLAGS) $<

subr_prf.o: $S/kern/subr_prf.c 
	$(CC) $(CFLAGS) $<

in_proto.o: $S/netinet/in_proto.c 
	$(CC) $(CFLAGS) $<

inet_ntoa.o: $S/libkern/inet_ntoa.c 
	$(CC) $(CFLAGS) $<

uipc_socket.o: $S/kern/uipc_socket.c 
	$(CC) $(CFLAGS) $<

uipc_sockbuf.o: $S/kern/uipc_sockbuf.c 
	$(CC) $(CFLAGS) $<

udp_usrreq.o: $S/netinet/udp_usrreq.c 
	$(CC) $(CFLAGS) $<

igmp.o: $S/netinet/igmp.c 
	$(CC) $(CFLAGS) $<

arc4random.o: $S/libkern/arc4random.c 
	$(CC) $(CFLAGS) $<

md5c.o: $S/kern/md5c.c 
	$(CC) $(CFLAGS) $<

nkns_in.o: $S/nknshim/nkns_in.c 
	$(CC) $(CFLAGS) $<

nkns_sysctl.o: $S/nknshim/nkns_sysctl.c 
	$(CC) $(CFLAGS) $<

subr_msgbuf.o: $S/kern/subr_msgbuf.c 
	$(CC) $(CFLAGS) $<

nkns_init_main.o: $S/nknshim/nkns_init_main.c 
	$(CC) $(CFLAGS) $<

kern_jail.o: $S/kern/kern_jail.c 
	$(CC) $(CFLAGS) $<

in_mcast.o: $S/netinet/in_mcast.c 
	$(CC) $(CFLAGS) $<

route.o: $S/net/route.c 
	$(CC) $(CFLAGS) $<

bcd.o: $S/libkern/bcd.c 
	$(CC) $(CFLAGS) $<

nkns_osl.o: $S/nknshim/nkns_osl.c 
	$(CC) $(CFLAGS) $<

nkns_api.o: $S/nknshim/nkns_api.c 
	$(CC) $(CFLAGS) $<

nkns_debug.o: $S/nknshim/nkns_debug.c 
	$(CC) $(CFLAGS) $<

uipc_domain.o: $S/kern/uipc_domain.c 
	$(CC) $(CFLAGS) $<

subr_unit.o: $S/kern/subr_unit.c 
	$(CC) $(CFLAGS) $<

init_main.o: $S/kern/init_main.c 
	$(CC) $(CFLAGS) $<

uipc_syscalls.o: $S/kern/uipc_syscalls.c 
	$(CC) $(CFLAGS) $<

sys_socket.o: $S/kern/sys_socket.c 
	$(CC) $(CFLAGS) $<

uma_core.o: $S/vm/uma_core.c 
	$(CC) $(CFLAGS) $<

kern_timeout.o: $S/kern/kern_timeout.c 
	$(CC) $(CFLAGS) $<

in.o: $S/netinet/in.c 
	$(CC) $(CFLAGS) $<

nkn_ks_base.o: $S/nknshim/nkn_ks_base.c 
	$(CC) $(CFLAGS) $<

nkn_ks_parse.o: $S/nknshim/nkn_ks_parse.c 
	$(CC) $(CFLAGS) $<

nkn_ks_socket.o: $S/nknshim/nkn_ks_socket.c 
	$(CC) $(CFLAGS) $<

nkns_umacore.o: $S/nknshim/nkns_umacore.c 
	$(CC) $(CFLAGS) $<

nkns_mem.o: $S/nknshim/nkns_mem.c 
	$(CC) $(CFLAGS) $<

test.o: ./test.c 
	$(CC) $(CFLAGS) $<


