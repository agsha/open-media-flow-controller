:
PRODUCT_NAME="Media Flow Logger"
# Script to install mfld and associated files.
# Files in the tar file, which we need to be able to install.
#   install-mf.sh (this script)
#   mfld
#   mfld.cfg
#   eula.txt
#   version.sh
#   mfc-ver.sh
#   mfc-ver.txt

ERR=0
for i in bin/mfld cfg/mfld.cfg info/eula.txt info/version.sh other/mfc-ver.sh other/mfc-ver.txt
do
  if [ ! -f ${i} ] ; then
    echo Error: missing file to install: ${i}
    ERR=1
  fi
done
if [ ${ERR} -ne 0 ] ; then
  echo Not installing ${PRODUCT_NAME}
  exit 1
fi
echo
echo Installing ${PRODUCT_NAME}
echo

# For installation the directory structure is:
# $install_home/bin/mfld
# $install_home/cfg/mfld.cfg
#  
# We also need to create a directory /var/log/nkn/
# This directory should be writable.
#  
# To start mfld :
# $install_home/bin/mfld  -f  $install_home/cfg/mfld.cfg -s -d

INSTALL_HOME=${MFL_INSTALL_ROOT}/var/local/mfl
LOG_DIR=${MFL_INSTALL_ROOT}/var/log/nkn

if [ -d ${INSTALL_HOME} ] ; then
  MFL_FULL_VER=
  if [ -f ${INSTALL_HOME}/version.sh ] ; then
    LINE=`grep ^MFL_FULL_VER= ${INSTALL_HOME}/version.sh`
    if [ ! -z "${LINE}" ] ; then
      eval "${LINE}"
    fi
  fi
  if [ ! -z "${MFL_FULL_VER}" ] ; then
    echo "${MFL_FULL_VER} already installed in ${INSTALL_HOME}"
  else
    echo "The installation directory ${INSTALL_HOME} already exists."
  fi
  echo
  while true ; do
    echo Do you want to overwrite the existing ${PRODUCT_NAME} 'installation? (y/n)'
    read ANS
    case "_${ANS}" in
    _y) rm -rf ${INSTALL_HOME}
        break
        ;;
    _n) echo
        echo Not installing
        exit 1
        ;;
    *)  continue
        ;;
    esac
  done
fi

for D in ${INSTALL_HOME} ${LOG_DIR}
do
  [ -d ${D} ] && continue
  mkdir -p ${D}
  if [ ! -d ${D} ] ; then
    echo Unable to create ${D}
    echo Not installing
    exit 1
  fi
done
chmod 777 ${LOG_DIR}

echo
sleep 1
echo Please read the EULA.
echo
sleep 1
echo To install ${PRODUCT_NAME} you must first agree to this license.
echo
sleep 1
echo The text of the license is in the file `pwd`/info/eula.txt
echo
sleep 1
echo Press Enter to Read the EULA.
read ANS
echo
echo To stop reading the EULA enter "'q' at the 'more' prompt"
sleep 4
echo
more info/eula.txt

echo
sleep 1
echo
while true ; do
  echo 'Do you agree to this license? (y/n)'
  read ANS
  case "_${ANS}" in
  _y) echo Agreed to EULA > ${INSTALL_HOME}/agreed.txt
      break
      ;;
  _n) echo
      echo Not installing
      rmdir ${INSTALL_HOME}
      exit 1
      ;;
  *)  continue
      ;;
  esac
done

# We also need to create a directory /var/log/nkn/
# This directory should be writable.

mkdir ${INSTALL_HOME}/bin
mkdir ${INSTALL_HOME}/web
mkdir ${INSTALL_HOME}/cfg
mkdir ${INSTALL_HOME}/info
mkdir ${INSTALL_HOME}/other

# Install mfld mfld.cfg eula.txt version.sh mfc-ver.sh mfc-ver.txt
cp  bin/*    ${INSTALL_HOME}/bin
chmod -R 555 ${INSTALL_HOME}/bin

cp -r web/*  ${INSTALL_HOME}/web
chmod -R 444 ${INSTALL_HOME}/web/html
chmod -R 555 ${INSTALL_HOME}/web/cgi-bin

cp  info/*   ${INSTALL_HOME}/info
chmod -R 444 ${INSTALL_HOME}/info

cp  other/*  ${INSTALL_HOME}/other
chmod -R 444 ${INSTALL_HOME}/other

cp  cfg/*    ${INSTALL_HOME}/cfg
chmod -R 644 ${INSTALL_HOME}/cfg
cat cfg/mfld.cfg \
  | sed "s#/var/log/nkn#${LOG_DIR}#" \
  > ${INSTALL_HOME}/cfg/mfld.cfg
chmod -R 444 ${INSTALL_HOME}/cfg

README_FILE=${INSTALL_HOME}/README.txt
echo 'To start mfld :'                            >  ${README_FILE}
echo "  ${INSTALL_HOME}/bin/mfld  -f  ${INSTALL_HOME}/cfg/mfld.cfg -s -d" >> ${README_FILE}
echo "This uses the directory ${LOG_DIR}"            >> ${README_FILE}
echo 'Make sure that this directory is writable.'    >> ${README_FILE}
echo 'This directory is configurable in mfld.cfg' >> ${README_FILE}

echo
sleep 1
echo Finished installing ${PRODUCT_NAME} in ${INSTALL_HOME} and ${LOG_DIR}
echo
sleep 1
echo Please read ${README_FILE} for instructions for using ${PRODUCT_NAME}
exit 0
