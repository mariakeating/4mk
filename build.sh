#!/bin/sh
cd "$(dirname "$(readlink -f "$0")")"
for argument in "$@"; do eval "$argument=1"; done

code="$PWD"/code

mkdir -p build
cd build

gcc -Wno-write-strings -fno-rtti -fno-exceptions -I"$code" -I"$code"/custom -I/usr/include/freetype2 "$code"/platform_linux/linux_4ed.cpp -o 4ed -lfreetype -lX11 -lGL -lXfixes

gcc -shared -fPIC -Wno-write-strings -fno-rtti -fno-exceptions -I"$code"/custom "$code"/4ed_app_target.cpp -o 4ed_app.so

gcc -shared -fPIC -Wno-write-strings -fno-rtti -fno-exceptions -I"$code" "$code"/custom/4coder_default_bindings.cpp -o custom_4coder.so
