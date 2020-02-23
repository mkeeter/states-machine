#!/bin/sh
set -x -e

cd ../..
TARGET=win32-cross make clean
TARGET=win32-cross make -j8
x86_64-w64-mingw32-strip StatesMachine.exe

if [ "$1" == "zip" ]
then
    rm -rf zip "States Machine.zip"
    mkdir zip
    cp -r StatesMachine.exe zip
    cp -r README.md zip/README.txt

    zip -r "States Machine.zip" zip

    # Clean up
    rm -r zip
    rm -r StatesMachine.exe
fi

TARGET=win32-cross make clean
