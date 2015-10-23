#!/bin/bash

#$1 - device name
#$2 - start block

PAGE_SZ=4096
BLOCK_SZ=$[PAGE_SZ*64]
SZ_NUM=$(fdisk -l $1 | head -2 | tail -1 | sed -e 's@.*, @@g' -e 's@ bytes@@')
SZ=$(echo "scale=0; $SZ_NUM / 4096 * 4096" | bc)
NUM_BLOCK=$[SZ/BLOCK_SZ]

echo $BLOCK_SZ $PAGE_SZ $NUM_BLOCK
sleep 2
./nand_diag $1 $BLOCK_SZ $PAGE_SZ $2 $NUM_BLOCK
