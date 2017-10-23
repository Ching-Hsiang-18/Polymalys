#!/bin/bash
if [ "$1" = "" ]; then
  echo "Usage: ./do.sh prog"
fi

arm-unknown-linux-gnueabi-gcc -o "${1}" "${1}.c" -static -g
if [ "$?" != "0" ]; then
  echo "Erreur de compil"
  exit 1
fi
ln -sf "${1}" ./cible
ulimit -c unlimited
./poly 2>&1
orange --auto "${1}".c main > "${1}".orange

