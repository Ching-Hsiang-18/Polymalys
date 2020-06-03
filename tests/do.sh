#!/bin/bash
if [ "$1" = "" ]; then
  echo "Usage: ./do.sh <C program without extension>" 
fi

arm-unknown-linux-gnueabi-gcc -o "${1}" "${1}.c" -static -g -O0
if [ "$?" != "0" ]; then
  echo "Compilation error"
  exit 1
fi
ln -sf "${1}" ./target
ulimit -c unlimited
./poly 2>&1
#orange --auto "${1}".c main > "${1}".orange

