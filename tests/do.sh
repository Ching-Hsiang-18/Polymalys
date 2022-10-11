#!/bin/bash
if [ "$1" = "" ]; then
  echo "Usage: ./do.sh <C program without extension>" 
fi
if [ "$HOSTNAME" = "edel-brau" ]; then
  arm-unknown-linux-gnueabi-cc  -o "${1}" "${1}.c" -static -g -O0
else
  arm-none-eabi-gcc -o "${1}" "${1}.c" -static -g -O0 -specs=nosys.specs
fi
if [ "$?" != "0" ]; then
  echo "Compilation error"
  exit 1
fi
ln -sf "${1}" ./target
ulimit -c unlimited
nm -o target |grep "T entry"
if [ "$?" == "0" ]; then
./poly.sh 2>&1
else
./poly.sh ./target main 2>&1
fi
#orange --auto "${1}".c main > "${1}".orange

