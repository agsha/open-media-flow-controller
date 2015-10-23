#!/bin/bash
# Create a pxe tgz file.
MY_DIR=`dirname "${0}"`
case "${MY_DIR}" in
/*)   ;;
../*) MY_DIR=`pwd`/.. ;;
.|./) MY_DIR=`pwd` ;;
*) MY_DIR=`pwd`/"${MY_DIR}" ;;
esac

# The mf_env_funcs.dot files defines the functions:
#   Get_Env_Settings_From_File()
#   Validate_PROD()
#   Set_Defaults()
#   Set_NKN_Env_From_Source_Tree()
#   Make_ENVS()
#   efVprint()
#
EF=${MY_DIR}/mf_env_funcs.dot
if [ ! -f "${EF}" ] ; then
    echo Error, no ${EF}
    exit 1
fi
. ${EF}
VERBOSE_EFST=no

if [ -z "${REAL_USER}" ] ; then
  REAL_USER=${USER}
fi

FROM_HERE=${1}
STORAGE01_SCP_USER=${2}
if [ -z "${FROM_HERE}" ] ; then
  echo Specify the ab-pkg directory. Examples:
  echo "  " david@10.157.42.56:/b2/out1/ab-pkg/mfX-rc/mfX-2.0.2-rc-3_11350_170/release/
  echo "  " david@10.157.42.56:/b2/out1/ab-pkg/mfX-rc/mfX-2.0.2-rc-3_11350_170
  echo "  " 10.157.42.56:/b2/out1/ab-pkg/mfX-rc/mfX-2.0.2-rc-3_11350_170/release/
  echo "  " 10.157.42.56:/b2/out1/ab-pkg/mfX-rc/mfX-2.0.2-rc-3_11350_170
  echo "  " /b2/out1/ab-pkg/mfX-rc/mfX-2.0.2-rc-3_11350_170/release/
  echo "  " /b2/out1/ab-pkg/mfX-rc/mfX-2.0.2-rc-3_11350_170
  echo Specify the build directory. Examples:
  echo "  " david@10.157.42.52:/b2/out1/ab-out/mfX-rc/mfX-2.0.2-rc-3_11350_170/product-nokeena-x86_64/release/
  echo "  " david@10.157.42.56:/b2/out1/ab-pkg/mfX-rc/mfX-2.0.2-rc-3_11350_170
  echo "  " 10.157.42.56:/b2/out1/ab-out/mfX-rc/mfX-2.0.2-rc-3_11350_170/product-nokeena-x86_64/release/
  echo "  " 10.157.42.56:/b2/out1/ab-out/mfX-rc/mfX-2.0.2-rc-3_11350_170
  echo "  " /b2/out1/ab-out/mfX-rc/mfX-2.0.2-rc-3_11350_170/product-nokeena-x86_64/release/
  echo "  " /b2/out1/ab-out/mfX-rc/mfX-2.0.2-rc-3_11350_170
  echo Second parameter is the user name to scp from cmbu-storage01 with.
  exit 1
fi
MY_DIR=`dirname "${0}"`
case "${MY_DIR}" in
/*)   ;;
../*) MY_DIR=`pwd`/.. ;;
.|./) MY_DIR=`pwd` ;;
*) MY_DIR=`pwd`/"${MY_DIR}" ;;
esac
case ${FROM_HERE} in
local:*)
  ACCESS=local
  FROM_HERE=`echo ${FROM_HERE} | cut -f2 -d:`
 ;;
*:/b1/*|scp:/b1/*)
  ACCESS=scp
  DEFAULT_USER=david
  DEFAULT_HOST=cmbu-build01.englab.juniper.net
  ;;
*:/b2/*|scp:/b2/*)
  ACCESS=scp
  DEFAULT_USER=david
  DEFAULT_HOST=cmbu-build01.englab.juniper.net
  ;;
*:/b3/*|scp:/b3/*)
  ACCESS=scp
  DEFAULT_USER=david
  DEFAULT_HOST=cmbu-build01.englab.juniper.net
  ;;
*:/b4/*|scp:/b4/*)
  ACCESS=scp
  DEFAULT_USER=david
  DEFAULT_HOST=cmbu-build01.englab.juniper.net
  ;;
*:*)
  ACCESS=scp
  DEFAULT_USER=dpeet
  DEFAULT_HOST=cmbu-storage01.juniper.net
  ;;
*)
  ACCESS=local
 ;;
esac
if [ -z "${STORAGE01_SCP_USER}" ] ; then
  STORAGE01_SCP_USER=dpeet
elif [ "${STORAGE01_SCP_USER}" = "me" ] ; then
  STORAGE01_SCP_USER=${USER}
fi
PREV=no
for i in *
do
  [ -z "${i}" ] && continue
  [ -z "${i}" ] && continue
  case ${i} in
  image-*.img*|mfgcd-*.iso*|pxe-*.tgz*|additional-*.tgz*|pxe|sums*.txt|do-this-upload*.sh|mibs|index-*.html|index-*.list)
    PREV=yes ;;
  *)
    if [ "${i}" != "*" ] ; then
      echo This is not an empty directory.  Change to an empty dir first.
      exit 1
    fi
    ;;
  esac
done
if [ "${PREV}" = "yes" ] ; then
  echo This command was previously run in this directory.
  echo Removing the old files
  rm -f image-*.img* mfgcd-*.iso* pxe-*.tgz* additional-*.tgz*
  rm -f pxe/*
  rm -f sums*.txt md5sums*.txt
  rm -f do-this-upload*.sh
  rm -rf mibs/*
  rm -rf index-*.html index-*.list
fi
[ ! -d pxe ] && mkdir pxe
[ ! -d mibs ] && mkdir mibs

case ${FROM_HERE} in
*@*:*)
  U=`echo ${FROM_HERE} | cut -f1 -d@`
  H=`echo ${FROM_HERE} | cut -f2 -d@ | cut -f1 -d:`
  ;;
*:*)
  U=${DEFAULT_USER}
  H=`echo ${FROM_HERE} | cut -f1 -d:`
  ;;
*)
  U=${DEFAULT_USER}
  H=${DEFAULT_HOST}
  ;;
esac
if [ "${U}" = "me" ] ; then
  U=${USER}
fi

if [ "${ACCESS}" = "scp" ] ; then
  case ${FROM_HERE} in
  *@*:*/) SCP_FROM=${FROM_HERE} ;;
  *@*:*)  SCP_FROM=${FROM_HERE}/release ;;
  *:*/)   SCP_FROM=${U}@${FROM_HERE}         ;;
  *:*)    SCP_FROM=${U}@${FROM_HERE}/release ;;
  */)     SCP_FROM=${U}@${H}:${FROM_HERE} ;;
  *)      SCP_FROM=${U}@${H}:${FROM_HERE}/release ;;
  esac
else
  case ${FROM_HERE} in
  */) CP_FROM=${FROM_HERE} ;;
  *)  CP_FROM=${FROM_HERE}/release ;;
  esac
fi

if [ "${ACCESS}" = "scp" ] ; then
  scp ${SCP_FROM}/image/image-*.img . 2> /dev/null
else
  cp ${CP_FROM}/image/image-*.img . 2> /dev/null
fi
IMG_FILE_NAME=`ls image-*.img 2> /dev/null`
if [ -z "${IMG_FILE_NAME}" ] ; then
  if [ "${ACCESS}" = "scp" ] ; then
    SCP_FROM=`echo ${SCP_FROM} | sed "s#/release#/product-nokeena-x86_64/release#"`
    scp ${SCP_FROM}/image/image-*.img . 2> /dev/null
  else
    CP_FROM=`echo ${CP_FROM} | sed "s#/release#/product-nokeena-x86_64/release#"`
    cp ${CP_FROM}/image/image-*.img . 2> /dev/null
  fi
  IMG_FILE_NAME=`ls image-*.img`
  if [ -z "${IMG_FILE_NAME}" ] ; then
    echo image-*.img file not downloaded
    exit 1
  fi
fi
RELEASE_VER=`echo ${IMG_FILE_NAME} | sed "/image-/s///" | sed "/.img/s///"`
PXE_FILE_NAME=pxe-${RELEASE_VER}.tgz
ADD_FILE_NAME=additional-${RELEASE_VER}.tgz
MIBS_DIR=mibs-${RELEASE_VER}
MIBS_FILE_NAME=${MIBS_DIR}.tgz
mkdir -p mibs/${MIBS_DIR}
if [ "${ACCESS}" = "scp" ] ; then
  scp -p ${SCP_FROM}/mfgcd/mfgcd-*.iso .
  scp -p ${SCP_FROM}/manufacture/manufacture-*.tgz pxe
  scp -p ${SCP_FROM}/bootflop/vmlinuz-bootflop-*  pxe
  scp -p ${SCP_FROM}/rootflop/rootflop-*.img pxe
  if [ -f     ${SCP_FROM}/dmi/version.txt ] ; then
    scp -p ${SCP_FROM}/dmi/version.txt dmi_version.txt
  elif[ -f     ${SCP_FROM}/junos/version.txt ]  && \
    # Note: "junos" has been replaced with "dmi" in newer releases.
    scp -p ${SCP_FROM}/junos/version.txt junos_version.txt
  fi
  [ -f     ${SCP_FROM}/wms/version.txt ] && \
    scp -p ${SCP_FROM}/wms/version.txt wms_version.txt
  #[ -f     ${SCP_FROM}/vod/version.txt ] && \
  #  scp -p ${SCP_FROM}/vod/version.txt vod_version.txt
  scp -p ${SCP_FROM}/../mibs/* mibs/${MIBS_DIR}
else
  date
  echo cp -p ${CP_FROM}/mfgcd/mfgcd-*.iso .
  cp      -p ${CP_FROM}/mfgcd/mfgcd-*.iso .
  echo cp -p ${CP_FROM}/manufacture/manufacture-*.tgz pxe
  cp      -p ${CP_FROM}/manufacture/manufacture-*.tgz pxe
  echo cp -p ${CP_FROM}/bootflop/vmlinuz-bootflop-*  pxe
  cp      -p ${CP_FROM}/bootflop/vmlinuz-bootflop-*  pxe
  echo cp -p ${CP_FROM}/rootflop/rootflop-*.img pxe
  cp      -p ${CP_FROM}/rootflop/rootflop-*.img pxe
  if [ -f      ${CP_FROM}/dmi/version.txt ] ; then
    echo cp -p ${CP_FROM}/dmi/version.txt dmi_version.txt
    cp      -p ${CP_FROM}/dmi/version.txt dmi_version.txt
  elif [ -f      ${CP_FROM}/junos/version.txt ] ; then
    # Note: "junos" has been replaced with "dmi" in newer releases.
    echo cp -p ${CP_FROM}/junos/version.txt junos_version.txt
    cp      -p ${CP_FROM}/junos/version.txt junos_version.txt
  fi
  if [ -f      ${CP_FROM}/wms/version.txt ] ; then
    echo cp -p ${CP_FROM}/wms/version.txt wms_version.txt
    cp      -p ${CP_FROM}/wms/version.txt wms_version.txt
  fi
  if [ -f wms_version.txt ] ; then
    WV=`cat wms_version.txt | grep ^PKG_FILE= | cut -f2 -d=`
    [ -z "${WV}" ] && rm wms_version.txt
  fi
  # The VOD stuff is not going to be used.
  #if [ -f      ${CP_FROM}/vod/version.txt ] ; then
  #  echo cp -p ${CP_FROM}/vod/version.txt vod_version.txt
  #  cp      -p ${CP_FROM}/vod/version.txt vod_version.txt
  #fi
  echo cp -p ${CP_FROM}/../mibs/* mibs/${MIBS_DIR}
  cp      -p ${CP_FROM}/../mibs/* mibs/${MIBS_DIR}
  date
  echo
fi
ERR=0
DMI_UPLOAD=
JUNOS_UPLOAD=
WMS_UPLOAD=
VOD_UPLOAD=
JVF=
VVF=
if [ ! -f pxe/manufacture-*.tgz ] ; then
  ERR=1
  echo manufacture-*.tgz not downloaded
else
  MY_VER=`basename pxe/manufacture-*.tgz | sed "s/manufacture-//" | sed "s/.tgz//"`
  # Create the file "additional" text file that specifies the filenames.
  # with DMI_FILE_NAME= and WMS_FILE_NAME=
  # (Obsolete JUNOS_FILE_NAME= and VOD_FILE_NAME)
  echo "#Additional package files - ${MY_VER}" > pxe/additional-${MY_VER}.txt
  ADD_CNT=0
  ADD_LIST=
  #
  THIS=dmi
  THAT=DMI
  VF=${THIS}_version.txt
  PKG_FILE=
  if [ -f ${VF} ] ; then
    JVF=${VF}
    . ${VF}
    if [ ! -z "${PKG_FILE}" ] ; then
      if [ -f "${PKG_FILE}" ] ; then
        BASE_NAME=`basename ${PKG_FILE}`
        ADD_LIST="${ADD_LIST} ${BASE_NAME}"
        echo ${THAT}_FILE_NAME=${BASE_NAME} >> pxe/additional-${MY_VER}.txt
        ADD_CNT=`expr ${ADD_CNT} + 1`
      fi
    fi
  fi
  DMI_UPLOAD=${PKG_FILE}

  # For use for old releases before "junos" reference was changed to "dmi".
  THIS=junos
  THAT=JUNOS
  VF=${THIS}_version.txt
  PKG_FILE=
  if [ -f ${VF} ] ; then
    JVF=${VF}
    . ${VF}
    if [ ! -z "${PKG_FILE}" ] ; then
      if [ -f "${PKG_FILE}" ] ; then
        BASE_NAME=`basename ${PKG_FILE}`
        ADD_LIST="${ADD_LIST} ${BASE_NAME}"
        echo ${THAT}_FILE_NAME=${BASE_NAME} >> pxe/additional-${MY_VER}.txt
        ADD_CNT=`expr ${ADD_CNT} + 1`
      fi
    fi
  fi
  JUNOS_UPLOAD=${PKG_FILE}

  THIS=wms
  THAT=WMS
  VF=${THIS}_version.txt
  PKG_FILE=
  if [ -f ${VF} ] ; then
    VVF=${VF}
    . ${VF}
    if [ ! -z "${PKG_FILE}" ] ; then
      if [ -f "${PKG_FILE}" ] ; then
        BASE_NAME=`basename ${PKG_FILE}`
        ADD_LIST="${ADD_LIST} ${BASE_NAME}"
        echo ${THAT}_FILE_NAME=${BASE_NAME} >> pxe/additional-${MY_VER}.txt
        ADD_CNT=`expr ${ADD_CNT} + 1`
      fi
    fi
  fi
  WMS_UPLOAD=${PKG_FILE}
  
  # The VOD stuff is not going to be used.
  #THIS=vod
  #THAT=VOD
  #VF=${THIS}_version.txt
  #PKG_FILE=
  #if [ -f ${VF} ] ; then
  #  VVF=${VF}
  #  . ${VF}
  #  if [ ! -z "${PKG_FILE}" ] ; then
  #    if [ -f "${PKG_FILE}" ] ; then
  #      BASE_NAME=`basename ${PKG_FILE}`
  #      ADD_LIST="${ADD_LIST} ${BASE_NAME}"
  #      echo ${THAT}_FILE_NAME=${BASE_NAME} >> pxe/additional-${MY_VER}.txt
  #      ADD_CNT=`expr ${ADD_CNT} + 1`
  #    fi
  #  fi
  #fi
  #VOD_UPLOAD=${PKG_FILE}
  
  # When no additional files, remove the additional-*.txt file
  if [ ${ADD_CNT} -eq 0 ] ; then
    rm pxe/additional-${MY_VER}.txt
  else
    for i in ${DMI_UPLOAD} ${JUNOS_UPLOAD} ${WMS_UPLOAD} ${VOD_UPLOAD}
    do
      [ ! -f ${i} ] && continue
      echo Copy $i for pxe
      cp -a ${i} pxe/
    done
  fi
fi

if [ ! -f pxe/vmlinuz-bootflop-* ] ; then
  ERR=1
  echo vmlinuz-bootflop-* not downloaded
fi
if [ ! -f pxe/rootflop-*.img ] ; then
  ERR=1
  echo rootflop-*.tgz not downloaded
fi
if [ -f mfgcd-*.iso ] ; then
  CD_ISO=`ls mfgcd-*.iso`
else
  CD_ISO=
  echo WARNING: Did not download the mfgcd-*.iso file
fi
if [ -f mibs/${MIBS_DIR}/mibs-sums.txt ] ; then
  GOT_MIBS=yes
else
  GOT_MIBS=no
  echo WARNING: mibs files not downloaded
fi

#
[ ${ERR} = 1 ] && exit 1
cp ${IMG_FILE_NAME} pxe
echo Create ${PXE_FILE_NAME}
cd pxe
tar czf ../${PXE_FILE_NAME} *
cd ..
echo
# Now create the additional stuff tar
if [ -f pxe/additional-${MY_VER}.txt ] ; then
  echo Create ${ADD_FILE_NAME}
  cd pxe
  tar czf ../${ADD_FILE_NAME} additional-${MY_VER}.txt ${ADD_LIST}
  cd ..
fi
echo
if [ "${GOT_MIBS}" = "yes" ] ; then
  echo Create ${MIBS_FILE_NAME}
  cd mibs
  tar zcvf ../${MIBS_FILE_NAME} ${MIBS_DIR}
  cd ..
  echo
else
  MIBS_FILE_NAME=
fi
#

# Create sums for the package files.
Fsums=sums-${RELEASE_VER}.txt
Fmd5sums=md5sums-${RELEASE_VER}.txt
Fsha1sums=sha1sums-${RELEASE_VER}.txt
rm -f ${Fsums}
rm -f ${Fmd5sums}
rm -f ${Fsha1sums}
for i in *-${RELEASE_VER}*
do
 SUM=`sum ${i}`
 echo "${SUM} ${i}" >> ${Fsums}

 SUM=`md5sum ${i}`
 echo "${SUM}" | cut -f1 -d' ' > ${i}.md5
 echo "${SUM}" >> ${Fmd5sums}

 SUM=`sha1sum ${i}`
 echo "${SUM}" | cut -f1 -d' ' > ${i}.sha1
 echo "${SUM}" >> ${Fsha1sums}
done  

# Create the index files
rm -rf index-${RELEASE_VER}.list
rm -rf index-${RELEASE_VER}.html
echo '<html><head><title>'    > index-${RELEASE_VER}.html
echo "${RELEASE_VER}"        >> index-${RELEASE_VER}.html
echo '</title></head><body>' >> index-${RELEASE_VER}.html
echo "${RELEASE_VER}"        >> index-${RELEASE_VER}.html
echo '<ul>'                  >> index-${RELEASE_VER}.html

# Put the file in a certain order and then pick up
LIST=
for i in image*img pxe*tgz mfgcd*.iso mibs*.tgz additional*.tgz tproxy* Tproxy* *-${RELEASE_VER}*
do
  [ -z "${i}" ] && continue
  [ ! -f "${i}" ] && continue
  case ${i} in
  sums*|md5sums*|sha1sums*) continue;;
  *.md5|*.sha1) continue ;;
  index*) continue ;;
  esac
  echo ${LIST} | grep -q ${i}
  [ ${?} -ne 0 ] && LIST="${LIST} ${i}"
done
LIST="${LIST} sums-${RELEASE_VER}.txt md5sums-${RELEASE_VER}.txt sha1sums-${RELEASE_VER}.txt index-${RELEASE_VER}.list"

for i in ${LIST}
do
  [ ! -f ${i} ] && continue
  echo '<li><a href="'${i}'">'${i}'</a>' >> index-${RELEASE_VER}.html
  echo 'https://download.juniper.net/software/mediaflow/'${i} >> index-${RELEASE_VER}.list
  if [ -f ${i}.md5 ] ; then
    echo '  (<a href="'${i}.sha1'">'md5'</a>)' >> index-${RELEASE_VER}.html
    echo 'https://download.juniper.net/software/mediaflow/'${i}.md5 >> index-${RELEASE_VER}.list
  fi
  if [ -f ${i}.sha1 ] ; then
    echo '  (<a href="'${i}.sha1'">'sha1'</a>)' >> index-${RELEASE_VER}.html
    echo 'https://download.juniper.net/software/mediaflow/'${i}.sha1 >> index-${RELEASE_VER}.list
  fi
  case ${i} in
  image*) echo ' Upgrade Image' >> index-${RELEASE_VER}.html ;;
  pxe*)   echo ' PXE Installation' >> index-${RELEASE_VER}.html ;;
  mfgcd*) echo ' ISO Installation' >> index-${RELEASE_VER}.html ;;
  mibs*)  echo ' MIBs' >> index-${RELEASE_VER}.html ;;
  addit*) echo ' Additional installation files' >> index-${RELEASE_VER}.html ;;
  tproxy_*setup*|Tproxy_*Setup*) echo ' Reference Setup Configuration' >> index-${RELEASE_VER}.html ;;
  tproxy_*namespace*|Tproxy_*Namespace*) echo ' Reference Setup Configuration' >> index-${RELEASE_VER}.html ;;
  sums*)  echo ' BSD sum Checksums' >> index-${RELEASE_VER}.html ;;
  md5*)   echo ' MD5 Checksums' >> index-${RELEASE_VER}.html ;;
  sha1*)  echo ' SHA1 Checksums' >> index-${RELEASE_VER}.html ;;
  index*) echo ' List of files' >> index-${RELEASE_VER}.html ;;
  esac
  echo '</li>' >> index-${RELEASE_VER}.html
done
echo '</ul>'          >> index-${RELEASE_VER}.html
echo '</body></html>' >> index-${RELEASE_VER}.html
echo 'https://download.juniper.net/software/mediaflow/'index-${RELEASE_VER}.html >> index-${RELEASE_VER}.list

echo :::::::::::::::::::::::::::::::::::::::::::::::
echo Files created here
ls -l ${IMG_FILE_NAME} ${CD_ISO} ${PXE_FILE_NAME} ${MIBS_FILE_NAME} \
  ${JVF} ${VVF} *.md5 *.sha1 *sums-*.txt index-*
  

echo :::::::::::::::::::::::::::::::::::::::::::::::
echo PXE tar file contents:
tar tvf ${PXE_FILE_NAME}
echo :::::::::::::::::::::::::::::::::::::::::::::::
if [ ! -z ${JVF} ] ; then
  echo Contents of ${JVF}
  cat  ${JVF}
  echo :::::::::::::::::::::::::::::::::::::::::::::::
fi
if [ ! -z ${VVF} ] ; then
  echo Contents of ${VVF}
  cat  ${VVF}
  echo :::::::::::::::::::::::::::::::::::::::::::::::
fi
if [ -f pxe/additional-${MY_VER}.txt ] ; then
  echo Contents of pxe/additional-${MY_VER}.txt
  cat pxe/additional-${MY_VER}.txt
fi
echo :::::::::::::::::::::::::::::::::::::::::::::::
echo Contents of sums-*.txt
cat sums-*.txt
echo :::::::::::::::::::::::::::::::::::::::::::::::
echo Contents of md5sums-*.txt
cat md5sums-*.txt
echo :::::::::::::::::::::::::::::::::::::::::::::::
echo Contents of sha1sums-*.txt
cat sha1sums-*.txt
echo :::::::::::::::::::::::::::::::::::::::::::::::

echo Finished bundling ${RELEASE_VER}
REL_NAME=`echo ${RELEASE_VER} | cut -f1 -d_`
if [ -f ${MY_DIR}/smb-mf-upload.sh ] ; then
  echo  ${MY_DIR}/smb-mf-upload.sh -d ${REL_NAME} '${*}' ${RELEASE_VER} > do-this-upload.sh
  echo  ${MY_DIR}/cp-mf-upload.sh -d ${REL_NAME} '${*}' ${RELEASE_VER} > do-this-upload-cp.sh
  chmod +x do-this-upload*.sh
elif [ -f /eng/bin-upload/smb-mf-upload.sh ] ; then
  echo    /eng/bin-upload/smb-mf-upload.sh -d ${REL_NAME} '${*}' ${RELEASE_VER} > do-this-upload.sh
  echo    /eng/bin-upload/cp-mf-upload.sh -d ${REL_NAME} '${*}' ${RELEASE_VER} > do-this-upload-cp.sh
  chmod +x do-this-upload*.sh
elif [ -f ~/bin-upload/smb-mf-upload.sh ] ; then
  echo    ~/bin-upload/smb-mf-upload.sh -d ${REL_NAME} '${*}' ${RELEASE_VER} > do-this-upload.sh
  echo    ~/bin-upload/cp-mf-upload.sh -d ${REL_NAME} '${*}' ${RELEASE_VER} > do-this-upload-cp.sh
  chmod +x do-this-upload*.sh
else
  echo NOT creating do-this-upload.sh, could not find smb-mf-upload.sh
fi
