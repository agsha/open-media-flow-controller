:
# Script to set PROD_TREE_ROOT as needed to build the MFC source,
# based on the values in:
#  ${PROD_CUSTOMER_ROOT}/nokeena/src/mk/build_settings.dot
#
# Requires these to be set properly:
#   PROD_CUSTOMER_ROOT
# Uses these if set:
#   PROD_PRODUCT
#   BUILD_SAMARA_SRC_TOP
#
# svn://cmbu-sv01.englab.juniper.net/thirdparty/samara/current/tree
# svn://cmbu-sv01.englab.juniper.net/thirdparty/samara/branches/eucalyptus/tree
[ -z "${BUILD_SAMARA_SRC_TOP}" ] && \
  BUILD_SAMARA_SRC_TOP=/source/samara

BS_FILE="${PROD_CUSTOMER_ROOT}"/nokeena/src/mk/build_settings.dot

export PROD_TREE_ROOT=
if [ -z "${PROD_CUSTOMER_ROOT}" ] ; then
  echo Error, PROD_CUSTOMER_ROOT is not set 1>&2
elif [ ! -f "${BS_FILE}" ] ; then
  echo Error, no file "${BS_FILE}" 1>&2
else
  [ -z "${PROD_PRODUCT}" ] && PROD_PRODUCT=nokeena
  export PROD_PRODUCT_LC=`echo ${PROD_PRODUCT} | tr '[A-Z]' '[a-z]'`
  SAMARA_SVN_REV=`cat ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/mk/build_settings.dot \
    | grep -v '^#' \
    | grep -v '#SAMARA_SVN_REV=' \
    | grep SAMARA_SVN_REV= \
    | head -1 \
    | cut -f2 -d=`
  SAMARA_SVN_BRANCH=`cat ${PROD_CUSTOMER_ROOT}/${PROD_PRODUCT}/src/mk/build_settings.dot \
    | grep -v '^#' \
    | grep -v '#SAMARA_SVN_BRANCH=' \
    | grep SAMARA_SVN_BRANCH= \
    | head -1 \
    | cut -f2 -d=`

  if [ -z "${SAMARA_SVN_REV}" ] ; then
    echo Error, SAMARA_SVN_REV is not set in the build_settings.dot file 1>&2
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
fi

echo PROD_TREE_ROOT=${PROD_TREE_ROOT}
