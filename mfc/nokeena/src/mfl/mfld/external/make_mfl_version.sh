:
# Script to create the MFL version settings from the MFC version
# settings from the version file given on the command line.
. ${1}
MFL_RELEASE=`echo ${BUILD_PROD_RELEASE} | sed "s/mfc/mfl/"`
if [ -z "${MFL_RELEASE}" ] ; then
  echo Error, failed the determine MFL_RELEASE
  exit 1
fi
MFL_FULL_VER=${MFL_RELEASE}-${BUILD_ID}
echo MFL_FULL_VER=${MFL_FULL_VER}
echo MFL_RELEASE=${MFL_RELEASE}
