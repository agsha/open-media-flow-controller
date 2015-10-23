#/bin/bash
#
# The dmi image file needs to be installed as
#      /nkn/virt/pools/default/mfc-dmi-adapter.img
# (Because libvirt only likes the image file in the pools directory,
# and we want to use a single standard name for the installed one.)
# Note:
# The pre_img_install.sh script has a function that places the dmi zip
# file and matching info files in /nkn/dmi-adapter-zips/.
# That script is run from writeimage graft function 4 in customer_rootflop.sh
#
# TBD what to do when there already is an mfc-dmi-adapter.img file installed.
#
Upgrade_Extract()
{
  if [ ! -d /nkn/dmi-adapter-zips ] ; then
    echo Upgrade_Extract >> ${LOG_FILE}
    # The graft function 4 was not run because we upgraded from an MFC version
    # that did not have that.  (With upgrades, writeimage runs in the environment
    # being upgraded from.)
    # Look for mfc install images in the dir /var/opt/tms/images/.
    # When the GUI was used to upgrade the file there is "webimage.tbz"
    # When the CLI was used to upload the image file it will have the same
    # filename that it was uploaded from.
    # When find a match, then put the build_version.sh and md5sums along with
    # the mfc-dmi-adapater zip file into /nkn/dmi-adapter-zips in the
    # same way that pre_img_install.sh does.
    # Compare the build_version.sh file with that in each archive to
    # find which is the one to use.
    mkdir /nkn/dmi-adapter-zips
    cd /nkn/dmi-adapter-zips
    for FN in /var/opt/tms/images/*
    do
      [ ! -f "${FN}" ] && continue
      echo Process "${FN}" >> ${LOG_FILE}
      rm -f build_version.sh md5sums
      # Skip if no build_version.sh file is in the top of the archive.
      unzip "${FN}" build_version.sh > /dev/null 2>&1
      if [ ! -f build_version.sh ] ; then
        echo No build_version.sh >> ${LOG_FILE}
        continue
      fi
      cmp -s build_version.sh /etc/build_version.sh
      if [ ${?} -ne 0 ] ; then
        echo build_version.sh does not match >> ${LOG_FILE}
        rm -f build_version.sh
        continue
      fi
      unzip ${FN} md5sums > /dev/null 2>&1
      if [ ! -f md5sums ] ; then
        echo Warning: No md5sums >> ${LOG_FILE}
      fi
      echo Extract mfc-dmi-adapater image file from MFC upgrade image.
      echo \
      unzip ${FN} 'mfc-dmi-adapter-*.img.zip' >> ${LOG_FILE}
      unzip ${FN} 'mfc-dmi-adapter-*.img.zip' >> ${LOG_FILE} 2>&1
      DMI_ADAPTER_ZIP=`ls mfc-dmi-adapter-*.img.zip 2> /dev/null`
      if [ -z "${DMI_ADAPTER_ZIP}" ] ; then
        echo "No mfc-dmi-adapter-*.img.zip" >> ${LOG_FILE}
        rm -f build_version.sh md5sums
        continue
      fi
      MFC_IMAGE_TBZ=`unzip -l ${FN} 'image*.tbz' | grep image | awk '{ print $4 }'`
      MFC_IMAGE_TBZ=`echo ${MFC_IMAGE_TBZ}`
      echo DMI_ADAPTER_ZIP=${DMI_ADAPTER_ZIP} >> build_version.sh
      echo MFC_IMAGE_TBZ=${MFC_IMAGE_TBZ}     >> build_version.sh
      break
    done
  fi
}

Process_zips_dir()
{
  echo Process_zips_dir >> ${LOG_FILE}
  # See if there is something to be done in this directory.
  # When a build_version.sh and md5sums file are here and the
  # matching mfc-dmi-adapter*.img.zip file, then set up for
  # later processing.

  # When no build_version.sh file, then nothing to do.
  [ ! -f /nkn/dmi-adapter-zips/build_version.sh ] && return
  cd /nkn/dmi-adapter-zips

  # Get the settings from the file.
  # Two settings were added by the writeimage graft_4 functions
  # to help match the dmi adapter to the mfc version:
  DMI_ADAPTER_ZIP=
  MFC_IMAGE_TBZ=
  eval `grep ^DMI_ADAPTER_ZIP= build_version.sh`
  eval `grep ^MFC_IMAGE_TBZ= build_version.sh`
  echo DMI_ADAPTER_ZIP=${DMI_ADAPTER_ZIP} >> ${LOG_FILE}
  echo MFC_IMAGE_TBZ=${MFC_IMAGE_TBZ}     >> ${LOG_FILE}
  # These next two are standard in build_version.sh:
  BUILD_PROD_RELEASE=
  BUILD_ID=
  eval `grep ^BUILD_PROD_RELEASE= build_version.sh`
  eval `grep ^BUILD_ID= build_version.sh`
  # Example of BUILD_PROD_RELEASE, BUILD_ID and BUILD_PROD_VERISON are:
  # BUILD_PROD_RELEASE="mfc-12.3.0-qa"
  # BUILD_ID="27_25492_317"
  #  and
  # BUILD_PROD_RELEASE="mfc-12.2.0"
  # BUILD_ID="25491_327+11-dpeet"
  echo BUILD_PROD_RELEASE=${BUILD_PROD_RELEASE} >> ${LOG_FILE}
  echo BUILD_ID=${BUILD_ID}               >> ${LOG_FILE}

  [ ! -d ${BUILD_PROD_RELEASE} ] && mkdir ${BUILD_PROD_RELEASE}
  FULL_VER="${BUILD_PROD_RELEASE}-${BUILD_ID}"
  echo FULL_VER=${FULL_VER}               >> ${LOG_FILE}

  if [ ! -f md5sums ] ; then
    # This should never happen! Make fake content so the code will not break.
    echo "# Fake md5sums" > md5sums
    echo "0 ./mfc-dmi-adapter-unknown.img.zip" >> md5sums
    echo "0 ./image-mfc-unknown.tbz" >> md5sums
  fi
  if [ -z "${DMI_ADAPTER_ZIP}" ] ; then
    # This should never happen, but try to fix so later code will not break.
    ITEM=`cat md5sums | grep mfc-dmi-adapter- | awk '{ print $2 }'`
    DMI_ADAPTER_ZIP=`basename ${ITEM}`
    echo DMI_ADAPTER_ZIP=${DMI_ADAPTER_ZIP} >> build_version.sh
  fi
  if [ -z "${MFC_IMAGE_TBZ}" ] ; then
    # This should never happen, but try to fix so later code will not break.
    ITEM=`cat md5sums | grep image-mfc- | awk '{ print $2 }'`
    MFC_IMAGE_TBZ=`basename ${ITEM}`
    echo MFC_IMAGE_TBZ=${MFC_IMAGE_TBZ} >> build_version.sh
  fi
  echo Create ${BUILD_PROD_RELEASE}/build_version.${FULL_VER} >> ${LOG_FILE}
  echo Create ${BUILD_PROD_RELEASE}/md5sums.${FULL_VER} >> ${LOG_FILE}
  mv build_version.sh ${BUILD_PROD_RELEASE}/build_version.${FULL_VER}
  mv md5sums ${BUILD_PROD_RELEASE}/md5sums.${FULL_VER}
}

Convert_Old_Style()
{
  # When there is a /nkn/dmi-images directory, convert to the dmi-adapter-zips style.
  [ ! -d /nkn/dmi-images ] && return
  echo Convert_Old_Style >> ${LOG_FILE}
  [ ! -d /nkn/dmi-adapter-zips ] && mkdir /nkn/dmi-adapter-zips
  cd /nkn/dmi-images
  BUILD_PROD_RELEASE=
  BUILD_ID=
  FULL_VER=
  if [ ! -f build_version.sh  -a -f download-this.txt ] ; then
    DOWNLOAD_URL=
    eval `grep ^DOWNLOAD_URL= download-this.txt`
    if [ ! -z "${DOWNLOAD_URL}" ] ; then
      T1=`dirname "${DOWNLOAD_URL}"`
      T2=`basename "${T1}"`
      case ${T2} in
      mfc-*)
        FULL_VER=${T2}
        Z_BUILD_PROD_RELEASE=`echo ${FULL_VER} | cut -f1-2 -d-`
        eval `grep ^BUILD_PROD_RELEASE= /log/mfglog/build_version.sh`
        if [ "${Z_BUILD_PROD_RELEASE}" = "${BUILD_PROD_RELEASE}" ] ; then
          echo Use /log/mfglog/build_version.sh >> ${LOG_FILE}
          cp /log/mfglog/build_version.sh .
          echo DOWNLOAD_URL=${DOWNLOAD_URL} >> build_version.sh
        else
          echo Create build_version.sh from info in download-this.txt >> ${LOG_FILE}
          BUILD_ID=`echo ${FULL_VER} | cut -f3- -d-`
          echo BUILD_PROD_RELEASE=${Z_BUILD_PROD_RELEASE} > build_version.sh
          echo BUILD_ID=${BUILD_ID} >> build_version.sh
          echo DOWNLOAD_URL=${DOWNLOAD_URL} >> build_version.sh
        fi
        ;;
      esac
    fi
  fi
  if [ ! -f build_version.sh ] ; then
    echo Use /log/mfglog/build_version.sh >> ${LOG_FILE}
    cp /log/mfglog/build_version.sh .
  fi
  BUILD_PROD_RELEASE=
  BUILD_ID=
  eval `grep ^BUILD_PROD_RELEASE= build_version.sh`
  eval `grep ^BUILD_ID= build_version.sh`
  [ -z "${BUILD_PROD_RELEASE}" ] && BUILD_PROD_RELEASE=unknown
  [ -z "${BUILD_ID}" ] && BUILD_ID=unknown
  FULL_VER="${BUILD_PROD_RELEASE}-${BUILD_ID}"

  echo BUILD_PROD_RELEASE=${BUILD_PROD_RELEASE} >> ${LOG_FILE}
  echo BUILD_ID=${BUILD_ID} >> ${LOG_FILE}
  echo FULL_VER=${FULL_VER} >> ${LOG_FILE}
  [ ! -d /nkn/dmi-adapter-zips/${BUILD_PROD_RELEASE} ] && mkdir /nkn/dmi-adapter-zips/${BUILD_PROD_RELEASE}

  PKG_FILE=
  if [ -f info.txt ] ; then
    eval `grep ^PKG_FILE= info.txt`
    echo Contents of info.txt >> ${LOG_FILE}
    cat info.txt >> ${LOG_FILE}
    echo EOF >> ${LOG_FILE}
  else
    # This should never happen, but try to fix so later code will not break.
    for i in *.img.zip
    do
      [ ! -f ${i} ] && continue
      PKG_FILE="${i}"
    done
  fi
  if [ -z "${PKG_FILE}" ] ; then
    DMI_ADAPTER_ZIP=mfc-dmi-adapter-unknown.img.zip
  else
    DMI_ADAPTER_ZIP=${PKG_FILE}
  fi
  echo DMI_ADAPTER_ZIP=${DMI_ADAPTER_ZIP}  >> build_version.sh
  echo MFC_IMAGE_TBZ=image-${FULL_VER}.tbz >> build_version.sh
  echo DMI_ADAPTER_ZIP=${DMI_ADAPTER_ZIP}  >> ${LOG_FILE}
  echo MFC_IMAGE_TBZ=image-${FULL_VER}.tbz >> ${LOG_FILE}
  echo Create /nkn/dmi-adapter-zips/${BUILD_PROD_RELEASE}/build_version.${FULL_VER} >> ${LOG_FILE}
  mv build_version.sh /nkn/dmi-adapter-zips/${BUILD_PROD_RELEASE}/build_version.${FULL_VER}
  if [ -f info.txt ] ; then
    # Remove info.txt from /nkn/dmi-images.
    mv info.txt /nkn/dmi-adapter-zips/${BUILD_PROD_RELEASE}/info.txt.${FULL_VER}
  fi
  if [ -f download_this.txt ] ; then
    # Remove download_this.txt from /nkn/dmi-images.
    mv download_this.txt /nkn/dmi-adapter-zips/${BUILD_PROD_RELEASE}/download_this.txt.${FULL_VER}
  fi

  # Move the dmi-adapter image zips to the new directory.
  for i in *.img.zip
  do
    [ ! -f ${i} ] && continue
    if [ -f /nkn/dmi-adapter-zips/${i} ] ; then
      echo Already exists: /nkn/dmi-adapter-zips/${i} >> ${LOG_FILE}
      rm -f ${i}
    else
      echo Move ${i} to /nkn/dmi-adapter-zips/ >> ${LOG_FILE}
      mv ${i} /nkn/dmi-adapter-zips
    fi
  done
  # Move any other files out of the directory.
  mv * /nkn/dmi-adapter-zips/${BUILD_PROD_RELEASE}/ 2> /dev/null
  cd /nkn
  rmdir /nkn/dmi-images
}

Do_Install()
{
  # Get the BUILD_PROD_RELEASE and BUILD_ID settings so we can determine
  # which one to set up.
  BUILD_PROD_RELEASE=
  BUILD_ID=
  eval `grep ^BUILD_PROD_RELEASE= /etc/build_version.sh`
  eval `grep ^BUILD_ID= /etc/build_version.sh`
  FULL_VER="${BUILD_PROD_RELEASE}-${BUILD_ID}"

  # Check if this is an upgrade situation
  if [ -f /nkn/virt/pools/default/mfc-dmi-adapter.img ] ; then
    # A mfc-dmi-adapter has already been installed.
    Update_Install
    return
  fi

  # Not already installed, so set it up now.
  BV_F=/nkn/dmi-adapter-zips/${BUILD_PROD_RELEASE}/build_version.${FULL_VER}
  if [ ! -f ${BVF} ] ; then
    echo
    echo Warning, no dmi-adapter defined for ${FULL_VER} | tee -a ${LOG_FILE}
    echo Cannot install the mfc-dmi-adapter | tee -a ${LOG_FILE}
    echo
    return
  fi
  # Dot in the file to get the DMI_ADAPTER_ZIP settings.
  DMI_ADAPTER_ZIP=
  eval `grep ^DMI_ADAPTER_ZIP= ${BV_F}`
  if [ ! -f /nkn/dmi-adapter-zips/${DMI_ADAPTER_ZIP} ] ; then
    echo
    echo Error, missing ${DMI_ADAPTER_ZIP} | tee -a ${LOG_FILE}
    echo Failed to install the mfc-dmi-adapter | tee -a ${LOG_FILE}
    echo
    return
  fi
  IMG_NAME=`basename ${DMI_ADAPTER_ZIP} .zip`
  rm -f /nkn/tmp/${IMG_NAME}
  unzip /nkn/dmi-adapter-zips/${DMI_ADAPTER_ZIP} -d /nkn/tmp 2>&1 | tee -a ${LOG_FILE}
  if [ ! -f /nkn/tmp/${IMG_NAME} ] ; then
    echo
    echo Error, Failed to extract ${IMG_NAME} | tee -a ${LOG_FILE}
    echo Failed to install the mfc-dmi-adapter | tee -a ${LOG_FILE}
    echo
    return
  fi
  [ ! -d /nkn/virt/pools/default ] && mkdir -p /nkn/virt/pools/default
  mv /nkn/tmp/${IMG_NAME} /nkn/virt/pools/default/mfc-dmi-adapter.img
  if [ ! -f /nkn/virt/pools/default/mfc-dmi-adapter.img ] ; then
    echo
    echo Error, Failed to create /nkn/virt/pools/default/mfc-dmi-adapter.img | tee -a ${LOG_FILE}
    echo Failed to install the mfc-dmi-adapter | tee -a ${LOG_FILE}
    echo
    return
  fi
  echo ${IMG_NAME},${FULL_VER} > /log/install/mfc-dmi-adapter.installed
}

Update_Install()
{
  echo Checking dmi-adapter image for upgrade | tee -a ${LOG_FILE}

  # Step 1 : Check if MFC version in  /log/install/mfc-dmi-adapter.installed 
  #	matches with running MFC version $BUILD_PROD_RELEASE-$BUILD_ID
  #	     If yes, then nothing to do else go to step 2
  # Step 2 : Check the settings in /log/install/mfc-dmi-adapter.installed 
  # 	and compare it to $DMI_ADAPTER_ZIP in 
  #	/nkn/dmi-adapter-zips/<BUILD_PROD_RELEASE>/build_version.<MFC_IMAGE_TBZ-.tbz>
  #	If it matches then nothing to do as DMI adapter has not changed
  # Step 3 : Since we have new DMI adapter install it over the existing one

  # Compare the current running MFC version (FULL_VER) value with DMI the installed version
  if [ ! -f ${DMI_INSTALLED} ]; then # Should never happen !!!!
    echo Error, Failed to find ${DMI_INSTALLED}| tee -a ${LOG_FILE}
    echo Skipping upgrade check for mfc-dmi-adapter | tee -a ${LOG_FILE}
    return
  fi

  DMI_INSTALLED_MFC_VER=`cat ${DMI_INSTALLED} | cut -f2 -d','`
  if [ -z "${DMI_INSTALLED_MFC_VER}" ]; then
    echo Error, Failed to find installed version DMI | tee -a ${LOG_FILE}
    echo Skipping upgrade check for mfc-dmi-adapter | tee -a ${LOG_FILE}
    return
  fi

  # Compare installed MFC version vs running MFC version
  if [ "$DMI_INSTALLED_MFC_VER" == "$FULL_VER" ]; then
    echo "Info: Correct version of DMI adapter installed for ${FULL_VER}" | tee -a ${LOG_FILE}
    return
  fi

  # Since it does not match, need to check matching version exists.
  echo "Warning: Current installed version of DMI adapter does not match ${FULL_VER}" | tee -a ${LOG_FILE}
  echo "Upgrading the DMI adapter matching ${FULL_VER}" | tee -a ${LOG_FILE}

  # About to upgrade, so set it up now.
  BV_F=/nkn/dmi-adapter-zips/${BUILD_PROD_RELEASE}/build_version.${FULL_VER}
  if [ ! -f ${BVF} ] ; then
    echo
    echo Warning, no dmi-adapter defined for ${FULL_VER} | tee -a ${LOG_FILE}
    echo Cannot upgrade the mfc-dmi-adapter | tee -a ${LOG_FILE}
    echo
    return
  fi
  # Dot in the file to get the DMI_ADAPTER_ZIP settings.
  DMI_ADAPTER_ZIP=
  eval `grep ^DMI_ADAPTER_ZIP= ${BV_F}`
  if [ ! -f /nkn/dmi-adapter-zips/${DMI_ADAPTER_ZIP} ] ; then
    echo
    echo Error, missing ${DMI_ADAPTER_ZIP} | tee -a ${LOG_FILE}
    echo Failed to upgrade the mfc-dmi-adapter | tee -a ${LOG_FILE}
    echo
    return
  fi
  IMG_NAME=`basename ${DMI_ADAPTER_ZIP} .zip`

  # Check to see if there is a change in DMI version
  DMI_INSTALLED_IMG=`cat ${DMI_INSTALLED} | cut -f1 -d','`
  if [ "${DMI_INSTALLED_IMG}" == "${IMG_NAME}" ]; then
    echo "Info: Correct version of DMI adapter installed for ${FULL_VER}" | tee -a ${LOG_FILE}
    return
  fi

  # Now install the new version
  rm -f /nkn/tmp/${IMG_NAME}
  unzip /nkn/dmi-adapter-zips/${DMI_ADAPTER_ZIP} -d /nkn/tmp 2>&1 | tee -a ${LOG_FILE}
  if [ ! -f /nkn/tmp/${IMG_NAME} ] ; then
    echo
    echo Error, Failed to extract ${IMG_NAME} | tee -a ${LOG_FILE}
    echo Failed to upgrade the mfc-dmi-adapter | tee -a ${LOG_FILE}
    echo
    return
  fi
  if [ -f /nkn/virt/pools/default/mfc-dmi-adapter.img.bak ] ; then
  	rm -f /nkn/virt/pools/default/mfc-dmi-adapter.img.bak;
  fi
  mv /nkn/virt/pools/default/mfc-dmi-adapter.img /nkn/virt/pools/default/mfc-dmi-adapter.img.bak
  mv /nkn/tmp/${IMG_NAME} /nkn/virt/pools/default/mfc-dmi-adapter.img
  if [ ! -f /nkn/virt/pools/default/mfc-dmi-adapter.img ] ; then
    echo
    echo Error, Failed to create /nkn/virt/pools/default/mfc-dmi-adapter.img | tee -a ${LOG_FILE}
    echo Failed to install the mfc-dmi-adapter | tee -a ${LOG_FILE}
    echo
    return
  fi
  echo ${IMG_NAME},${FULL_VER} > /log/install/mfc-dmi-adapter.installed
  echo Upgraded DMI adapter to ${IMG_NAME} | tee -a ${LOG_FILE}

}


#
# MAIN LOGIN BEGINS HERE
#
LOG_FILE=/log/install/mfc-dmi-adapter-install.log
DMI_INSTALLED=/log/install/mfc-dmi-adapter.installed
[ ! -d /log/install ] && mkdir /log/install
date >> ${LOG_FILE}

# Check if we running in Pacifica, if so don't do anything
PACIFICA_HW=no
[ -f /etc/pacifica_hw ] && . /etc/pacifica_hw
if [ $PACIFICA_HW == "yes" ]; then
  echo "DMI Adapter: Hardware is AS-MXC (Pacifica) hence skipping installation" | tee -a ${LOG_FILE}
  exit 0 ;
fi

# Do not install (or upgrade) dmi-adapter if less than 3G free.
# 3G is 3145728 (3 * 1024 * 1024)
DO_IT=yes
NKN_FREE_K=`df -P /nkn/. | grep "/nkn" | awk '{print $4}'`
if [ -z "${NKN_FREE_K}" ] ; then
  echo Error getting /nkn free space size | tee -a ${LOG_FILE}
  echo Skipping dmi-adapter install/upgrade check | tee -a ${LOG_FILE}
  DO_IT=no
elif [ ${NKN_FREE_K} -lt 3145728 ] ; then
  echo Low free space on /nkn ${NKN_FREE_K} | tee -a ${LOG_FILE}
  echo Skipping dmi-adapter install/upgrade check | tee -a ${LOG_FILE}
  return
  DO_IT=no
fi
if [ "${DO_IT}" = "yes" ] ; then
  echo "/nkn free size = ${NKN_FREE_K} K" >> ${LOG_FILE}

  Upgrade_Extract
  Convert_Old_Style
  Process_zips_dir

  # At this point /nkn/dmi-adapter-zips/ directory is set up.
  Do_Install

  NKN_FREE_K=`df -P /nkn/. | grep "/nkn" | awk '{print $4}'`
  echo "/nkn free size = ${NKN_FREE_K} K" >> ${LOG_FILE}
fi

#
# End of dmi_adapter_install.sh
#
