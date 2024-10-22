# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2020 Mediatek Inc.

#!/usr/bin/env python

from argparse import ArgumentParser, FileType, ArgumentDefaultsHelpFormatter
import os
import sys
import stat
import shutil
import re

def help():
    print('Usage:')
    print('  python scripts/gen_build_config.py --project <project> --kernel-defconfig <kernel project defconfig file> --kernel-defconfig-overlays <kernel project overlay defconfig files> --kernel-build-config-overlays <kernel build config overlays> --build-mode <mode> --out-file <gen build.config>')
    print('Or:')
    print('  python scripts/gen_build_config.py -p <project> --kernel-defconfig <kernel project defconfig file> --kernel-defconfig-overlays <kernel project overlay defconfig files> --kernel-build-config-overlays <kernel build config overlays> -m <mode> -o <gen build.config>')
    print('')
    print('Attention: Must set generated build.config, and project!!')
    sys.exit(2)

def main(**args):

    project = args["project"]
    kernel_defconfig = args["kernel_defconfig"]
    kernel_defconfig_overlays = args["kernel_defconfig_overlays"]
    kernel_build_config_overlays = args["kernel_build_config_overlays"]
    build_mode = args["build_mode"]
    abi_mode = args["abi_mode"]
    out_file = args["out_file"]
    if (project == '') or (out_file == ''):
        help()

    gen_build_config = out_file
    gen_build_config_dir = os.path.dirname(gen_build_config)
    if not os.path.exists(gen_build_config_dir):
        os.makedirs(gen_build_config_dir)

    if (build_mode == 'eng') or (build_mode == 'userdebug'):
          mode_config = '%s.config' % (build_mode)

    file_text = []
    project_line='PROJECT=%s' % project
    file_text.append(project_line)
    defconfig_overlays_line='DEFCONFIG_OVERLAYS="%s"' % kernel_defconfig_overlays
    file_text.append(defconfig_overlays_line)
    mode_line='MODE=%s' % build_mode
    file_text.append(mode_line)

    file_handle = open(gen_build_config, 'w')
    for line in file_text:
        file_handle.write(line + '\n')
    file_handle.close()


if __name__ == '__main__':
    parser = ArgumentParser(formatter_class=ArgumentDefaultsHelpFormatter)

    parser.add_argument("-p","--project", dest="project", help="specify the project to build kernel.", required=True)
    parser.add_argument("--kernel-defconfig", dest="kernel_defconfig", help="special kernel project defconfig file.",default="")
    parser.add_argument("--kernel-defconfig-overlays", dest="kernel_defconfig_overlays", help="special kernel project overlay defconfig files.",default="")
    parser.add_argument("--kernel-build-config-overlays", dest="kernel_build_config_overlays", help="special kernel build config overlays files.",default="")
    parser.add_argument("-m","--build-mode", dest="build_mode", help="specify the build mode to build kernel.", default="user")
    parser.add_argument("--abi", dest="abi_mode", help="specify whether build.config is used to check ABI.", default="no")
    parser.add_argument("-o","--out-file", dest="out_file", help="specify the generated build.config file.", required=True)

    args = parser.parse_args()
    main(**args.__dict__)
