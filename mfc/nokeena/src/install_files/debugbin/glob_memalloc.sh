#! /bin/bash
if [ "$1" = "-bm" ]; then
  /opt/nkn/bin/nkncnt -t 0 -s "glob_malloc_mod" | grep -v "= 0 " | egrep -v "nkn_obj_t|bm_nkn_page_t" | sort -n -k3,3 | awk '{SUM += $3} END { print SUM}'
elif [ "$1" = "-ls" ]; then
  /opt/nkn/bin/nkncnt -t 0 -s "glob_malloc_mod" | grep -v "= 0 " | sort -n -k3,3
else
  /opt/nkn/bin/nkncnt -t 0 -s "glob_malloc_mod" | grep -v "= 0 " | sort -n -k3,3 | awk '{SUM += $3} END { print SUM}'
fi
