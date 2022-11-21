# Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 and
# only version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.


from parser_util import register_parser, RamParser
from print_out import print_out_str
from mm import get_vmemmap, page_buddy


def print_reserved_mem(ramdump):
    reserved_mem_addr = ramdump.address_of('reserved_mem')
    output_file = ramdump.open_file("reserved_mem.txt")
    reserved_mem_count = ramdump.read_int('reserved_mem_count')
    size_of_reserved_mem = ramdump.sizeof('struct reserved_mem')
    output_file.write("reserved_mem {\n")
    str = "name = 0x{0:x} '{1}'\n\t\tbase = 0x{2:x}\n\t\tsize = 0x{" \
          "3:x} ({4})KB\n\t\trange = <0x{5:x} - 0x{6:x}>\n\t"
    index = 0
    while index < reserved_mem_count:
        reserved_mem = reserved_mem_addr + index*size_of_reserved_mem
        name_addr = ramdump.read_structure_field(
                            reserved_mem, 'struct reserved_mem', 'name')
        name = ramdump.read_cstring(name_addr, 48)
        base = ramdump.read_structure_field(
                            reserved_mem, 'struct reserved_mem', 'base')
        size = ramdump.read_structure_field(
                            reserved_mem, 'struct reserved_mem', 'size')
        output_file.write("\t{\n\t\t")
        output_file.write(str.format(name_addr, name, base, size, size // 1024,
                                     base, base+size))
        output_file.write("}\n")
        index = index + 1
    output_file.write("\n}")
    output_file.close()


def page_trace(ramdump, pfn):
    if ramdump.is_config_defined("CONFIG_SPARSEMEM"):
        mem_section = ramdump.read_word('mem_section')
        if ramdump.kernel_version >= (4, 14):
            mem_section = ramdump.read_word(mem_section)
    else:
        mem_section = ramdump.address_of('contig_page_data')
    nr_entries_offset = 0
    trace_entries_offset = 0
    offset = 0
    struct_holding_trace_entries = 0
    trace_entry_size = ramdump.sizeof("unsigned long")
    if ramdump.is_config_defined('CONFIG_SPARSEMEM'):
        page_ext_offset = ramdump.field_offset(
                            'struct mem_section', 'page_ext')
    else:
        page_ext_offset = ramdump.field_offset(
                            'struct pglist_data', 'node_page_ext')
    if ramdump.is_config_defined('CONFIG_STACKDEPOT'):
        trace_entries_offset = ramdump.field_offset(
                            'struct stack_record', 'entries')
    else:
        trace_entries_offset = ramdump.field_offset(
                            'struct page_ext', 'trace_entries')
    nr_entries_offset = ramdump.field_offset(
                            'struct page_ext', 'nr_entries')
    if ramdump.is_config_defined('CONFIG_SPARSEMEM'):
        mem_section_size = ramdump.sizeof('struct mem_section')
    else:
        mem_section_size = 0
    page_ext_size = ramdump.sizeof('struct page_ext')
    if ramdump.kernel_version >= (4, 9):
        page_owner_size = ramdump.sizeof('struct page_owner')
        page_ext_size = page_ext_size + page_owner_size
        page_owner_ops_offset = ramdump.read_structure_field(
                'page_owner_ops', 'struct page_ext_operations', 'offset')
    phys = pfn << 12
    if phys == None or phys == 0:
        return

    if ramdump.is_config_defined('CONFIG_MEMORY_HOTPLUG'):
        section_size_bits = int(ramdump.get_config_val(
                                'CONFIG_HOTPLUG_SIZE_BITS'))
        offset = phys >> section_size_bits
    else:
        offset = phys >> 30

    if ramdump.is_config_defined("CONFIG_SPARSEMEM"):
        mem_section_0_offset = (mem_section + (offset * mem_section_size))
        page_ext = ramdump.read_word(
                        mem_section_0_offset + page_ext_offset)
    else:
        page_ext = ramdump.read_word(mem_section + page_ext_offset)
    if ramdump.arm64:
        temp_page_ext = page_ext + (pfn * page_ext_size)
    else:
        pfn_index = pfn - (ramdump.phys_offset >> 12)
        temp_page_ext = page_ext + (pfn_index * page_ext_size)

    temp_page_ext = temp_page_ext + page_owner_ops_offset
    if not ramdump.is_config_defined('CONFIG_STACKDEPOT'):
        nr_trace_entries = ramdump.read_int(
                            temp_page_ext + nr_entries_offset)
        struct_holding_trace_entries = temp_page_ext
    else:
        handle = ramdump.read_structure_field(
                    temp_page_ext, 'struct page_owner', 'handle')

        slabindex = handle & 0x1fffff
        handle_offset = (handle >> 0x15) & 0x3ff
        handle_offset = handle_offset << 4
        stack_slab = ramdump.address_of('stack_slabs')
        stack_slab_size = ramdump.sizeof('void *')
        slab = ramdump.read_word(
                    stack_slab + (stack_slab_size * slabindex))
        stack = slab + handle_offset
        nr_trace_entries = ramdump.read_structure_field(
                            stack, 'struct stack_record', 'size')
        struct_holding_trace_entries = stack
    if nr_trace_entries <= 0 or nr_trace_entries > 16:
        return
    alloc_str = ''
    for i in range(0, nr_trace_entries):
        addr = ramdump.read_word(
                struct_holding_trace_entries + trace_entries_offset +
                i*trace_entry_size)
        if addr == 0:
            break
        look = ramdump.unwind_lookup(addr)
        if look is None:
            break
        symname, offset = look
        unwind_dat = '      [<{0:x}>] {1}+0x{2:x}\n'.format(
                                addr, symname, offset)
        alloc_str = alloc_str + unwind_dat
    return alloc_str


def parse_pfn(ramdump, pfn, cma, op_file):

    vmemmap = get_vmemmap(ramdump)
    page_size = ramdump.sizeof('struct page')
    page = vmemmap + pfn*page_size
    str = "{0} pfn : 0x{1:x} page : 0x{2:x} flag : 0x{3:x} mapping : 0x{" \
          "4:x} count : {5} _mapcount : {6:x} {7}\n{8}\n"
    str1 = "{0} pfn : 0x{1:x}--0x{2:x} head_page : 0x{3:x} flag : {4:x} " \
           "mapping : 0x{5:x} count : {6} _mapcount : {7:x} {8}\n{9}\n"
    str2 = "{0} pfn : 0x{1:x} pge : 0x{2:x} count : {3} _mapcount : " \
           "{4:x} {5}\n"
    str3 = "{0} pfn : 0x{1:x}--0x{2:x} head_page : 0x{3:x} count : {4} " \
           "_mapcount : {5:x}  {6}\n"
    page_flags = ramdump.read_structure_field(page, 'struct page', 'flags')
    tail_page = ramdump.read_structure_field(
                            page, 'struct page', 'compound_head')
    if (tail_page & 1) == 1:
        page = tail_page-1
    nr_pages = 1
    page_count = ramdump.read_structure_field(
                            page, 'struct page', '_refcount.counter')
    mapcount_offset = ramdump.field_offset('struct page', '_mapcount')
    page_mapcount = ramdump.read_int(page + mapcount_offset)

    if page_mapcount == 0xffffffff:
        page_mapcount = -1
    page_mapping = ramdump.read_structure_field(page, 'struct page', 'mapping')
    is_pinned_str = ""
    if (page_mapcount >= 0)and ((page_count - page_mapcount) >= 2):
        is_pinned_str = "<===pinned"
    if cma == 1:
        cma_usage = "[devm]"
    else:
        cma_usage = "[ncma]"
        # test if buddy
        if page_mapcount == 0xffffff80:
            cma_usage = "[budd]"
            nr_pages = ramdump.read_structure_field(
                            page, 'struct page', 'private')
            nr_pages = 1 << nr_pages
        elif page_mapping != 0:
            anon_page = page_mapping & 0x1
            if anon_page != 0:
                cma_usage = "[anon]"
            else:
                cma_usage = "[file]"
        else:
            cma_usage = "[unkw]"

    if ramdump.is_config_defined('CONFIG_PAGE_OWNER'):
        if (page_buddy(ramdump, page)) or page_count == 0:
            function_list = ""
        else:
            function_list = page_trace(ramdump, pfn)
        if nr_pages == 1:
            op_file.write(str.format(
                cma_usage, pfn, page, page_flags, page_mapping, page_count,
                page_mapcount, is_pinned_str, function_list))
        else:
            op_file.write(str1.format(cma_usage, pfn, pfn+nr_pages-1,
                                      page, page_flags, page_mapping,
                                      page_count, page_mapcount,
                                      is_pinned_str, function_list))
    else:
        if nr_pages == 1:
            op_file.write(str2.format(cma_usage, pfn, page, page_count,
                                      page_mapcount, is_pinned_str))
        else:
            op_file.write(str3.format(
                cma_usage, pfn, pfn+nr_pages-1, page, page_count,
                page_mapcount, is_pinned_str))
    return nr_pages


def cma_region_dump(ramdump, cma, cma_name):
    base_pfn = ramdump.read_structure_field(
                        cma, 'struct cma', 'base_pfn')
    cma_count = ramdump.read_structure_field(
                        cma, 'struct cma', 'count')
    bitmap = ramdump.read_structure_field(
                        cma, 'struct cma', 'bitmap')
    bitmap_end = bitmap + cma_count // 8
    in_system = 1
    end_pfn = base_pfn + cma_count
    name = "cma_report_" + cma_name + ".txt"
    op_file = ramdump.open_file(name)
    op_file.write("CMA report\n")
    op_file.write(" - name : {0}\n".format(cma_name))
    op_file.write(" - base_pfn\t\t\t: 0x{0:x}\n".format(base_pfn))
    op_file.write(" - end_pfn\t\t\t: 0x{0:x}\n".format(end_pfn))
    op_file.write(" - count\t\t\t: 0x{0:x}\n".format(cma_count))
    op_file.write(" - size\t\t\t\t: {0}KB\n".format(cma_count << 0x2))
    op_file.write(" - bitmap_start\t\t: 0x{0:x}\n".format(bitmap))
    op_file.write(" - bitmap_end\t\t: 0x{0:x}\n".format(bitmap_end))
    op_file.write(" - in_system\t\t: {0}\n\n".format(in_system))

    byte_index = 0
    PFNS_PER_BYTE = 8
    COUNT_TO_BYTE = cma_count // PFNS_PER_BYTE

    while byte_index < COUNT_TO_BYTE:
        pfn_index = 0
        pfn = 0
        while byte_index < COUNT_TO_BYTE:
            value = ramdump.read_byte(bitmap+byte_index)
            byte_to_advance = 1
            while pfn_index < PFNS_PER_BYTE:
                pfn = base_pfn + byte_index*PFNS_PER_BYTE + pfn_index
                bit_value = (value >> pfn_index) & 0x1
                cma = 0
                if bit_value != 0:
                    cma = 1
                else:
                    cma = 0
                pfn_to_avance = 1
                pfn_to_avance = parse_pfn(ramdump, pfn, cma, op_file)
                pfn_index = pfn_index+pfn_to_avance
                if pfn_index >= PFNS_PER_BYTE:
                    byte_to_advance = pfn_index // PFNS_PER_BYTE
                    pfn_index = pfn_index % 8
                    byte_index = byte_index + byte_to_advance
                    if byte_index >= COUNT_TO_BYTE:
                        break
            byte_index = byte_index+1
    op_file.close()


def print_cma_areas(ramdump):
    output_file = ramdump.open_file("cma_report_simple.txt")
    cma_area_count = ramdump.read_u32('cma_area_count')
    cma_area_base_addr = ramdump.address_of('cma_areas')
    cma_index = 0
    size_of_cma_area = ramdump.sizeof('struct cma')
    str = "cma : 0x{0:x} cma_base_pfn : 0x{1:x} size : 0x{2:x} pages ({3}KB)\n"
    str1 = "name : {0}\n\n"
    cma = [0] * cma_area_count
    cma_name = [None] * cma_area_count

    while cma_index < cma_area_count:
        cma_area = cma_area_base_addr + cma_index*size_of_cma_area
        base_pfn = ramdump.read_structure_field(
                                cma_area, 'struct cma', 'base_pfn')
        cma_size = ramdump.read_structure_field(
                                cma_area, 'struct cma', 'count')
        name_addr = ramdump.read_structure_field(
                                cma_area, 'struct cma', 'name')
        name = ramdump.read_cstring(name_addr, 48)
        if name == "linux,cma":
            name = "dma_contiguous_default_area"
        cma[cma_index] = cma_area
        cma_name[cma_index] = name
        output_file.write(str.format(cma_area, base_pfn, cma_size, cma_size*4))
        output_file.write(str1.format(name))
        cma_index = cma_index + 1

    output_file.close()

    cma_index = 0
    while cma_index < cma_area_count:
        cma1 = cma[cma_index]
        cma_name1 = cma_name[cma_index]
        cma_region_dump(ramdump, cma1, cma_name1)
        cma_index = cma_index + 1


def print_tasklet_info(ramdump, core, tasklet):
    print_out_str("Pending Tasklet info for {0}:".format(tasklet))
    tasklet_vec_addr = ramdump.address_of(tasklet)
    tasklet_head = tasklet_vec_addr + ramdump.per_cpu_offset(core)
    tasklet_head = ramdump.read_word(tasklet_head)
    next_offset = ramdump.field_offset('struct tasklet_struct', 'next')
    func_offset = ramdump.field_offset('struct tasklet_struct', 'func')
    while (tasklet_head != 0x0):
        tasklet_func_addr = ramdump.read_word(tasklet_head + func_offset)
        tasklet_func = ramdump.unwind_lookup(tasklet_func_addr)
        if tasklet_func is None:
            tasklet_func = "Dynamic module/symbol not found"
        print_out_str("{0:x} -> {1}".format(tasklet_func_addr, tasklet_func))
        tasklet_head = ramdump.read_word(tasklet_head+next_offset)


def parse_softirq_stat(ramdump):
    irq_stat_addr = ramdump.address_of('irq_stat')
    softirq_name_addr = ramdump.address_of('softirq_to_name')
    sizeof_softirq_name = ramdump.sizeof('softirq_to_name')
    sofrirq_name_arr_size = sizeof_softirq_name // ramdump.sizeof('char *')
    no_of_cpus = ramdump.get_num_cpus()
    index = 0
    size_of_irq_stat = ramdump.sizeof('irq_cpustat_t')
    while index < no_of_cpus:
        if ramdump.kernel_version >= (4, 19):
            irq_stat = irq_stat_addr + ramdump.per_cpu_offset(index)
        else:
            irq_stat = irq_stat_addr + index*size_of_irq_stat
        softirq_pending = ramdump.read_structure_field(
                                irq_stat, 'irq_cpustat_t', '__softirq_pending')
        pending = ""
        pos = sofrirq_name_arr_size - 1
        while pos:
            if softirq_pending & (1 << pos):
                flag_addr = ramdump.read_word(ramdump.array_index(
                    softirq_name_addr, "char *", pos))
                flag = ramdump.read_cstring(flag_addr, 48)
                pending += flag
                pending += " | "
            pos = pos - 1
        if pending == "":
            pending = "None"
        print_out_str("core {0} : __softirq_pending = {1}".format(
                                index, pending))
        if "TASKLET" in pending:
            print_tasklet_info(ramdump, index, 'tasklet_vec')
            print_tasklet_info(ramdump, index, 'tasklet_hi_vec')
        index = index + 1


def do_parse_qsee_log(ramdump):
    qsee_out = ramdump.open_file('qsee_log.txt')
    g_qsee_log_addr = ramdump.address_of('g_qsee_log')
    g_qsee_log_v2_addr = ramdump.address_of('g_qsee_log_v2')
    tzdbg_addr = ramdump.address_of('tzdbg')
    is_enlarged_buf_addr = ramdump.field_offset('struct tzdbg', 'is_enlarged_buf')
    is_enlarged_buf = False
    if is_enlarged_buf_addr is not None:
        is_enlarged_buf = ramdump.read_bool(tzdbg_addr + is_enlarged_buf_addr)
    log_buf_offset = ramdump.field_offset('struct tzdbg_log_t', 'log_buf')
    qsee_log_buf_size = 0x8000 ##define QSEE_LOG_BUF_SIZE 0x8000
    if is_enlarged_buf is True:
        qsee_log_buf_size = 0x20000 ##define QSEE_LOG_BUF_SIZE_V2 0x20000
        g_qsee_log_addr = g_qsee_log_v2_addr
        log_buf_offset = ramdump.field_offset('struct tzdbg_log_v2_t', 'log_buf')
    if g_qsee_log_addr is None:
        print_out_str("!!! g_qsee_logs not found")
        qsee_out.close()
        return
    try:
        g_qsee_log_addr = ramdump.read_word(g_qsee_log_addr)
    except Exception as e:
        print_out_str('!!! Cannot read g_qsee_log_addr')
        qsee_out.close()
        return
    log_buf_addr = g_qsee_log_addr + log_buf_offset
    qsee_log_data = ramdump.read_cstring(log_buf_addr, qsee_log_buf_size)
    qsee_log_data = qsee_log_data.rstrip(' \t\r\n\0')
    qsee_out.write(qsee_log_data)
    qsee_out.close()


@register_parser('--print-reserved-mem', 'Print reserved memory info ')
class ReservedMem(RamParser):

    def parse(self):
        print_reserved_mem(self.ramdump)


@register_parser('--print-cma-areas', 'Print cma memory region info ')
class CmaAreas(RamParser):

    def parse(self):
        if self.ramdump.kernel_version < (4, 9):
            print_out_str("Linux version lower than 4.9 is not supported!!")
            return
        else:
            print_cma_areas(self.ramdump)


@register_parser('--print-softirq-stat', 'Print softirq pending info ')
class SoftirqStat(RamParser):

    def parse(self):
        parse_softirq_stat(self.ramdump)


@register_parser('--print-qsee-log', 'Extract qsee com logs')
class ParseQseeLog(RamParser):
    def parse(self):
        do_parse_qsee_log(self.ramdump)
