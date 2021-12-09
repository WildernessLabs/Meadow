# arm-builtin
Provide ARM builtin functions for use with LLVM mono AoT

When mono generates AoT code using LLVM the code generated includes references to many builtin and standard C library routines. The NuttX library provides many of these standard routines but the builtins are usually provided by things such as libgcc and LLVM's compile-rt, both of which are shared objects.

This library is built as a static object that is used at AoT time when the object is being linked.
