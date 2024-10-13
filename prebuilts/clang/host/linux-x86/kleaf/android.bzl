# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""For Android kernel builds, configure CC toolchain for target binaries."""

load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "feature",
    "flag_group",
    "flag_set",
)
load(
    "@rules_cc//cc:action_names.bzl",
    "ALL_CC_COMPILE_ACTION_NAMES",
    "ALL_CC_LINK_ACTION_NAMES",
)

def _ldflags(ndk_triple):
    # From _setup_env.sh
    # USERLDFLAGS
    return feature(
        name = "kleaf-android-ldflags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = ALL_CC_LINK_ACTION_NAMES,
                flag_groups = [
                    flag_group(
                        flags = [
                            "--target={}".format(ndk_triple),
                        ],
                    ),
                ],
            ),
        ],
        implies = [
            # We need to set -fuse-ld=lld for Android's build env since AOSP LLVM's
            # clang is not configured to use LLD by default, and BFD has been
            # intentionally removed. This way CC_CAN_LINK can properly link the test in
            # scripts/cc-can-link.sh.
            "kleaf-lld-compiler-rt",
        ],
    )

def _clfags(ndk_triple):
    # From _setup_env.sh
    # USERCFLAGS
    return feature(
        name = "kleaf-android-cflags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = ALL_CC_COMPILE_ACTION_NAMES,
                flag_groups = [
                    flag_group(
                        flags = [
                            "--target={}".format(ndk_triple),
                            # Some kernel headers trigger -Wunused-function for unused static
                            # functions with clang; GCC does not warn about unused static inline
                            # functions. The kernel sets __attribute__((maybe_unused)) on such
                            # functions when W=1 is not set.
                            "-Wno-unused-function",

                            # To help debug these flags, consider commenting back in the following,
                            # and add `echo $@ > /tmp/log.txt` and `2>>/tmp/log.txt` to the
                            # invocation of $@ in scripts/cc-can-link.sh.
                            # "-Wl,--verbose",
                            # "-v",
                        ],
                    ),
                ],
            ),
        ],
    )

def _features(ctx):
    if ctx.attr.ndk_triple:
        return [
            _ldflags(ctx.attr.ndk_triple),
            _clfags(ctx.attr.ndk_triple),
        ]
    return []

android = struct(
    features = _features,
)
