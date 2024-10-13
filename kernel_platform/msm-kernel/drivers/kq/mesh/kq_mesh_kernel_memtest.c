// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Sagy Shih <sagy.shih@mediatek.com>
 */

#include <linux/printk.h>
#include <linux/slab.h>

#define MEM_TEST_SIZE		KMALLOC_MAX_SIZE
#define MIN_TEST_SIZE		256
#define FORCE_ERROR 		0
#define MAXNUM_OF_TESTAREA	200
#define ERR_CHECK(addr, pattern)  \
{ if(*(addr) != pattern) \
  {pr_err("DDR buffer validation failed at %p, %016llx must be %016llx", addr, *(addr), pattern); \
  panic("[mesh-k] kernel space memory test failure\n");} }

void kernel_mem_test_sz(unsigned int size, unsigned int num_of_testarea)
{
	void *ptr[MAXNUM_OF_TESTAREA] = {0,};
	u64 *addr, *eaddr;
	char *temp;
	u64 pattern[8] = {0xffffffffffffffff, 0x0000000000000000, 0xa5a5a5a5a5a5a5a5, 0x5a5a5a5a5a5a5a5a, 0x55aa55aa55aa55aa, 0xaa55aa55aa55aa55, 0x5aa5a55a5aa5a55a, 0xacedbadddeadface};
	int i = 0, j = 0;

	if(num_of_testarea > MAXNUM_OF_TESTAREA) {
		pr_info("%s num_of_testarea exceeded %d, > MAXNUM_OF_TESTAREA %d!\n", __func__, num_of_testarea, MAXNUM_OF_TESTAREA);
		return;
	}

	if(size > KMALLOC_MAX_SIZE) {
		pr_info("%s size exceeded %d, > KMALLOC_MAX_SIZE %lu!\n", __func__, size, KMALLOC_MAX_SIZE);
		return;
	} else if (size < MIN_TEST_SIZE) {
		pr_info("%s size too small to test %d, < MIN_TEST_SIZE %d!\n", __func__, size, MIN_TEST_SIZE);
		return;
	}

	for(i = 0; i < num_of_testarea; i++) {
		temp = kmalloc(size, GFP_KERNEL);
		ptr[i] = temp;

		if (!ptr[i]) {
			pr_info("%s failed to kmalloc size %d!\n", __func__, size);
			break;
		}
	}

	for(i = 0; (ptr[i] && i < num_of_testarea); i++)
	{
		addr = ptr[i];
		eaddr = ptr[i] + size;
		pr_info("%dth loop\n", i);
		pr_info("Test DRAM start address %llx - %llx\n", (u64)addr, (u64)eaddr);
		pr_info("Test DRAM test SIZE 0x%x\n", size);

		for(j = 0; j<8; j++)
		{
			addr = ptr[i];
			eaddr = ptr[i] + size;
			pr_info("%d. pattern test %llx", j, pattern[j]);

			for (; (u64)(addr+0x20) <= (u64)eaddr; addr=addr+0x20)
			{
				*(addr+0x00) = pattern[j]; *(addr+0x01) = pattern[j]; *(addr+0x02) = pattern[j]; *(addr+0x03) = pattern[j];
				*(addr+0x04) = pattern[j]; *(addr+0x05) = pattern[j]; *(addr+0x06) = pattern[j]; *(addr+0x07) = pattern[j];
				*(addr+0x08) = pattern[j]; *(addr+0x09) = pattern[j]; *(addr+0x0A) = pattern[j]; *(addr+0x0B) = pattern[j];
				*(addr+0x0C) = pattern[j]; *(addr+0x0D) = pattern[j]; *(addr+0x0E) = pattern[j]; *(addr+0x0F) = pattern[j];

				*(addr+0x10) = pattern[j]; *(addr+0x11) = pattern[j]; *(addr+0x12) = pattern[j]; *(addr+0x13) = pattern[j];
				*(addr+0x14) = pattern[j]; *(addr+0x15) = pattern[j]; *(addr+0x16) = pattern[j]; *(addr+0x17) = pattern[j];
				*(addr+0x18) = pattern[j]; *(addr+0x19) = pattern[j]; *(addr+0x1A) = pattern[j]; *(addr+0x1B) = pattern[j];
				*(addr+0x1C) = pattern[j]; *(addr+0x1D) = pattern[j]; *(addr+0x1E) = pattern[j]; *(addr+0x1F) = pattern[j];
				//pr_info("addr %llx %llx", (u64)addr, *addr);
			}

#if FORCE_ERROR
			{
			u64 *DDR_TEST_ERROR = (u64*)0x90000000ULL;
			*DDR_TEST_ERROR = 0xDEADDEADDEADDEAD;
			}
#endif

			addr = ptr[i];
			eaddr = ptr[i] + size;
			for (; (u64)(addr+0x20) <= (u64)eaddr; addr=addr+0x20)
			{
				ERR_CHECK(addr+0x00, pattern[j]);
				ERR_CHECK(addr+0x01, pattern[j]);
				ERR_CHECK(addr+0x02, pattern[j]);
				ERR_CHECK(addr+0x03, pattern[j]);
				ERR_CHECK(addr+0x04, pattern[j]);
				ERR_CHECK(addr+0x05, pattern[j]);
				ERR_CHECK(addr+0x06, pattern[j]);
				ERR_CHECK(addr+0x07, pattern[j]);
				ERR_CHECK(addr+0x08, pattern[j]);
				ERR_CHECK(addr+0x09, pattern[j]);
				ERR_CHECK(addr+0x0A, pattern[j]);
				ERR_CHECK(addr+0x0B, pattern[j]);
				ERR_CHECK(addr+0x0C, pattern[j]);
				ERR_CHECK(addr+0x0D, pattern[j]);
				ERR_CHECK(addr+0x0E, pattern[j]);
				ERR_CHECK(addr+0x0F, pattern[j]);

				ERR_CHECK(addr+0x10, pattern[j]);
				ERR_CHECK(addr+0x11, pattern[j]);
				ERR_CHECK(addr+0x12, pattern[j]);
				ERR_CHECK(addr+0x13, pattern[j]);
				ERR_CHECK(addr+0x14, pattern[j]);
				ERR_CHECK(addr+0x15, pattern[j]);
				ERR_CHECK(addr+0x16, pattern[j]);
				ERR_CHECK(addr+0x17, pattern[j]);
				ERR_CHECK(addr+0x18, pattern[j]);
				ERR_CHECK(addr+0x19, pattern[j]);
				ERR_CHECK(addr+0x1A, pattern[j]);
				ERR_CHECK(addr+0x1B, pattern[j]);
				ERR_CHECK(addr+0x1C, pattern[j]);
				ERR_CHECK(addr+0x1D, pattern[j]);
				ERR_CHECK(addr+0x1E, pattern[j]);
				ERR_CHECK(addr+0x1F, pattern[j]);
			}
		}

		kfree(ptr[i]);
	}

	pr_info("ddr_test_boot_region finished.");
}

void kernel_mem_test(unsigned int size, unsigned int num_of_testarea)
{
	kernel_mem_test_sz(size, num_of_testarea);
}