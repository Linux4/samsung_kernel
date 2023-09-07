#!/usr/bin/env python
#
# Copyright (C) year MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.

import sys
import struct

boot_img_size = 2048

def parse_dtb(input):
	dtb_list = []
	with open(input, 'rb') as f:
		kernel_data = f.read()
		kernel_size = f.tell()
		kernel_length = kernel_size
		dtb_offset = kernel_size - 8
		while dtb_offset > 0:
			dtb_magic = struct.unpack('>I', kernel_data[dtb_offset : dtb_offset+4])[0]
			if dtb_magic == 0xD00DFEED:
				dtb_size = struct.unpack('>I', kernel_data[dtb_offset+4 : dtb_offset+8])[0]
				if dtb_offset + dtb_size == kernel_length:
					kernel_length = kernel_length - dtb_size
					dtb_list.append(dtb_offset)
			dtb_offset = dtb_offset - 1
	return dtb_list

def write_header(output, dtb_list):
	with open(output, 'ab+') as f:
		magic1 = struct.pack('I', 0xD11DFEED)
		f.write(magic1)
		for offset in dtb_list:
			offset_set = struct.pack('I', boot_img_size + offset)
			f.write(offset_set)
		count_str = struct.pack('I', len(dtb_list))
		f.write(count_str)
		magic2 = struct.pack('I', 0xD22DFEED)
		f.write(magic2)

def main(argv):
	if len(argv) < 2:
		print("Usage: python post_process_dtbs.py zImage-dtb")
		sys.exit(1)
	input_img = argv[1]
	output_img = argv[1]
	dtb_list = parse_dtb(input_img)
	write_header(output_img, dtb_list)

if __name__ == '__main__':
	main(sys.argv)
