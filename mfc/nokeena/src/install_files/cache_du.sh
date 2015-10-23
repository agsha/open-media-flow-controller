#! /bin/bash
if [ $# -lt 1 ]; then
  echo "Usage: cache_du <directory name>"
  exit 0
fi
/opt/nkn/bin/container_read -C -R $1 | awk '{ t += $2 } END { print t }'
