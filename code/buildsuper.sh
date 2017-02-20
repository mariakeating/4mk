#!/bin/bash

# NOTE(allen): This code here is pulled from stack exchange, it could totally be wrong
# but I just don't know.  The goal is to get the path to the buildsuper.sh script so that
# path can be used as an include path which allows a file in any folder to be built in place.
SCRIPT_FILE=$(readlink -f "$0")
CODE_HOME=$(dirname "$SCRIPT_FILE")

SOURCE="$1"
if [ -z "$SOURCE" ]; then
    SOURCE="$CODE_HOME/4coder_default_bindings.cpp"
fi

echo "Building custom_4coders.so from $SOURCE ..."

SOURCE=$(readlink -f "$SOURCE")

g++ -I"$CODE_HOME" -Wno-write-strings -std=gnu++0x "$SOURCE" -shared -o custom_4coder.so -fPIC

