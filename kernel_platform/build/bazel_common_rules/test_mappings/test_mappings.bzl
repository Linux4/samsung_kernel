# Copyright (C) 2022 The Android Open Source Project
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

load("//build/bazel_common_rules/exec:embedded_exec.bzl", "embedded_exec")

def test_mappings_dist(
        name,
        dist_dir = None,
        **kwargs):
    """Run this target to generate test mapping archive to the location given
    by `--dist_dir` command-line argument. If `--dist_dir` command-line argument
    is not specified, default to the `dist_dir` argument of this rule.

    For example:

    ```
    test_mappings(
        name = "my_test_mappings",
        args = ["--dist_dir", "out/dist"],
    )
    ```

    ```
    # generate to <workspace_root>/out/dist
    $ bazel run my_test_mappings

    # generate to <workspace_root>/path
    $ bazel run my_test_mappings -- --dist_dir=path

    # generate to /tmp/path
    $ bazel run my_test_mappings -- --dist_dir=/tmp/path
    ```

    Args:
        name: name of this target.
        kwargs: Additional arguments to the internal rule, e.g. `visibility`.
    """

    native.sh_binary(
        name = name + "_internal",
        srcs = ["//build/bazel_common_rules/test_mappings:test_mappings.sh"],
        data = ["//prebuilts/build-tools:linux-x86"],
        args = ["--dist_dir", dist_dir] if dist_dir else None,
        **kwargs
    )

    embedded_exec(
        name = name,
        actual = name + "_internal",
    )
