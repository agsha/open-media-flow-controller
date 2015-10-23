:
# Copy the image and related files to the directory where where the cron job
# looks for image files to put into the pxe-boot menu.
#
ADD_USER_NAME=yes
ENABLE_DMI=on
#ENABLE_VOD=off
#ENABLE_WMS=off
PXE_MENU_ENABLE=off
EULA_Q_ENABLE=off
INSTALL_OPTS=
DRY_RUN=off
export RC_OPT=
#VERSION_STRING=

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

Help()
{
  echo Syntax:
  echo '     <options> <dir> <short-name> <description>'
  echo '<dir> : The directory to copy from.  Use "-" for current PROD_OUTPUT_ROOT.'
  echo '<shortname>   : a short name for this for the pxe menu.'
  echo '<description> : the description for the pxe menu.'
  echo 'The "<options>" zero or more of:'
  echo '  "--noAddName"   : to not prepend the user name to the menu name.'
  echo '  "-s"            : to not prepend the user name to the menu name.'
  echo '  "--dmi-on"      : Include the MFC-DMI (vjx) image file if available.'
  echo '  "--dmi-off"     : Do NOT include the MFC-DMI (vjx) image file.'
  #echo '  "--wms-on"      : Include the WMS package file if available.'
  #echo '  "--wms-off"     : Do NOT include the WMS package file.'
  echo '  "--pxemenu-on"  : Use the show-pxe-menu in install-opts'
  echo '  "--pxemenu-off" : Not use the show-pxe-menu in install-opts'
  echo '  "--eula-off"    : Use the accept-eula in install-opts'
  echo '  "--eula-on"     : Not use the accept-eula in install-opts'
  echo '  "--install-opts=<install-options>" : comma delimited list'
  echo 'All the install-options:  (comma delimited)'
  echo '# accept-eula    -- Do not show the EULA menu.'
  echo '# auto-eth       -- Automatically do eth-setup with the ethsetup= setting.'
  echo '# auto-install   -- Completely automatic install, not even log in.'
  echo '# auto-reboot    -- When install completes, automatically reboot.'
  echo '# ethsetup=<eth-setup parameter>'
  echo '# init-cache     -- Automatically do the install with init-cache'
  echo '# keep-cache     -- Automatically do the install with keep-cache'
  echo '# show-pxe-menu  -- Show the URL menu when was pxe-booted.'
  echo '# Special options for manufacture:'
  echo '#   disk-layout= -- Specify disk-layout name to use'
  echo '#   root-dev=    -- Specify the root dev (and pdev)'
  echo '#   eusb-dev=    -- Specify the eusb dev'
  echo '#   max-fs=      -- Maximum filesystem size on root drive.'
  echo '#   coredump-part= -- size, grow-weight, max for /coredump partition.'
  echo '#   swap-part=   -- size, grow-weight, max for /swap partition.'
  echo '#   root-part=   -- size, grow-weight, max for the /root partitions.'
  echo '#   config-part= -- size, grow-weight, max for /config partition.'
  echo '#   var-part=    -- size, grow-weight, max for /var partition.'
  echo '#   buf-part=    -- size, grow-weight, max for /nkn partition.'
  echo '#   cache-part=  -- size, grow-weight, max for cache partition.'
  echo '#   log-part=    -- size, grow-weight, max for /log partition.'
  echo 'Special options for release control builds:'
  echo '  "--rc=<type>" : For release-control builds, specify the type.'
  echo 'Debugging options:'
  echo '  "--dryrun"    : Do not actually copy files to the pxe server'
  echo 'Default:'
  echo "--dmi-${ENABLE_DMI}" "--pxemenu-${PXE_MENU_ENABLE}" "--eula-${EULA_Q_ENABLE}"
  #echo "--wms-${ENABLE_WMS}"
}
TO_LIST=
case "_${1}" in
_-h|_--help) Help ; exit 1 ;;
esac
while true ; do
  [ -z "${1}" ] && break
  case ${1} in
  #--ver|-v)         VERSION_STRING="${2}"
  #                  shift; shift
  #                  ;;
  --noadd*|--noAdd*|-s) ADD_USER_NAME=no
                    shift
                    ;;
  --rc=*)           RC_OPT=`echo ${1} | cut -f2 -d=`
                    shift
                    ;;
  --dmi-off)        ENABLE_DMI=off
                    shift
                    ;;
  --dmi-on)         ENABLE_DMI=on
                    shift
                    ;;
  #--wms-off)        ENABLE_WMS=off
  #                  shift
  #                  ;;
  #--wms-on)         ENABLE_WMS=on
  #                  shift
  #                  ;;
  --pxemenu-off)    PXE_MENU_ENABLE=off
                    shift
                    ;;
  --pxemenu-on)     PXE_MENU_ENABLE=on
                    shift
                    ;;
  --eula-off|--accept-eula) EULA_Q_ENABLE=off
                    shift
                    ;;
  --eula-on|--show-eula)    EULA_Q_ENABLE=on
                    shift
                    ;;
  --install-opts)   INSTALL_OPTS=${2}
                    shift; shift
                    ;;
  --install-opts=*) INSTALL_OPTS=`echo ${1} | cut -f2- -d=`
                    shift
                    ;;
  --dryrun=*|--dry-run=*)  DRY_RUN=`echo ${1} | cut -f2- -d=`
                    shift
                    ;;
  --dry*)           DRY_RUN=on
                    shift
                    ;;
  --*)              echo Unknown option ${1}
                    exit 1
                    ;;
  *)                break
                    ;;
  esac
done
if [ "_${1}" = "_" -o "_${2}" = "_" ] ; then
    Help
    exit 1
fi 
PXE_OPTS=,
[ "${PXE_MENU_ENABLE}" = "on"  ] && PXE_OPTS=${PXE_OPTS}show-pxe-menu,
[ "${EULA_Q_ENABLE}"   = "off" ] && PXE_OPTS=${PXE_OPTS}accept-eula,
[ ! -z "${INSTALL_OPTS}" ] && PXE_OPTS=${PXE_OPTS}${INSTALL_OPTS},

if [ "${DRY_RUN}" = "off" ] ; then
  TO_LIST="/home2/pxe-boot"
  if [ ! -d ${TO_LIST} ] ; then
    TO_LIST="/home2/tftpboot/pxe-boot"
    if [ ! -d ${TO_LIST} ] ; then
      echo Error, /home2/pxe-boot and /home2/tftpboot/pxe-boot is not available.
      echo Cannot proceed.
      exit 1
    fi
  fi
else 
  if [ "${DRY_RUN}" = "on" ] ; then
    TO_LIST=/tmp/dryrun-copytopxe
  else
    TO_LIST=${DRY_RUN}
  fi
  if [ ! -d ${TO_LIST} ] ; then
    mkdir /tmp/dryrun
  fi
fi

FROM_DIR=${1}
SHORT_NAME=${2}
shift
shift
PREPEND=
if [ "${ADD_USER_NAME}" = "yes" ] ; then
  PREPEND="${REAL_USER}-"
fi
MENU_NAME=${PREPEND}${SHORT_NAME}
D1="${1}"
DESC="${@}"
# Put the menu name in the description unless it is already there.
echo "${DESC}" | grep -q "^${MENU_NAME} "
if [ ${?} -ne 0 ] ; then
  DESC="${MENU_NAME} ${DESC}"
fi

Set_Out_Dir()
{
  [ ! -z "${PROD_OUTPUT_ROOT}" ] && return
  echo PROD_OUTPUT_ROOT not set.
  SAVE_PWD=`pwd`
  if [ ! -z "${PROD_CUSTOMER_ROOT}" ] ; then
      cd ${PROD_CUSTOMER_ROOT}
      # Get settings from file :env.sh
      Get_Env_Settings_From_File
  fi
  if [ ! -z "${PROD_OUTPUT_ROOT}" ] ; then
    echo Using PROD_OUTPUT_ROOT from :env.sh via PROD_CUSTOMER_ROOT=${PROD_CUSTOMER_ROOT}
    export PROD_OUTPUT_ROOT
    return
  fi
  cd "${SAVE_PWD}"
  Get_Env_Settings_From_File
  if [ ! -z "${PROD_OUTPUT_ROOT}" ] ; then
    echo Using PROD_OUTPUT_ROOT from :env.sh via ${SAVE_PWD}
    export PROD_OUTPUT_ROOT
    return
  fi
  echo Cannot find env.sh from PROD_CUSTOMER_ROOT setting or current directory,
  echo so cannot deduce PROD_OUTPUT_ROOT.
  exit 1
}

Copy_Link_Additional()
{
  [ ! -f ${VF} ] && return
  PKG_FILE=
  . ${VF}
  [ ! -f "${PKG_FILE}" ] && return
  ADD_CNT=`expr ${ADD_CNT} + 1`
  BASE_NAME=`basename ${PKG_FILE}`
  if [ -f ../${THIS}/${BASE_NAME} ] ; then
    echo Already uploaded ${THIS}/${BASE_NAME}
  else
    echo cp ${PKG_FILE} ../${THIS}/${BASE_NAME}
    [ "${DRY_RUN}" = "off" ] && \
    cp ${PKG_FILE} ../${THIS}/${BASE_NAME}
  fi
  echo ${THAT}_FILE_NAME=${BASE_NAME} >> additional-${MY_VER}.txt
  rm -f ${BASE_NAME}
  echo ln -s ../${THIS}/${BASE_NAME} ${BASE_NAME}
  ln -s ../${THIS}/${BASE_NAME} ${BASE_NAME}
  rm -f ${SHORT_FILENAME}
  echo ln -s ${BASE_NAME} ${SHORT_FILENAME}
  ln -s ${BASE_NAME} ${SHORT_FILENAME}
}

Copy_Files()
{
  THIS_TOP=${1}
  TO_DIR=${1}/${MENU_NAME}
  echo TO_DIR="${TO_DIR}"
  
  if [ "_${FROM_DIR}" = "_-" ] ; then
    Set_Out_Dir
    FROM_DIR=${PROD_OUTPUT_ROOT}
  fi
  TRY1=${FROM_DIR}
  if [ ! -d ${FROM_DIR}/bootflop ] ; then
      TRY2=${FROM_DIR}/release
      TRY3=${FROM_DIR}/product-*-x86_64/release
      TRY4=${FROM_DIR}/product-*-I386/release
      if [    -d ${FROM_DIR}/release/bootflop ] ; then
        FROM_DIR=${FROM_DIR}/release
        TRY3=
        TRY4=
      else
        if [    -d ${FROM_DIR}/product-*-x86_64/release/bootflop ] ; then
          FROM_DIR=${FROM_DIR}/product-*-x86_64/release
          TRY4=
        elif [  -d ${FROM_DIR}/product-*-I386/release/bootflop ] ; then
          FROM_DIR=${FROM_DIR}/product-*-I386/release
        fi
      fi
  fi
  for i in ${FROM_DIR} ${FROM_DIR}/bootflop ${FROM_DIR}/rootflop
  do
      [ -d ${i} ] && continue
      echo No such directory ${i}
      echo Incomplete or invalid from directory.  Tried:
      echo ${TRY1}
      echo ${TRY2}
      echo ${TRY3}
      echo ${TRY4}
      exit 1
  done
  
  ERR=0
  for i in rootflop/rootflop- bootflop/vmlinuz-bootflop- image/image-
  do
      COUNT=`ls ${FROM_DIR}/${i}* | wc -l`
      if [ ${COUNT} -eq 0 ] ; then
          echo Missing ${FROM_DIR}/${i}'*'
          ERR=1
      elif [ ${COUNT} -ne 1 ] ; then
          echo Too many files ${FROM_DIR}/${i}'*'
          ls ${FROM_DIR}/${i}*
          ERR=1
      fi
  done
  if [ ${ERR} -ne 0 ] ; then
      echo Error
      echo Not doing anything
      exit 1
  fi
  
  if [ ! -d "${TO_DIR}" ]; then
    mkdir -p  "${TO_DIR}"
    chmod 755 "${TO_DIR}"
  fi
  if [ ! -d ${TO_DIR} ]; then
    echo Error, cannot create directory ${TO_DIR}
    exit 1
  fi
  sudo /bin/chown -R ${REAL_USER} ${TO_DIR} 2> /dev/null
  chmod -R 755 ${TO_DIR}
  rm -f ${TO_DIR}/rootflop-*
  rm -f ${TO_DIR}/vmlinuz-*
  rm -f ${TO_DIR}/image-*
  rm -f ${TO_DIR}/manufacture-*
  rm -f ${TO_DIR}/additional-*
  rm -f ${TO_DIR}/dmi-*
  rm -f ${TO_DIR}/junos-*
  rm -f ${TO_DIR}/wms-*
  rm -f ${TO_DIR}/description.txt
  rm -f ${TO_DIR}/pxe-opts.txt
  rm -f ${TO_DIR}/rc-opt.txt
  rm -f ${TO_DIR}/ver.txt
  rm -f ${TO_DIR}/envs.txt
  rm -rf ${TO_DIR}/Done
  for i in ${TO_DIR}/description.txt ${TO_DIR}/pxe-opts.txt ${TO_DIR}/rc-opt.txt ${TO_DIR}/ver.txt ${TO_DIR}/envs.txt
  do
    if [ -f ${i} ] ; then
      echo Error, Failed to remove previous ${i}
      exit 1
    fi
  done
  
  EC=0
  echo cp ${FROM_DIR}/rootflop/rootflop-*.img ${TO_DIR}
  [ "${DRY_RUN}" = "off" ] && \
  cp      ${FROM_DIR}/rootflop/rootflop-*.img ${TO_DIR}
  EC=`expr ${?} + ${EC}`
  echo cp ${FROM_DIR}/bootflop/vmlinuz-bootflop-* ${TO_DIR}
  [ "${DRY_RUN}" = "off" ] && \
  cp      ${FROM_DIR}/bootflop/vmlinuz-bootflop-* ${TO_DIR}
  EC=`expr ${?} + ${EC}`
  echo cp ${FROM_DIR}/image/image-*.img ${TO_DIR}
  [ "${DRY_RUN}" = "off" ] && \
  cp      ${FROM_DIR}/image/image-*.img ${TO_DIR}
  EC=`expr ${?} + ${EC}`
  if [ -f ${FROM_DIR}/manufacture/manufacture-.tgz ] ; then 
    echo Warning, fixing up incorrectly named manufacture.tgz file
    NEW=`echo ${FROM_DIR}/image/image-*.img | sed -e "s#/image/image-#/manufacture/manufacture-#" | sed -e "s/\.img$/.tgz/"`
    echo mv ${FROM_DIR}/manufacture/manufacture-.tgz ${NEW}
    mv      ${FROM_DIR}/manufacture/manufacture-.tgz ${NEW}
  fi
  if [ -f ${FROM_DIR}/manufacture/manufacture-*.tgz ] ; then 
    echo cp ${FROM_DIR}/manufacture/manufacture-*.tgz ${TO_DIR}
    [ "${DRY_RUN}" = "off" ] && \
    cp      ${FROM_DIR}/manufacture/manufacture-*.tgz ${TO_DIR}
    EC=`expr ${?} + ${EC}`
  fi
  if [ "${DRY_RUN}" = "off" ] ; then
    if [ ${EC} -ne 0 ] ; then
      echo Error, failed to copy the files to ${TO_DIR}
      exit 1
    fi
  fi
  MY_VER=`basename ${FROM_DIR}/manufacture/manufacture-*.tgz | sed "s/manufacture-//" | sed "s/.tgz//"`
  #if [ "${ENABLE_DMI}.${ENABLE_VOD}.${ENABLE_WMS}" != "off.off.off" ]
  if [ "${ENABLE_DMI}" != "off" ] ; then
    # Copy or link the DMI, VOD and WMS package files.
    # Create the file "additional.txt" that specifies the filenames
    # with DMI_FILE_NAME= VOD_FILE_NAME= and/or WMS_FILE_NAME
    cd ${TO_DIR}
    echo "#Additional package files - ${MY_VER}" > additional-${MY_VER}.txt
    ADD_CNT=0
    if [ "${ENABLE_DMI}" = "on" ] ; then
      THIS=dmi
      THAT=DMI
      SHORT_FILENAME=dmi.img.zip
      VF=${FROM_DIR}/${THIS}/version.txt
      Copy_Link_Additional
      # For older versions, the file is names "junos*"
      THIS=junos
      THAT=JUNOS
      SHORT_FILENAME=junos.img.zip
      VF=${FROM_DIR}/${THIS}/version.txt
      Copy_Link_Additional
    fi
    #if [ "${ENABLE_WMS}" = "on" ] ; then
    #  THIS=wms
    #  THAT=WMS
    #  SHORT_FILENAME=wms.img
    #  VF=${FROM_DIR}/${THIS}/version.txt
    #  Copy_Link_Additional
    #fi
    # If nothing was added then delete the additional-*.txt file.
    [ ${ADD_CNT} -eq 0 ] && rm additional-${MY_VER}.txt
  fi
  
  rm -f ${TO_DIR}/ver.txt
  DS=
  INFO_DIR=
    
  for E_FILE in  \
    ${FROM_DIR}/../info/envs.txt \
    ${FROM_DIR}/../log/info/envs.txt \
    ${FROM_DIR}/../envs.txt \
    ${FROM_DIR}/../*/envs.txt \
    ${FROM_DIR}/../../envs.txt \
    ${FROM_DIR}/../../*/envs.txt 
  do
    [ -f ${E_FILE} ] && break
  done
  if [ -f ${E_FILE} ] ; then
    echo Found ${E_FILE}
    ls -l ${E_FILE}
    DS=`grep Build_Time= ${E_FILE} | cut -f1 -d_`
    cp ${E_FILE} ${TO_DIR}
    touch -r ${E_FILE} ${TO_DIR}/envs.txt
    echo touch -r ${E_FILE} ${TO_DIR}/envs.txt
    ls -l ${TO_DIR}/envs.txt
  else
    echo NotFound: ${E_FILE}
    echo PROD_OUTPUT_ROOT=${PROD_OUTPUT_ROOT}      > ${TO_DIR}/envs.txt
    echo PROD_CUSTOMER_ROOT=${PROD_CUSTOMER_ROOT} >> ${TO_DIR}/envs.txt
    echo PROD_TREE_ROOT=${PROD_TREE_ROOT}         >> ${TO_DIR}/envs.txt
    touch -r ${FROM_DIR}/image/image-*.img           ${TO_DIR}/envs.txt
    echo touch -r ${FROM_DIR}/image/image-*.img
    ls -l ${TO_DIR}/envs.txt
  fi
  if [ ! -f ${TO_DIR}/envs.txt ] ; then
    echo Failed to create ${TO_DIR}/envs.txt
    exit 1
  fi
  if [ -z "${DS}" ] ; then
    DS=`ls -l --time-style=+%Y-%m-%d ${TO_DIR}/envs.txt | cut -f 6 -d' '`
  fi
  cd ${TO_DIR}
  V=`echo vmlin* | cut -f3- -d-`
  if [ "_${D1}" = "_" ] ; then
      echo "`basename ${TO_DIR}` ${V} ${DS}" > ${TO_DIR}/description.txt
  else
      DS=" ${DS}"
      echo "${DESC}" | grep " 2010-" > /dev/null
      [ ${?} -eq 0 ] && DS=""
      echo "${DESC}" | grep " 2011-" > /dev/null
      [ ${?} -eq 0 ] && DS=""
      echo "${DESC}" | grep " 2012-" > /dev/null
      [ ${?} -eq 0 ] && DS=""
      echo "${DESC}" | grep " 2013-" > /dev/null
      [ ${?} -eq 0 ] && DS=""
      echo "${DESC}" | grep " 2014-" > /dev/null
      [ ${?} -eq 0 ] && DS=""
      echo "${DESC}${DS}" > ${TO_DIR}/description.txt
  fi
  echo "${PXE_OPTS}" > ${TO_DIR}/pxe-opts.txt
  [ ! -z "${RC_OPT}" ] && echo "${RC_OPT}" > ${TO_DIR}/rc-opt.txt
  # Now create the image link
  rm -f image.img
  if [ -f image-*.img ] ; then
    rm -f                   image.img
    ln -s image-*.img       image.img
  fi
  rm -f pxe_rootfs.img
  if [ -f rootflop-*.img ] ; then
    rm -f                   pxe_rootfs.img
    ln -s rootflop-*.img    pxe_rootfs.img
  fi
  rm -f pxe_kernel.img
  if [ -f vmlin*-bootflop-* ] ; then
    rm -f                   pxe_kernel.img
    ln -s vmlin*-bootflop-* pxe_kernel.img
  fi
  rm -f manufacture.tgz
  if [ -f manufacture-*.tgz ] ; then
    rm -f                   manufacture.tgz
    ln -s manufacture-*     manufacture.tgz
  fi
  rm -f additional.txt
  if [ -f additional-*.txt ] ; then
    rm -f                   additional.txt
    ln -s additional-*      additional.txt
  fi
  sudo /bin/chown ${REAL_USER} ${TO_DIR} ${TO_DIR}/* 2> /dev/null
  chmod 444 ${TO_DIR}/*.* 2> /dev/null

  # Now update the time-stamp file so that the script
  # /eng/bin-pxeserver/update-pxe.sh will see the change.
  A=`sum ${1}/update-timestamp.txt`
  chmod 666 ${1}/update-timestamp.txt 2> /dev/null
  date >    ${1}/update-timestamp.txt
  chmod 666 ${1}/update-timestamp.txt 2> /dev/null
  B=`sum ${1}/update-timestamp.txt`
  if [ "${A}" = "${B}" ] ; then
    echo
    echo ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !
    echo Failed to update ${1}/update-timestamp.txt
    echo The PXE menu may not get updated.
    echo ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! ! !
    echo
  fi
  sudo /bin/chown 701:701 ${1}/update-timestamp.txt 2> /dev/null
}

for TOP_DIR in ${TO_LIST}
do
  Copy_Files ${TOP_DIR}
done
echo Using menu name:" ${MENU_NAME}"
echo Using description:" `cat ${TO_DIR}/description.txt`"
echo Using pxe-opts:" ${PXE_OPTS}"
