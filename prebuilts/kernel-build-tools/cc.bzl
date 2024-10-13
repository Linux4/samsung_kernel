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

"""CC rules for this package."""

def import_libraries(
        name,
        hdrs,
        strip_include_prefix,
        shared_libraries,
        **kwargs):
    """Define a cc_library from a list of prebuilts.

    Args:
        name: name of the target.
        hdrs: The list of header files published by this library to be directly included by
          sources in dependent rules.
        strip_include_prefix: The prefix to strip from the paths of the headers of this rule.
        shared_libraries: A list of dynamic library files.

          This cannot be a `filegroup`; it must be a list of targets, each corresponding
          to a single dynamic library file.
        **kwargs: other arguments to the top-level target.
    """
    private_kwargs = dict(kwargs)
    private_kwargs["visibility"] = ["//visibility:private"]

    targets = []
    for f in shared_libraries:
        native.cc_import(
            name = name + "_" + f,
            shared_library = f,
            **private_kwargs
        )
        targets.append(name + "_" + f)

    native.cc_library(
        name = name,
        deps = targets,
        hdrs = hdrs,
        strip_include_prefix = strip_include_prefix,
        **kwargs
    )
