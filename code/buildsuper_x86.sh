#!/bin/bash

# Store the real CWD
REAL_PWD="$PWD"

# Find the code home folder
TARGET_FILE="$0"
cd `dirname $TARGET_FILE`
TARGET_FILE=`basename $TARGET_FILE`
while [ -L "$TARGET_FILE" ]
do
    TARGET_FILE=`readlink $TARGET_FILE`
    cd `dirname $TARGET_FILE`
    TARGET_FILE=`basename $TARGET_FILE`
done
PHYS_DIR=`pwd -P`
SCRIPT_FILE=$PHYS_DIR/$TARGET_FILE
CODE_HOME=$(dirname "$SCRIPT_FILE")

# Find the most reasonable candidate build file
SOURCE="$1"
if [ -z "$SOURCE" ]; then
    SOURCE="$CODE_HOME/4coder_default_bindings.cpp"
fi

TARGET_FILE="$SOURCE"
cd `dirname $TARGET_FILE`
TARGET_FILE=`basename $TARGET_FILE`
while [ -L "$TARGET_FILE" ]
do
    TARGET_FILE=`readlink $TARGET_FILE`
    cd `dirname $TARGET_FILE`
    TARGET_FILE=`basename $TARGET_FILE`
done
PHYS_DIR=`pwd -P`
SOURCE=$PHYS_DIR/$TARGET_FILE

FLAGS="-Wno-write-strings -Wno-null-dereference -Wno-comment -Wno-switch -Wno-writable-strings"
ARCH=-m32
DEBUG=-g

cd "$REAL_PWD"
g++ -I"$CODE_HOME" $FLAGS $DEBUG -std=gnu++0x "$CODE_HOME/4coder_metadata_generator.cpp" -o metadata_generator
./metadata_generator -R "$CODE_HOME" "$CODE_HOME"

g++ -I"$CODE_HOME" $ARCH $FLAGS $DEBUG -std=gnu++0x "$SOURCE" -shared -o custom_4coder.so -fPIC

rm metadata_generator

