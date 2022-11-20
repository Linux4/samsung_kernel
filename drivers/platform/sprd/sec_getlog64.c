/*
 *  sec_getlog64.c
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <asm/setup.h>
#include <linux/memblock.h>
#include <soc/sprd/sec_log64.h>
#define GETLOG_PHYSICAL_OFFSET 0x40000000

extern phys_addr_t sec_logbuf_base_addr;

static void sec_getlog_trim(unsigned int *val)
{
	//getlog64 tools decrease PHYSICAL_OFFSET 0x40000000. it's bug.
	//this function only for common getlog tool.
	*val = (*val) + (unsigned int)GETLOG_PHYSICAL_OFFSET;
}

static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	void *p_fb;		/* it must be physical address */
	u32 xres;
	u32 yres;
	u32 bpp;		/* color depth : 16 or 24 */
	u32 frames;		/* frame buffer count : 2 */
} frame_buf_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('f' << 24) | ('b' << 16) | ('u' << 8) | ('f' << 0)),
};

void sec_getlog_supply_fbinfo(void *p_fb, u32 xres, u32 yres, u32 bpp,
			      u32 frames)
{
	if (p_fb) {
		pr_info("%s: 0x%p %d %d %d %d\n", __func__, p_fb, xres, yres,
			bpp, frames);
		frame_buf_mark.p_fb = p_fb;
		frame_buf_mark.xres = xres;
		frame_buf_mark.yres = yres;
		frame_buf_mark.bpp = bpp;
		frame_buf_mark.frames = frames;
	}
}
EXPORT_SYMBOL(sec_getlog_supply_fbinfo);

static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	u32 log_mark_version;
	u32 framebuffer_mark_version;
	void *this;			/* this is used for addressing
					   log buffer in 2 dump files */
	struct {
		phys_addr_t size;	/* memory block's size */
		phys_addr_t addr;	/* memory block'sPhysical address */
	} mem[2];
} marks_ver_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('v' << 24) | ('e' << 16) | ('r' << 8) | ('s' << 0)),
#ifdef CONFIG_ARM64
	.log_mark_version = 2,
#else
	.log_mark_version = 1,
#endif
	.framebuffer_mark_version = 1,
	.this = &marks_ver_mark,
};

/* mark for GetLog extraction */
static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	void *p_main;
	void *p_radio;
	void *p_events;
	void *p_system;
} plat_log_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('p' << 24) | ('l' << 16) | ('o' << 8) | ('g' << 0)),
};

void sec_getlog_supply_loggerinfo(void *p_main,
				  void *p_radio, void *p_events, void *p_system)
{
	pr_info("%s: 0x%p 0x%p 0x%p 0x%p\n", __func__, p_main, p_radio,
		p_events, p_system);
		plat_log_mark.p_main = p_main;
		sec_getlog_trim(&plat_log_mark.p_main);
		plat_log_mark.p_radio = p_radio;
		sec_getlog_trim(&plat_log_mark.p_radio);
		plat_log_mark.p_events = p_events;
		sec_getlog_trim(&plat_log_mark.p_events);
		plat_log_mark.p_system = p_system;
		sec_getlog_trim(&plat_log_mark.p_system);
}
EXPORT_SYMBOL(sec_getlog_supply_loggerinfo);

static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	void *klog_buf;
} kernel_log_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('k' << 24) | ('l' << 16) | ('o' << 8) | ('g' << 0)),
};

void sec_getlog_supply_kloginfo(void *klog_buf)
{
	unsigned long size_struct_sec_log_buffer = sizeof(struct sec_log_buffer);
	/* Round-up to 8 byte alignment */
	size_struct_sec_log_buffer = (((size_struct_sec_log_buffer + 8) / 8) * 8);

	pr_info("%s: virt 0x%p\n", __func__, klog_buf);
	//kernel_log_mark.klog_buf = 0x46B00018;
	kernel_log_mark.klog_buf = sec_logbuf_base_addr - GETLOG_PHYSICAL_OFFSET
		+ size_struct_sec_log_buffer;
	pr_info("%s: phys + offset + header 0x%p\n", __func__, kernel_log_mark.klog_buf);
}
EXPORT_SYMBOL(sec_getlog_supply_kloginfo);

static int __init sec_getlog_init(void)
{
	pr_info("sec_debug: GetLog support (ver:0x%p)\n", &marks_ver_mark);
	return 0;
}

core_initcall(sec_getlog_init);
