#!/bin/bash

SMARTCTL=/opt/nkn/bin/smartctl

SMART_ARGS="-i -a -v 1,raw24/raw32 -v 9,msec24hour32 -v 13,raw24/raw32 -v 195,raw24/raw32 -v 198,raw24/raw32 -v 201,raw24/raw32 -v 204,raw24/raw32"

DEVICE=$1
OUTFILE=$2
TMPFILE=/nkn/tmp/smart.out.$$

#Get the SMART data from the device
$SMARTCTL ${SMART_ARGS} $DEVICE > $TMPFILE

#Parse and get the required details
grep "Device Model"  $TMPFILE | awk '{print $3}' > $OUTFILE
grep "Serial Number" $TMPFILE | awk '{print $3}' >> $OUTFILE
grep "Firmware Version" $TMPFILE | awk '{print $3}' >> $OUTFILE
drive_size=`fdisk -l $DEVICE | grep GB | awk '{print $3}'`
echo $drive_size >> $OUTFILE

val=`grep "1 Raw_Read_Error_Rate" $TMPFILE | tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "5 Reallocated_Sector_Ct" $TMPFILE | tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "9 Power_On_Hours" $TMPFILE | tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep " 12 Power_Cycle_Count" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "13 Read_Soft_Error_Rate" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "100 Unknown_Attribute" $TMPFILE | tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE


val=`grep "170 Unknown_Attribute" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "171 Unknown_Attribute" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "172 Unknown_Attribute" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE


val=`grep "174 Unknown_Attribute" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "177 Wear_Leveling_Count" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "181 Program_Fail_Cnt_Total" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "182 Erase_Fail_Count_Total" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "184 End-to-End_Error" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "187 Reported_Uncorrect" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "194 Temperature_Celsius" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "195 Hardware_ECC_Recovered" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "196 Reallocated_Event_Count" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "198 Offline_Uncorrectable" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE


val=`grep "199 UDMA_CRC_Error_Count" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "201 Soft_Read_Error_Rate" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "204 Soft_ECC_Correction" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "230 Head_Amplitude" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "231 Temperature_Celsius" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "232 Available_Reservd_Space" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "233 Media_Wearout_Indicator" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "234 Unknown_Attribute" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "235 Unknown_Attribute" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "241 Total_LBAs_Written" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE

val=`grep "242 Total_LBAs_Read" $TMPFILE |  tr '/' ' '| awk '{print $10}'`
if [ "$val" == "" ]; then
    val=0
fi
echo $val >> $OUTFILE
rm $TMPFILE
