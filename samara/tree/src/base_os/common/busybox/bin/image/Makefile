#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

PROG=busybox

INSTALL_TREE=image
PROD_INSTALL_PREFIX=
PROG_INSTALL_DIR=/sbin
PROG_STRIP=1
PROG_STRIP_FLAGS=--remove-section=.note --remove-section=.comment

LICENSE_REQUIRES_REDIST=1

WARNINGS_OVERRIDE=1
DEFINES_OVERRIDE=1
DEBUG_OVERRIDE=1

WARNINGS=\
	-Wall \
	-Wshadow \
	-Wwrite-strings \
	-Wundef \
	-Wstrict-prototypes \
	-Wunused \
	-Wunused-parameter \
	-Wunused-function \
	-Wunused-value \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wdeclaration-after-statement \
	-Wold-style-definition \

DEFINES=-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
	-D_GNU_SOURCE \
	-DNDEBUG \
	-DBB_VER=\"1.20.2\" -DBB_BT=AUTOCONF_TIMESTAMP \
	-DENABLE_WERROR \

PATH_TO_SRC=../../src

INCLUDES= \
	-I${PATH_TO_SRC}/include \
	-I${PATH_TO_SRC}/libbb \
	-include ${PATH_TO_SRC}/include/autoconf.h \

ifeq (${PROD_TARGET_PLATFORM_FULL}-${ROOTFLOP_OLD_BINARYS},LINUX_EL_EL5-1)
	export PROD_TARGET_CPU_WORDSIZE=32
endif

CFLAGS=	\
	-funsigned-char \
	-static-libgcc \
	-Os \

LDADD+= \


# Note that this is like partially strip'ing the binary
LDFLAGS+=-s

# XXX!!! Warning!  Dependencies currently DO NOT WORK with directory names like this!

vpath %.c ../../src

SRCS=                      \
	applets/applets.c \
	archival/bbunzip.c \
	archival/gzip.c \
	archival/libarchive/data_align.c \
	archival/libarchive/data_extract_all.c \
	archival/libarchive/data_extract_to_stdout.c \
	archival/libarchive/data_skip.c \
	archival/libarchive/decompress_bunzip2.c \
	archival/libarchive/decompress_gunzip.c \
	archival/libarchive/decompress_unlzma.c \
	archival/libarchive/decompress_unxz.c \
	archival/libarchive/filter_accept_all.c \
	archival/libarchive/filter_accept_list.c \
	archival/libarchive/filter_accept_reject_list.c \
	archival/libarchive/find_list_entry.c \
	archival/libarchive/header_list.c \
	archival/libarchive/header_skip.c \
	archival/libarchive/header_verbose_list.c \
	archival/libarchive/init_handle.c \
	archival/libarchive/open_transformer.c \
	archival/libarchive/seek_by_jump.c \
	archival/libarchive/seek_by_read.c \
	console-tools/chvt.c \
	console-tools/clear.c \
	console-tools/deallocvt.c \
	console-tools/dumpkmap.c \
	console-tools/fgconsole.c \
	console-tools/kbd_mode.c \
	console-tools/loadfont.c \
	console-tools/loadkmap.c \
	console-tools/openvt.c \
	console-tools/reset.c \
	console-tools/resize.c \
	console-tools/setconsole.c \
	console-tools/setkeycodes.c \
	console-tools/setlogcons.c \
	console-tools/showkey.c \
	coreutils/basename.c \
	coreutils/cal.c \
	coreutils/cat.c \
	coreutils/chgrp.c \
	coreutils/chmod.c \
	coreutils/chown.c \
	coreutils/chroot.c \
	coreutils/cksum.c \
	coreutils/comm.c \
	coreutils/cp.c \
	coreutils/cut.c \
	coreutils/date.c \
	coreutils/dd.c \
	coreutils/df.c \
	coreutils/dirname.c \
	coreutils/dos2unix.c \
	coreutils/du.c \
	coreutils/echo.c \
	coreutils/env.c \
	coreutils/expand.c \
	coreutils/expr.c \
	coreutils/false.c \
	coreutils/fold.c \
	coreutils/fsync.c \
	coreutils/head.c \
	coreutils/hostid.c \
	coreutils/id.c \
	coreutils/install.c \
	coreutils/ln.c \
	coreutils/logname.c \
	coreutils/ls.c \
	coreutils/md5_sha1_sum.c \
	coreutils/mkdir.c \
	coreutils/mkfifo.c \
	coreutils/mknod.c \
	coreutils/mv.c \
	coreutils/nice.c \
	coreutils/nohup.c \
	coreutils/od.c \
	coreutils/printenv.c \
	coreutils/printf.c \
	coreutils/pwd.c \
	coreutils/readlink.c \
	coreutils/realpath.c \
	coreutils/rm.c \
	coreutils/rmdir.c \
	coreutils/seq.c \
	coreutils/sleep.c \
	coreutils/sort.c \
	coreutils/split.c \
	coreutils/stat.c \
	coreutils/stty.c \
	coreutils/sum.c \
	coreutils/sync.c \
	coreutils/tac.c \
	coreutils/tail.c \
	coreutils/tee.c \
	coreutils/test.c \
	coreutils/test_ptr_hack.c \
	coreutils/touch.c \
	coreutils/tr.c \
	coreutils/true.c \
	coreutils/tty.c \
	coreutils/uname.c \
	coreutils/uniq.c \
	coreutils/usleep.c \
	coreutils/uudecode.c \
	coreutils/uuencode.c \
	coreutils/wc.c \
	coreutils/who.c \
	coreutils/whoami.c \
	coreutils/yes.c \
	coreutils/libcoreutils/cp_mv_stat.c \
	coreutils/libcoreutils/getopt_mk_fifo_nod.c \
	debianutils/mktemp.c \
	debianutils/run_parts.c \
	debianutils/which.c \
	editors/awk.c \
	editors/cmp.c \
	editors/diff.c \
	editors/ed.c \
	editors/patch.c \
	editors/sed.c \
	editors/vi.c \
	findutils/find.c \
	findutils/grep.c \
	findutils/xargs.c \
	init/halt.c \
	init/init.c \
	init/mesg.c \
	libbb/appletlib.c \
	libbb/ask_confirmation.c \
	libbb/bb_askpass.c \
	libbb/bb_bswap_64.c \
	libbb/bb_do_delay.c \
	libbb/bb_pwd.c \
	libbb/bb_qsort.c \
	libbb/bb_strtonum.c \
	libbb/change_identity.c \
	libbb/chomp.c \
	libbb/compare_string_array.c \
	libbb/concat_path_file.c \
	libbb/concat_subpath_file.c \
	libbb/copy_file.c \
	libbb/copyfd.c \
	libbb/correct_password.c \
	libbb/crc32.c \
	libbb/create_icmp6_socket.c \
	libbb/create_icmp_socket.c \
	libbb/default_error_retval.c \
	libbb/device_open.c \
	libbb/dump.c \
	libbb/execable.c \
	libbb/fclose_nonstdin.c \
	libbb/fflush_stdout_and_exit.c \
	libbb/fgets_str.c \
	libbb/find_mount_point.c \
	libbb/find_pid_by_name.c \
	libbb/find_root_device.c \
	libbb/full_write.c \
	libbb/get_console.c \
	libbb/get_cpu_count.c \
	libbb/get_last_path_component.c \
	libbb/get_line_from_file.c \
	libbb/get_shell_name.c \
	libbb/get_volsize.c \
	libbb/getopt32.c \
	libbb/getpty.c \
	libbb/hash_md5_sha.c \
	libbb/herror_msg.c \
	libbb/human_readable.c \
	libbb/inet_cksum.c \
	libbb/inet_common.c \
	libbb/info_msg.c \
	libbb/inode_hash.c \
	libbb/isdirectory.c \
	libbb/kernel_version.c \
	libbb/last_char_is.c \
	libbb/lineedit.c \
	libbb/lineedit_ptr_hack.c \
	libbb/llist.c \
	libbb/login.c \
	libbb/loop.c \
	libbb/make_directory.c \
	libbb/makedev.c \
	libbb/match_fstype.c \
	libbb/messages.c \
	libbb/mode_string.c \
	libbb/parse_config.c \
	libbb/parse_mode.c \
	libbb/percent_decode.c \
	libbb/perror_msg.c \
	libbb/perror_nomsg.c \
	libbb/perror_nomsg_and_die.c \
	libbb/pidfile.c \
	libbb/platform.c \
	libbb/print_flags.c \
	libbb/printable.c \
	libbb/printable_string.c \
	libbb/process_escape_sequence.c \
	libbb/procps.c \
	libbb/progress.c \
	libbb/ptr_to_globals.c \
	libbb/pw_encrypt.c \
	libbb/read.c \
	libbb/read_key.c \
	libbb/read_printf.c \
	libbb/recursive_action.c \
	libbb/remove_file.c \
	libbb/rtc.c \
	libbb/run_shell.c \
	libbb/safe_gethostname.c \
	libbb/safe_poll.c \
	libbb/safe_strncpy.c \
	libbb/safe_write.c \
	libbb/setup_environment.c \
	libbb/signals.c \
	libbb/simplify_path.c \
	libbb/single_argv.c \
	libbb/skip_whitespace.c \
	libbb/speed_table.c \
	libbb/str_tolower.c \
	libbb/strrstr.c \
	libbb/time.c \
	libbb/trim.c \
	libbb/u_signal_names.c \
	libbb/udp_io.c \
	libbb/unicode.c \
	libbb/utmp.c \
	libbb/uuencode.c \
	libbb/vdprintf.c \
	libbb/verror_msg.c \
	libbb/vfork_daemon_rexec.c \
	libbb/warn_ignoring_args.c \
	libbb/wfopen.c \
	libbb/wfopen_input.c \
	libbb/write.c \
	libbb/xatonum.c \
	libbb/xconnect.c \
	libbb/xfunc_die.c \
	libbb/xfuncs.c \
	libbb/xfuncs_printf.c \
	libbb/xgetcwd.c \
	libbb/xgethostbyname.c \
	libbb/xreadlink.c \
	libbb/xrealloc_vector.c \
	libbb/xregcomp.c \
	libpwdgrp/uidgid_get.c \
	loginutils/getty.c \
	loginutils/login.c \
	loginutils/su.c \
	loginutils/sulogin.c \
	miscutils/bbconfig.c \
	miscutils/chrt.c \
	miscutils/dc.c \
	miscutils/devmem.c \
	miscutils/eject.c \
	miscutils/fbsplash.c \
	miscutils/hdparm.c \
	miscutils/ionice.c \
	miscutils/last_fancy.c \
	miscutils/less.c \
	miscutils/makedevs.c \
	miscutils/mountpoint.c \
	miscutils/readahead.c \
	miscutils/runlevel.c \
	miscutils/setserial.c \
	miscutils/setsid.c \
	miscutils/strings.c \
	miscutils/time.c \
	miscutils/timeout.c \
	miscutils/volname.c \
	miscutils/wall.c \
	modutils/depmod.c \
	modutils/insmod.c \
	modutils/lsmod.c \
	modutils/modinfo.c \
	modutils/modprobe.c \
	modutils/modutils.c \
	modutils/rmmod.c \
	networking/arp.c \
	networking/arping.c \
	networking/brctl.c \
	networking/hostname.c \
	networking/ifconfig.c \
	networking/ifenslave.c \
	networking/ifupdown.c \
	networking/interface.c \
	networking/ip.c \
	networking/ipcalc.c \
	networking/nameif.c \
	networking/netstat.c \
	networking/nslookup.c \
	networking/ntpd.c \
	networking/ping.c \
	networking/route.c \
	networking/telnet.c \
	networking/tftp.c \
	networking/traceroute.c \
	networking/vconfig.c \
	networking/wget.c \
	networking/zcip.c \
	networking/libiproute/ip_parse_common_args.c \
	networking/libiproute/ipaddress.c \
	networking/libiproute/iplink.c \
	networking/libiproute/iproute.c \
	networking/libiproute/iprule.c \
	networking/libiproute/libnetlink.c \
	networking/libiproute/ll_addr.c \
	networking/libiproute/ll_map.c \
	networking/libiproute/ll_proto.c \
	networking/libiproute/ll_types.c \
	networking/libiproute/rt_names.c \
	networking/libiproute/rtm_map.c \
	networking/libiproute/utils.c \
	networking/udhcp/arpping.c \
	networking/udhcp/common.c \
	networking/udhcp/d6_dhcpc.c \
	networking/udhcp/d6_packet.c \
	networking/udhcp/d6_socket.c \
	networking/udhcp/dhcpc.c \
	networking/udhcp/domain_codec.c \
	networking/udhcp/packet.c \
	networking/udhcp/signalpipe.c \
	networking/udhcp/socket.c \
	procps/free.c \
	procps/fuser.c \
	procps/iostat.c \
	procps/kill.c \
	procps/lsof.c \
	procps/mpstat.c \
	procps/pgrep.c \
	procps/pidof.c \
	procps/pmap.c \
	procps/ps.c \
	procps/pstree.c \
	procps/pwdx.c \
	procps/renice.c \
	procps/sysctl.c \
	procps/top.c \
	procps/uptime.c \
	procps/watch.c \
	shell/ash.c \
	shell/ash_ptr_hack.c \
	shell/cttyhack.c \
	shell/math.c \
	shell/random.c \
	shell/shell_common.c \
	sysklogd/klogd.c \
	sysklogd/logread.c \
	sysklogd/syslogd_and_logger.c \
	util-linux/blkid.c \
	util-linux/blockdev.c \
	util-linux/dmesg.c \
	util-linux/fdformat.c \
	util-linux/fdisk.c \
	util-linux/findfs.c \
	util-linux/flock.c \
	util-linux/freeramdisk.c \
	util-linux/getopt.c \
	util-linux/hexdump.c \
	util-linux/hwclock.c \
	util-linux/ipcrm.c \
	util-linux/ipcs.c \
	util-linux/losetup.c \
	util-linux/lspci.c \
	util-linux/lsusb.c \
	util-linux/mdev.c \
	util-linux/mkswap.c \
	util-linux/more.c \
	util-linux/mount.c \
	util-linux/pivot_root.c \
	util-linux/rdate.c \
	util-linux/rdev.c \
	util-linux/rev.c \
	util-linux/script.c \
	util-linux/setarch.c \
	util-linux/swaponoff.c \
	util-linux/switch_root.c \
	util-linux/umount.c \
	util-linux/volume_id/btrfs.c \
	util-linux/volume_id/cramfs.c \
	util-linux/volume_id/ext.c \
	util-linux/volume_id/fat.c \
	util-linux/volume_id/get_devname.c \
	util-linux/volume_id/hfs.c \
	util-linux/volume_id/iso9660.c \
	util-linux/volume_id/jfs.c \
	util-linux/volume_id/linux_raid.c \
	util-linux/volume_id/linux_swap.c \
	util-linux/volume_id/luks.c \
	util-linux/volume_id/ntfs.c \
	util-linux/volume_id/ocfs2.c \
	util-linux/volume_id/reiserfs.c \
	util-linux/volume_id/romfs.c \
	util-linux/volume_id/sysv.c \
	util-linux/volume_id/udf.c \
	util-linux/volume_id/util.c \
	util-linux/volume_id/volume_id.c \
	util-linux/volume_id/xfs.c \


PROG_PRE_BUILD_TARGET=pre_build
PROG_POST_BUILD_TARGET=post_build

include ${PROD_TREE_ROOT}/src/mk/prog.mk

# Juniper commented this out, because the real usr/bin/nslookup is installed now
# from bind-utils-*.rpm
#DO_PROG_POST_INSTALL=(cd ${PROD_INSTALL_OUTPUT_DIR}/${INSTALL_TREE}; \
#	mkdir -p usr/bin ; \
#	ln -sf ../../sbin/busybox usr/bin/nslookup ; \
#	)

pre_build:

post_build:
