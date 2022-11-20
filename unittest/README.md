This repository contains unit test codes for kernel.

When you add new test codes, please following steps..

1. Add `build` file to make ko file
 * `build` run the make with argument - kernel path like `make -C "$1" ARCH="$2" CROSS_COMPILE="$3" M=$(pwd) modules`
 ** $1: Kernel directory
 ** $2: Architecture. arm or arm64
 ** $3: Cross-compile
 ** If you do not need the argument, ignore it.
 * Samsung build scripts will run the all `build` script in this repository, copy all files in `target` to `android/out/target/product/<product name>/unittest/target/<module>/`, and make `UNIT_TEST_TARGET_<product name and so on>.tar` which contains all files in `target`.
 * Samsung build scripts will run the all `build` script in this repository, copy all files in `host` to `android/out/target/product/<product name>/unittest/host/<module>`, and make `UNIT_TEST_HOST_<product name and so on>.tar` which contains all files in `host`.

 2. Add the test related scripts to LTP
  * LTP will get the `UNIT_TEST_TARGET_<model name>.tar`, extract it, push the files to the device.
  * LTP will get the `UNIT_TEST_HOST_<model name>.tar`, extract it, copy to runing host.

If you have any questions, please contact to repository owners.
* Alex Kyoungdong Jang <alex.jang@samsung.com>

