#  Makefile
#
#
#
#

include ${PROD_TREE_ROOT}/src/mk/common.mk

LIB=parted-rootflop
export INSTALL_TREE=rootflop

ifeq (${PROD_TARGET_PLATFORM_FULL}-${ROOTFLOP_OLD_BINARYS},LINUX_EL_EL5-1)
	export PROD_TARGET_CPU_WORDSIZE=32
endif

vpath %.c ../../src/libparted

LICENSE_REQUIRES_REDIST=1

SRCS=   \
	labels/efi_crc32.c \
	labels/bsd.c \
	labels/loop.c \
	labels/gpt.c \
	labels/dos.c \
	fs/jfs/jfs.c \
	fs/reiserfs/geom_dal.c \
	fs/reiserfs/reiserfs.c \
	fs/hfs/file.c \
	fs/hfs/journal.c \
	fs/hfs/hfs.c \
	fs/hfs/probe.c \
	fs/hfs/advfs_plus.c \
	fs/hfs/file_plus.c \
	fs/hfs/cache.c \
	fs/hfs/advfs.c \
	fs/hfs/reloc_plus.c \
	fs/hfs/reloc.c \
	fs/linux_swap/linux_swap.c \
	fs/xfs/xfs.c \
	fs/amiga/interface.c \
	fs/amiga/asfs.c \
	fs/amiga/affs.c \
	fs/amiga/amiga.c \
	fs/amiga/apfs.c \
	fs/ufs/ufs.c \
	fs/fat/fatio.c \
	fs/fat/traverse.c \
	fs/fat/calc.c \
	fs/fat/table.c \
	fs/fat/bootsector.c \
	fs/fat/count.c \
	fs/fat/fat.c \
	fs/fat/resize.c \
	fs/fat/context.c \
	fs/fat/clstdup.c \
	fs/ntfs/ntfs.c \
	fs/ext2/ext2_buffer.c \
	fs/ext2/parted_io.c \
	fs/ext2/interface.c \
	fs/ext2/ext2_inode_relocator.c \
	fs/ext2/tune.c \
	fs/ext2/ext2_mkfs.c \
	fs/ext2/ext2.c \
	fs/ext2/ext2_meta.c \
	fs/ext2/ext2_block_relocator.c \
	fs/ext2/ext2_resize.c \
	filesys.c \
	timer.c \
	arch/linux.c \
	exception.c \
	disk.c \
	unit.c \
	device.c \
	debug.c \
	libparted.c \
	cs/geom.c \
	cs/natmath.c \
	cs/constraint.c \


# Disabled:
# 	labels/aix.c \
# 	labels/vtoc.c \
# 	labels/dvh.c \
# 	labels/pc98.c \
# 	labels/dasd.c \
# 	labels/mac.c \
# 	labels/sun.c \
# 	labels/fdasd.c \
# 	labels/rdb.c \
#

LIB_BUILD_DIR=${ILIB_DIR}/base_os/common/parted/lib/libparted.rootflop

INCLUDES= \
	-I${PROD_TREE_ROOT}/src/base_os/common/parted/src/include \
	-I${PROD_TREE_ROOT}/src/base_os/common/parted/src \

DEFINES= \
	-DHAVE_CONFIG_H \
	-D_REENTRANT \


# We are already defining it to =64
#	-D_FILE_OFFSET_BITS=no \

CFLAGS=

DEFINES_OVERRIDE=1
WARNINGS_OVERRIDE=1
WARNINGS_ARE_ERRORS=0
WARNINGS= \
	-W \
	-Wall \
	-Wno-unused \
	-Wno-switch \
	-Wno-format \

include ${PROD_TREE_ROOT}/src/mk/lib.mk
