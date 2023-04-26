// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <exynos_drm_hdr.h>
#include <exynos_drm_decon.h>
#include <exynos_drm_dpp.h>
#include <hdr_cal.h>
#include <dpp_cal.h>

static int dpu_hdr_log_level = 6;
module_param(dpu_hdr_log_level, int, 0600);
MODULE_PARM_DESC(dpu_hdr_log_level, "log level for dpu hdr [default : 6]");

#define HDR_DRIVER_NAME "exynos-drmhdr"
#define hdr_info(hdr, fmt, ...)	\
dpu_pr_info(HDR_DRIVER_NAME, (hdr)->id, dpu_hdr_log_level, fmt, ##__VA_ARGS__)

#define hdr_warn(hdr, fmt, ...)	\
dpu_pr_warn(HDR_DRIVER_NAME, (hdr)->id, dpu_hdr_log_level, fmt, ##__VA_ARGS__)

#define hdr_err(hdr, fmt, ...)	\
dpu_pr_err(HDR_DRIVER_NAME, (hdr)->id, dpu_hdr_log_level, fmt, ##__VA_ARGS__)

#define hdr_debug(hdr, fmt, ...)\
dpu_pr_debug(HDR_DRIVER_NAME, (hdr)->id, dpu_hdr_log_level, fmt, ##__VA_ARGS__)

struct hdr_coef_header {
	unsigned int total_bytesize;
	union hdr_coef_header_type {
		struct hdr_coef_header_type_unpack {
			unsigned char hw_type; // 0: None, 1: S.LSI, 2: Custom
			unsigned char layer_index;
			unsigned char log_level;
			unsigned char optional_flag;
		} unpack;
		unsigned int pack;
	} type;
	union hdr_coef_header_con {
		struct hdr_coef_header_con_unpack {
			unsigned char mul_en; /* de-multiplication -> HDR -> re-multiplication */
			unsigned char mod_en; /* HDR module on */
			unsigned char reserved[2];
		} unpack;
		unsigned int pack;
	} sfr_con; // uses this for other sfr_con, HDR con would move to lut part
	union hdr_coef_header_lut {
		struct hdr_coef_header_lut_unpack {
			unsigned short shall; // # of shall group
			unsigned short need; // # of need group
		} unpack;
		unsigned int pack;
	} num;
};

#define HDR_LUT_MAGIC 0xDADADADA

struct hdr_lut_header {
	unsigned int byte_offset;
	unsigned int length;
	unsigned int magic;
};

enum hdr_dummy_type {
	HDR_DUMMY_BASE_OFFSET,
	HDR_DUMMY_LOG_LEVEL,
	HDR_DUMMY_CON0_OFFSET,
	HDR_DUMMY_CON0_VALUE,
	HDR_DUMMY_CON1_OFFSET,
	HDR_DUMMY_CON1_VALUE,
	HDR_DUMMY_CON2_OFFSET,
	HDR_DUMMY_CON2_VALUE,
	HDR_DUMMY_CON3_OFFSET,
	HDR_DUMMY_CON3_VALUE,
	HDR_DUMMY_CON4_OFFSET,
	HDR_DUMMY_CON4_VALUE,
	HDR_DUMMY_MAX,
	HDR_DUMMY_CON_NUM = (HDR_DUMMY_MAX - HDR_DUMMY_CON0_OFFSET) / 2,
};

static uint dpu_hdr_dummy_ctx[HDR_DUMMY_MAX];
static uint dpu_hdr_dummy_ctx_num;
module_param_array(dpu_hdr_dummy_ctx, uint, &dpu_hdr_dummy_ctx_num, 0600);
MODULE_PARM_DESC(dpu_hdr_dummy_ctx, "set dummy context for HDR path test");

#define HDR_DUMMY_CTX_SIZE(n) (sizeof(struct hdr_coef_header) + \
	(sizeof(struct hdr_lut_header) + sizeof(dpu_hdr_dummy_ctx[0])) * (n))

static inline bool IS_HDR_TEST_MODE(void)
{
	return (dpu_hdr_dummy_ctx_num > HDR_DUMMY_CON0_OFFSET);
}

static char hdr_dump_buf[MAX_DPP_CNT][PAGE_SIZE];
static uint hdr_dump_off;
static uint hdr_dump_size;
static int hdr_set_dump_buf(const char *buf, const struct kernel_param *kp)
{
	int ret, i;
	uint offset, size;

	ret = sscanf(buf, "%x,%x", &offset, &size);
	if (ret != 2)
		offset = size = 0;

	for (i = 0; i < ARRAY_SIZE(hdr_dump_buf); ++i)
		hdr_dump_buf[i][0] = '\0';
	hdr_dump_off = offset;
	hdr_dump_size = size;

	return param_set_charp(buf, kp);
}

static int hdr_get_dump_buf(char *buf, const struct kernel_param *kp)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hdr_dump_buf); i++) {
		pr_info("=== HDR_L%d DUMP 0x%x ~ 0x%x ===\n", i,
			hdr_dump_off, hdr_dump_off + hdr_dump_size);
		pr_info("%s", hdr_dump_buf[i]);
	}

	return param_get_charp(buf, kp);
}

static const struct kernel_param_ops dpu_hdr_save_dump_ops = {
	.set = hdr_set_dump_buf,
	.get = hdr_get_dump_buf,
};

static char param[128];
module_param_cb(dpu_hdr_save_dump, &dpu_hdr_save_dump_ops, param, 0644);
__MODULE_PARM_TYPE(dpu_hdr_save_dump, "uint");
MODULE_PARM_DESC(dpu_hdr_save_dump, "Save HDR dump to buffer from next update");

static struct class *hdr_cls;
static struct device_attribute *hdr_attrs[];

#define PREFIX_LEN      40
#define ROW_LEN         32
static void hdr_print_hex_dump(struct exynos_hdr *hdr, u32 offset,
				size_t size, const void *buf)
{
	char prefix_buf[PREFIX_LEN];
	size_t i, row;

	hdr_info(hdr, "=== dump %d ===\n", size);
	for (i = 0; i < size; i += ROW_LEN) {
		if (size - i < ROW_LEN)
			row = size - i;
		else
			row = ROW_LEN;
		snprintf(prefix_buf, sizeof(prefix_buf), "HDR:[%08X] ", offset + (u32)i);
		print_hex_dump(KERN_INFO, prefix_buf, DUMP_PREFIX_NONE,
				32, 4, buf + i, row, false);
	}
}

static void hdr_dump_to_buf(struct exynos_hdr *hdr)
{
	size_t i, row, len;
	const void *in_buf = hdr->base_regs.regs + hdr_dump_off;
	size_t in_size = hdr_dump_size;
	char line_buf[256];
	size_t line_size = sizeof(line_buf);
	char *out_buf;
	size_t out_size;

	if (!hdr || hdr->id >= ARRAY_SIZE(hdr_dump_buf))
		return;

	if ((hdr_dump_off + hdr_dump_size) > hdr->base_regs.size)
		return;

	out_buf = hdr_dump_buf[hdr->id];
	out_size = sizeof(hdr_dump_buf[hdr->id]);

	if (!in_buf || !out_buf)
		return;

	out_buf[0] = '\0';
	for (i = 0, len = 0; i < in_size; i += ROW_LEN) {
		if (in_size - i < ROW_LEN)
			row = in_size - i;
		else
			row = ROW_LEN;
		hex_dump_to_buffer(in_buf + i, row, ROW_LEN, 4, line_buf, line_size, false);

		len += snprintf(out_buf + len, out_size - len, "%s\n", line_buf);
		if (len >= out_size)
			break;
	}
	out_buf[out_size - 1] = '\0';
}

static int hdr_import_buffer(struct exynos_hdr *hdr,
				const struct exynos_drm_plane_state *exynos_plane_state)
{
	struct dma_buf *buf = NULL;
	struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR(NULL);
	int ret;
	int64_t hdr_fd;

	hdr_debug(hdr, "+\n");

	dma_buf_map_clear(&map);
	hdr_fd = exynos_plane_state->hdr_fd;
	if (hdr_fd < 3) {
		if (hdr_fd <= 0) // reset value of hdr_fd is 0
			hdr_debug(hdr, "not allocated hdr_fd [%lld]\n", hdr_fd);
		else
			hdr_err(hdr, "invalid hdr_fd [%lld]\n", hdr_fd);
		goto error;
	}

	buf = dma_buf_get(hdr_fd);
	if (IS_ERR_OR_NULL(buf)) {
		if ((hdr_fd == hdr->hdr_fd) && IS_ERR(buf)) {
			hdr_debug(hdr, "bad fd [%ld] but continued with old vbuf\n", PTR_ERR(buf));
			goto done;
		}

		hdr_warn(hdr, "failed to get dma buf [%x] of fd [%lld]\n", buf, hdr_fd);
		goto error;
	}

	if ((hdr_fd == hdr->hdr_fd) && (buf == hdr->dma_buf)) {
		dma_buf_put(buf);
		goto done;
	}

	// release old buf
	if (hdr->dma_buf) {
		if (hdr->vaddr) {
			struct dma_buf_map old_map =
				DMA_BUF_MAP_INIT_VADDR(hdr->vaddr);

			dma_buf_vunmap(hdr->dma_buf, &old_map);
		}
		dma_buf_put(hdr->dma_buf);
	}

	ret = dma_buf_vmap(buf, &map);
	if (ret) {
		hdr_err(hdr, "failed to vmap buffer [%x]\n", ret);
		goto error;
	}

	hdr->hdr_fd = hdr_fd;
	hdr->dma_buf = buf;
	hdr->vaddr = map.vaddr;
done:
	return hdr->dma_buf->size;

error:
	if (!IS_ERR_OR_NULL(buf)) {
		if (!dma_buf_map_is_null(&map))
			dma_buf_vunmap(buf, &map);
		dma_buf_put(buf);
	}
	return -1;
}

static struct hdr_context *hdr_acquire_context(struct exynos_hdr *hdr, int size)
{
	int i, ctx_no;

	for (i = 0; i < MAX_HDR_CONTEXT; i++) {
		if (!hdr->ctx[i].data)
			hdr->ctx[i].data = kzalloc(size, GFP_KERNEL);
	}

	ctx_no = (atomic_inc_return(&hdr->ctx_no) & INT_MAX) % MAX_HDR_CONTEXT;
	return &hdr->ctx[ctx_no];
}

static int hdr_dummy_remap(struct exynos_hdr *hdr)
{
	phys_addr_t addr;
	size_t size;

	if (!hdr->comm_regs.regs || !IS_HDR_TEST_MODE())
		return -EINVAL;

	addr = hdr->comm_regs.addr + dpu_hdr_dummy_ctx[HDR_DUMMY_BASE_OFFSET];
	size = hdr->comm_regs.size;

	if (hdr->base_regs.regs) {
		if (hdr->base_regs.addr == addr &&
			hdr->base_regs.size == size) {
			return 0;
		}

		iounmap(hdr->base_regs.regs);
		hdr->base_regs.regs = NULL;
		hdr->base_regs.addr = 0;
		hdr->base_regs.size = 0;
	}

	hdr->base_regs.regs = ioremap(addr, size);
	if (!hdr->base_regs.regs)
		return -EINVAL;
	hdr->base_regs.addr = addr;
	hdr->base_regs.size = size;
	hdr_regs_desc_init(hdr->id, hdr->base_regs.regs, "hdr_custom", REGS_HDR_BASE);

	hdr->initialized = true;
	hdr_info(hdr, "HDR/WCG initialized (0x%x, 0x%x)\n", addr, size);

	return 0;
}

static int hdr_dummy_buffer(struct exynos_hdr *hdr,
			struct exynos_drm_plane_state *exynos_plane_state)
{
	struct hdr_coef_header *coef_h;
	struct hdr_lut_header *lut_h;
	u32 lut_num, size, idx = HDR_DUMMY_CON0_OFFSET;
	u32 *data;
	struct hdr_context *ctx;

	if (!IS_HDR_TEST_MODE())
		return -1;

	ctx = hdr_acquire_context(hdr, HDR_DUMMY_CTX_SIZE(HDR_DUMMY_CON_NUM));
	if (!ctx || !ctx->data) {
		hdr_err(hdr, "no valid ctx\n");
		return -1;
	}

	coef_h = (struct hdr_coef_header *)ctx->data;
	coef_h->type.unpack.hw_type = HDR_HW_TYPE_2;
	coef_h->type.unpack.layer_index = hdr->id;
	coef_h->type.unpack.log_level = dpu_hdr_dummy_ctx[HDR_DUMMY_LOG_LEVEL];
	coef_h->sfr_con.unpack.mul_en = 0;
	coef_h->sfr_con.unpack.mod_en = 1;
	coef_h->num.unpack.shall = 0;
	coef_h->num.unpack.need = 0;
	lut_num = (dpu_hdr_dummy_ctx_num - HDR_DUMMY_CON0_OFFSET) / 2;
	coef_h->total_bytesize = HDR_DUMMY_CTX_SIZE(lut_num);

	size = sizeof(*coef_h);
	while (size < coef_h->total_bytesize && idx < (dpu_hdr_dummy_ctx_num - 1)) {
		coef_h->num.unpack.shall++;
		lut_h = (struct hdr_lut_header *)((char *)coef_h + size);
		lut_h->byte_offset = dpu_hdr_dummy_ctx[idx++];
		lut_h->length = 1;
		lut_h->magic = HDR_LUT_MAGIC;
		data = (u32 *)((char *)lut_h + sizeof(*lut_h));
		*data = dpu_hdr_dummy_ctx[idx++];
		size += (u32)(sizeof(*lut_h) + lut_h->length*sizeof(*data));
	}

	exynos_plane_state->hdr_en = (coef_h->sfr_con.unpack.mod_en != 0);
	exynos_plane_state->hdr_ctx = ctx->data;

	return 0;
}

static int hdr_prepare_buffer(struct exynos_hdr *hdr,
			struct exynos_drm_plane_state *exynos_plane_state)
{
	const struct hdr_coef_header *coef_h;
	struct hdr_context *ctx;
	int ret;

	ret = hdr_import_buffer(hdr, exynos_plane_state);
	if (ret < 0)
		return -1;

	coef_h = (struct hdr_coef_header *)hdr->vaddr;
	if (!coef_h) {
		hdr_err(hdr, "no allocated virtual buffer\n");
		return -1;
	}

	if (!coef_h->total_bytesize || (coef_h->total_bytesize < sizeof(*coef_h))) {
		hdr_err(hdr, "invalid size: total %d\n", coef_h->total_bytesize);
		return -1;
	}

	ctx = hdr_acquire_context(hdr, ret);
	if (!ctx || !ctx->data) {
		hdr_err(hdr, "no valid ctx\n");
		return -1;
	}

	memcpy(ctx->data, (void *)hdr->vaddr, coef_h->total_bytesize);
	exynos_plane_state->hdr_en = (coef_h->sfr_con.unpack.mod_en != 0);
	exynos_plane_state->hdr_ctx = ctx->data;

	return 0;
}

static int hdr_prepare_context(struct exynos_hdr *hdr,
			struct exynos_drm_plane_state *exynos_plane_state)
{
	int ret;

	if (IS_HDR_TEST_MODE())
		ret = hdr_dummy_buffer(hdr, exynos_plane_state);
	else
		ret = hdr_prepare_buffer(hdr, exynos_plane_state);

	return ret;
}

static int hdr_update_context(struct exynos_hdr *hdr,
			const struct exynos_drm_plane_state *exynos_plane_state)
{
	const struct hdr_coef_header *coef_h = NULL;
	const struct hdr_lut_header *lut_h;
	unsigned int size, lut_size, count = 0;
	u32 id = hdr->id;
	const u32 *data;
	unsigned char hw_type;

	if (!exynos_plane_state->hdr_ctx) {
		hdr_debug(hdr, "null hdr_ctx\n");
		return -1;
	}

	coef_h = (struct hdr_coef_header *)exynos_plane_state->hdr_ctx;
	if (!coef_h) {
		hdr_err(hdr, "no allocated virtual buffer\n");
		goto error;
	}

	if (!coef_h->total_bytesize || (coef_h->total_bytesize < sizeof(*coef_h))) {
		hdr_err(hdr, "invalid size: total %d\n", coef_h->total_bytesize);
		goto error;
	}

	if (coef_h->type.unpack.layer_index != id) {
		hdr_err(hdr, "invalid layer index %d\n", coef_h->type.unpack.layer_index);
		goto error_dump;
	}

	hw_type = coef_h->type.unpack.hw_type;
	if (hw_type == HDR_HW_TYPE_0 || hw_type >= HDR_HW_TYPE_MAX) {
		hdr_err(hdr, "invalid hw type %d\n", coef_h->type.unpack.hw_type);
		goto error_dump;
	}

	dpp_reg_set_hdr_mul_con(hdr->dpp->id, coef_h->sfr_con.unpack.mul_en);
	if (coef_h->type.unpack.log_level >= 10) {
		hdr_info(hdr, "coef: fd %lld total %d hwtype %d con %d,%d num %d+%d\n",
				hdr->hdr_fd, coef_h->total_bytesize, hw_type,
				coef_h->sfr_con.unpack.mul_en, coef_h->sfr_con.unpack.mod_en,
				coef_h->num.unpack.shall, coef_h->num.unpack.need);
	}

	size = sizeof(*coef_h);
	while (size < coef_h->total_bytesize) {
		lut_h = (struct hdr_lut_header *)((char *)coef_h + size);
		if (coef_h->type.unpack.log_level >= 10) {
			hdr_info(hdr, "lut%d/%d: magic %x, len %d off %d\n", count,
				coef_h->num.unpack.shall + coef_h->num.unpack.need,
				lut_h->magic, lut_h->length, lut_h->byte_offset);
		}

		if (lut_h->magic != HDR_LUT_MAGIC || !lut_h->length) {
			hdr_err(hdr, "invalid params [%x] len %d\n",
					lut_h->magic, lut_h->length);
			goto error_dump;
		}

		data = (const u32 *)((char *)lut_h + sizeof(*lut_h));
		size += sizeof(*lut_h);

		lut_size = (unsigned int)(lut_h->length*sizeof(*data));
		if (((lut_h->byte_offset + lut_size) > hdr->base_regs.size) ||
			(!hdr_reg_verify(id, hw_type, lut_h->byte_offset, lut_h->length))) {
			hdr_err(hdr, "invalid offset %d, size %d\n",
					lut_h->byte_offset, lut_size);
			goto error_dump;
		}
		size += lut_size;

		if (count < coef_h->num.unpack.shall ||
			hdr->state == HDR_STATE_DISABLE) {
			hdr_reg_set_lut(id, lut_h->byte_offset, lut_h->length, data);
			if (coef_h->type.unpack.log_level >= 10) {
				hdr_print_hex_dump(hdr, lut_h->byte_offset,
					lut_h->length * sizeof(*data), data);
			}
		}
		count++;
	}

	if (count != (coef_h->num.unpack.shall + coef_h->num.unpack.need)) {
		hdr_warn(hdr, "number of data %d vs %d mismatch\n", count,
				(coef_h->num.unpack.shall + coef_h->num.unpack.need));
	}

	if (coef_h->type.unpack.log_level >= 10) {
		hdr_info(hdr, "context updated %d (%d+%d)\n",
			count, coef_h->num.unpack.shall, coef_h->num.unpack.need);
	}

	return 0;
error_dump:
	hdr_print_hex_dump(hdr, 0, coef_h->total_bytesize, coef_h);
error:
	hdr_err(hdr, "parsing error\n");
	return -1;
}

static int hdr_remap_regs(struct exynos_hdr *hdr, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int i;
	u32 id = hdr->id;
	struct resource res;

	i = of_property_match_string(np, "reg-names", "hdr_lsi");
	if (i >= 0) {
		if (of_address_to_resource(np, i, &res)) {
			hdr_err(hdr, "failed to map to resource\n");
			goto err;
		}

		hdr->base_regs.regs = devm_ioremap_resource(dev, &res);
		if (!hdr->base_regs.regs) {
			hdr_err(hdr, "failed to remap hdr registers\n");
			goto err;
		}
		hdr->base_regs.addr = (phys_addr_t)res.start;
		hdr->base_regs.size = resource_size(&res);

		hdr_regs_desc_init(id, hdr->base_regs.regs, "hdr_lsi", REGS_HDR_BASE);
		hdr_info(hdr, "hdr_lsi (0x%x, 0x%x)\n", hdr->base_regs.addr, hdr->base_regs.size);
	}

	i = of_property_match_string(np, "reg-names", "hdr_comm");
	if (i >= 0) {
		if (of_address_to_resource(np, i, &res)) {
			hdr_err(hdr, "failed to map to resource\n");
			goto err;
		}

		hdr->comm_regs.regs = devm_ioremap_resource(dev, &res);
		if (!hdr->comm_regs.regs) {
			hdr_err(hdr, "failed to remap hdr registers\n");
			goto err;
		}
		hdr->comm_regs.addr = (phys_addr_t)res.start;
		hdr->comm_regs.size = resource_size(&res);
		hdr_info(hdr, "hdr_comm (0x%x, 0x%x)\n", hdr->comm_regs.addr, hdr->comm_regs.size);
	}

	return 0;

err:
	if (hdr->base_regs.regs)
		iounmap(hdr->base_regs.regs);

	if (hdr->comm_regs.regs)
		iounmap(hdr->comm_regs.regs);

	return -EINVAL;
}

static int hdr_init_resources(struct exynos_hdr *hdr, struct device *dev)
{
	int i;
	int ret = 0;
	char name[16];

	ret = hdr_remap_regs(hdr, dev);
	if (ret) {
		hdr_err(hdr, "failed to remap regs\n");
		goto err;
	}

	if (!hdr_cls)
		hdr_cls = class_create(THIS_MODULE, "hdr");
	if (IS_ERR_OR_NULL(hdr_cls)) {
		hdr_err(hdr, "failed to create hdr class\n");
		goto err;
	}

	sprintf(name, "hdr%d", hdr->id);
	hdr->dev = device_create(hdr_cls, NULL, 0, &hdr, name);
	if (IS_ERR_OR_NULL(hdr->dev)) {
		hdr_err(hdr, "failed to create hdr device\n");
		goto err;
	}

	for (i = 0; hdr_attrs[i] != NULL; i++) {
		ret = device_create_file(hdr->dev, hdr_attrs[i]);
		if (ret) {
			hdr_err(hdr, "failed to create file: %s\n",
				(*hdr_attrs[i]).attr.name);
			goto err;
		}
	}

	dev_set_drvdata(hdr->dev, hdr);

	return ret;
err:
	return -1;
}

/*============== CREATE SYSFS ===================*/
#define HDR_CREATE_SYSFS_FUNC(name) \
	static ssize_t dpp_hdr_ ## name ## _show(struct device *dev, \
				struct device_attribute *attr, char *buf) \
	{ \
		int ret = 0; \
		struct exynos_hdr *hdr = dev_get_drvdata(dev); \
		if (buf == NULL || !hdr || !hdr->initialized) \
			return -1;\
		mutex_lock(&hdr->lock); \
		hdr_debug(hdr, "+\n"); \
		ret = hdr_ ## name ## _show(hdr, buf); \
		if (ret < 0) \
			hdr_err(hdr, "err -\n"); \
		else \
			hdr_debug(hdr, "-\n"); \
		mutex_unlock(&hdr->lock); \
		return ret; \
	} \
	static ssize_t dpp_hdr_ ## name ## _store(struct device *dev, \
		struct device_attribute *attr, const char *buffer, size_t count) \
	{ \
		int ret = 0; \
		struct exynos_hdr *hdr = dev_get_drvdata(dev); \
		if (count <= 0 || buffer == NULL || !hdr || !hdr->initialized) \
			return -1;\
		mutex_lock(&hdr->lock); \
		hdr_debug(hdr, "+\n"); \
		ret = hdr_ ## name ## _store(hdr, buffer, count); \
		if (ret < 0) \
			hdr_err(hdr, "err -\n"); \
		else \
			hdr_debug(hdr, "-\n"); \
		mutex_unlock(&hdr->lock); \
		return ret; \
	} \
	static DEVICE_ATTR(name, 0660, \
					dpp_hdr_ ## name ## _show, \
					dpp_hdr_ ## name ## _store)

#define HDR_CREATE_SYSFS_FUNC_SHOW(name) \
	static ssize_t dpp_hdr_ ## name ## _show(struct device *dev, \
				struct device_attribute *attr, char *buf) \
	{ \
		int ret = 0; \
		struct exynos_hdr *hdr = dev_get_drvdata(dev); \
		if (buf == NULL || !hdr || !hdr->initialized) \
			return -1;\
		mutex_lock(&hdr->lock); \
		hdr_debug(hdr, "+\n"); \
		ret = hdr_ ## name ## _show(hdr, buf); \
		if (ret < 0) \
			hdr_err(hdr, "err -\n"); \
		else \
			hdr_debug(hdr, "-\n"); \
		mutex_unlock(&hdr->lock); \
		return ret; \
	} \
	static DEVICE_ATTR(name, 0440, \
					dpp_hdr_ ## name ## _show, NULL)

static ssize_t hdr_dump_show(struct exynos_hdr *hdr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s", hdr_dump_buf[hdr->id]);
}

HDR_CREATE_SYSFS_FUNC_SHOW(dump);

static struct device_attribute *hdr_attrs[] = {
	&dev_attr_dump,
	NULL,
};
/*============== CREATE SYSFS END ===================*/
static void __exynos_hdr_dump(struct exynos_hdr *hdr)
{
	__hdr_dump(hdr->id, &hdr->base_regs);
}

static void __exynos_hdr_prepare(struct exynos_hdr *hdr,
			struct exynos_drm_plane_state *exynos_plane_state)
{
	hdr_debug(hdr, "+\n");
	hdr_prepare_context(hdr, exynos_plane_state);
	hdr_debug(hdr, "-\n");
}

static void __exynos_hdr_update(struct exynos_hdr *hdr,
			const struct exynos_drm_plane_state *exynos_plane_state)
{
	int ret;

	hdr_debug(hdr, "+\n");
	ret = hdr_update_context(hdr, exynos_plane_state);
	if (ret == 0) {
		hdr->state = HDR_STATE_ENABLE;
		if (hdr_dump_size)
			hdr_dump_to_buf(hdr);
	}
	hdr_debug(hdr, "-\n");
}

static void __exynos_hdr_disable(struct exynos_hdr *hdr)
{
	hdr_debug(hdr, "+\n");
	hdr->state = HDR_STATE_DISABLE;
	hdr_debug(hdr, "-\n");
}

static bool __exynos_hdr_check(struct exynos_hdr *hdr)
{
	struct dpp_device *dpp = hdr->dpp;

	if (IS_HDR_TEST_MODE()) {
		hdr_dummy_remap(hdr);
	} else {
		if (!(test_bit(DPP_ATTR_HDR, &dpp->attr)
			|| test_bit(DPP_ATTR_HDR10_PLUS, &dpp->attr)
			|| test_bit(DPP_ATTR_C_HDR, &dpp->attr)
			|| test_bit(DPP_ATTR_C_HDR10_PLUS, &dpp->attr)
			|| test_bit(DPP_ATTR_WCG, &dpp->attr))) {
			return false;
		}
	}

	return (hdr->initialized && hdr->base_regs.regs);
}

static const struct exynos_hdr_funcs hdr_funcs = {
	.dump = __exynos_hdr_dump,
	.prepare = __exynos_hdr_prepare,
	.update = __exynos_hdr_update,
	.disable = __exynos_hdr_disable,
	.check = __exynos_hdr_check,
};

void exynos_hdr_dump(struct exynos_hdr *hdr)
{
	const struct exynos_hdr_funcs *funcs;

	if (!hdr)
		return;

	funcs = hdr->funcs;
	if (funcs && funcs->check(hdr))
		funcs->dump(hdr);
}

void exynos_hdr_prepare(struct exynos_hdr *hdr,
			struct exynos_drm_plane_state *exynos_plane_state)
{
	const struct exynos_hdr_funcs *funcs;

	if (!hdr || !exynos_plane_state)
		return;

	funcs = hdr->funcs;
	if (funcs && funcs->check(hdr))
		funcs->prepare(hdr, exynos_plane_state);
}

void exynos_hdr_update(struct exynos_hdr *hdr,
			const struct exynos_drm_plane_state *exynos_plane_state)
{
	const struct exynos_hdr_funcs *funcs;

	if (!hdr || !exynos_plane_state)
		return;

	funcs = hdr->funcs;
	if (funcs && funcs->check(hdr))
		funcs->update(hdr, exynos_plane_state);
}

void exynos_hdr_disable(struct exynos_hdr *hdr)
{
	const struct exynos_hdr_funcs *funcs;

	if (!hdr)
		return;

	funcs = hdr->funcs;
	if (funcs && funcs->check(hdr))
		funcs->disable(hdr);
}

struct exynos_hdr *exynos_hdr_register(struct dpp_device *dpp)
{
	struct exynos_hdr *hdr;
	int ret;

	if (!dpp) {
		pr_err("%s dpp is NULL\n", __func__);
		return NULL;
	}

	hdr = devm_kzalloc(dpp->dev, sizeof(struct exynos_hdr), GFP_KERNEL);
	if (!hdr)
		return NULL;

	hdr->dpp = dpp;
	hdr->id = dpp->id;

	ret = hdr_init_resources(hdr, dpp->dev);
	if (ret) {
		hdr_err(hdr, "failed to init resources\n");
		return NULL;
	}

	mutex_init(&hdr->lock);
	init_waitqueue_head(&hdr->wait_update);
	atomic_set(&hdr->ctx_no, 0);
	hdr->funcs = &hdr_funcs;
	hdr->state = HDR_STATE_DISABLE;
	if (hdr->base_regs.regs) {
		hdr->initialized = true;
		hdr_info(hdr, "HDR/WCG supported\n");
	}
	return hdr;
}
