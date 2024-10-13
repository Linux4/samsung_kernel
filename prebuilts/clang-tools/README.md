Prebuilts for Clang/LLVM-based tools used in Android
====================================================

For the latest version of this doc, please make sure to visit:
[Android Clang/LLVM-based Tools Readme Doc](https://android.googlesource.com/platform/prebuilts/clang-tools/+/master/README.md)

Build Instructions
------------------

```
$ mkdir clang-tools && cd clang-tools
$ repo init -u https://android.googlesource.com/platform/manifest -b clang-tools
$ repo sync -c
$ OUT_DIR=out prebuilts/clang-tools/build-prebuilts.sh
```

Update Prebuilts
----------------

From an AOSP main tree or a clang-tools tree run:

```
$ prebuilts/clang-tools/update-prebuilts.sh \
    <build-id from go/ab/aosp-clang-tools>
```
