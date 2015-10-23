#! /bin/bash

echo Test all ethernet port pairs.
echo Pairs to be tested:
# Create a file of the pairs with the info needed.
TMP_FILE=/tmp/eth-pairs.list
rm -f ${TMP_FILE}
PREV=
for ITEM in `/sbin/ifconfig -a | grep ^eth | cut -f1 -d' '`
do
  if [ -z "${PREV}" ] ; then
    PREV=${ITEM}
  else
    echo ${PREV},${ITEM} >> ${TMP_FILE}
    echo ${PREV} ${ITEM}
    PREV=
  fi
done
echo
echo Starting test
cd /tmp
rm -rf ethlooptest.dir
mkdir ethlooptest.dir
cd ethlooptest.dir
for SET in `cat ${TMP_FILE}`
do
  A_NAME=`echo ${SET} | cut -f1 -d,`
  B_NAME=`echo ${SET} | cut -f2 -d,`
  echo "==${A_NAME} ${B_NAME}"
  # Make sure these two have inet addrs.
  ifconfig ${A_NAME} | grep -q "  UP "
  A_UP=${?}
  ifconfig ${B_NAME} | grep -q "  UP "
  B_UP=${?}
  ifconfig ${A_NAME} up
  ifconfig ${B_NAME} up
  # This command creates the output file ethtest-${A_NAME}-${B_NAME}.log
  /opt/nkn/bin/ethlooptest ${A_NAME} ${B_NAME}
  RV=$?
  # Put interfaces back they way they were.
  [ ${A_UP} -ne 0 ] && ifconfig ${A_NAME} down
  [ ${B_UP} -ne 0 ] && ifconfig ${B_NAME} down
  if [ ${RV} -eq 0 ] ; then
    echo ${A_NAME}:${B_NAME} ok
  else
    echo ${A_NAME}:${B_NAME} fail
  fi
  if [ -f ethtest-${A_NAME}-${B_NAME}.log ] ; then
    cat ethtest-${A_NAME}-${B_NAME}.log
  else
    echo Error, no test results for ${A_NAME}:${B_NAME}
  fi
done
cat ethtest-*-*.log | grep "^Result"
P_CNT=`grep ^Result ethtest-*-*.log | grep PASS | wc -l | awk '{ print $1 }'`
F_CNT=`grep ^Result ethtest-*-*.log | grep FAIL | wc -l | awk '{ print $1 }'`
echo Passed: ${P_CNT}, Failed: ${F_CNT}

# Save the test results.
[ ! -d /log/ethtestlog ] && mkdir /log/ethtestlog
cp ethtest-*.log /log/ethtestlog/

if [ ${F_CNT} -ne 0 ] ; then
  echo Test failed.
  exit 1
fi
exit 0
