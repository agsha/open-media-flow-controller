#!/bin/sh

#
#
#
#

#
# This script makes a two floppy images, a "boot" floppy and a "root"
# floppy.  Together these can be used to manufacture a full release
# image on to a box, and to set manufacturing settings.

# The boot floppy (made for x86 only) has the following components:
# 
# 1) Syslinux:  the boot loader
# 2) Kernel: the system kernel (as vmlinuz)
#
# The root floppy has the following components:
#
# 3) Root filesystem: a root file system (a gzip'd ext2fs image)
#
# Part 3) contains the following items:
#    a) busybox          All base system utilities (incl tar, wget)
#    b) sfdisk,parted    To partition hard disk
#    c) mke2fs,e2label   To make file systems on hard disk
#    d) manufacture.sh,
#       writeimage.sh    Scripts to drive the manufacturing process
#    e) grub             To write as the boot manager (x86 only)
#

if [ -z "${CROSS_PATH_PREADD}" ]; then
    PATH=/usr/bin:/bin:/usr/sbin:/sbin
else
    PATH=${CROSS_PATH_PREADD}:/usr/bin:/bin:/usr/sbin:/sbin
fi
export PATH


usage()
{
    echo "usage: $0 [-b BOOTFLOP_NAME] [-r ROOTFLOP_NAME] [-R ROOTFLOP_RAW_NAME] [-p] [-a ARCH_FAMILY] [-v]"
    echo ""
    exit 1
}

# ==================================================
# Cleanup when we are finished or interrupted
# ==================================================
cleanup()
{
    # Get out of any dirs we might remove
    cd /

    if [ ! -z "${UNMOUNT_ROOTFS}" ]; then
        umount ${UNMOUNT_ROOTFS}
        rmdir ${UNMOUNT_ROOTFS}
    fi

    if [ ! -z "${UNMOUNT_BOOTFL}" ]; then
        umount ${UNMOUNT_BOOTFL}
        rm -r ${UNMOUNT_BOOTFL}
    fi

    if [ ! -z "${CLEANUP_FILES}" ]; then
        rm -f ${CLEANUP_FILES}
    fi
}


# ==================================================
# Cleanup when called from 'trap' for ^C, signal, or fatal error
# ==================================================
cleanup_exit()
{
    cleanup
    exit 1
}

# ==================================================
# Based on verboseness setting suppress command output
# ==================================================
do_verbose()
{
    if [ ${VERBOSE} -gt 0 ]; then
        $*
    else
        $* > /dev/null 2>&1
    fi
}

# ==================================================
# Echo or not based on VERBOSE setting
# ==================================================
vecho()
{
    level=$1
    shift

    if [ ${VERBOSE} -gt ${level} ]; then
        echo "$*"
    fi
}

# ==================================================
# Ensure that a given filesystem is supported by the kernel.
# Returns 0 on success, 1 otherwise.
# ==================================================
ensure_fstype()
{
    enfstype=$1

    if ! grep -qw "${enfstype}" /proc/filesystems ; then
        /sbin/modprobe -q "${enfstype}" || return 1
        if ! grep -qw "${enfstype}" /proc/filesystems ; then
            return 1
        fi
    fi
    return 0
}

if [ -z "${PROD_OUTPUT_ROOT}" \
    -o -z "${PROD_INSTALL_OUTPUT_DIR}" -o -z "${PROD_RELEASE_OUTPUT_DIR}" ]; then
    echo "ERROR: must be called from build or set PROD_OUTPUT_ROOT"
    exit 1
fi

if [ `id -u` != 0 ]; then
    echo "ERROR: must be called as root"
    exit 1
fi

PARSE=`/usr/bin/getopt 'r:R:b:a:pv' "$@"`

if [ $? != 0 ] ; then
    usage
fi

eval set -- "$PARSE"

VERBOSE=1
ROOTFS_PAD=1
DO_ROOTFLOP=0
DO_BOOTFLOP=0
TARCH_FAMILY=x86

# These types cannot be changed.  Also, "mkmfgusb.sh" and "imgtoiso.sh"
# depend on this.  "mkmfgcd.sh" also needs the ROOTFS_FSTYPE to be
# something that the kernel can use directly as an initrd.
ROOTFS_FSTYPE=ext2
BOOTFL_FSTYPE=msdos

# If we're going to package up (for example with "mkimage") the
# generated rootflop, then if this is set, we'll also save off the
# generated rootflop, as is, under this name.  This is needed for tools
# (like "make mfgusb") that want to post-process the rootflop, and do
# not want to have to un-"mkimage" a file.
ROOTFLOP_RAW_NAME=

while true ; do
    case "$1" in
        -r) DO_ROOTFLOP=1; ROOTFLOP_NAME=$2; shift 2 ;;
        -R) ROOTFLOP_RAW_NAME=$2; shift 2 ;;
        -b) DO_BOOTFLOP=1; BOOTFLOP_NAME=$2; shift 2 ;;
        -p) ROOTFS_PAD=0; shift ;;
        -a) TARCH_FAMILY=$2; shift 2 ;;
        -v) VERBOSE=$((${VERBOSE}+1)); shift ;;
        --) shift ; break ;;
        *) echo "mkbootfl.sh: parse failure" >&2 ; usage ;;
    esac
done

if [ ! -z "$*" ] ; then
    usage
fi

if [ -z "${ROOTFLOP_NAME}" -o "${ROOTFLOP_NAME}" = "-" ]; then
    DO_ROOTFLOP=0
fi

if [ -z "${BOOTFLOP_NAME}" -o "${BOOTFLOP_NAME}" = "-" ]; then
    DO_BOOTFLOP=0
fi

# Do we want tar to print every file
if [ ${VERBOSE} -gt 1 ]; then
    TAR_VERBOSE=v
else
    TAR_VERBOSE=
fi


UNMOUNT_ROOTFS=
UNMOUNT_BOOTFL=
CLEANUP_FILES=
trap "cleanup_exit" HUP INT QUIT PIPE TERM

#==================================================
# Root file system (for initrd / pxe / root floppy)
#==================================================
#
# First, make the root filesystem using the loopback mount technique.
#
if [ ${DO_ROOTFLOP} -ne 0 ]; then
    vecho 0 "==== Creating manufacturing root filesystem named ${ROOTFLOP_NAME}"

    ensure_fstype ${ROOTFS_FSTYPE} || cleanup_exit

    ROOTFS_FILE=/tmp/rootfs.$$
    ROOTFS_COMP_FILE=/tmp/rootflop.$$
    ROOTFS_MOUNT=/tmp/rootfs_mount.$$

    # This must be less than the mfg kernel's CONFIG_BLK_DEV_RAM_SIZE
    ROOTFS_SIZE=60000

    umount ${ROOTFS_MOUNT} > /dev/null 2>&1
    rm -f ${ROOTFS_FILE}
    CLEANUP_FILES="${CLEANUP_FILES} ${ROOTFS_FILE}"
    dd if=/dev/zero of=${ROOTFS_FILE} bs=1k count=${ROOTFS_SIZE} 2> /dev/null || cleanup_exit

    #
    # This is because mke2fs now needs to be told not to reserve space to resize
    # the partition.
    #   
    mke2fs_version=`mke2fs -V 2>&1 | head -1 | awk '{print $2}'`
    mke2fs_version_major=`echo ${mke2fs_version} | awk -F. '{print $1}'`
    mke2fs_version_minor=`echo ${mke2fs_version} | awk -F. '{print $2}'`

    if [ ${mke2fs_version_major} -gt 1 -o ${mke2fs_version_minor} -ge 35 ]; then
        mke2fs_resize_options='-O ^resize_inode'
        if [ ${mke2fs_version_major} -eq 1 -a ${mke2fs_version_minor} -eq 35 ]; then
        # This is crazy, but in FC2 and FC3 there is different behavior with
        # the same version number!  So we test in this case if the flag will work

            FAILURE=0
            mke2fs -n ${mke2fs_resize_options} -q -F ${ROOTFS_FILE} > /dev/null 2>&1 || FAILURE=1
            if [ ${FAILURE} -eq 1 ]; then
                mke2fs_resize_options=
            fi
        fi
    else
        mke2fs_resize_options=
    fi

    FAILURE=0
    mke2fs ${mke2fs_resize_options} -q -m 0 -N 2000 -F ${ROOTFS_FILE} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Error: could not make filesystem on ${ROOTFS_FILE}"
        cleanup_exit
    fi


    mkdir -p ${ROOTFS_MOUNT}
    mount -o loop,noatime -t ${ROOTFS_FSTYPE} ${ROOTFS_FILE} ${ROOTFS_MOUNT} || cleanup_exit
    UNMOUNT_ROOTFS=${ROOTFS_MOUNT}

    rmdir ${ROOTFS_MOUNT}/lost+found


    #---------------
    # Root floppy file system files (built from source tree)
    #---------------

    # Copy in all the installed files

    cd ${ROOTFS_MOUNT}/

    (cd ${PROD_INSTALL_OUTPUT_DIR}/rootflop; tar -cpf - . || exit 1) | tar -x${TAR_VERBOSE}pf - || cleanup_exit


    #---------------
    # Devices   XXX this should have been in the previous part!
    #---------------

    mkdir -p ${ROOTFS_MOUNT}/dev
    cd ${ROOTFS_MOUNT}/dev

    mknod console c 5 1
    mknod mem c 1 1
    mknod kmem c 1 2
    mknod null c 1 3
    mknod port c 1 4
    mknod zero c 1 5
    mknod full c 1 7
    mknod random c 1 8
    mknod urandom c 1 9
    mknod kmsg c 1 11
    mknod rtc c 10 135
    mknod hwrng c 10 183
    ln -s hwrng hw_random
    ln -s /proc/kcore core

    # loop devs
    for i in `seq 0 7`; do
        mknod loop$i b 7 $i
    done

    # ram devs
    for i in `seq 0 9`; do
        mknod ram$i b 1 $i
    done
    ln -s ram1 ram

    # ttys
    mknod tty c 5 0
    mknod systty c 4 0
    for i in `seq 0 9`; do
        mknod tty$i c 4 $i
    done

    # ttySs
    for i in `seq 0 2`; do
        mknod ttyS$i c 4 `expr 64 + $i`
    done

    # virtual console screen devs
    for i in `seq 0 9`; do
        mknod vcs$i b 7 $i
    done
    ln -s vcs0 vcs

    # virtual console screen w/ attributes devs
    for i in `seq 0 9`; do
        mknod vcsa$i b 7 $i
    done
    ln -s vcsa0 vcsa

    # ide hda
    for i in `seq 1 15`; do
        mknod hda$i b 3 $i
    done
    mknod hda b 3 0

    # ide hdb
    for i in `seq 1 15`; do
        mknod hdb$i b 3 `expr 64 + $i`
    done
    mknod hdb b 3 64

    # ide hdc
    for i in `seq 1 15`; do
        mknod hdc$i b 22 $i
    done
    mknod hdc b 22 0

    # ide hdd
    for i in `seq 1 15`; do
        mknod hdd$i b 22 `expr 64 + $i`
    done
    mknod hdd b 22 64

    # scsi sda
    for i in `seq 0 15`; do
        mknod sda$i b 8 `expr 16 \* 0 + $i`
    done
    mknod sda b 8 `expr 16 \* 0 + 0`

    # scsi sdb
    for i in `seq 0 15`; do
        mknod sdb$i b 8 `expr 16 \* 1 + $i`
    done
    mknod sdb b 8 `expr 16 \* 1 + 0`

    # scsi sdc
    for i in `seq 0 15`; do
        mknod sdc$i b 8 `expr 16 \* 2 + $i`
    done
    mknod sdc b 8 `expr 16 \* 2 + 0`

    # scsi sdd
    for i in `seq 0 15`; do
        mknod sdd$i b 8 `expr 16 \* 3 + $i`
    done
    mknod sdd b 8 `expr 16 \* 3 + 0`

    # scsi cdrom
    for i in `seq 0 3`; do
        mknod scd$i b 11 $i
    done

    # XXXX Sadly, this major number is _not_ statically allocated!
    VIRTIO_MAJOR=254
    # virtio vda
    for i in `seq 0 15`; do
        mknod vda$i b ${VIRTIO_MAJOR} `expr 16 \* 0 + $i`
    done
    mknod vda b ${VIRTIO_MAJOR} `expr 16 \* 0 + 0`

    # virtio vdb
    for i in `seq 0 15`; do
        mknod vdb$i b ${VIRTIO_MAJOR} `expr 16 \* 1 + $i`
    done
    mknod vdb b ${VIRTIO_MAJOR} `expr 16 \* 1 + 0`

    # virtio vdc
    for i in `seq 0 15`; do
        mknod vdc$i b ${VIRTIO_MAJOR} `expr 16 \* 2 + $i`
    done
    mknod vdc b ${VIRTIO_MAJOR} `expr 16 \* 2 + 0`

    # virtio vdd
    for i in `seq 0 15`; do
        mknod vdd$i b ${VIRTIO_MAJOR} `expr 16 \* 3 + $i`
    done
    mknod vdd b ${VIRTIO_MAJOR} `expr 16 \* 3 + 0`

    # pty
    for i in `seq 0 31`; do
        mknod pty$i c 2 $i
    done
    mknod ptmx c 5 2

    # fd
    for i in `seq 0 1`; do
        mknod fd$i b 2 $i
        mknod fd${i}H1440 b 2 `expr 28 + $i`
    done

    # mtd
    for i in `seq 0 15`; do
        mknod mtdblock$i b 31 $i
        mknod mtd$i c 90 `expr $i \* 2`
        mknod mtd${i}ro c 90 `expr $i \* 2 + 1`
    done

    # mmc/sd
    #
    # XXXX NOTE: We are using a non-standard 16 partitions per device.
    # The stock MMC kernel driver only supports 8, so the kernel must be
    # patched accordingly, or the devices will not all work.
    MMC_PARTS_PER_DEV=16
    for i in `seq 1 ${MMC_PARTS_PER_DEV}`; do
        mknod mmcblk0p$i b 179 `expr ${MMC_PARTS_PER_DEV} \* 0 + $i`
    done
    mknod mmcblk0 b 179 `expr ${MMC_PARTS_PER_DEV} \* 0`

    # i2c
    for i in `seq 0 1`; do
        mknod i2c-${i} c 89 ${i}
    done

    cd ${ROOTFS_MOUNT}

    #---------------
    # Cleanup rootfs
    #---------------

    cd /tmp
    umount ${ROOTFS_MOUNT}
    rmdir ${ROOTFS_MOUNT}
    UNMOUNT_ROOTFS=

    ##echo "Finished rootfs"

    #==================================================
    # Root floppy image
    #==================================================

    # Already made, just needs to be compressed and filled out to size
    rm -f ${ROOTFS_COMP_FILE}
    CLEANUP_FILES="${CLEANUP_FILES} ${ROOTFS_COMP_FILE} ${ROOTFS_COMP_FILE}.temp"
    gzip -9c ${ROOTFS_FILE} > ${ROOTFS_COMP_FILE}.temp || cleanup_exit

    if [ ${ROOTFS_PAD} -ne 0 ]; then
        floppy_size=1440
        rcfs=`/bin/ls -l ${ROOTFS_COMP_FILE}.temp | awk '{print $5}'`
        if [ -z "${rcfs}" ]; then
            rcfs=0
        fi
        if [ ${rcfs} -gt 1474560 ]; then
            echo "*** Warning: root floppy is too big for standard 1440k floppy, at ${rcfs} (max 1474560)."
            if [ ${rcfs} -lt 1763328 ]; then
                floppy_size=1722
                echo "Generated root floppy will be sized for use on 1722k floppy."
            elif [ ${rcfs} -lt 2949120 ]; then
                floppy_size=2880
                echo "Generated root floppy will be sized for use on 2880k floppy."
            else
                echo "Root floppy is too large."
                cleanup_exit
            fi
        fi

        vecho 0 "-- Padding manufacturing root filesystem to ${floppy_size}KB"
        # If the image isn't exactly 1440k, then vmware gets mad
        dd if=/dev/zero of=${ROOTFS_COMP_FILE} bs=1k count=${floppy_size} 2> /dev/null || cleanup_exit
        dd if=${ROOTFS_COMP_FILE}.temp of=${ROOTFS_COMP_FILE} conv=notrunc bs=1k 2> /dev/null || cleanup_exit
    else
        mv ${ROOTFS_COMP_FILE}.temp ${ROOTFS_COMP_FILE} || cleanup_exit
    fi

    mkdir -p ${PROD_RELEASE_OUTPUT_DIR}/rootflop || cleanup_exit

    if [ -z "${TARCH_FAMILY}" -o "${TARCH_FAMILY}" = "x86" ]; then
        install -m 644 ${ROOTFS_COMP_FILE} ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME} || cleanup_exit
        vecho -1 "-- Manufacturing root filesystem image: ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME}"
    elif [ "${TARCH_FAMILY}" = "ppc" ]; then
        if [ ! -z "${ROOTFLOP_RAW_NAME}" ]; then
            install -m 644 ${ROOTFS_COMP_FILE} ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_RAW_NAME} || cleanup_exit
        fi
        mkimage -A ppc -O linux -T ramdisk -C gzip -n "${ROOTFLOP_NAME}" -d "${ROOTFS_COMP_FILE}" "${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME}" || cleanup_exit
        vecho -1 "-- Manufacturing root filesystem (uImage): ${PROD_RELEASE_OUTPUT_DIR}/rootflop/${ROOTFLOP_NAME}"
    else
        vecho -1 "*** Error: unsupported target architecture: ${TARCH_FAMILY}"
        cleanup_exit
    fi
fi

#==================================================
# Boot floppy image
#==================================================
if [ ${DO_BOOTFLOP} -ne 0 ]; then
    vecho 0 "==== Creating manufacturing kernel floppy named ${BOOTFLOP_NAME}"

    ensure_fstype ${BOOTFL_FSTYPE} || cleanup_exit

    BOOTFL_FILE=/tmp/bootflop.$$
    BOOTFL_SIZE=1440
    BOOTFL_MOUNT=/tmp/bootflop_mount.$$
    KERNEL=${PROD_INSTALL_OUTPUT_DIR}/bootflop/vmlinuz
    # Leave space for fs and syslinux
    BOOTFL_OVERHEAD=30720

    rcfs=`/bin/ls -l ${KERNEL} | awk '{print $5}'`
    if [ -z "${rcfs}" ]; then
        rcfs=0
    fi
    MAX_KERN_SIZE=`expr ${BOOTFL_SIZE} \* 1024 - ${BOOTFL_OVERHEAD}`
    if [ ${rcfs} -gt ${MAX_KERN_SIZE} ]; then
        echo "*** Warning: boot floppy kernel is too big for standard 1440k floppy, at ${rcfs} (max ${MAX_KERN_SIZE})."

        BOOTFL_SIZE=1722
        MAX_KERN_SIZE=`expr ${BOOTFL_SIZE} \* 1024 - ${BOOTFL_OVERHEAD}`
        if [ ${rcfs} -lt ${MAX_KERN_SIZE} ]; then
            echo "Generated boot floppy will be sized for use on 1722k floppy."
        else
            BOOTFL_SIZE=2880
            MAX_KERN_SIZE=`expr ${BOOTFL_SIZE} \* 1024 - ${BOOTFL_OVERHEAD}`
            if [ ${rcfs} -lt ${MAX_KERN_SIZE} ]; then
                echo "Generated boot floppy will be sized for use on 2880k floppy."
            else
                echo "Boot floppy is too large."
                cleanup_exit
            fi
        fi
    fi

    rm -f ${BOOTFL_FILE}
    CLEANUP_FILES="${CLEANUP_FILES} ${BOOTFL_FILE}"
    dd if=/dev/zero of=${BOOTFL_FILE} bs=1k count=${BOOTFL_SIZE} 2> /dev/null || cleanup_exit
    mkdosfs ${BOOTFL_FILE} > /dev/null || cleanup_exit

    syslinux -s ${BOOTFL_FILE} || cleanup_exit

    mkdir -p ${BOOTFL_MOUNT} || cleanup_exit
    mount -o loop -t ${BOOTFL_FSTYPE} ${BOOTFL_FILE} ${BOOTFL_MOUNT} || cleanup_exit
    UNMOUNT_BOOTFL=${BOOTFL_MOUNT}

    cp --preserve=mode,timestamps ${KERNEL} ${BOOTFL_MOUNT}/linux || cleanup_exit


    # XXX this should come from the build tree instead of being inline

    # Note that we're not setting 'ramdisk_size' below, so the kernel
    # default must be sufficient.

    cat >> ${BOOTFL_MOUNT}/syslinux.cfg <<EOF || cleanup_exit
    SERIAL 0 9600 0x3
    DEFAULT linux
    APPEND load_ramdisk=1 prompt_ramdisk=1 console=ttyS0,9600n8 console=tty0 root=/dev/fd0 rw noexec=off panic=10
    TIMEOUT 10
    PROMPT 1
EOF

    umount ${BOOTFL_MOUNT}
    rm -r ${BOOTFL_MOUNT}
    UNMOUNT_BOOTFL=
    mkdir -p ${PROD_RELEASE_OUTPUT_DIR}/bootflop || cleanup_exit
    install -m 644 ${BOOTFL_FILE} ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_NAME} || cleanup_exit
    vecho -1 "-- Manufacturing kernel floppy image: ${PROD_RELEASE_OUTPUT_DIR}/bootflop/${BOOTFLOP_NAME}"

fi

##################################################

# Cleanup the (temporary) files
cleanup

vecho 0 "==== Done."
exit 0
