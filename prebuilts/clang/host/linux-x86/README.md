Android Clang/LLVM Prebuilts
============================

For the latest version of this doc, please make sure to visit:
[Android Clang/LLVM Prebuilts Readme Doc](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/README.md)

LLVM Users
----------

* [**Android Platform**](https://android.googlesource.com/platform/)
  * Currently clang-r498229
  * clang-r487747c for Android U release
  * clang-r450784d for Android T release
  * clang-r416183b1 for Android S release
  * clang-r383902b1 for Android R-QPR2 release
  * clang-r383902b for Android R release
  * clang-r353983c1 for Android Q-QPR2 release
  * clang-r353983c for Android Q release
  * Look for "ClangDefaultVersion" and/or "clang-" in [build/soong/cc/config/global.go](https://android.googlesource.com/platform/build/soong/+/master/cc/config/global.go/).
    * [AOSP Code Search link](https://cs.android.com/android/platform/superproject/+/master:build/soong/cc/config/global.go?q=ClangDefaultVersion)

* [**Android Platform LLVM binutils**](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/llvm-binutils-stable/)
  * Currently clang-r498229
  * These are *symlinks* to llvm tools and can be updated by running [update-binutils.py](https://android.googlesource.com/toolchain/llvm_android/+/refs/heads/master/update-binutils.py).

* [**Android Platform clang-stable**](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-stable/)
  * Currently clang-r498229
  * These are *copies* of some clang tools and can be updated by running [update-clang-stable.py](https://android.googlesource.com/toolchain/llvm_android/+/refs/heads/master/update-clang-stable.py).

* [**RenderScript**](https://developer.android.com/guide/topics/renderscript/index.html)
  * Currently clang-3289846
  * Look for "RSClangVersion" and/or "clang-" in [build/soong/cc/config/global.go](https://android.googlesource.com/platform/build/soong/+/master/cc/config/global.go/).
    * [AOSP Code Search link](https://cs.android.com/android/platform/superproject/+/master:build/soong/cc/config/global.go?q=RSClangVersion)

* [**Android Linux Kernel**](http://go/android-systems)
  * Currently clang-r498229
    * Look for "clang-" in [mainline build configs](https://android.googlesource.com/kernel/common/+/refs/heads/android-mainline/build.config.constants).
    * Look for "clang-" in [android15-6.1 build configs](https://android.googlesource.com/kernel/common/+/refs/heads/android15-6.1/build.config.constants)
  * When adding or removing a clang prebuilt, the list in `kleaf/versions.bzl` needs to be updated.
  * Internal LLVM developers should look in the partner gerrit for more kernel configurations.

* [**NDK**](https://developer.android.com/ndk)
  * Currently clang-r487747c
  * Look for "clang-" in [ndk/toolchains.py](https://android.googlesource.com/platform/ndk/+/refs/heads/master/ndk/toolchains.py)

* [**Trusty**](https://source.android.com/security/trusty/)
  * LINUX_CLANG_BINDIR: clang-r475365b
  * CLANG_BINDIR: clang-r475365b
  * Look for "clang-" in [vendor/google/aosp/scripts/envsetup.sh](https://android.googlesource.com/trusty/vendor/google/aosp/+/master/scripts/envsetup.sh).

* [**Android Emulator**](https://developer.android.com/studio/run/emulator.html)
  * Currently clang-r487747c
  * Linux emulator is temporarily hardcoded to clang-r487747c, see b/268674933
  * Look for "clang-" in [external/qemu/android/build/toolchains.json](https://android.googlesource.com/platform/external/qemu/+/emu-master-dev/android/build/toolchains.json#2).
    * Note that they work out of the emu-master-dev branch.
    * [Android Code Search link](https://cs.android.com/android/platform/superproject/+/emu-master-dev:external/qemu/android/build/toolchains.json?q=clang)

* [**Context Hub Runtime Environment (CHRE)**](https://android.googlesource.com/platform/system/chre/)
  * Currently clang-r498229
  * Look in [system/chre/build/clang.mk](https://googleplex-android.googlesource.com/platform/system/chre/+/refs/heads/master/build/clang.mk#13).
    * [Internal Code Search link](https://source.corp.google.com/android/system/chre/build/clang.mk?q=clang-)

* [**OpenJDK (jdk/build)**](https://android.googlesource.com/toolchain/jdk/build/)
  * Currently clang-r416183b
  * Look for "clang-" in [build-jetbrainsruntime-linux.sh](https://android.googlesource.com/toolchain/jdk/build/+/refs/heads/master/build-jetbrainsruntime-linux.sh)
  * Look for "clang-" in [build-openjdk-darwin.sh](https://android.googlesource.com/toolchain/jdk/build/+/refs/heads/master/build-openjdk-darwin.sh)

* [**Clang Tools**](https://android.googlesource.com/platform/prebuilts/clang-tools/)
  * Currently clang-r498229
  * Look for "clang-r" in [envsetup.sh](https://android.googlesource.com/platform/development/+/refs/heads/master/vndk/tools/header-checker/android/envsetup.sh)
  * Check out branch clang-tools and run test: OUT_DIR=out prebuilts/clang-tools/build-prebuilts.sh

* **Android Rust**
  * Toolchain
    * Currently clang-r498229
    * Look for "CLANG_REVISION" in [paths.py](https://android.googlesource.com/toolchain/android_rust/+/refs/heads/master/paths.py)
  * Bindgen
    * Currently clang-r498229
    * Look for "bindgenClangVersion" in [bindgen.go](https://android.googlesource.com/platform/build/soong/+/refs/heads/master/rust/bindgen.go)

* **Stage 1 compiler**
  * Currently clang-r498229
  * Look for "clang-r" in [toolchain/llvm_android/constants.py](https://android.googlesource.com/toolchain/llvm_android/+/refs/heads/master/constants.py)
  * Note the chicken & egg paradox of a self hosting bootstrapping compiler; this can only be updated AFTER a new prebuilt is checked in.

* **Android Studio / Android Game Development Extension**
  * Currently clang-r498229
  * Look in [lldb-utils/config/clang.version](https://googleplex-android.git.corp.google.com/platform/external/lldb-utils/+/refs/heads/lldb-master-dev/config/clang.version)



Prebuilt Versions
-----------------

* [clang-3289846](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-3289846/) - September 2016
* [clang-r328903](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r328903/) - May 2018
* [clang-r339409b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r339409b/) - October 2018
* [clang-r344140b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r344140b/) - November 2018
* [clang-r346389b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r346389b/) - December 2018
* [clang-r346389c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r346389c/) - January 2019
* [clang-r349610](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r349610/) - February 2019
* [clang-r349610b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r349610b/) - February 2019
* [clang-r353983b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r353983b/) - March 2019
* [clang-r353983c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r353983c/) - April 2019
* [clang-r353983d](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r353983d/) - June 2019
* [clang-r365631b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master/clang-r365631b/) - September 2019
* [clang-r365631c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-r365631c/) - September 2019
* [clang-r365631c1](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-r365631c/) - March 2020
* [clang-r370808](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-r370808/) - December 2019
* [clang-r370808b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-r370808b/) - January 2020
* [clang-r377782b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r377782b) - February 2020
* [clang-r377782c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r377782c) - March 2020
* [clang-r377782d](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r377782d) - April 2020
* [clang-r383902](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r383902) - May 2020
* [clang-r383902b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r383902b) - June 2020
* [clang-r383902b1](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r383902b1) - October 2020
* [clang-r383902c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r383902c) - June 2020
* [clang-r399163](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r399163) - August 2020
* [clang-r399163b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r399163b) - October 2020
* [clang-r407598](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r407598) - January 2021
* [clang-r407598b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r407598b) - January 2021
* [clang-r412851](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r412851) - February 2021
* [clang-r416183](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r416183) - March 2021
* [clang-r416183b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r416183b) - April 2021
* [clang-r416183c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r416183b) - June 2021
* [clang-r416183b1](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r416183b) - June 2021
* [clang-r428724](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r428724) - August 2021
* [clang-r433403](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r433403) - September 2021
* [clang-r433403b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r433403b) - October 2021
* [clang-r437112](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r437112) - November 2021
* [clang-r437112b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r437112b) - January 2022
* [clang-r445002](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r445002) - February 2022
* [clang-r450784](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r450784) - March 2022
* [clang-r450784b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r450784b) - April 2022
* [clang-r450784c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r450784c) - April 2022
* [clang-r450784d](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r450784d) - April 2022
* [clang-r450784e](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r450784e) - April 2022
* [clang-r458507](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r458507) - July 2022
* [clang-r468909](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r468909) - October 2022
* [clang-r468909b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+log/refs/heads/master/clang-r468909b) - October 2022
* [clang-r475365b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-r475365b) - December 2022
* [clang-r487747](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-r487747) - March 2023
* [clang-r487747b](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-r487747b) - April 2023
* [clang-r487747c](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-r487747c) - May 2023
* [clang-r498229](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/refs/heads/master/clang-r498229) - July 2023


More Information
----------------

We have a public mailing list that you can subscribe to:
[android-llvm@googlegroups.com](https://groups.google.com/forum/#!forum/android-llvm)

See also our [release notes](RELEASE_NOTES.md).
