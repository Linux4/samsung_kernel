# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2020 Mediatek Inc.

#!/usr/bin/env python

from argparse import ArgumentParser, FileType, ArgumentDefaultsHelpFormatter
import os
import sys
import stat
import shutil
import re

# Parse project defconfig to get special config file, build config file and out-of-tree kernel modules
def get_config_in_defconfig(file_name, kernel_dir):
    file_handle = open(file_name, 'r')
    pattern_buildconfig = re.compile('^CONFIG_BUILD_CONFIG_FILE\s*=\s*(.+)$')
    pattern_extmodules = re.compile('^CONFIG_EXT_MODULES\s*=\s*(.+)$')
    build_config = ''
    ext_modules = ''
    for line in file_handle.readlines():
        result = pattern_buildconfig.match(line)
        if result:
            build_config = result.group(1).strip('"')
        result = pattern_extmodules.match(line)
        if result:
            ext_modules = result.group(1).strip('"')
    file_handle.close()
    return (build_config, ext_modules)

def help():
    print('Usage:')
    print('  python scripts/gen_build_config.py --project <project> --kernel-defconfig <kernel project defconfig file> --kernel-defconfig-overlays <kernel project overlay defconfig files> --kernel-build-config-overlays <kernel build config overlays> --build-mode <mode> --out-file <gen build.config>')
    print('Or:')
    print('  python scripts/gen_build_config.py -p <project> --kernel-defconfig <kernel project defconfig file> --kernel-defconfig-overlays <kernel project overlay defconfig files> --kernel-build-config-overlays <kernel build config overlays> -m <mode> -o <gen build.config>')
    print('')
    print('Attention: Must set generated build.config, and project or kernel project defconfig file!!')
    sys.exit(2)

def main(**args):

    project = args["project"]
    kernel_defconfig = args["kernel_defconfig"]
    kernel_defconfig_overlays = args["kernel_defconfig_overlays"]
    kernel_build_config_overlays = args["kernel_build_config_overlays"]
    build_mode = args["build_mode"]
    abi_mode = args["abi_mode"]
    out_file = args["out_file"]
    if ((project == '') and (kernel_defconfig == ''))  or (out_file == ''):
        help()

    # get gen_build_config.py absolute path and name by sys.argv[0]: /***/{kernel dir}/scripts/gen_build_config.py
    # kernel directory is dir(sys.argv[0])/../.., absolute path
    current_working_dir = os.path.abspath(os.getcwd())
    current_file_path = os.path.abspath(sys.argv[0])
    abs_kernel_dir = os.path.dirname(os.path.dirname(current_file_path))
    kernel_dir = abs_kernel_dir.split(current_working_dir)[1].lstrip('/')

    gen_build_config = out_file
    gen_build_config_dir = os.path.dirname(gen_build_config)
    if not os.path.exists(gen_build_config_dir):
        os.makedirs(gen_build_config_dir)

    mode_config = ''
    if (build_mode == 'eng') or (build_mode == 'userdebug'):
          mode_config = '%s.config' % (build_mode)
    project_defconfig = ''
    project_defconfig_name = ''
    if kernel_defconfig:
          project_defconfig_name = kernel_defconfig
          pattern_project = re.compile('^(.+)_defconfig$')
          project = pattern_project.match(kernel_defconfig).group(1).strip()
    else:
          project_defconfig_name = '%s_defconfig' % (project)
    defconfig_dir = ''
    if project_defconfig_name != 'gki_defconfig':
        if os.path.exists('%s/arch/arm/configs/%s' % (abs_kernel_dir, project_defconfig_name)):
            defconfig_dir = 'arch/arm/configs'
        elif os.path.exists('%s/arch/arm64/configs/%s' % (abs_kernel_dir, project_defconfig_name)):
            defconfig_dir = 'arch/arm64/configs'
        else:
            print('Error: cannot find project defconfig file under ' + abs_kernel_dir)
            sys.exit(2)
        project_defconfig = '%s/%s/%s' % (abs_kernel_dir, defconfig_dir, project_defconfig_name)

    device_modules_build_config = ''
    ext_modules = ''
    if project_defconfig_name == 'gki_defconfig':
        device_modules_build_config = '%s/build.config.mtk_kernel_device_modules' % (abs_kernel_dir)
        mode_config = ''
    else:
        (device_modules_build_config, ext_modules) = get_config_in_defconfig(project_defconfig, os.path.basename(abs_kernel_dir))
        device_modules_build_config = '%s/%s' % (abs_kernel_dir, device_modules_build_config)

    if not os.path.exists(device_modules_build_config):
        print('Error: cannot get build.config under ' + abs_kernel_dir + '.')
        print('Please check whether ' + project_defconfig + ' defined CONFIG_BUILD_CONFIG_FILE.')
        sys.exit(2)

    file_text = []
    file_text.append("PATH=/usr/bin:/bin:${ROOT_DIR}/../prebuilts/perl/linux-x86/bin:${ROOT_DIR}/build/kernel/build-tools/path/linux-x86")
    file_text.append("TRIM_NONLISTED_KMI=")
    file_text.append("KMI_SYMBOL_LIST_STRICT_MODE=")
    file_text.append("MODULES_ORDER=")
    file_text.append("KMI_ENFORCED=1")
    file_text.append("if [ \"x${DO_ABI_MONITOR}\" == \"x1\" ]; then")
    file_text.append("  KMI_SYMBOL_LIST_MODULE_GROUPING=0")
    file_text.append("  KMI_SYMBOL_LIST_ADD_ONLY=1")
    file_text.append("  ADDITIONAL_KMI_SYMBOL_LISTS=\"${ADDITIONAL_KMI_SYMBOL_LISTS} android/abi_gki_aarch64\"")
    file_text.append("fi")
    file_text.append("if [ -z \"${SOURCE_DATE_EPOCH}\" ]; then")
    file_text.append("  export SOURCE_DATE_EPOCH=0")
    file_text.append("  export GKI_SOURCE_DATE_EPOCH=0")
    file_text.append("fi")

    file_text.append("\nDEFCONFIG=olddefconfig")
    all_defconfig = '${ROOT_DIR}/${KERNEL_DIR}/arch/arm64/configs/gki_defconfig'
    pre_defconfig_cmds = 'cd ${KERNEL_DIR} && make O=${OUT_DIR} gki_defconfig && cd ${ROOT_DIR}'
    if project_defconfig_name != 'gki_defconfig':
        kernel_defconfig_overlays_files = ''
        for name in kernel_defconfig_overlays.split():
            kernel_defconfig_overlays_files = '%s ${ROOT_DIR}/%s/kernel/configs/%s' % (kernel_defconfig_overlays_files, kernel_dir, name)
        all_defconfig = '${ROOT_DIR}/%s/%s/%s %s' % (kernel_dir, defconfig_dir, project_defconfig_name, kernel_defconfig_overlays_files)
    if mode_config:
        all_defconfig = '%s ${ROOT_DIR}/%s/kernel/configs/%s' % (all_defconfig, kernel_dir, mode_config)
    pre_defconfig_cmds = '%s && mkdir -p ${OUT_DIR} && cd ${OUT_DIR} && bash ${ROOT_DIR}/${KERNEL_DIR}/scripts/kconfig/merge_config.sh -m .config %s && cd ${ROOT_DIR}' % (pre_defconfig_cmds, all_defconfig)
    pre_defconfig_cmds = 'PRE_DEFCONFIG_CMDS=\'if [ -f ${OUT_DIR}/.config ]; then cp -f -p ${OUT_DIR}/.config ${OUT_DIR}/.config.timestamp; else touch ${OUT_DIR}/.config.timestamp; fi; %s\'' % (pre_defconfig_cmds)
    file_text.append(pre_defconfig_cmds)

    file_text.append("POST_DEFCONFIG_CMDS='if ! cmp -s ${OUT_DIR}/.config.timestamp ${OUT_DIR}/.config; then rm -f ${OUT_DIR}/.config.timestamp; else mv -f ${OUT_DIR}/.config.timestamp ${OUT_DIR}/.config; fi'\n")

    ext_modules_list = ''
    ext_modules_file = '%s/kernel/configs/ext_modules.list' % (abs_kernel_dir)
    if os.path.exists(ext_modules_file):
        file_handle = open(ext_modules_file, 'r')
        for line in file_handle.readlines():
            line_strip = line.strip()
            ext_modules_list = '%s %s' % (ext_modules_list, line_strip)
        file_handle.close()
    ext_modules_list = 'EXT_MODULES_TMP=\"%s %s\"' % (ext_modules_list.strip(), ext_modules.strip())
    file_text.append(ext_modules_list)
    file_text.append("# deal with EXT_MODULES")
    file_text.append("EXT_MODULES=${DEVICE_MODULES_DIR}")
    file_text.append("for dir in ${EXT_MODULES_TMP}")
    file_text.append("do")
    file_text.append("  if [ -d \"${dir}\" ]; then")
    file_text.append("    EXT_MODULES=\"${EXT_MODULES} ${dir}\"")
    file_text.append("  else")
    file_text.append("    echo \"WARNING: remove ${dir} from EXT_MODULES, as this dir does not exist!\"")
    file_text.append("  fi")
    file_text.append("done")
    file_text.append("# use internal repo if it exists")
    file_text.append("EXT_MODULES_TMP=${EXT_MODULES}")
    file_text.append("for dir in ${EXT_MODULES_TMP}")
    file_text.append("do")
    file_text.append("  if [[ \"${dir}\" == ../vendor/mediatek/kernel_modules/cpufreq_cus ]] || [[ \"${dir}\" == ../vendor/mediatek/kernel_modules/cpufreq_int ]]; then")
    file_text.append("    continue")
    file_text.append("  fi")
    file_text.append("  if [[ \"${dir}\" == *_int ]]; then")
    file_text.append("    dir_cus=`echo ${dir} | sed -e 's#_int#_cus#g'`")
    file_text.append("  else")
    file_text.append("    dir_cus=${dir}_cus")
    file_text.append("  fi")
    file_text.append("  if [[ \"${EXT_MODULES}\" =~ \"${dir_cus}\" ]]; then")
    file_text.append("    EXT_MODULES=`echo ${EXT_MODULES} | sed \"s#${dir_cus}##g\"`")
    file_text.append("    echo \"WARNING: at internal, use ${dir} instead of ${dir_cus}.\"")
    file_text.append("  fi")
    file_text.append("done")
    file_text.append("EXT_MODULES=${EXT_MODULES}")
    file_text.append("echo \"EXT_MODULES=${EXT_MODULES}\"")

    file_text.append("DIST_CMDS='cp -p ${OUT_DIR}/.config ${DIST_DIR}'\n")

    gen_build_config_gki_goals = '%s.gki_goals' % (gen_build_config)
    file_handle = open(gen_build_config_gki_goals, 'w')
    file_handle.write('MAKE_GOALS=\"PAHOLE_FLAGS=\"--btf_gen_floats\" ${MAKE_GOALS} Image.lz4 Image.gz\"')
    file_handle.close()
    gki_build_config_fragments = '  GKI_BUILD_CONFIG_FRAGMENTS=%s' % (gen_build_config_gki_goals)

    file_text.append("if [ \"x${PROJECT_DEFCONFIG_NAME}\" != \"xgki_defconfig\" ]; then")
    file_text.append("  GKI_BUILD_CONFIG=${KERNEL_DIR}/build.config.gki.aarch64.vendor")
    file_text.append(gki_build_config_fragments)
    file_text.append("fi")
    file_text.append("GKI_PATH=${ROOT_DIR}/../prebuilts/perl/linux-x86/bin:${ROOT_DIR}/build/kernel/build-tools/path/linux-x86:/usr/bin:/bin")
    file_text.append("GKI_KCONFIG_EXT_PREFIX=${KCONFIG_EXT_PREFIX}")
    file_text.append("GKI_VENDOR_DEFCONFIG=${DEFCONFIG}")
    file_text.append("GKI_PRE_DEFCONFIG_CMDS=${PRE_DEFCONFIG_CMDS}")
    file_text.append("GKI_POST_DEFCONFIG_CMDS=${GKI_POST_DEFCONFIG_CMDS}")
    file_text.append("if [ -d \"${ROOT_DIR}/../vendor/mediatek/internal\" ]; then")
    file_text.append("  MGK_INTERNAL=true")
    file_text.append("fi")

    gen_build_config_mtk = '%s.mtk' % (gen_build_config)
    file_handle = open(gen_build_config_mtk, 'w')
    for line in file_text:
        file_handle.write(line + '\n')
    file_handle.close()

    gki_build_config = '%s/build.config.gki.aarch64' % (abs_kernel_dir)
    gen_build_config_gki = '%s/build.config.gki.aarch64' % (gen_build_config_dir)
    file_text = []
    if os.path.exists(gki_build_config):
        file_handle = open(gki_build_config, 'r')
        for line in file_handle.readlines():
            line_strip = line.strip()
            pattern_source = re.compile('^\.\s.+$')
            result = pattern_source.match(line_strip)
            if not result:
                file_text.append(line_strip)
        file_handle.close()
    file_handle = open(gen_build_config_gki, 'w')
    for line in file_text:
        file_handle.write(line + '\n')
    file_handle.close()

    file_handle = open(gen_build_config, 'w')
    kernel_dir_line = 'KERNEL_DIR=%s' % (kernel_dir)
    file_handle.write(kernel_dir_line + '\n')
    build_config_fragments = 'BUILD_CONFIG_FRAGMENTS="${KERNEL_DIR}/%s"' % (os.path.basename(device_modules_build_config))
    file_handle.write(build_config_fragments + '\n\n')
    rel_gen_build_config_dir = 'REL_GEN_BUILD_CONFIG_DIR=`./${KERNEL_DIR}/scripts/get_rel_path.sh %s ${ROOT_DIR}`' % (gen_build_config_dir)
    file_handle.write(rel_gen_build_config_dir + '\n')
    kernel_build_mode_cmds = 'KERNEL_BUILD_MODE=%s' % (build_mode)
    file_handle.write(kernel_build_mode_cmds + '\n')
    kernel_project_defconfig = 'PROJECT_DEFCONFIG_NAME=%s' % (project_defconfig_name)
    file_handle.write(kernel_project_defconfig + '\n')
    rel_project_defconfig = '%s/%s/%s' % (kernel_dir, defconfig_dir, project_defconfig_name)
    kernel_defconfig_cmds = '  KERNEL_DEFCONFIG_FILE=%s' % (rel_project_defconfig)
    file_handle.write('if [ "x${PROJECT_DEFCONFIG_NAME}" != "xgki_defconfig" ]; then\n')
    file_handle.write(kernel_defconfig_cmds + '\n')
    file_handle.write('  GET_CONFIG_ABI_MONITOR=`grep \"^CONFIG_ABI_MONITOR\s*=\s*y\" ${KERNEL_DEFCONFIG_FILE} | xargs`\n')
    file_handle.write('  if [ "${KERNEL_BUILD_MODE}" == "user" ] && [ "x${GET_CONFIG_ABI_MONITOR}" != "x" ]; then\n')
    file_handle.write('    DO_ABI_MONITOR=1\n')
    build_config_fragments = '    BUILD_CONFIG_FRAGMENTS="${BUILD_CONFIG_FRAGMENTS} ${REL_GEN_BUILD_CONFIG_DIR}/%s' % (os.path.basename(gen_build_config_gki))
    file_handle.write(build_config_fragments + '"\n')
    file_handle.write('  fi\n')
    file_handle.write('fi\n')

    build_config_fragments = 'BUILD_CONFIG_FRAGMENTS="${BUILD_CONFIG_FRAGMENTS} ${REL_GEN_BUILD_CONFIG_DIR}/%s' % (os.path.basename(gen_build_config_mtk))
    file_handle.write(build_config_fragments + '"\n')
    file_handle.write('if [ "x${PROJECT_DEFCONFIG_NAME}" != "xgki_defconfig" ]; then\n')
    file_handle.write('  if [ "x${GKI_BUILD_CONFIG}" == "x" ] && [ "x${GKI_PREBUILTS_DIR}" == "x" ]; then\n')
    file_handle.write('    if [ "x${ENABLE_GKI_CHECKER}" == "xtrue" ] || [ -d "${ROOT_DIR}/../vendor/mediatek/internal" ] && [ "${KERNEL_BUILD_MODE}" == "user" ]; then\n')
    file_handle.write('      BUILD_CONFIG_FRAGMENTS="${BUILD_CONFIG_FRAGMENTS} ${KERNEL_DIR}/build.config.mtk.check_gki"\n')
    file_handle.write('    fi\n')
    file_handle.write('  fi\n')
    file_handle.write('fi\n')
    if kernel_build_config_overlays:
        build_config_fragments = 'BUILD_CONFIG_FRAGMENTS="${BUILD_CONFIG_FRAGMENTS} %s"' % (kernel_build_config_overlays)
        file_handle.write(build_config_fragments + '\n')
    file_handle.close()

if __name__ == '__main__':
    parser = ArgumentParser(formatter_class=ArgumentDefaultsHelpFormatter)

    parser.add_argument("-p","--project", dest="project", help="specify the project to build kernel.", default="")
    parser.add_argument("--kernel-defconfig", dest="kernel_defconfig", help="special kernel project defconfig file.",default="")
    parser.add_argument("--kernel-defconfig-overlays", dest="kernel_defconfig_overlays", help="special kernel project overlay defconfig files.",default="")
    parser.add_argument("--kernel-build-config-overlays", dest="kernel_build_config_overlays", help="special kernel build config overlays files.",default="")
    parser.add_argument("-m","--build-mode", dest="build_mode", help="specify the build mode to build kernel.", default="user")
    parser.add_argument("--abi", dest="abi_mode", help="specify whether build.config is used to check ABI.", default="no")
    parser.add_argument("-o","--out-file", dest="out_file", help="specify the generated build.config file.", required=True)

    args = parser.parse_args()
    main(**args.__dict__)
