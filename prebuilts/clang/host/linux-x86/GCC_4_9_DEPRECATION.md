# Android GCC 4.9 Deprecation Schedule
Android’s GCC 4.9 has been deprecated in favor of Clang, and will be removed
from Android in January 2020 as per the below timeline.

## Timeline
* January 2019
  * Move GCC binary to new file name.
  * Create shell wrapper with old GCC binary name that calls new binary name.
  * Shell wrapper print deprecation warning to stdout, with a link to more info.
* April 2019
  * Move GCC binary name.
  * Add 1s sleep to wrapper.
* July 2019
  * Move GCC binary name.
  * Total 3s sleep.
* October 2019
  * Move GCC binary name.
  * Total 10s sleep.
* January 2020
  * Remove GCC from Android.

## Questions
Please report bugs and feature requests to <android-llvm@googlegroups.com>.

Prebuilts of Clang can be found for Linux x86_64 hosts:

https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/master

Prebuilts of newer versions of Clang will be made available regularly.  These
prebuilts exist only to accelerate Clang’s release cycle and contain no out of
tree patches, only reverts and cherry-picks from upstream.

Vendors looking to keep GCC from being removed may pin the revision of Android
GCC prebuilts, but should not expect Google to maintain GCC support going
forward.
