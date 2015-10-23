#!/bin/bash

if [ ! -e /etc/vxa_model ]; then
      return
fi

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/StorMan
TMPDIR=/nkn/tmp/Adaptec_logs
LOGDIR=/var/log/nkn/Adaptec_logs
mkdir -p $LOGDIR $TMPDIR
DATE=`date '+%Y%m%d-%H%M%S'`

# Get system serial number
SerialNum=`/opt/nkn/bin/dmidecode | awk '/Handle 0x0001/,/Handle 0x0002/{print}' | grep Serial | awk '{print $3}'`

echo "Extracting logs from Adapter on System S/N = $SerialNum"
/usr/StorMan/arcconf getlogs 1 device > $TMPDIR/ADPTC_${SerialNum}_device.log
/usr/StorMan/arcconf getlogs 1 device tabular > $TMPDIR/ADPTC_${SerialNum}_device_tabular.log
/usr/StorMan/arcconf getlogs 1 dead > $TMPDIR/ADPTC_${SerialNum}_dead.log
/usr/StorMan/arcconf getlogs 1 event > $TMPDIR/ADPTC_${SerialNum}_event.log
/usr/StorMan/arcconf getlogs 1 uart > $TMPDIR/ADPTC_${SerialNum}_uart.log
/usr/StorMan/arcconf getconfig 1 > $TMPDIR/ADPTC_${SerialNum}_config.log

echo "Done."
echo "Hard drive error logs:"
echo "------------------------------------------------------------"
cat $TMPDIR/ADPTC_${SerialNum}_device.log
echo "------------------------------------------------------------"
cd  $LOGDIR
tar czf ${SerialNum}_raidlogs-$DATE.tgz $TMPDIR

# Cleanup
/bin/rm -f $TMPDIR/ADPTC_${SerialNum}*.log

echo "Done"
