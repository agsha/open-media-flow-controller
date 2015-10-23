:
# This script installs the config and other updatable files to
# the directory /config/nkn/dpi-analyzer/
# Copy all the files in this directory except *.sh files when
# the file is not already there.
# Also make them writeable
FROM_DIR=/opt/dpi-analyzer/defaultconfig
TO_DIR=/config/nkn/dpi-analyzer

if [ ! -d ${TO_DIR} ] ; then
   mkdir ${TO_DIR}
   #chown admin ${TO_DIR}
fi

cd ${FROM_DIR}
for FILE in *
do
  case ${FILE} in
  *.sh) continue ;;
  esac
  [ -f ${TO_DIR}/${FILE} ] && continue
  cp -a ${FILE} ${TO_DIR}/${FILE}
  chmod 644 ${TO_DIR}/${FILE}
done
