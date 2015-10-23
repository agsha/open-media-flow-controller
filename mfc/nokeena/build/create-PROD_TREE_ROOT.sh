# If the PROD_TREE_ROOT dir does not exist and the dir name includes the rev,
# and it matches the following then create it.
#    /b*/samara*/current-*/tree
#    /b*/samara*/eucalyptus-*/tree
# Example: PROD_TREE_ROOT=/b2/samara/revs/current-247/tree
# Example: PROD_TREE_ROOT=/build/samara/eucalyptus-281/tree
#
SAMARA_CUR_SVN_URL=svn://cmbu-sv01.englab.juniper.net/thirdparty/samara/current/tree
SAMARA_CUR_SVN_URL=svn://cmbu-svn01.juniper.net/thirdpartyall/samara/current/tree
SAMARA_EUC_SVN_URL=svn://cmbu-sv01.englab.juniper.net/thirdparty/samara/branches/eucalyptus/tree
SAMARA_EUC_SVN_URL=svn://cmbu-svn01.juniper.net/thirdpartyall/samara/branches/eucalyptus/tree
# 
ARG1="${1}"
FULL_ABOVE=`dirname ${PROD_TREE_ROOT}`
ABOVE_DIR=`basename ${FULL_ABOVE}`
#

case "_${PROD_TREE_ROOT}" in
_)
  echo Error, PROD_TREE_ROOT not set
  exit 1
  ;;
_/b*/samara*/current-[0-9]*[0-9]/tree) 
  ;;
_/b*/samara*/eucalyptus-[0-9]*[0-9]/tree) 
  ;;
*)
  if [ -d ${PROD_TREE_ROOT} ] ; then
    echo Exists: PROD_TREE_ROOT=${PROD_TREE_ROOT}
    case _${ARG1} in
    _-f*|-_-f*)
      echo Not removing old tree.
      ;;
    esac
    exit 0
  fi
  echo Does not exist: PROD_TREE_ROOT=${PROD_TREE_ROOT}
  exit 1
  ;;
esac

case ${ABOVE_DIR} in
current-[0-9]*[0-9]) 
  SAMARA_REV=`echo ${ABOVE_DIR} | cut -c9-`
  SAMARA_SVN_URL=${SAMARA_CUR_SVN_URL}
  ;;
eucalyptus-[0-9]*[0-9]) 
  SAMARA_REV=`echo ${ABOVE_DIR} | cut -c12-`
  SAMARA_SVN_URL=${SAMARA_EUC_SVN_URL}
  ;;
*) 
  echo Cannot determine svn rev.
  exit 1
  ;;
esac
svn info -r ${SAMARA_REV} ${SAMARA_SVN_URL} > /dev/null
RV=${?}
if [ ${RV} -ne 0 ] ; then
  echo No such Samara 
fi
case _${ARG1} in
_-f*|-_-f*)
  rm -rf ${PROD_TREE_ROOT}
  if [ -d ${PROD_TREE_ROOT} ] ; then
    echo Failed to remove the tree.
    exit 1
  fi
  ;;
*)
  if [ -d ${PROD_TREE_ROOT} ] ; then
    # Nothing to do.
    exit 0
  fi
  ;;
esac

[ ! -d ${FULL_ABOVE} ] && mkdir –p ${FULL_ABOVE}
if [ ! -d ${FULL_ABOVE} ] ; then
  echo Failed to create ${FULL_ABOVE}
  exit 1
fi
cd ${FULL_ABOVE}
svn checkout -r ${SAMARA_REV} ${SAMARA_SVN_URL}

if [ ! -d ${BASE} ] ; then
  echo Failed to checkout samara source
  exit 1
fi
