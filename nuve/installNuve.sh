#!/bin/bash

SCRIPT=`pwd`/$0
FILENAME=`basename $SCRIPT`
PATHNAME=`dirname $SCRIPT`
ROOT=$PATHNAME/..
BUILD_DIR=$ROOT/build
CURRENT_DIR=`pwd`
DB_DIR="$BUILD_DIR"/db

cd $PATHNAME

cd nuveAPI

echo [nuve] Installing node_modules for nuve

npm install amqp
npm install express
npm install mongodb
npm install mongojs
npm install aws-lib

echo [nuve] Done, node_modules installed

cd ../nuveClient/tools

./compile.sh

echo [nuve] Done, nuve.js compiled

echo [nuve] Setup lynckia files

cd $CURRENT_DIR

sudo dos2unix /home/ubuntu/lynckia/scripts/initMeetechoNode.sh
sudo chmod 755 /home/ubuntu/lynckia/scripts/initMeetechoNode.sh

sudo mkdir /home/ubuntu/lynckia/recorded

echo [nuve] Lynckia Setted-Up
