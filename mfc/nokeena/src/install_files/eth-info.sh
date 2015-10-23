:
OUT=${1}
doit()
{
  echo ETH-INFO `date`
  echo ==== /sbin/lspci -m -D =========================================
  /sbin/lspci -m -D
  echo ==== /sbin/lspci -m -D -n ======================================
  /sbin/lspci -m -D -n
  echo ==== lspci showing only Ethernet controllers ===================
  /sbin/lspci -m -D    | grep "^..............Ethernet"
  /sbin/lspci -m -D -n | grep "^..............0200"
  echo ==== ifconfig and ethtool ======================================
  for i in `/sbin/ifconfig -a | grep ^eth | cut -f1 -d' '`
  do
    echo +${i}++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    echo ==== /sbin/ifconfig ${i} ======================================
    /sbin/ifconfig ${i}
    echo ==== /sbin/ethtool -i ${i} ====================================
    /sbin/ethtool -i ${i}
    echo ==== /sbin/ethtool ${i} =======================================
    /sbin/ethtool ${i}
  done
  echo ================================================================
}

if [ -z "${OUT}" ] ; then
  doit
else
  doit >> ${OUT}
fi
