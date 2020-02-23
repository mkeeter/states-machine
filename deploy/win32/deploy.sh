#!/bin/sh
set -x -e

cd ../..
TARGET=win32-cross make clean
TARGET=win32-cross make -j8
x86_64-w64-mingw32-strip StatesMachine.exe

# Parse the revision, branch, and tag from version.c
REV=$(cat src/version.c | sed -ne 's/.*GIT_REV="\([^\"]*\)".*/\1/gp')
TAG=$(cat src/version.c | sed -ne 's/.*GIT_TAG="\([^\"]*\)".*/\1/gp')
if [ -z $TAG ]
then
    TAG=$(cat src/version.c | sed -ne 's/.*GIT_BRANCH="\([^\"]*\)".*/\1/gp')
fi
VERSION="$TAG [$REV]"

if [ "$1" == "zip" ]
then
    rm -rf zip "States Machine.zip"
    mkdir zip
    cp -r StatesMachine.exe zip
    cp -r README.md zip/README.txt

    mv zip "States Machine $VERSION"
    zip -r "States Machine.zip" "States Machine $VERSION"

    # Clean up
    rm -r "States Machine $VERSION"
    rm -r StatesMachine.exe
fi

TARGET=win32-cross make clean
