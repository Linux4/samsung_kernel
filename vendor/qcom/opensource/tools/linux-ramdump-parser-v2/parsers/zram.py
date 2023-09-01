#SPDX-License-Identifier: GPL-2.0-only
#Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.

import linux_radix_tree
from mm import page_address
from mm import pfn_to_page
import os
from parser_util import register_parser, RamParser, cleanupString
from lzo1xlib import Lzo1xParser
from print_out import print_out_str
import struct
import traceback

@register_parser('--print-zram', 'Extract data from zram')
class Zram(RamParser):

    OBJ_TAG_BITS    = 1
    BITS_PER_LONG   = 64

    ZSPAGE_MAGIC   = 0x58
    FULLNESS_BITS  = 2
    CLASS_BITS	   = 8
    ISOLATED_BITS  = 3
    MAGIC_VAL_BITS = 8

    CRYPTO_MAX_ALG_NAME       = 128
    MAX_POSSIBLE_PHYSMEM_BITS = 48

    CRYPTO_LZO     = "lzo"
    CRYPTO_LZO_RLE = "lzo-rle"
    def __init__(self, *args):
        super(Zram, self).__init__(*args)

        # struct zram
        self.zram_mem_pool_oft = self.ramdump.field_offset('struct zram', 'mem_pool')
        self.zram_table_oft    = self.ramdump.field_offset('struct zram', 'table')
        self.zram_comp_oft     = self.ramdump.field_offset('struct zram', 'comp')
        self.zram_disksize_oft = self.ramdump.field_offset('struct zram', 'disksize')
        self.zram_compressor_oft = self.ramdump.field_offset('struct zram', 'compressor')

        # struct zram_table_entry
        self.zram_table_entry_flags_oft  = self.ramdump.field_offset('struct zram_table_entry', 'flags')
        self.zram_table_entry_handle_oft = self.ramdump.field_offset('struct zram_table_entry', 'handle')
        self.sizeof_zram_table_entry     = self.ramdump.sizeof("struct zram_table_entry")

        # struct page
        self.page_private_oft  = self.ramdump.field_offset('struct page', 'private')
        self.page_freelist_oft = self.ramdump.field_offset('struct page', 'freelist')
        if not self.page_freelist_oft:
            self.page_freelist_oft = self.ramdump.field_offset('struct page', 'index')
        self.zspage_magic_oft  = self.ramdump.field_offset('struct zspage', 'magic')

        # struct size_class
        self.size_class_size_oft                = self.ramdump.field_offset('struct size_class', 'size')
        self.size_class_objs_per_zspage_offset  = self.ramdump.field_offset('struct size_class', 'objs_per_zspage')
        self.size_class_pages_per_zspage_offset = self.ramdump.field_offset('struct size_class', 'pages_per_zspage')
        self.zs_pool_size_class_oft             = self.ramdump.field_offset('struct zs_pool', 'size_class')
        self.lzo_parser = Lzo1xParser()
        self.init_config()

    def init_config(self):
        if self.ramdump.is_config_defined('CONFIG_ARM64_PAGE_SHIFT'):
            self.PAGE_SHIFT = int(self.ramdump.get_config_val('CONFIG_ARM64_PAGE_SHIFT'))
        else:
            self.PAGE_SHIFT = 12

        if (self.ramdump.kernel_version) < (6, 1, 0):
            self.__SWP_TYPE_SHIFT = 2
            self.__SWP_TYPE_BITS = 6
            self.ZRAM_FLAG_SHIFT = 24
            self.HUGE_BITS = 0
        else:
            self.__SWP_TYPE_SHIFT = 3
            self.__SWP_TYPE_BITS = 5
            self.ZRAM_FLAG_SHIFT = self.PAGE_SHIFT + 1
            self.HUGE_BITS = 1

        self.__SWP_OFFSET_BITS = 50
        self.__SWP_TYPE_MASK = ((1 << self.__SWP_TYPE_BITS) - 1)
        self.__SWP_OFFSET_SHIFT = (self.__SWP_TYPE_BITS + self.__SWP_TYPE_SHIFT)
        self.__SWP_OFFSET_MASK = ((1 << self.__SWP_OFFSET_BITS) - 1)


        self.ZRAM_SAME = self.ZRAM_FLAG_SHIFT + 1
        self.ZRAM_WB = self.ZRAM_FLAG_SHIFT + 2
        self.sizeof_long = self.ramdump.sizeof("unsigned long")
        self.ZS_HANDLE_SIZE = self.sizeof_long

        self.PAGE_SIZE = 1 << self.PAGE_SHIFT
        self._PFN_BITS = (self.MAX_POSSIBLE_PHYSMEM_BITS - self.PAGE_SHIFT)
        self.OBJ_INDEX_BITS = (self.BITS_PER_LONG - self._PFN_BITS - self.OBJ_TAG_BITS) # 64-36 -1 = 27
        self.OBJ_INDEX_MASK = (1 << self.OBJ_INDEX_BITS) -1

    def read_binary(self, addr, length):
        """Reads binary data of specified length from addr_or_name."""
        min = 0
        msg = b''
        size = length
        while length > 0:
            addr = addr + min
            # msg located in the same page
            if length < (0x1000 - addr % 0x1000):
                min = length
            # msg separated in two pages
            else:
                min = 0x1000 - addr % 0x1000
            length = length - min
            addr_phys = self.ramdump.virt_to_phys(addr)
            msg_binary = self.ramdump.read_physical(addr_phys, min)
            if msg_binary is None or msg_binary == '':
                return msg
            msg = msg + msg_binary
        return msg

    def save_to_file(self, zram_addr, zdata, index):
        with self.ramdump.open_file(os.path.join("zram", \
                "zram_%x_index_0x%x.bin" % (zram_addr, index)), 'wb') as out_file:
            out_file.write(self.zdata)

    def process_zram(self, zram_addr, index, need_save_to_file=True):
        table_addr = self.ramdump.read_word(zram_addr +  self.zram_table_oft)
        table_entry_addr = self.ramdump.array_index(table_addr, "struct zram_table_entry", index)
        entry_flags = self.ramdump.read_word(table_entry_addr + self.zram_table_entry_flags_oft)
        handle_addr = self.ramdump.read_word(table_entry_addr + self.zram_table_entry_handle_oft)
        if handle_addr is None or entry_flags is None :
            return False
        elif handle_addr == 0:
            self.zdata  = bytearray(self.PAGE_SIZE)
            return True
        elif (entry_flags & (1<< self.ZRAM_SAME)) != 0 :
            element = handle_addr
            self.zdata  = bytearray(self.PAGE_SIZE)
            e = struct.unpack("<Q", struct.pack(">Q", element))[0]
            e_s = "{0:016x}".format(e)
            e_a = bytearray.fromhex(e_s)
            idx = 0
            while idx < self.PAGE_SIZE:
                self.zdata[idx : idx + self.sizeof_long] = e_a[0 : self.sizeof_long]
                idx += self.sizeof_long
            if need_save_to_file:
                self.save_to_file(zram_addr, self.zdata, index)
            #print_out_str("Zram: parse success due to ZRAM_SAME, element=0x%x, index=0x%x" % (element, index))
            return True
        elif (entry_flags & (1<< self.ZRAM_WB)) != 0 :
            self.zdata = None
            #print_out_str("Zram: parse failed due to ZRAM_WB not support, index=0x%x" % index)
            return True

        handle = self.ramdump.read_word(handle_addr)
        size = entry_flags & ((1 << self.ZRAM_FLAG_SHIFT) - 1)
        mem_pool_addr = self.ramdump.read_word(zram_addr + self.zram_mem_pool_oft)
        data = self.zs_map_object(mem_pool_addr, handle);
        if (size == self.PAGE_SIZE):
            sl = len(data)
            if sl > self.PAGE_SIZE:
                sl = self.PAGE_SIZE
            self.zdata = bytearray(self.PAGE_SIZE)
            idx = 0
            while idx < sl:
                self.zdata[idx] = data[idx]
                idx += 1
            if need_save_to_file:
                self.save_to_file(zram_addr, self.zdata, index)
            #print_out_str("Zram: parse success due to size == self.PAGE_SIZE index=0x%x" % index)
        else:
            if data[0] != 17 or (data[1] != 1 and data[1] !=0):
                raise Exception("invalid lzo-rel header")
            oudata = bytearray(self.PAGE_SIZE)
            try:
                self.lzo_parser.lzo1x_decompress_safe(data[0 : size], size, oudata, self.PAGE_SIZE)
            except Exception as e:
                if self.lzo_parser.error != self.lzo_parser.LZO_E_OK:
                    traceback.print_exc()
                    print_out_str(traceback.format_exc())
                    raise Exception("lzo1x_decompress_safe error happen!! error=%s, index=0x%x" \
                        % (self.lzo_parser.error_to_str(self.lzo_parser.error), index))
                else:
                    #print_out_str("Zram: parse success due to lzo1x_decompress_safe, decompressed data len=0x%x, index=0x%x"\
                    #        % (self.lzo_parser.ou_len, index))
                    self.zdata = self.lzo_parser.oudata[0 : self.lzo_parser.ou_len]
                    if need_save_to_file:
                        self.save_to_file(zram_addr, self.zdata, index)
        return True

    def zs_map_object(self, mem_pool_addr, handle):
        pfn = handle >> (self.OBJ_TAG_BITS + self.OBJ_INDEX_BITS)
        page_addr = pfn_to_page(self.ramdump, pfn)
        page_private_addr = self.ramdump.read_word(page_addr + self.page_private_oft)
        magic_int = self.ramdump.read_u32(page_private_addr + self.zspage_magic_oft)
        magic = (magic_int >> (self.HUGE_BITS + self.FULLNESS_BITS + self.CLASS_BITS + 1 + self.ISOLATED_BITS)) \
                         & ((1 << self.MAGIC_VAL_BITS) -1)
        if magic != self.ZSPAGE_MAGIC:
            print_out_str("Zram: Page is not a zram page")
            raise Exception("(struct page*)=0x%x (struct page*)->private=0x%x is not a zram page, \
                        magic=0x%x expect magic=0x%x)" % \
                        (page_addr, page_private_addr, magic, self.ZSPAGE_MAGIC))

        class_idx = (magic_int  >> (self.HUGE_BITS + self.FULLNESS_BITS)) & ((1<<(self.CLASS_BITS+1)) -1)
        size_class_addr = mem_pool_addr + self.zs_pool_size_class_oft
        class_addr = self.ramdump.read_word(self.ramdump.array_index( \
                    size_class_addr, "struct size_class *", class_idx))
        size = self.ramdump.read_s32(class_addr +  self.size_class_size_oft)

        obj_idx = (handle >> self.OBJ_TAG_BITS) & self.OBJ_INDEX_MASK
        off = (size * obj_idx) & (self.PAGE_SIZE -1)
        vm_addr = page_address(self.ramdump, page_addr) + off
        data = b''
        if off + size <= self.PAGE_SIZE:
            data = self.read_binary(vm_addr, size)
        else:
            page0_size = self.PAGE_SIZE - off
            data = self.read_binary(vm_addr, page0_size)
            page_freelist_addr = self.ramdump.read_word(page_addr + self.page_freelist_oft)
            page1_vm_addr = page_address(self.ramdump, page_freelist_addr)
            data += self.read_binary(page1_vm_addr, size - page0_size)
        # is huge page?
        objs_per_zspage = self.ramdump.read_s32(class_addr +  self.size_class_objs_per_zspage_offset)
        pages_per_zspage = self.ramdump.read_s32(class_addr +  self.size_class_pages_per_zspage_offset)
        is_huge_page = (objs_per_zspage == 1) and (pages_per_zspage == 1)
        if is_huge_page:
            return data
        else:
            return data[self.ZS_HANDLE_SIZE:len(data)]

    def read_data(self, addr, pte):
        try:
            if addr:
                page_offset = addr & 0xFFF
                type = ((pte >> self.__SWP_TYPE_SHIFT) & self.__SWP_TYPE_MASK)
                offset = (pte >> self.__SWP_OFFSET_SHIFT) & self.__SWP_OFFSET_MASK
                if type == 0x0:
                    self.zdata = None
                    zram_index_idr = self.ramdump.address_of('zram_index_idr')
                    zram_dev_rtw = linux_radix_tree.RadixTreeWalker(self.ramdump)
                    zram_dev_rtw.walk_radix_tree(zram_index_idr,
                                    self.process_zram, offset, False)
                    if self.zdata != None:
                        return self.zdata[page_offset : len(self.zdata)]
        except:
            pass
        return None


    def zram_exact(self, zram_addr):
        compressor = cleanupString(self.ramdump.read_cstring( \
                    zram_addr + self.zram_compressor_oft, self.CRYPTO_MAX_ALG_NAME))
        if compressor !=  self.CRYPTO_LZO and compressor != self.CRYPTO_LZO_RLE:
            print_out_str("Zram: not support compressoFr %s" % compressor)
            return

        disk_size = self.ramdump.read_word(zram_addr + self.zram_disksize_oft)
        disk_size_pages = disk_size >> 12
        print_out_str("zram disk_size %d Mb, compressor %s" % (disk_size/1024/1024, compressor))

        index = 0
        while index < disk_size_pages:
            try:
                ret = self.process_zram(zram_addr, index * self.sizeof_zram_table_entry)
                if ret is False:
                    break
            except Exception as e:
                traceback.print_exc()
                print_out_str(traceback.format_exc())
            index += 1

    def parse(self):
        if self.ramdump.kernel_version >= (5, 4):
            zram_index_idr = self.ramdump.address_of('zram_index_idr')
            zram_dev_rtw = linux_radix_tree.RadixTreeWalker(self.ramdump)
            try:
                zram_dev_rtw.walk_radix_tree(zram_index_idr,
                                self.zram_exact)
            except Exception as e:
                traceback.print_exc()
                print_out_str(traceback.format_exc())
