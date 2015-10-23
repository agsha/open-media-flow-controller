#! /bin/bash
if [ $# -eq 0 ]; then
  LINES=20
else
  LINES=$1
fi
/bin/ps aux | /usr/bin/head -n 1
/bin/ps aux  | /bin/sort -n -r -k5,5 | /usr/bin/head -n $LINES | sed 's/^/> /g'
