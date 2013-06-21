#!/bin/bash

SCRIPT=`pwd`/$0
ROOT=`dirname $SCRIPT`
CURRENT_DIR=`pwd`

cd $ROOT/nuveAPI

#node nuve.js &
forever start -l nuvelog.log -o nuvelog.out -e nuvelog.err nuve.js


cd $CURRENT_DIR
