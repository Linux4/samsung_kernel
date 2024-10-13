# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Helper macros to forward to native equivalents.
"""

def java_binary(**attrs):
    native.java_binary(**attrs)

def java_import(**attrs):
    native.java_import(**attrs)

def java_library(**attrs):
    native.java_library(**attrs)

def java_lite_proto_library(**attrs):
    native.java_lite_proto_library(**attrs)

def java_proto_library(**attrs):
    native.java_proto_library(**attrs)

def java_test(**attrs):
    native.java_test(**attrs)

def java_package_configuration(**attrs):
    native.java_package_configuration(**attrs)

def java_plugin(**attrs):
    native.java_plugin(**attrs)

def java_runtime(**attrs):
    native.java_runtime(**attrs)

def java_toolchain(**attrs):
    native.java_toolchain(**attrs)
