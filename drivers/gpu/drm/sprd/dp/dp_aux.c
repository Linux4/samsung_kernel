// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 Synopsys, Inc.
 * Copyright (C) 2020 Unisoc Inc.
 */

#include "dw_dptx.h"

static int dptx_handle_aux_reply(struct dptx *dptx)
{
	u32 auxsts;
	u32 status;
	u32 auxm;
	u32 br;
	int count = 0;

	while (1) {
		auxsts = dptx_readl(dptx, DPTX_AUX_STS);

		if (!(auxsts & DPTX_AUX_STS_REPLY_RECEIVED))
			break;

		count++;
		if (count > 5000)
			return -ETIMEDOUT;

		udelay(1);
	}

	auxsts = dptx_readl(dptx, DPTX_AUX_STS);

	status = (auxsts & DPTX_AUX_STS_STATUS_MASK) >>
		DPTX_AUX_STS_STATUS_SHIFT;

	auxm = (auxsts & DPTX_AUX_STS_AUXM_MASK) >>
		DPTX_AUX_STS_AUXM_SHIFT;
	br = (auxsts & DPTX_AUX_STS_BYTES_READ_MASK) >>
		DPTX_AUX_STS_BYTES_READ_SHIFT;

	DRM_DEBUG("%s: 0x%x, sts=%d, auxm=%d, br=%d\n",
		     __func__, auxsts, status, auxm, br);

	switch (status) {
	case DPTX_AUX_STS_STATUS_ACK:
		DRM_DEBUG("%s: DPTX_AUX_STS_STATUS_ACK\n", __func__);
		break;
	case DPTX_AUX_STS_STATUS_NACK:
		DRM_DEBUG("%s: DPTX_AUX_STS_STATUS_NACK\n", __func__);
		break;
	case DPTX_AUX_STS_STATUS_DEFER:
		DRM_DEBUG("%s: DPTX_AUX_STS_STATUS_DEFER\n", __func__);
		break;
	case DPTX_AUX_STS_STATUS_I2C_NACK:
		DRM_DEBUG("%s: DPTX_AUX_STS_STATUS_I2C_NACK\n",
			     __func__);
		break;
	case DPTX_AUX_STS_STATUS_I2C_DEFER:
		DRM_DEBUG("%s: DPTX_AUX_STS_STATUS_I2C_DEFER\n",
			     __func__);
		break;
	default:
		DRM_ERROR("Invalid AUX status 0x%x\n", status);
		break;
	}

	dptx->aux.data[0] = dptx_readl(dptx, DPTX_AUX_DATA0);
	dptx->aux.data[1] = dptx_readl(dptx, DPTX_AUX_DATA1);
	dptx->aux.data[2] = dptx_readl(dptx, DPTX_AUX_DATA2);
	dptx->aux.data[3] = dptx_readl(dptx, DPTX_AUX_DATA3);
	dptx->aux.sts = auxsts;

	return 0;
}

static void dptx_aux_clear_data(struct dptx *dptx)
{
	dptx_writel(dptx, DPTX_AUX_DATA0, 0);
	dptx_writel(dptx, DPTX_AUX_DATA1, 0);
	dptx_writel(dptx, DPTX_AUX_DATA2, 0);
	dptx_writel(dptx, DPTX_AUX_DATA3, 0);
}

static int dptx_aux_read_data(struct dptx *dptx, u8 *bytes, u32 len)
{
	int i;
	u32 *data = dptx->aux.data;

	for (i = 0; i < len; i++)
		bytes[i] = (data[i / 4] >> ((i % 4) * 8)) & 0xff;

	return len;
}

static int dptx_aux_write_data(struct dptx *dptx, u8 const *bytes,
			       u32 len)
{
	int i;
	u32 data[4];

	memset(data, 0, sizeof(u32) * 4);

	for (i = 0; i < len; i++)
		data[i / 4] |= (bytes[i] << ((i % 4) * 8));

	dptx_writel(dptx, DPTX_AUX_DATA0, data[0]);
	dptx_writel(dptx, DPTX_AUX_DATA1, data[1]);
	dptx_writel(dptx, DPTX_AUX_DATA2, data[2]);
	dptx_writel(dptx, DPTX_AUX_DATA3, data[3]);

	return len;
}

static int dptx_aux_rw(struct dptx *dptx,
		       bool rw, bool i2c, bool mot,
		       bool addr_only, u32 addr,
		       u8 *bytes, u32 len)
{
	int retval;
	int tries = 0;
	u32 auxcmd, type, status, br;

again:
	mdelay(1);
	tries++;
	if (tries > 20) {
		DRM_ERROR("AUX exceeded retries\n");
		return -EINVAL;
	}

	DRM_DEBUG("%s: addr=0x%08x, len=%d, try=%d\n",
		     __func__, addr, len, tries);

	if ((len > 16) || (len == 0)) {
		DRM_ERROR("AUX read/write len must be 1-15, len=%d\n",
			len);
		return -EINVAL;
	}

	type = rw ? DPTX_AUX_CMD_TYPE_READ : DPTX_AUX_CMD_TYPE_WRITE;

	if (!i2c)
		type |= DPTX_AUX_CMD_TYPE_NATIVE;

	if (i2c && mot)
		type |= DPTX_AUX_CMD_TYPE_MOT;

	dptx_aux_clear_data(dptx);

	if (!rw)
		dptx_aux_write_data(dptx, bytes, len);

	auxcmd = ((type << DPTX_AUX_CMD_TYPE_SHIFT) |
		  (addr << DPTX_AUX_CMD_ADDR_SHIFT) |
		  ((len - 1) << DPTX_AUX_CMD_REQ_LEN_SHIFT));

	if (addr_only)
		auxcmd |= DPTX_AUX_CMD_I2C_ADDR_ONLY;

	dptx_writel(dptx, DPTX_AUX_CMD, auxcmd);

	retval = dptx_handle_aux_reply(dptx);
	if (retval)
		return retval;

	if (atomic_read(&dptx->aux.abort)) {
		DRM_DEBUG("AUX aborted\n");
		return -ETIMEDOUT;
	}

	status = (dptx->aux.sts & DPTX_AUX_STS_STATUS_MASK) >>
		DPTX_AUX_STS_STATUS_SHIFT;

	br = (dptx->aux.sts & DPTX_AUX_STS_BYTES_READ_MASK) >>
		DPTX_AUX_STS_BYTES_READ_SHIFT;

	switch (status) {
	case DPTX_AUX_STS_STATUS_ACK:
		DRM_DEBUG("AUX Success\n");
		if (br == 0) {
			DRM_DEBUG("BR=0, Retry\n");
			dptx_soft_reset(dptx, DPTX_SRST_CTRL_AUX);
			goto again;
		}
		break;
	case DPTX_AUX_STS_STATUS_NACK:
	case DPTX_AUX_STS_STATUS_I2C_NACK:
		DRM_DEBUG("AUX Nack\n");
		return -EINVAL;
	case DPTX_AUX_STS_STATUS_I2C_DEFER:
	case DPTX_AUX_STS_STATUS_DEFER:
		DRM_DEBUG("AUX Defer\n");
		goto again;
	default:
		DRM_ERROR("AUX Status Invalid\n");
		dptx_soft_reset(dptx, DPTX_SRST_CTRL_AUX);
		goto again;
	}

	if (rw)
		dptx_aux_read_data(dptx, bytes, len);

	return 0;
}

int dptx_aux_rw_bytes(struct dptx *dptx,
		      bool rw, bool i2c, u32 addr,
		      u8 *bytes, u32 len)
{
	int retval;
	u32 i, curlen;

	for (i = 0; i < len;) {
		curlen = min_t(u32, len - i, 16);

		if (!i2c) {
			retval = dptx_aux_rw(dptx, rw, i2c, true, false,
					     addr + i, &bytes[i], curlen);
		} else {
			retval = dptx_aux_rw(dptx, rw, i2c, true, false,
					     addr, &bytes[i], curlen);
		}
		if (retval)
			return retval;

		i += curlen;
	}

	return 0;
}

int dptx_read_bytes_from_i2c(struct dptx *dptx,
			     u32 device_addr,
			     u8 *bytes,
			     u32 len)
{
	return dptx_aux_rw_bytes(dptx, true, true,
				 device_addr, bytes, len);
}

int dptx_write_bytes_to_i2c(struct dptx *dptx,
			    u32 device_addr,
			    u8 *bytes,
			    u32 len)
{
	return dptx_aux_rw_bytes(dptx, false, true,
				 device_addr, bytes, len);
}

/**
 * dptx_aux_transfer() - Send AUX transfers
 * @dptx: The dptx struct
 * @aux_msg: AUX message
 *
 * Returns result of AUX transfer on success otherwise negative errno.
 */
int dptx_aux_transfer(struct dptx *dptx, struct drm_dp_aux_msg *aux_msg)
{
	u8 req;
	void *buf;
	int len;
	u32 addr, hpdsts;

	hpdsts = dptx_readl(dptx, DPTX_HPDSTS);

	if (!(hpdsts & DPTX_HPDSTS_STATUS)) {
		DRM_ERROR("%s: Not connected\n", __func__);
		return -EINVAL;
	}

	addr = aux_msg->address;
	req = aux_msg->request;
	buf = aux_msg->buffer;
	len = aux_msg->size;

	switch (aux_msg->request & ~DP_AUX_I2C_MOT) {
	case DP_AUX_NATIVE_WRITE:
		dptx_aux_rw_bytes(dptx, false, false, addr, buf, len);
		break;
	case DP_AUX_NATIVE_READ:
		dptx_aux_rw_bytes(dptx, true, false, addr, buf, len);
		break;
	case DP_AUX_I2C_WRITE:
		dptx_write_bytes_to_i2c(dptx, addr, buf, len);
		break;
	case DP_AUX_I2C_READ:
		dptx_read_bytes_from_i2c(dptx, addr, buf, len);
		break;
	default:
		DRM_ERROR("AUX request Invalid\n");
		break;
	}

	if ((aux_msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_I2C_WRITE ||
	    (aux_msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_I2C_READ)
		aux_msg->reply = DP_AUX_I2C_REPLY_ACK;
	else if ((aux_msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_NATIVE_WRITE ||
		 (aux_msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_NATIVE_READ)
		aux_msg->reply = DP_AUX_NATIVE_REPLY_ACK;

	return len;
}
