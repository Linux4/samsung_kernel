/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cpu.h>
#include <linux/highmem.h>
#include <linux/limits.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "tzdev_internal.h"
#include "core/log.h"
#include "core/smc_channel.h"
#include "core/smc_channel_impl.h"

struct tzdev_smc_channel {
	struct tzdev_smc_channel_metadata *metadata;
	struct page **pages;
	size_t pages_count;
	size_t read_offset;
	void *impl_data;
};

struct tzdev_smc_channel_impl *impl;

#ifdef CONFIG_TZDEV_SK_MULTICORE
static DEFINE_PER_CPU(struct tzdev_smc_channel, smc_channel_var);

static struct tzdev_smc_channel *smc_channel_get(void)
{
	return &get_cpu_var(smc_channel_var);
}

static void smc_channel_put(void)
{
	put_cpu_var(smc_channel_var);
}

static struct tzdev_smc_channel *smc_channel_ptr(int cpu)
{
	return per_cpu_ptr(&smc_channel_var, cpu);
}
#else /* !CONFIG_TZDEV_SK_MULTICORE */
static DEFINE_MUTEX(smc_channel_lock);
static struct tzdev_smc_channel smc_channel_var;

static struct tzdev_smc_channel *smc_channel_get(void)
{
	mutex_lock(&smc_channel_lock);
	return &smc_channel_var;
}

static void smc_channel_put(void)
{
	mutex_unlock(&smc_channel_lock);
}

static struct tzdev_smc_channel *smc_channel_ptr(int cpu)
{
	return &smc_channel_var;
}
#endif /* CONFIG_TZDEV_SK_MULTICORE */

/**
 * tzdev_smc_channel_acquire() - Acquire smc channel.
 *
 * Return: smc channel.
 */
struct tzdev_smc_channel *tzdev_smc_channel_acquire(void)
{
	struct tzdev_smc_channel *channel = smc_channel_get();

	channel->read_offset = 0;
	impl->acquire(channel->metadata);

	return channel;
}

/**
 * tzdev_smc_channel_release() - Release smc channel.
 */
void tzdev_smc_channel_release(struct tzdev_smc_channel *channel)
{
	int i;
	int ret;

	/* Free additionally allocated pages */
	ret = impl->release(channel->metadata, channel->impl_data);
	if (ret != -EADDRNOTAVAIL)
		for (i = CONFIG_TZDEV_SMC_CHANNEL_PERSISTENT_PAGES; i < channel->pages_count; i++)
			__free_page(channel->pages[i]);

	channel->pages_count = CONFIG_TZDEV_SMC_CHANNEL_PERSISTENT_PAGES;

	smc_channel_put();
}

/**
 * tzdev_smc_channel_reserve() - Reserve space in smc channel.
 * @channel: smc channel.
 * @size: size to reserve.
 *
 * If count of free memory below than size then it allocates additional pages.
 *
 * Return: 0 on success and error on failure.
 */
int tzdev_smc_channel_reserve(struct tzdev_smc_channel *channel, size_t size)
{
	int ret;
	size_t i, old_pages_count, new_pages_count;
	struct page **pages;

	old_pages_count = channel->pages_count;
	new_pages_count = PAGE_ALIGN(impl->get_write_offset(channel->metadata) + size) / PAGE_SIZE;

	if (old_pages_count >= new_pages_count)
		return 0;

	pages = krealloc(channel->pages, new_pages_count * sizeof(struct page), GFP_ATOMIC);
	if (!pages)
		return -ENOMEM;
	channel->pages = pages;

	/* Allocate additional pages and write their pfns to meta page */
	for (i = old_pages_count; i < new_pages_count; i++) {
		pages[i] = alloc_page(GFP_ATOMIC);
		if (!pages[i]) {
			ret = -ENOMEM;
			goto free_additional_pages;
		}
	}

	ret = impl->reserve(channel->metadata, pages, old_pages_count, new_pages_count,
			channel->impl_data);
	if (ret) {
		log_error(tzdev_smc_channel, "Failed to reserve mem, error=%d\n", ret);
		goto free_additional_pages;
	}

	channel->pages_count = new_pages_count;

	return 0;

free_additional_pages:
	if (ret != -EADDRNOTAVAIL) {
		for (i = old_pages_count; i < new_pages_count; i++) {
			if (!pages[i])
				break;
			__free_page(pages[i]);
		}
	}
	kfree(pages);

	return -ENOMEM;
}

/**
 * tzdev_smc_channel_write() - Write data into smc channel.
 * @channel: smc channel.
 * @buffer: data buffer to write.
 * @size: buffer's size.
 *
 * If count of free memory below than size then it allocates additional pages.
 *
 * Return: 0 on success and error on failure.
 */
int tzdev_smc_channel_write(struct tzdev_smc_channel *channel, const void *buffer, size_t size)
{
	size_t i, n, off, page_num, page_off;
	char *vaddr;
	int ret;

	ret = tzdev_smc_channel_reserve(channel, size);
	if (ret)
		return ret;

	if (channel->read_offset) {
		channel->read_offset = 0;
		impl->set_write_offset(channel->metadata, 0);
	}

	off = impl->get_write_offset(channel->metadata);
	for (i = 0; i < size; i += n) {
		page_num = (off + i) >> PAGE_SHIFT;
		page_off = offset_in_page(off + i);
		n = min(size - i, PAGE_SIZE - page_off);

		vaddr = kmap_atomic(channel->pages[page_num]);
		memcpy(vaddr + page_off, (char *)buffer + i, n);
		kunmap_atomic(vaddr);
	}

	impl->set_write_offset(channel->metadata, off + size);

	return 0;
}

/**
 * tzdev_smc_channel_read() - Read data from smc channel.
 * @channel: smc channel.
 * @buffer: buffer for received data.
 * @size: count of data to read.
 *
 * Return: 0 on success and error on failure.
 */
int tzdev_smc_channel_read(struct tzdev_smc_channel *channel, void *buffer, size_t size)
{
	size_t i, n, off, page_num, page_off;
	char *vaddr;

	if ((channel->read_offset + size) > impl->get_write_offset(channel->metadata))
		return -EMSGSIZE;

	off = channel->read_offset;
	for (i = 0; i < size; i += n) {
		page_num = (off + i) >> PAGE_SHIFT;
		page_off = offset_in_page(off + i);
		n = min(size - i, PAGE_SIZE - page_off);

		vaddr = kmap_atomic(channel->pages[page_num]);
		memcpy((char *)buffer + i, vaddr + page_off, n);
		kunmap_atomic(vaddr);
	}

	channel->read_offset += size;
	return 0;
}

/**
 * tzdev_smc_channel_init() - Init smc channels subsystem.
 *
 * It must be initialized before subsystems that use smc channels.
 *
 * Return: 0 on success.
 */
int tzdev_smc_channel_init(void)
{
	struct tzdev_smc_channel *channel;
	struct page *meta_pages;
	unsigned int order;
	size_t i;
	int ret, cpu;

	impl = tzdev_get_smc_channel_impl();

	order = order_base_2(NR_SW_CPU_IDS);
	meta_pages = alloc_pages(GFP_KERNEL, order);
	if (!meta_pages)
		return 1;

	for (cpu = 0; cpu < NR_SW_CPU_IDS; cpu++) {
		channel = smc_channel_ptr(cpu);

		channel->metadata = (void *)((char *)page_address(meta_pages) + PAGE_SIZE * cpu);
		if (!channel->metadata) {
			log_error(tzdev_smc_channel, "Failed to allocate meta page\n");
			goto free_channels;
		}

		channel->pages_count = CONFIG_TZDEV_SMC_CHANNEL_PERSISTENT_PAGES;
		channel->pages = kmalloc_array(channel->pages_count, sizeof(struct page),
				GFP_KERNEL | __GFP_ZERO);
		if (!channel->pages) {
			log_error(tzdev_smc_channel, "Failed to allocate data pages array\n");
			goto free_channels;
		}

		for (i = 0; i < channel->pages_count; i++) {
			channel->pages[i] = alloc_page(GFP_KERNEL);
			if (!channel->pages[i]) {
				log_error(tzdev_smc_channel, "Failed to allocate data pages\n");
				goto free_channels;
			}
		}

		if (impl->data_size) {
			channel->impl_data = kzalloc(impl->data_size, GFP_KERNEL);
			if (!channel->impl_data) {
				log_error(tzdev_smc_channel, "Failed to alloc impl data\n");
				goto free_channels;
			}
		}

		ret = impl->init(channel->metadata, channel->pages, channel->pages_count,
				channel->impl_data);
		if (ret) {
			log_error(tzdev_smc_channel, "Failed to init metadata (error %d)\n", ret);
			goto free_channels;
		}
	}

	ret = impl->init_swd(meta_pages);
	if (ret) {
		log_error(tzdev_smc_channel, "Failed to init smc channels (error %d)", ret);
		goto free_channels;
	}

	return 0;

free_channels:
	for (; cpu >= 0; cpu--) {
		channel = smc_channel_ptr(cpu);

		if (impl->data_size && !channel->impl_data)
			continue;

		ret = impl->deinit(channel->impl_data);
		if (ret != -EADDRNOTAVAIL) {
			for (i = 0; i < channel->pages_count; i++) {
				if (!channel->pages[i])
					break;
				__free_page(channel->pages[i]);
			}
		}
		kfree(channel->impl_data);
		kfree(channel->pages);

		memset(channel, 0, sizeof(struct tzdev_smc_channel));
	}
	__free_pages(meta_pages, order);
	return 1;
}
