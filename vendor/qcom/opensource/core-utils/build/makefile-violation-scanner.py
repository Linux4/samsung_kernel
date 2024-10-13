# Copyright (c) 2021 The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
from __future__ import print_function
try:
    from makefile_whitelist import *
except ImportError:
    pass
import os
import subprocess
import re
import sys

kernel_errors = set()
shell_errors = set()
recursive_errors = set()
rm_errors = set()
datetime_errors = set()
local_copy_headers_errors = set()
target_product_errors = set()
is_product_in_list_errors = set()
ro_build_product_errors = set()

# Variables for the kernel check
cur_file_name = str()
cnt_module = 0
fnd_c_include = False
fnd_add_dep = False

# Default enforcement variables
BUILD_REQUIRES_KERNEL_DEPS = True
BUILD_BROKEN_USES_SHELL = False
BUILD_BROKEN_USES_RECURSIVE_VARS = True
BUILD_BROKEN_USES_RM_OUT = False
BUILD_BROKEN_USES_DATETIME = False
BUILD_BROKEN_USES_LOCAL_COPY_HEADERS = False
BUILD_BROKEN_USES_TARGET_PRODUCT = False

# Set whitelist variables if not defined
whitelist_vars = ["SHELL_WHITELIST", "RM_WHITELIST", "LOCAL_COPY_HEADERS_WHITELIST",
                  "DATETIME_WHITELIST", "TARGET_PRODUCT_WHITELIST", "RECURSIVE_WHITELIST", "KERNEL_WHITELIST"]
for w in whitelist_vars:
    if w not in globals():
        exec("%s = set()" % w, globals())


def check_kernel_deps(line, file_name):
    global cur_file_name
    global cnt_module
    global fnd_c_include
    global fnd_add_dep

    if file_name != cur_file_name:
        cur_file_name = file_name
        cnt_module = 0
        fnd_c_include = False
        fnd_add_dep = False

    if re.match(r'LOCAL_C_INCLUDES.*KERNEL_OBJ/usr.*', line):
        fnd_c_include = True
    elif re.match(r'LOCAL_ADDITIONAL_DEPENDENCIES.*KERNEL_OBJ/usr.*', line):
        fnd_add_dep = True
    elif re.match(r'.*CLEAR_VARS.*', line):
        if cnt_module > 0:
            if fnd_c_include and not fnd_add_dep:
                kernel_errors.add(str(cnt_module)+'::'+file_name)
        fnd_c_include = False
        fnd_add_dep = False
        cnt_module += 1


def check_shell(line, file_name):
    global shell_errors
    if re.match(r'.*\$\(shell.*', line):
        shell_errors.add(file_name)


def check_recursive(line, file_name):
    global recursive_errors
    if line.startswith('$'):
        pass
    elif re.match(r'.*[:+?=\>\<]=.*', line):
        pass
    elif re.match(r'.*=.*', line):
        recursive_errors.add(file_name)


def check_rm(line, file_name):
    global rm_errors
    if re.match(r'.*([\s]+|^|@)rm[\s]+.*', line):
        rm_errors.add(file_name)


def check_datetime(line, file_name):
    global datetime_errors
    if re.match(r'.*-Wno-error=date-time.*', line) or re.match(r'.*-Wno-date-time.*', line):
        datetime_errors.add(file_name)


def check_local_copy_headers(line, file_name):
    global local_copy_headers_errors
    if re.match(r'.*LOCAL_COPY_HEADERS_TO.*', line):
        local_copy_headers_errors.add(file_name)


def check_target_product_related(line, file_name):
    global target_product_errors
    global is_product_in_list_errors
    global ro_build_product_errors
    if re.match(r'.*\$\(TARGET_PRODUCT\).*', line) or \
            re.match(r'.*\$\{TARGET_PRODUCT\}.*', line) or \
            re.match(r'.*\'TARGET_PRODUCT\'.*', line) or \
            re.match(r'.*\"TARGET_PRODUCT\".*', line) or \
            re.match(r'.*\$TARGET_PRODUCT.*', line):
        target_product_errors.add(file_name)

    if re.match(r'.*is-product-in-list.*', line):
        is_product_in_list_errors.add(file_name)

    if re.match(r'.*ro.build.product.*', line):
        ro_build_product_errors.add(file_name)


def scan_files(file_list):
    # Scan each file
    for f in file_list:
        if f == '%s/makefile-violation-scanner.py' % QTI_BUILDTOOLS_DIR.replace(ANDROID_BUILD_TOP, ''):
            continue
        try:
            with open(ANDROID_BUILD_TOP+f) as o_file:
                for line in o_file:
                    line = line.strip()
                    if not line.startswith('#'):

                        # Take care of backslash (\) continuation
                        while line.endswith('\\'):
                            try:
                                line = line[:-1] + next(o_file).strip()
                            except StopIteration:
                                line = line[:-1]

                        if re.match(r'.*/Android.mk', f):
                            # Check all makefile only issues
                            if f not in SHELL_WHITELIST:
                                check_shell(line, f)
                            if f not in RM_WHITELIST:
                                check_rm(line, f)
                            if f not in LOCAL_COPY_HEADERS_WHITELIST:
                                check_local_copy_headers(line, f)
                            if f not in DATETIME_WHITELIST:
                                check_datetime(line, f)
                            # if f not in RECURSIVE_WHITELIST:
                            #     check_recursive(line, f)
                            if f not in KERNEL_WHITELIST:
                                check_kernel_deps(line, f)

                        # Check TARGET_PRODUCT in all files
                        if f not in TARGET_PRODUCT_WHITELIST:
                            check_target_product_related(line, f)

                # Check kernel issue at end of file (in case no CLEAR_VARS)
                if fnd_c_include and not fnd_add_dep:
                    kernel_errors.add(str(cnt_module)+'::'+f)
        except IOError:
            print("Error opening file %s" % f)


def print_messages():

    found_errors = False

    if BUILD_REQUIRES_KERNEL_DEPS and len(kernel_errors) > 0:
        print("-----------------------------------------------------")
        print("cnt_kernel_error : %s" % len(kernel_errors))
        print("Error: Missing LOCAL_ADDITIONAL_DEPENDENCIES in below modules.")
        print("please use LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr")
        for error in kernel_errors:
            module = error.split("::")[0]
            file_name = error.split("::")[1]
            print("    Module: %s in %s" % (module, file_name))
        print("-----------------------------------------------------")
        found_errors = True
    if (not BUILD_BROKEN_USES_SHELL) and len(shell_errors) > 0:
        print("-----------------------------------------------------")
        print("cnt_shell_error : %s" % len(shell_errors))
        print("Error: Using $(shell) in below files. Please remove usage of $(shell)")
        for file_name in shell_errors:
            print("    %s" % file_name)
        print("-----------------------------------------------------")
        found_errors = True
    if (not BUILD_BROKEN_USES_RECURSIVE_VARS) and len(recursive_errors) > 0:
        print("-----------------------------------------------------")
        print("cnt_recursive_error : %s" % len(recursive_errors))
        print("Warning: Using recursive assignment (=) in below files.")
        print("please review use of recursive assignment and convert to simple assignment (:=) if necessary.")
        for file_name in recursive_errors:
            print("    %s" % file_name)
        print("-----------------------------------------------------")
        found_errors = True
    if (not BUILD_BROKEN_USES_RM_OUT) and len(rm_errors) > 0:
        print("-----------------------------------------------------")
        print("cnt_rm_error : %s" % len(rm_errors))
        print("Error: Using rm in below makefiles. Please remove use of rm to prevent recompilation.")
        for file_name in rm_errors:
            print("    %s" % file_name)
        print("-----------------------------------------------------")
        found_errors = True
    if (not BUILD_BROKEN_USES_DATETIME) and len(datetime_errors) > 0:
        print("-----------------------------------------------------")
        print("cnt_datetime_error : %s" % len(datetime_errors))
        print("Error: Using CFLAG -Wno-error=date-time or -Wno-date-time in below makefiles. This may lead to varying build output.")
        print("Please remove use of this CFLAG.")
        for file_name in datetime_errors:
            print("    %s" % file_name)
        print("-----------------------------------------------------")
        found_errors = True
    if (not BUILD_BROKEN_USES_LOCAL_COPY_HEADERS) and len(local_copy_headers_errors) > 0:
        print("-----------------------------------------------------")
        print("cnt_local_copy_headers_error : %s" %
              len(local_copy_headers_errors))
        print("Error: Using local_copy_headers in below makefiles. This will be deprecated soon, please remove.")
        for file_name in local_copy_headers_errors:
            print("    %s" % file_name)
        print("-----------------------------------------------------")
        found_errors = True
    if (not BUILD_BROKEN_USES_TARGET_PRODUCT) and len(target_product_errors) > 0:
        print("-----------------------------------------------------")
        print("cnt_target_product_error : %s" % len(target_product_errors))
        print("Error: Using TARGET_PRODUCT in below makefiles. Please replace them with TARGET_BOARD_PLATFORM")
        for file_name in target_product_errors:
            print("    %s" % file_name)
        print("-----------------------------------------------------")
        found_errors = True
    if (not BUILD_BROKEN_USES_TARGET_PRODUCT) and len(is_product_in_list_errors) > 0:
        print("-----------------------------------------------------")
        print("cnt_is_product_in_list_error : %s" %
              len(is_product_in_list_errors))
        print("Error: Using is-product-in-list in below makefiles. Please replace them with is-board-platform-in-list")
        for file_name in is_product_in_list_errors:
            print("    %s" % file_name)
        print("-----------------------------------------------------")
        found_errors = True
    if (not BUILD_BROKEN_USES_TARGET_PRODUCT) and len(ro_build_product_errors) > 0:
        print("-----------------------------------------------------")
        print("cnt_ro_build_product_error : %s" % len(ro_build_product_errors))
        print("Error: Using ro.build.product in below makefiles. Please replace them with ro.board.platform")
        for file_name in ro_build_product_errors:
            print("    %s" % file_name)
        print("-----------------------------------------------------")
        found_errors = True

    return found_errors


def main():

    global QTI_BUILDTOOLS_DIR, ANDROID_BUILD_TOP
    QTI_BUILDTOOLS_DIR = "%s/build" % os.environ.get('QTI_BUILDTOOLS_DIR')
    ANDROID_BUILD_TOP = os.environ.get('ANDROID_BUILD_TOP') + '/'
    TARGET_PRODUCT = os.environ.get('TARGET_PRODUCT')
    subdirs = ["hardware/qcom", "vendor/qcom", "device/qcom"]
    subdir_abspath_str = " ".join([ANDROID_BUILD_TOP+i for i in subdirs])

    print("-----------------------------------------------------")
    print(" Checking makefile errors ")
    print("      Checking dependency on kernel headers ......")
    print("      Checking $(shell) usage ......")
    print("      Checking recursive usage ......")
    print("      Checking rm usage ......")
    print("      Checking -Wno-error=date-time usage ......")
    print("      Checking local_copy_headers usage ......")
    print("      Checking TARGET_PRODUCT usage ......")
    print("      Checking is-product-in-list usage ......")
    print("      Checking ro.build.product usage ......")
    print("-----------------------------------------------------")
    sys.stdout.flush()

    # Read configuration
    try:
        with open("%s/makefile_violation_config.mk" % QTI_BUILDTOOLS_DIR) as config_file:
            for line in config_file.readlines():
                statement = line.strip().replace(":=", "=").replace(
                    "true", "True").replace("false", "False")
                exec(statement, globals())
    except IOError:
        pass  # Uses default values

    # Read enforcement override flags
    try:
        with open("%sdevice/qcom/%s/enforcement_override.mk" % (ANDROID_BUILD_TOP, TARGET_PRODUCT)) as override_file:
            for line in override_file.readlines():
                statement = line.strip().replace(":=", "=").replace(
                    "true", "True").replace("false", "False")
                exec(statement, globals())
    except IOError:
        pass  # No overrides for the target

    # Find all files and convert to relative path
    with open(os.devnull, 'w') as dev_null:
        files = subprocess.check_output(
            """find %s -type f \( -iname '*.mk' -o -iname '*.sh' -o -iname '*.py' \); :;""" % subdir_abspath_str, shell=True, stderr=dev_null)
        files = files.strip().replace(ANDROID_BUILD_TOP, '').split('\n')

    scan_files(files)

    found_errors = print_messages()
    if found_errors:
        exit(1)
    else:
        exit(0)


if __name__ == '__main__':
    main()
