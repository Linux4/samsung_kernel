#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/io.h>

#include "trout_fm_ctrl.h"
#include "trout_interface.h"

#define SHARK_FM_REG_BASE 0x40270000
#define SHARK_FM_SIZE 0x10000

#define SHARK_APB_BASE_ADDR 0x402E0000
#define SHARK_APB_BASE_ADDR_SIZE 0x20

#define SHARK_AHB_BASE_ADDR 0x71300000
#define SHARK_AHB_BASE_ADDR_SIZE 0x10

#define PINMAP_ADDR 0x402A0000
#define PINMAP_ADDR_SIZE 0x500

#define SHARK_FM_MSPI_BASE 0x40070000
#define SHARK_FM_MSPI_BASE_SIZE 0x20

static void __iomem *shark_io_base, *apb_base, \
*ahb_base, *pinmap_base, *mspi_base;

unsigned int onchip_read(u32 addr, u32 *val)
{
	unsigned int addr_offset;
	unsigned int *vir_addr;
	void __iomem *shark_temp_base;
	if (addr >= SHARK_FM_REG_BASE && addr <= (SHARK_FM_REG_BASE + \
		SHARK_FM_SIZE)) {
		addr_offset = addr - SHARK_FM_REG_BASE;

		vir_addr = (unsigned int *)(addr_offset + \
			(unsigned int)shark_io_base);

		*val = *vir_addr;
	} else if (addr >= SHARK_APB_BASE_ADDR && addr <= \
	(SHARK_APB_BASE_ADDR + SHARK_APB_BASE_ADDR_SIZE)) {
		addr_offset = addr - SHARK_APB_BASE_ADDR;

		vir_addr = (unsigned int *)(addr_offset + \
			(unsigned int)apb_base);

		*val = *vir_addr;
	} else if (addr >= SHARK_AHB_BASE_ADDR && addr <= \
	(SHARK_AHB_BASE_ADDR + SHARK_AHB_BASE_ADDR_SIZE)) {
		addr_offset = addr - SHARK_AHB_BASE_ADDR;

		vir_addr = (unsigned int *)(addr_offset + \
			(unsigned int)ahb_base);

		*val = *vir_addr;
	} else if (addr >= PINMAP_ADDR && addr <= (PINMAP_ADDR + \
	PINMAP_ADDR_SIZE)) {
		addr_offset = addr - PINMAP_ADDR;

		vir_addr = (unsigned int *)(addr_offset + \
			(unsigned int)pinmap_base);

		*val = *vir_addr;
	} else if (addr >= SHARK_FM_MSPI_BASE && addr <= \
	(SHARK_FM_MSPI_BASE + SHARK_FM_MSPI_BASE_SIZE)) {
		addr_offset = addr - SHARK_FM_MSPI_BASE;

		vir_addr = (unsigned int *)(addr_offset + \
			(unsigned int)mspi_base);

		*val = *vir_addr;
	} else {
		TROUT_PRINT\
			("read out of reg range, addr is %x\n", addr);
		shark_temp_base = ioremap(addr, 4);
		if (shark_temp_base == 0) {
			TROUT_PRINT\
				("failed to ioremap shark_temp_base region\n");
			return -1;
		}
		vir_addr = (unsigned int *)shark_temp_base;
		*val = *vir_addr;
		iounmap(shark_temp_base);
	}
	/*TROUT_PRINT("read result is %x\n",(*val));*/

		return 0;
}

unsigned int onchip_write(u32 addr, u32 val)
{
	unsigned int addr_offset;
	unsigned int *vir_addr;
	void __iomem *shark_temp_base;
	if (addr >= SHARK_FM_REG_BASE && addr <= (SHARK_FM_REG_BASE + \
		SHARK_FM_SIZE)) {
		addr_offset = addr - SHARK_FM_REG_BASE;

		vir_addr = (unsigned int *)(addr_offset + \
			(unsigned int)shark_io_base);

		*vir_addr = val;

	} else if (addr >= SHARK_APB_BASE_ADDR && addr <= \
	(SHARK_APB_BASE_ADDR + SHARK_APB_BASE_ADDR_SIZE)) {
		addr_offset = addr - SHARK_APB_BASE_ADDR;

		vir_addr = (unsigned int *)(addr_offset + \
			(unsigned int)apb_base);

		*vir_addr = val;

	} else if (addr >= SHARK_AHB_BASE_ADDR && addr <= \
	(SHARK_AHB_BASE_ADDR + SHARK_AHB_BASE_ADDR_SIZE)) {
		addr_offset = addr - SHARK_AHB_BASE_ADDR;

		vir_addr = (unsigned int *)(addr_offset + \
			(unsigned int)ahb_base);

		*vir_addr = val;

	} else if (addr >= PINMAP_ADDR && addr <= \
	(PINMAP_ADDR + PINMAP_ADDR_SIZE)) {
		addr_offset = addr - PINMAP_ADDR;

		vir_addr = (unsigned int *)(addr_offset + \
			(unsigned int)pinmap_base);

		*vir_addr = val;

	} else if (addr >= SHARK_FM_MSPI_BASE && addr <= \
	(SHARK_FM_MSPI_BASE + SHARK_FM_MSPI_BASE_SIZE)) {
		addr_offset = addr - SHARK_FM_MSPI_BASE;

		vir_addr = (unsigned int *)(addr_offset + \
			(unsigned int)mspi_base);

		*vir_addr = val;
	} else {
		TROUT_PRINT("write out of reg range, addr is %x\n", addr);
		shark_temp_base = ioremap(addr, 4);
		if (shark_temp_base == 0) {
			TROUT_PRINT\
				("failed to ioremap shark_temp_base region\n");
			return -1;
		}
		vir_addr = (unsigned int *)shark_temp_base;
		*vir_addr = val;
		iounmap(shark_temp_base);
	}
	/*TROUT_PRINT("write result is %x\n",(*vir_addr));*/
	return 0;
}

unsigned int onchip_init(void)
{
	TROUT_PRINT("enter shark interface\n");
	shark_io_base = ioremap(SHARK_FM_REG_BASE, SHARK_FM_SIZE);
	if (shark_io_base == 0) {
		TROUT_PRINT("failed to ioremap io base region\n");
		return -1;
	}

	pinmap_base = ioremap(PINMAP_ADDR, PINMAP_ADDR_SIZE);
	if (pinmap_base == 0) {
		TROUT_PRINT("failed to ioremap pinmap base region\n");
		return -1;
	}

	apb_base = ioremap(SHARK_APB_BASE_ADDR, SHARK_APB_BASE_ADDR_SIZE);
	if (apb_base == 0) {
		TROUT_PRINT("failed to ioremap apb base region\n");
		return -1;
	}

	ahb_base = ioremap(SHARK_AHB_BASE_ADDR, SHARK_AHB_BASE_ADDR_SIZE);
	if (ahb_base == 0) {
		TROUT_PRINT("failed to ioremap ahb base region\n");
		return -1;
	}

	mspi_base = ioremap(SHARK_FM_MSPI_BASE, SHARK_FM_MSPI_BASE_SIZE);
	if (mspi_base == 0) {
		TROUT_PRINT("failed to ioremap mspi base region\n");
		return -1;
	}
	return 0;
}

unsigned int onchip_exit(void)
{
	iounmap(shark_io_base);
	iounmap(apb_base);
	iounmap(ahb_base);
	iounmap(pinmap_base);
	iounmap(mspi_base);
	return 0;
}

static struct trout_interface onchip_interface = {
	.name = "onchip",
	.init = onchip_init,
	.exit = onchip_exit,
	.read_reg = onchip_read,
	.write_reg = onchip_write,
};

int trout_onchip_init(struct trout_interface **p)
{
	*p = &onchip_interface;

	return 0;
}

