/*
 * @file mstdrv_transmit_nonsecure.c
 * @brief MST Nonsecure Transmit
 * Copyright (c) 2020, Samsung Electronics Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mstdrv_transmit_nonsecure.h"

/**
 * mst_change - change MST gpio pins based on input and current data bit
 * @mst_val: data bit to set
 * @mst_stat: current data bit
 */
int mst_change(int mst_val, int mst_stat, int mst_data)
{
	if (mst_stat) {
		gpio_set_value(mst_data, ON);
		if (mst_val) {
			udelay((MST_DATA_TIME_DURATION / 2));
			gpio_set_value(mst_data, OFF);
			udelay((MST_DATA_TIME_DURATION / 2));
			mst_stat = ON;
		} else {
			udelay(MST_DATA_TIME_DURATION);
			mst_stat = OFF;
		}
	} else {
		gpio_set_value(mst_data, OFF);
		if (mst_val) {
			udelay((MST_DATA_TIME_DURATION / 2));
			gpio_set_value(mst_data, ON);
			udelay((MST_DATA_TIME_DURATION / 2));
			mst_stat = OFF;
		} else {
			udelay(MST_DATA_TIME_DURATION);
			mst_stat = ON;
		}
	}
	return mst_stat;
}

/**
 * transmit_mst_data - Transmit test track data
 * @track: 1:track1, 2:track2
 */
int transmit_mst_data(int track, int mst_en, int mst_data, spinlock_t event_lock)
{
	int ret_val = 1;
	int i = 0;
	int j = 0;
	int mst_stat = 0;
	unsigned long flags;
	char midstring[] = { 69, 98, 21, 84, 82, 84, 81, 88, 16, 22, 22,
		81, 87, 16, 21, 88, 88, 22, 62, 104, 117, 97,
		110, 103, 79, 37, 110, 121, 97, 110, 103, 64,
		64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62,
		81, 82, 81, 16, 81, 16, 81, 16, 16, 16, 16, 16,
		16, 82, 21, 25, 16, 81, 16, 16, 16, 16, 16, 16,
		19, 88, 19, 16, 16, 16, 16, 16, 16, 31, 70
	};

	char midstring2[] = { 11, 22, 16, 1, 16, 21, 22, 7, 8, 19, 22, 19,
		25, 2, 25, 8, 8, 13, 2, 21, 16, 1, 16, 16, 16,
		21, 16, 16, 16, 16, 22, 16, 16, 7, 19, 25, 19, 16, 31, 13
	};

	gpio_set_value(mst_data, OFF);
	gpio_set_value(mst_en, OFF);
	//gpio_set_value(mst_pwr_en, OFF);
	udelay(300);
	//gpio_set_value(mst_pwr_en, ON);
	gpio_set_value(mst_en, ON);
	msleep(PWR_EN_WAIT_TIME);

	if (track == TRACK1) {
		spin_lock_irqsave(&event_lock, flags);
		for (i = 0; i < LEADING_ZEROES; i++) {
			mst_stat = mst_change(OFF, mst_stat, mst_data);
		}

		i = 0;
		do {
			for (j = 0; j < BITS_PER_CHAR_TRACK1; j++) {
				if (((midstring[i] & (1 << j)) >> j) == 1) {
					mst_stat = mst_change(ON, mst_stat, mst_data);
				} else {
					mst_stat = mst_change(OFF, mst_stat, mst_data);
				}
			}
			i++;
		} while ((i < sizeof(midstring))
				&& (midstring[i - 1] != LRC_CHAR_TRACK1));

		for (i = 0; i < TRAILING_ZEROES; i++) {
			mst_stat = mst_change(OFF, mst_stat, mst_data);
		}

		spin_unlock_irqrestore(&event_lock, flags);
		gpio_set_value(mst_en, OFF);
		gpio_set_value(mst_data, OFF);
		//gpio_set_value(mst_pwr_en, OFF);
	} else if (track == TRACK2) {
		spin_lock_irqsave(&event_lock, flags);
		for (i = 0; i < LEADING_ZEROES; i++) {
			mst_stat = mst_change(OFF, mst_stat, mst_data);
		}

		i = 0;
		do {
			for (j = 0; j < BITS_PER_CHAR_TRACK2; j++) {
				if (((midstring2[i] & (1 << j)) >> j) == 1) {
					mst_stat = mst_change(ON, mst_stat, mst_data);
				} else {
					mst_stat = mst_change(OFF, mst_stat, mst_data);
				}
			}
			i++;
		} while ((i < sizeof(midstring2)));

		for (i = 0; i < TRAILING_ZEROES; i++) {
			mst_stat = mst_change(OFF, mst_stat, mst_data);
		}

		spin_unlock_irqrestore(&event_lock, flags);
		gpio_set_value(mst_en, OFF);
		gpio_set_value(mst_data, OFF);
		//gpio_set_value(mst_pwr_en, OFF);
	} else {
		ret_val = 0;
	}

	return ret_val;
}

MODULE_AUTHOR("dong-uk.seo@samsung.com");
MODULE_DESCRIPTION("MST QC/LSI combined driver for NONSECURE transmit");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
