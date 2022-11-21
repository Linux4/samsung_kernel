
#include <linux/io.h>
#include <linux/module.h>
//#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
//#include <linux/interrupt.h>
//#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/exynos_ion.h>


#include "fimc-is-config.h"


#include "fimc-is-binary.h"
#include "fimc-is-mem.h"

#define LEN_OF_VECTOR_SET 17
#define NUM_VECTOR_SET 2

static unsigned int get_newline_offset(const char *src, size_t size)
{
	size_t offset = 1;

	do {
		offset++;
		if ((*src == 0x0d) && (*(src + 1) == 0x0a))
			return offset;
		src++;
	} while (offset <= size);

	return 0;
}


static int reset_value_test(char *ip)
{
	struct fimc_is_binary bin;
	char *filename;
	unsigned int addr, val, val_read;
	unsigned int ofs;
	int ret;
	size_t count = 0;
	char *buf;

	void __iomem *baseaddr;

	filename = __getname();
	if (unlikely(!filename))
		return -ENOMEM;

	snprintf(filename, PATH_MAX, "sfrtest/%s_reset%s", ip, ".txt");

	ret = request_binary(&bin, FIMC_IS_ISP_LIB_SDCARD_PATH, filename, NULL);
	if (ret) {
		err_vec("failed to load vector configuration (%d): %sname: %s",
				ret, FIMC_IS_ISP_LIB_SDCARD_PATH, filename);
		__putname(filename);
		return ret;
	} else {
		info_vec("vector configuration info - size: 0x%lx name: %s\n",
				bin.size, filename);
		__putname(filename);
	}

	baseaddr = ioremap_nocache(0x14400000, 0x100000);

	buf = (char *)bin.data;
	while (count < bin.size) {
		if (sscanf(buf, "%x %x\n", &addr, &val) == NUM_VECTOR_SET) {
			dbg_psv("addr: 0x%08x, val: 0x%08x\n", addr, val);
			count += LEN_OF_VECTOR_SET;
			buf += LEN_OF_VECTOR_SET;

			val_read = __raw_readl(baseaddr + (addr - 0x00400000));

			if (val != val_read) {
				ret++;
				err_vec("reset Miis-match found at 0x%08x: 0x%08x != 0x%08x\n", addr, val, val_read);
				}

		}

		ofs = get_newline_offset(buf, bin.size - count);
		if (ofs) {
			count += ofs;
			buf += ofs;
		} else {
			break;
		}
	}

	iounmap(baseaddr);

	release_binary(&bin);

	info_vec("%s finished\n", ip);
	return ret;
}

static int sfr_rw_test(char *ip)
{
	struct fimc_is_binary bin;
	char *filename;
	unsigned int addr, val, val_read;
	unsigned int ofs;
	int ret;
	size_t count = 0;
	char *buf;

	void __iomem *baseaddr;

	filename = __getname();
	if (unlikely(!filename))
		return -ENOMEM;

	snprintf(filename, PATH_MAX, "sfrtest/%s_access%s", ip, ".txt");

	ret = request_binary(&bin, FIMC_IS_ISP_LIB_SDCARD_PATH, filename, NULL);
	if (ret) {
		err_vec("failed to load vector configuration (%d): %sname: %s",
				ret, FIMC_IS_ISP_LIB_SDCARD_PATH, filename);
		__putname(filename);
		return ret;
	} else {
		info_vec("vector configuration info - size: 0x%lx name: %s\n",
				bin.size, filename);
		__putname(filename);
	}

	baseaddr = ioremap_nocache(0x14400000, 0x100000);

	buf = (char *)bin.data;
	while (count < bin.size) {
		if (sscanf(buf, "%x %x\n", &addr, &val) == NUM_VECTOR_SET) {
			dbg_psv("addr: 0x%08x, val: 0x%08x\n", addr, val);
			count += LEN_OF_VECTOR_SET;
			buf += LEN_OF_VECTOR_SET;

			 __raw_writel(val, baseaddr + (addr - 0x00400000));
			val_read = __raw_readl(baseaddr + (addr - 0x00400000));

			if (val != val_read) {
				ret++;
				err_vec("R/W Mis-match at 0x%08x: 0x%08x != 0x%08x\n", addr, val, val_read);
				}

		}

		ofs = get_newline_offset(buf, bin.size - count);
		if (ofs) {
			count += ofs;
			buf += ofs;
		} else {
			break;
		}
	}

	iounmap(baseaddr);

	release_binary(&bin);

	return ret;
}


int csis0_rstval_test(void)
{
	return reset_value_test("csis0");
}

int csis1_rstval_test(void)
{
	return reset_value_test("csis1");
}

int bns_rstval_test(void)
{
	return reset_value_test("bns");
}

#if defined(CONFIG_SOC_EXYNOS7880)
int taa_rstval_test(void)
{
	return reset_value_test("taa");
}
#endif

int isp_rstval_test(void)
{
	return reset_value_test("isp");
}

int mcsc_rstval_test(void)
{
	return reset_value_test("mcsc");
}

int vra_rstval_test(void)
{
	return reset_value_test("vra");
}

int csis0_sfr_access_test(void)
{
	return sfr_rw_test("csis0");
}

int csis1_sfr_access_test(void)
{
	return sfr_rw_test("csis1");
}

int bns_sfr_access_test(void)
{
	return sfr_rw_test("bns");
}

#if defined(CONFIG_SOC_EXYNOS7880)
int taa_sfr_access_test(void)
{
	return sfr_rw_test("taa");
}
#endif

int isp_sfr_access_test(void)
{
	return sfr_rw_test("isp");
}

int mcsc_sfr_access_test(void)
{
	return sfr_rw_test("mcsc");
}

int vra_sfr_access_test(void)
{
	return sfr_rw_test("vra");
}

static int fimcis_reset(void)
{
	void __iomem *baseaddr;
	u32 readval;

	/* csis0 */
	baseaddr = ioremap(0x14420000, 0x10000);
	__raw_writel(0x2, baseaddr + 0x4);
	while (1) {
		readval = __raw_readl(baseaddr + 4);
		if (readval == 0x00004000)
			break;
		}
	iounmap(baseaddr);
	info_vec("csis0 reset...\n");

	/* csis1 */
	baseaddr = ioremap(0x14460000, 0x10000);
	__raw_writel(0x2, baseaddr + 0x4);
	while (1) {
		readval = __raw_readl(baseaddr + 4);
		if (readval == 0x00004000)
			break;
		}
	iounmap(baseaddr);
	info_vec("csis1 reset...\n");

	/* bns */
	baseaddr = ioremap(0x14410000, 0x10000);
	__raw_writel(0x00080000, baseaddr + 0x4);
	while (1) {
		readval = __raw_readl(baseaddr + 4);
		if ((readval & (1<<18)) == (1<<18))
			break;
		}
	__raw_writel(0x00020000, baseaddr + 0x4);
	iounmap(baseaddr);
	info_vec("bns reset...\n");

	/* 3aa */
	baseaddr = ioremap(0x14480000, 0x10000);
	__raw_writel(0x00000001, baseaddr + 0xC);
	while (1) {
		readval = __raw_readl(baseaddr + 0xC);
		if (readval  == 0x00000000)
			break;
		}
	iounmap(baseaddr);
	info_vec("3aa reset...\n");

	/* isp */
	baseaddr = ioremap(0x14400000, 0x10000);
	__raw_writel(0x00000001, baseaddr + 0xC);
	while (1) {
		readval = __raw_readl(baseaddr + 0xC);
		if (readval  == 0x00000000)
			break;
		}
	iounmap(baseaddr);
	info_vec("isp reset...\n");

	/* mcsc */
	baseaddr = ioremap(0x14430000, 0x10000);
	__raw_writel(0x00000000, baseaddr + 0x0);
	__raw_writel(0x00000000, baseaddr + 0x07a4);
	__raw_writel(0xFFFFFFFF, baseaddr + 0x07ac);
	__raw_writel(0x00000001, baseaddr + 0x24);
	__raw_writel(0x00000001, baseaddr + 0x800);
	__raw_writel(0x00000001, baseaddr + 0x20);
	while (1) {
		readval = __raw_readl(baseaddr + 0x790);
		if ((readval & 1)  == 0x00000000)
			break;
}
	iounmap(baseaddr);
	info_vec("mcsc reset...\n");

	/* vra */
	baseaddr = ioremap(0x14440000, 0x10000);
	__raw_writel(0x00000001, baseaddr + 0x3008);
	while (1) {
		readval = __raw_readl(baseaddr + 0x300C);
		if ((readval & 1)  == 0x00000001)
			break;
		}
	__raw_writel(0x00000001, baseaddr + 0x3004);

	__raw_writel(0x00000002, baseaddr + 0xb04C);
	__raw_writel(0x00000001, baseaddr + 0xb008);
	while (1) {
		readval = __raw_readl(baseaddr + 0xb00C);
		if ((readval & 1)  == 0x00000001)
			break;
		}
	__raw_writel(0x00000001, baseaddr + 0xb004);
	__raw_writel(0x00000001, baseaddr + 0xb048);

	iounmap(baseaddr);
	info_vec("vra reset...\n");

	return 0;
}


int fimcis_sfr_test(void)
{
	int errorcode = 0;

	printk("fimcis_sfr_test start\n");


	fimcis_reset();
	errorcode = errorcode | csis0_rstval_test();
	errorcode = errorcode | csis1_rstval_test();
	errorcode = errorcode | bns_rstval_test();

#if defined(CONFIG_SOC_EXYNOS7880)
	errorcode = errorcode | taa_rstval_test();
#endif

	errorcode = errorcode | isp_rstval_test();
	errorcode = errorcode | mcsc_rstval_test();
	errorcode = errorcode | vra_rstval_test();

	errorcode = errorcode | csis0_sfr_access_test();
	errorcode = errorcode | csis1_sfr_access_test();
	errorcode = errorcode | bns_sfr_access_test();

#if defined(CONFIG_SOC_EXYNOS7880)
	errorcode = errorcode | taa_sfr_access_test();
#endif
	errorcode = errorcode | isp_sfr_access_test();
	errorcode = errorcode | mcsc_sfr_access_test();
	errorcode = errorcode | vra_sfr_access_test();

#if 1
	fimcis_reset();

	errorcode = errorcode | csis0_rstval_test();
	errorcode = errorcode | csis1_rstval_test();
	errorcode = errorcode | bns_rstval_test();

#if defined(CONFIG_SOC_EXYNOS7880)
	errorcode = errorcode | taa_rstval_test();
#endif

	errorcode = errorcode | isp_rstval_test();
	errorcode = errorcode | mcsc_rstval_test();
	errorcode = errorcode | vra_rstval_test();
#endif

	printk("fimcis_sfr_test uErrorCode: 0x%x\n", errorcode);

	return errorcode;
}


