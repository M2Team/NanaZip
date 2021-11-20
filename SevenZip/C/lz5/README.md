LZ5 - Library Files
================================

The __lib__ directory contains several files, but you don't necessarily need them all.

To integrate fast LZ5 compression/decompression into your program, you basically just need "**lz5.c**" and "**lz5.h**".

For more compression at the cost of compression speed (while preserving decompression speed), use **lz5hc** on top of regular lz5. `lz5hc` only provides compression functions. It also needs `lz5` to compile properly.

If you want to produce files or data streams compatible with `lz5` command line utility, use **lz5frame**. This library encapsulates lz5-compressed blocks into the [official interoperable frame format]. In order to work properly, lz5frame needs lz5 and lz5hc, and also **xxhash**, which provides error detection algorithm.
(_Advanced stuff_ : It's possible to hide xxhash symbols into a local namespace. This is what `liblz5` does, to avoid symbol duplication in case a user program would link to several libraries containing xxhash symbols.)

A more complex "lz5frame_static.h" is also provided, although its usage is not recommended. It contains definitions which are not guaranteed to remain stable within future versions. Use for static linking ***only***.

The other files are not source code. There are :

 - LICENSE : contains the BSD license text
 - Makefile : script to compile or install lz5 library (static or dynamic)
 - liblz5.pc.in : for pkg-config (make install)

[official interoperable frame format]: ../lz5_Frame_format.md
