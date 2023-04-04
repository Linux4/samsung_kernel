# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

from parser_util import register_parser, RamParser, cleanupString

def extract_va_minidump(ramdump):
    kva_out = ramdump.open_file('kva_output.txt')
    sde_enc_addr = ramdump.get_section_address("sde_enc_phys_wb")[0][0]
    frame_count = ramdump.read_structure_field(sde_enc_addr, 'struct sde_encoder_phys_wb', 'frame_count')
    kickoff_count = ramdump.read_structure_field(sde_enc_addr, 'struct sde_encoder_phys_wb', 'kickoff_count')
    kva_out.write("sde_enc_phys_wb :\n")
    kva_out.write("\tframe_count : {}\n".format(frame_count))
    kva_out.write("\tkickoff_count : {}\n".format(kickoff_count))

    kva_out.write("\n\nmsm_drm_priv :\n")
    msm_drm_priv_addr = ramdump.get_section_address("msm_drm_priv")[0][0]
    registered_offset = ramdump.field_offset('struct msm_drm_private', 'registered')
    registered = ramdump.read_bool(msm_drm_priv_addr + registered_offset)
    num_planes = ramdump.read_structure_field(msm_drm_priv_addr, 'struct msm_drm_private', 'num_planes')
    wait_type_offset = ramdump.field_offset('struct msm_drm_private', 'pp_event_worker.lock.dep_map.wait_type_inner')
    cpu_offset = ramdump.field_offset('struct msm_drm_private', 'pp_event_worker.lock.dep_map.cpu')
    wait_type = ramdump.read_byte(msm_drm_priv_addr + wait_type_offset)
    cpu = ramdump.read_int(msm_drm_priv_addr + cpu_offset)
    kva_out.write("\tregistered : {}\n".format(registered))
    kva_out.write("\tnum_planes : {}\n".format(num_planes))
    kva_out.write("\tpp_event_worker->lock->dep_map->wait_type : {}\n".format(wait_type))
    kva_out.write("\tpp_event_worker->lock->dep_map->cpu : {}\n".format(cpu))

@register_parser('--vm-minidump-sample', 'Print reserved memory info ')
class VaMinidump(RamParser):

    def parse(self):
        extract_va_minidump(self.ramdump)


