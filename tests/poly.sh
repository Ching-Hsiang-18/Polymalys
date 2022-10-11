#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
LD_LIBRARY_PATH=$SCRIPT_DIR/../otawa/lib/otawa/otawa:$SCRIPT_DIR/../otawa/lib/ $SCRIPT_DIR/poly $* 
