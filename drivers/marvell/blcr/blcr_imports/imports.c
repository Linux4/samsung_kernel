
#ifndef EXPORT_SYMTAB
  #define EXPORT_SYMTAB
#endif

#include "blcr_config.h"
#include "blcr_imports.h"
#include "blcr_ksyms.h"

_CR_IMPORT_KCODE(__kuser_cmpxchg, 0xc078b100)
_CR_IMPORT_KCODE(__kuser_helper_start, 0xc078b0a0)
_CR_IMPORT_KCODE(arch_pick_mmap_layout, 0xc011bdc8)
_CR_IMPORT_KCODE(arch_unmap_area, 0xc01bbe3c)
_CR_IMPORT_KCODE(attach_pid, 0xc0150e64)
_CR_IMPORT_KCODE(change_pid, 0xc0150eac)
_CR_IMPORT_KCODE(copy_fs_struct, 0xc01f7430)
_CR_IMPORT_KCODE(copy_siginfo_to_user, 0xc0148970)
_CR_IMPORT_KCODE(detach_pid, 0xc0150ea4)
_CR_IMPORT_KCODE(do_pipe_flags, 0xc01da0cc)
_CR_IMPORT_KCODE(do_sigaction, 0xc01491b4)
_CR_IMPORT_KCODE(do_sigaltstack, 0xc01493b0)
_CR_IMPORT_KCODE(expand_files, 0xc01e8a5c)
_CR_IMPORT_KCODE(find_task_by_pid_ns, 0xc0150f34)
_CR_IMPORT_KCODE(free_fs_struct, 0xc01f7384)
_CR_IMPORT_KCODE(free_pid, 0xc01509c4)
_CR_IMPORT_KCODE(get_dumpable, 0xc01d82b4)
_CR_IMPORT_KCODE(group_send_sig_info, 0xc0147880)
_CR_IMPORT_KCODE(groups_search, 0xc015aa90)
_CR_IMPORT_KCODE(pipe_fcntl, 0xc01da27c)
_CR_IMPORT_KCODE(set_dumpable, 0xc01d8074)
_CR_IMPORT_KCODE(set_fs_pwd, 0xc01f7178)
_CR_IMPORT_KCODE(sys_dup2, 0xc01df390)
_CR_IMPORT_KCODE(sys_ftruncate, 0xc01d0b00)
_CR_IMPORT_KCODE(sys_link, 0xc01ded44)
_CR_IMPORT_KCODE(sys_lseek, 0xc01d1978)
_CR_IMPORT_KCODE(sys_mprotect, 0xc01bec04)
_CR_IMPORT_KCODE(sys_mremap, 0xc01bf714)
_CR_IMPORT_KCODE(sys_munmap, 0xc01bd1f4)
_CR_IMPORT_KCODE(sys_prctl, 0xc014ba08)
_CR_IMPORT_KCODE(sys_setitimer, 0xc013c094)
_CR_IMPORT_KDATA(anon_pipe_buf_ops, 0xc05ca2c0)
_CR_IMPORT_KDATA(pid_hash, 0xc0861e80)
_CR_IMPORT_KDATA(pidhash_shift, 0xc07f7798)
_CR_IMPORT_KDATA(ramfs_file_operations, 0xc05d4240)
_CR_IMPORT_KDATA(shmem_file_operations, 0xc05c76c0)
_CR_IMPORT_KDATA(suid_dumpable, 0xc089fea0)
_CR_IMPORT_KDATA(tasklist_lock, 0xc07de040)
/* END AUTO-GENERATED FRAGMENT */

const char *blcr_config_timestamp = BLCR_CONFIG_TIMESTAMP;
EXPORT_SYMBOL_GPL(blcr_config_timestamp);
