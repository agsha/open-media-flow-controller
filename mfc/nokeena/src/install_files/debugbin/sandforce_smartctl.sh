#! /bin/bash
/opt/nkn/bin/smartctl -i -a -v 1,raw24/raw32 -v 9,msec24hour32 -v 13,raw24/raw32 -v 195,raw24/raw32 -v 198,raw24/raw32 -v 201,raw24/raw32 -v 204,raw24/raw32 $1
