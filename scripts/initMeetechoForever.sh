#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
EXTRAS=$ROOT/extras

cd $EXTRAS/basic_example
forever start -l meetlog.log -o meetlog.out -e meetlog.err  meetechoServer.js &
echo "Lynckia 4 Meetecho starting..."
