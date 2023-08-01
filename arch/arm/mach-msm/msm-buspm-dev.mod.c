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
	{ 0xd6965146, __VMLINUX_SYMBOL_STR(noop_llseek) },
	{ 0x19b727f, __VMLINUX_SYMBOL_STR(misc_deregister) },
	{ 0xc8ddbd69, __VMLINUX_SYMBOL_STR(misc_register) },
	{ 0x67c2fa54, __VMLINUX_SYMBOL_STR(__copy_to_user) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x28368bf, __VMLINUX_SYMBOL_STR(arm_dma_ops) },
	{ 0x6da86f3a, __VMLINUX_SYMBOL_STR(clk_disable) },
	{ 0xf9e22a82, __VMLINUX_SYMBOL_STR(clk_unprepare) },
	{ 0x630304ed, __VMLINUX_SYMBOL_STR(clk_enable) },
	{ 0x957f502, __VMLINUX_SYMBOL_STR(clk_prepare) },
	{ 0xac69fe79, __VMLINUX_SYMBOL_STR(clk_set_rate) },
	{ 0xec4d9e3a, __VMLINUX_SYMBOL_STR(clk_get_sys) },
	{ 0xfa2a45e, __VMLINUX_SYMBOL_STR(__memzero) },
	{ 0xfbc74f64, __VMLINUX_SYMBOL_STR(__copy_from_user) },
	{ 0x5f754e5a, __VMLINUX_SYMBOL_STR(memset) },
	{ 0xbbccdff9, __VMLINUX_SYMBOL_STR(msm_rpm_free_request) },
	{ 0xcd9436d6, __VMLINUX_SYMBOL_STR(msm_rpm_wait_for_ack) },
	{ 0xd60cf4cf, __VMLINUX_SYMBOL_STR(msm_rpm_send_request) },
	{ 0x47cfdaeb, __VMLINUX_SYMBOL_STR(msm_rpm_add_kvp_data) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x980434a2, __VMLINUX_SYMBOL_STR(msm_rpm_create_request) },
	{ 0x4738be51, __VMLINUX_SYMBOL_STR(remap_pfn_range) },
	{ 0xf087137d, __VMLINUX_SYMBOL_STR(__dynamic_pr_debug) },
	{ 0xefd6cf06, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr0) },
	{ 0x33d5d501, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xa2e9338a, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xc6cbbc89, __VMLINUX_SYMBOL_STR(capable) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "86D17D3C18CEB7F286BE241");
