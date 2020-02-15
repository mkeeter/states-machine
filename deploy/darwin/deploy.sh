#!/bin/sh
set -x -e

EXE="StatesMachine"
APP="States Machine.app"

cd ../..
make clean
make -j8
strip states-machine

# Parse the revision, branch, and tag from version.c
REV=$(cat src/version.c | sed -ne 's/.*GIT_REV="\([^\"]*\)".*/\1/gp')
TAG=$(cat src/version.c | sed -ne 's/.*GIT_TAG="\([^\"]*\)".*/\1/gp')
if [ -z $TAG ]
then
    TAG=$(cat src/version.c | sed -ne 's/.*GIT_BRANCH="\([^\"]*\)".*/\1/gp')
fi
VERSION="$TAG [$REV]"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS"
mkdir -p "$APP/Contents/Resources"
mkdir -p "$APP/Contents/Frameworks"

STATES_MACHINE=$APP/Contents/MacOS/$EXE
cp states-machine "$STATES_MACHINE"
cp "deploy/darwin/$EXE.icns" "$APP/Contents/Resources"
sed "s/VERSION/$VERSION/g" deploy/darwin/Info.plist > "$APP/Contents/Info.plist"

if [ "$1" == "dmg" ]
then
    rm -rf dmg $EXE.dmg
    mkdir dmg
    cp -r $APP dmg
    hdiutil create $EXE.dmg -volname "$EXE $VERSION" -srcfolder dmg

    # Clean up
    rm -rf dmg
fi

make clean
