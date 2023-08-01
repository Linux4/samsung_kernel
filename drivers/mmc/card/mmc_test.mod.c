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
	{ 0xe0903c2c, __VMLINUX_SYMBOL_STR(single_release) },
	{ 0xfb5b91c1, __VMLINUX_SYMBOL_STR(seq_read) },
	{ 0xf5a2d050, __VMLINUX_SYMBOL_STR(seq_lseek) },
	{ 0x2894e5f9, __VMLINUX_SYMBOL_STR(mmc_unregister_driver) },
	{ 0x764d97e4, __VMLINUX_SYMBOL_STR(mmc_register_driver) },
	{ 0x1424f59, __VMLINUX_SYMBOL_STR(sg_copy_to_buffer) },
	{ 0x8371daff, __VMLINUX_SYMBOL_STR(sg_copy_from_buffer) },
	{ 0xefdd2345, __VMLINUX_SYMBOL_STR(sg_init_one) },
	{ 0x12da5bb2, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0x27fa66e1, __VMLINUX_SYMBOL_STR(nr_free_buffer_pages) },
	{ 0x2b66a3a5, __VMLINUX_SYMBOL_STR(mem_map) },
	{ 0xd5152710, __VMLINUX_SYMBOL_STR(sg_next) },
	{ 0x2a3ca1a0, __VMLINUX_SYMBOL_STR(page_address) },
	{ 0xf88c3301, __VMLINUX_SYMBOL_STR(sg_init_table) },
	{ 0xce9ed0ff, __VMLINUX_SYMBOL_STR(mmc_wait_for_req) },
	{ 0x6c364bd6, __VMLINUX_SYMBOL_STR(mmc_wait_for_cmd) },
	{ 0x5f754e5a, __VMLINUX_SYMBOL_STR(memset) },
	{ 0x2d80efb, __VMLINUX_SYMBOL_STR(mmc_start_req) },
	{ 0xa8cbd73b, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0x46608fa0, __VMLINUX_SYMBOL_STR(getnstimeofday) },
	{ 0xbf6a661, __VMLINUX_SYMBOL_STR(mmc_can_trim) },
	{ 0x4449f6dd, __VMLINUX_SYMBOL_STR(mmc_erase) },
	{ 0x8ca3a5e, __VMLINUX_SYMBOL_STR(mmc_can_erase) },
	{ 0x7f8e311, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x54ac93a6, __VMLINUX_SYMBOL_STR(debugfs_remove) },
	{ 0x8d6a5a89, __VMLINUX_SYMBOL_STR(debugfs_create_file) },
	{ 0x3c9b1ed0, __VMLINUX_SYMBOL_STR(contig_page_data) },
	{ 0x8f678b07, __VMLINUX_SYMBOL_STR(__stack_chk_guard) },
	{ 0xf0fdf6cb, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0xd995b388, __VMLINUX_SYMBOL_STR(mmc_rpm_release) },
	{ 0x7a9e8535, __VMLINUX_SYMBOL_STR(mmc_release_host) },
	{ 0xefb5f511, __VMLINUX_SYMBOL_STR(__mmc_claim_host) },
	{ 0x2530a02b, __VMLINUX_SYMBOL_STR(mmc_rpm_hold) },
	{ 0xf29003b, __VMLINUX_SYMBOL_STR(__alloc_pages_nodemask) },
	{ 0x86a4889a, __VMLINUX_SYMBOL_STR(kmalloc_order_trace) },
	{ 0x11a13e31, __VMLINUX_SYMBOL_STR(_kstrtol) },
	{ 0xfbc74f64, __VMLINUX_SYMBOL_STR(__copy_from_user) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x64d0da8e, __VMLINUX_SYMBOL_STR(__free_pages) },
	{ 0xeb8d0b7c, __VMLINUX_SYMBOL_STR(mmc_set_blocklen) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xe6da44a, __VMLINUX_SYMBOL_STR(set_normalized_timespec) },
	{ 0x2e5810c6, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr1) },
	{ 0x33d5d501, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xa2e9338a, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x59e5070d, __VMLINUX_SYMBOL_STR(__do_div64) },
	{ 0xfa2a45e, __VMLINUX_SYMBOL_STR(__memzero) },
	{ 0xa308599c, __VMLINUX_SYMBOL_STR(mmc_set_data_timeout) },
	{ 0xd0eada40, __VMLINUX_SYMBOL_STR(mmc_can_reset) },
	{ 0xd112147b, __VMLINUX_SYMBOL_STR(mmc_hw_reset_check) },
	{ 0xf7802486, __VMLINUX_SYMBOL_STR(__aeabi_uidivmod) },
	{ 0xe707d823, __VMLINUX_SYMBOL_STR(__aeabi_uidiv) },
	{ 0x6a8476b5, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x9c956a0f, __VMLINUX_SYMBOL_STR(seq_printf) },
	{ 0xae0655cf, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x50d2d6c3, __VMLINUX_SYMBOL_STR(single_open) },
	{ 0xefd6cf06, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr0) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

