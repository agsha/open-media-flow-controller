#! /bin/bash
top -bH -n 1 | awk 'NR==8,NR==12' | awk '{print $9 , $12}'
