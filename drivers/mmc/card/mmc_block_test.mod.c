#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

MODULE_INFO(intree, "Y");

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x7c02cfe8, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xfa799e30, __VMLINUX_SYMBOL_STR(test_iosched_unregister) },
	{ 0x33d5d501, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xf0372292, __VMLINUX_SYMBOL_STR(test_iosched_register) },
	{ 0x275ef902, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0xa2e9338a, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xb15599d2, __VMLINUX_SYMBOL_STR(mmc_blk_get_packed_statistics) },
	{ 0xf7802486, __VMLINUX_SYMBOL_STR(__aeabi_uidivmod) },
	{ 0x37e74642, __VMLINUX_SYMBOL_STR(get_jiffies_64) },
	{ 0xff178f6, __VMLINUX_SYMBOL_STR(__aeabi_idivmod) },
	{ 0x7c2b655, __VMLINUX_SYMBOL_STR(mmc_blk_init_bkops_statistics) },
	{ 0xd85cd67e, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0x6fe7bb3, __VMLINUX_SYMBOL_STR(test_iosched_add_unique_test_req) },
	{ 0x38490187, __VMLINUX_SYMBOL_STR(check_test_completion) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xe6e31e3, __VMLINUX_SYMBOL_STR(__blk_put_request) },
	{ 0x2536b17a, __VMLINUX_SYMBOL_STR(test_get_test_data) },
	{ 0x8d6a5a89, __VMLINUX_SYMBOL_STR(debugfs_create_file) },
	{ 0xae2e7398, __VMLINUX_SYMBOL_STR(debugfs_create_u32) },
	{ 0xdf80d73c, __VMLINUX_SYMBOL_STR(test_iosched_get_debugfs_tests_root) },
	{ 0xb320185c, __VMLINUX_SYMBOL_STR(test_iosched_get_debugfs_utils_root) },
	{ 0x12da5bb2, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0x54ac93a6, __VMLINUX_SYMBOL_STR(debugfs_remove) },
	{ 0x76aa5377, __VMLINUX_SYMBOL_STR(test_iosched_set_test_result) },
	{ 0x65043b59, __VMLINUX_SYMBOL_STR(test_iosched_set_ignore_round) },
	{ 0xc8b57c27, __VMLINUX_SYMBOL_STR(autoremove_wake_function) },
	{ 0x1cfb04fa, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0x1000e51, __VMLINUX_SYMBOL_STR(schedule) },
	{ 0x499cb58c, __VMLINUX_SYMBOL_STR(prepare_to_wait) },
	{ 0x5f754e5a, __VMLINUX_SYMBOL_STR(memset) },
	{ 0x5fc56a46, __VMLINUX_SYMBOL_STR(_raw_spin_unlock) },
	{ 0x9c0bd51f, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0x1d678958, __VMLINUX_SYMBOL_STR(test_iosched_get_req_queue) },
	{ 0x17cdac8c, __VMLINUX_SYMBOL_STR(test_iosched_add_wr_rd_test_req) },
	{ 0x4cdb3178, __VMLINUX_SYMBOL_STR(ns_to_timeval) },
	{ 0xe707d823, __VMLINUX_SYMBOL_STR(__aeabi_uidiv) },
	{ 0x2196324, __VMLINUX_SYMBOL_STR(__aeabi_idiv) },
	{ 0xd9ce8f0c, __VMLINUX_SYMBOL_STR(strnlen) },
	{ 0xb81960ca, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0x29c57f9f, __VMLINUX_SYMBOL_STR(mmc_blk_init_packed_statistics) },
	{ 0x53035116, __VMLINUX_SYMBOL_STR(test_iosched_mark_test_completion) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0x3148825a, __VMLINUX_SYMBOL_STR(__blk_run_queue) },
	{ 0x1a1431fd, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irq) },
	{ 0x3507a132, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irq) },
	{ 0x9e14baba, __VMLINUX_SYMBOL_STR(test_iosched_create_test_req) },
	{ 0xbb7a3ff6, __VMLINUX_SYMBOL_STR(test_iosched_start_test) },
	{ 0xfa2a45e, __VMLINUX_SYMBOL_STR(__memzero) },
	{ 0x20c55ae0, __VMLINUX_SYMBOL_STR(sscanf) },
	{ 0x2e5810c6, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr1) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xefd6cf06, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr0) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=test-iosched";

