/*
* * Copyright (C) 2012 Spreadtrum Communications Inc.
* *
* * This software is licensed under the terms of the GNU General Public
* * License version 2, as published by the Free Software Foundation, and
* * may be copied, distributed, and modified under those terms.
* *
* * This program is distributed in the hope that it will be useful,
* * but WITHOUT ANY WARRANTY; without even the implied warranty of
* * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* * GNU General Public License for more details.
* */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/board.h>
#include <soc/sprd/adi.h>
#include <linux/leds.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#include <linux/mfd/sm5701_core.h>
#include <video/sprd_img.h>

#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#endif

#define flash_get_max_index(_index, cnt, _array_var) \
	do {	(_index) = ARRAY_SIZE(_array_var); \
		(_index) = MIN((_index), SPRD_FLASH_MAX_CELL); \
	} while(0)

/*main time (ms)*/
static const struct sprd_flash_element main_time[]= {
	{0x0, 100},
	{0x1, 200},
	{0x2, 300},
	{0x3, 400},
	{0x4, 500},
	{0x5, 600},
	{0x6, 700},
	{0x7, 800},
	{0x8, 900},
	{0x9, 1000},
	{0xA, 1100},
	{0xB, 1200},
/*
	0xC, 1300,
	0xD, 1400,
	0xE, 1500,
	0xF, 1600,
*/
};

/*preflash charge (mA) */
static const struct sprd_flash_element preflash_charge[] = {
	{0x0, 10},
	{0x1, 20},
	{0x2, 30},
	{0x3, 40},
	{0x4, 50},
	{0x5, 60},
	{0x6, 70},
	{0x7, 80},
	{0x8, 90},
	{0x9, 100},
	{0xA, 110},
	{0xB, 120},
	{0xC, 130},
	{0xD, 140},
	{0xE, 150},
	{0xF, 160},
	{0x10, 170},
	{0x11, 180},
	{0x12, 190},
	{0x13, 200},
	{0x14, 210},
	{0x15, 220},
	{0x16, 230},
	{0x17, 240},
	{0x18, 250},
	{0x19, 260},
	{0x1A, 270},
	{0x1B, 280},
	{0x1C, 290},
	{0x1D, 300},
	{0x1E, 310},
	{0x1F, 320},
};

/*main charge (mA)*/
static const struct sprd_flash_element main_charge[]= {
	 {0x0, 300},
	 {0x1, 325},
	 {0x2, 350},
	 {0x3, 375},
	 {0x4, 400},
	 {0x5, 425},
	 {0x6, 450},
	 {0x7, 475},
	 {0x8, 500},
	 {0x9, 525},
	 {0xA, 550},
	 {0xB, 575},
	 {0xC, 600},
	 {0xD, 625},
	 {0xE, 650},
	 {0xF, 700},
	 {0x10, 750},
	 {0x11, 800},
	 {0x12, 850},
	 {0x13, 900},
	 {0x14, 950},
	 {0x15, 1000},
/*
	 0x16, 1050,
	 0x17, 1100,
	 0x18, 1150,
	 0x19, 1200,
	 0x1A, 1250,
	 0x1B, 1300,
	 0x1C, 1350,
	 0x1D, 1400,
	 0x1E, 1450,
	 0x1F, 1500,
*/
};

int sprd_flash_on(void)
{
	printk("sprd_flash_on\n");
	sm5701_led_ready(MOVIE_MODE);
	sm5701_set_fleden(SM5701_FLEDEN_ON_MOVIE);

	return 0;
}

int sprd_flash_high_light(void)
{
	printk("sprd_flash_high_light\n");
	sm5701_led_ready(FLASH_MODE);
	sm5701_set_fleden(SM5701_FLEDEN_ON_FLASH);

	return 0;
}

int sprd_flash_close(void)
{
	printk("sprd_flash_close\n");
	sm5701_set_fleden(SM5701_FLEDEN_DISABLED);
	sm5701_led_ready(LED_DISABLE);

	return 0;
}

/*0: charge;  1: time*/
int sprd_flash_check_param(uint8_t which_func, uint8_t type, struct sprd_flash_element *element)
{
	int ret = -1;
	int max_index = 0;

	if (NULL == element) {
		printk(KERN_ERR"%s param is error!!!\n", __func__);
		return -1;
	}
	/*0: charge;  1: time*/
	if (0== which_func) {
		switch (type) {
		case FLASH_TYPE_PREFLASH:
			max_index = ARRAY_SIZE(preflash_charge);
			if ( 0 < max_index
				&& element->index < max_index
				&& (element->val == preflash_charge[element->index].val)) {
				ret = 0;
			}
			break;
		case FLASH_TYPE_MAIN:
			max_index = ARRAY_SIZE(main_charge);
			if ( 0 < max_index
				&& element->index < max_index
				&& (element->val == main_charge[element->index].val)) {
				ret = 0;
			}
			break;
		default:
			printk(KERN_ERR"%s not support type!\n", __func__);
			break;
		}
	} else if (1 == which_func) {
		if (FLASH_TYPE_MAIN == type) {
			max_index = ARRAY_SIZE(main_time);
			if ( 0 < max_index
				&& element->index < max_index
				&& (element->val == main_time[element->index].val)) {
				ret = 0;
			}
		} else {
			printk(KERN_ERR"%s not support type!\n", __func__);
		}
	} else {
		printk(KERN_ERR"%s not support func!\n", __func__);
	}

	printk(KERN_INFO"%s ret=%d\n", __func__, ret);

	return ret;
}


int sprd_flash_set_charge(uint8_t type, struct sprd_flash_element *element)
{
	int ret = 0;

	if (NULL == element) {
		printk(KERN_ERR"%s param is error!!!\n", __func__);
		return -1;
	}

	if (sprd_flash_check_param(0, type, element)) {
		printk(KERN_ERR"%s check param failed!\n", __func__);
		return -1;
	}

	switch (type) {
	case FLASH_TYPE_PREFLASH:
		sm5701_led_ready(MOVIE_MODE);
		sm5701_set_imled(element->index);
		break;
	case FLASH_TYPE_MAIN:
		sm5701_led_ready(FLASH_MODE);
		sm5701_set_ifled(element->index);
		break;
	default:
		printk(KERN_ERR"%s not support type!\n", __func__);
		break;
	}


	return ret;
}

int sprd_flash_set_time(uint8_t type, struct sprd_flash_element *element)
{
	int ret = 0;

	if (NULL == element) {
		printk(KERN_ERR"%s param is error!!!\n", __func__);
		return -1;
	}
	if (sprd_flash_check_param(1, type, element)) {
		printk(KERN_ERR"%s check param failed!\n", __func__);
		return -1;
	}

	switch (type) {
	case FLASH_TYPE_MAIN:
		sm5701_led_ready(FLASH_MODE);

		sm5701_set_noneshot(0);
		sm5701_set_onetimer(element->index);
		break;
	default:
		printk(KERN_ERR"%s not support type!\n",__func__);
		break;
	}

	return ret;
}

int sprd_flash_get_charge(uint8_t type, uint8_t *def_val,uint8_t *count, struct sprd_flash_element *element)
{
	int ret = 0;
	int max_index = 0;
	int ref_val = 0;

	if (NULL == def_val || NULL == element || NULL == count) {
		printk(KERN_ERR"%s param %p %p %p is error!!!\n", __func__, def_val, element, count);
		return -1;
	}


	switch (type) {
	case FLASH_TYPE_PREFLASH:
		flash_get_max_index(max_index, *count, preflash_charge);
		*count = max_index;
		memcpy(element, preflash_charge, max_index * sizeof(*element));
		sm5701_get_imled(&ref_val);
		if (ref_val > max_index)
			ref_val = max_index;
		*def_val = ref_val;
		break;
	case FLASH_TYPE_MAIN:
		flash_get_max_index(max_index, *count, main_charge);
		*count = max_index;
		memcpy(element, main_charge, max_index * sizeof(*element));
		sm5701_get_ifled(&ref_val);
		if (ref_val > max_index)
			ref_val = max_index;
		*def_val = ref_val;
		break;
	default:
		printk(KERN_ERR"%s not support type!\n", __func__);
		break;
	}


	return ret;
}

int sprd_flash_get_time(uint8_t type, uint8_t *count, struct sprd_flash_element *element)
{
	int ret = 0;
	int max_index = 0;

	if (NULL == element) {
		printk(KERN_ERR"%s param is error!!!\n", __func__);
		return -1;
	}

	switch (type) {
	case FLASH_TYPE_MAIN:
		flash_get_max_index(max_index, *count, main_time);
		*count = max_index;
		memcpy(element, main_time, max_index * sizeof(*element));
		break;
	default:
		printk(KERN_ERR"%s not support type!\n", __func__);
		break;
	}

	return ret;
}

int sprd_flash_get_max_capacity(uint16_t *max_charge, uint16_t *max_time)
{
	int ret = 0;
	int max_index = 0;

	if (NULL == max_charge || NULL == max_time) {
		printk(KERN_ERR"%s param %p %p is error!!!\n", __func__, max_charge, max_time);
		return -1;
	}

	flash_get_max_index(max_index, ARRAY_SIZE(main_charge), main_charge);
	if (max_index) {
		*max_charge = main_charge[max_index - 1].val;
	}
	flash_get_max_index(max_index, ARRAY_SIZE(main_time), main_time);
	if (max_index) {
		*max_time = main_time[max_index - 1].val;
	}

	return ret;
}

int sprd_flash_cfg(struct sprd_flash_cfg_param *param, void *arg)
{
	int ret = 0;

	if (NULL == param || NULL == param->data) {
		printk(KERN_ERR"%s param %p %p is error!!!\n", __func__, param, param->data);
		return -1;
	}

	switch (param->io_id) {
	case FLASH_IOID_GET_CHARGE: {
		struct sprd_flash_cell *user_cell = NULL;
		struct sprd_flash_cell real_cell;

		user_cell = (struct sprd_flash_cell *)param->data;
		if (!user_cell) {
			ret = -1;
			printk(KERN_ERR"%s param get charge is NULL !!!\n", __func__);
			goto out;
		}
		memset(&real_cell, 0, sizeof(real_cell));
		real_cell.type = user_cell->type;
		real_cell.count = user_cell->count;
		ret = sprd_flash_get_charge(real_cell.type, &real_cell.def_val, &real_cell.count, real_cell.element);
		if (0 == ret)
			ret = copy_to_user(user_cell, &real_cell, sizeof(real_cell));
	}
		break;
	case FLASH_IOID_GET_TIME: {
		struct sprd_flash_cell *user_cell = NULL;
		struct sprd_flash_cell real_cell;

		user_cell = (struct sprd_flash_cell *)param->data;
		if (!user_cell) {
			ret = -1;
			printk(KERN_ERR"%s param get time is NULL !!!\n", __func__);
			goto out;
		}
		memset(&real_cell, 0, sizeof(real_cell));
		real_cell.type = user_cell->type;
		real_cell.count = user_cell->count;
		ret = sprd_flash_get_time(real_cell.type, &real_cell.count, real_cell.element);
		if (0 == ret)
			ret = copy_to_user(user_cell, &real_cell, sizeof(real_cell));
	}
		break;
	case FLASH_IOID_SET_TIME: {
		struct sprd_flash_cell *user_cell = NULL;

		user_cell = (struct sprd_flash_cell *)param->data;
		if (!user_cell) {
			ret = -1;
			printk(KERN_ERR"%s param set time is NULL !!!\n", __func__);
			goto out;
		}
		ret = sprd_flash_set_time(user_cell->type, user_cell->element);
	}
		break;
	case FLASH_IOID_SET_CHARGE: {
		struct sprd_flash_cell *user_cell = NULL;

		user_cell = (struct sprd_flash_cell *)param->data;
		if (!user_cell) {
			ret = -1;
			printk(KERN_ERR"%s param set charge is NULL !!!\n", __func__);
			goto out;
		}
		ret = sprd_flash_set_charge(user_cell->type, user_cell->element);
	}
		break;
	case FLASH_IOID_GET_MAX_CAPACITY: {
		struct sprd_flash_capacity *user_capacity = NULL;
		struct sprd_flash_capacity real_capacity;

		user_capacity = (struct sprd_flash_capacity *)param->data;
		if (!user_capacity) {
			ret = -1;
			printk(KERN_ERR"%s param capacity is NULL !!!\n", __func__);
			goto out;
		}
		ret = sprd_flash_get_max_capacity(&real_capacity.max_charge, &real_capacity.max_time);
		if (0 == ret)
			ret = copy_to_user(user_capacity, &real_capacity, sizeof(real_capacity));
	}
		break;
	default:
		break;
	}

out:

	return ret;
}

