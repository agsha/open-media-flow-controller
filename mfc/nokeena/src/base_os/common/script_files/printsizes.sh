:
# This is a script just for developers, to print out the most important
# partition settings from the customer_rootflop.sh file.
grep MINSIZE customer_rootflop.sh
echo =================================
for i in BIGLOGNC FLASHNCACHE DEFAULT FLASHROOT 32G 3GCACHE 3GNCACHE PACIFICA CLOUDVM CLOUDRC
do
  cat customer_rootflop.sh | grep "##${i}"
  cat customer_rootflop.sh | grep "##Model" | grep ${i}
  cat customer_rootflop.sh | grep IL_LO_${i}_TARGET_DISK | grep MINSIZE

  for j in SWAP COREDUMP
  do
    for k in SIZE
    do
      cat customer_rootflop.sh | grep IL_LO_${i}_PART_${j}_${k}
    done
  done
  for j in NKN LOG DMFS DMRAW
  do
    for k in SIZE GROWTHCAP GROWTHWEIGHT
    do
      cat customer_rootflop.sh | grep IL_LO_${i}_PART_${j}_${k}
    done
  done
  echo =================================
done

