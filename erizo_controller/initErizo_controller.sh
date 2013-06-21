#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
ROOT=`dirname $SCRIPT`
CURRENT_DIR=`pwd`

echo $LD_LIBRARY_PATH

cd $ROOT/erizoController
#node erizoController.js &
forever start -l erizolog.log -o erizolog.out -e erizolog.err erizoController.js

cd $CURRENT_DIR
