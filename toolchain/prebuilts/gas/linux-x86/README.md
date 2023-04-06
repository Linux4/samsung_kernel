# platform/prebuilts/gas/linux-x86

This repository exists as a temporary repository for containing copies of
prebuilt binaries of GNU `as` (the assembler), copied from
1. https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/x86/x86_64-linux-android-4.9/
2. https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/
3. https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/

It only contains binaries for linux-x86 host, which can target x86 and ARM (32b
and 64b variants).

It will be used to minimize Android Common Kernel dependencies on GNU binutils
for the "S" release, as part of:
https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/BINUTILS_KERNEL_DEPRECATION.md
(The creation of this repository is step 4).

If all goes well, then these binaries may never end up being used. Their
existence is a hedge in the case of updates to kernel sources requiring the
fallback to GNU `as` instead of Clang's integrated assembler.

As of Sept 29 2020,
https://www.kernel.org/doc/html/latest/process/changes.html
for a v5.9-rc7 Linux kernel requires binutils version 2.23 or newer. These
binaries are based off of 2.27.0.20170315.
