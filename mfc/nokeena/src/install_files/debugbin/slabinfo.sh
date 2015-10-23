#! /bin/bash
if [ "$1" = "-ls" ]; then
  cat /proc/slabinfo | awk '{print $1"\t"$3*$4}' | sort -n -k2,2
else
  cat /proc/slabinfo | awk '{ SUM += $3 * $4 } END { print SUM }'
fi
