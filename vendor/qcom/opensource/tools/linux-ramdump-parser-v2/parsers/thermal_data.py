# Copyright (c) 2015, 2020-2021 The Linux Foundation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 and
# only version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

from print_out import print_out_str
from parser_util import register_parser, RamParser
import linux_list as llist

TSENS_MAX_SENSORS = 16
DEBUG_SIZE = 10

@register_parser(
    '--thermal-info', 'Useful information from thermal data structures')
class Thermal_info(RamParser):

    def print_thermal_info(
            self, sensor_dbg_info_start_address, ram_dump,
            time_stamp, sensor_mapping):
        for ncpu in ram_dump.iter_cpus():
            self.output_file.write(
                "------------------------------------------------\n")
            self.output_file.write(
                " TEMPERATURE ENTRIES FOR CPU:{0} \n".format(
                    int(ncpu)))
            self.output_file.write(
                "------------------------------------------------\n")
            cpu_sensor_addr = sensor_dbg_info_start_address + \
                sensor_mapping[ncpu]
            for i in range(0, 10):
                temp = self.ramdump.read_word(cpu_sensor_addr + (i * 8), True)
                time = self.ramdump.read_word(
                    cpu_sensor_addr + time_stamp + (i * 8), True)
                self.output_file.write(
                    "Temperature reading -  {0} ".format(int(temp)))
                self.output_file.write("TimeStamp - {0}\n".format(int(time)))

    def tmdev_data(self, ram_dump):
        sensor_mapping = []
        self.output_file.write("Thermal sensor data \n")

        tmdev = self.ramdump.address_of('tmdev')
        tmdev_address = self.ramdump.read_word(tmdev, True)
        sensor_dbg_info_size = ram_dump.sizeof('struct tsens_sensor_dbg_info')
        sensor_dbg_info = self.ramdump.field_offset(
            'struct tsens_tm_device',
            'sensor_dbg_info')
        time_stamp = self.ramdump.field_offset(
            'struct tsens_sensor_dbg_info',
            'time_stmp')
        cpus_sensor = self.ramdump.address_of('cpus')
        cpus_sensor_size = ram_dump.sizeof('struct cpu_info')
        sensor_id_offset = self.ramdump.field_offset(
            'struct cpu_info',
            'sensor_id')

        if not all((tmdev, sensor_dbg_info_size, sensor_dbg_info,
                    time_stamp, cpus_sensor, cpus_sensor_size,
                    sensor_id_offset)):
            self.output_file.write("Not supported for this target yet  :-( \n")
            return

        for i in ram_dump.iter_cpus():
            cpu_sensor_id_address = cpus_sensor + sensor_id_offset
            sensor_id = self.ramdump.read_u32(cpu_sensor_id_address, True)
            cpus_sensor = cpus_sensor + cpus_sensor_size
            sensor_mapping.append((sensor_id - 1) * sensor_dbg_info_size)

        self.print_thermal_info(
            (tmdev_address + sensor_dbg_info),
            ram_dump,
            time_stamp,
            sensor_mapping)

    def list_func(self, tsens_device_p):
        dev_o = self.ramdump.field_offset('struct tsens_device', 'dev')
        dev = self.ramdump.read_word(dev_o + tsens_device_p)
        kobj_o = self.ramdump.field_offset('struct device', ' kobj')
        kobj = (dev + kobj_o)
        name_o = self.ramdump.field_offset('struct kobject', 'name')
        name_addr = self.ramdump.read_word(name_o + kobj)
        name = self.ramdump.read_cstring(name_addr)
        if name is not None:
            print("%s" % (name), file=self.output_file)
            tsens_dbg_o = self.ramdump.field_offset('struct tsens_device', 'tsens_dbg')
            tsens_dbg = tsens_device_p + tsens_dbg_o
            sensor_dbg_info_o = self.ramdump.field_offset('struct tsens_dbg_context', 'sensor_dbg_info')
            sensor_dbg_info = sensor_dbg_info_o  + tsens_dbg
            self.output_file.write('v.v (struct tsens_device)0x{:8x} 0x{:8x}\n'.format(tsens_device_p, sensor_dbg_info))
            for i in range(0, TSENS_MAX_SENSORS):
                idx = self.ramdump.read_u32(self.ramdump.array_index(sensor_dbg_info, 'struct tsens_dbg', i))
                tsens_dbg_addr = self.ramdump.array_index(sensor_dbg_info, 'struct tsens_dbg', i)
                print ("    idx: %d tsens_dbg_addr 0x%x" %(idx, tsens_dbg_addr), file=self.output_file)
                time_stmp_o = self.ramdump.field_offset('struct tsens_dbg', 'time_stmp')
                temp_o = self.ramdump.field_offset('struct tsens_dbg', 'temp')
                print("             time_stmp       temp ", file=self.output_file)
                for j in range(0, DEBUG_SIZE):
                    time_stmp = self.ramdump.read_word(self.ramdump.array_index(time_stmp_o + tsens_dbg_addr, 'unsigned long long', j))
                    temp = self.ramdump.read_u64(
                    self.ramdump.array_index(temp_o + tsens_dbg_addr, 'unsigned long', j))
                    print("             %d   %d" % (time_stmp, temp), file=self.output_file)


    def get_thermal_info(self):
        tsens_device_list = self.ramdump.address_of('tsens_device_list')
        list_offset = self.ramdump.field_offset('struct tsens_device', 'list')
        list_walker = llist.ListWalker(self.ramdump, tsens_device_list, list_offset)
        list_walker.walk(tsens_device_list, self.list_func)

    def parse(self):
        self.output_file = self.ramdump.open_file('thermal_info.txt')
        if self.ramdump.kernel_version >= (5, 4):
            self.get_thermal_info()
        else:
            self.tmdev_data(self.ramdump)

        self.output_file.close()
        print_out_str("--- Wrote the output to thermal_info.txt")
