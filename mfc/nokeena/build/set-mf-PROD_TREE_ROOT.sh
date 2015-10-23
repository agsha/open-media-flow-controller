:
# Script to dot in to set PROD_TREE_ROOT as needed to build the MFx source,
# based on the values in:
#  ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/mk/build_settings.dot
#
# Requires these to be set properly:
#   PROD_CUSTOMER_ROOT
# Uses these if set:
#   PROD_PRODUCT
#   BUILD_SAMARA_SRC_TOP
#
# When PROD_PRODUCT is not set, then it is set.
#
# svn://cmbu-sv01.englab.juniper.net/thirdparty/samara/current/tree
# svn://cmbu-sv01.englab.juniper.net/thirdparty/samara/branches/eucalyptus/tree
[ -z "${BUILD_SAMARA_SRC_TOP}" ] && \
  BUILD_SAMARA_SRC_TOP=/source/samara

Get_PROD_TREE_ROOT()
{
  SAMARA_SVN_REV=`cat ${1} \
    | grep -v '^#' \
    | grep -v '#SAMARA_SVN_REV=' \
    | grep SAMARA_SVN_REV= \
    | head -1 \
    | cut -f2 -d=`
  SAMARA_SVN_BRANCH=`cat ${1} \
    | grep -v '^#' \
    | grep -v '#SAMARA_SVN_BRANCH=' \
    | grep SAMARA_SVN_BRANCH= \
    | head -1 \
    | cut -f2 -d=`

  if [ -z "${SAMARA_SVN_REV}" ] ; then
    echo Error, SAMARA_SVN_REV is not set in the ${1} 1>&2
    PROD_TREE_ROOT=
  else 
    [ -z "${SAMARA_SVN_BRANCH}" ] && SAMARA_SVN_BRANCH=current
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

Find_Build_Settings()
{
  local SETTINGS_FILE
  if [ -z "${PROD_CUSTOMER_ROOT}" ] ; then
    echo Error, PROD_CUSTOMER_ROOT is not set 1>&2
    return
  fi
  if [ ! -z "${PROD_PRODUCT}" ] ; then
    SETTINGS_FILE=${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/mk/build_settings.dot
    if [ ! -f "${SETTINGS_FILE}" ] ; then
      echo Error, no file ${SETTINGS_FILE} 1>&2
      return
    fi
    Get_PROD_TREE_ROOT ${SETTINGS_FILE}
    return
  fi
  for SETTINGS_FILE in ${PROD_CUSTOMER_ROOT}/*/src/mk/build_settings.dot
  do
    if [ -f "${SETTINGS_FILE}" ] ; then
      T=`dirname ${SETTINGS_FILE}`
      T=`dirname ${T}`
      T=`dirname ${T}`
      export PROD_PRODUCT=`basename ${T}`
      echo Set: PROD_PRODUCT=${PROD_PRODUCT}
      export PROD_PRODUCT_LC=${PROD_PRODUCT}
      Get_PROD_TREE_ROOT ${SETTINGS_FILE}
      return
    fi
  done
  echo Error, Could not find ${PROD_CUSTOMER_ROOT}/'*'/src/mk/build_settings.dot 1>&2
}

export PROD_TREE_ROOT=
Find_Build_Settings
echo PROD_TREE_ROOT=${PROD_TREE_ROOT}
