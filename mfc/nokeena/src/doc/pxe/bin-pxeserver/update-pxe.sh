#!/bin/bash
# Update the PXE boot menu system from the contents of the
# standard and automatic dirs.
ARG="${1}"
export APPEND_OPTS="panic=10 noexec=off console=tty0 ramdisk_size=61440 quiet loglevel=4 rw nodmraid pci=noaer"

case `uname -n` in
*Xsv01*)
        SERVER_IP=10.157.42.52
        SERVER_IP2=
        PXE_TOP_DIR=/tftpboot
        AUTO_SUBDIR=Automatic
        ;;
*sv02*)
        SERVER_IP=10.157.42.224
        SERVER_IP2=172.18.172.57
        PXE_TOP_DIR=/home2/tftpboot
        AUTO_SUBDIR=pxe-boot
        ;;
cmbu-ixs2-3*)
        SERVER_IP=10.157.43.137
        SERVER_IP2=
        PXE_TOP_DIR=/tftpboot
        AUTO_SUBDIR=Automatic
        ;;
*)      echo Must be run on sv02 only.
        exit 1
        ;;
esac
AUTO_DIR=${PXE_TOP_DIR}/${AUTO_SUBDIR}
[ ! -d ${AUTO_DIR} ] && return


export WORK_DIR=${PXE_TOP_DIR}/work
export REL_CFG_DIR=pxelinux.cfg
export FULL_CFG_DIR=${PXE_TOP_DIR}/${REL_CFG_DIR}
export PROTO_DIR=${PXE_TOP_DIR}/proto
export SUBMENUS_DIR=${PXE_TOP_DIR}/submenus
export PREV_DIR=${PXE_TOP_DIR}/previous
[ ! -d ${WORK_DIR}     ] && mkdir -p ${WORK_DIR}
[ ! -d ${FULL_CFG_DIR} ] && mkdir -p ${FULL_CFG_DIR}
[ ! -d ${PROTO_DIR}    ] && mkdir -p ${PROTO_DIR}
[ ! -d ${SUBMENUS_DIR} ] && mkdir -p ${SUBMENUS_DIR}
[ ! -d ${PREV_DIR}     ] && mkdir -p ${PREV_DIR}
chmod 777 ${WORK_DIR}
chmod 777 ${FULL_CFG_DIR}
chmod 777 ${PROTO_DIR}
chmod 777 ${SUBMENUS_DIR}
chmod 777 ${PREV_DIR}

LOG_FILE=${WORK_DIR}/updm.log
echo >> ${LOG_FILE}
echo +++++++++++++++++++ `date` ++++++++++++++++++++ >> ${LOG_FILE}
echo >> ${LOG_FILE}

[ ! -f ${WORK_DIR}/last ] && echo none > ${WORK_DIR}/last
[ ! -f ${AUTO_DIR}/update-timestamp.txt ] && date > ${AUTO_DIR}/update-timestamp.txt
if [ -f ${WORK_DIR}/this ] ; then
  # Already in progress.
  echo Already in progress. Lock file ${WORK_DIR}/this
  exit 0
fi
cp ${AUTO_DIR}/update-timestamp.txt ${WORK_DIR}/this
cmp -s ${WORK_DIR}/this ${WORK_DIR}/last
RV=${?}
if [ ${RV} -eq 0 ] ; then
  # There was no change to the timestamp since last time.
  # When no change, do not update the menu when the -c option specified.
  if [ "_${ARG}" = "_-c" ] ; then
     rm ${WORK_DIR}/this
     exit 0
  fi
fi

F=${PROTO_DIR}/cfg.head
echo DEFAULT menu.c32      > ${F}
echo PROMPT 0              >> ${F}
echo SERIAL 0 9600         >> ${F}
echo MENU WIDTH 80         >> ${F}
echo MENU ENDROW 24        >> ${F}
echo MENU MARGIN 2         >> ${F}
echo MENU PASSWORDROW 11   >> ${F}
echo MENU PASSWORDMARGIN 3 >> ${F}
echo MENU ROWS 17          >> ${F}
echo MENU TABMSGROW 21     >> ${F}
echo MENU CMDLINEROW 1     >> ${F}
echo MENU TIMEOUTROW 24    >> ${F}

###############################################################################
Parse_Auto_Dir()
{
  [ ! -d ${AUTO_DIR} ] && return
  LISTING_HTML=${WORK_DIR}/list.html
  VERS_FILE=${WORK_DIR}/vers.txt
  ITEMS_1_FILE=${WORK_DIR}/items-autobuild.txt
  ITEMS_2_FILE=${WORK_DIR}/items2.txt
  ITEMS_3_FILE=${WORK_DIR}/items3.txt
  ITEMS_4_FILE=${WORK_DIR}/items4.txt
  ITEMS_5_FILE=${WORK_DIR}/items5.txt
  ITEMS_6_FILE=${WORK_DIR}/items6.txt
  ITEMS_7_FILE=${WORK_DIR}/items7.txt
  DEVELOPER_ITEMS_FILE=${WORK_DIR}/items-dev.txt
  RECENT_ITEMS_FILE=${WORK_DIR}/recent-dev.txt
  rm -f ${LISTING_HTML} ${VERS_FILE}
  rm -f ${ITEMS_1_FILE} ${ITEMS_2_FILE} ${ITEMS_3_FILE} ${ITEMS_4_FILE}
  rm -f ${ITEMS_5_FILE} ${ITEMS_6_FILE} ${ITEMS_7_FILE}
  rm -f ${DEVELOPER_ITEMS_FILE} ${RECENT_ITEMS_FILE}
  # First make the full listing file list of all the items.
  echo '<html><head><title>'Copyto-pxe list'</title></head><body>'              > ${LISTING_HTML}
  echo '<font size="+2">'Copyto-pxe list `date`'</font>'                        >> ${LISTING_HTML}
  echo '<table><tr><td>Name</td><td>Desc</td><td>Branch</td><td>Date</td></tr>' >> ${LISTING_HTML}
  cd ${AUTO_DIR}
  ls -lt */envs.txt | grep -v mfc- | head -32  | awk '{ print $9 }' | cut -f1 -d/ > ${RECENT_ITEMS_FILE}
  for THIS_LABEL in *
  do
    cd ${AUTO_DIR}
    if [ -z "${THIS_LABEL}" ] ; then
      echo Parse_Auto_Dir: no files in ${AUTO_DIR}
      continue
    fi
    echo "${THIS_LABEL}" | grep -q " "
    if [ ${?} -eq 0 ] ; then
      echo Cannot use \'${THIS_LABEL}\', it has spaces in the name
      continue
    fi
    [ ! -d "${THIS_LABEL}" ] && continue
    cd ${THIS_LABEL}
    for i in rootflop*
      do R=${i}
    done
    for i in vmlin*
      do K=${i}
    done
    [ ! -f ${R} ] && continue
    [ ! -f ${K} ] && continue
    # Settings that are set by envs.txt when an autobuild.
    # Build_Time: yyyy-mm-dd_hh:MM:ss
    Build_Time=
    # Branch: mainline, denali, cayoosh, ...
    Branch=
    # Build_Type:  "dv", "qa", "rc", "zm"
    Build_Type=
    # Revision: the SVN revision number
    Revision=
    # Full_Name: The official name of the build.
    # For rc builds, "-rc" does is NOT in the name.
    # Only for dv, qa, zm builds does "-dv", "-qa", "-zm" appear in the Full_Name.
    Full_Name=
    # Settings that are always set by envs.txt (by build-mine.sh)
    # NKN_BUILD_PROD_RELEASE, NKN_BUILD_ID, BUILD_CUSTOMER_SUFFIX, 
    [ -f envs.txt ] && . envs.txt
  
    if [ -f pxe-opts.txt ] ; then
       PXE_OPTS=installopt=`cat pxe-opts.txt`
    else
       PXE_OPTS=
    fi
  
    SVN_REV=unknown
    BRANCH=
    BDATE=
    if [ -f rc-opt.txt -a -f envs.txt ] ; then
      # Generate the description from info from the envs.txt file.
      BRIEF="${Full_Name}"
      BDATE=`echo "${Build_Time}" | cut -f1 -d_`
      BRANCH="${Branch}"
      SVN_REV="${Revision}"
      SPACES="                          "
      SPACES=${SPACES:${#BRIEF}}
      DESC="${BRIEF} ${SPACES}BR:${BRANCH} ${BDATE}"
    elif [ -f description.txt ] ; then
      DESC="`cat description.txt | head -1`"
      # Reduce all multiple white space to one space,
      # and remove leading and trailing spaces.
      LINE=`echo "${DESC}" | tr "[:blank:]" " " | tr -s " " | sed -e 's/^ //' -e 's/ \$//' `
      case ${LINE} in
      *\ BR:*\ 20[0-9][0-9]-[0-9][0-9]-[0-9][0-9])
        BRIEF=${LINE%% BR:*}
        BACK=${LINE#* BR:}
        BRANCH=${BACK%% *}
        BDATE=${BACK#* }
        SVN_REV=`echo ${BRIEF} | cut -f2 -d_`
        ;;
      *\ 20[0-9][0-9]-[0-9][0-9]-[0-9][0-9])
        LEN=${#LINE}
        P1=$((${LEN} - 11))
        P2=$((${LEN} - 10))
        BRIEF=${LINE:0:${P1}}
        BDATE=${LINE:${P2}}
        ;;
     *)
        BRIEF="${LINE}"
        ;;
     esac
    else
      DESC="MFC Manufacture environment"
      BRIEF="${DESC}"
    fi
    # Put a line in the items file for this entry.
    case "_${Build_Type}" in
    _dv|_qa|_rc|_zm)
      echo "${THIS_LABEL}|${Build_Type}|${BRANCH}|${SVN_REV}|${K}|${R}|${BDATE}|${PXE_OPTS}|${DESC}" >> ${ITEMS_1_FILE}
      SV=`echo ${THIS_LABEL} | cut -f1-2 -d-`
      grep ${SV}\$ ${VERS_FILE} > /dev/null 2>&1
      [ ${?} -ne 0 ] && echo ${SV} >> ${VERS_FILE}
      ;;
    *) 
      echo "${THIS_LABEL}|${K}|${R}|${BDATE}|${PXE_OPTS}|${DESC}" >> ${DEVELOPER_ITEMS_FILE}
      ;;
    esac
    # Put in the Listing file.
    H_BRIEF=`echo "${BRIEF}" | sed "s/&/&amp;/g" | sed "s/</&lt;/g" | sed "s/>/&gt;/g"`
    H_BRANCH="${BRANCH}"
    [ -z "${H_BRANCH}" ] && H_BRANCH="&nbsp;"
    H_BDATE="${BDATE}"
    [ -z "${H_BDATE}" ] && H_BDATE="&nbsp;"
    LINK_NAME="${THIS_LABEL}"
    echo '<tr>' >> ${LISTING_HTML}
    echo '<td><a href=http://'${SERVER_IP}'/pxe/pxe-boot/'${THIS_LABEL}'>'${LINK_NAME}'</a></td>' >> ${LISTING_HTML}
    echo '<td><tt>'${H_BRIEF}'</tt></td>' >> ${LISTING_HTML}
    echo '<td>'${H_BRANCH}'</td>' >> ${LISTING_HTML}
    echo '<td>'${H_BDATE}'</td>' >> ${LISTING_HTML}
    echo '</tr>' >> ${LISTING_HTML}
  done
  echo '</table>' >> ${LISTING_HTML}
  echo '<p/><hr>' >> ${LISTING_HTML}
  if [ ! -z "${SERVER_IP2}" ] ; then
    echo '[alternate address <a href="'http://${SERVER_IP2}/pxe'">http://'${SERVER_IP2}'/pxe</a>]' >> ${LISTING_HTML}
  fi
  echo '<br/>' >> ${LISTING_HTML}
  echo '</body></html>' >> ${LISTING_HTML}
}
Parse_Auto_Dir
  
###############################################################################
###############################################################################

Save_Prev()
{
  [ ! -f ${1} ] && return
  SP_TMP=`dirname ${1}`
  SF_TARGET=${PREV_DIR}/`basename ${SP_TMP}`.`basename ${1}`
  if [ -f ${SF_TARGET}.1 ] ; then
    cmp -s ${1} ${SF_TARGET}.1
    if [ $? -eq 0 ] ; then
      [  -f ${SF_TARGET}.3 ] && mv ${SF_TARGET}.3 ${SF_TARGET}.4
      [  -f ${SF_TARGET}.2 ] && mv ${SF_TARGET}.2 ${SF_TARGET}.3
      [  -f ${SF_TARGET}.1 ] && mv ${SF_TARGET}.1 ${SF_TARGET}.2
    fi
  fi
  mv ${1} ${SF_TARGET}.1
}

Submenu_Top()
{
  cat ${PROTO_DIR}/cfg.head > ${2}
  echo "MENU TITLE ${1}"    >> ${2}
  echo                      >> ${2}
  echo "LABEL main"         >> ${2}
  echo "  MENU LABEL Return to main menu" >> ${2}
  echo "  KERNEL menu.c32"  >> ${2}
  echo "  APPEND ${REL_CFG_DIR}/default" >> ${2}
  echo                      >> ${2}
}

Subsubmenu_Top()
{
  cat ${PROTO_DIR}/cfg.head > ${2}
  echo "MENU TITLE ${1}"    >> ${2}
  echo                      >> ${2}
  echo "LABEL main"         >> ${2}
  echo "  MENU LABEL Return to ${3} menu" >> ${2}
  echo "  KERNEL menu.c32"  >> ${2}
  echo "  APPEND submenus/${3}/default" >> ${2}
  echo                      >> ${2}
}

# Now create the menu files.
TOP_CONFIG_FILE=${FULL_CFG_DIR}/default
Save_Prev ${TOP_CONFIG_FILE}

echo "# Generated by ${0} from ${PROTO_DIR}/cfg.head" > ${TOP_CONFIG_FILE}
cat ${PROTO_DIR}/cfg.head     >> ${TOP_CONFIG_FILE}
echo MENU TITLE PXE Main Menu >> ${TOP_CONFIG_FILE}
echo TIMEOUT 600              >> ${TOP_CONFIG_FILE}
echo ONTIMEOUT local          >> ${TOP_CONFIG_FILE}
echo                          >> ${TOP_CONFIG_FILE}
echo "LABEL main"             >> ${TOP_CONFIG_FILE}
echo "  MENU LABEL Refresh menu" >> ${TOP_CONFIG_FILE}
echo "  KERNEL menu.c32"      >> ${TOP_CONFIG_FILE}
echo "  APPEND ${REL_CFG_DIR}/default" >> ${TOP_CONFIG_FILE}
echo                          >> ${TOP_CONFIG_FILE}
echo 'LABEL local'            >> ${TOP_CONFIG_FILE}
echo ' MENU LABEL Boot local' >> ${TOP_CONFIG_FILE}
echo ' LOCALBOOT 0'           >> ${TOP_CONFIG_FILE}
echo                          >> ${TOP_CONFIG_FILE}

Do_RC_Latest()
{
  ###########################
  # Generate RC latest menu #
  ###########################
  MC=latest
  ML="Menu: Latest autobuilds"
  echo                      >> ${TOP_CONFIG_FILE}
  echo "LABEL ${MC}"        >> ${TOP_CONFIG_FILE}
  echo "  MENU LABEL ${ML}" >> ${TOP_CONFIG_FILE}
  echo "  KERNEL menu.c32"  >> ${TOP_CONFIG_FILE}
  echo "  APPEND submenus/${MC}/default" >> ${TOP_CONFIG_FILE}
  THIS_CONFIG_DIR=${SUBMENUS_DIR}/${MC}
  THIS_CONFIG_FILE=${THIS_CONFIG_DIR}/default
  [ ! -d ${THIS_CONFIG_DIR} ] && mkdir ${THIS_CONFIG_DIR}
  Save_Prev ${THIS_CONFIG_FILE}
  Submenu_Top "Latest Auto Builds" ${THIS_CONFIG_FILE}
  ######################
  # Do the magic here. #
  # TBD                #
  ######################
}

Do_Versions()
{
  pwd >> ${LOG_FILE}
  echo i=${i} >> ${LOG_FILE}
  if [ ! -s ${1} ] ; then
    echo Skip ${1}, it is empty >> ${LOG_FILE}
    return
  fi
  wc ${1} >> ${LOG_FILE}
  ##########################
  # Generate version menus #
  ##########################
  for V_NAME in `sort -r ${1}`
  do
    MC=${V_NAME}
    ML="Menu: ${V_NAME} builds"
    echo "${ML}" >> ${LOG_FILE}
    echo                      >> ${TOP_CONFIG_FILE}
    echo "LABEL ${MC}"        >> ${TOP_CONFIG_FILE}
    echo "  MENU LABEL ${ML}" >> ${TOP_CONFIG_FILE}
    echo "  KERNEL menu.c32"  >> ${TOP_CONFIG_FILE}
    echo "  APPEND submenus/${MC}/default" >> ${TOP_CONFIG_FILE}
    THIS_CONFIG_DIR=${SUBMENUS_DIR}/${MC}
    THIS_CONFIG_FILE=${THIS_CONFIG_DIR}/default
    [ ! -d ${THIS_CONFIG_DIR} ] && mkdir ${THIS_CONFIG_DIR}
    Save_Prev ${THIS_CONFIG_FILE}
    Submenu_Top "${V_NAME}" ${THIS_CONFIG_FILE}
    ######################
    # Do the magic here. #
    ######################
    # Reverse sort by svn rev number.
    grep "^${V_NAME}-" ${ITEMS_1_FILE} | sort -r -g -k 4 -t '|' | while read LINE
    do
      [ -z "${LINE}" ] && continue
      THIS_LABEL=`echo "${LINE}" | cut -f1 -d'|'`
      BUILD_TYPE=`echo "${LINE}" | cut -f2 -d'|'`
      BRANCH=`echo "${LINE}" | cut -f3 -d'|'`
      SVN_REV=`echo "${LINE}" | cut -f4 -d'|'`
      K=`echo "${LINE}" | cut -f5 -d'|'`
      R=`echo "${LINE}" | cut -f6 -d'|'`
      BDATE=`echo "${LINE}" | cut -f7 -d'|'`
      PXE_OPTS=`echo "${LINE}" | cut -f8 -d'|'`
      DESC=`echo "${LINE}" | cut -f9 -d'|'`
      J=${AUTO_SUBDIR}/${THIS_LABEL}
      APPEND_LINE=" APPEND initrd=${J}/${R} ${APPEND_OPTS} installfrom=pxe/${AUTO_SUBDIR}/${THIS_LABEL} ${PXE_OPTS}"
      APPEND_OPTS_FILE=${AUTO_DIR}/${THIS_LABEL}/append-opts.txt
      if [ -f ${APPEND_OPTS_FILE} ] ; then
        APPEND_LINE=" APPEND initrd=${J}/${R} `head -1 ${APPEND_OPTS_FILE}` installfrom=pxe/${AUTO_SUBDIR}/${THIS_LABEL} ${PXE_OPTS}"
      fi
      echo ''                    >> ${THIS_CONFIG_FILE}
      echo "LABEL ${THIS_LABEL}" >> ${THIS_CONFIG_FILE}
      echo " MENU LABEL ${DESC}" >> ${THIS_CONFIG_FILE}
      echo " KERNEL ${J}/${K}"   >> ${THIS_CONFIG_FILE}
      echo "${APPEND_LINE}"      >> ${THIS_CONFIG_FILE}
      echo " IPAPPEND 3"         >> ${THIS_CONFIG_FILE}
    done
  done
}


Do_Developer()
{
  [ ! -f ${DEVELOPER_ITEMS_FILE} ] && return
  ##########################
  # Generate standard menu #
  ##########################
  MC=developer
  ML="Menu: Developer builds"
  echo                      >> ${TOP_CONFIG_FILE}
  echo "LABEL ${MC}"        >> ${TOP_CONFIG_FILE}
  echo "  MENU LABEL ${ML}" >> ${TOP_CONFIG_FILE}
  echo "  KERNEL menu.c32"  >> ${TOP_CONFIG_FILE}
  echo "  APPEND submenus/${MC}/default" >> ${TOP_CONFIG_FILE}
  THIS_CONFIG_DIR=${SUBMENUS_DIR}/${MC}
  THIS_CONFIG_FILE=${THIS_CONFIG_DIR}/default
  [ ! -d ${THIS_CONFIG_DIR} ] && mkdir ${THIS_CONFIG_DIR}
  Save_Prev ${THIS_CONFIG_FILE}
  Submenu_Top "${ML}" ${THIS_CONFIG_FILE}
  ######################
  # Do the magic here. #
  ######################
  cat ${DEVELOPER_ITEMS_FILE} | sort | while read LINE
  do
    [ -z "${LINE}" ] && continue
    THIS_LABEL=`echo "${LINE}" | cut -f1 -d'|'`
    K=`echo "${LINE}" | cut -f2 -d'|'`
    R=`echo "${LINE}" | cut -f3 -d'|'`
    BDATE=`echo "${LINE}" | cut -f4 -d'|'`
    PXE_OPTS=`echo "${LINE}" | cut -f5 -d'|'`
    DESC=`echo "${LINE}" | cut -f6 -d'|'`
    J=${AUTO_SUBDIR}/${THIS_LABEL}
    APPEND_LINE=" APPEND initrd=${J}/${R} ${APPEND_OPTS} installfrom=pxe/${AUTO_SUBDIR}/${THIS_LABEL} ${PXE_OPTS}"
    APPEND_OPTS_FILE=${AUTO_DIR}/${THIS_LABEL}/append-opts.txt
    if [ -f ${APPEND_OPTS_FILE} ] ; then
      APPEND_LINE=" APPEND initrd=${J}/${R} `head -1 ${APPEND_OPTS_FILE}` installfrom=pxe/${AUTO_SUBDIR}/${THIS_LABEL} ${PXE_OPTS}"
    fi
    echo ''                    >> ${THIS_CONFIG_FILE}
    echo "LABEL ${THIS_LABEL}" >> ${THIS_CONFIG_FILE}
    echo " MENU LABEL ${DESC}" >> ${THIS_CONFIG_FILE}
    echo " KERNEL ${J}/${K}"   >> ${THIS_CONFIG_FILE}
    echo "${APPEND_LINE}"      >> ${THIS_CONFIG_FILE}
    echo " IPAPPEND 3"         >> ${THIS_CONFIG_FILE}
    echo ''                    >> ${THIS_CONFIG_FILE}
  done
}

Do_Recent_Developer()
{
  [ ! -f ${DEVELOPER_ITEMS_FILE} ] && return
  ##########################
  # Generate standard menu #
  ##########################
  MC=recent
  ML="Menu: Recent developer builds"
  echo                      >> ${TOP_CONFIG_FILE}
  echo "LABEL ${MC}"        >> ${TOP_CONFIG_FILE}
  echo "  MENU LABEL ${ML}" >> ${TOP_CONFIG_FILE}
  echo "  KERNEL menu.c32"  >> ${TOP_CONFIG_FILE}
  echo "  APPEND submenus/${MC}/default" >> ${TOP_CONFIG_FILE}
  THIS_CONFIG_DIR=${SUBMENUS_DIR}/${MC}
  THIS_CONFIG_FILE=${THIS_CONFIG_DIR}/default
  [ ! -d ${THIS_CONFIG_DIR} ] && mkdir ${THIS_CONFIG_DIR}
  Save_Prev ${THIS_CONFIG_FILE}
  Submenu_Top "${ML}" ${THIS_CONFIG_FILE}
  ######################
  # Do the magic here. #
  ######################
  cat ${RECENT_ITEMS_FILE} | while read ITEM
  do
    LINE=`grep "^${ITEM}|" ${DEVELOPER_ITEMS_FILE}`
    [ -z "${LINE}" ] && continue
    THIS_LABEL=`echo "${LINE}" | cut -f1 -d'|'`
    K=`echo "${LINE}" | cut -f2 -d'|'`
    R=`echo "${LINE}" | cut -f3 -d'|'`
    BDATE=`echo "${LINE}" | cut -f4 -d'|'`
    PXE_OPTS=`echo "${LINE}" | cut -f5 -d'|'`
    DESC=`echo "${LINE}" | cut -f6 -d'|'`
    J=${AUTO_SUBDIR}/${THIS_LABEL}
    APPEND_LINE=" APPEND initrd=${J}/${R} ${APPEND_OPTS} installfrom=pxe/${AUTO_SUBDIR}/${THIS_LABEL} ${PXE_OPTS}"
    APPEND_OPTS_FILE=${AUTO_DIR}/${THIS_LABEL}/append-opts.txt
    if [ -f ${APPEND_OPTS_FILE} ] ; then
      APPEND_LINE=" APPEND initrd=${J}/${R} `head -1 ${APPEND_OPTS_FILE}` installfrom=pxe/${AUTO_SUBDIR}/${THIS_LABEL} ${PXE_OPTS}"
    fi

    echo ''                    >> ${THIS_CONFIG_FILE}
    echo "LABEL ${THIS_LABEL}" >> ${THIS_CONFIG_FILE}
    echo " MENU LABEL ${DESC}" >> ${THIS_CONFIG_FILE}
    echo " KERNEL ${J}/${K}"   >> ${THIS_CONFIG_FILE}
    echo "${APPEND_LINE}"      >> ${THIS_CONFIG_FILE}
    echo " IPAPPEND 3"         >> ${THIS_CONFIG_FILE}
    echo ''                    >> ${THIS_CONFIG_FILE}
  done
}

Do_Old_MFC()
{
  ##########################
  # Generate Old MFC menu  #
  ##########################
  MC=old_mfc
  ML="Menu: Old MFC boot images"
  echo                      >> ${TOP_CONFIG_FILE}
  echo "LABEL ${MC}"        >> ${TOP_CONFIG_FILE}
  echo "  MENU LABEL ${ML}" >> ${TOP_CONFIG_FILE}
  echo "  KERNEL menu.c32"  >> ${TOP_CONFIG_FILE}
  echo "  APPEND submenus/${MC}/default" >> ${TOP_CONFIG_FILE}
  THIS_CONFIG_DIR=${SUBMENUS_DIR}/${MC}
  THIS_CONFIG_FILE=${THIS_CONFIG_DIR}/default
  [ ! -d ${THIS_CONFIG_DIR} ] && mkdir ${THIS_CONFIG_DIR}
  Save_Prev ${THIS_CONFIG_FILE}
  Submenu_Top "${ML}" ${THIS_CONFIG_FILE}
  echo "###" Adding from ${PXE_TOP_DIR}/Old-MFC/cfg.tail >> ${THIS_CONFIG_FILE}
  cat ${PXE_TOP_DIR}/Old-MFC/cfg.tail >> ${THIS_CONFIG_FILE}
  echo "###" `date` >> ${THIS_CONFIG_FILE}
}

Do_Standard()
{
  ##########################
  # Generate standard menu #
  ##########################
  MC=standard
  ML="Menu: Standard boot images"
  echo                      >> ${TOP_CONFIG_FILE}
  echo "LABEL ${MC}"        >> ${TOP_CONFIG_FILE}
  echo "  MENU LABEL ${ML}" >> ${TOP_CONFIG_FILE}
  echo "  KERNEL menu.c32"  >> ${TOP_CONFIG_FILE}
  echo "  APPEND submenus/${MC}/default" >> ${TOP_CONFIG_FILE}
  THIS_CONFIG_DIR=${SUBMENUS_DIR}/${MC}
  THIS_CONFIG_FILE=${THIS_CONFIG_DIR}/default
  [ ! -d ${THIS_CONFIG_DIR} ] && mkdir ${THIS_CONFIG_DIR}
  Save_Prev ${THIS_CONFIG_FILE}
  Submenu_Top "${ML}" ${THIS_CONFIG_FILE}
  echo "###" Adding from ${PXE_TOP_DIR}/Standard/cfg.tail >> ${THIS_CONFIG_FILE}
  cat ${PXE_TOP_DIR}/Standard/cfg.tail >> ${THIS_CONFIG_FILE}
  echo "###" `date` >> ${THIS_CONFIG_FILE}
}

Do_Space()
{
  ##########################
  # Generate standard menu #
  ##########################
  MC=space
  ML="Menu: Space boot images"
  echo                      >> ${TOP_CONFIG_FILE}
  echo "LABEL ${MC}"        >> ${TOP_CONFIG_FILE}
  echo "  MENU LABEL ${ML}" >> ${TOP_CONFIG_FILE}
  echo "  KERNEL menu.c32"  >> ${TOP_CONFIG_FILE}
  echo "  APPEND submenus/${MC}/default" >> ${TOP_CONFIG_FILE}
  THIS_CONFIG_DIR=${SUBMENUS_DIR}/${MC}
  THIS_CONFIG_FILE=${THIS_CONFIG_DIR}/default
  [ ! -d ${THIS_CONFIG_DIR} ] && mkdir ${THIS_CONFIG_DIR}
  Save_Prev ${THIS_CONFIG_FILE}
  Submenu_Top "${ML}" ${THIS_CONFIG_FILE}
  if [ -f ${PXE_TOP_DIR}/space/cfg.tail ] ; then
    echo "###" Adding from ${PXE_TOP_DIR}/space/cfg.tail >> ${THIS_CONFIG_FILE}
    cat ${PXE_TOP_DIR}/space/cfg.tail >> ${THIS_CONFIG_FILE}
  fi
  echo "###" `date` >> ${THIS_CONFIG_FILE}
}

#Do_RC_Latest
Do_Recent_Developer
Do_Standard
Do_Space
Do_Developer
cat ${VERS_FILE} | grep -v ^mfc-2 | grep -v ^mfm > ${WORK_DIR}/vers1.txt
cat ${VERS_FILE} | grep ^mfc-2 > ${WORK_DIR}/vers2.txt
cat ${VERS_FILE} | grep ^mfm   > ${WORK_DIR}/vers3.txt
Do_Versions ${WORK_DIR}/vers1.txt
Do_Versions ${WORK_DIR}/vers2.txt
Do_Versions ${WORK_DIR}/vers3.txt
Do_Old_MFC

##############################################################################
# Now put everything in the top menu. ########################################
##############################################################################
#echo "${THIS_LABEL}|${Build_Type}|${BRANCH}|${SVN_REV}|${K}|${R}|${BDATE}|${PXE_OPTS}|${DESC}" >> ${ITEMS_1_FILE}
#echo "${THIS_LABEL}|${K}|${R}|${BDATE}|${PXE_OPTS}|${DESC}" >> ${DEVELOPER_ITEMS_FILE}

cat ${ITEMS_1_FILE} | sort | while read LINE
do
  [ -z "${LINE}" ] && continue
  THIS_LABEL=`echo "${LINE}" | cut -f1 -d'|'`
  BUILD_TYPE=`echo "${LINE}" | cut -f2 -d'|'`
  BRANCH=`echo "${LINE}" | cut -f3 -d'|'`
  SVN_REV=`echo "${LINE}" | cut -f4 -d'|'`
  K=`echo "${LINE}" | cut -f5 -d'|'`
  R=`echo "${LINE}" | cut -f6 -d'|'`
  BDATE=`echo "${LINE}" | cut -f7 -d'|'`
  PXE_OPTS=`echo "${LINE}" | cut -f8 -d'|'`
  DESC=`echo "${LINE}" | cut -f9 -d'|'`
  J=${AUTO_SUBDIR}/${THIS_LABEL}

  echo ''                    >> ${TOP_CONFIG_FILE}
  echo "LABEL ${THIS_LABEL}" >> ${TOP_CONFIG_FILE}
  echo " MENU LABEL ${DESC}" >> ${TOP_CONFIG_FILE}
  echo " KERNEL ${J}/${K}"   >> ${TOP_CONFIG_FILE}
  echo " APPEND initrd=${J}/${R} ${APPEND_OPTS} \
  installfrom=pxe/${AUTO_SUBDIR}/${THIS_LABEL} ${PXE_OPTS}" \
                             >> ${TOP_CONFIG_FILE}
  echo " IPAPPEND 3"         >> ${TOP_CONFIG_FILE}
done

cat ${DEVELOPER_ITEMS_FILE} | while read LINE
do
  [ -z "${LINE}" ] && continue
  THIS_LABEL=`echo "${LINE}" | cut -f1 -d'|'`
  K=`echo "${LINE}" | cut -f2 -d'|'`
  R=`echo "${LINE}" | cut -f3 -d'|'`
  BDATE=`echo "${LINE}" | cut -f4 -d'|'`
  PXE_OPTS=`echo "${LINE}" | cut -f5 -d'|'`
  DESC=`echo "${LINE}" | cut -f6 -d'|'`
  J=${AUTO_SUBDIR}/${THIS_LABEL}

  echo ''                    >> ${TOP_CONFIG_FILE}
  echo "LABEL ${THIS_LABEL}" >> ${TOP_CONFIG_FILE}
  echo " MENU LABEL ${DESC}" >> ${TOP_CONFIG_FILE}
  echo " KERNEL ${J}/${K}"   >> ${TOP_CONFIG_FILE}
  echo " APPEND initrd=${J}/${R} ${APPEND_OPTS}  \
  installfrom=pxe/${AUTO_SUBDIR}/${THIS_LABEL} ${PXE_OPTS}" \
                             >> ${TOP_CONFIG_FILE}
  echo " IPAPPEND 3"         >> ${TOP_CONFIG_FILE}
  echo ''                    >> ${TOP_CONFIG_FILE}
done

mv ${WORK_DIR}/this ${WORK_DIR}/last
exit


xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

