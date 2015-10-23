:
function Look_For_Customer_Root()
{
  # Look for known directory pattern that shows that we are at the root of
  # the MFC source tree.  When found set PROD_CUSTOMER_ROOT to it.
  PROD_PRODUCT=juniper
  export PROD_PRODUCT_LC=${PROD_PRODUCT}
  MATCH=${PROD_PRODUCT}/src/mk/build_settings.dot
  SAVE1_DIR=`pwd`
  TRY_DIR=
  COUNT=0
  while true; do
    COUNT=`expr ${COUNT} + 1`
    if [ ${COUNT} -gt 10 ] ; then
      echo Source tree not found
      break
    fi
    if [ -f ${TRY_DIR}${MATCH} ] ; then
      ############################################# CD ###
      cd ./${TRY_DIR}
      export PROD_CUSTOMER_ROOT=`pwd`
      echo Setting PROD_CUSTOMER_ROOT="${PROD_CUSTOMER_ROOT}"
      break
    fi
    TRY_DIR=../${TRY_DIR}
    if [ ! -d ${TRY_DIR} ] ; then
      echo Source tree not found
      break
    fi
  done
  ############################################# CD ###
  cd ${SAVE1_DIR}
  return
}
Look_For_Customer_Root
