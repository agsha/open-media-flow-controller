:
OUT=${1}
D=/sbin/dmidecode
if [ ! -f ${D} ] ; then
  D=/usr/sbin/dmidecode
  [ ! -f ${D} ] && D=dmidecode
fi
function doit()
{
  echo DMIDECODE-INFO `date`
  echo ================================================================
  echo ==== dmidecode -s STRING =======================================
  for i in \
    system-manufacturer \
    system-product-name \
    system-version \
    system-serial-number \
    system-uuid \
    baseboard-manufacturer \
    baseboard-product-name \
    baseboard-version \
    baseboard-serial-number \
    baseboard-asset-tag \
    chassis-manufacturer \
    chassis-type \
    chassis-version \
    chassis-serial-number \
    chassis-asset-tag \
    processor-family \
    processor-manufacturer \
    processor-version \
    processor-frequency \
    bios-vendor \
    bios-version \
    bios-release-date \
    ;
  do
    echo ${i}=`${D} -s ${i}`
  done
  echo ==== dmidecode -q  =============================================
  ${D} -q
  echo ==== End of dmidecode output ===================================
  echo ================================================================
}

if [ -z "${OUT}" ] ; then
  doit
else
  doit >> ${OUT}
fi
