#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Module Utils contains Utils class with general purpose helper functions.
"""

import struct
import os
from itertools import chain

__author__ = "Vadym Stupakov"
__copyright__ = "Copyright (c) 2017 Samsung Electronics"
__credits__ = ["Vadym Stupakov"]
__version__ = "1.0"
__maintainer__ = "Vadym Stupakov"
__email__ = "v.stupakov@samsung.com"
__status__ = "Production"


class Utils:
    """
    Utils class with general purpose helper functions.
    """
    @staticmethod
    def flatten(alist):
        """
        Make list from sub lists
        :param alist: any list: [[item1, item2], [item3, item4], ..., [itemN, itemN+1]]
        :return: [item1, item2, item3, item4, ..., itemN, itemN+1]
        """
        if alist is []:
            return []
        elif type(alist) is not list:
            return [alist]
        else:
            return [el for el in chain.from_iterable(alist)]

    @staticmethod
    def pairwise(iterable):
        """
        Iter over two elements: [s0, s1, s2, s3, ..., sN] -> (s0, s1), (s2, s3), ..., (sN, sN+1)
        :param iterable:
        :return: (s0, s1), (s2, s3), ..., (sN, sN+1)
        """
        a = iter(iterable)
        return zip(a, a)

    @staticmethod
    def paths_exists(path_list):
        """
        Check if path exist, otherwise raise FileNotFoundError exception
        :param path_list: list of paths
        """
        for path in path_list:
            if not os.path.exists(path):
                raise FileNotFoundError("File: \"" + path + "\" doesn't exist!\n")

    @staticmethod
    def to_int(value, base=16):
        """
        Converts string to int
        :param value: string or int
        :param base: string base int
        :return: integer value
        """
        if isinstance(value, int):
            return value
        elif isinstance(value, str):
            return int(value.strip(), base)

    def to_bytearray(self, value):
        """
        Converts list to bytearray with block size 8 byte
        :param value: list of integers or bytearray or int
        :return: bytes
        """
        if isinstance(value, bytearray) or isinstance(value, bytes):
            return value
        elif isinstance(value, list):
            value = self.flatten(value)
            return struct.pack("%sQ" % len(value), *value)
        elif isinstance(value, int):
            return struct.pack("Q", value)

    @staticmethod
    def human_size(nbytes):
        """
        Print in human readable
        :param nbytes: number of bytes
        :return: human readable string. For instance: 0x26a5d (154.6 K)
        """
        raw = nbytes
        suffixes = ("B", "K", "M")
        i = 0
        while nbytes >= 1024 and i < len(suffixes) - 1:
            nbytes /= 1024.
            i += 1
        f = "{:.1f}".format(nbytes).rstrip("0").rstrip(".")
        return "{} ({} {})".format(hex(raw), f, suffixes[i])

    @staticmethod
    def signed_int32_to_num(int32_value):
        """
        Converts int32 branch length from BL x opcode to signed value
        :param int32_value: general 32bit integer value 
        :return signed integer value
        """
        if int32_value & 0x80000000:
            return -1 * (((~int32_value) & 0x7fffffff) + 1)
        else:
            return int32_value

    def extract_bl_len(self, opcode):
        """
        Extracts branch length from BL x opcode 
        :param opcode: 32bit opcode
        :return python integer value

        aarch64: 'BL <label>' is 1001 01XX XXXX XXXX XXXX XXXX XXXX XXXX   // 0x94000000
        <label> the program label to be unconditionally branched to. 
        It`s offset from the address of this instruction, in the range +/-128MB,is encoded as "imm26" times 4.  
        """
        instruction_mask = 0xfc000000

        if opcode & 0x02000000:
            # neg branch
            opcode_bl_word_len = opcode | instruction_mask
        else:
            opcode_bl_word_len = opcode & ~instruction_mask
            
        return 4 * self.signed_int32_to_num(opcode_bl_word_len)

    def is_bl(self, opcode):
        """
        Check if the opcode is BL
        :param opcode: 32bit opcode
        :return true or false
        
        aarch64: 'BL #X' is 1001 01XX XXXX XXXX XXXX XXXX XXXX XXXX // 0x94000000
        """
        if (opcode & 0xfc000000) == 0x94000000:
            return True
        else:
            return False