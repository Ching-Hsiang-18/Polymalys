#!/bin/bash
for T in $(cat tests.txt); do
  if [ "$VERBOSE" != "" ]; then
    echo ""
    echo "=========="
    echo "Testing ${T}"
    cat "${T}.c"
  else
    echo -n "Testing ${T}..."
  fi
  ./do.sh "${T}" |grep "_ITERATION" > tmp
  diff -q tmp ref/${T} >/dev/null 2>&1
  if [ "$?" != "0" ]; then
    echo "KO"
    echo "Expected:"
    cat "ref/${T}"
    echo "Got:"
    cat tmp
    exit 1
  fi
  if [ "$VERBOSE" != "" ]; then
    echo ""
    echo "----------"
    cat tmp
    echo "=========="
  else
    echo "OK"
  fi
done
exit 0
