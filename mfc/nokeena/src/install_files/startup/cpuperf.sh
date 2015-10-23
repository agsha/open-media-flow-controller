#/bin/bash
# Set the first half of all CPUs to "performance" mode.

if [ -d /sys/devices/system/cpu/cpu0/cpufreq ]; then
  cpu_cnt=`cat /proc/cpuinfo | grep processor | wc -l`
  perf_cpu=`expr $cpu_cnt / 2`
  for ((cnt=0; cnt<perf_cpu; cnt++))
  do
    echo "performance" > /sys/devices/system/cpu/cpu$cnt/cpufreq/scaling_governor
  done
fi 

