#!/usr/bin/python
# -*- coding: utf-8 -*-
#Copyright (c) 2021 The Linux Foundation. All rights reserved.
#
#Redistribution and use in source and binary forms, with or without
#modification, are permitted provided that the following conditions are
#met:
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
#      copyright notice, this list of conditions and the following
#      disclaimer in the documentation and/or other materials provided
#      with the distribution.
#    * Neither the name of The Linux Foundation nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#
#THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
#WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
#MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
#ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
#BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
#BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
#OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
#IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os,json,sys
import subprocess
from xml.etree import ElementTree as et

module_info_dict = {}
git_project_dict = {}
out_path = os.getenv("OUT")
croot = os.getenv("ANDROID_BUILD_TOP")
violated_modules = []
git_repository_list = []
whitelist_projects_list = []
qssi_install_keywords = ["system","system_ext","product"]
vendor_install_keywords = ["vendor"]
violation_file_path = out_path
aidl_metadata_file = croot + "/out/soong/.intermediates/system/tools/aidl/build/aidl_metadata_json/aidl_metadata.json"
noship_project_marker = os.path.join(croot,os.getenv("QCPATH"),"interfaces-noship")
aidl_metadata_dict = {}

def parse_xml_file(path):
    xml_element = None
    if os.path.isfile(path):
        try:
            xml_element = et.parse(path).getroot()
        except Exception as e:
            print("Exiting!! Xml Parsing Failed : " + path)
            sys.exit(1)
    else:
        print("Exiting!! File not Present : " + path)
        sys.exit(1)
    return xml_element

def load_json_file(path):
    json_dict = {}
    if os.path.isfile(path):
        json_file_handler = open(path,'r')
        try:
            json_dict = json.load(json_file_handler)
        except Exception as e:
            print("Exiting!! Json Loading Failed : " + path)
            sys.exit(1)
    else:
        print("Exiting!! File not Present : " + path)
        sys.exit(1)
    return json_dict

def check_if_module_contributing_to_qssi_or_vendor(install_path):
    qssi_path = False
    vendor_path = False
    installed_image = install_path.split(out_path.split(croot+"/")[1] + "/")[1]
    for qssi_keyword in qssi_install_keywords:
        if installed_image.startswith(qssi_keyword):
            qssi_path = True
            break
    if qssi_path:
        return {"qssi_path":qssi_path, "vendor_path":vendor_path}
    else:
        for vendor_keyword in vendor_install_keywords:
            if installed_image.startswith(vendor_keyword):
                vendor_path = True
                break
    return {"qssi_path":qssi_path, "vendor_path":vendor_path}

def find_and_update_git_project_path(path):
     for git_repository in git_repository_list:
         if git_repository in path:
             return git_repository
         else:
             return path

def filter_whitelist_projects(violation_list):
    filtered_project_violation_list = []
    for violator in violation_list:
        if not ignore_whitelist_projects(violator):
            filtered_project_violation_list.append(violator)
    return filtered_project_violation_list

def print_violations_to_file(violation_list,qssi_path_project_list,vendor_path_project_list):
    violation_list = filter_whitelist_projects(violation_list)
    if violation_list:
        ## Open file to write Violation list
        violation_file_handler = open(violation_file_path + "/commonsys-intf-violator.txt", "w")
        violation_file_handler.write("############ Violation List ###########\n\n")
        for violator in violation_list:
            qssi_module_list =  qssi_path_project_list[violator]
            vendor_module_list = vendor_path_project_list[violator]
            violation_file_handler.writelines("Git Project : " + str(violator)+"\n")
            violation_file_handler.writelines("QSSI Violations \n")
            for qssi_module in qssi_module_list:
                violation_file_handler.writelines(str(qssi_module) + ",")
            violation_file_handler.writelines("\nVendor Violations \n")
            for vendor_module in vendor_module_list:
                violation_file_handler.writelines(str(vendor_module)  + ",")
            violation_file_handler.writelines("\n################################################# \n\n")
        violation_file_handler.close()
        if commonsys_intf_enforcement:
            print("Commonsys-Intf Violation found !! Exiting Compilation !!")
            print("For details execute : cat $OUT/configs/commonsys-intf-violator.txt")
            sys.exit(-1)

def check_for_hidl_aidl_intermediate_libs(module_name,class_type):
    if "@" in module_name or "-ndk" in module_name:
        if class_type == "EXECUTABLES" or "-impl" in module_name:
            return False
        else:
            return True
    else:
        ## Refer aidl_metadata database
        for aidl_info in aidl_metadata_dict:
            if not aidl_info is None:
                aidl_module_name = aidl_info["name"]
                if aidl_module_name in module_name :
                    if class_type == "JAVA_LIBRARIES" or class_type == "SHARED_LIBRARIES":
                        return True
        return False

def ignore_whitelist_projects(project_name):
    for project in whitelist_projects_list :
        if project == project_name:
            return True
    return False

def find_commonsys_intf_project_paths():
    qssi_install_keywords = ["system","system_ext","product"]
    vendor_install_keywords = ["vendor"]
    path_keyword = "vendor"
    qssi_path_project_list={}
    vendor_path_project_list={}
    violation_list = {}
    for module in module_info_dict:
        try:
            install_path = module_info_dict[module]['installed'][0]
            project_path = module_info_dict[module]['path'][0]
            class_type   = module_info_dict[module]['class'][0]
        except IndexError:
            continue

        if project_path is None or install_path is None or class_type is None:
            continue

        relative_out_path = out_path.split(croot + "/")[1]
        ## Ignore host and other paths
        if not relative_out_path in install_path:
            continue
        ## We are interested in only source paths which are
        ## starting with vendor for now.
        aidl_hidl_lib = check_for_hidl_aidl_intermediate_libs(module,class_type)
        if project_path.startswith(path_keyword) and not aidl_hidl_lib:
            qssi_or_vendor = check_if_module_contributing_to_qssi_or_vendor(install_path)
            if not qssi_or_vendor["qssi_path"] and not qssi_or_vendor["vendor_path"]:
                continue

            project_path = find_and_update_git_project_path(project_path)
            if qssi_or_vendor["qssi_path"]:
                install_path_list =  []
                if project_path in qssi_path_project_list:
                    install_path_list = qssi_path_project_list[project_path]
                install_path_list.append(install_path)
                qssi_path_project_list[project_path] = install_path_list
                 ## Check if path is present in vendor list as well , if yes then it will be a violation
                if project_path in vendor_path_project_list:
                    violation_list[project_path] = install_path
                continue

            if qssi_or_vendor["vendor_path"]:
                install_path_list =  []
                if project_path in vendor_path_project_list:
                    install_path_list = vendor_path_project_list[project_path]
                 #if not install_path in install_path_list:
                install_path_list.append(install_path)
                vendor_path_project_list[project_path] = install_path_list
                vendor_path = True
                ## Check if path is present in qssi list as well , if yes then it will be a violation
                if project_path in qssi_path_project_list:
                    violation_list[project_path] = install_path
    print_violations_to_file(violation_list,qssi_path_project_list,vendor_path_project_list)

def start_commonsys_intf_checker():
    global module_info_dict
    global git_repository_list
    global violation_file_path
    global whitelist_projects_list
    global aidl_metadata_dict
    script_dir = os.path.dirname(os.path.realpath(__file__))
    if os.path.exists(violation_file_path + "/configs"):
        violation_file_path = violation_file_path + "/configs"
    module_info_dict = load_json_file(out_path + "/module-info.json")
    whitelist_projects_list = load_json_file(script_dir + "/whitelist_commonsys_intf_project.json")
    manifest_root = parse_xml_file(croot + "/.repo/manifest.xml")
    aidl_metadata_dict = load_json_file(aidl_metadata_file)
    for project in manifest_root.findall("project"):
        git_project_path = project.attrib.get('path')
        if not git_project_path == None:
            git_repository_list.append(git_project_path)
    find_commonsys_intf_project_paths()

def read_enforcement_value_from_mkfile():
    global commonsys_intf_enforcement
    script_dir = os.path.dirname(os.path.realpath(__file__))
    configs_enforcement_mk_path = os.path.join(script_dir,"configs_enforcement.mk")
    if os.path.exists(configs_enforcement_mk_path):
        configs_enforcement = open(configs_enforcement_mk_path,"r")
        mkfile_lines = configs_enforcement.readlines()
        for line in mkfile_lines:
            enforcement_flag = (line.split(":=")[0]).strip()
            enforcement_value = (line.split(":=")[1]).strip()
            if enforcement_flag == "PRODUCT_ENFORCE_COMMONSYSINTF_CHECKER":
                if enforcement_value == "true":
                    commonsys_intf_enforcement = True
                elif enforcement_value == "false":
                    commonsys_intf_enforcement = False
                else:
                    print("Unrecongnized Enforcement flag option : " + str(enforcement_value))
                    sys.exit(-1)
                break
    else:
        print("configs_enforcement.mk fime missing. Exiting!!")
        sys.exit(-1)

def main():
    if os.path.exists(noship_project_marker):
        read_enforcement_value_from_mkfile()
        start_commonsys_intf_checker()
        print("Commonsys-Intf Script Executed Successfully!!")
    else:
        print("Skipping Commonsys-Intf Checker!!")

if __name__ == '__main__':
    main()
