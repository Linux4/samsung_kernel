/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include <utils/Log.h>
#include "cpu_core_info.h"
using namespace std;

#define CPU_INFO_PATH "proc/device-tree/cpus/"
#define MAX_CORES 16
#define ERR_CODE -1

int fetched_cpu_info;
int number_of_cores, number_of_clusters;
int cores[MAX_CORES];
int core_cluster_map[MAX_CORES];
map<int,vector<int>> cluster_core_map;

int init() {
    if (fetched_cpu_info == 0) {
        if (get_cpu_info()) {
            return ERR_CODE;
        }
        fetched_cpu_info = 1;
    }
    return 0;
}

int get_number_of_clusters() {
    if(init()) {
        return ERR_CODE;
    }
    return number_of_clusters;
}

int get_number_of_cores() {
    if(init()) {
        return ERR_CODE;
    }
    return number_of_cores;
}

int get_cluster(int core) {
    if(init()) {
        return ERR_CODE;
    }
    if (core < 0 || core > number_of_cores - 1) {
        ALOGE("Core %d does not exist", core);
        return ERR_CODE;
    }
    return core_cluster_map[core];
}

uint32_t get_cores() {
    uint32_t core_info = 0;
    if(init()) {
        return ERR_CODE;
    }
    int i;
    for (i = 0; i < number_of_cores; i++) {
        core_info = (!cores[i] ? (core_info | (1 << i)) : core_info);
    }
    return core_info;
}

int get_cores_within_cluster(int cluster, int *cores) {
    if(init()) {
        return ERR_CODE;
    }
    vector<int> cores_within_cluster;
    if( cluster_core_map.find(cluster) != cluster_core_map.end() ) {
        cores_within_cluster = cluster_core_map[cluster];
    } else {
        ALOGI("Cluster %d not found", cluster);
        return ERR_CODE;
    }
    int i;
    for(i = 0; i < cores_within_cluster.size(); i++) {
        cores[i] = cores_within_cluster.at(i);
    }
    return i;
}

int get_cpu_info(){
    string cpu_cluster_map_path(CPU_INFO_PATH);
    cpu_cluster_map_path.append("cpu-map/");
    DIR *cpus = opendir(CPU_INFO_PATH), *cpu_map= opendir(cpu_cluster_map_path.c_str());
    struct dirent *cpus_entry, *cpu_map_entry, *cpu_cluster_entry;
    fstream fin;
    string line;
    if (cpus == NULL) {
        ALOGE("Could not open directory: %s" CPU_INFO_PATH);
        return ERR_CODE;
    } else {
        int core = 0;
        while((cpus_entry = readdir(cpus))) {
            string filename = cpus_entry -> d_name;
            if (filename.rfind("cpu@") == 0) {
                number_of_cores++;
                int core = filename.at(filename.find('@')+1) - '0';
                string s(CPU_INFO_PATH);
                s.append(filename).append("/enable-method");
                fin.open(s, ios::in);
                if(!fin) {
                    ALOGE("File not found");
                    return ERR_CODE;
                }
                while(getline(fin, line)) {
                    if(line.substr(0,4) == "none") {
                        cores[core] = 1;
                    }
                }
                fin.close();
            }
            if (filename.compare("cpu-map") == 0) {
                vector<string> cluster_dname;
                if (cpu_map == NULL) {
                    ALOGE("Could not open directory: %s", cpu_cluster_map_path.c_str());
                    return ERR_CODE;
                } else {
                    while((cpu_map_entry = readdir(cpu_map))) {
                        string dir_name = cpu_map_entry -> d_name;
                        if (dir_name.rfind("cluster") == 0) {
                            number_of_clusters++;
                            cluster_dname.push_back(dir_name);
                        }
                    }
                    sort(cluster_dname.begin(), cluster_dname.end());
                    for(string dir_name : cluster_dname) {
                        int cluster = dir_name.back() - '0';
                        string s(cpu_cluster_map_path);
                        s.append(dir_name);
                        DIR *cpu_cluster_map = opendir(s.c_str());
                        if (cpu_cluster_map == NULL) {
                            ALOGE("Could not open directory: %s", s.c_str());
                            return ERR_CODE;
                        } else {
                            while((cpu_cluster_entry = readdir(cpu_cluster_map))) {
                                string cluster_dir_name = cpu_cluster_entry -> d_name;
                                if (cluster_dir_name.rfind("core") == 0) {
                                    core_cluster_map[core] = cluster;
                                    core++;
                                }
                            }
                        }
                    }
                }
            }
        }
        if (core != number_of_cores) {
            ALOGE("Inconsistent entries detected");
            return ERR_CODE;
        }
    }
    for(int i = 0; i < number_of_cores; i++) {
        cluster_core_map[core_cluster_map[i]].push_back(i);
    }
    return 0;
}
