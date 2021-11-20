#!/bin/sh
# C/brotli/*
# /TR 2017-05-25

find . -type d -exec chmod 775 {} \;
find . -type f -exec chmod 644 {} \;
chmod +x $0

mv include/brotli/* .
rm -rf include
for i in */*.c *.h; do
  sed -i 's|<brotli/port.h>|"port.h"|g' "$i"
  sed -i 's|<brotli/types.h>|"types.h"|g' "$i"
  sed -i 's|<brotli/encode.h>|"encode.h"|g' "$i"
  sed -i 's|<brotli/decode.h>|"decode.h"|g' "$i"
done
for i in */*.h; do
  sed -i 's|<brotli/port.h>|"../port.h"|g' "$i"
  sed -i 's|<brotli/types.h>|"../types.h"|g' "$i"
  sed -i 's|<brotli/encode.h>|"../encode.h"|g' "$i"
  sed -i 's|<brotli/decode.h>|"../decode.h"|g' "$i"
done

cd common
sed -i 's|include "./|include "./common/|g' *.c
for f in *.c; do mv $f ../br_$f; done

cd ../dec
sed -i 's|include "./|include "./dec/|g' *.c
sed -i 's|include "../common|include "./common|g' *.c
for f in *.c; do mv $f ../br_$f; done

cd ../enc
sed -i 's|include "./|include "./enc/|g' *.c
sed -i 's|include "../common|include "./common/|g' *.c
for f in *.c; do mv $f ../br_$f; done

exit

# then put these to "port.h"

/* disable some warnings /TR */
#pragma warning(disable : 4100)
#pragma warning(disable : 4127)
#pragma warning(disable : 4189)
#pragma warning(disable : 4295)
#pragma warning(disable : 4389)
#pragma warning(disable : 4701)
