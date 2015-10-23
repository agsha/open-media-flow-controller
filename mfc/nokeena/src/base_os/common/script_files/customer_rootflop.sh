# /etc/customer_rootflop.sh
# This file contains definitions and graft functions for use in the
# manufacture environment, as well as while running the installed product.
#
# This file used by the following scripts as part of installing the image:
#  ${PROD_TREE_ROOT}/src/base_os/common/script_files/manufacgture.sh
#  ${PROD_TREE_ROOT}/src/base_os/common/script_files/writeimage.sh
#  ${PROD_TREE_ROOT}/src/base_os/common/script_files/layout_settings.sh
# Notes:
# - writeimage.sh is executed by manufacture.sh (NOT dotted in).
# - layout_settings.sh is dotted in by both manufacture.sh and writeimage.sh
# - So the graft points used by layout_settings.sh need to work the
#   same when dotted in by either.

mf_wi_graft_log()
{
  [ -d /log -a ! -d /log/install ] && mkdir /log/install
  if [ -d /log/install ] ; then
    G_LOG=/log/install/wi_graft.log
  else
    G_LOG=/tmp/wi_graft.log
  fi
  if [ "_${1}" = "_clear" ] ; then
     rm -f ${G_LOG}.prev.prev
     [ -f  ${G_LOG}.prev ] && mv ${G_LOG}.prev ${G_LOG}.prev.prev
     rm -f ${G_LOG}.prev
     [ -f  ${G_LOG} ] && mv ${G_LOG} ${G_LOG}.prev
     rm -f ${G_LOG}
     return
  fi
  echo ===================================================================== >> ${G_LOG}
  echo "Function ${*} `date`"           >> ${G_LOG}
  echo "WORKSPACE_DIR=${WORKSPACE_DIR}" >> ${G_LOG}
  echo "SYSIMAGE_FILE=${SYSIMAGE_FILE}" >> ${G_LOG}
  echo "UNZIP_DIR=    ${UNZIP_DIR}"     >> ${G_LOG}
  if [ ! -z "${WORKSPACE_DIR}" ] ; then
    find ${WORKSPACE_DIR} -type f       >> ${G_LOG} 2>&1
  fi
  df -k >> ${G_LOG}
  cat /proc/meminfo >> ${G_LOG}
  RESTORE_P=`set +o | grep posix`
  set -o posix
  set | grep -v ^BUILD_ | grep -v ^IL_LO_ | grep -v ^IMAGE_BUILD >> ${G_LOG}
  ${RESTORE_P}
  echo ===================================================================== >> ${G_LOG}
}

# -----------------------------------------------------------------------------
# Graft point #1 for writeimage.sh.  This is called during initialization.
#
HAVE_WRITEIMAGE_GRAFT_1=y
writeimage_graft_1()
{
  mf_wi_graft_log clear
  mf_wi_graft_log writeimage_graft_1
  Ram_size_calc
  Tmpfs_size_calc
  Coredump_size_calc
}

# This function is called from writeimage_graft_1() and from manufacture/install-mfc
Ram_size_calc()
{
  TMP_KB=`grep ^MemTotal: /proc/meminfo | cut -f2 -d:`
  RAM_KB=`echo ${TMP_KB} | cut -f1 -d' '`
  TMP_KB=`expr ${RAM_KB} + 1023`
  RAM_MB=`expr ${TMP_KB} / 1024`
}

Tmpfs_size_calc()
{
  # Adjust the default size of the tmpfs.
  # When plenty of RAM, make it big.
  # We really do not support RAM less than 4GB.
  # One of our VXAs with "4GB" reports RAM_KB=4051508,
  # which calculates to RAM_MB=3957.
  if [ ${RAM_MB} -ge 4900 ] ; then
    TMPFS_SIZE_MB=4000
  elif [ ${RAM_MB} -ge 3900 ] ; then
    # 4GB systems will use this.
    TMPFS_SIZE_MB=3000
  else
    # 2GB systems will use this.
    # This is not big enough when the *.img file includes the
    # large mfc-dmi-adapter*.zip (600MB), and installation will fail
    # at the image-verify step.
    TMPFS_SIZE_MB=1600
  fi
  # Only change the default MKE2FS setting to use our wrapper scripts
  # when it exists, which is only in the fresh manufacture environment.
  if [ -x /usr/sbin/mke2fs ] ; then
    MKE2FS=/usr/sbin/mke2fs
    IS_MANUFACTURE=yes
  else
    IS_MANUFACTURE=no
  fi
  if [ "${IS_MANUFACTURE}" = "yes" ] ; then
    # The TMPFS is really only used for manufacturing.
    echo TMPFS_SIZE_MB=${TMPFS_SIZE_MB} > /tmp/tmpfs_size_mb.txt
  fi
}

# This function is called from writeimage_graft_1() and from manufacture/install-mfc
Coredump_size_calc()
{
  #   8 * 1024 =  8192 MB
  #  64 * 1024 = 65536 MB
  #  72 * 1024 = 73728 MB
  COREDUMP_SIZE=73728
  # Disk sizes when coredump is 72GB.
  MIN_SIZE_DEFAULT=140000
  MIN_SIZE_BIGLOGNC=130000
  MIN_SIZE_MIRROR=130000
  MIN_SIZE_FLASHROOT=130000
  MIN_SIZE_FLASHNCACHE=120000
}

# -----------------------------------------------------------------------------
# Graft point #2 for writeimage.sh.  Please see the writeimage.sh script
# for specifics on where this is called from.
#
HAVE_WRITEIMAGE_GRAFT_2=n
# writeimage_graft_2()
# {
# }

# -----------------------------------------------------------------------------
# Helper function to help support the cciss driver
#
make_cciss_devs()
{
    DEV_DIR=$1
    shift

    # Make device nodes for Compaq's SMART Array Controllers.
    # Note that due to the strange device names, we're creating an extra
    #      device name c0d0p, which has the same major/minor as c0d0 .
    #      This allows us to just append the partition number to the drive
    #      device number.

    NR_CTLR=1
    NR_VOL=1
    NR_PART=16

    if [ ! -d ${DEV_DIR} ]; then
        mkdir -p ${DEV_DIR}
    fi

    C=0;
    while [ $C -lt $NR_CTLR ]; do
        MAJ=`expr $C + 104`
        D=0
        while [ $D -lt $NR_VOL ]; do
            P=0
            while [ $P -lt $NR_PART ]; do
                MIN=`expr $D \* 16 + $P`
                if [ $P -eq 0 ]; then
                    # Make it with and without the 'p'
                    fn=${DEV_DIR}/c${C}d${D}
                    rm -f ${fn}
                    mknod ${fn} b $MAJ $MIN
                    rm -f ${fn}p
                    mknod ${fn}p b $MAJ $MIN
                else
                    fn=${DEV_DIR}/c${C}d${D}p${P}
                    rm -f ${fn}
                    mknod ${fn} b $MAJ $MIN
                fi
                P=`expr $P + 1`
            done
            D=`expr $D + 1`
        done
        C=`expr $C + 1`
    done
}


# -----------------------------------------------------------------------------
# Graft point #3 for writeimage.sh.  This is called at the end of fstab
# generation.  It can be used to append customer specific lines to the
# fstab.
#
HAVE_WRITEIMAGE_GRAFT_3=y
writeimage_graft_3()
{
    FAILURE=0
    make_cciss_devs ./dev/cciss || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Could not make cciss devs"
        cleanup_exit
    fi
}

# -----------------------------------------------------------------------------
# Graft point #4 for writeimage.sh.  This is called right after the version
# of the image to be used is determined.  It can be used to do customer
# specific determinations about what images should be allowed to be
# installed or manufactured.  
#
HAVE_WRITEIMAGE_GRAFT_4=y
writeimage_graft_4()
{
  mf_wi_graft_log writeimage_graft_4
  # NOTE: writeimage.sh takes all th BUILD_* settings in the install image
  # and puts them in the environment here as IMAGE_BUILD_* before calling
  # this graft_4 function.
  # By default, we make sure the product matches
  if [ ${BUILD_PROD_PRODUCT} != ${IMAGE_BUILD_PROD_PRODUCT} ]; then
    vlechoerr -1 "*** Bad product: image ${IMAGE_BUILD_PROD_PRODUCT} on system ${BUILD_PROD_PRODUCT}"
    cleanup_exit "Mismatched image product"
  fi
  if [ -d /log/install ] ; then
    MY_LOG=/log/install/writeimage_graft_4.log
    W_LOG=/log/install/Writeimage_g4.log
  elif [ -d /log ] ; then
    MY_LOG=/log/writeimage_graft_4.log
    W_LOG=/log/Writeimage_g4.log
  else
    MY_LOG=/tmp/writeimage_graft_4.log
    W_LOG=/tmp/Writeimage_g4.log
  fi
  date > ${MY_LOG}
  df -k >> ${MY_LOG}
  cat /proc/meminfo >> ${MY_LOG}
  echo "WORKSPACE_DIR=${WORKSPACE_DIR}" >> ${MY_LOG}
  echo "SYSIMAGE_FILE=${SYSIMAGE_FILE}" >> ${MY_LOG}
  echo "UNZIP_DIR=    ${UNZIP_DIR}"     >> ${MY_LOG}
  find ${WORKSPACE_DIR} -type f         >> ${MY_LOG} 2>&1
  ###########################################################################
  #rm -rf /tmp/save-workspace
  #mkdir /tmp/save-workspace
  #cp -a ${WORKSPACE_DIR} /tmp/save-workspace
  ###########################################################################
  echo =============================== >> ${MY_LOG}

  [ ! -d /tmp/from_pkg_dir ] && mkdir /tmp/from_pkg_dir
  rm -f /tmp/from_pkg_dir/*.sh
  [ -z "${UNZIP_DIR}" ] && return
  [ ! -d "${UNZIP_DIR}" ] && return
  for i in ${UNZIP_DIR}/*.sh
  do
    [ ! -f "${i}" ] && continue
    echo "cp ${i} /tmp/from_pkg_dir/" >> ${MY_LOG}
    cp ${i} /tmp/from_pkg_dir/
  done
  F=/tmp/from_pkg_dir/pre_img_install.sh
  if [ ! -s ${F} ] ; then
    echo No file ${F} >> ${MY_LOG}
    return
  fi
  echo Dot in ${F} >> ${MY_LOG}
  . ${F}
  grep -q "Writeimage_g4" ${F}
  if [ ${?} -eq 0 ] ; then
    Writeimage_g4 > ${W_LOG} 2>&1
    RV=${?}
    cat ${W_LOG}
    echo RV=${RV}     >> ${W_LOG}
    df -k             >> ${W_LOG}
    cat /proc/meminfo >> ${W_LOG}
    if [ ${RV} -ne 0 ] ; then
      df -k
      cat /proc/meminfo
    fi
  else
    echo No function Writeimage_g4 >> ${MY_LOG}
  fi
}

# -----------------------------------------------------------------------------
# Graft point #5 for writeimage.sh.  This is called right before the bootmgr
# (grub) is installed.  It can be used to emit things into the device map:
# ${mount_point}/boot/grub/device.map if a device is not being probed
# properly.  It can also be used to install the bootmgr yourself, at which
# point you should set DID_GRUB_INSTALL=1 .  See also, HAVE_AISET_GRAFT_1 below, 
# which is used when re-installing the grub later.
#
HAVE_WRITEIMAGE_GRAFT_5=y
writeimage_graft_5()
{
    mf_wi_graft_log writeimage_graft_5
    if [ -d /log/install ] ; then
      MY_LOG=/log/install/wi_g5.log
    elif [ -d /log ] ; then
      MY_LOG=/log/wi_g5.log
    else
      MY_LOG=/tmp/wi_g5.log
    fi
    echo curr_target_dev=${curr_target_dev} > ${MY_LOG}
    echo mount_point=${mount_point} >> ${MY_LOG}
    ls -l ${mount_point} >> ${MY_LOG}
    df ${mount_point} >> ${MY_LOG}
    rm -f /tmp/grub_defered.txt
    rm -f /tmp/grub_install_failed.txt

    # See if this is the cciss case
    IS_CCISS=0
    echo ${curr_target_dev} | grep -q '/cciss/' && IS_CCISS=1
    if [ ${IS_CCISS} -eq 1 ]; then
        DID_GRUB_INSTALL=1

        # Copy over the files grub needs
        FAILURE=0
        do_verbose grub-install --just-copy --root-directory=${mount_point} ${curr_target_dev} || FAILURE=1
        if [ ${FAILURE} -eq 1 ]; then
            echo "*** Could not copy grub files to ${curr_target_dev}" | tee -a ${MY_LOG}
            cleanup_exit
        fi

        ctds=`echo ${curr_target_dev} | sed 's/p$//'`
        echo "(hd0) ${ctds}" > ${mount_point}/boot/grub/device.map

        # Install the grub
        /sbin/grub --batch --device-map=/dev/null <<EOF
device (hd0) ${ctds}
root (hd0,0)
setup (hd0)
quit
EOF
        if [ ${?} -ne 0 ]; then
            echo "*** Could not install grub on ${curr_target_dev}" | tee -a ${MY_LOG}
            cleanup_exit
        fi
        return
    fi
    if [ -f /tmp/raid_disk_info.txt ] ; then
        # Dot in the file.  It sets RAID_DEV, D1_PDEV and others.
        # (e.g. values RAID_DEV=/dev/md0, D1_PDEV=/dev/sda)
        RAID_DEV=na
        D1_DEV=na
        D2_DEV=na
        D1_PDEV=na
        D2_PDEV=na
        . /tmp/raid_disk_info.txt
        if [ "${curr_target_dev}" = "${RAID_DEV}" ] ; then
            echo ++ Set up grub for raid boot | tee -a ${MY_LOG}
            # Web page with very useful info:
            # http://www.jms1.net/grub-raid-1.shtml

            # TBD install grub for Mirrored boot drives.
            DID_GRUB_INSTALL=1
            echo "Defer grub install ${RAID_DEV} ${D1_DEV} ${D2_DEV}" | tee -a ${MY_LOG}
            echo ${curr_target_dev},${mount_point}  > /tmp/grub_deferred.txt
            find ${mount_point} | xargs ls -ld  >> /tmp/grub_deferred.txt
            return
        fi
    fi
    if [ "${curr_target_dev}" != "/dev/sda" ] ; then
        # If the root device is not sda then update device.map file
        # with device as hd(0)
        echo "Put into ${mount_point}/boot/grub/device.map: (hd0) ${curr_target_dev}"  >> ${MY_LOG}
        echo "(hd0) ${curr_target_dev}" > ${mount_point}/boot/grub/device.map
        return
    fi
}

JLOG=/tmp/wi_j.log
# This graft point is called right before getting the size and other
# drive information.
# When we are installing or upgrading on RAID mirror root drive, the device
# name in ${imdev} is the raid device name (E.g. /dev/md)".
# We want the writeimage code to get the disk info from real device.
HAVE_WRITEIMAGE_GRAFT_J1=y
writeimage_graft_J1()
{
  case ${imdev} in
  /dev/md*) ;;
  *) return ;;
  esac
  echo J1 `date` >> ${JLOG}
  set > /tmp/wi_j1_env
  SAVE_DEV=
  # This can be called when doing a manufacture and when doing an upgrade.
  if [ -d /opt/nkn/startup ] ; then
    Upgrade_mirror_devs
  else
    Manufacture_mirror_devs
  fi
}


Manufacture_mirror_devs()
{
  [ ! -f /tmp/raid_disk_info.txt ] && return
  # Dot in the raid_disk_info.txt file.  It sets RAID_DEV, D1_PDEV and others.
  # (e.g. values RAID_DEV=/dev/md, D1_PDEV=/dev/sda)
  RAID_DEV=na
  D1_DEV=na
  . /tmp/raid_disk_info.txt
  echo Test imdev=${imdev}  RAID_DEV=${RAID_DEV} >> ${JLOG}
  [ "${imdev}" != "${RAID_DEV}" ] && return
  SAVE_DEV=${imdev}
  imdev=${D1_DEV}
  echo NEW imdev=${imdev} >> ${JLOG}
  # Make sure all the partitions on both drives are clear.
  for PNUM in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
  do
    for D in ${D1_PDEV} ${D2_PDEV}
    do
      THIS_DEV=${D}${P}
      [ ! -b ${THIS_DEV} ] && continue
      # Erase the first 100M of the partition
      echo \
      dd if=/dev/zero of=${THIS_DEV} bs=1M count=100 >> ${JLOG} 2>&1
      dd if=/dev/zero of=${THIS_DEV} bs=1M count=100 >> ${JLOG} 2>&1
      SIZE_K=`sfdisk -q -s ${THIS_DEV}`
      echo Size of ${THIS_DEV} is ${SIZE_K} >> ${JLOG}
      # Erase the last 100M of the partition
      # Use awk because busybox expr does not work above 2^32.
      OFFSET_M=`echo ${SIZE_K} | awk '{f= $1 / 1024 - 100; if (f > 0) {print int(f)} else {print 0}}'`
      echo \
      dd if=/dev/zero of=${THIS_DEV} bs=1M seek=${OFFSET_M} count=100 >> ${JLOG} 2>&1
      dd if=/dev/zero of=${THIS_DEV} bs=1M seek=${OFFSET_M} count=100 >> ${JLOG} 2>&1
    done
  done
  echo \
  dd if=/dev/zero of=${D1_DEV} bs=1M count=100 >> ${JLOG} 2>&1
  dd if=/dev/zero of=${D1_DEV} bs=1M count=100 >> ${JLOG} 2>&1
  echo \
  dd if=/dev/zero of=${D2_DEV} bs=1M count=100 >> ${JLOG} 2>&1
  dd if=/dev/zero of=${D2_DEV} bs=1M count=100 >> ${JLOG} 2>&1
}

Upgrade_mirror_devs()
{
  # If not using mirror root, then just return.
  grep -q "active raid1" /proc/mdstat
  [ ${?} -ne 0 ] && return
  # The RAID_DEV global is only used during manufacturing.
  RAID_DEV=na
  # Put the real HDD device name in HDD_DEV
  LINE=`grep "active raid1" /proc/mdstat | head -1`
  D=`echo ${LINE} | cut -f 5 -d' ' | cut -f1 -d'['`
  [ -z "${D}" ] && return
  echo Mirror imdev=${imdev} >> ${JLOG}
  SAVE_DEV=${imdev}
  imdev=/dev/${D}
  echo NEW imdev=${imdev} >> ${JLOG}
}

HAVE_WRITEIMAGE_GRAFT_J2=y
writeimage_graft_J2()
{
  echo J2 `date` >> ${JLOG}
  [ -z "${SAVE_DEV}" ] && return
  imdev=${SAVE_DEV}
  echo Restore imdev=${imdev} >> ${JLOG}
}


# This graft point is called right before creating the partition table for
# a disk and then zeroing the partition.
# When we are want to install on RAID root drive, the device name in
# ${target_dev} is the raid device name (E.g. /dev/md0)".
# We want the writeimage code to create the partitions on the real device.
HAVE_WRITEIMAGE_GRAFT_J3=y
writeimage_graft_J3()
{
  echo J3 `date` >> ${JLOG}
  set > /tmp/wi_j3_env
  SAVE_DEV=
  [ ! -f /tmp/raid_disk_info.txt ] && return
  # Dot in the file.  It sets RAID_DEV, D1_PDEV and others.
  # (e.g. values RAID_DEV=/dev/md0, D1_PDEV=/dev/sda)
  RAID_DEV=na
  D1_DEV=na
  . /tmp/raid_disk_info.txt
  echo Test target_dev=${target_dev}  RAID_DEV=${RAID_DEV} >> ${JLOG}
  [ "${target_dev}" != "${RAID_DEV}" ] && return
  SAVE_DEV=${target_dev}
  target_dev=${D1_DEV}
  echo NEW target_dev=${target_dev} >> ${JLOG}
}


HAVE_WRITEIMAGE_GRAFT_J4=y
writeimage_graft_J4()
{
  echo J4 `date` >> ${JLOG}
  [ -z "${SAVE_DEV}" ] && return
  target_dev=${SAVE_DEV}
  # At this point the partitions have been created on the first drive.
  # Run a script to duplicate the partitions on the second drive
  # and then create the RAID partitions.
  cp ${temp_ptable} /tmp/wi_j4_ptable
  set > /tmp/wi_j4_env
  bash /sbin/make_raid.sh /tmp/raid_disk_info.txt /tmp/wi_j4_ptable \
     2>&1 | tee -a ${JLOG}
}


# -----------------------------------------------------------------------------

# Graft point #1 for aiset.sh.  This is called right before the bootmgr
# (grub) is re-installed.  It can be used to edit things in the device
# map: ${mount_point}/boot/grub/device.map if the existing device map
# needs to be changed.  Unlike HAVE_WRITEIMAGE_GRAFT_5, the device map
# already exists, so you likely do not need to use this graft point,
# unless you need to change the device map of a deployed box for some
# reason.  You should not just do the same thing here as in
# HAVE_WRITEIMAGE_GRAFT_5 .  This graft point can also be used to
# re-install the bootmgr yourself, at which point you should set
# DID_GRUB_INSTALL=1 .  See also, HAVE_WRITEIMAGE_GRAFT_5 above, which
# is used when initially installing the grub at manufacture time.

HAVE_AISET_GRAFT_1=n
# aiset_graft_1()
# {
# }

# -----------------------------------------------------------------------------
# Graft point #1 for manufacture.sh.  This is called during initialization.
# Table of layout scheme names:
#           With DM    Without DM   MirrorRoot
#         +----------+------------+-----------+
# VXA     |  vxa1    |  vxa2      | mirror    |
#         +----------+------------+-----------+
# non-VXA |  cache   |  normal    | mirror    |
#         +----------+------------+-----------+
#
# The "mirror" type does NOT have DM.
#
HAVE_MANUFACTURE_GRAFT_1=y
manufacture_graft_1()
{
     CFG_MODEL_CHOICES="normal mirror cache vxa1 vxa2 demo8g2 demo32g1 cloudvm cloudrc"
     CFG_MODEL_DEF="normal"
     # Note: we overridethe MODEL_*_DEV_LIST setting on the manufacture.sh
     # command line with the "-d /dev/..." parameter.

     # ###################################################################
     # Minimum disk size for this is 92GB disk.  Includes root drive cache.
     # Limit /nkn and /log to 102,400MB
     MODEL_cache_ENABLE=1
     MODEL_cache_KERNEL_TYPE="smp"
     MODEL_cache_LCD_TYPE="none"
     MODEL_cache_DEV_LIST="/dev/sda"
     MODEL_cache_LAYOUT="DEFAULT"
     MODEL_cache_IF_LIST=
     MODEL_cache_IF_NAMING="none"
     MODEL_cache_SMARTD_OPT="0"

     # ###################################################################
     # No root drive cache model. Both buf and log partitions grow equally.
     # Limit /nkn to 102,400MB
     MODEL_normal_ENABLE=1
     MODEL_normal_KERNEL_TYPE="smp"
     MODEL_normal_LCD_TYPE="none"
     MODEL_normal_DEV_LIST="/dev/sda"
     MODEL_normal_LAYOUT="BIGLOGNC"
     MODEL_normal_IF_LIST=
     MODEL_normal_IF_NAMING="none"
     MODEL_normal_SMARTD_OPT="0"

     # ###################################################################
     # Mirror root drive model. Both buf and log partitions grow equally.
     # Limit /nkn to 102,400MB
     MODEL_mirror_ENABLE=1
     MODEL_mirror_KERNEL_TYPE="smp"
     MODEL_mirror_LCD_TYPE="none"
     MODEL_mirror_DEV_LIST="/dev/sda"
     MODEL_mirror_LAYOUT="MIRROR"
     MODEL_mirror_IF_LIST=
     MODEL_mirror_IF_NAMING="none"
     MODEL_mirror_SMARTD_OPT="0"

     # ###################################################################
     # Minimum disk size for this is 8GB disk.  No root drive cache.
     # For demo purposes only.
     MODEL_demo8g2_ENABLE=1
     MODEL_demo8g2_KERNEL_TYPE="smp"
     MODEL_demo8g2_LCD_TYPE="none"
     MODEL_demo8g2_DEV_LIST="/dev/sda"
     MODEL_demo8g2_LAYOUT="8GNCACHE"
     MODEL_demo8g2_IF_LIST=
     MODEL_demo8g2_IF_NAMING="none"
     MODEL_demo8g2_SMARTD_OPT="0"

     # ###################################################################
     # Minimum disk size for this is 32GB disk. With Cache.
     # Limit /nkn and /log to 102,400MB
     # For demo purposes only.
     MODEL_demo32g1_ENABLE=1
     MODEL_demo32g1_KERNEL_TYPE="smp"
     MODEL_demo32g1_LCD_TYPE="none"
     MODEL_demo32g1_DEV_LIST="/dev/sda"
     MODEL_demo32g1_LAYOUT="32G"
     MODEL_demo32g1_IF_LIST=
     MODEL_demo32g1_IF_NAMING="none"
     MODEL_demo32g1_SMARTD_OPT="0"

     # ###################################################################
     # VXA installation, with cache on first hard drive.
     # Similar to "cache=DEFAULT" but for VXA with eUSB.
     MODEL_vxa1_ENABLE=1
     MODEL_vxa1_KERNEL_TYPE="smp"
     MODEL_vxa1_LCD_TYPE="none"
     MODEL_vxa1_DEV_LIST="/dev/sda /dev/sdb"
     MODEL_vxa1_LAYOUT="FLASHROOT"
     MODEL_vxa1_IF_LIST=
     MODEL_vxa1_IF_NAMING="none"
     MODEL_vxa1_SMARTD_OPT="0"

     # ###################################################################
     # VXA installation, with NO cache on first hard drive.
     # Similar to "normal=BIGLOGNC" but for VXA with eUSB.
     MODEL_vxa2_ENABLE=1
     MODEL_vxa2_KERNEL_TYPE="smp"
     MODEL_vxa2_LCD_TYPE="none"
     MODEL_vxa2_DEV_LIST="/dev/sda /dev/sdb"
     MODEL_vxa2_LAYOUT="FLASHNCACHE"
     MODEL_vxa2_IF_LIST=
     MODEL_vxa2_IF_NAMING="none"
     MODEL_vxa2_SMARTD_OPT="0"

     # ###################################################################
     # PACIFICA installation, with root and log on RAMFS and the rest 
     # on first drive namely sda. 
     MODEL_pacifica_ENABLE=1
     MODEL_pacifica_KERNEL_TYPE="smp"
     MODEL_pacifica_LCD_TYPE="none"
     MODEL_pacifica_DEV_LIST="/dev/sda"
     MODEL_pacifica_LAYOUT="PACIFICA"
     MODEL_pacifica_IF_LIST=
     MODEL_pacifica_IF_NAMING="none"
     MODEL_pacifica_SMARTD_OPT="0"

     # ###################################################################
     # CLOUDVM installation. No root drive cache.
     # Small log and coredump.
     MODEL_cloudvm_ENABLE=1
     MODEL_cloudvm_KERNEL_TYPE="smp"
     MODEL_cloudvm_LCD_TYPE="none"
     MODEL_cloudvm_DEV_LIST="/dev/vda"
     MODEL_cloudvm_LAYOUT="CLOUDVM"
     MODEL_cloudvm_IF_LIST=
     MODEL_cloudvm_IF_NAMING="none"
     MODEL_cloudvm_SMARTD_OPT="0"

     # ###################################################################
     # CLOUDRC installation. With root drive cache.
     # Small coredump.
     MODEL_cloudrc_ENABLE=1
     MODEL_cloudrc_KERNEL_TYPE="smp"
     MODEL_cloudrc_LCD_TYPE="none"
     MODEL_cloudrc_DEV_LIST="/dev/vda"
     MODEL_cloudrc_LAYOUT="CLOUDRC"
     MODEL_cloudrc_IF_LIST=
     MODEL_cloudrc_IF_NAMING="none"
     MODEL_cloudrc_SMARTD_OPT="0"

     # ###################################################################

    FAILURE=0
    make_cciss_devs /dev/cciss || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
        echo "*** Could not make cciss devs"
        exit 1
    fi
}

# -----------------------------------------------------------------------------
# Graft point #2 for manufacture.sh.  This is called towards the end, after
# writeimage.  You can do anything you want here, such as acting on custom
# options that you parsed from the command line or prompted the user for
# interactively, using graft points 3-6.
#
HAVE_MANUFACTURE_GRAFT_2=y
manufacture_graft_2()
{
  [ ! -s /tmp/from_pkg_dir/post_img_install.sh ] && return
  if [ -d /log/install ] ; then
    MY_LOG=/log/install/manufacture_g2.log
  elif [ -d /log ] ; then
    MY_LOG=/log/manufacture_g2.log
  else
    MY_LOG=/tmp/manufacture_g2.log
  fi
  date > ${MY_LOG}
  df -k >> ${MY_LOG}
  # Dot the script file in and run the function.
  . /tmp/from_pkg_dir/post_img_install.sh
  grep -q "Manufacture_g2" /tmp/from_pkg_dir/post_img_install.sh
  if [ ${?} -eq 0 ] ; then
    Manufacture_g2 >> ${MY_LOG} 2>&1
  fi
}

# -----------------------------------------------------------------------------
# Graft point #3 for manufacture.sh.  This is called from within the
# function that prints a usage message if the user enters invalid
# options.  Use this to print a summary of your options.
# See the context in manufacture.sh.
#
HAVE_MANUFACTURE_GRAFT_3=n
#manufacture_graft_3()
#{
#}

# -----------------------------------------------------------------------------
# Graft point #3b for manufacture.sh.  This is called from within the
# function that prints a usage message if the user enters invalid
# options.  Use this to print a longer description of your options.
# See the context in manufacture.sh.
#
HAVE_MANUFACTURE_GRAFT_3b=n
#manufacture_graft_3b()
#{
#}

# -----------------------------------------------------------------------------
# Graft point #4b for manufacture.sh.  This is called after the base XOPT
# parsing of the kernel command-line parameters.  This is for
# customer-specific options you want to be able to set via the PXE 'append'
# method, in order to automatically image systems with certain settings.
#
HAVE_MANUFACTURE_GRAFT_4b=n
# manufacture_graft_4b()
# {
# }

# -----------------------------------------------------------------------------
# Graft point #4 for manufacture.sh.  This is called before we run getopt
# to parse the command-line options.  You can add your own options in
# getopt format to $GETOPT_OPTS.  You should also initialize any
# variables you'll use to hold your options, e.g. "HAVE_OPT_HELLO=0"
# and "OPT_HELLO=".  Again, see the context in manufacture.sh.
#
HAVE_MANUFACTURE_GRAFT_4=n
#manufacture_graft_4()
#{
#}

# -----------------------------------------------------------------------------
# Graft point #5 for manufacture.sh.  This is called after the main
# script has parsed all of its basic options.  The options are in $PARSE,
# so you should refresh them with:
#     eval set -- "$PARSE"
# ...and then iterate through them like manufacture.sh does, 
# looking for your own options.
#
HAVE_MANUFACTURE_GRAFT_5=n
#manufacture_graft_5()
#{
#}

# -----------------------------------------------------------------------------
# Graft point #6 for manufacture.sh.  This is called after manufacture.sh
# has processed most of its options, but before it has taken any action.
# You should process your options here.  If an option was specified,
# copy it into the variable that you will use later. e.g. copy
# $OPT_HELLO to $CFG_HELLO.  Otherwise, prompt the user for the option,
# perhaps with a default in square brackets, and take whatever they specify.
# Either way, print out a "Using ..." message that says what the value of
# your option is.  Don't act on your option yet; you can do this later 
# in graft point 2 (which runs after us, despite its number).
#
HAVE_MANUFACTURE_GRAFT_6=y
manufacture_graft_6()
{
  # We do not need some of the things in MFG_POST_COPY.
  MFG_POST_COPY=`echo ${MFG_POST_COPY} | sed -e "#/opt/tms/bin/genlicense#s###"`
}

# -----------------------------------------------------------------------------
# Graft point #1 for remanufacture.sh.  This is called during pass 1, right
# after the base binaries have been copied over, but before the pivot_root.
# It could be used for copying over other files or stopping other processes.
#
HAVE_REMANUFACTURE_GRAFT_1=n
# remanufacture_graft_1()
# {
# }

# -----------------------------------------------------------------------------
# Graft point #2 for remanufacture.sh.  This is called during pass 2, right
# before we'll call manufacture.sh .  Note that this is in the new, chroot'd
# to environment, not the regular full system.  It could be used for setting
# MFG_ARGS to contain other settings to pass to manufacture.sh , or to call
# manufacture.sh directrly, by setting DO_MFG=0 .  
#
HAVE_REMANUFACTURE_GRAFT_2=n
# remanufacture_graft_2()
# {
# }

# -----------------------------------------------------------------------------
# Graft point #3 for remanufacture.sh.  This is called during pass 3, right
# after we potentially copy over the system config.  It could be used for
# copying over other manufacture time settings from the old system.  
# NEW_MFG_DB_PATH and NEW_MFG_INC_DB_PATH are the new ones, and MFG_DB_PATH
# and MFG_INC_DB_PATH are the old ones (from the system before the
# remanufacture).
#
HAVE_REMANUFACTURE_GRAFT_3=n
# remanufacture_graft_3()
# {
# }

# -----------------------------------------------------------------------------
# Graft point #4 for remanufacture.sh.  This is called during pass 1,
# for customer-specific shutdown or cleanup before killing all processes.
#
HAVE_REMANUFACTURE_GRAFT_4=n
# remanufacture_graft_4()
# {
# }

# -----------------------------------------------------------------------------
# Graft point #5 for remanufacture.sh.  This is called at the start of pass 0.
#
HAVE_REMANUFACTURE_GRAFT_5=y
remanufacture_graft_5()
{
  # Remanufacture is not supported.
  # (hidden CLI: image remanufacture <imagename>)
  # We could support it but we would need a bunch of code in the other
  # remanufacture_graft functions to do what the normal install scripts do.
  vlechoerr -1 "*** Remanufacturing is not supported."
  exit 1
}

# -----------------------------------------------------------------------------
# Graft point #6 for remanufacture.sh.  This is called during pass 1, before
# creating /rootfs, for Customer-specific configuration, such as
# changing ROOTFS_SIZE.
#
HAVE_REMANUFACTURE_GRAFT_6=n
# remanufacture_graft_6()
# {
# }

# -----------------------------------------------------------------------------
# Graft point #7 for remanufacture.sh.  This is called during pass 2, right
# after running manufacture.sh, before the test for success or failure.
#
HAVE_REMANUFACTURE_GRAFT_7=n
# remanufacture_graft_7()
# {
# }

# -----------------------------------------------------------------------------
# Graft point #1 for imgverify.sh.  This is called before any actions
# have been taken, to allow default settings to be overridden.  
#
HAVE_IMGVERIFY_GRAFT_1=n
# imgverify_graft_1()
# {
# }

# -----------------------------------------------------------------------------
# Graft point #1 for layout-settings.sh.  This is used to allow custom
# layouts to be defined
# For our own use:
# -- Optional parameter to NOT dot in the update file at the end: noupdate
#
HAVE_LAYOUT_SETTINGS_GRAFT_1=y
layout_settings_graft_1()
{
# #####################################################
# About setting the coredump size.
# #####################################################
# Mike wants the coredump partitions at least 64GB so able to
# get a uncompressed nvsd core dump as big as 50GB along with
# the compressed version which would be less than 10GB.
# A compressed vmcore generated when testing 512K concurrent connections
# was not more tha 6GB, regardless of the RAM size.
# Kumar wants 72GB.
# 72GB gives some headroom beyond (10 + 50 + 6) for other # process dumps.
#  Note: 72 * 1024 = 73728 MB
# Special for non-supported disk sizes::::::::::::::::::::::::::::::::
# For disks sizes that are less than the minimum size, which are for demo
# purposes only, we make coredump smaller.
# Make it 4096MB for the layout for 32GB drives.
# For the 4GB drive layout use even smaller coredump (probably too small
# to be useful other than letting the system run normally).

# # Calculated true minimum disk sizes (M) from the beginning partition sizes:
#                              BMGR BOOT BOOT ROOT1 ROOT2 CNFG   VAR  CORED    NKN    LOG   DFS  DRAW
# echo "# cache-DEFAULT     $(( 128 +128 +128 +4000 +4000 +256 +3072 +73728 +21000  +9000   +800 +8000 ))"
# echo "# normal-BIGLOGNC   $(( 128 +128 +128 +4000 +4000 +256 +3072 +73728 +21000  +9000              ))"
# echo "# mirror-MIRROR     $(( 128 +128 +128 +4000 +4000 +256 +3072 +73728 +21000  +9000 +10000       ))"
# echo "# vxa1-FLASHROOT    $((                                       73728 +21000 +10000   +900 +9000 ))"
# echo "# vxa2-FLASHNCACHE  $((                                       73728 +21000 +10000              ))"
# echo "# demo32g1-32G      $((  30  +30  +30 +2000 +2000 +256 +3072  +4096  +5000  +5000   +300 +3000 ))"
# echo "# demo8g2-8GNCACHE  $((  30  +30  +30 +2000 +2000  +64  +580   +100   +500   +500              ))"
# echo "# pacifica-PACIFICA $((                           +256 +1024 +40960 +20480          +300 +3000 ))"
# echo "# cloudvm-CLOUDVM   $(( 128 +128 +128 +2000 +2000 +256 +3072   +100  +5000   +500              ))"
# echo "# cloudrc-CLOUDRC   $(( 128 +128 +128 +2000 +2000 +256 +3072   +100  +5000  +5000   +300 +3000 ))"
#                              BMGR BOOT BOOT ROOT1 ROOT2 CNFG   VAR  CORED    NKN    LOG   DFS  DRAW
# # The "+10000" partition in Mirror under the DFS column is really is for spare 10G partition.

# cache-DEFAULT    124240 + size of RAM over 64G  stated min size 140000+  (15,760 MB diff)
# normal-BIGLOGNC  115440 + size of RAM over 64G  stated min size 130000+  (14,560 MB diff)
# mirror-MIRROR    125440 + size of RAM over 64G  stated min size 130000+  ( 4,560 MB diff)
# vxa1-FLASHROOT   114628 + size of RAM over 64G  stated min size 130000+  (15,372 MB diff)
# vxa2-FLASHNCACHE 104728 + size of RAM over 64G  stated min size 120000+  (15,272 MB diff)
# demo32g1-32G      24814  stated min size  31000
# demo8g2-8GNCACHE   5834  stated min size   8000
# pacifica-PACIFICA 66020  stated min size  67000
# cloudvm-CLOUDVM   13312  stated min size  14000
# cloudrc-CLOUDRC   21112  stated min size  22000


# ###########################################################################
# ###########################################################################
# ==========================================================
##DEFAULT: Cache on root drive and limit /nkn and /log to 102,400MB
##DEFAULT: The cache, nkn and log partitions are the same size until the limit.
# Partitions :
#  1 : /bootmgr
#  2 : /boot (1 of 2)
#  3 : /boot (2 of 2) - 2nd boot image
#  4 : extended partition
#  5 : / (1 of 2)
#  6 : / (2 of 2) - for 2nd boot image 
#  7 : /config
#  8 : /var
#  9 : /coredump
# 10 : /nkn
# 11 : /log
# 12 : dmfs
# 13 : dmraw
# ==========================================================

##Model: cache -> DEFAULT
IL_LO_DEFAULT_LOCS="BOOTMGR_1 BOOT_1 BOOT_2 ROOT_1 ROOT_2 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1 DMFS_1 DMRAW_1"

# Targets are things like a disk or a flash device
IL_LO_DEFAULT_TARGET_NUM_TARGETS=1
IL_LO_DEFAULT_TARGETS="DISK1"
IL_LO_DEFAULT_TARGET_DISK1_NUM=1

# Configuration option to indicate that the root locations
# should be read-only. It is still necessary to indicate
# 'ro' in the partition option line for root.
IL_LO_DEFAULT_ROOT_RDONLY=1

IL_LO_DEFAULT_INITRD_PATH=
IL_LO_DEFAULT_ROOT_MOUNT_BY_LABEL=0

IL_LO_DEFAULT_TARGET_DISK1_MINSIZE=${MIN_SIZE_DEFAULT}
IL_LO_DEFAULT_TARGET_DISK1_LABELTYPE=

# What partitions get made on the targets
IL_LO_DEFAULT_PARTS="BOOTMGR BOOT1 BOOT2 EXT ROOT1 ROOT2 CONFIG VAR COREDUMP NKN LOG DMFS DMRAW"

IL_LO_DEFAULT_IMAGE_1_INSTALL_LOCS="BOOT_1 ROOT_1"
IL_LO_DEFAULT_IMAGE_2_INSTALL_LOCS="BOOT_2 ROOT_2"
IL_LO_DEFAULT_IMAGE_1_LOCS="ROOT_1 BOOT_1 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"
IL_LO_DEFAULT_IMAGE_2_LOCS="ROOT_2 BOOT_2 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

IL_LO_DEFAULT_OPT_LAST_PART_FILL=1
IL_LO_DEFAULT_OPT_ALL_PARTS_FILL=1

IL_LO_DEFAULT_EXTRA_KERNEL_PARAMS="crashkernel=auto"

# Each of these says which PART (partition) below goes with which LOC
#     (standardized location) .  It also says for each LOC which subdirectory
#     under the PART the LOC is in, and what files to extract there

IL_LO_DEFAULT_LOC_BOOTMGR_1_PART=BOOTMGR
IL_LO_DEFAULT_LOC_BOOTMGR_1_DIR=/
IL_LO_DEFAULT_LOC_BOOTMGR_1_IMAGE_EXTRACT="./bootmgr"
IL_LO_DEFAULT_LOC_BOOTMGR_1_IMAGE_EXTRACT_PREFIX="/bootmgr"

IL_LO_DEFAULT_LOC_BOOT_1_PART=BOOT1
IL_LO_DEFAULT_LOC_BOOT_1_DIR=/
IL_LO_DEFAULT_LOC_BOOT_1_IMAGE_EXTRACT="./boot"
IL_LO_DEFAULT_LOC_BOOT_1_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_DEFAULT_LOC_BOOT_2_PART=BOOT2
IL_LO_DEFAULT_LOC_BOOT_2_DIR=/
IL_LO_DEFAULT_LOC_BOOT_2_IMAGE_EXTRACT="./boot"
IL_LO_DEFAULT_LOC_BOOT_2_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_DEFAULT_LOC_ROOT_1_PART=ROOT1
IL_LO_DEFAULT_LOC_ROOT_1_DIR=/
IL_LO_DEFAULT_LOC_ROOT_1_IMAGE_EXTRACT="./"
IL_LO_DEFAULT_LOC_ROOT_1_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_DEFAULT_LOC_ROOT_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_DEFAULT_LOC_ROOT_2_PART=ROOT2
IL_LO_DEFAULT_LOC_ROOT_2_DIR=/
IL_LO_DEFAULT_LOC_ROOT_2_IMAGE_EXTRACT="./"
IL_LO_DEFAULT_LOC_ROOT_2_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_DEFAULT_LOC_ROOT_2_IMAGE_EXTRACT_PREFIX=""

IL_LO_DEFAULT_LOC_CONFIG_1_PART=CONFIG
IL_LO_DEFAULT_LOC_CONFIG_1_DIR=/
IL_LO_DEFAULT_LOC_CONFIG_1_IMAGE_EXTRACT="./config"
IL_LO_DEFAULT_LOC_CONFIG_1_IMAGE_EXTRACT_PREFIX="/config"

IL_LO_DEFAULT_LOC_VAR_1_PART=VAR
IL_LO_DEFAULT_LOC_VAR_1_DIR=/
IL_LO_DEFAULT_LOC_VAR_1_IMAGE_EXTRACT="./var"
IL_LO_DEFAULT_LOC_VAR_1_IMAGE_EXTRACT_PREFIX="/var"

IL_LO_DEFAULT_LOC_COREDUMP_1_PART=COREDUMP
IL_LO_DEFAULT_LOC_COREDUMP_1_DIR=/
IL_LO_DEFAULT_LOC_COREDUMP_1_IMAGE_EXTRACT="./coredump"
IL_LO_DEFAULT_LOC_COREDUMP_1_IMAGE_EXTRACT_PREFIX="/coredump"

IL_LO_DEFAULT_LOC_NKN_1_PART=NKN
IL_LO_DEFAULT_LOC_NKN_1_DIR=/
IL_LO_DEFAULT_LOC_NKN_1_IMAGE_EXTRACT="./nkn"
IL_LO_DEFAULT_LOC_NKN_1_IMAGE_EXTRACT_PREFIX="/nkn"

IL_LO_DEFAULT_LOC_LOG_1_PART=LOG
IL_LO_DEFAULT_LOC_LOG_1_DIR=/
IL_LO_DEFAULT_LOC_LOG_1_IMAGE_EXTRACT="./log"
IL_LO_DEFAULT_LOC_LOG_1_IMAGE_EXTRACT_PREFIX="/log"

IL_LO_DEFAULT_LOC_DMFS_1_PART=DMFS
IL_LO_DEFAULT_LOC_DMFS_1_DIR=/
IL_LO_DEFAULT_LOC_DMFS_1_IMAGE_EXTRACT=""
IL_LO_DEFAULT_LOC_DMFS_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_DEFAULT_LOC_DMRAW_1_PART=DMRAW
IL_LO_DEFAULT_LOC_DMRAW_1_DIR=/
IL_LO_DEFAULT_LOC_DMRAW_1_IMAGE_EXTRACT=""
IL_LO_DEFAULT_LOC_DMRAW_1_IMAGE_EXTRACT_PREFIX=""


# Each of these says how a partition is named, labled, and sized.

IL_LO_DEFAULT_PART_BOOTMGR_TARGET=DISK1
IL_LO_DEFAULT_PART_BOOTMGR_PART_NUM=1
IL_LO_DEFAULT_PART_BOOTMGR_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_BOOTMGR_PART_NUM}
IL_LO_DEFAULT_PART_BOOTMGR_LABEL=BOOTMGR
IL_LO_DEFAULT_PART_BOOTMGR_MOUNT=/bootmgr
IL_LO_DEFAULT_PART_BOOTMGR_BOOTABLE=1
IL_LO_DEFAULT_PART_BOOTMGR_PART_ID=83
IL_LO_DEFAULT_PART_BOOTMGR_FSTYPE=ext3
IL_LO_DEFAULT_PART_BOOTMGR_SIZE=128
IL_LO_DEFAULT_PART_BOOTMGR_GROWTHCAP=
IL_LO_DEFAULT_PART_BOOTMGR_GROWTHWEIGHT=
IL_LO_DEFAULT_PART_BOOTMGR_OPTIONS="defaults,noatime,ro"

IL_LO_DEFAULT_PART_BOOT1_TARGET=DISK1
IL_LO_DEFAULT_PART_BOOT1_PART_NUM=2
IL_LO_DEFAULT_PART_BOOT1_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_BOOT1_PART_NUM}
IL_LO_DEFAULT_PART_BOOT1_LABEL=BOOT_1
IL_LO_DEFAULT_PART_BOOT1_MOUNT=/boot
IL_LO_DEFAULT_PART_BOOT1_BOOTABLE=
IL_LO_DEFAULT_PART_BOOT1_PART_ID=83
IL_LO_DEFAULT_PART_BOOT1_FSTYPE=ext3
IL_LO_DEFAULT_PART_BOOT1_SIZE=128
IL_LO_DEFAULT_PART_BOOT1_GROWTHCAP=
IL_LO_DEFAULT_PART_BOOT1_GROWTHWEIGHT=
IL_LO_DEFAULT_PART_BOOT1_OPTIONS="defaults,noatime,ro"

IL_LO_DEFAULT_PART_BOOT2_TARGET=DISK1
IL_LO_DEFAULT_PART_BOOT2_PART_NUM=3
IL_LO_DEFAULT_PART_BOOT2_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_BOOT2_PART_NUM}
IL_LO_DEFAULT_PART_BOOT2_LABEL=BOOT_2
IL_LO_DEFAULT_PART_BOOT2_MOUNT=/boot
IL_LO_DEFAULT_PART_BOOT2_BOOTABLE=
IL_LO_DEFAULT_PART_BOOT2_PART_ID=83
IL_LO_DEFAULT_PART_BOOT2_FSTYPE=ext3
IL_LO_DEFAULT_PART_BOOT2_SIZE=128
IL_LO_DEFAULT_PART_BOOT2_GROWTHCAP=
IL_LO_DEFAULT_PART_BOOT2_GROWTHWEIGHT=
IL_LO_DEFAULT_PART_BOOT2_OPTIONS="defaults,noatime,ro"

IL_LO_DEFAULT_PART_EXT_TARGET=DISK1
IL_LO_DEFAULT_PART_EXT_PART_NUM=4
IL_LO_DEFAULT_PART_EXT_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_EXT_PART_NUM}
IL_LO_DEFAULT_PART_EXT_LABEL=
IL_LO_DEFAULT_PART_EXT_MOUNT=
IL_LO_DEFAULT_PART_EXT_BOOTABLE=
IL_LO_DEFAULT_PART_EXT_PART_ID=0f
IL_LO_DEFAULT_PART_EXT_FSTYPE=
IL_LO_DEFAULT_PART_EXT_SIZE=
IL_LO_DEFAULT_PART_EXT_GROWTHCAP=
IL_LO_DEFAULT_PART_EXT_GROWTHWEIGHT=

IL_LO_DEFAULT_PART_ROOT1_TARGET=DISK1
IL_LO_DEFAULT_PART_ROOT1_PART_NUM=5
IL_LO_DEFAULT_PART_ROOT1_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_ROOT1_PART_NUM}
IL_LO_DEFAULT_PART_ROOT1_LABEL=ROOT_1
IL_LO_DEFAULT_PART_ROOT1_MOUNT=/
IL_LO_DEFAULT_PART_ROOT1_BOOTABLE=
IL_LO_DEFAULT_PART_ROOT1_PART_ID=83
IL_LO_DEFAULT_PART_ROOT1_FSTYPE=ext3
IL_LO_DEFAULT_PART_ROOT1_SIZE=4000
IL_LO_DEFAULT_PART_ROOT1_GROWTHCAP=
IL_LO_DEFAULT_PART_ROOT1_GROWTHWEIGHT=
IL_LO_DEFAULT_PART_ROOT1_OPTIONS="defaults,noatime,ro"

IL_LO_DEFAULT_PART_ROOT2_TARGET=DISK1
IL_LO_DEFAULT_PART_ROOT2_PART_NUM=6
IL_LO_DEFAULT_PART_ROOT2_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_ROOT2_PART_NUM}
IL_LO_DEFAULT_PART_ROOT2_LABEL=ROOT_2
IL_LO_DEFAULT_PART_ROOT2_MOUNT=/
IL_LO_DEFAULT_PART_ROOT2_BOOTABLE=
IL_LO_DEFAULT_PART_ROOT2_PART_ID=83
IL_LO_DEFAULT_PART_ROOT2_FSTYPE=ext3
IL_LO_DEFAULT_PART_ROOT2_SIZE=4000
IL_LO_DEFAULT_PART_ROOT2_GROWTHCAP=
IL_LO_DEFAULT_PART_ROOT2_GROWTHWEIGHT=
IL_LO_DEFAULT_PART_ROOT2_OPTIONS="defaults,noatime,ro"

IL_LO_DEFAULT_PART_CONFIG_TARGET=DISK1
IL_LO_DEFAULT_PART_CONFIG_PART_NUM=7
IL_LO_DEFAULT_PART_CONFIG_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_CONFIG_PART_NUM}
IL_LO_DEFAULT_PART_CONFIG_LABEL=CONFIG
IL_LO_DEFAULT_PART_CONFIG_MOUNT=/config
IL_LO_DEFAULT_PART_CONFIG_BOOTABLE=
IL_LO_DEFAULT_PART_CONFIG_PART_ID=83
IL_LO_DEFAULT_PART_CONFIG_FSTYPE=ext3
IL_LO_DEFAULT_PART_CONFIG_SIZE=256
IL_LO_DEFAULT_PART_CONFIG_GROWTHCAP=
IL_LO_DEFAULT_PART_CONFIG_GROWTHWEIGHT=

IL_LO_DEFAULT_PART_VAR_TARGET=DISK1
IL_LO_DEFAULT_PART_VAR_PART_NUM=8
IL_LO_DEFAULT_PART_VAR_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_VAR_PART_NUM}
IL_LO_DEFAULT_PART_VAR_LABEL=VAR
IL_LO_DEFAULT_PART_VAR_MOUNT=/var
IL_LO_DEFAULT_PART_VAR_BOOTABLE=
IL_LO_DEFAULT_PART_VAR_PART_ID=83
IL_LO_DEFAULT_PART_VAR_FSTYPE=ext3
IL_LO_DEFAULT_PART_VAR_SIZE=3072
IL_LO_DEFAULT_PART_VAR_GROWTHCAP=
IL_LO_DEFAULT_PART_VAR_GROWTHWEIGHT=

IL_LO_DEFAULT_PART_COREDUMP_TARGET=DISK1
IL_LO_DEFAULT_PART_COREDUMP_PART_NUM=9
IL_LO_DEFAULT_PART_COREDUMP_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_COREDUMP_PART_NUM}
IL_LO_DEFAULT_PART_COREDUMP_LABEL=COREDUMP
IL_LO_DEFAULT_PART_COREDUMP_MOUNT=/coredump
IL_LO_DEFAULT_PART_COREDUMP_BOOTABLE=
IL_LO_DEFAULT_PART_COREDUMP_PART_ID=83
IL_LO_DEFAULT_PART_COREDUMP_FSTYPE=ext3
IL_LO_DEFAULT_PART_COREDUMP_SIZE=${COREDUMP_SIZE}
IL_LO_DEFAULT_PART_COREDUMP_GROWTHCAP=
IL_LO_DEFAULT_PART_COREDUMP_GROWTHWEIGHT=
IL_LO_DEFAULT_PART_COREDUMP_OPTIONS="defaults,noatime"

IL_LO_DEFAULT_PART_NKN_TARGET=DISK1
IL_LO_DEFAULT_PART_NKN_PART_NUM=10
IL_LO_DEFAULT_PART_NKN_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_NKN_PART_NUM}
IL_LO_DEFAULT_PART_NKN_LABEL=NKN
IL_LO_DEFAULT_PART_NKN_MOUNT=/nkn
IL_LO_DEFAULT_PART_NKN_BOOTABLE=
IL_LO_DEFAULT_PART_NKN_PART_ID=83
IL_LO_DEFAULT_PART_NKN_FSTYPE=ext3
IL_LO_DEFAULT_PART_NKN_SIZE=21000
IL_LO_DEFAULT_PART_NKN_GROWTHCAP=102400
IL_LO_DEFAULT_PART_NKN_GROWTHWEIGHT=33
IL_LO_DEFAULT_PART_NKN_OPTIONS="defaults,noatime"

IL_LO_DEFAULT_PART_LOG_TARGET=DISK1
IL_LO_DEFAULT_PART_LOG_PART_NUM=11
IL_LO_DEFAULT_PART_LOG_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_LOG_PART_NUM}
IL_LO_DEFAULT_PART_LOG_LABEL=LOG
IL_LO_DEFAULT_PART_LOG_MOUNT=/log
IL_LO_DEFAULT_PART_LOG_BOOTABLE=
IL_LO_DEFAULT_PART_LOG_PART_ID=83
IL_LO_DEFAULT_PART_LOG_FSTYPE=ext3
IL_LO_DEFAULT_PART_LOG_SIZE=9000
IL_LO_DEFAULT_PART_LOG_GROWTHCAP=102400
IL_LO_DEFAULT_PART_LOG_GROWTHWEIGHT=33
IL_LO_DEFAULT_PART_LOG_OPTIONS="defaults,noatime"

IL_LO_DEFAULT_PART_DMFS_TARGET=DISK1
IL_LO_DEFAULT_PART_DMFS_PART_NUM=12
IL_LO_DEFAULT_PART_DMFS_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_DMFS_PART_NUM}
IL_LO_DEFAULT_PART_DMFS_LABEL=DMFS
IL_LO_DEFAULT_PART_DMFS_MOUNT=
IL_LO_DEFAULT_PART_DMFS_BOOTABLE=
IL_LO_DEFAULT_PART_DMFS_PART_ID=83
  # For the dmfs parition set the FSTYPE to blank, so it will not create a new
  # filesystem on the partition, so we can preserve the contents.
  # Our install-mfd will do all the needed making of filesystems and labels,
  # because it knows when we want to preserve the data or not.
IL_LO_DEFAULT_PART_DMFS_FSTYPE=
IL_LO_DEFAULT_PART_DMFS_SIZE=800
IL_LO_DEFAULT_PART_DMFS_GROWTHCAP=
# We want this to be about 10% the size of DMRAW
IL_LO_DEFAULT_PART_DMFS_GROWTHWEIGHT=3

IL_LO_DEFAULT_PART_DMRAW_TARGET=DISK1
IL_LO_DEFAULT_PART_DMRAW_PART_NUM=13
IL_LO_DEFAULT_PART_DMRAW_DEV=${IL_LO_DEFAULT_TARGET_DISK1_DEV}${IL_LO_DEFAULT_PART_DMRAW_PART_NUM}
IL_LO_DEFAULT_PART_DMRAW_LABEL=DMRAW
IL_LO_DEFAULT_PART_DMRAW_MOUNT=
IL_LO_DEFAULT_PART_DMRAW_BOOTABLE=
IL_LO_DEFAULT_PART_DMRAW_PART_ID=
IL_LO_DEFAULT_PART_DMRAW_FSTYPE=
IL_LO_DEFAULT_PART_DMRAW_SIZE=8000
IL_LO_DEFAULT_PART_DMRAW_GROWTHCAP=
IL_LO_DEFAULT_PART_DMRAW_GROWTHWEIGHT=30


# ###########################################################################
# ###########################################################################
# ==========================================================
##BIGLOGNC: No cache on the root drive.  /nkn 50%, /log 50%
##BIGLOGNC: Limit /nkn to 102,400MB
# Partitions :
#  1 : /bootmgr
#  2 : /boot (1 of 2)
#  3 : /boot (2 of 2) - 2nd boot image
#  4 : extended partition
#  5 : / (1 of 2)
#  6 : / (2 of 2) - for 2nd boot image 
#  7 : /config
#  8 : /var
#  9 : /coredump
# 10 : /nkn
# 11 : /log
# ==========================================================

##Model: normal -> BIGLOGNC
IL_LO_BIGLOGNC_LOCS="BOOTMGR_1 BOOT_1 BOOT_2 ROOT_1 ROOT_2 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

# Targets are things like a disk or a flash device
IL_LO_BIGLOGNC_TARGET_NUM_TARGETS=1
IL_LO_BIGLOGNC_TARGETS="DISK1"
IL_LO_BIGLOGNC_TARGET_DISK1_NUM=1
IL_LO_BIGLOGNC_ROOT_RDONLY=1
IL_LO_BIGLOGNC_INITRD_PATH=
IL_LO_BIGLOGNC_ROOT_MOUNT_BY_LABEL=0
IL_LO_BIGLOGNC_TARGET_DISK1_MINSIZE=${MIN_SIZE_BIGLOGNC}
IL_LO_BIGLOGNC_TARGET_DISK1_LABELTYPE=

# What partitions get made on the targets
IL_LO_BIGLOGNC_PARTS="BOOTMGR BOOT1 BOOT2 EXT ROOT1 ROOT2 CONFIG VAR COREDUMP NKN LOG"

IL_LO_BIGLOGNC_IMAGE_1_INSTALL_LOCS="BOOT_1 ROOT_1"
IL_LO_BIGLOGNC_IMAGE_2_INSTALL_LOCS="BOOT_2 ROOT_2"
IL_LO_BIGLOGNC_IMAGE_1_LOCS="ROOT_1 BOOT_1 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"
IL_LO_BIGLOGNC_IMAGE_2_LOCS="ROOT_2 BOOT_2 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

IL_LO_BIGLOGNC_OPT_LAST_PART_FILL=1
IL_LO_BIGLOGNC_OPT_ALL_PARTS_FILL=1

IL_LO_BIGLOGNC_EXTRA_KERNEL_PARAMS="crashkernel=auto"

# Each of these says which PART (partition) below goes with which LOC
#     (standardized location) .  It also says for each LOC which subdirectory
#     under the PART the LOC is in, and what files to extract there

IL_LO_BIGLOGNC_LOC_BOOTMGR_1_PART=BOOTMGR
IL_LO_BIGLOGNC_LOC_BOOTMGR_1_DIR=/
IL_LO_BIGLOGNC_LOC_BOOTMGR_1_IMAGE_EXTRACT="./bootmgr"
IL_LO_BIGLOGNC_LOC_BOOTMGR_1_IMAGE_EXTRACT_PREFIX="/bootmgr"

IL_LO_BIGLOGNC_LOC_BOOT_1_PART=BOOT1
IL_LO_BIGLOGNC_LOC_BOOT_1_DIR=/
IL_LO_BIGLOGNC_LOC_BOOT_1_IMAGE_EXTRACT="./boot"
IL_LO_BIGLOGNC_LOC_BOOT_1_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_BIGLOGNC_LOC_BOOT_2_PART=BOOT2
IL_LO_BIGLOGNC_LOC_BOOT_2_DIR=/
IL_LO_BIGLOGNC_LOC_BOOT_2_IMAGE_EXTRACT="./boot"
IL_LO_BIGLOGNC_LOC_BOOT_2_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_BIGLOGNC_LOC_ROOT_1_PART=ROOT1
IL_LO_BIGLOGNC_LOC_ROOT_1_DIR=/
IL_LO_BIGLOGNC_LOC_ROOT_1_IMAGE_EXTRACT="./"
IL_LO_BIGLOGNC_LOC_ROOT_1_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_BIGLOGNC_LOC_ROOT_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_BIGLOGNC_LOC_ROOT_2_PART=ROOT2
IL_LO_BIGLOGNC_LOC_ROOT_2_DIR=/
IL_LO_BIGLOGNC_LOC_ROOT_2_IMAGE_EXTRACT="./"
IL_LO_BIGLOGNC_LOC_ROOT_2_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_BIGLOGNC_LOC_ROOT_2_IMAGE_EXTRACT_PREFIX=""

IL_LO_BIGLOGNC_LOC_CONFIG_1_PART=CONFIG
IL_LO_BIGLOGNC_LOC_CONFIG_1_DIR=/
IL_LO_BIGLOGNC_LOC_CONFIG_1_IMAGE_EXTRACT="./config"
IL_LO_BIGLOGNC_LOC_CONFIG_1_IMAGE_EXTRACT_PREFIX="/config"

IL_LO_BIGLOGNC_LOC_VAR_1_PART=VAR
IL_LO_BIGLOGNC_LOC_VAR_1_DIR=/
IL_LO_BIGLOGNC_LOC_VAR_1_IMAGE_EXTRACT="./var"
IL_LO_BIGLOGNC_LOC_VAR_1_IMAGE_EXTRACT_PREFIX="/var"

IL_LO_BIGLOGNC_LOC_COREDUMP_1_PART=COREDUMP
IL_LO_BIGLOGNC_LOC_COREDUMP_1_DIR=/
IL_LO_BIGLOGNC_LOC_COREDUMP_1_IMAGE_EXTRACT="./coredump"
IL_LO_BIGLOGNC_LOC_COREDUMP_1_IMAGE_EXTRACT_PREFIX="/coredump"

IL_LO_BIGLOGNC_LOC_NKN_1_PART=NKN
IL_LO_BIGLOGNC_LOC_NKN_1_DIR=/
IL_LO_BIGLOGNC_LOC_NKN_1_IMAGE_EXTRACT="./nkn"
IL_LO_BIGLOGNC_LOC_NKN_1_IMAGE_EXTRACT_PREFIX="/nkn"

IL_LO_BIGLOGNC_LOC_LOG_1_PART=LOG
IL_LO_BIGLOGNC_LOC_LOG_1_DIR=/
IL_LO_BIGLOGNC_LOC_LOG_1_IMAGE_EXTRACT="./log"
IL_LO_BIGLOGNC_LOC_LOG_1_IMAGE_EXTRACT_PREFIX="/log"

# Each of these says how a partition is named, labled, and sized.

IL_LO_BIGLOGNC_PART_BOOTMGR_TARGET=DISK1
IL_LO_BIGLOGNC_PART_BOOTMGR_PART_NUM=1
IL_LO_BIGLOGNC_PART_BOOTMGR_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_BOOTMGR_PART_NUM}
IL_LO_BIGLOGNC_PART_BOOTMGR_LABEL=BOOTMGR
IL_LO_BIGLOGNC_PART_BOOTMGR_MOUNT=/bootmgr
IL_LO_BIGLOGNC_PART_BOOTMGR_BOOTABLE=1
IL_LO_BIGLOGNC_PART_BOOTMGR_PART_ID=83
IL_LO_BIGLOGNC_PART_BOOTMGR_FSTYPE=ext3
IL_LO_BIGLOGNC_PART_BOOTMGR_SIZE=128
IL_LO_BIGLOGNC_PART_BOOTMGR_GROWTHCAP=
IL_LO_BIGLOGNC_PART_BOOTMGR_GROWTHWEIGHT=
IL_LO_BIGLOGNC_PART_BOOTMGR_OPTIONS="defaults,noatime,ro"

IL_LO_BIGLOGNC_PART_BOOT1_TARGET=DISK1
IL_LO_BIGLOGNC_PART_BOOT1_PART_NUM=2
IL_LO_BIGLOGNC_PART_BOOT1_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_BOOT1_PART_NUM}
IL_LO_BIGLOGNC_PART_BOOT1_LABEL=BOOT_1
IL_LO_BIGLOGNC_PART_BOOT1_MOUNT=/boot
IL_LO_BIGLOGNC_PART_BOOT1_BOOTABLE=
IL_LO_BIGLOGNC_PART_BOOT1_PART_ID=83
IL_LO_BIGLOGNC_PART_BOOT1_FSTYPE=ext3
IL_LO_BIGLOGNC_PART_BOOT1_SIZE=128
IL_LO_BIGLOGNC_PART_BOOT1_GROWTHCAP=
IL_LO_BIGLOGNC_PART_BOOT1_GROWTHWEIGHT=
IL_LO_BIGLOGNC_PART_BOOT1_OPTIONS="defaults,noatime,ro"

IL_LO_BIGLOGNC_PART_BOOT2_TARGET=DISK1
IL_LO_BIGLOGNC_PART_BOOT2_PART_NUM=3
IL_LO_BIGLOGNC_PART_BOOT2_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_BOOT2_PART_NUM}
IL_LO_BIGLOGNC_PART_BOOT2_LABEL=BOOT_2
IL_LO_BIGLOGNC_PART_BOOT2_MOUNT=/boot
IL_LO_BIGLOGNC_PART_BOOT2_BOOTABLE=
IL_LO_BIGLOGNC_PART_BOOT2_PART_ID=83
IL_LO_BIGLOGNC_PART_BOOT2_FSTYPE=ext3
IL_LO_BIGLOGNC_PART_BOOT2_SIZE=128
IL_LO_BIGLOGNC_PART_BOOT2_GROWTHCAP=
IL_LO_BIGLOGNC_PART_BOOT2_GROWTHWEIGHT=
IL_LO_BIGLOGNC_PART_BOOT2_OPTIONS="defaults,noatime,ro"

IL_LO_BIGLOGNC_PART_EXT_TARGET=DISK1
IL_LO_BIGLOGNC_PART_EXT_PART_NUM=4
IL_LO_BIGLOGNC_PART_EXT_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_EXT_PART_NUM}
IL_LO_BIGLOGNC_PART_EXT_LABEL=
IL_LO_BIGLOGNC_PART_EXT_MOUNT=
IL_LO_BIGLOGNC_PART_EXT_BOOTABLE=
IL_LO_BIGLOGNC_PART_EXT_PART_ID=0f
IL_LO_BIGLOGNC_PART_EXT_FSTYPE=
IL_LO_BIGLOGNC_PART_EXT_SIZE=
IL_LO_BIGLOGNC_PART_EXT_GROWTHCAP=
IL_LO_BIGLOGNC_PART_EXT_GROWTHWEIGHT=

IL_LO_BIGLOGNC_PART_ROOT1_TARGET=DISK1
IL_LO_BIGLOGNC_PART_ROOT1_PART_NUM=5
IL_LO_BIGLOGNC_PART_ROOT1_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_ROOT1_PART_NUM}
IL_LO_BIGLOGNC_PART_ROOT1_LABEL=ROOT_1
IL_LO_BIGLOGNC_PART_ROOT1_MOUNT=/
IL_LO_BIGLOGNC_PART_ROOT1_BOOTABLE=
IL_LO_BIGLOGNC_PART_ROOT1_PART_ID=83
IL_LO_BIGLOGNC_PART_ROOT1_FSTYPE=ext3
IL_LO_BIGLOGNC_PART_ROOT1_SIZE=4000
IL_LO_BIGLOGNC_PART_ROOT1_GROWTHCAP=
IL_LO_BIGLOGNC_PART_ROOT1_GROWTHWEIGHT=
IL_LO_BIGLOGNC_PART_ROOT1_OPTIONS="defaults,noatime,ro"

IL_LO_BIGLOGNC_PART_ROOT2_TARGET=DISK1
IL_LO_BIGLOGNC_PART_ROOT2_PART_NUM=6
IL_LO_BIGLOGNC_PART_ROOT2_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_ROOT2_PART_NUM}
IL_LO_BIGLOGNC_PART_ROOT2_LABEL=ROOT_2
IL_LO_BIGLOGNC_PART_ROOT2_MOUNT=/
IL_LO_BIGLOGNC_PART_ROOT2_BOOTABLE=
IL_LO_BIGLOGNC_PART_ROOT2_PART_ID=83
IL_LO_BIGLOGNC_PART_ROOT2_FSTYPE=ext3
IL_LO_BIGLOGNC_PART_ROOT2_SIZE=4000
IL_LO_BIGLOGNC_PART_ROOT2_GROWTHCAP=
IL_LO_BIGLOGNC_PART_ROOT2_GROWTHWEIGHT=
IL_LO_BIGLOGNC_PART_ROOT2_OPTIONS="defaults,noatime,ro"

IL_LO_BIGLOGNC_PART_CONFIG_TARGET=DISK1
IL_LO_BIGLOGNC_PART_CONFIG_PART_NUM=7
IL_LO_BIGLOGNC_PART_CONFIG_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_CONFIG_PART_NUM}
IL_LO_BIGLOGNC_PART_CONFIG_LABEL=CONFIG
IL_LO_BIGLOGNC_PART_CONFIG_MOUNT=/config
IL_LO_BIGLOGNC_PART_CONFIG_BOOTABLE=
IL_LO_BIGLOGNC_PART_CONFIG_PART_ID=83
IL_LO_BIGLOGNC_PART_CONFIG_FSTYPE=ext3
IL_LO_BIGLOGNC_PART_CONFIG_SIZE=256
IL_LO_BIGLOGNC_PART_CONFIG_GROWTHCAP=
IL_LO_BIGLOGNC_PART_CONFIG_GROWTHWEIGHT=

IL_LO_BIGLOGNC_PART_VAR_TARGET=DISK1
IL_LO_BIGLOGNC_PART_VAR_PART_NUM=8
IL_LO_BIGLOGNC_PART_VAR_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_VAR_PART_NUM}
IL_LO_BIGLOGNC_PART_VAR_LABEL=VAR
IL_LO_BIGLOGNC_PART_VAR_MOUNT=/var
IL_LO_BIGLOGNC_PART_VAR_BOOTABLE=
IL_LO_BIGLOGNC_PART_VAR_PART_ID=83
IL_LO_BIGLOGNC_PART_VAR_FSTYPE=ext3
IL_LO_BIGLOGNC_PART_VAR_SIZE=3072
IL_LO_BIGLOGNC_PART_VAR_GROWTHCAP=
IL_LO_BIGLOGNC_PART_VAR_GROWTHWEIGHT=

IL_LO_BIGLOGNC_PART_COREDUMP_TARGET=DISK1
IL_LO_BIGLOGNC_PART_COREDUMP_PART_NUM=9
IL_LO_BIGLOGNC_PART_COREDUMP_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_COREDUMP_PART_NUM}
IL_LO_BIGLOGNC_PART_COREDUMP_LABEL=COREDUMP
IL_LO_BIGLOGNC_PART_COREDUMP_MOUNT=/coredump
IL_LO_BIGLOGNC_PART_COREDUMP_BOOTABLE=
IL_LO_BIGLOGNC_PART_COREDUMP_PART_ID=83
IL_LO_BIGLOGNC_PART_COREDUMP_FSTYPE=ext3
IL_LO_BIGLOGNC_PART_COREDUMP_SIZE=${COREDUMP_SIZE}
IL_LO_BIGLOGNC_PART_COREDUMP_GROWTHCAP=
IL_LO_BIGLOGNC_PART_COREDUMP_GROWTHWEIGHT=
IL_LO_BIGLOGNC_PART_COREDUMP_OPTIONS="defaults,noatime"

IL_LO_BIGLOGNC_PART_NKN_TARGET=DISK1
IL_LO_BIGLOGNC_PART_NKN_PART_NUM=10
IL_LO_BIGLOGNC_PART_NKN_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_NKN_PART_NUM}
IL_LO_BIGLOGNC_PART_NKN_LABEL=NKN
IL_LO_BIGLOGNC_PART_NKN_MOUNT=/nkn
IL_LO_BIGLOGNC_PART_NKN_BOOTABLE=
IL_LO_BIGLOGNC_PART_NKN_PART_ID=83
IL_LO_BIGLOGNC_PART_NKN_FSTYPE=ext3
IL_LO_BIGLOGNC_PART_NKN_SIZE=21000
IL_LO_BIGLOGNC_PART_NKN_GROWTHCAP=102400
IL_LO_BIGLOGNC_PART_NKN_GROWTHWEIGHT=50
IL_LO_BIGLOGNC_PART_NKN_OPTIONS="defaults,noatime"

IL_LO_BIGLOGNC_PART_LOG_TARGET=DISK1
IL_LO_BIGLOGNC_PART_LOG_PART_NUM=11
IL_LO_BIGLOGNC_PART_LOG_DEV=${IL_LO_BIGLOGNC_TARGET_DISK1_DEV}${IL_LO_BIGLOGNC_PART_LOG_PART_NUM}
IL_LO_BIGLOGNC_PART_LOG_LABEL=LOG
IL_LO_BIGLOGNC_PART_LOG_MOUNT=/log
IL_LO_BIGLOGNC_PART_LOG_BOOTABLE=
IL_LO_BIGLOGNC_PART_LOG_PART_ID=83
IL_LO_BIGLOGNC_PART_LOG_FSTYPE=ext3
IL_LO_BIGLOGNC_PART_LOG_SIZE=9000
IL_LO_BIGLOGNC_PART_LOG_GROWTHCAP=
IL_LO_BIGLOGNC_PART_LOG_GROWTHWEIGHT=50
IL_LO_BIGLOGNC_PART_LOG_OPTIONS="defaults,noatime"


# ###########################################################################
# ###########################################################################
# ==========================================================
##MIRROR: No cache on the root drive. /nkn 50%, /log 50%
## Partition type "fd" which is for raid autodetect.
##MIRROR: Limit /nkn to 102,400MB
# Partitions :
#  1 : /bootmgr
#  2 : /boot (1 of 2)
#  3 : /boot (2 of 2) - 2nd boot image
#  4 : extended partition
#  5 : / (1 of 2)
#  6 : / (2 of 2) - for 2nd boot image 
#  7 : /config
#  8 : /var
#  9 : /coredump
# 10 : /nkn
# 11 : /spare
# 12 : /log
# ==========================================================

##Model: mirror -> MIRROR
IL_LO_MIRROR_LOCS="BOOTMGR_1 BOOT_1 BOOT_2 ROOT_1 ROOT_2 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 SPARE_1 LOG_1"

# Targets are things like a disk or a flash device
IL_LO_MIRROR_TARGET_NUM_TARGETS=1
IL_LO_MIRROR_TARGETS="DISK1"
IL_LO_MIRROR_TARGET_DISK1_NUM=1
IL_LO_MIRROR_ROOT_RDONLY=1
IL_LO_MIRROR_INITRD_PATH=
IL_LO_MIRROR_ROOT_MOUNT_BY_LABEL=0
IL_LO_MIRROR_TARGET_DISK1_MINSIZE=${MIN_SIZE_MIRROR}
IL_LO_MIRROR_TARGET_DISK1_LABELTYPE=

# What partitions get made on the targets
IL_LO_MIRROR_PARTS="BOOTMGR BOOT1 BOOT2 EXT ROOT1 ROOT2 CONFIG VAR COREDUMP NKN SPARE LOG"

IL_LO_MIRROR_IMAGE_1_INSTALL_LOCS="BOOT_1 ROOT_1"
IL_LO_MIRROR_IMAGE_2_INSTALL_LOCS="BOOT_2 ROOT_2"
IL_LO_MIRROR_IMAGE_1_LOCS="ROOT_1 BOOT_1 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"
IL_LO_MIRROR_IMAGE_2_LOCS="ROOT_2 BOOT_2 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

IL_LO_MIRROR_OPT_LAST_PART_FILL=1
IL_LO_MIRROR_OPT_ALL_PARTS_FILL=1

IL_LO_MIRROR_EXTRA_KERNEL_PARAMS="crashkernel=auto"

# Each of these says which PART (partition) below goes with which LOC
#     (standardized location) .  It also says for each LOC which subdirectory
#     under the PART the LOC is in, and what files to extract there

IL_LO_MIRROR_LOC_BOOTMGR_1_PART=BOOTMGR
IL_LO_MIRROR_LOC_BOOTMGR_1_DIR=/
IL_LO_MIRROR_LOC_BOOTMGR_1_IMAGE_EXTRACT="./bootmgr"
IL_LO_MIRROR_LOC_BOOTMGR_1_IMAGE_EXTRACT_PREFIX="/bootmgr"

IL_LO_MIRROR_LOC_BOOT_1_PART=BOOT1
IL_LO_MIRROR_LOC_BOOT_1_DIR=/
IL_LO_MIRROR_LOC_BOOT_1_IMAGE_EXTRACT="./boot"
IL_LO_MIRROR_LOC_BOOT_1_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_MIRROR_LOC_BOOT_2_PART=BOOT2
IL_LO_MIRROR_LOC_BOOT_2_DIR=/
IL_LO_MIRROR_LOC_BOOT_2_IMAGE_EXTRACT="./boot"
IL_LO_MIRROR_LOC_BOOT_2_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_MIRROR_LOC_ROOT_1_PART=ROOT1
IL_LO_MIRROR_LOC_ROOT_1_DIR=/
IL_LO_MIRROR_LOC_ROOT_1_IMAGE_EXTRACT="./"
IL_LO_MIRROR_LOC_ROOT_1_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_MIRROR_LOC_ROOT_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_MIRROR_LOC_ROOT_2_PART=ROOT2
IL_LO_MIRROR_LOC_ROOT_2_DIR=/
IL_LO_MIRROR_LOC_ROOT_2_IMAGE_EXTRACT="./"
IL_LO_MIRROR_LOC_ROOT_2_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_MIRROR_LOC_ROOT_2_IMAGE_EXTRACT_PREFIX=""

IL_LO_MIRROR_LOC_CONFIG_1_PART=CONFIG
IL_LO_MIRROR_LOC_CONFIG_1_DIR=/
IL_LO_MIRROR_LOC_CONFIG_1_IMAGE_EXTRACT="./config"
IL_LO_MIRROR_LOC_CONFIG_1_IMAGE_EXTRACT_PREFIX="/config"

IL_LO_MIRROR_LOC_VAR_1_PART=VAR
IL_LO_MIRROR_LOC_VAR_1_DIR=/
IL_LO_MIRROR_LOC_VAR_1_IMAGE_EXTRACT="./var"
IL_LO_MIRROR_LOC_VAR_1_IMAGE_EXTRACT_PREFIX="/var"

IL_LO_MIRROR_LOC_COREDUMP_1_PART=COREDUMP
IL_LO_MIRROR_LOC_COREDUMP_1_DIR=/
IL_LO_MIRROR_LOC_COREDUMP_1_IMAGE_EXTRACT="./coredump"
IL_LO_MIRROR_LOC_COREDUMP_1_IMAGE_EXTRACT_PREFIX="/coredump"

IL_LO_MIRROR_LOC_NKN_1_PART=NKN
IL_LO_MIRROR_LOC_NKN_1_DIR=/
IL_LO_MIRROR_LOC_NKN_1_IMAGE_EXTRACT="./nkn"
IL_LO_MIRROR_LOC_NKN_1_IMAGE_EXTRACT_PREFIX="/nkn"

IL_LO_MIRROR_LOC_SPARE_1_PART=SPARE
IL_LO_MIRROR_LOC_SPARE_1_DIR=/
IL_LO_MIRROR_LOC_SPARE_1_IMAGE_EXTRACT=""
IL_LO_MIRROR_LOC_SPARE_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_MIRROR_LOC_LOG_1_PART=LOG
IL_LO_MIRROR_LOC_LOG_1_DIR=/
IL_LO_MIRROR_LOC_LOG_1_IMAGE_EXTRACT="./log"
IL_LO_MIRROR_LOC_LOG_1_IMAGE_EXTRACT_PREFIX="/log"

# Each of these says how a partition is named, labled, and sized.

IL_LO_MIRROR_PART_BOOTMGR_TARGET=DISK1
IL_LO_MIRROR_PART_BOOTMGR_PART_NUM=1
IL_LO_MIRROR_PART_BOOTMGR_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_BOOTMGR_PART_NUM}
IL_LO_MIRROR_PART_BOOTMGR_LABEL=BOOTMGR
IL_LO_MIRROR_PART_BOOTMGR_MOUNT=/bootmgr
IL_LO_MIRROR_PART_BOOTMGR_BOOTABLE=1
IL_LO_MIRROR_PART_BOOTMGR_PART_ID=fd
IL_LO_MIRROR_PART_BOOTMGR_FSTYPE=ext3
IL_LO_MIRROR_PART_BOOTMGR_SIZE=128
IL_LO_MIRROR_PART_BOOTMGR_GROWTHCAP=
IL_LO_MIRROR_PART_BOOTMGR_GROWTHWEIGHT=
IL_LO_MIRROR_PART_BOOTMGR_OPTIONS="defaults,noatime,ro"

IL_LO_MIRROR_PART_BOOT1_TARGET=DISK1
IL_LO_MIRROR_PART_BOOT1_PART_NUM=2
IL_LO_MIRROR_PART_BOOT1_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_BOOT1_PART_NUM}
IL_LO_MIRROR_PART_BOOT1_LABEL=BOOT_1
IL_LO_MIRROR_PART_BOOT1_MOUNT=/boot
IL_LO_MIRROR_PART_BOOT1_BOOTABLE=
IL_LO_MIRROR_PART_BOOT1_PART_ID=fd
IL_LO_MIRROR_PART_BOOT1_FSTYPE=ext3
IL_LO_MIRROR_PART_BOOT1_SIZE=128
IL_LO_MIRROR_PART_BOOT1_GROWTHCAP=
IL_LO_MIRROR_PART_BOOT1_GROWTHWEIGHT=
IL_LO_MIRROR_PART_BOOT1_OPTIONS="defaults,noatime,ro"

IL_LO_MIRROR_PART_BOOT2_TARGET=DISK1
IL_LO_MIRROR_PART_BOOT2_PART_NUM=3
IL_LO_MIRROR_PART_BOOT2_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_BOOT2_PART_NUM}
IL_LO_MIRROR_PART_BOOT2_LABEL=BOOT_2
IL_LO_MIRROR_PART_BOOT2_MOUNT=/boot
IL_LO_MIRROR_PART_BOOT2_BOOTABLE=
IL_LO_MIRROR_PART_BOOT2_PART_ID=fd
IL_LO_MIRROR_PART_BOOT2_FSTYPE=ext3
IL_LO_MIRROR_PART_BOOT2_SIZE=128
IL_LO_MIRROR_PART_BOOT2_GROWTHCAP=
IL_LO_MIRROR_PART_BOOT2_GROWTHWEIGHT=
IL_LO_MIRROR_PART_BOOT2_OPTIONS="defaults,noatime,ro"

IL_LO_MIRROR_PART_EXT_TARGET=DISK1
IL_LO_MIRROR_PART_EXT_PART_NUM=4
IL_LO_MIRROR_PART_EXT_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_EXT_PART_NUM}
IL_LO_MIRROR_PART_EXT_LABEL=
IL_LO_MIRROR_PART_EXT_MOUNT=
IL_LO_MIRROR_PART_EXT_BOOTABLE=
IL_LO_MIRROR_PART_EXT_PART_ID=0f
IL_LO_MIRROR_PART_EXT_FSTYPE=
IL_LO_MIRROR_PART_EXT_SIZE=
IL_LO_MIRROR_PART_EXT_GROWTHCAP=
IL_LO_MIRROR_PART_EXT_GROWTHWEIGHT=

IL_LO_MIRROR_PART_ROOT1_TARGET=DISK1
IL_LO_MIRROR_PART_ROOT1_PART_NUM=5
IL_LO_MIRROR_PART_ROOT1_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_ROOT1_PART_NUM}
IL_LO_MIRROR_PART_ROOT1_LABEL=ROOT_1
IL_LO_MIRROR_PART_ROOT1_MOUNT=/
IL_LO_MIRROR_PART_ROOT1_BOOTABLE=
IL_LO_MIRROR_PART_ROOT1_PART_ID=fd
IL_LO_MIRROR_PART_ROOT1_FSTYPE=ext3
IL_LO_MIRROR_PART_ROOT1_SIZE=4000
IL_LO_MIRROR_PART_ROOT1_GROWTHCAP=
IL_LO_MIRROR_PART_ROOT1_GROWTHWEIGHT=
IL_LO_MIRROR_PART_ROOT1_OPTIONS="defaults,noatime,ro"

IL_LO_MIRROR_PART_ROOT2_TARGET=DISK1
IL_LO_MIRROR_PART_ROOT2_PART_NUM=6
IL_LO_MIRROR_PART_ROOT2_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_ROOT2_PART_NUM}
IL_LO_MIRROR_PART_ROOT2_LABEL=ROOT_2
IL_LO_MIRROR_PART_ROOT2_MOUNT=/
IL_LO_MIRROR_PART_ROOT2_BOOTABLE=
IL_LO_MIRROR_PART_ROOT2_PART_ID=fd
IL_LO_MIRROR_PART_ROOT2_FSTYPE=ext3
IL_LO_MIRROR_PART_ROOT2_SIZE=4000
IL_LO_MIRROR_PART_ROOT2_GROWTHCAP=
IL_LO_MIRROR_PART_ROOT2_GROWTHWEIGHT=
IL_LO_MIRROR_PART_ROOT2_OPTIONS="defaults,noatime,ro"

IL_LO_MIRROR_PART_CONFIG_TARGET=DISK1
IL_LO_MIRROR_PART_CONFIG_PART_NUM=7
IL_LO_MIRROR_PART_CONFIG_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_CONFIG_PART_NUM}
IL_LO_MIRROR_PART_CONFIG_LABEL=CONFIG
IL_LO_MIRROR_PART_CONFIG_MOUNT=/config
IL_LO_MIRROR_PART_CONFIG_BOOTABLE=
IL_LO_MIRROR_PART_CONFIG_PART_ID=fd
IL_LO_MIRROR_PART_CONFIG_FSTYPE=ext3
IL_LO_MIRROR_PART_CONFIG_SIZE=256
IL_LO_MIRROR_PART_CONFIG_GROWTHCAP=
IL_LO_MIRROR_PART_CONFIG_GROWTHWEIGHT=

IL_LO_MIRROR_PART_VAR_TARGET=DISK1
IL_LO_MIRROR_PART_VAR_PART_NUM=8
IL_LO_MIRROR_PART_VAR_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_VAR_PART_NUM}
IL_LO_MIRROR_PART_VAR_LABEL=VAR
IL_LO_MIRROR_PART_VAR_MOUNT=/var
IL_LO_MIRROR_PART_VAR_BOOTABLE=
IL_LO_MIRROR_PART_VAR_PART_ID=fd
IL_LO_MIRROR_PART_VAR_FSTYPE=ext3
IL_LO_MIRROR_PART_VAR_SIZE=3072
IL_LO_MIRROR_PART_VAR_GROWTHCAP=
IL_LO_MIRROR_PART_VAR_GROWTHWEIGHT=

IL_LO_MIRROR_PART_COREDUMP_TARGET=DISK1
IL_LO_MIRROR_PART_COREDUMP_PART_NUM=9
IL_LO_MIRROR_PART_COREDUMP_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_COREDUMP_PART_NUM}
IL_LO_MIRROR_PART_COREDUMP_LABEL=COREDUMP
IL_LO_MIRROR_PART_COREDUMP_MOUNT=/coredump
IL_LO_MIRROR_PART_COREDUMP_BOOTABLE=
IL_LO_MIRROR_PART_COREDUMP_PART_ID=fd
IL_LO_MIRROR_PART_COREDUMP_FSTYPE=ext3
IL_LO_MIRROR_PART_COREDUMP_SIZE=${COREDUMP_SIZE}
IL_LO_MIRROR_PART_COREDUMP_GROWTHCAP=
IL_LO_MIRROR_PART_COREDUMP_GROWTHWEIGHT=
IL_LO_MIRROR_PART_COREDUMP_OPTIONS="defaults,noatime"

IL_LO_MIRROR_PART_NKN_TARGET=DISK1
IL_LO_MIRROR_PART_NKN_PART_NUM=10
IL_LO_MIRROR_PART_NKN_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_NKN_PART_NUM}
IL_LO_MIRROR_PART_NKN_LABEL=NKN
IL_LO_MIRROR_PART_NKN_MOUNT=/nkn
IL_LO_MIRROR_PART_NKN_BOOTABLE=
IL_LO_MIRROR_PART_NKN_PART_ID=fd
IL_LO_MIRROR_PART_NKN_FSTYPE=ext3
IL_LO_MIRROR_PART_NKN_SIZE=21000
IL_LO_MIRROR_PART_NKN_GROWTHCAP=102400
IL_LO_MIRROR_PART_NKN_GROWTHWEIGHT=50
IL_LO_MIRROR_PART_NKN_OPTIONS="defaults,noatime"

IL_LO_MIRROR_PART_SPARE_TARGET=DISK1
IL_LO_MIRROR_PART_SPARE_PART_NUM=11
IL_LO_MIRROR_PART_SPARE_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_SPARE_PART_NUM}
IL_LO_MIRROR_PART_SPARE_LABEL=SPARE
IL_LO_MIRROR_PART_SPARE_MOUNT=
IL_LO_MIRROR_PART_SPARE_BOOTABLE=
IL_LO_MIRROR_PART_SPARE_PART_ID=
IL_LO_MIRROR_PART_SPARE_FSTYPE=
IL_LO_MIRROR_PART_SPARE_SIZE=10000
IL_LO_MIRROR_PART_SPARE_GROWTHCAP=
IL_LO_MIRROR_PART_SPARE_GROWTHWEIGHT=

IL_LO_MIRROR_PART_LOG_TARGET=DISK1
IL_LO_MIRROR_PART_LOG_PART_NUM=12
IL_LO_MIRROR_PART_LOG_DEV=${IL_LO_MIRROR_TARGET_DISK1_DEV}${IL_LO_MIRROR_PART_LOG_PART_NUM}
IL_LO_MIRROR_PART_LOG_LABEL=LOG
IL_LO_MIRROR_PART_LOG_MOUNT=/log
IL_LO_MIRROR_PART_LOG_BOOTABLE=
IL_LO_MIRROR_PART_LOG_PART_ID=fd
IL_LO_MIRROR_PART_LOG_FSTYPE=ext3
IL_LO_MIRROR_PART_LOG_SIZE=9000
IL_LO_MIRROR_PART_LOG_GROWTHCAP=
IL_LO_MIRROR_PART_LOG_GROWTHWEIGHT=50
IL_LO_MIRROR_PART_LOG_OPTIONS="defaults,noatime"


# ###########################################################################
# ###########################################################################
# ==========================================================
##FLASHROOT: 4 to 16GB root drive.  Cache on second drive.
##FLASHROOT: Similar to DEFAULT, except for VXA machines.
# Root disk minumum disk size: 4GB
# Root flash-disk Partitions :
#  1 : /bootmgr
#  2 : /boot (1 of 2)
#  3 : /boot (2 of 2) - 2nd boot image
#  4 : extended partition
#  5 : / (1 of 2)
#  6 : / (2 of 2) - for 2nd boot image
#  7 : /config
#  8 : /var
# Second disk minumum disk size: 130GB
# Second disk partitions :
#  1 : /coredump
#  2 : /nkn
#  3 : /log
#  4 : extended partition
#  5 : dmfs
#  6 : dmraw
# ==========================================================

##Model: vxa1 -> FLASHROOT
IL_LO_FLASHROOT_LOCS="BOOTMGR_1 BOOT_1 BOOT_2 ROOT_1 ROOT_2 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1 DMFS_1 DMRAW_1"

# Targets are things like a disk or a flash device
IL_LO_FLASHROOT_TARGET_NUM_TARGETS=2
IL_LO_FLASHROOT_TARGETS="DISK1 DISK2"
IL_LO_FLASHROOT_TARGET_DISK1_NUM=1
IL_LO_FLASHROOT_TARGET_DISK2_NUM=2

IL_LO_FLASHROOT_ROOT_RDONLY=1

IL_LO_FLASHROOT_INITRD_PATH="initrd.img"
IL_LO_FLASHROOT_ROOT_MOUNT_BY_LABEL=1

# Actual size of "4G" eUSB size is 3920 MB.
IL_LO_FLASHROOT_TARGET_DISK1_MINSIZE=3920
IL_LO_FLASHROOT_TARGET_DISK1_LABELTYPE=
IL_LO_FLASHROOT_TARGET_DISK2_MINSIZE=${MIN_SIZE_FLASHROOT}
IL_LO_FLASHROOT_TARGET_DISK2_LABELTYPE=

# What partitions get made on the targets
IL_LO_FLASHROOT_PARTS="BOOTMGR BOOT1 BOOT2 EXT ROOT1 ROOT2 CONFIG VAR COREDUMP NKN LOG EXT2 DMFS DMRAW"

IL_LO_FLASHROOT_IMAGE_1_INSTALL_LOCS="BOOT_1 ROOT_1"
IL_LO_FLASHROOT_IMAGE_2_INSTALL_LOCS="BOOT_2 ROOT_2"
IL_LO_FLASHROOT_IMAGE_1_LOCS="ROOT_1 BOOT_1 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"
IL_LO_FLASHROOT_IMAGE_2_LOCS="ROOT_2 BOOT_2 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

IL_LO_FLASHROOT_OPT_LAST_PART_FILL=1
IL_LO_FLASHROOT_OPT_ALL_PARTS_FILL=1

# The EXTRA_KERNEL_PARAMS are for the grub command line.
# The kernel parameter rootdelay is set to handle the eUSB case.
IL_LO_FLASHROOT_EXTRA_KERNEL_PARAMS="rootdelay=5 crashkernel=auto"

# Each of these says which PART (partition) below goes with which LOC
#     (standardized location) .  It also says for each LOC which subdirectory
#     under the PART the LOC is in, and what files to extract there

IL_LO_FLASHROOT_LOC_BOOTMGR_1_PART=BOOTMGR
IL_LO_FLASHROOT_LOC_BOOTMGR_1_DIR=/
IL_LO_FLASHROOT_LOC_BOOTMGR_1_IMAGE_EXTRACT="./bootmgr"
IL_LO_FLASHROOT_LOC_BOOTMGR_1_IMAGE_EXTRACT_PREFIX="/bootmgr"

IL_LO_FLASHROOT_LOC_BOOT_1_PART=BOOT1
IL_LO_FLASHROOT_LOC_BOOT_1_DIR=/
IL_LO_FLASHROOT_LOC_BOOT_1_IMAGE_EXTRACT="./boot"
IL_LO_FLASHROOT_LOC_BOOT_1_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_FLASHROOT_LOC_BOOT_2_PART=BOOT2
IL_LO_FLASHROOT_LOC_BOOT_2_DIR=/
IL_LO_FLASHROOT_LOC_BOOT_2_IMAGE_EXTRACT="./boot"
IL_LO_FLASHROOT_LOC_BOOT_2_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_FLASHROOT_LOC_ROOT_1_PART=ROOT1
IL_LO_FLASHROOT_LOC_ROOT_1_DIR=/
IL_LO_FLASHROOT_LOC_ROOT_1_IMAGE_EXTRACT="./"
IL_LO_FLASHROOT_LOC_ROOT_1_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_FLASHROOT_LOC_ROOT_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_FLASHROOT_LOC_ROOT_2_PART=ROOT2
IL_LO_FLASHROOT_LOC_ROOT_2_DIR=/
IL_LO_FLASHROOT_LOC_ROOT_2_IMAGE_EXTRACT="./"
IL_LO_FLASHROOT_LOC_ROOT_2_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_FLASHROOT_LOC_ROOT_2_IMAGE_EXTRACT_PREFIX=""

IL_LO_FLASHROOT_LOC_CONFIG_1_PART=CONFIG
IL_LO_FLASHROOT_LOC_CONFIG_1_DIR=/
IL_LO_FLASHROOT_LOC_CONFIG_1_IMAGE_EXTRACT="./config"
IL_LO_FLASHROOT_LOC_CONFIG_1_IMAGE_EXTRACT_PREFIX="/config"

IL_LO_FLASHROOT_LOC_VAR_1_PART=VAR
IL_LO_FLASHROOT_LOC_VAR_1_DIR=/
IL_LO_FLASHROOT_LOC_VAR_1_IMAGE_EXTRACT="./var"
IL_LO_FLASHROOT_LOC_VAR_1_IMAGE_EXTRACT_PREFIX="/var"

IL_LO_FLASHROOT_LOC_COREDUMP_1_PART=COREDUMP
IL_LO_FLASHROOT_LOC_COREDUMP_1_DIR=/
IL_LO_FLASHROOT_LOC_COREDUMP_1_IMAGE_EXTRACT="./coredump"
IL_LO_FLASHROOT_LOC_COREDUMP_1_IMAGE_EXTRACT_PREFIX="/coredump"

IL_LO_FLASHROOT_LOC_NKN_1_PART=NKN
IL_LO_FLASHROOT_LOC_NKN_1_DIR=/
IL_LO_FLASHROOT_LOC_NKN_1_IMAGE_EXTRACT="./nkn"
IL_LO_FLASHROOT_LOC_NKN_1_IMAGE_EXTRACT_PREFIX="/nkn"

IL_LO_FLASHROOT_LOC_LOG_1_PART=LOG
IL_LO_FLASHROOT_LOC_LOG_1_DIR=/
IL_LO_FLASHROOT_LOC_LOG_1_IMAGE_EXTRACT="./log"
IL_LO_FLASHROOT_LOC_LOG_1_IMAGE_EXTRACT_PREFIX="/log"

IL_LO_FLASHROOT_LOC_DMFS_1_PART=DMFS
IL_LO_FLASHROOT_LOC_DMFS_1_DIR=/
IL_LO_FLASHROOT_LOC_DMFS_1_IMAGE_EXTRACT=""
IL_LO_FLASHROOT_LOC_DMFS_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_FLASHROOT_LOC_DMRAW_1_PART=DMRAW
IL_LO_FLASHROOT_LOC_DMRAW_1_DIR=/
IL_LO_FLASHROOT_LOC_DMRAW_1_IMAGE_EXTRACT=""
IL_LO_FLASHROOT_LOC_DMRAW_1_IMAGE_EXTRACT_PREFIX=""


# Each of these says how a partition is named, labled, and sized.

IL_LO_FLASHROOT_PART_BOOTMGR_TARGET=DISK1
IL_LO_FLASHROOT_PART_BOOTMGR_PART_NUM=1
IL_LO_FLASHROOT_PART_BOOTMGR_DEV=${IL_LO_FLASHROOT_TARGET_DISK1_DEV}${IL_LO_FLASHROOT_PART_BOOTMGR_PART_NUM}
IL_LO_FLASHROOT_PART_BOOTMGR_LABEL=BOOTMGR
IL_LO_FLASHROOT_PART_BOOTMGR_MOUNT=/bootmgr
IL_LO_FLASHROOT_PART_BOOTMGR_BOOTABLE=1
IL_LO_FLASHROOT_PART_BOOTMGR_PART_ID=83
IL_LO_FLASHROOT_PART_BOOTMGR_FSTYPE=ext3
IL_LO_FLASHROOT_PART_BOOTMGR_SIZE=30
IL_LO_FLASHROOT_PART_BOOTMGR_GROWTHCAP=
IL_LO_FLASHROOT_PART_BOOTMGR_GROWTHWEIGHT=
IL_LO_FLASHROOT_PART_BOOTMGR_OPTIONS="defaults,noatime,ro"

IL_LO_FLASHROOT_PART_BOOT1_TARGET=DISK1
IL_LO_FLASHROOT_PART_BOOT1_PART_NUM=2
IL_LO_FLASHROOT_PART_BOOT1_DEV=${IL_LO_FLASHROOT_TARGET_DISK1_DEV}${IL_LO_FLASHROOT_PART_BOOT1_PART_NUM}
IL_LO_FLASHROOT_PART_BOOT1_LABEL=BOOT_1
IL_LO_FLASHROOT_PART_BOOT1_MOUNT=/boot
IL_LO_FLASHROOT_PART_BOOT1_BOOTABLE=
IL_LO_FLASHROOT_PART_BOOT1_PART_ID=83
IL_LO_FLASHROOT_PART_BOOT1_FSTYPE=ext3
IL_LO_FLASHROOT_PART_BOOT1_SIZE=30
IL_LO_FLASHROOT_PART_BOOT1_GROWTHCAP=
IL_LO_FLASHROOT_PART_BOOT1_GROWTHWEIGHT=
IL_LO_FLASHROOT_PART_BOOT1_OPTIONS="defaults,noatime,ro"

IL_LO_FLASHROOT_PART_BOOT2_TARGET=DISK1
IL_LO_FLASHROOT_PART_BOOT2_PART_NUM=3
IL_LO_FLASHROOT_PART_BOOT2_DEV=${IL_LO_FLASHROOT_TARGET_DISK1_DEV}${IL_LO_FLASHROOT_PART_BOOT2_PART_NUM}
IL_LO_FLASHROOT_PART_BOOT2_LABEL=BOOT_2
IL_LO_FLASHROOT_PART_BOOT2_MOUNT=/boot
IL_LO_FLASHROOT_PART_BOOT2_BOOTABLE=
IL_LO_FLASHROOT_PART_BOOT2_PART_ID=83
IL_LO_FLASHROOT_PART_BOOT2_FSTYPE=ext3
IL_LO_FLASHROOT_PART_BOOT2_SIZE=30
IL_LO_FLASHROOT_PART_BOOT2_GROWTHCAP=
IL_LO_FLASHROOT_PART_BOOT2_GROWTHWEIGHT=
IL_LO_FLASHROOT_PART_BOOT2_OPTIONS="defaults,noatime,ro"

IL_LO_FLASHROOT_PART_EXT_TARGET=DISK1
IL_LO_FLASHROOT_PART_EXT_PART_NUM=4
IL_LO_FLASHROOT_PART_EXT_DEV=${IL_LO_FLASHROOT_TARGET_DISK1_DEV}${IL_LO_FLASHROOT_PART_EXT_PART_NUM}
IL_LO_FLASHROOT_PART_EXT_LABEL=
IL_LO_FLASHROOT_PART_EXT_MOUNT=
IL_LO_FLASHROOT_PART_EXT_BOOTABLE=
IL_LO_FLASHROOT_PART_EXT_PART_ID=0f
IL_LO_FLASHROOT_PART_EXT_FSTYPE=
IL_LO_FLASHROOT_PART_EXT_SIZE=
IL_LO_FLASHROOT_PART_EXT_GROWTHCAP=
IL_LO_FLASHROOT_PART_EXT_GROWTHWEIGHT=

# On 4G drive, the size of each root partition will be 1433.
# On 8G drive, the size of each root partitioin will be around 3400?
# On 16G drive, the size of each root partition will be 4000.
IL_LO_FLASHROOT_PART_ROOT1_TARGET=DISK1
IL_LO_FLASHROOT_PART_ROOT1_PART_NUM=5
IL_LO_FLASHROOT_PART_ROOT1_DEV=${IL_LO_FLASHROOT_TARGET_DISK1_DEV}${IL_LO_FLASHROOT_PART_ROOT1_PART_NUM}
IL_LO_FLASHROOT_PART_ROOT1_LABEL=ROOT_1
IL_LO_FLASHROOT_PART_ROOT1_MOUNT=/
IL_LO_FLASHROOT_PART_ROOT1_BOOTABLE=
IL_LO_FLASHROOT_PART_ROOT1_PART_ID=83
IL_LO_FLASHROOT_PART_ROOT1_FSTYPE=ext3
IL_LO_FLASHROOT_PART_ROOT1_SIZE=1430
IL_LO_FLASHROOT_PART_ROOT1_GROWTHCAP=4000
IL_LO_FLASHROOT_PART_ROOT1_GROWTHWEIGHT=49
IL_LO_FLASHROOT_PART_ROOT1_OPTIONS="defaults,noatime,ro"

IL_LO_FLASHROOT_PART_ROOT2_TARGET=DISK1
IL_LO_FLASHROOT_PART_ROOT2_PART_NUM=6
IL_LO_FLASHROOT_PART_ROOT2_DEV=${IL_LO_FLASHROOT_TARGET_DISK1_DEV}${IL_LO_FLASHROOT_PART_ROOT2_PART_NUM}
IL_LO_FLASHROOT_PART_ROOT2_LABEL=ROOT_2
IL_LO_FLASHROOT_PART_ROOT2_MOUNT=/
IL_LO_FLASHROOT_PART_ROOT2_BOOTABLE=
IL_LO_FLASHROOT_PART_ROOT2_PART_ID=83
IL_LO_FLASHROOT_PART_ROOT2_FSTYPE=ext3
IL_LO_FLASHROOT_PART_ROOT2_SIZE=1430
IL_LO_FLASHROOT_PART_ROOT2_GROWTHCAP=4000
IL_LO_FLASHROOT_PART_ROOT2_GROWTHWEIGHT=49
IL_LO_FLASHROOT_PART_ROOT2_OPTIONS="defaults,noatime,ro"

IL_LO_FLASHROOT_PART_CONFIG_TARGET=DISK1
IL_LO_FLASHROOT_PART_CONFIG_PART_NUM=7
IL_LO_FLASHROOT_PART_CONFIG_DEV=${IL_LO_FLASHROOT_TARGET_DISK1_DEV}${IL_LO_FLASHROOT_PART_CONFIG_PART_NUM}
IL_LO_FLASHROOT_PART_CONFIG_LABEL=CONFIG
IL_LO_FLASHROOT_PART_CONFIG_MOUNT=/config
IL_LO_FLASHROOT_PART_CONFIG_BOOTABLE=
IL_LO_FLASHROOT_PART_CONFIG_PART_ID=83
IL_LO_FLASHROOT_PART_CONFIG_FSTYPE=ext3
IL_LO_FLASHROOT_PART_CONFIG_SIZE=64
IL_LO_FLASHROOT_PART_CONFIG_GROWTHCAP=
IL_LO_FLASHROOT_PART_CONFIG_GROWTHWEIGHT=
IL_LO_FLASHROOT_PART_CONFIG_OPTIONS="defaults,noatime"

# For disks larger than 8GB then /var gets the balance of the space
# after the root partitions max out at 4000 each.
IL_LO_FLASHROOT_PART_VAR_TARGET=DISK1
IL_LO_FLASHROOT_PART_VAR_PART_NUM=8
IL_LO_FLASHROOT_PART_VAR_DEV=${IL_LO_FLASHROOT_TARGET_DISK1_DEV}${IL_LO_FLASHROOT_PART_VAR_PART_NUM}
IL_LO_FLASHROOT_PART_VAR_LABEL=VAR
IL_LO_FLASHROOT_PART_VAR_MOUNT=/var
IL_LO_FLASHROOT_PART_VAR_BOOTABLE=
IL_LO_FLASHROOT_PART_VAR_PART_ID=83
IL_LO_FLASHROOT_PART_VAR_FSTYPE=ext3
IL_LO_FLASHROOT_PART_VAR_SIZE=900
IL_LO_FLASHROOT_PART_VAR_GROWTHCAP=
IL_LO_FLASHROOT_PART_VAR_GROWTHWEIGHT=2
IL_LO_FLASHROOT_PART_VAR_OPTIONS="defaults,noatime"

IL_LO_FLASHROOT_PART_COREDUMP_TARGET=DISK2
IL_LO_FLASHROOT_PART_COREDUMP_PART_NUM=1
IL_LO_FLASHROOT_PART_COREDUMP_DEV=${IL_LO_FLASHROOT_TARGET_DISK2_DEV}${IL_LO_FLASHROOT_PART_COREDUMP_PART_NUM}
IL_LO_FLASHROOT_PART_COREDUMP_LABEL=COREDUMP
IL_LO_FLASHROOT_PART_COREDUMP_MOUNT=/coredump
IL_LO_FLASHROOT_PART_COREDUMP_BOOTABLE=
IL_LO_FLASHROOT_PART_COREDUMP_PART_ID=83
IL_LO_FLASHROOT_PART_COREDUMP_FSTYPE=ext3
IL_LO_FLASHROOT_PART_COREDUMP_SIZE=${COREDUMP_SIZE}
IL_LO_FLASHROOT_PART_COREDUMP_GROWTHCAP=
IL_LO_FLASHROOT_PART_COREDUMP_GROWTHWEIGHT=
IL_LO_FLASHROOT_PART_COREDUMP_OPTIONS="defaults,noatime"

IL_LO_FLASHROOT_PART_NKN_TARGET=DISK2
IL_LO_FLASHROOT_PART_NKN_PART_NUM=2
IL_LO_FLASHROOT_PART_NKN_DEV=${IL_LO_FLASHROOT_TARGET_DISK2_DEV}${IL_LO_FLASHROOT_PART_NKN_PART_NUM}
IL_LO_FLASHROOT_PART_NKN_LABEL=NKN
IL_LO_FLASHROOT_PART_NKN_MOUNT=/nkn
IL_LO_FLASHROOT_PART_NKN_BOOTABLE=
IL_LO_FLASHROOT_PART_NKN_PART_ID=83
IL_LO_FLASHROOT_PART_NKN_FSTYPE=ext3
IL_LO_FLASHROOT_PART_NKN_SIZE=21000
IL_LO_FLASHROOT_PART_NKN_GROWTHCAP=102400
IL_LO_FLASHROOT_PART_NKN_GROWTHWEIGHT=33
IL_LO_FLASHROOT_PART_NKN_OPTIONS="defaults,noatime"

IL_LO_FLASHROOT_PART_LOG_TARGET=DISK2
IL_LO_FLASHROOT_PART_LOG_PART_NUM=3
IL_LO_FLASHROOT_PART_LOG_DEV=${IL_LO_FLASHROOT_TARGET_DISK2_DEV}${IL_LO_FLASHROOT_PART_LOG_PART_NUM}
IL_LO_FLASHROOT_PART_LOG_LABEL=LOG
IL_LO_FLASHROOT_PART_LOG_MOUNT=/log
IL_LO_FLASHROOT_PART_LOG_BOOTABLE=
IL_LO_FLASHROOT_PART_LOG_PART_ID=83
IL_LO_FLASHROOT_PART_LOG_FSTYPE=ext3
IL_LO_FLASHROOT_PART_LOG_SIZE=10000
IL_LO_FLASHROOT_PART_LOG_GROWTHCAP=102400
IL_LO_FLASHROOT_PART_LOG_GROWTHWEIGHT=33
IL_LO_FLASHROOT_PART_LOG_OPTIONS="defaults,noatime"

IL_LO_FLASHROOT_PART_EXT2_TARGET=DISK2
IL_LO_FLASHROOT_PART_EXT2_PART_NUM=4
IL_LO_FLASHROOT_PART_EXT2_DEV=${IL_LO_FLASHROOT_TARGET_DISK2_DEV}${IL_LO_FLASHROOT_PART_EXT2_PART_NUM}
IL_LO_FLASHROOT_PART_EXT2_LABEL=
IL_LO_FLASHROOT_PART_EXT2_MOUNT=
IL_LO_FLASHROOT_PART_EXT2_BOOTABLE=
IL_LO_FLASHROOT_PART_EXT2_PART_ID=0f
IL_LO_FLASHROOT_PART_EXT2_FSTYPE=
IL_LO_FLASHROOT_PART_EXT2_SIZE=
IL_LO_FLASHROOT_PART_EXT2_GROWTHCAP=
IL_LO_FLASHROOT_PART_EXT2_GROWTHWEIGHT=

IL_LO_FLASHROOT_PART_DMFS_TARGET=DISK2
IL_LO_FLASHROOT_PART_DMFS_PART_NUM=5
IL_LO_FLASHROOT_PART_DMFS_DEV=${IL_LO_FLASHROOT_TARGET_DISK2_DEV}${IL_LO_FLASHROOT_PART_DMFS_PART_NUM}
IL_LO_FLASHROOT_PART_DMFS_LABEL=DMFS
IL_LO_FLASHROOT_PART_DMFS_MOUNT=
IL_LO_FLASHROOT_PART_DMFS_BOOTABLE=
IL_LO_FLASHROOT_PART_DMFS_PART_ID=83
  # For the dmfs parition set the FSTYPE to blank, so it will not create a new
  # filesystem on the partition, so we can preserve the contents.
  # Our install-mfd will do all the needed making of filesystems and labels,
  # because it knows when we want to preserve the data or not.
IL_LO_FLASHROOT_PART_DMFS_FSTYPE=
IL_LO_FLASHROOT_PART_DMFS_SIZE=900
IL_LO_FLASHROOT_PART_DMFS_GROWTHCAP=
# We want this to be about 10% the size of DMRAW
IL_LO_FLASHROOT_PART_DMFS_GROWTHWEIGHT=3

IL_LO_FLASHROOT_PART_DMRAW_TARGET=DISK2
IL_LO_FLASHROOT_PART_DMRAW_PART_NUM=6
IL_LO_FLASHROOT_PART_DMRAW_DEV=${IL_LO_FLASHROOT_TARGET_DISK2_DEV}${IL_LO_FLASHROOT_PART_DMRAW_PART_NUM}
IL_LO_FLASHROOT_PART_DMRAW_LABEL=DMRAW
IL_LO_FLASHROOT_PART_DMRAW_MOUNT=
IL_LO_FLASHROOT_PART_DMRAW_BOOTABLE=
IL_LO_FLASHROOT_PART_DMRAW_PART_ID=
IL_LO_FLASHROOT_PART_DMRAW_FSTYPE=
IL_LO_FLASHROOT_PART_DMRAW_SIZE=9000
IL_LO_FLASHROOT_PART_DMRAW_GROWTHCAP=
IL_LO_FLASHROOT_PART_DMRAW_GROWTHWEIGHT=33


# ###########################################################################
# ###########################################################################
# ==========================================================
##FLASHNCACHE: 4 to 16GB root drive. Second drive: NO cache, /nkn 50%, /log 50%
##FLASHNCACHE: Limit /nkn to 102,400MB
##FLASHNCACHE: Similar to BIGLOGNC, but for VXA machines.
# Root disk minumum disk size: 4GB
# Root flash-disk Partitions :
#  1 : /bootmgr
#  2 : /boot (1 of 2)
#  3 : /boot (2 of 2) - 2nd boot image
#  4 : extended partition
#  5 : / (1 of 2)
#  6 : / (2 of 2) - for 2nd boot image
#  7 : /config
#  8 : /var
# Second disk minumum disk size: 120GB
# Second disk partitions :
#  1 : /coredump
#  2 : /nkn
#  3 : /log
# ==========================================================

##Model: vxa2 -> FLASHNCACHE
IL_LO_FLASHNCACHE_LOCS="BOOTMGR_1 BOOT_1 BOOT_2 ROOT_1 ROOT_2 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

# Targets are things like a disk or a flash device
IL_LO_FLASHNCACHE_TARGET_NUM_TARGETS=2
IL_LO_FLASHNCACHE_TARGETS="DISK1 DISK2"
IL_LO_FLASHNCACHE_TARGET_DISK1_NUM=1
IL_LO_FLASHNCACHE_TARGET_DISK2_NUM=2

IL_LO_FLASHNCACHE_ROOT_RDONLY=1

IL_LO_FLASHNCACHE_INITRD_PATH="initrd.img"
IL_LO_FLASHNCACHE_ROOT_MOUNT_BY_LABEL=1

# Actual size of "4G" eUSB size is 3920 MB.
IL_LO_FLASHNCACHE_TARGET_DISK1_MINSIZE=3920
IL_LO_FLASHNCACHE_TARGET_DISK1_LABELTYPE=
IL_LO_FLASHNCACHE_TARGET_DISK2_MINSIZE=${MIN_SIZE_FLASHNCACHE}
IL_LO_FLASHNCACHE_TARGET_DISK2_LABELTYPE=

# What partitions get made on the targets
IL_LO_FLASHNCACHE_PARTS="BOOTMGR BOOT1 BOOT2 EXT ROOT1 ROOT2 CONFIG VAR COREDUMP NKN LOG EXT2"

IL_LO_FLASHNCACHE_IMAGE_1_INSTALL_LOCS="BOOT_1 ROOT_1"
IL_LO_FLASHNCACHE_IMAGE_2_INSTALL_LOCS="BOOT_2 ROOT_2"
IL_LO_FLASHNCACHE_IMAGE_1_LOCS="ROOT_1 BOOT_1 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"
IL_LO_FLASHNCACHE_IMAGE_2_LOCS="ROOT_2 BOOT_2 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

IL_LO_FLASHNCACHE_OPT_LAST_PART_FILL=1
IL_LO_FLASHNCACHE_OPT_ALL_PARTS_FILL=1

# The EXTRA_KERNEL_PARAMS are for the grub command line.
# The kernel parameter rootdelay is set to handle the eUSB case.
IL_LO_FLASHNCACHE_EXTRA_KERNEL_PARAMS="rootdelay=5 crashkernel=auto"

# Each of these says which PART (partition) below goes with which LOC
#     (standardized location) .  It also says for each LOC which subdirectory
#     under the PART the LOC is in, and what files to extract there

IL_LO_FLASHNCACHE_LOC_BOOTMGR_1_PART=BOOTMGR
IL_LO_FLASHNCACHE_LOC_BOOTMGR_1_DIR=/
IL_LO_FLASHNCACHE_LOC_BOOTMGR_1_IMAGE_EXTRACT="./bootmgr"
IL_LO_FLASHNCACHE_LOC_BOOTMGR_1_IMAGE_EXTRACT_PREFIX="/bootmgr"

IL_LO_FLASHNCACHE_LOC_BOOT_1_PART=BOOT1
IL_LO_FLASHNCACHE_LOC_BOOT_1_DIR=/
IL_LO_FLASHNCACHE_LOC_BOOT_1_IMAGE_EXTRACT="./boot"
IL_LO_FLASHNCACHE_LOC_BOOT_1_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_FLASHNCACHE_LOC_BOOT_2_PART=BOOT2
IL_LO_FLASHNCACHE_LOC_BOOT_2_DIR=/
IL_LO_FLASHNCACHE_LOC_BOOT_2_IMAGE_EXTRACT="./boot"
IL_LO_FLASHNCACHE_LOC_BOOT_2_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_FLASHNCACHE_LOC_ROOT_1_PART=ROOT1
IL_LO_FLASHNCACHE_LOC_ROOT_1_DIR=/
IL_LO_FLASHNCACHE_LOC_ROOT_1_IMAGE_EXTRACT="./"
IL_LO_FLASHNCACHE_LOC_ROOT_1_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_FLASHNCACHE_LOC_ROOT_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_FLASHNCACHE_LOC_ROOT_2_PART=ROOT2
IL_LO_FLASHNCACHE_LOC_ROOT_2_DIR=/
IL_LO_FLASHNCACHE_LOC_ROOT_2_IMAGE_EXTRACT="./"
IL_LO_FLASHNCACHE_LOC_ROOT_2_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_FLASHNCACHE_LOC_ROOT_2_IMAGE_EXTRACT_PREFIX=""

IL_LO_FLASHNCACHE_LOC_CONFIG_1_PART=CONFIG
IL_LO_FLASHNCACHE_LOC_CONFIG_1_DIR=/
IL_LO_FLASHNCACHE_LOC_CONFIG_1_IMAGE_EXTRACT="./config"
IL_LO_FLASHNCACHE_LOC_CONFIG_1_IMAGE_EXTRACT_PREFIX="/config"

IL_LO_FLASHNCACHE_LOC_VAR_1_PART=VAR
IL_LO_FLASHNCACHE_LOC_VAR_1_DIR=/
IL_LO_FLASHNCACHE_LOC_VAR_1_IMAGE_EXTRACT="./var"
IL_LO_FLASHNCACHE_LOC_VAR_1_IMAGE_EXTRACT_PREFIX="/var"

IL_LO_FLASHNCACHE_LOC_COREDUMP_1_PART=COREDUMP
IL_LO_FLASHNCACHE_LOC_COREDUMP_1_DIR=/
IL_LO_FLASHNCACHE_LOC_COREDUMP_1_IMAGE_EXTRACT="./coredump"
IL_LO_FLASHNCACHE_LOC_COREDUMP_1_IMAGE_EXTRACT_PREFIX="/coredump"

IL_LO_FLASHNCACHE_LOC_NKN_1_PART=NKN
IL_LO_FLASHNCACHE_LOC_NKN_1_DIR=/
IL_LO_FLASHNCACHE_LOC_NKN_1_IMAGE_EXTRACT="./nkn"
IL_LO_FLASHNCACHE_LOC_NKN_1_IMAGE_EXTRACT_PREFIX="/nkn"

IL_LO_FLASHNCACHE_LOC_LOG_1_PART=LOG
IL_LO_FLASHNCACHE_LOC_LOG_1_DIR=/
IL_LO_FLASHNCACHE_LOC_LOG_1_IMAGE_EXTRACT="./log"
IL_LO_FLASHNCACHE_LOC_LOG_1_IMAGE_EXTRACT_PREFIX="/log"

# Each of these says how a partition is named, labled, and sized.

IL_LO_FLASHNCACHE_PART_BOOTMGR_TARGET=DISK1
IL_LO_FLASHNCACHE_PART_BOOTMGR_PART_NUM=1
IL_LO_FLASHNCACHE_PART_BOOTMGR_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK1_DEV}${IL_LO_FLASHNCACHE_PART_BOOTMGR_PART_NUM}
IL_LO_FLASHNCACHE_PART_BOOTMGR_LABEL=BOOTMGR
IL_LO_FLASHNCACHE_PART_BOOTMGR_MOUNT=/bootmgr
IL_LO_FLASHNCACHE_PART_BOOTMGR_BOOTABLE=1
IL_LO_FLASHNCACHE_PART_BOOTMGR_PART_ID=83
IL_LO_FLASHNCACHE_PART_BOOTMGR_FSTYPE=ext3
IL_LO_FLASHNCACHE_PART_BOOTMGR_SIZE=30
IL_LO_FLASHNCACHE_PART_BOOTMGR_GROWTHCAP=
IL_LO_FLASHNCACHE_PART_BOOTMGR_GROWTHWEIGHT=
IL_LO_FLASHNCACHE_PART_BOOTMGR_OPTIONS="defaults,noatime,ro"

IL_LO_FLASHNCACHE_PART_BOOT1_TARGET=DISK1
IL_LO_FLASHNCACHE_PART_BOOT1_PART_NUM=2
IL_LO_FLASHNCACHE_PART_BOOT1_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK1_DEV}${IL_LO_FLASHNCACHE_PART_BOOT1_PART_NUM}
IL_LO_FLASHNCACHE_PART_BOOT1_LABEL=BOOT_1
IL_LO_FLASHNCACHE_PART_BOOT1_MOUNT=/boot
IL_LO_FLASHNCACHE_PART_BOOT1_BOOTABLE=
IL_LO_FLASHNCACHE_PART_BOOT1_PART_ID=83
IL_LO_FLASHNCACHE_PART_BOOT1_FSTYPE=ext3
IL_LO_FLASHNCACHE_PART_BOOT1_SIZE=30
IL_LO_FLASHNCACHE_PART_BOOT1_GROWTHCAP=
IL_LO_FLASHNCACHE_PART_BOOT1_GROWTHWEIGHT=
IL_LO_FLASHNCACHE_PART_BOOT1_OPTIONS="defaults,noatime,ro"

IL_LO_FLASHNCACHE_PART_BOOT2_TARGET=DISK1
IL_LO_FLASHNCACHE_PART_BOOT2_PART_NUM=3
IL_LO_FLASHNCACHE_PART_BOOT2_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK1_DEV}${IL_LO_FLASHNCACHE_PART_BOOT2_PART_NUM}
IL_LO_FLASHNCACHE_PART_BOOT2_LABEL=BOOT_2
IL_LO_FLASHNCACHE_PART_BOOT2_MOUNT=/boot
IL_LO_FLASHNCACHE_PART_BOOT2_BOOTABLE=
IL_LO_FLASHNCACHE_PART_BOOT2_PART_ID=83
IL_LO_FLASHNCACHE_PART_BOOT2_FSTYPE=ext3
IL_LO_FLASHNCACHE_PART_BOOT2_SIZE=30
IL_LO_FLASHNCACHE_PART_BOOT2_GROWTHCAP=
IL_LO_FLASHNCACHE_PART_BOOT2_GROWTHWEIGHT=
IL_LO_FLASHNCACHE_PART_BOOT2_OPTIONS="defaults,noatime,ro"

IL_LO_FLASHNCACHE_PART_EXT_TARGET=DISK1
IL_LO_FLASHNCACHE_PART_EXT_PART_NUM=4
IL_LO_FLASHNCACHE_PART_EXT_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK1_DEV}${IL_LO_FLASHNCACHE_PART_EXT_PART_NUM}
IL_LO_FLASHNCACHE_PART_EXT_LABEL=
IL_LO_FLASHNCACHE_PART_EXT_MOUNT=
IL_LO_FLASHNCACHE_PART_EXT_BOOTABLE=
IL_LO_FLASHNCACHE_PART_EXT_PART_ID=0f
IL_LO_FLASHNCACHE_PART_EXT_FSTYPE=
IL_LO_FLASHNCACHE_PART_EXT_SIZE=
IL_LO_FLASHNCACHE_PART_EXT_GROWTHCAP=
IL_LO_FLASHNCACHE_PART_EXT_GROWTHWEIGHT=

# On 4G drive, the size of each root partition will be 1433.
# On 8G drive, the size of each root partitioin will be around 3400?
# On 16G drive, the size of each root partition will be 4000.
IL_LO_FLASHNCACHE_PART_ROOT1_TARGET=DISK1
IL_LO_FLASHNCACHE_PART_ROOT1_PART_NUM=5
IL_LO_FLASHNCACHE_PART_ROOT1_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK1_DEV}${IL_LO_FLASHNCACHE_PART_ROOT1_PART_NUM}
IL_LO_FLASHNCACHE_PART_ROOT1_LABEL=ROOT_1
IL_LO_FLASHNCACHE_PART_ROOT1_MOUNT=/
IL_LO_FLASHNCACHE_PART_ROOT1_BOOTABLE=
IL_LO_FLASHNCACHE_PART_ROOT1_PART_ID=83
IL_LO_FLASHNCACHE_PART_ROOT1_FSTYPE=ext3
IL_LO_FLASHNCACHE_PART_ROOT1_SIZE=1430
IL_LO_FLASHNCACHE_PART_ROOT1_GROWTHCAP=4000
IL_LO_FLASHNCACHE_PART_ROOT1_GROWTHWEIGHT=49
IL_LO_FLASHNCACHE_PART_ROOT1_OPTIONS="defaults,noatime,ro"

IL_LO_FLASHNCACHE_PART_ROOT2_TARGET=DISK1
IL_LO_FLASHNCACHE_PART_ROOT2_PART_NUM=6
IL_LO_FLASHNCACHE_PART_ROOT2_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK1_DEV}${IL_LO_FLASHNCACHE_PART_ROOT2_PART_NUM}
IL_LO_FLASHNCACHE_PART_ROOT2_LABEL=ROOT_2
IL_LO_FLASHNCACHE_PART_ROOT2_MOUNT=/
IL_LO_FLASHNCACHE_PART_ROOT2_BOOTABLE=
IL_LO_FLASHNCACHE_PART_ROOT2_PART_ID=83
IL_LO_FLASHNCACHE_PART_ROOT2_FSTYPE=ext3
IL_LO_FLASHNCACHE_PART_ROOT2_SIZE=1430
IL_LO_FLASHNCACHE_PART_ROOT2_GROWTHCAP=4000
IL_LO_FLASHNCACHE_PART_ROOT2_GROWTHWEIGHT=49
IL_LO_FLASHNCACHE_PART_ROOT2_OPTIONS="defaults,noatime,ro"

IL_LO_FLASHNCACHE_PART_CONFIG_TARGET=DISK1
IL_LO_FLASHNCACHE_PART_CONFIG_PART_NUM=7
IL_LO_FLASHNCACHE_PART_CONFIG_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK1_DEV}${IL_LO_FLASHNCACHE_PART_CONFIG_PART_NUM}
IL_LO_FLASHNCACHE_PART_CONFIG_LABEL=CONFIG
IL_LO_FLASHNCACHE_PART_CONFIG_MOUNT=/config
IL_LO_FLASHNCACHE_PART_CONFIG_BOOTABLE=
IL_LO_FLASHNCACHE_PART_CONFIG_PART_ID=83
IL_LO_FLASHNCACHE_PART_CONFIG_FSTYPE=ext3
IL_LO_FLASHNCACHE_PART_CONFIG_SIZE=64
IL_LO_FLASHNCACHE_PART_CONFIG_GROWTHCAP=
IL_LO_FLASHNCACHE_PART_CONFIG_GROWTHWEIGHT=
IL_LO_FLASHNCACHE_PART_CONFIG_OPTIONS="defaults,noatime"

# For disks larger than 8GB then /var gets the balance of the space
# after the root partitions max out at 4000 each.
IL_LO_FLASHNCACHE_PART_VAR_TARGET=DISK1
IL_LO_FLASHNCACHE_PART_VAR_PART_NUM=8
IL_LO_FLASHNCACHE_PART_VAR_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK1_DEV}${IL_LO_FLASHNCACHE_PART_VAR_PART_NUM}
IL_LO_FLASHNCACHE_PART_VAR_LABEL=VAR
IL_LO_FLASHNCACHE_PART_VAR_MOUNT=/var
IL_LO_FLASHNCACHE_PART_VAR_BOOTABLE=
IL_LO_FLASHNCACHE_PART_VAR_PART_ID=83
IL_LO_FLASHNCACHE_PART_VAR_FSTYPE=ext3
IL_LO_FLASHNCACHE_PART_VAR_SIZE=900
IL_LO_FLASHNCACHE_PART_VAR_GROWTHCAP=
IL_LO_FLASHNCACHE_PART_VAR_GROWTHWEIGHT=2
IL_LO_FLASHNCACHE_PART_VAR_OPTIONS="defaults,noatime"

IL_LO_FLASHNCACHE_PART_COREDUMP_TARGET=DISK2
IL_LO_FLASHNCACHE_PART_COREDUMP_PART_NUM=1
IL_LO_FLASHNCACHE_PART_COREDUMP_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK2_DEV}${IL_LO_FLASHNCACHE_PART_COREDUMP_PART_NUM}
IL_LO_FLASHNCACHE_PART_COREDUMP_LABEL=COREDUMP
IL_LO_FLASHNCACHE_PART_COREDUMP_MOUNT=/coredump
IL_LO_FLASHNCACHE_PART_COREDUMP_BOOTABLE=
IL_LO_FLASHNCACHE_PART_COREDUMP_PART_ID=83
IL_LO_FLASHNCACHE_PART_COREDUMP_FSTYPE=ext3
IL_LO_FLASHNCACHE_PART_COREDUMP_SIZE=${COREDUMP_SIZE}
IL_LO_FLASHNCACHE_PART_COREDUMP_GROWTHCAP=
IL_LO_FLASHNCACHE_PART_COREDUMP_GROWTHWEIGHT=
IL_LO_FLASHNCACHE_PART_COREDUMP_OPTIONS="defaults,noatime"

IL_LO_FLASHNCACHE_PART_NKN_TARGET=DISK2
IL_LO_FLASHNCACHE_PART_NKN_PART_NUM=2
IL_LO_FLASHNCACHE_PART_NKN_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK2_DEV}${IL_LO_FLASHNCACHE_PART_NKN_PART_NUM}
IL_LO_FLASHNCACHE_PART_NKN_LABEL=NKN
IL_LO_FLASHNCACHE_PART_NKN_MOUNT=/nkn
IL_LO_FLASHNCACHE_PART_NKN_BOOTABLE=
IL_LO_FLASHNCACHE_PART_NKN_PART_ID=83
IL_LO_FLASHNCACHE_PART_NKN_FSTYPE=ext3
IL_LO_FLASHNCACHE_PART_NKN_SIZE=21000
IL_LO_FLASHNCACHE_PART_NKN_GROWTHCAP=102400
IL_LO_FLASHNCACHE_PART_NKN_GROWTHWEIGHT=50
IL_LO_FLASHNCACHE_PART_NKN_OPTIONS="defaults,noatime"

IL_LO_FLASHNCACHE_PART_LOG_TARGET=DISK2
IL_LO_FLASHNCACHE_PART_LOG_PART_NUM=3
IL_LO_FLASHNCACHE_PART_LOG_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK2_DEV}${IL_LO_FLASHNCACHE_PART_LOG_PART_NUM}
IL_LO_FLASHNCACHE_PART_LOG_LABEL=LOG
IL_LO_FLASHNCACHE_PART_LOG_MOUNT=/log
IL_LO_FLASHNCACHE_PART_LOG_BOOTABLE=
IL_LO_FLASHNCACHE_PART_LOG_PART_ID=83
IL_LO_FLASHNCACHE_PART_LOG_FSTYPE=ext3
IL_LO_FLASHNCACHE_PART_LOG_SIZE=10000
IL_LO_FLASHNCACHE_PART_LOG_GROWTHCAP=
IL_LO_FLASHNCACHE_PART_LOG_GROWTHWEIGHT=50
IL_LO_FLASHNCACHE_PART_LOG_OPTIONS="defaults,noatime"

IL_LO_FLASHNCACHE_PART_EXT2_TARGET=DISK2
IL_LO_FLASHNCACHE_PART_EXT2_PART_NUM=4
IL_LO_FLASHNCACHE_PART_EXT2_DEV=${IL_LO_FLASHNCACHE_TARGET_DISK2_DEV}${IL_LO_FLASHNCACHE_PART_EXT2_PART_NUM}
IL_LO_FLASHNCACHE_PART_EXT2_LABEL=
IL_LO_FLASHNCACHE_PART_EXT2_MOUNT=
IL_LO_FLASHNCACHE_PART_EXT2_BOOTABLE=
IL_LO_FLASHNCACHE_PART_EXT2_PART_ID=0f
IL_LO_FLASHNCACHE_PART_EXT2_FSTYPE=
IL_LO_FLASHNCACHE_PART_EXT2_SIZE=
IL_LO_FLASHNCACHE_PART_EXT2_GROWTHCAP=
IL_LO_FLASHNCACHE_PART_EXT2_GROWTHWEIGHT=


# ###########################################################################
# ###########################################################################
# ==========================================================
##32G: For demo purposes, install on a small drive (at least 32GB)
##32G: Small coredump partition.
##32G: Includes root drive cache.
##32G: Limit /nkn and /log to 102,400MB
# Minimum disk size for this is 32GB disk, no maximum.
# Partitions :
#  1 : /bootmgr
#  2 : /boot (1 of 2)
#  3 : /boot (2 of 2) - 2nd boot image
#  4 : extended partition
#  5 : / (1 of 2)
#  6 : / (2 of 2) - for 2nd boot image 
#  7 : /config
#  8 : /var
#  9: /coredump
# 10 : /nkn
# 11 : /log
# 12 : dmfs
# 13 : dmraw
# ==========================================================

##Model: demo32g1 -> 32G
IL_LO_32G_LOCS="BOOTMGR_1 BOOT_1 BOOT_2 ROOT_1 ROOT_2 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1 DMFS_1 DMRAW_1"

# Targets are things like a disk or a flash device
IL_LO_32G_TARGET_NUM_TARGETS=1
IL_LO_32G_TARGETS="DISK1"
IL_LO_32G_TARGET_DISK1_NUM=1

# Configuration option to indicate that the root locations
# should be read-only. It is still necessary to indicate
# 'ro' in the partition option line for root.
IL_LO_32G_ROOT_RDONLY=1

IL_LO_32G_INITRD_PATH=
IL_LO_32G_ROOT_MOUNT_BY_LABEL=0

IL_LO_32G_TARGET_DISK1_MINSIZE=31000
IL_LO_32G_TARGET_DISK1_LABELTYPE=

# What partitions get made on the targets
IL_LO_32G_PARTS="BOOTMGR BOOT1 BOOT2 EXT ROOT1 ROOT2 CONFIG VAR COREDUMP NKN LOG DMFS DMRAW"

IL_LO_32G_IMAGE_1_INSTALL_LOCS="BOOT_1 ROOT_1"
IL_LO_32G_IMAGE_2_INSTALL_LOCS="BOOT_2 ROOT_2"
IL_LO_32G_IMAGE_1_LOCS="ROOT_1 BOOT_1 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"
IL_LO_32G_IMAGE_2_LOCS="ROOT_2 BOOT_2 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

IL_LO_32G_OPT_LAST_PART_FILL=1
IL_LO_32G_OPT_ALL_PARTS_FILL=1

IL_LO_32G_EXTRA_KERNEL_PARAMS="crashkernel=auto"

# Each of these says which PART (partition) below goes with which LOC
#     (standardized location) .  It also says for each LOC which subdirectory
#     under the PART the LOC is in, and what files to extract there

IL_LO_32G_LOC_BOOTMGR_1_PART=BOOTMGR
IL_LO_32G_LOC_BOOTMGR_1_DIR=/
IL_LO_32G_LOC_BOOTMGR_1_IMAGE_EXTRACT="./bootmgr"
IL_LO_32G_LOC_BOOTMGR_1_IMAGE_EXTRACT_PREFIX="/bootmgr"

IL_LO_32G_LOC_BOOT_1_PART=BOOT1
IL_LO_32G_LOC_BOOT_1_DIR=/
IL_LO_32G_LOC_BOOT_1_IMAGE_EXTRACT="./boot"
IL_LO_32G_LOC_BOOT_1_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_32G_LOC_BOOT_2_PART=BOOT2
IL_LO_32G_LOC_BOOT_2_DIR=/
IL_LO_32G_LOC_BOOT_2_IMAGE_EXTRACT="./boot"
IL_LO_32G_LOC_BOOT_2_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_32G_LOC_ROOT_1_PART=ROOT1
IL_LO_32G_LOC_ROOT_1_DIR=/
IL_LO_32G_LOC_ROOT_1_IMAGE_EXTRACT="./"
IL_LO_32G_LOC_ROOT_1_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_32G_LOC_ROOT_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_32G_LOC_ROOT_2_PART=ROOT2
IL_LO_32G_LOC_ROOT_2_DIR=/
IL_LO_32G_LOC_ROOT_2_IMAGE_EXTRACT="./"
IL_LO_32G_LOC_ROOT_2_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_32G_LOC_ROOT_2_IMAGE_EXTRACT_PREFIX=""

IL_LO_32G_LOC_CONFIG_1_PART=CONFIG
IL_LO_32G_LOC_CONFIG_1_DIR=/
IL_LO_32G_LOC_CONFIG_1_IMAGE_EXTRACT="./config"
IL_LO_32G_LOC_CONFIG_1_IMAGE_EXTRACT_PREFIX="/config"

IL_LO_32G_LOC_VAR_1_PART=VAR
IL_LO_32G_LOC_VAR_1_DIR=/
IL_LO_32G_LOC_VAR_1_IMAGE_EXTRACT="./var"
IL_LO_32G_LOC_VAR_1_IMAGE_EXTRACT_PREFIX="/var"

IL_LO_32G_LOC_COREDUMP_1_PART=COREDUMP
IL_LO_32G_LOC_COREDUMP_1_DIR=/
IL_LO_32G_LOC_COREDUMP_1_IMAGE_EXTRACT="./coredump"
IL_LO_32G_LOC_COREDUMP_1_IMAGE_EXTRACT_PREFIX="/coredump"

IL_LO_32G_LOC_NKN_1_PART=NKN
IL_LO_32G_LOC_NKN_1_DIR=/
IL_LO_32G_LOC_NKN_1_IMAGE_EXTRACT="./nkn"
IL_LO_32G_LOC_NKN_1_IMAGE_EXTRACT_PREFIX="/nkn"

IL_LO_32G_LOC_LOG_1_PART=LOG
IL_LO_32G_LOC_LOG_1_DIR=/
IL_LO_32G_LOC_LOG_1_IMAGE_EXTRACT="./log"
IL_LO_32G_LOC_LOG_1_IMAGE_EXTRACT_PREFIX="/log"

IL_LO_32G_LOC_DMFS_1_PART=DMFS
IL_LO_32G_LOC_DMFS_1_DIR=/
IL_LO_32G_LOC_DMFS_1_IMAGE_EXTRACT=""
IL_LO_32G_LOC_DMFS_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_32G_LOC_DMRAW_1_PART=DMRAW
IL_LO_32G_LOC_DMRAW_1_DIR=/
IL_LO_32G_LOC_DMRAW_1_IMAGE_EXTRACT=""
IL_LO_32G_LOC_DMRAW_1_IMAGE_EXTRACT_PREFIX=""


# Each of these says how a partition is named, labled, and sized.

IL_LO_32G_PART_BOOTMGR_TARGET=DISK1
IL_LO_32G_PART_BOOTMGR_PART_NUM=1
IL_LO_32G_PART_BOOTMGR_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_BOOTMGR_PART_NUM}
IL_LO_32G_PART_BOOTMGR_LABEL=BOOTMGR
IL_LO_32G_PART_BOOTMGR_MOUNT=/bootmgr
IL_LO_32G_PART_BOOTMGR_BOOTABLE=1
IL_LO_32G_PART_BOOTMGR_PART_ID=83
IL_LO_32G_PART_BOOTMGR_FSTYPE=ext3
IL_LO_32G_PART_BOOTMGR_SIZE=30
IL_LO_32G_PART_BOOTMGR_GROWTHCAP=
IL_LO_32G_PART_BOOTMGR_GROWTHWEIGHT=
IL_LO_32G_PART_BOOTMGR_OPTIONS="defaults,noatime,ro"

IL_LO_32G_PART_BOOT1_TARGET=DISK1
IL_LO_32G_PART_BOOT1_PART_NUM=2
IL_LO_32G_PART_BOOT1_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_BOOT1_PART_NUM}
IL_LO_32G_PART_BOOT1_LABEL=BOOT_1
IL_LO_32G_PART_BOOT1_MOUNT=/boot
IL_LO_32G_PART_BOOT1_BOOTABLE=
IL_LO_32G_PART_BOOT1_PART_ID=83
IL_LO_32G_PART_BOOT1_FSTYPE=ext3
IL_LO_32G_PART_BOOT1_SIZE=30
IL_LO_32G_PART_BOOT1_GROWTHCAP=
IL_LO_32G_PART_BOOT1_GROWTHWEIGHT=
IL_LO_32G_PART_BOOT1_OPTIONS="defaults,noatime,ro"

IL_LO_32G_PART_BOOT2_TARGET=DISK1
IL_LO_32G_PART_BOOT2_PART_NUM=3
IL_LO_32G_PART_BOOT2_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_BOOT2_PART_NUM}
IL_LO_32G_PART_BOOT2_LABEL=BOOT_2
IL_LO_32G_PART_BOOT2_MOUNT=/boot
IL_LO_32G_PART_BOOT2_BOOTABLE=
IL_LO_32G_PART_BOOT2_PART_ID=83
IL_LO_32G_PART_BOOT2_FSTYPE=ext3
IL_LO_32G_PART_BOOT2_SIZE=30
IL_LO_32G_PART_BOOT2_GROWTHCAP=
IL_LO_32G_PART_BOOT2_GROWTHWEIGHT=
IL_LO_32G_PART_BOOT2_OPTIONS="defaults,noatime,ro"

IL_LO_32G_PART_EXT_TARGET=DISK1
IL_LO_32G_PART_EXT_PART_NUM=4
IL_LO_32G_PART_EXT_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_EXT_PART_NUM}
IL_LO_32G_PART_EXT_LABEL=
IL_LO_32G_PART_EXT_MOUNT=
IL_LO_32G_PART_EXT_BOOTABLE=
IL_LO_32G_PART_EXT_PART_ID=0f
IL_LO_32G_PART_EXT_FSTYPE=
IL_LO_32G_PART_EXT_SIZE=
IL_LO_32G_PART_EXT_GROWTHCAP=
IL_LO_32G_PART_EXT_GROWTHWEIGHT=

IL_LO_32G_PART_ROOT1_TARGET=DISK1
IL_LO_32G_PART_ROOT1_PART_NUM=5
IL_LO_32G_PART_ROOT1_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_ROOT1_PART_NUM}
IL_LO_32G_PART_ROOT1_LABEL=ROOT_1
IL_LO_32G_PART_ROOT1_MOUNT=/
IL_LO_32G_PART_ROOT1_BOOTABLE=
IL_LO_32G_PART_ROOT1_PART_ID=83
IL_LO_32G_PART_ROOT1_FSTYPE=ext3
IL_LO_32G_PART_ROOT1_SIZE=2000
IL_LO_32G_PART_ROOT1_GROWTHCAP=
IL_LO_32G_PART_ROOT1_GROWTHWEIGHT=
IL_LO_32G_PART_ROOT1_OPTIONS="defaults,noatime,ro"

IL_LO_32G_PART_ROOT2_TARGET=DISK1
IL_LO_32G_PART_ROOT2_PART_NUM=6
IL_LO_32G_PART_ROOT2_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_ROOT2_PART_NUM}
IL_LO_32G_PART_ROOT2_LABEL=ROOT_2
IL_LO_32G_PART_ROOT2_MOUNT=/
IL_LO_32G_PART_ROOT2_BOOTABLE=
IL_LO_32G_PART_ROOT2_PART_ID=83
IL_LO_32G_PART_ROOT2_FSTYPE=ext3
IL_LO_32G_PART_ROOT2_SIZE=2000
IL_LO_32G_PART_ROOT2_GROWTHCAP=
IL_LO_32G_PART_ROOT2_GROWTHWEIGHT=
IL_LO_32G_PART_ROOT2_OPTIONS="defaults,noatime,ro"

IL_LO_32G_PART_CONFIG_TARGET=DISK1
IL_LO_32G_PART_CONFIG_PART_NUM=7
IL_LO_32G_PART_CONFIG_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_CONFIG_PART_NUM}
IL_LO_32G_PART_CONFIG_LABEL=CONFIG
IL_LO_32G_PART_CONFIG_MOUNT=/config
IL_LO_32G_PART_CONFIG_BOOTABLE=
IL_LO_32G_PART_CONFIG_PART_ID=83
IL_LO_32G_PART_CONFIG_FSTYPE=ext3
IL_LO_32G_PART_CONFIG_SIZE=256
IL_LO_32G_PART_CONFIG_GROWTHCAP=
IL_LO_32G_PART_CONFIG_GROWTHWEIGHT=

IL_LO_32G_PART_VAR_TARGET=DISK1
IL_LO_32G_PART_VAR_PART_NUM=8
IL_LO_32G_PART_VAR_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_VAR_PART_NUM}
IL_LO_32G_PART_VAR_LABEL=VAR
IL_LO_32G_PART_VAR_MOUNT=/var
IL_LO_32G_PART_VAR_BOOTABLE=
IL_LO_32G_PART_VAR_PART_ID=83
IL_LO_32G_PART_VAR_FSTYPE=ext3
IL_LO_32G_PART_VAR_SIZE=3072
IL_LO_32G_PART_VAR_GROWTHCAP=
IL_LO_32G_PART_VAR_GROWTHWEIGHT=

IL_LO_32G_PART_COREDUMP_TARGET=DISK1
IL_LO_32G_PART_COREDUMP_PART_NUM=9
IL_LO_32G_PART_COREDUMP_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_COREDUMP_PART_NUM}
IL_LO_32G_PART_COREDUMP_LABEL=COREDUMP
IL_LO_32G_PART_COREDUMP_MOUNT=/coredump
IL_LO_32G_PART_COREDUMP_BOOTABLE=
IL_LO_32G_PART_COREDUMP_PART_ID=83
IL_LO_32G_PART_COREDUMP_FSTYPE=ext3
IL_LO_32G_PART_COREDUMP_SIZE=4096
IL_LO_32G_PART_COREDUMP_GROWTHCAP=
IL_LO_32G_PART_COREDUMP_GROWTHWEIGHT=
IL_LO_32G_PART_COREDUMP_OPTIONS="defaults,noatime"

IL_LO_32G_PART_NKN_TARGET=DISK1
IL_LO_32G_PART_NKN_PART_NUM=10
IL_LO_32G_PART_NKN_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_NKN_PART_NUM}
IL_LO_32G_PART_NKN_LABEL=NKN
IL_LO_32G_PART_NKN_MOUNT=/nkn
IL_LO_32G_PART_NKN_BOOTABLE=
IL_LO_32G_PART_NKN_PART_ID=83
IL_LO_32G_PART_NKN_FSTYPE=ext3
IL_LO_32G_PART_NKN_SIZE=5000
IL_LO_32G_PART_NKN_GROWTHCAP=102400
IL_LO_32G_PART_NKN_GROWTHWEIGHT=33
IL_LO_32G_PART_NKN_OPTIONS="defaults,noatime"

IL_LO_32G_PART_LOG_TARGET=DISK1
IL_LO_32G_PART_LOG_PART_NUM=11
IL_LO_32G_PART_LOG_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_LOG_PART_NUM}
IL_LO_32G_PART_LOG_LABEL=LOG
IL_LO_32G_PART_LOG_MOUNT=/log
IL_LO_32G_PART_LOG_BOOTABLE=
IL_LO_32G_PART_LOG_PART_ID=83
IL_LO_32G_PART_LOG_FSTYPE=ext3
IL_LO_32G_PART_LOG_SIZE=5000
IL_LO_32G_PART_LOG_GROWTHCAP=102400
IL_LO_32G_PART_LOG_GROWTHWEIGHT=33
IL_LO_32G_PART_LOG_OPTIONS="defaults,noatime"

IL_LO_32G_PART_DMFS_TARGET=DISK1
IL_LO_32G_PART_DMFS_PART_NUM=12
IL_LO_32G_PART_DMFS_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_DMFS_PART_NUM}
IL_LO_32G_PART_DMFS_LABEL=DMFS
IL_LO_32G_PART_DMFS_MOUNT=
IL_LO_32G_PART_DMFS_BOOTABLE=
IL_LO_32G_PART_DMFS_PART_ID=83
  # For the dmfs parition set the FSTYPE to blank, so it will not create a new
  # filesystem on the partition, so we can preserve the contents.
  # Our install-mfd will do all the needed making of filesystems and labels,
  # because it knows when we want to preserve the data or not.
IL_LO_32G_PART_DMFS_FSTYPE=
IL_LO_32G_PART_DMFS_SIZE=300
IL_LO_32G_PART_DMFS_GROWTHCAP=
# We want this to be about 10% the size of DMRAW
IL_LO_32G_PART_DMFS_GROWTHWEIGHT=3

IL_LO_32G_PART_DMRAW_TARGET=DISK1
IL_LO_32G_PART_DMRAW_PART_NUM=13
IL_LO_32G_PART_DMRAW_DEV=${IL_LO_32G_TARGET_DISK1_DEV}${IL_LO_32G_PART_DMRAW_PART_NUM}
IL_LO_32G_PART_DMRAW_LABEL=DMRAW
IL_LO_32G_PART_DMRAW_MOUNT=
IL_LO_32G_PART_DMRAW_BOOTABLE=
IL_LO_32G_PART_DMRAW_PART_ID=
IL_LO_32G_PART_DMRAW_FSTYPE=
IL_LO_32G_PART_DMRAW_SIZE=3000
IL_LO_32G_PART_DMRAW_GROWTHCAP=
IL_LO_32G_PART_DMRAW_GROWTHWEIGHT=30


# ###########################################################################
# ###########################################################################
# ==========================================================
##8GNCACHE: For demo purposes, install on a 8GB drive, but with no root drive cache.
# Partitions :
#  1 : /bootmgr
#  2 : /boot (1 of 2)
#  3 : /boot (2 of 2) - 2nd boot image
#  4 : extended partition
#  5 : / (1 of 2)
#  6 : / (2 of 2) - for 2nd boot image
#  7 : /config
#  8 : /var
#  9 : /coredump
# 10 : /nkn
# 11 : /log
# ==========================================================

##Model: demo8g2 -> 8GNCACHE
IL_LO_8GNCACHE_LOCS="BOOTMGR_1 BOOT_1 BOOT_2 ROOT_1 ROOT_2 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

# Targets are things like a disk or a flash device
IL_LO_8GNCACHE_TARGET_NUM_TARGETS=1
IL_LO_8GNCACHE_TARGETS="DISK1"
IL_LO_8GNCACHE_TARGET_DISK1_NUM=1

IL_LO_8GNCACHE_ROOT_RDONLY=1

IL_LO_8GNCACHE_INITRD_PATH=
IL_LO_8GNCACHE_ROOT_MOUNT_BY_LABEL=0

IL_LO_8GNCACHE_TARGET_DISK1_MINSIZE=8000
IL_LO_8GNCACHE_TARGET_DISK1_LABELTYPE=

# What partitions get made on the targets
IL_LO_8GNCACHE_PARTS="BOOTMGR BOOT1 BOOT2 EXT ROOT1 ROOT2 CONFIG VAR COREDUMP NKN LOG"

IL_LO_8GNCACHE_IMAGE_1_INSTALL_LOCS="BOOT_1 ROOT_1"
IL_LO_8GNCACHE_IMAGE_2_INSTALL_LOCS="BOOT_2 ROOT_2"
IL_LO_8GNCACHE_IMAGE_1_LOCS="ROOT_1 BOOT_1 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"
IL_LO_8GNCACHE_IMAGE_2_LOCS="ROOT_2 BOOT_2 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

IL_LO_8GNCACHE_OPT_LAST_PART_FILL=1
IL_LO_8GNCACHE_OPT_ALL_PARTS_FILL=1

IL_LO_8GNCACHE_EXTRA_KERNEL_PARAMS="crashkernel=auto"

# Each of these says which PART (partition) below goes with which LOC
#     (standardized location) .  It also says for each LOC which subdirectory
#     under the PART the LOC is in, and what files to extract there

IL_LO_8GNCACHE_LOC_BOOTMGR_1_PART=BOOTMGR
IL_LO_8GNCACHE_LOC_BOOTMGR_1_DIR=/
IL_LO_8GNCACHE_LOC_BOOTMGR_1_IMAGE_EXTRACT="./bootmgr"
IL_LO_8GNCACHE_LOC_BOOTMGR_1_IMAGE_EXTRACT_PREFIX="/bootmgr"

IL_LO_8GNCACHE_LOC_BOOT_1_PART=BOOT1
IL_LO_8GNCACHE_LOC_BOOT_1_DIR=/
IL_LO_8GNCACHE_LOC_BOOT_1_IMAGE_EXTRACT="./boot"
IL_LO_8GNCACHE_LOC_BOOT_1_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_8GNCACHE_LOC_BOOT_2_PART=BOOT2
IL_LO_8GNCACHE_LOC_BOOT_2_DIR=/
IL_LO_8GNCACHE_LOC_BOOT_2_IMAGE_EXTRACT="./boot"
IL_LO_8GNCACHE_LOC_BOOT_2_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_8GNCACHE_LOC_ROOT_1_PART=ROOT1
IL_LO_8GNCACHE_LOC_ROOT_1_DIR=/
IL_LO_8GNCACHE_LOC_ROOT_1_IMAGE_EXTRACT="./"
IL_LO_8GNCACHE_LOC_ROOT_1_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_8GNCACHE_LOC_ROOT_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_8GNCACHE_LOC_ROOT_2_PART=ROOT2
IL_LO_8GNCACHE_LOC_ROOT_2_DIR=/
IL_LO_8GNCACHE_LOC_ROOT_2_IMAGE_EXTRACT="./"
IL_LO_8GNCACHE_LOC_ROOT_2_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_8GNCACHE_LOC_ROOT_2_IMAGE_EXTRACT_PREFIX=""

IL_LO_8GNCACHE_LOC_CONFIG_1_PART=CONFIG
IL_LO_8GNCACHE_LOC_CONFIG_1_DIR=/
IL_LO_8GNCACHE_LOC_CONFIG_1_IMAGE_EXTRACT="./config"
IL_LO_8GNCACHE_LOC_CONFIG_1_IMAGE_EXTRACT_PREFIX="/config"

IL_LO_8GNCACHE_LOC_VAR_1_PART=VAR
IL_LO_8GNCACHE_LOC_VAR_1_DIR=/
IL_LO_8GNCACHE_LOC_VAR_1_IMAGE_EXTRACT="./var"
IL_LO_8GNCACHE_LOC_VAR_1_IMAGE_EXTRACT_PREFIX="/var"

IL_LO_8GNCACHE_LOC_COREDUMP_1_PART=COREDUMP
IL_LO_8GNCACHE_LOC_COREDUMP_1_DIR=/
IL_LO_8GNCACHE_LOC_COREDUMP_1_IMAGE_EXTRACT="./coredump"
IL_LO_8GNCACHE_LOC_COREDUMP_1_IMAGE_EXTRACT_PREFIX="/coredump"

IL_LO_8GNCACHE_LOC_NKN_1_PART=NKN
IL_LO_8GNCACHE_LOC_NKN_1_DIR=/
IL_LO_8GNCACHE_LOC_NKN_1_IMAGE_EXTRACT="./nkn"
IL_LO_8GNCACHE_LOC_NKN_1_IMAGE_EXTRACT_PREFIX="/nkn"

IL_LO_8GNCACHE_LOC_LOG_1_PART=LOG
IL_LO_8GNCACHE_LOC_LOG_1_DIR=/
IL_LO_8GNCACHE_LOC_LOG_1_IMAGE_EXTRACT="./log"
IL_LO_8GNCACHE_LOC_LOG_1_IMAGE_EXTRACT_PREFIX="/log"

# Each of these says how a partition is named, labled, and sized.

IL_LO_8GNCACHE_PART_BOOTMGR_TARGET=DISK1
IL_LO_8GNCACHE_PART_BOOTMGR_PART_NUM=1
IL_LO_8GNCACHE_PART_BOOTMGR_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_BOOTMGR_PART_NUM}
IL_LO_8GNCACHE_PART_BOOTMGR_LABEL=BOOTMGR
IL_LO_8GNCACHE_PART_BOOTMGR_MOUNT=/bootmgr
IL_LO_8GNCACHE_PART_BOOTMGR_BOOTABLE=1
IL_LO_8GNCACHE_PART_BOOTMGR_PART_ID=83
IL_LO_8GNCACHE_PART_BOOTMGR_FSTYPE=ext3
IL_LO_8GNCACHE_PART_BOOTMGR_SIZE=30
IL_LO_8GNCACHE_PART_BOOTMGR_GROWTHCAP=
IL_LO_8GNCACHE_PART_BOOTMGR_GROWTHWEIGHT=
IL_LO_8GNCACHE_PART_BOOTMGR_OPTIONS="defaults,noatime,ro"

IL_LO_8GNCACHE_PART_BOOT1_TARGET=DISK1
IL_LO_8GNCACHE_PART_BOOT1_PART_NUM=2
IL_LO_8GNCACHE_PART_BOOT1_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_BOOT1_PART_NUM}
IL_LO_8GNCACHE_PART_BOOT1_LABEL=BOOT_1
IL_LO_8GNCACHE_PART_BOOT1_MOUNT=/boot
IL_LO_8GNCACHE_PART_BOOT1_BOOTABLE=
IL_LO_8GNCACHE_PART_BOOT1_PART_ID=83
IL_LO_8GNCACHE_PART_BOOT1_FSTYPE=ext3
IL_LO_8GNCACHE_PART_BOOT1_SIZE=30
IL_LO_8GNCACHE_PART_BOOT1_GROWTHCAP=
IL_LO_8GNCACHE_PART_BOOT1_GROWTHWEIGHT=
IL_LO_8GNCACHE_PART_BOOT1_OPTIONS="defaults,noatime,ro"

IL_LO_8GNCACHE_PART_BOOT2_TARGET=DISK1
IL_LO_8GNCACHE_PART_BOOT2_PART_NUM=3
IL_LO_8GNCACHE_PART_BOOT2_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_BOOT2_PART_NUM}
IL_LO_8GNCACHE_PART_BOOT2_LABEL=BOOT_2
IL_LO_8GNCACHE_PART_BOOT2_MOUNT=/boot
IL_LO_8GNCACHE_PART_BOOT2_BOOTABLE=
IL_LO_8GNCACHE_PART_BOOT2_PART_ID=83
IL_LO_8GNCACHE_PART_BOOT2_FSTYPE=ext3
IL_LO_8GNCACHE_PART_BOOT2_SIZE=30
IL_LO_8GNCACHE_PART_BOOT2_GROWTHCAP=
IL_LO_8GNCACHE_PART_BOOT2_GROWTHWEIGHT=
IL_LO_8GNCACHE_PART_BOOT2_OPTIONS="defaults,noatime,ro"

IL_LO_8GNCACHE_PART_EXT_TARGET=DISK1
IL_LO_8GNCACHE_PART_EXT_PART_NUM=4
IL_LO_8GNCACHE_PART_EXT_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_EXT_PART_NUM}
IL_LO_8GNCACHE_PART_EXT_LABEL=
IL_LO_8GNCACHE_PART_EXT_MOUNT=
IL_LO_8GNCACHE_PART_EXT_BOOTABLE=
IL_LO_8GNCACHE_PART_EXT_PART_ID=0f
IL_LO_8GNCACHE_PART_EXT_FSTYPE=
IL_LO_8GNCACHE_PART_EXT_SIZE=
IL_LO_8GNCACHE_PART_EXT_GROWTHCAP=
IL_LO_8GNCACHE_PART_EXT_GROWTHWEIGHT=

IL_LO_8GNCACHE_PART_ROOT1_TARGET=DISK1
IL_LO_8GNCACHE_PART_ROOT1_PART_NUM=5
IL_LO_8GNCACHE_PART_ROOT1_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_ROOT1_PART_NUM}
IL_LO_8GNCACHE_PART_ROOT1_LABEL=ROOT_1
IL_LO_8GNCACHE_PART_ROOT1_MOUNT=/
IL_LO_8GNCACHE_PART_ROOT1_BOOTABLE=
IL_LO_8GNCACHE_PART_ROOT1_PART_ID=83
IL_LO_8GNCACHE_PART_ROOT1_FSTYPE=ext3
IL_LO_8GNCACHE_PART_ROOT1_SIZE=2000
IL_LO_8GNCACHE_PART_ROOT1_GROWTHCAP=
IL_LO_8GNCACHE_PART_ROOT1_GROWTHWEIGHT=
IL_LO_8GNCACHE_PART_ROOT1_OPTIONS="defaults,noatime,ro"

IL_LO_8GNCACHE_PART_ROOT2_TARGET=DISK1
IL_LO_8GNCACHE_PART_ROOT2_PART_NUM=6
IL_LO_8GNCACHE_PART_ROOT2_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_ROOT2_PART_NUM}
IL_LO_8GNCACHE_PART_ROOT2_LABEL=ROOT_2
IL_LO_8GNCACHE_PART_ROOT2_MOUNT=/
IL_LO_8GNCACHE_PART_ROOT2_BOOTABLE=
IL_LO_8GNCACHE_PART_ROOT2_PART_ID=83
IL_LO_8GNCACHE_PART_ROOT2_FSTYPE=ext3
IL_LO_8GNCACHE_PART_ROOT2_SIZE=2000
IL_LO_8GNCACHE_PART_ROOT2_GROWTHCAP=
IL_LO_8GNCACHE_PART_ROOT2_GROWTHWEIGHT=
IL_LO_8GNCACHE_PART_ROOT2_OPTIONS="defaults,noatime,ro"

IL_LO_8GNCACHE_PART_CONFIG_TARGET=DISK1
IL_LO_8GNCACHE_PART_CONFIG_PART_NUM=7
IL_LO_8GNCACHE_PART_CONFIG_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_CONFIG_PART_NUM}
IL_LO_8GNCACHE_PART_CONFIG_LABEL=CONFIG
IL_LO_8GNCACHE_PART_CONFIG_MOUNT=/config
IL_LO_8GNCACHE_PART_CONFIG_BOOTABLE=
IL_LO_8GNCACHE_PART_CONFIG_PART_ID=83
IL_LO_8GNCACHE_PART_CONFIG_FSTYPE=ext3
IL_LO_8GNCACHE_PART_CONFIG_SIZE=64
IL_LO_8GNCACHE_PART_CONFIG_GROWTHCAP=
IL_LO_8GNCACHE_PART_CONFIG_GROWTHWEIGHT=
IL_LO_8GNCACHE_PART_CONFIG_OPTIONS="defaults,noatime"

IL_LO_8GNCACHE_PART_VAR_TARGET=DISK1
IL_LO_8GNCACHE_PART_VAR_PART_NUM=8
IL_LO_8GNCACHE_PART_VAR_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_VAR_PART_NUM}
IL_LO_8GNCACHE_PART_VAR_LABEL=VAR
IL_LO_8GNCACHE_PART_VAR_MOUNT=/var
IL_LO_8GNCACHE_PART_VAR_BOOTABLE=
IL_LO_8GNCACHE_PART_VAR_PART_ID=83
IL_LO_8GNCACHE_PART_VAR_FSTYPE=ext3
IL_LO_8GNCACHE_PART_VAR_SIZE=580
IL_LO_8GNCACHE_PART_VAR_GROWTHCAP=640
IL_LO_8GNCACHE_PART_VAR_GROWTHWEIGHT=10
IL_LO_8GNCACHE_PART_VAR_OPTIONS="defaults,noatime"

IL_LO_8GNCACHE_PART_COREDUMP_TARGET=DISK1
IL_LO_8GNCACHE_PART_COREDUMP_PART_NUM=9
IL_LO_8GNCACHE_PART_COREDUMP_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_COREDUMP_PART_NUM}
IL_LO_8GNCACHE_PART_COREDUMP_LABEL=COREDUMP
IL_LO_8GNCACHE_PART_COREDUMP_MOUNT=/coredump
IL_LO_8GNCACHE_PART_COREDUMP_BOOTABLE=
IL_LO_8GNCACHE_PART_COREDUMP_PART_ID=83
IL_LO_8GNCACHE_PART_COREDUMP_FSTYPE=ext3
IL_LO_8GNCACHE_PART_COREDUMP_SIZE=100
IL_LO_8GNCACHE_PART_COREDUMP_GROWTHCAP=
IL_LO_8GNCACHE_PART_COREDUMP_GROWTHWEIGHT=
IL_LO_8GNCACHE_PART_COREDUMP_OPTIONS="defaults,noatime"

IL_LO_8GNCACHE_PART_NKN_TARGET=DISK1
IL_LO_8GNCACHE_PART_NKN_PART_NUM=10
IL_LO_8GNCACHE_PART_NKN_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_NKN_PART_NUM}
IL_LO_8GNCACHE_PART_NKN_LABEL=NKN
IL_LO_8GNCACHE_PART_NKN_MOUNT=/nkn
IL_LO_8GNCACHE_PART_NKN_BOOTABLE=
IL_LO_8GNCACHE_PART_NKN_PART_ID=83
IL_LO_8GNCACHE_PART_NKN_FSTYPE=ext3
IL_LO_8GNCACHE_PART_NKN_SIZE=500
IL_LO_8GNCACHE_PART_NKN_GROWTHCAP=
IL_LO_8GNCACHE_PART_NKN_GROWTHWEIGHT=50
IL_LO_8GNCACHE_PART_NKN_OPTIONS="defaults,noatime"

IL_LO_8GNCACHE_PART_LOG_TARGET=DISK1
IL_LO_8GNCACHE_PART_LOG_PART_NUM=11
IL_LO_8GNCACHE_PART_LOG_DEV=${IL_LO_8GNCACHE_TARGET_DISK1_DEV}${IL_LO_8GNCACHE_PART_LOG_PART_NUM}
IL_LO_8GNCACHE_PART_LOG_LABEL=LOG
IL_LO_8GNCACHE_PART_LOG_MOUNT=/log
IL_LO_8GNCACHE_PART_LOG_BOOTABLE=
IL_LO_8GNCACHE_PART_LOG_PART_ID=83
IL_LO_8GNCACHE_PART_LOG_FSTYPE=ext3
IL_LO_8GNCACHE_PART_LOG_SIZE=500
IL_LO_8GNCACHE_PART_LOG_GROWTHCAP=
IL_LO_8GNCACHE_PART_LOG_GROWTHWEIGHT=50
IL_LO_8GNCACHE_PART_LOG_OPTIONS="defaults,noatime"

# ###########################################################################
# ###########################################################################
# ==========================================================
##PACIFICA: For pacifica board only
##PACIFICA: Root drive on RAMFS.
##PACIFICA: log on RAMFS.
##PACIFICA: Limit /nkn and /var to 50GB
# Max disk size for this is 100GB disk.
# Partitions :
#  1 : swap
#  2 : /config
#  3 : /var
#  4: Extended Partition
#  5 : /coredump
#  6 : /nkn
#  7 : dmfs
#  8 : dmraw
# ==========================================================

##Model: pacifica
IL_LO_PACIFICA_LOCS="ROOT_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1 DMFS_1 DMRAW_1"

# Targets are things like a disk or a flash device
IL_LO_PACIFICA_TARGET_NUM_TARGETS=1
IL_LO_PACIFICA_TARGETS="DISK1"
IL_LO_PACIFICA_TARGET_DISK1_NUM=1

# Configuration option to indicate that the root locations
# should be read-only. It is still necessary to indicate
# 'ro' in the partition option line for root.
IL_LO_PACIFICA_ROOT_RDONLY=1

IL_LO_PACIFICA_INITRD_PATH=
IL_LO_PACIFICA_ROOT_MOUNT_BY_LABEL=0

IL_LO_PACIFICA_TARGET_DISK1_MINSIZE=67000
IL_LO_PACIFICA_TARGET_DISK1_LABELTYPE=

# What partitions get made on the targets
IL_LO_PACIFICA_PARTS="ROOT1 SWAP EXT CONFIG VAR COREDUMP NKN LOG DMFS DMRAW"

IL_LO_PACIFICA_IMAGE_1_INSTALL_LOCS="ROOT_1"
IL_LO_PACIFICA_IMAGE_1_LOCS="ROOT_1 BOOT_1 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

IL_LO_PACIFICA_OPT_LAST_PART_FILL=1
IL_LO_PACIFICA_OPT_ALL_PARTS_FILL=1

IL_LO_PACIFICA_EXTRA_KERNEL_PARAMS="crashkernel=auto"

# Each of these says which PART (partition) below goes with which LOC
#     (standardized location) .  It also says for each LOC which subdirectory
#     under the PART the LOC is in, and what files to extract there

IL_LO_PACIFICA_LOC_ROOT_1_PART=ROOT1
IL_LO_PACIFICA_LOC_ROOT_1_DIR=/
IL_LO_PACIFICA_LOC_ROOT_1_IMAGE_EXTRACT="./"
IL_LO_PACIFICA_LOC_ROOT_1_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot"
IL_LO_PACIFICA_LOC_ROOT_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_PACIFICA_LOC_CONFIG_1_PART=CONFIG
IL_LO_PACIFICA_LOC_CONFIG_1_DIR=/
IL_LO_PACIFICA_LOC_CONFIG_1_IMAGE_EXTRACT="./config"
IL_LO_PACIFICA_LOC_CONFIG_1_IMAGE_EXTRACT_PREFIX="/config"

IL_LO_PACIFICA_LOC_VAR_1_PART=VAR
IL_LO_PACIFICA_LOC_VAR_1_DIR=/
IL_LO_PACIFICA_LOC_VAR_1_IMAGE_EXTRACT="./var"
IL_LO_PACIFICA_LOC_VAR_1_IMAGE_EXTRACT_PREFIX="/var"

IL_LO_PACIFICA_LOC_COREDUMP_1_PART=COREDUMP
IL_LO_PACIFICA_LOC_COREDUMP_1_DIR=/
IL_LO_PACIFICA_LOC_COREDUMP_1_IMAGE_EXTRACT="./coredump"
IL_LO_PACIFICA_LOC_COREDUMP_1_IMAGE_EXTRACT_PREFIX="/coredump"

IL_LO_PACIFICA_LOC_NKN_1_PART=NKN
IL_LO_PACIFICA_LOC_NKN_1_DIR=/
IL_LO_PACIFICA_LOC_NKN_1_IMAGE_EXTRACT="./nkn"
IL_LO_PACIFICA_LOC_NKN_1_IMAGE_EXTRACT_PREFIX="/nkn"

IL_LO_PACIFICA_LOC_LOG_1_PART=LOG
IL_LO_PACIFICA_LOC_LOG_1_DIR=/
IL_LO_PACIFICA_LOC_LOG_1_IMAGE_EXTRACT="./log"
IL_LO_PACIFICA_LOC_LOG_1_IMAGE_EXTRACT_PREFIX="/log"

IL_LO_PACIFICA_LOC_DMFS_1_PART=DMFS
IL_LO_PACIFICA_LOC_DMFS_1_DIR=/
IL_LO_PACIFICA_LOC_DMFS_1_IMAGE_EXTRACT=""
IL_LO_PACIFICA_LOC_DMFS_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_PACIFICA_LOC_DMRAW_1_PART=DMRAW
IL_LO_PACIFICA_LOC_DMRAW_1_DIR=/
IL_LO_PACIFICA_LOC_DMRAW_1_IMAGE_EXTRACT=""
IL_LO_PACIFICA_LOC_DMRAW_1_IMAGE_EXTRACT_PREFIX=""


# Each of these says how a partition is named, labled, and sized.

IL_LO_PACIFICA_PART_CONFIG_TARGET=DISK1
IL_LO_PACIFICA_PART_CONFIG_PART_NUM=2
IL_LO_PACIFICA_PART_CONFIG_DEV=${IL_LO_PACIFICA_TARGET_DISK1_DEV}${IL_LO_PACIFICA_PART_CONFIG_PART_NUM}
IL_LO_PACIFICA_PART_CONFIG_LABEL=CONFIG
IL_LO_PACIFICA_PART_CONFIG_MOUNT=/config
IL_LO_PACIFICA_PART_CONFIG_BOOTABLE=
IL_LO_PACIFICA_PART_CONFIG_PART_ID=83
IL_LO_PACIFICA_PART_CONFIG_FSTYPE=ext3
IL_LO_PACIFICA_PART_CONFIG_SIZE=256
IL_LO_PACIFICA_PART_CONFIG_GROWTHCAP=
IL_LO_PACIFICA_PART_CONFIG_GROWTHWEIGHT=

IL_LO_PACIFICA_PART_VAR_TARGET=DISK1
IL_LO_PACIFICA_PART_VAR_PART_NUM=3
IL_LO_PACIFICA_PART_VAR_DEV=${IL_LO_PACIFICA_TARGET_DISK1_DEV}${IL_LO_PACIFICA_PART_VAR_PART_NUM}
IL_LO_PACIFICA_PART_VAR_LABEL=VAR
IL_LO_PACIFICA_PART_VAR_MOUNT=/var
IL_LO_PACIFICA_PART_VAR_BOOTABLE=
IL_LO_PACIFICA_PART_VAR_PART_ID=83
IL_LO_PACIFICA_PART_VAR_FSTYPE=ext3
IL_LO_PACIFICA_PART_VAR_SIZE=1024
IL_LO_PACIFICA_PART_VAR_GROWTHCAP=
IL_LO_PACIFICA_PART_VAR_GROWTHWEIGHT=

IL_LO_PACIFICA_PART_EXT_TARGET=DISK1
IL_LO_PACIFICA_PART_EXT_PART_NUM=4
IL_LO_PACIFICA_PART_EXT_DEV=${IL_LO_PACIFICA_TARGET_DISK1_DEV}${IL_LO_PACIFICA_PART_EXT_PART_NUM}
IL_LO_PACIFICA_PART_EXT_LABEL=
IL_LO_PACIFICA_PART_EXT_MOUNT=
IL_LO_PACIFICA_PART_EXT_BOOTABLE=
IL_LO_PACIFICA_PART_EXT_PART_ID=0f
IL_LO_PACIFICA_PART_EXT_FSTYPE=
IL_LO_PACIFICA_PART_EXT_SIZE=
IL_LO_PACIFICA_PART_EXT_GROWTHCAP=
IL_LO_PACIFICA_PART_EXT_GROWTHWEIGHT=

IL_LO_PACIFICA_PART_COREDUMP_TARGET=DISK1
IL_LO_PACIFICA_PART_COREDUMP_PART_NUM=5
IL_LO_PACIFICA_PART_COREDUMP_DEV=${IL_LO_PACIFICA_TARGET_DISK1_DEV}${IL_LO_PACIFICA_PART_COREDUMP_PART_NUM}
IL_LO_PACIFICA_PART_COREDUMP_LABEL=COREDUMP
IL_LO_PACIFICA_PART_COREDUMP_MOUNT=/coredump
IL_LO_PACIFICA_PART_COREDUMP_BOOTABLE=
IL_LO_PACIFICA_PART_COREDUMP_PART_ID=83
IL_LO_PACIFICA_PART_COREDUMP_FSTYPE=ext3
IL_LO_PACIFICA_PART_COREDUMP_SIZE=40960
IL_LO_PACIFICA_PART_COREDUMP_GROWTHCAP=
IL_LO_PACIFICA_PART_COREDUMP_GROWTHWEIGHT=
IL_LO_PACIFICA_PART_COREDUMP_OPTIONS="defaults,noatime"

IL_LO_PACIFICA_PART_NKN_TARGET=DISK1
IL_LO_PACIFICA_PART_NKN_PART_NUM=6
IL_LO_PACIFICA_PART_NKN_DEV=${IL_LO_PACIFICA_TARGET_DISK1_DEV}${IL_LO_PACIFICA_PART_NKN_PART_NUM}
IL_LO_PACIFICA_PART_NKN_LABEL=NKN
IL_LO_PACIFICA_PART_NKN_MOUNT=/nkn
IL_LO_PACIFICA_PART_NKN_BOOTABLE=
IL_LO_PACIFICA_PART_NKN_PART_ID=83
IL_LO_PACIFICA_PART_NKN_FSTYPE=ext3
IL_LO_PACIFICA_PART_NKN_SIZE=20480
IL_LO_PACIFICA_PART_NKN_GROWTHCAP=
IL_LO_PACIFICA_PART_NKN_GROWTHWEIGHT=
IL_LO_PACIFICA_PART_NKN_OPTIONS="defaults,noatime"

IL_LO_PACIFICA_PART_DMFS_TARGET=DISK1
IL_LO_PACIFICA_PART_DMFS_PART_NUM=7
IL_LO_PACIFICA_PART_DMFS_DEV=${IL_LO_PACIFICA_TARGET_DISK1_DEV}${IL_LO_PACIFICA_PART_DMFS_PART_NUM}
IL_LO_PACIFICA_PART_DMFS_LABEL=DMFS
IL_LO_PACIFICA_PART_DMFS_MOUNT=
IL_LO_PACIFICA_PART_DMFS_BOOTABLE=
IL_LO_PACIFICA_PART_DMFS_PART_ID=83
  # For the dmfs parition set the FSTYPE to blank, so it will not create a new
  # filesystem on the partition, so we can preserve the contents.
  # Our install-mfd will do all the needed making of filesystems and labels,
  # because it knows when we want to preserve the data or not.
IL_LO_PACIFICA_PART_DMFS_FSTYPE=
IL_LO_PACIFICA_PART_DMFS_SIZE=300
IL_LO_PACIFICA_PART_DMFS_GROWTHCAP=
# We want this to be about 10% the size of DMRAW
IL_LO_PACIFICA_PART_DMFS_GROWTHWEIGHT=3

IL_LO_PACIFICA_PART_DMRAW_TARGET=DISK1
IL_LO_PACIFICA_PART_DMRAW_PART_NUM=8
IL_LO_PACIFICA_PART_DMRAW_DEV=${IL_LO_PACIFICA_TARGET_DISK1_DEV}${IL_LO_PACIFICA_PART_DMRAW_PART_NUM}
IL_LO_PACIFICA_PART_DMRAW_LABEL=DMRAW
IL_LO_PACIFICA_PART_DMRAW_MOUNT=
IL_LO_PACIFICA_PART_DMRAW_BOOTABLE=
IL_LO_PACIFICA_PART_DMRAW_PART_ID=
IL_LO_PACIFICA_PART_DMRAW_FSTYPE=
IL_LO_PACIFICA_PART_DMRAW_SIZE=3000
IL_LO_PACIFICA_PART_DMRAW_GROWTHCAP=
IL_LO_PACIFICA_PART_DMRAW_GROWTHWEIGHT=30


# ###########################################################################
# ###########################################################################
# ==========================================================
##CLOUDVM: No cache on the root drive.
##CLOUDVM: Minimal log and coredump.
##CLOUDVM: /nkn takes 100% extra space
# Partitions :
#  1 : /bootmgr
#  2 : /boot (1 of 2)
#  3 : /boot (2 of 2) - 2nd boot image
#  4 : extended partition
#  5 : / (1 of 2)
#  6 : / (2 of 2) - for 2nd boot image
#  7 : /config
#  8 : /var
#  9 : /coredump
# 10 : /nkn
# 11 : /log
# ==========================================================

##Model: cloudvm -> CLOUDVM
IL_LO_CLOUDVM_LOCS="BOOTMGR_1 BOOT_1 BOOT_2 ROOT_1 ROOT_2 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

# Targets are things like a disk or a flash device
IL_LO_CLOUDVM_TARGET_NUM_TARGETS=1
IL_LO_CLOUDVM_TARGETS="DISK1"
IL_LO_CLOUDVM_TARGET_DISK1_NUM=1
IL_LO_CLOUDVM_ROOT_RDONLY=1
IL_LO_CLOUDVM_INITRD_PATH=
IL_LO_CLOUDVM_ROOT_MOUNT_BY_LABEL=0
IL_LO_CLOUDVM_TARGET_DISK1_MINSIZE=14000
IL_LO_CLOUDVM_TARGET_DISK1_LABELTYPE=

# What partitions get made on the targets
IL_LO_CLOUDVM_PARTS="BOOTMGR BOOT1 BOOT2 EXT ROOT1 ROOT2 CONFIG VAR COREDUMP NKN LOG"

IL_LO_CLOUDVM_IMAGE_1_INSTALL_LOCS="BOOT_1 ROOT_1"
IL_LO_CLOUDVM_IMAGE_2_INSTALL_LOCS="BOOT_2 ROOT_2"
IL_LO_CLOUDVM_IMAGE_1_LOCS="ROOT_1 BOOT_1 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"
IL_LO_CLOUDVM_IMAGE_2_LOCS="ROOT_2 BOOT_2 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

IL_LO_CLOUDVM_OPT_LAST_PART_FILL=1
IL_LO_CLOUDVM_OPT_ALL_PARTS_FILL=1

IL_LO_CLOUDVM_EXTRA_KERNEL_PARAMS="crashkernel=auto"

# Each of these says which PART (partition) below goes with which LOC
#     (standardized location) .  It also says for each LOC which subdirectory
#     under the PART the LOC is in, and what files to extract there

IL_LO_CLOUDVM_LOC_BOOTMGR_1_PART=BOOTMGR
IL_LO_CLOUDVM_LOC_BOOTMGR_1_DIR=/
IL_LO_CLOUDVM_LOC_BOOTMGR_1_IMAGE_EXTRACT="./bootmgr"
IL_LO_CLOUDVM_LOC_BOOTMGR_1_IMAGE_EXTRACT_PREFIX="/bootmgr"

IL_LO_CLOUDVM_LOC_BOOT_1_PART=BOOT1
IL_LO_CLOUDVM_LOC_BOOT_1_DIR=/
IL_LO_CLOUDVM_LOC_BOOT_1_IMAGE_EXTRACT="./boot"
IL_LO_CLOUDVM_LOC_BOOT_1_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_CLOUDVM_LOC_BOOT_2_PART=BOOT2
IL_LO_CLOUDVM_LOC_BOOT_2_DIR=/
IL_LO_CLOUDVM_LOC_BOOT_2_IMAGE_EXTRACT="./boot"
IL_LO_CLOUDVM_LOC_BOOT_2_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_CLOUDVM_LOC_ROOT_1_PART=ROOT1
IL_LO_CLOUDVM_LOC_ROOT_1_DIR=/
IL_LO_CLOUDVM_LOC_ROOT_1_IMAGE_EXTRACT="./"
IL_LO_CLOUDVM_LOC_ROOT_1_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_CLOUDVM_LOC_ROOT_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_CLOUDVM_LOC_ROOT_2_PART=ROOT2
IL_LO_CLOUDVM_LOC_ROOT_2_DIR=/
IL_LO_CLOUDVM_LOC_ROOT_2_IMAGE_EXTRACT="./"
IL_LO_CLOUDVM_LOC_ROOT_2_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_CLOUDVM_LOC_ROOT_2_IMAGE_EXTRACT_PREFIX=""

IL_LO_CLOUDVM_LOC_CONFIG_1_PART=CONFIG
IL_LO_CLOUDVM_LOC_CONFIG_1_DIR=/
IL_LO_CLOUDVM_LOC_CONFIG_1_IMAGE_EXTRACT="./config"
IL_LO_CLOUDVM_LOC_CONFIG_1_IMAGE_EXTRACT_PREFIX="/config"

IL_LO_CLOUDVM_LOC_VAR_1_PART=VAR
IL_LO_CLOUDVM_LOC_VAR_1_DIR=/
IL_LO_CLOUDVM_LOC_VAR_1_IMAGE_EXTRACT="./var"
IL_LO_CLOUDVM_LOC_VAR_1_IMAGE_EXTRACT_PREFIX="/var"

IL_LO_CLOUDVM_LOC_COREDUMP_1_PART=COREDUMP
IL_LO_CLOUDVM_LOC_COREDUMP_1_DIR=/
IL_LO_CLOUDVM_LOC_COREDUMP_1_IMAGE_EXTRACT="./coredump"
IL_LO_CLOUDVM_LOC_COREDUMP_1_IMAGE_EXTRACT_PREFIX="/coredump"

IL_LO_CLOUDVM_LOC_NKN_1_PART=NKN
IL_LO_CLOUDVM_LOC_NKN_1_DIR=/
IL_LO_CLOUDVM_LOC_NKN_1_IMAGE_EXTRACT="./nkn"
IL_LO_CLOUDVM_LOC_NKN_1_IMAGE_EXTRACT_PREFIX="/nkn"

IL_LO_CLOUDVM_LOC_LOG_1_PART=LOG
IL_LO_CLOUDVM_LOC_LOG_1_DIR=/
IL_LO_CLOUDVM_LOC_LOG_1_IMAGE_EXTRACT="./log"
IL_LO_CLOUDVM_LOC_LOG_1_IMAGE_EXTRACT_PREFIX="/log"

# Each of these says how a partition is named, labled, and sized.

IL_LO_CLOUDVM_PART_BOOTMGR_TARGET=DISK1
IL_LO_CLOUDVM_PART_BOOTMGR_PART_NUM=1
IL_LO_CLOUDVM_PART_BOOTMGR_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_BOOTMGR_PART_NUM}
IL_LO_CLOUDVM_PART_BOOTMGR_LABEL=BOOTMGR
IL_LO_CLOUDVM_PART_BOOTMGR_MOUNT=/bootmgr
IL_LO_CLOUDVM_PART_BOOTMGR_BOOTABLE=1
IL_LO_CLOUDVM_PART_BOOTMGR_PART_ID=83
IL_LO_CLOUDVM_PART_BOOTMGR_FSTYPE=ext3
IL_LO_CLOUDVM_PART_BOOTMGR_SIZE=128
IL_LO_CLOUDVM_PART_BOOTMGR_GROWTHCAP=
IL_LO_CLOUDVM_PART_BOOTMGR_GROWTHWEIGHT=
IL_LO_CLOUDVM_PART_BOOTMGR_OPTIONS="defaults,noatime,ro"

IL_LO_CLOUDVM_PART_BOOT1_TARGET=DISK1
IL_LO_CLOUDVM_PART_BOOT1_PART_NUM=2
IL_LO_CLOUDVM_PART_BOOT1_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_BOOT1_PART_NUM}
IL_LO_CLOUDVM_PART_BOOT1_LABEL=BOOT_1
IL_LO_CLOUDVM_PART_BOOT1_MOUNT=/boot
IL_LO_CLOUDVM_PART_BOOT1_BOOTABLE=
IL_LO_CLOUDVM_PART_BOOT1_PART_ID=83
IL_LO_CLOUDVM_PART_BOOT1_FSTYPE=ext3
IL_LO_CLOUDVM_PART_BOOT1_SIZE=128
IL_LO_CLOUDVM_PART_BOOT1_GROWTHCAP=
IL_LO_CLOUDVM_PART_BOOT1_GROWTHWEIGHT=
IL_LO_CLOUDVM_PART_BOOT1_OPTIONS="defaults,noatime,ro"

IL_LO_CLOUDVM_PART_BOOT2_TARGET=DISK1
IL_LO_CLOUDVM_PART_BOOT2_PART_NUM=3
IL_LO_CLOUDVM_PART_BOOT2_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_BOOT2_PART_NUM}
IL_LO_CLOUDVM_PART_BOOT2_LABEL=BOOT_2
IL_LO_CLOUDVM_PART_BOOT2_MOUNT=/boot
IL_LO_CLOUDVM_PART_BOOT2_BOOTABLE=
IL_LO_CLOUDVM_PART_BOOT2_PART_ID=83
IL_LO_CLOUDVM_PART_BOOT2_FSTYPE=ext3
IL_LO_CLOUDVM_PART_BOOT2_SIZE=128
IL_LO_CLOUDVM_PART_BOOT2_GROWTHCAP=
IL_LO_CLOUDVM_PART_BOOT2_GROWTHWEIGHT=
IL_LO_CLOUDVM_PART_BOOT2_OPTIONS="defaults,noatime,ro"

IL_LO_CLOUDVM_PART_EXT_TARGET=DISK1
IL_LO_CLOUDVM_PART_EXT_PART_NUM=4
IL_LO_CLOUDVM_PART_EXT_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_EXT_PART_NUM}
IL_LO_CLOUDVM_PART_EXT_LABEL=
IL_LO_CLOUDVM_PART_EXT_MOUNT=
IL_LO_CLOUDVM_PART_EXT_BOOTABLE=
IL_LO_CLOUDVM_PART_EXT_PART_ID=0f
IL_LO_CLOUDVM_PART_EXT_FSTYPE=
IL_LO_CLOUDVM_PART_EXT_SIZE=
IL_LO_CLOUDVM_PART_EXT_GROWTHCAP=
IL_LO_CLOUDVM_PART_EXT_GROWTHWEIGHT=

IL_LO_CLOUDVM_PART_ROOT1_TARGET=DISK1
IL_LO_CLOUDVM_PART_ROOT1_PART_NUM=5
IL_LO_CLOUDVM_PART_ROOT1_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_ROOT1_PART_NUM}
IL_LO_CLOUDVM_PART_ROOT1_LABEL=ROOT_1
IL_LO_CLOUDVM_PART_ROOT1_MOUNT=/
IL_LO_CLOUDVM_PART_ROOT1_BOOTABLE=
IL_LO_CLOUDVM_PART_ROOT1_PART_ID=83
IL_LO_CLOUDVM_PART_ROOT1_FSTYPE=ext3
IL_LO_CLOUDVM_PART_ROOT1_SIZE=2000
IL_LO_CLOUDVM_PART_ROOT1_GROWTHCAP=
IL_LO_CLOUDVM_PART_ROOT1_GROWTHWEIGHT=
IL_LO_CLOUDVM_PART_ROOT1_OPTIONS="defaults,noatime,ro"

IL_LO_CLOUDVM_PART_ROOT2_TARGET=DISK1
IL_LO_CLOUDVM_PART_ROOT2_PART_NUM=6
IL_LO_CLOUDVM_PART_ROOT2_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_ROOT2_PART_NUM}
IL_LO_CLOUDVM_PART_ROOT2_LABEL=ROOT_2
IL_LO_CLOUDVM_PART_ROOT2_MOUNT=/
IL_LO_CLOUDVM_PART_ROOT2_BOOTABLE=
IL_LO_CLOUDVM_PART_ROOT2_PART_ID=83
IL_LO_CLOUDVM_PART_ROOT2_FSTYPE=ext3
IL_LO_CLOUDVM_PART_ROOT2_SIZE=2000
IL_LO_CLOUDVM_PART_ROOT2_GROWTHCAP=
IL_LO_CLOUDVM_PART_ROOT2_GROWTHWEIGHT=
IL_LO_CLOUDVM_PART_ROOT2_OPTIONS="defaults,noatime,ro"

IL_LO_CLOUDVM_PART_CONFIG_TARGET=DISK1
IL_LO_CLOUDVM_PART_CONFIG_PART_NUM=7
IL_LO_CLOUDVM_PART_CONFIG_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_CONFIG_PART_NUM}
IL_LO_CLOUDVM_PART_CONFIG_LABEL=CONFIG
IL_LO_CLOUDVM_PART_CONFIG_MOUNT=/config
IL_LO_CLOUDVM_PART_CONFIG_BOOTABLE=
IL_LO_CLOUDVM_PART_CONFIG_PART_ID=83
IL_LO_CLOUDVM_PART_CONFIG_FSTYPE=ext3
IL_LO_CLOUDVM_PART_CONFIG_SIZE=256
IL_LO_CLOUDVM_PART_CONFIG_GROWTHCAP=
IL_LO_CLOUDVM_PART_CONFIG_GROWTHWEIGHT=

IL_LO_CLOUDVM_PART_VAR_TARGET=DISK1
IL_LO_CLOUDVM_PART_VAR_PART_NUM=8
IL_LO_CLOUDVM_PART_VAR_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_VAR_PART_NUM}
IL_LO_CLOUDVM_PART_VAR_LABEL=VAR
IL_LO_CLOUDVM_PART_VAR_MOUNT=/var
IL_LO_CLOUDVM_PART_VAR_BOOTABLE=
IL_LO_CLOUDVM_PART_VAR_PART_ID=83
IL_LO_CLOUDVM_PART_VAR_FSTYPE=ext3
IL_LO_CLOUDVM_PART_VAR_SIZE=3072
IL_LO_CLOUDVM_PART_VAR_GROWTHCAP=
IL_LO_CLOUDVM_PART_VAR_GROWTHWEIGHT=

# On a normally running vMFC, the coredump will put onto another drive at
# run time, so just have a very small coredump partition on the root drive.
IL_LO_CLOUDVM_PART_COREDUMP_TARGET=DISK1
IL_LO_CLOUDVM_PART_COREDUMP_PART_NUM=9
IL_LO_CLOUDVM_PART_COREDUMP_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_COREDUMP_PART_NUM}
IL_LO_CLOUDVM_PART_COREDUMP_LABEL=COREDUMP
IL_LO_CLOUDVM_PART_COREDUMP_MOUNT=/coredump
IL_LO_CLOUDVM_PART_COREDUMP_BOOTABLE=
IL_LO_CLOUDVM_PART_COREDUMP_PART_ID=83
IL_LO_CLOUDVM_PART_COREDUMP_FSTYPE=ext3
IL_LO_CLOUDVM_PART_COREDUMP_SIZE=100
IL_LO_CLOUDVM_PART_COREDUMP_GROWTHCAP=
IL_LO_CLOUDVM_PART_COREDUMP_GROWTHWEIGHT=
IL_LO_CLOUDVM_PART_COREDUMP_OPTIONS="defaults,noatime"

# The nkn partition will grow to take all the extra space.
IL_LO_CLOUDVM_PART_NKN_TARGET=DISK1
IL_LO_CLOUDVM_PART_NKN_PART_NUM=10
IL_LO_CLOUDVM_PART_NKN_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_NKN_PART_NUM}
IL_LO_CLOUDVM_PART_NKN_LABEL=NKN
IL_LO_CLOUDVM_PART_NKN_MOUNT=/nkn
IL_LO_CLOUDVM_PART_NKN_BOOTABLE=
IL_LO_CLOUDVM_PART_NKN_PART_ID=83
IL_LO_CLOUDVM_PART_NKN_FSTYPE=ext3
IL_LO_CLOUDVM_PART_NKN_SIZE=5000
IL_LO_CLOUDVM_PART_NKN_GROWTHCAP=
IL_LO_CLOUDVM_PART_NKN_GROWTHWEIGHT=100
IL_LO_CLOUDVM_PART_NKN_OPTIONS="defaults,noatime"

# On a normally running vMFC, the log will put onto another drive at
# run time, so just have a very small log partition on the root drive.
IL_LO_CLOUDVM_PART_LOG_TARGET=DISK1
IL_LO_CLOUDVM_PART_LOG_PART_NUM=11
IL_LO_CLOUDVM_PART_LOG_DEV=${IL_LO_CLOUDVM_TARGET_DISK1_DEV}${IL_LO_CLOUDVM_PART_LOG_PART_NUM}
IL_LO_CLOUDVM_PART_LOG_LABEL=LOG
IL_LO_CLOUDVM_PART_LOG_MOUNT=/log
IL_LO_CLOUDVM_PART_LOG_BOOTABLE=
IL_LO_CLOUDVM_PART_LOG_PART_ID=83
IL_LO_CLOUDVM_PART_LOG_FSTYPE=ext3
IL_LO_CLOUDVM_PART_LOG_SIZE=500
IL_LO_CLOUDVM_PART_LOG_GROWTHCAP=
IL_LO_CLOUDVM_PART_LOG_GROWTHWEIGHT=
IL_LO_CLOUDVM_PART_LOG_OPTIONS="defaults,noatime"


# ###########################################################################
# ###########################################################################
# ==========================================================
##CLOUDRC: Cache on the root drive. (RC means Root Cache)
##CLOUDRC: Minimal coredump, normal sized log partition..
##CLOUDRC: nkn, log and root cache take extra space
# Partitions :
#  1 : /bootmgr
#  2 : /boot (1 of 2)
#  3 : /boot (2 of 2) - 2nd boot image
#  4 : extended partition
#  5 : / (1 of 2)
#  6 : / (2 of 2) - for 2nd boot image
#  7 : /config
#  8 : /var
#  9 : /coredump
# 10 : /nkn
# 11 : /log
# 12 : dmfs
# 13 : dmraw
# ==========================================================

##Model: cloudrc -> CLOUDRC
IL_LO_CLOUDRC_LOCS="BOOTMGR_1 BOOT_1 BOOT_2 ROOT_1 ROOT_2 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1 DMFS_1 DMRAW_1"

# Targets are things like a disk or a flash device
IL_LO_CLOUDRC_TARGET_NUM_TARGETS=1
IL_LO_CLOUDRC_TARGETS="DISK1"
IL_LO_CLOUDRC_TARGET_DISK1_NUM=1
IL_LO_CLOUDRC_ROOT_RDONLY=1
IL_LO_CLOUDRC_INITRD_PATH=
IL_LO_CLOUDRC_ROOT_MOUNT_BY_LABEL=0
IL_LO_CLOUDRC_TARGET_DISK1_MINSIZE=22000
IL_LO_CLOUDRC_TARGET_DISK1_LABELTYPE=

# What partitions get made on the targets
IL_LO_CLOUDRC_PARTS="BOOTMGR BOOT1 BOOT2 EXT ROOT1 ROOT2 CONFIG VAR COREDUMP NKN LOG DMFS DMRAW"

IL_LO_CLOUDRC_IMAGE_1_INSTALL_LOCS="BOOT_1 ROOT_1"
IL_LO_CLOUDRC_IMAGE_2_INSTALL_LOCS="BOOT_2 ROOT_2"
IL_LO_CLOUDRC_IMAGE_1_LOCS="ROOT_1 BOOT_1 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"
IL_LO_CLOUDRC_IMAGE_2_LOCS="ROOT_2 BOOT_2 BOOTMGR_1 CONFIG_1 VAR_1 COREDUMP_1 NKN_1 LOG_1"

IL_LO_CLOUDRC_OPT_LAST_PART_FILL=1
IL_LO_CLOUDRC_OPT_ALL_PARTS_FILL=1

IL_LO_CLOUDRC_EXTRA_KERNEL_PARAMS="crashkernel=auto"

# Each of these says which PART (partition) below goes with which LOC
#     (standardized location) .  It also says for each LOC which subdirectory
#     under the PART the LOC is in, and what files to extract there

IL_LO_CLOUDRC_LOC_BOOTMGR_1_PART=BOOTMGR
IL_LO_CLOUDRC_LOC_BOOTMGR_1_DIR=/
IL_LO_CLOUDRC_LOC_BOOTMGR_1_IMAGE_EXTRACT="./bootmgr"
IL_LO_CLOUDRC_LOC_BOOTMGR_1_IMAGE_EXTRACT_PREFIX="/bootmgr"

IL_LO_CLOUDRC_LOC_BOOT_1_PART=BOOT1
IL_LO_CLOUDRC_LOC_BOOT_1_DIR=/
IL_LO_CLOUDRC_LOC_BOOT_1_IMAGE_EXTRACT="./boot"
IL_LO_CLOUDRC_LOC_BOOT_1_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_CLOUDRC_LOC_BOOT_2_PART=BOOT2
IL_LO_CLOUDRC_LOC_BOOT_2_DIR=/
IL_LO_CLOUDRC_LOC_BOOT_2_IMAGE_EXTRACT="./boot"
IL_LO_CLOUDRC_LOC_BOOT_2_IMAGE_EXTRACT_PREFIX="/boot"

IL_LO_CLOUDRC_LOC_ROOT_1_PART=ROOT1
IL_LO_CLOUDRC_LOC_ROOT_1_DIR=/
IL_LO_CLOUDRC_LOC_ROOT_1_IMAGE_EXTRACT="./"
IL_LO_CLOUDRC_LOC_ROOT_1_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_CLOUDRC_LOC_ROOT_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_CLOUDRC_LOC_ROOT_2_PART=ROOT2
IL_LO_CLOUDRC_LOC_ROOT_2_DIR=/
IL_LO_CLOUDRC_LOC_ROOT_2_IMAGE_EXTRACT="./"
IL_LO_CLOUDRC_LOC_ROOT_2_IMAGE_EXTRACT_EXCLUDE="./bootmgr ./boot ./config ./var ./nkn/* ./coredump/*"
IL_LO_CLOUDRC_LOC_ROOT_2_IMAGE_EXTRACT_PREFIX=""

IL_LO_CLOUDRC_LOC_CONFIG_1_PART=CONFIG
IL_LO_CLOUDRC_LOC_CONFIG_1_DIR=/
IL_LO_CLOUDRC_LOC_CONFIG_1_IMAGE_EXTRACT="./config"
IL_LO_CLOUDRC_LOC_CONFIG_1_IMAGE_EXTRACT_PREFIX="/config"

IL_LO_CLOUDRC_LOC_VAR_1_PART=VAR
IL_LO_CLOUDRC_LOC_VAR_1_DIR=/
IL_LO_CLOUDRC_LOC_VAR_1_IMAGE_EXTRACT="./var"
IL_LO_CLOUDRC_LOC_VAR_1_IMAGE_EXTRACT_PREFIX="/var"

IL_LO_CLOUDRC_LOC_COREDUMP_1_PART=COREDUMP
IL_LO_CLOUDRC_LOC_COREDUMP_1_DIR=/
IL_LO_CLOUDRC_LOC_COREDUMP_1_IMAGE_EXTRACT="./coredump"
IL_LO_CLOUDRC_LOC_COREDUMP_1_IMAGE_EXTRACT_PREFIX="/coredump"

IL_LO_CLOUDRC_LOC_NKN_1_PART=NKN
IL_LO_CLOUDRC_LOC_NKN_1_DIR=/
IL_LO_CLOUDRC_LOC_NKN_1_IMAGE_EXTRACT="./nkn"
IL_LO_CLOUDRC_LOC_NKN_1_IMAGE_EXTRACT_PREFIX="/nkn"

IL_LO_CLOUDRC_LOC_LOG_1_PART=LOG
IL_LO_CLOUDRC_LOC_LOG_1_DIR=/
IL_LO_CLOUDRC_LOC_LOG_1_IMAGE_EXTRACT="./log"
IL_LO_CLOUDRC_LOC_LOG_1_IMAGE_EXTRACT_PREFIX="/log"

IL_LO_CLOUDRC_LOC_DMFS_1_PART=DMFS
IL_LO_CLOUDRC_LOC_DMFS_1_DIR=/
IL_LO_CLOUDRC_LOC_DMFS_1_IMAGE_EXTRACT=""
IL_LO_CLOUDRC_LOC_DMFS_1_IMAGE_EXTRACT_PREFIX=""

IL_LO_CLOUDRC_LOC_DMRAW_1_PART=DMRAW
IL_LO_CLOUDRC_LOC_DMRAW_1_DIR=/
IL_LO_CLOUDRC_LOC_DMRAW_1_IMAGE_EXTRACT=""
IL_LO_CLOUDRC_LOC_DMRAW_1_IMAGE_EXTRACT_PREFIX=""

# Each of these says how a partition is named, labled, and sized.

IL_LO_CLOUDRC_PART_BOOTMGR_TARGET=DISK1
IL_LO_CLOUDRC_PART_BOOTMGR_PART_NUM=1
IL_LO_CLOUDRC_PART_BOOTMGR_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_BOOTMGR_PART_NUM}
IL_LO_CLOUDRC_PART_BOOTMGR_LABEL=BOOTMGR
IL_LO_CLOUDRC_PART_BOOTMGR_MOUNT=/bootmgr
IL_LO_CLOUDRC_PART_BOOTMGR_BOOTABLE=1
IL_LO_CLOUDRC_PART_BOOTMGR_PART_ID=83
IL_LO_CLOUDRC_PART_BOOTMGR_FSTYPE=ext3
IL_LO_CLOUDRC_PART_BOOTMGR_SIZE=128
IL_LO_CLOUDRC_PART_BOOTMGR_GROWTHCAP=
IL_LO_CLOUDRC_PART_BOOTMGR_GROWTHWEIGHT=
IL_LO_CLOUDRC_PART_BOOTMGR_OPTIONS="defaults,noatime,ro"

IL_LO_CLOUDRC_PART_BOOT1_TARGET=DISK1
IL_LO_CLOUDRC_PART_BOOT1_PART_NUM=2
IL_LO_CLOUDRC_PART_BOOT1_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_BOOT1_PART_NUM}
IL_LO_CLOUDRC_PART_BOOT1_LABEL=BOOT_1
IL_LO_CLOUDRC_PART_BOOT1_MOUNT=/boot
IL_LO_CLOUDRC_PART_BOOT1_BOOTABLE=
IL_LO_CLOUDRC_PART_BOOT1_PART_ID=83
IL_LO_CLOUDRC_PART_BOOT1_FSTYPE=ext3
IL_LO_CLOUDRC_PART_BOOT1_SIZE=128
IL_LO_CLOUDRC_PART_BOOT1_GROWTHCAP=
IL_LO_CLOUDRC_PART_BOOT1_GROWTHWEIGHT=
IL_LO_CLOUDRC_PART_BOOT1_OPTIONS="defaults,noatime,ro"

IL_LO_CLOUDRC_PART_BOOT2_TARGET=DISK1
IL_LO_CLOUDRC_PART_BOOT2_PART_NUM=3
IL_LO_CLOUDRC_PART_BOOT2_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_BOOT2_PART_NUM}
IL_LO_CLOUDRC_PART_BOOT2_LABEL=BOOT_2
IL_LO_CLOUDRC_PART_BOOT2_MOUNT=/boot
IL_LO_CLOUDRC_PART_BOOT2_BOOTABLE=
IL_LO_CLOUDRC_PART_BOOT2_PART_ID=83
IL_LO_CLOUDRC_PART_BOOT2_FSTYPE=ext3
IL_LO_CLOUDRC_PART_BOOT2_SIZE=128
IL_LO_CLOUDRC_PART_BOOT2_GROWTHCAP=
IL_LO_CLOUDRC_PART_BOOT2_GROWTHWEIGHT=
IL_LO_CLOUDRC_PART_BOOT2_OPTIONS="defaults,noatime,ro"

IL_LO_CLOUDRC_PART_EXT_TARGET=DISK1
IL_LO_CLOUDRC_PART_EXT_PART_NUM=4
IL_LO_CLOUDRC_PART_EXT_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_EXT_PART_NUM}
IL_LO_CLOUDRC_PART_EXT_LABEL=
IL_LO_CLOUDRC_PART_EXT_MOUNT=
IL_LO_CLOUDRC_PART_EXT_BOOTABLE=
IL_LO_CLOUDRC_PART_EXT_PART_ID=0f
IL_LO_CLOUDRC_PART_EXT_FSTYPE=
IL_LO_CLOUDRC_PART_EXT_SIZE=
IL_LO_CLOUDRC_PART_EXT_GROWTHCAP=
IL_LO_CLOUDRC_PART_EXT_GROWTHWEIGHT=

IL_LO_CLOUDRC_PART_ROOT1_TARGET=DISK1
IL_LO_CLOUDRC_PART_ROOT1_PART_NUM=5
IL_LO_CLOUDRC_PART_ROOT1_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_ROOT1_PART_NUM}
IL_LO_CLOUDRC_PART_ROOT1_LABEL=ROOT_1
IL_LO_CLOUDRC_PART_ROOT1_MOUNT=/
IL_LO_CLOUDRC_PART_ROOT1_BOOTABLE=
IL_LO_CLOUDRC_PART_ROOT1_PART_ID=83
IL_LO_CLOUDRC_PART_ROOT1_FSTYPE=ext3
IL_LO_CLOUDRC_PART_ROOT1_SIZE=2000
IL_LO_CLOUDRC_PART_ROOT1_GROWTHCAP=
IL_LO_CLOUDRC_PART_ROOT1_GROWTHWEIGHT=
IL_LO_CLOUDRC_PART_ROOT1_OPTIONS="defaults,noatime,ro"

IL_LO_CLOUDRC_PART_ROOT2_TARGET=DISK1
IL_LO_CLOUDRC_PART_ROOT2_PART_NUM=6
IL_LO_CLOUDRC_PART_ROOT2_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_ROOT2_PART_NUM}
IL_LO_CLOUDRC_PART_ROOT2_LABEL=ROOT_2
IL_LO_CLOUDRC_PART_ROOT2_MOUNT=/
IL_LO_CLOUDRC_PART_ROOT2_BOOTABLE=
IL_LO_CLOUDRC_PART_ROOT2_PART_ID=83
IL_LO_CLOUDRC_PART_ROOT2_FSTYPE=ext3
IL_LO_CLOUDRC_PART_ROOT2_SIZE=2000
IL_LO_CLOUDRC_PART_ROOT2_GROWTHCAP=
IL_LO_CLOUDRC_PART_ROOT2_GROWTHWEIGHT=
IL_LO_CLOUDRC_PART_ROOT2_OPTIONS="defaults,noatime,ro"

IL_LO_CLOUDRC_PART_CONFIG_TARGET=DISK1
IL_LO_CLOUDRC_PART_CONFIG_PART_NUM=7
IL_LO_CLOUDRC_PART_CONFIG_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_CONFIG_PART_NUM}
IL_LO_CLOUDRC_PART_CONFIG_LABEL=CONFIG
IL_LO_CLOUDRC_PART_CONFIG_MOUNT=/config
IL_LO_CLOUDRC_PART_CONFIG_BOOTABLE=
IL_LO_CLOUDRC_PART_CONFIG_PART_ID=83
IL_LO_CLOUDRC_PART_CONFIG_FSTYPE=ext3
IL_LO_CLOUDRC_PART_CONFIG_SIZE=256
IL_LO_CLOUDRC_PART_CONFIG_GROWTHCAP=
IL_LO_CLOUDRC_PART_CONFIG_GROWTHWEIGHT=

IL_LO_CLOUDRC_PART_VAR_TARGET=DISK1
IL_LO_CLOUDRC_PART_VAR_PART_NUM=8
IL_LO_CLOUDRC_PART_VAR_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_VAR_PART_NUM}
IL_LO_CLOUDRC_PART_VAR_LABEL=VAR
IL_LO_CLOUDRC_PART_VAR_MOUNT=/var
IL_LO_CLOUDRC_PART_VAR_BOOTABLE=
IL_LO_CLOUDRC_PART_VAR_PART_ID=83
IL_LO_CLOUDRC_PART_VAR_FSTYPE=ext3
IL_LO_CLOUDRC_PART_VAR_SIZE=3072
IL_LO_CLOUDRC_PART_VAR_GROWTHCAP=
IL_LO_CLOUDRC_PART_VAR_GROWTHWEIGHT=

# On a normally running vMFC, the coredump will put onto another drive at
# run time, so just have a very small coredump partition on the root drive.
IL_LO_CLOUDRC_PART_COREDUMP_TARGET=DISK1
IL_LO_CLOUDRC_PART_COREDUMP_PART_NUM=9
IL_LO_CLOUDRC_PART_COREDUMP_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_COREDUMP_PART_NUM}
IL_LO_CLOUDRC_PART_COREDUMP_LABEL=COREDUMP
IL_LO_CLOUDRC_PART_COREDUMP_MOUNT=/coredump
IL_LO_CLOUDRC_PART_COREDUMP_BOOTABLE=
IL_LO_CLOUDRC_PART_COREDUMP_PART_ID=83
IL_LO_CLOUDRC_PART_COREDUMP_FSTYPE=ext3
IL_LO_CLOUDRC_PART_COREDUMP_SIZE=100
IL_LO_CLOUDRC_PART_COREDUMP_GROWTHCAP=
IL_LO_CLOUDRC_PART_COREDUMP_GROWTHWEIGHT=
IL_LO_CLOUDRC_PART_COREDUMP_OPTIONS="defaults,noatime"

# The nkn partition will grow to take all the extra space.
IL_LO_CLOUDRC_PART_NKN_TARGET=DISK1
IL_LO_CLOUDRC_PART_NKN_PART_NUM=10
IL_LO_CLOUDRC_PART_NKN_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_NKN_PART_NUM}
IL_LO_CLOUDRC_PART_NKN_LABEL=NKN
IL_LO_CLOUDRC_PART_NKN_MOUNT=/nkn
IL_LO_CLOUDRC_PART_NKN_BOOTABLE=
IL_LO_CLOUDRC_PART_NKN_PART_ID=83
IL_LO_CLOUDRC_PART_NKN_FSTYPE=ext3
IL_LO_CLOUDRC_PART_NKN_SIZE=5000
IL_LO_CLOUDRC_PART_NKN_GROWTHCAP=102400
IL_LO_CLOUDRC_PART_NKN_GROWTHWEIGHT=33
IL_LO_CLOUDRC_PART_NKN_OPTIONS="defaults,noatime"

IL_LO_CLOUDRC_PART_LOG_TARGET=DISK1
IL_LO_CLOUDRC_PART_LOG_PART_NUM=11
IL_LO_CLOUDRC_PART_LOG_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_LOG_PART_NUM}
IL_LO_CLOUDRC_PART_LOG_LABEL=LOG
IL_LO_CLOUDRC_PART_LOG_MOUNT=/log
IL_LO_CLOUDRC_PART_LOG_BOOTABLE=
IL_LO_CLOUDRC_PART_LOG_PART_ID=83
IL_LO_CLOUDRC_PART_LOG_FSTYPE=ext3
IL_LO_CLOUDRC_PART_LOG_SIZE=5000
IL_LO_CLOUDRC_PART_LOG_GROWTHCAP=102400
IL_LO_CLOUDRC_PART_LOG_GROWTHWEIGHT=33
IL_LO_CLOUDRC_PART_LOG_OPTIONS="defaults,noatime"

IL_LO_CLOUDRC_PART_DMFS_TARGET=DISK1
IL_LO_CLOUDRC_PART_DMFS_PART_NUM=12
IL_LO_CLOUDRC_PART_DMFS_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_DMFS_PART_NUM}
IL_LO_CLOUDRC_PART_DMFS_LABEL=DMFS
IL_LO_CLOUDRC_PART_DMFS_MOUNT=
IL_LO_CLOUDRC_PART_DMFS_BOOTABLE=
IL_LO_CLOUDRC_PART_DMFS_PART_ID=83
  # For the dmfs parition set the FSTYPE to blank, so it will not create a new
  # filesystem on the partition, so we can preserve the contents.
  # Our install-mfd will do all the needed making of filesystems and labels,
  # because it knows when we want to preserve the data or not.
IL_LO_CLOUDRC_PART_DMFS_FSTYPE=
IL_LO_CLOUDRC_PART_DMFS_SIZE=300
IL_LO_CLOUDRC_PART_DMFS_GROWTHCAP=
# We want this to be about 10% the size of DMRAW
IL_LO_CLOUDRC_PART_DMFS_GROWTHWEIGHT=3

IL_LO_CLOUDRC_PART_DMRAW_TARGET=DISK1
IL_LO_CLOUDRC_PART_DMRAW_PART_NUM=13
IL_LO_CLOUDRC_PART_DMRAW_DEV=${IL_LO_CLOUDRC_TARGET_DISK1_DEV}${IL_LO_CLOUDRC_PART_DMRAW_PART_NUM}
IL_LO_CLOUDRC_PART_DMRAW_LABEL=DMRAW
IL_LO_CLOUDRC_PART_DMRAW_MOUNT=
IL_LO_CLOUDRC_PART_DMRAW_BOOTABLE=
IL_LO_CLOUDRC_PART_DMRAW_PART_ID=
IL_LO_CLOUDRC_PART_DMRAW_FSTYPE=
IL_LO_CLOUDRC_PART_DMRAW_SIZE=3000
IL_LO_CLOUDRC_PART_DMRAW_GROWTHCAP=
IL_LO_CLOUDRC_PART_DMRAW_GROWTHWEIGHT=30

# ###########################################################################
# ###########################################################################

# ###########################################################
#  NOW DOT IN THE FILE THAT HAS OVERRIDES FOR THESE SETTING #
# ###########################################################
if [ "${1}" != "noupdate" ] ; then
  OVERRIDE_FILE=/config/nkn/partition-override.dot
  [ -f ${OVERRIDE_FILE} ] && . ${OVERRIDE_FILE}
fi
}
