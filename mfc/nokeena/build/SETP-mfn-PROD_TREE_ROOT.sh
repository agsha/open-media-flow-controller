:
# Script to set PROD_TREE_ROOT as needed to build the MFN source.
# When no options are specifed:
#   When the current directory is a MFN source, then
#      Use this source tree
#   Else If PROD_CUSTOMER_ROOT is set and is a existing directory
#      Use source tree $PROD_CUSTOMER_ROOT
#   Else
#      Use the MFN mainline
# When options are specifed:
#   [directory where MFN source is located | branch name | svn url of MFN source] [svn revision]
#
# 
SAMARA_SVN_URL=svn://cmbu-sv01.englab.juniper.net/thirdparty/samara
SAMARA_SVN_URL=svn://cmbu-svn01.juniper.net/thirdpartyall/samara
if [ -z "${BUILD_SAMARA_SRC_TOP}" ] ; then
  BUILD_SAMARA_SRC_TOP=/source/samara
  echo Automatically setting BUILD_SAMARA_SRC_TOP=${BUILD_SAMARA_SRC_TOP} 1>&2
elif [ ! -d ${BUILD_SAMARA_SRC_TOP} ] ; then
  echo No such dir BUILD_SAMARA_SRC_TOP=${BUILD_SAMARA_SRC_TOP} 1>&2
  BUILD_SAMARA_SRC_TOP=
fi

MY_DIR=`dirname "${0}"`
case "${MY_DIR}" in
/*)   ;;
../*) MY_DIR=`pwd`/.. ;;
.|./) MY_DIR=`pwd` ;;
*) MY_DIR=`pwd`/"${MY_DIR}" ;;
esac    

BRANCH_NAME=unset
MY_REVISION=top
CR_SVN_URL=unset
# The script /eng/bin-svn/mfn_branch_name_from_version.sh returns the
# MFC branch name for a given the release version.
# The script /eng/bin-svn/mfn_svn_url_from_branch.sh returns the svn URL
# for the PROD_CUSTOMER_ROOT source for a given svn branch name.
# 
if [ -z "${1}" ] ; then
  echo "Error, parameter reqired"
  echo Required first parameter specifies the MFN source.  One of the following:
  echo '  -' directory_where_MFN_source_is_located
  echo '  -' branch_name
  echo '  -' MFN version, e.g. 0.0.0
  echo '  -' svn_url_of_MFN_source
  echo "Optional second parameter specifies the svn revision (default 'top')"
else
  if [ ! -z "${2}" ] ; then
   MY_REVISION="${2}"
  fi
  OPT1=${1}
  if [ -d ${OPT1} ] ; then
    SAVE_DIR=`pwd`
    cd "${OPT1}"
    CR_SVN_URL=`svn info | grep URL: 2> /dev/null | cut -f2 -d' '`
    case ${CR_SVN_URL} in
    "")  CR_SVN_URL="invalid:`pwd`"
      ;;
    svn://*appliance/trunk*)
      CR_SVN_URL=`echo "${CR_SVN_URL}" | cut -f1-6 -d/`
      ;;
    svn://*)
      CR_SVN_URL=`echo "${CR_SVN_URL}" | cut -f1-7 -d/`
      ;;
    esac
    cd "${SAVE_DIR}"
  else
    case ${OPT1} in
    svn://*appliance/trunk*)
      CR_SVN_URL=`echo "${OPT1}" | cut -f1-6 -d/`
      ;;
    svn://*)
      CR_SVN_URL=`echo "${OPT1}" | cut -f1-7 -d/`
      ;;
    0*|1*|2*|3*|4*)
      BR_NAME=`/eng/bin-svn/mfn_branch_name_from_version.sh ${OPT1} ${MY_REVISION}`
      CR_SVN_URL=`/eng/bin-svn/mfn_svn_url_from_branch.sh ${BR_NAME}`
      ;;
    *)
      CR_SVN_URL=`/eng/bin-svn/mfn_svn_url_from_branch.sh ${OPT1}`
      ;;
    esac
  fi
fi
if [ "${CR_SVN_URL}" = "unset" ] ; then
  # First see if we are in a MFN source tree or not.
  CR_SVN_URL=`svn info 2> /dev/null | grep URL: | cut -f2 -d' ' | cut -f1-7 -d/`
  if [ -z "${CR_SVN_URL}" ] ; then
    if [ -z "${PROD_CUSTOMER_ROOT}" ] ; then
      echo "Use MFN mainline" 1>&2
      CR_SVN_URL=`/eng/bin-svn/mfn_svn_url_from_branch.sh trunk`
    elif [ ! -d "${PROD_CUSTOMER_ROOT}" ] ; then
      echo "Use MFN mainline" 1>&2
      CR_SVN_URL=`/eng/bin-svn/mfn_svn_url_from_branch.sh trunk`
    else
      SAVE_DIR=`pwd`
      echo "Use PROD_CUSTOMER_ROOT=${PROD_CUSTOMER_ROOT}" 1>&2
      cd "${PROD_CUSTOMER_ROOT}"
      CR_SVN_URL=`svn info | grep URL: 2> /dev/null | cut -f2 -d' '`
      cd "${SAVE_DIR}"
      if [ -z "${CR_SVN_URL}" ] ; then
        CR_SVN_URL=PROD_CUSTOMER_ROOT_invalid
      fi
    fi
  fi
  case "${CR_SVN_URL}" in
  *nkn/appliance/trunk*)
    CR_SVN_URL=`echo ${CR_SVN_URL} | cut -f1-6 -d/`
  esac
fi

case "${CR_SVN_URL}" in
svn*trunk)
  BRANCH_NAME=mainline
  CR_SVN_URL=`echo ${CR_SVN_URL} | cut -f1-6 -d/`
  ;;
svn*)
  BRANCH_NAME=`basename ${CR_SVN_URL}`
  ;;
esac

Get_It()
{
  # Get the $PRODUCT/src/mk/build_settings.dot file, which
  # has the Samara SVN info and the versions string we need.
  # E.g.:
  #  SAMARA_SVN_REV=185
  #  SAMARA_SVN_BRANCH=current
  #  SAMARA_SVN_BRANCH=eucalyptus
  # The script /var/local/build/prod_tree_root.sh returns the PROD_TREE_ROOT setting
  # where the specified version of Samara is to be located.
  
  PROD_TREE_ROOT=
  TMP_DIR=/tmp/tmp.${$}.tmp
  mkdir -p ${TMP_DIR}/co
  SAVE_DIR=`pwd`
  cd ${TMP_DIR}/co
  if [ "${MY_REVISION}" = "top" ] ; then 
    svn checkout ${CR_SVN_URL}/juniper/src/mk > /dev/null 2>&1
  else
    svn checkout -r ${MY_REVISION} ${CR_SVN_URL}/juniper/src/mk >>  /dev/null 2>&1
  fi
  if [ ! -f mk/build_settings.dot ] ; then
    if [ "${MY_REVISION}" = "top" ] ; then
      echo No source file mk/build_settings.dot 1>&2
    else
      echo No source file mk/build_settings.dot in revision ${MY_REVISION} 1>&2
    fi
    return
  fi
  SAMARA_SVN_REV=missing
  SAMARA_SVN_BRANCH=missing
  # Dot it in!
  . mk/build_settings.dot
  # Override the setting that might be in the build_settings.dot
  if [ "${SAMARA_SVN_REV}" = "missing" ] ; then
      echo SAMARA_SVN_REV is not set in the build_settings.dot file 1>&2
      return
  fi
  if [ "${SAMARA_SVN_BRANCH}" = "missing" ] ; then
      SAMARA_SVN_BRANCH=current
      echo "SAMARA_SVN_BRANCH is not set in the build_settings.dot, using: ${SAMARA_SVN_BRANCH}" 1>&2
  fi
  if [ "${SAMARA_SVN_BRANCH}" = "current" ] ; then
    SAMARA_SVN_URL_PROD_TREE_ROOT=${SAMARA_SVN_URL}/current/tree
  else
    SAMARA_SVN_URL_PROD_TREE_ROOT=${SAMARA_SVN_URL}/branches/${SAMARA_SVN_BRANCH}/tree
  fi
  cd "${SAVE_DIR}"
  rm -rf ${TMP_DIR}
  if [ -z "${BUILD_SAMARA_SRC_TOP}" ] ; then
    if [ -x /var/local/build/prod_tree_root.sh ] ; then
      PROD_TREE_ROOT=`/var/local/build/prod_tree_root.sh ${SAMARA_SVN_URL_PROD_TREE_ROOT} ${SAMARA_SVN_REV}`
    else
      PROD_TREE_ROOT=
      echo '#' Unable to set PROD_TREE_ROOT.  BUILD_SAMARA_SRC_TOP not set and no /var/local/build/prod_tree_root.sh 1>&2
    fi
  else
    case ${SAMARA_SVN_REV} in
    top*|latest)
      echo '#' SAMARA_SVN_BRANCH=${SAMARA_SVN_BRANCH} SAMARA_SVN_REV=top-of-tree 1>&2
      PROD_TREE_ROOT=${BUILD_SAMARA_SRC_TOP}/${SAMARA_SVN_BRANCH}/tree
     ;;
    *)
      echo '#' SAMARA_SVN_BRANCH=${SAMARA_SVN_BRANCH} SAMARA_SVN_REV=${SAMARA_SVN_REV} 1>&2
      PROD_TREE_ROOT=${BUILD_SAMARA_SRC_TOP}/${SAMARA_SVN_BRANCH}-${SAMARA_SVN_REV}/tree
      ;;
    esac
    
  fi
}

if [ "${BRANCH_NAME}" = "unset" ] ; then
  echo Error, release version ${RELEASE_NAME} not accepted: ${CR_SVN_URL} 1>&2
  PROD_TREE_ROOT=
else
  Get_It
fi
export PROD_TREE_ROOT
echo PROD_TREE_ROOT=${PROD_TREE_ROOT}
