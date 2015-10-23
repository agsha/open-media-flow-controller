# This is pre_img_install.sh
# It must implement the function Writeimage_g4()
# This file is dotted in by the writeimage_graft_4() function in customer_rootflop.sh.
# Right after dotting this in it calls Writeimage_g4().

# Pull out the dmi image zip file from the package and install it
# where it can be used later after the new MFC version is booted.
# NOTE: 
# 1: When doing an upgrade-install this script is running in the
#    environment being upgraded *from*.
#    In this case the old /nkn filesystem is available.
# 2: When doing a fresh manufacture this script is is running in the
#    limited manufacture env.
#    In this case the nkn filesystem is not created yet.
#    So we save files and info we need in a tmpfs so that our manufacture
#    script will have access to the needed files and info.
Writeimage_g4()
{
  echo Writeimage_g4
  if [ -z "${UNZIP_DIR}" ] ; then
    echo UNZIP_DIR is not set
    return 1
  fi
  if [ ! -d "${UNZIP_DIR}" ] ; then
    echo No such UNZIP_DIR ${UNZIP_DIR}
    return 1
  fi
  FPN=`ls ${UNZIP_DIR}/*dmi-adapter*.zip 2> /dev/null`
  if [ -z "${FPN}" ] ; then
    echo No dmi-adapter file ${UNZIP_DIR}/*dmi-adapter*.zip
    return 1
  fi
  FILE_SZ=`ls -l ${FPN} | awk '{print $5}'`
  SZ_MB=`expr ${FILE_SZ} / 1048576`
  SZ_MB=`expr ${SZ_MB} + 5`
  FN=`basename ${FPN}`
  if [ "_${IS_MANUFACTURE}" = "_yes" ] ; then
    MNT_POINT=/tmp/dmi-adapter-extract
    TO_DIR=${MNT_POINT}
    if [ ! -d ${MNT_POINT} ] ; then
      mkdir ${MNT_POINT}
    else
      umount ${MNT_POINT} 2> /dev/null
    fi
    FAILURE=0
    mount -t tmpfs -o size=${SZ_MB}M,mode=700 none ${MNT_POINT} || FAILURE=1
    if [ ${FAILURE} -eq 1 ]; then
      echo
      echo ERROR: Failed to create tmpfs to extract dmi-adapter file.  Not enough RAM.
      echo
      cat /proc/meminfo
      return 1
    fi
  else
    # Not manufacturing, so doing an upgrade.
    if [ ! -d /nkn ] ; then
      echo
      echo Error: No directory /nkn
      echo
      df
      return 1
    fi
    TO_DIR=/nkn/dmi-adapter-zips
    [ ! -d ${TO_DIR} ] && mkdir ${TO_DIR}
    if [ ! -d ${TO_DIR} ] ; then
      echo
      echo ERROR: Failed to create directory ${TO_DIR}
      echo
      return 1
    fi
    # Save info about the current MFC in the old location so that the script
    # that installs the mfc-dmi-adapter image has enough info if it needs
    # to install an old image.
    if [ -d /nkn/dmi-images ] ; then
      if [ ! -f /nkn/dmi-images/build_version.sh ] ; then
        cp /etc/build_version.sh /nkn/dmi-images
      fi
    fi
  fi
  if [ -f ${TO_DIR}/${FN} ] ; then
    echo
    echo Note: Dmi-image already extracted: ${TO_DIR}/${FN}
    echo
  else
    echo cp -a ${FPN} ${TO_DIR}/
    cp -a ${FPN} ${TO_DIR}/ 2>&1
    ls -l ${FPN} ${TO_DIR}/${FN} 2>&1
    cmp -s ${FPN} ${TO_DIR}/${FN}
    if [ ${?} -ne 0 ] ; then
      echo
      echo ERROR: Copy of ${FPN} to ${TO_DIR} failed
      echo ERROR: Failed to extract dmi-adapter file.
      echo 
      df
      return 1
    fi
  fi
  for i in build_version.sh md5sums
  do
    echo \
    cp ${UNZIP_DIR}/${i} ${TO_DIR}/
    cp ${UNZIP_DIR}/${i} ${TO_DIR}/
  done
  echo DMI_ADAPTER_ZIP=${FN} >> ${TO_DIR}/build_version.sh
  MFC_IMAGE_TBZ=`ls ${UNZIP_DIR}/image-mfc-* 2> /dev/null`
  MFC_IMAGE_TBZ=`basename ${MFC_IMAGE_TBZ}`
  echo MFC_IMAGE_TBZ=${MFC_IMAGE_TBZ} >> ${TO_DIR}/build_version.sh
  return 0
}
