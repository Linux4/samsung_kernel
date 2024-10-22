// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/lockdep.h>
#include <linux/time64.h>
#include <linux/kernel.h>
#include <linux/ratelimit.h>
#include <linux/pm_runtime.h>
#include <linux/rpmsg.h>
#include <linux/delay.h>
#include <linux/random.h>

#include "apusys_rv_trace.h"

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#endif

#include <mt-plat/aee.h>

#include "apu.h"
#include "apu_hw.h"
#include "apu_config.h"
#include "apu_mbox.h"
#include "apu_ipi_config.h"
#include "apu_excep.h"
#define CREATE_TRACE_POINTS
#include "apusys_rv_events.h"
#include "apu_regdump.h"

#define APUSYS_RV_IPI_HANDLE_PRINT \
	"%s: ipi_id=%d, len=%d, csum=0x%x, serial_no=%d, user_id=0x%x, " \
	"latency=%lld, elapse=%lld, t_hndlr=%llu," \
	"t_mtx_lock=%llu, t_usage_cnt_update=%lld, t_wakup=%lld\n"
#define APUSYS_RV_IPI_HANDLE_PRINT_HANDLER_EXEC_LONG \
	"%s long: ipi_id=%d, len=%d, csum=0x%x, serial_no=%d, user_id=0x%x, " \
	"latency=%lld, elapse=%lld, t_hndlr=%llu," \
	"t_mtx_lock=%llu, t_usage_cnt_update=%lld, t_wakup=%lld\n"

#if IS_ENABLED(CONFIG_DEBUG_FS)
static void apu_power_on_off_profile(u32 on, u32 off,
	uint64_t time_diff_ns, uint64_t time_diff_ns2);
#else
static void apu_power_on_off_profile(u32 on, u32 off,
	uint64_t time_diff_ns, uint64_t time_diff_ns2)
{}
#endif

static struct lock_class_key ipi_lock_key[APU_IPI_MAX];

static unsigned int tx_serial_no;
static unsigned int rx_serial_no;
unsigned int temp_buf[APU_SHARE_BUFFER_SIZE / 4];
static uint32_t wait_inbox_timeout_cnt;
bool wait_inbox_timeout_aee_triggered;

/* for IRQ affinity tuning */
static struct mutex affin_lock;
static unsigned int affin_depth;
static struct mtk_apu *g_apu;

static int current_ipi_handler_id;

static bool pwr_profile_polling_mode;

static inline void dump_mbox0_reg(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	unsigned int i, val;

	/* mbox0_in mbox0_out mbox0_dummy */
	for (i = 0; i < 8; i++) {
		val = ioread32(apu->apu_mbox + i * APU_MBOX_SLOT_SIZE);
		dev_info(dev, "m0_in 0x%x (0x%x)\n", i * APU_MBOX_SLOT_SIZE, val);
	}

	for (i = 0; i < 8; i++) {
		val = ioread32(apu->apu_mbox + 0x20 + i * APU_MBOX_SLOT_SIZE);
		dev_info(dev, "m0_out 0x%x (0x%x)\n", 0x20 + i * APU_MBOX_SLOT_SIZE, val);
	}

	for (i = 0; i < 4; i++) {
		val = ioread32(apu->apu_mbox + 0x40 + i * APU_MBOX_SLOT_SIZE);
		dev_info(dev, "m0_dummy 0x%x (0x%x)\n", 0x40 + i * APU_MBOX_SLOT_SIZE,  val);
	}

	dev_info(dev, "m0_wkup_cfg 0x80 (0x%x)\n", ioread32(apu->apu_mbox + 0x80));
	dev_info(dev, "m0_func_cfg 0xB0 (0x%x)\n", ioread32(apu->apu_mbox + 0xB0));
	dev_info(dev, "m0_ibox_irq 0xC0 (0x%x)\n", ioread32(apu->apu_mbox + 0xC0));
	dev_info(dev, "m0_obox_irq 0xC4 (0x%x)\n", ioread32(apu->apu_mbox + 0xC4));
	dev_info(dev, "m0_err_irq 0xC8 (0x%x)\n", ioread32(apu->apu_mbox + 0xC8));
	dev_info(dev, "m0_ibox_mask 0xD0 (0x%x)\n", ioread32(apu->apu_mbox + 0xD0));
	dev_info(dev, "m0_ibox_pri_mask 0xD4 (0x%x)\n", ioread32(apu->apu_mbox + 0xD4));
	dev_info(dev, "m0_obox_mask 0xD8 (0x%x)\n", ioread32(apu->apu_mbox + 0xD8));
	dev_info(dev, "m0_err_mask 0xDC (0x%x)\n", ioread32(apu->apu_mbox + 0xDC));
	dev_info(dev, "m0_domain_cfg 0xE0 (0x%x)\n", ioread32(apu->apu_mbox + 0xE0));
	dev_info(dev, "m0_err_record 0xF0 (0x%x)\n", ioread32(apu->apu_mbox + 0xF0));
}

static inline void dump_msg_buf(struct mtk_apu *apu, void *data, uint32_t len)
{
	struct device *dev = apu->dev;
	uint32_t i;
	int size = 64, num;
	uint8_t buf[64], *ptr = buf;
	int ret;

	dev_info(dev, "===== dump message =====\n");
	for (i = 0; i < len; i++) {
		num = snprintf(ptr, size, "%02x ", ((uint8_t *)data)[i]);
		if (num <= 0) {
			dev_info(dev, "%s: snprintf return error(num = %d)\n",
				__func__, num);
			return;
		}
		size -= num;
		ptr += num;

		if ((i + 1) % 4 == 0) {
			ret = snprintf(ptr++, size--, " ");
			if (ret <= 0) {
				dev_info(dev, "%s: snprintf return error(ret = %d)\n",
					__func__, ret);
				return;
			}
		}

		if ((i + 1) % 16 == 0 || (i + 1) >= len) {
			dev_info(dev, "%s\n", buf);
			size = 64;
			ptr = buf;
		}
	}
	dev_info(dev, "========================\n");
}

static uint32_t calculate_csum(void *data, uint32_t len)
{
	uint32_t csum = 0, res = 0, i;
	uint8_t *ptr;

	for (i = 0; i < (len / sizeof(csum)); i++)
		csum += *(((uint32_t *)data) + i);

	ptr = (uint8_t *)data + len / sizeof(csum) * sizeof(csum);
	for (i = 0; i < (len % sizeof(csum)); i++)
		res |= *(ptr + i) << i * 8;

	csum += res;

	return csum;
}

static inline bool bypass_check(u32 id)
{
	/* whitelist IPI used in power off flow */
	return id == APU_IPI_DEEP_IDLE;
}

static void ipi_usage_cnt_update(struct mtk_apu *apu, u32 id, int diff)
{
	struct apu_ipi_desc *ipi = &apu->ipi_desc[id];

	if (ipi_attrs[id].ack != IPI_WITH_ACK)
		return;

	spin_lock(&apu->usage_cnt_lock);
	ipi->usage_cnt += diff;
	spin_unlock(&apu->usage_cnt_lock);
}

extern int apu_deepidle_power_on_aputop(struct mtk_apu *apu);

int apu_ipi_send(struct mtk_apu *apu, u32 id, void *data, u32 len,
		 u32 wait_ms)
{
	struct mtk_apu_hw_ops *hw_ops;
	struct apu_ipi_desc *ipi;
	struct timespec64 ts, te;
	struct device *dev;
	struct apu_mbox_hdr hdr;
	unsigned long timeout;
	int ret = 0;
	uint32_t user_id = 0;

	ktime_get_ts64(&ts);

	if ((!apu) || (id <= APU_IPI_INIT) ||
	    (id >= APU_IPI_MAX) || (id == APU_IPI_NS_SERVICE) ||
	    (len > APU_SHARE_BUFFER_SIZE) || (!data))
		return -EINVAL;

	dev = apu->dev;
	ipi = &apu->ipi_desc[id];
	hw_ops = &apu->platdata->ops;

	if ((apu->platdata->flags & F_FAST_ON_OFF) == 0)
		if (!pm_runtime_enabled(dev)) {
			dev_info(dev, "%s: rpm disabled, ipi=%d\n", __func__, id);
			return -EBUSY;
		}

	if ((apu->platdata->flags & F_FAST_ON_OFF) &&
		ipi_attrs[id].direction == IPI_HOST_INITIATE &&
		apu->ipi_pwr_ref_cnt[id] == 0) {
		dev_info(dev, "%s: host initiated ipi(%d) not power on\n",
			__func__, id);
		return -EINVAL;
	}

	mutex_lock(&apu->send_lock);

	if (ipi_attrs[id].direction == IPI_HOST_INITIATE &&
	    apu->ipi_inbound_locked == IPI_LOCKED && !bypass_check(id)) {
		/* remove to reduce log */
		/*
		 * apu_ipi_info_ratelimited(dev, "%s: ipi locked, ipi=%d\n",
		 *	__func__, id);
		 */
		mutex_unlock(&apu->send_lock);
		return -EAGAIN;
	}

	/* re-init inbox mask everytime for aoc */
	apu_mbox_inbox_init(apu);

	if ((apu->platdata->flags & F_FAST_ON_OFF) == 0) {
		ret = apu_deepidle_power_on_aputop(apu);
		if (ret) {
			dev_info(dev, "apu_deepidle_power_on_aputop failed\n");
			mutex_unlock(&apu->send_lock);
			return -ESHUTDOWN;
		}
	}

	ret = apu_mbox_wait_inbox(apu);
	if (ret) {
		dev_info(dev, "wait inbox fail, ret=%d\n", ret);
		wait_inbox_timeout_cnt++;
		if (!wait_inbox_timeout_aee_triggered &&
			wait_inbox_timeout_cnt > 3 && !apu->bypass_pwr_off_chk) {
			apusys_rv_aee_warn("APUSYS_RV", "Wait_inbox_timeout");
			wait_inbox_timeout_aee_triggered = true;
		}
		goto unlock_mutex;
	}
	wait_inbox_timeout_cnt = 0;

	/* copy message payload to share buffer, need to do cache flush if
	 * the buffer is cacheable. currently not
	 */
	memcpy_toio(apu->send_buf, data, len);

	hdr.id = id;
	hdr.len = len;
	hdr.csum = calculate_csum(data, len);
	hdr.serial_no = tx_serial_no++;

	if (len >= 4)
		user_id = ((uint32_t *) data)[len/4 - 1];

	if (hw_ops->ipi_send_pre)
		hw_ops->ipi_send_pre(apu, id,
			ipi_attrs[id].direction == IPI_HOST_INITIATE);

	apu_mbox_write_inbox(apu, &hdr);

	apu->ipi_id = id;
	apu->ipi_id_ack[id] = false;

	/* poll ack from remote processor if wait_ms specified */
	if (wait_ms) {
		timeout = jiffies + msecs_to_jiffies(wait_ms);
		ret = wait_event_timeout(apu->ack_wq,
					 &apu->ipi_id_ack[id],
					 timeout);

		apu->ipi_id_ack[id] = false;

		if (WARN(!ret, "apu ipi %d ack timeout!", id)) {
			ret = -EIO;
			goto unlock_mutex;
		} else {
			ret = 0;
		}
	}

	ipi_usage_cnt_update(apu, id, 1);

unlock_mutex:
	if (hw_ops->ipi_send_post)
		hw_ops->ipi_send_post(apu);

	mutex_unlock(&apu->send_lock);

	ktime_get_ts64(&te);
	ts = timespec64_sub(te, ts);

	if (apu->platdata->flags & F_APUSYS_RV_TAG_SUPPORT)
		trace_apusys_rv_ipi_send(id, len, hdr.serial_no, hdr.csum, user_id,
			ipi->usage_cnt, timespec64_to_ns(&ts));

	apu_info_ratelimited(dev,
		 "%s: ipi_id=%d, len=%d, csum=%x, serial_no=%d, user_id=0x%x, elapse=%lld\n",
		 __func__, id, len, hdr.csum, hdr.serial_no, user_id,
		 timespec64_to_ns(&ts));

	return ret;
}

int apu_ipi_lock(struct mtk_apu *apu)
{
	struct apu_ipi_desc *ipi;
	int i;
	bool ready_to_lock = true;

	if (mutex_trylock(&apu->send_lock) == 0)
		return -EBUSY;

	if (apu->ipi_inbound_locked == IPI_LOCKED) {
		dev_info(apu->dev, "%s: ipi already locked\n", __func__);
		mutex_unlock(&apu->send_lock);
		return 0;
	}

	spin_lock(&apu->usage_cnt_lock);
	for (i = 0; i < APU_IPI_MAX; i++) {
		ipi = &apu->ipi_desc[i];

		if (ipi_attrs[i].ack == IPI_WITH_ACK &&
		    ipi->usage_cnt != 0 &&
		    !bypass_check(i)) {
			spin_unlock(&apu->usage_cnt_lock);
			dev_info(apu->dev, "%s: ipi %d is still in use %d\n",
				 __func__, i, ipi->usage_cnt);
			spin_lock(&apu->usage_cnt_lock);
			ready_to_lock = false;
		}

	}

	if (!ready_to_lock) {
		spin_unlock(&apu->usage_cnt_lock);
		mutex_unlock(&apu->send_lock);
		return -EBUSY;
	}

	apu->ipi_inbound_locked = IPI_LOCKED;
	spin_unlock(&apu->usage_cnt_lock);

	mutex_unlock(&apu->send_lock);

	return 0;
}

void apu_ipi_unlock(struct mtk_apu *apu)
{
	mutex_lock(&apu->send_lock);

	if (apu->ipi_inbound_locked == IPI_UNLOCKED)
		dev_info(apu->dev, "%s: ipi already unlocked\n", __func__);

	spin_lock(&apu->usage_cnt_lock);
	apu->ipi_inbound_locked = IPI_UNLOCKED;
	spin_unlock(&apu->usage_cnt_lock);

	mutex_unlock(&apu->send_lock);
}

int apu_ipi_register(struct mtk_apu *apu, u32 id,
		     ipi_top_handler_t top_handler, ipi_handler_t handler, void *priv)
{
	if (!apu || id >= APU_IPI_MAX ||
		WARN_ON(!top_handler && !handler)) {
		if (apu)
			dev_info(apu->dev,
				"%s failed. apu=%p, id=%d, handler=%p, priv=%p\n",
				__func__, apu, id, handler, priv);
		return -EINVAL;
	}

	dev_info(apu->dev, "%s: apu=%p, ipi=%d, handler=%p, priv=%p",
		 __func__, apu, id, handler, priv);

	mutex_lock(&apu->ipi_desc[id].lock);
	apu->ipi_desc[id].top_handler = top_handler;
	apu->ipi_desc[id].handler = handler;
	apu->ipi_desc[id].priv = priv;
	mutex_unlock(&apu->ipi_desc[id].lock);

	return 0;
}

void apu_ipi_unregister(struct mtk_apu *apu, u32 id)
{
	if (!apu || id >= APU_IPI_MAX) {
		if (apu != NULL)
			dev_info(apu->dev, "%s: invalid id=%d\n", __func__, id);
		return;
	}

	mutex_lock(&apu->ipi_desc[id].lock);
	apu->ipi_desc[id].top_handler = NULL;
	apu->ipi_desc[id].handler = NULL;
	apu->ipi_desc[id].priv = NULL;
	mutex_unlock(&apu->ipi_desc[id].lock);
}

static int apu_init_ipi_top_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_apu *apu = priv;

	strscpy(apu->run.fw_ver, data, APU_FW_VER_LEN);

	apu->run.signaled = 1;
	wake_up_interruptible(&apu->run.wq);

	return IRQ_HANDLED;
}

static void apu_init_ipi_bottom_handler(void *data, unsigned int len, void *priv)
{
	struct mtk_apu *apu = priv;
	int ret;

	strscpy(apu->run.fw_ver, data, APU_FW_VER_LEN);

	/* remain for driver to know whether cold boot success or not */
	apu->run.signaled = 1;
	wake_up_interruptible(&apu->run.wq);

	ret = apu_power_on_off(apu->pdev, APU_IPI_INIT, 0, 1);
	if (ret)
		dev_info(apu->dev, "%s: call apu_power_on_off fail(%d)\n", __func__, ret);
}

static irqreturn_t apu_ipi_handler(int irq, void *priv)
{
	struct timespec64 ts, te, t_elapse, tl;
	struct timespec64 t1, t2, t_diff;
	uint64_t t_elapse_ns = 0, t_mtx_lock_ns = 0, t_handler_ns = 0;
	uint64_t t_usage_cnt_update_ns = 0, t_wakup_ns = 0;
	struct mtk_apu *apu = priv;
	struct device *dev = apu->dev;
	ipi_handler_t handler;
	u32 id, len;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	struct apu_ipi_desc *ipi;
	uint32_t user_id = 0;

	id = apu->hdr.id;
	len = apu->hdr.len;
	ipi = &apu->ipi_desc[id];

	if (len >= 4)
		user_id = ((uint32_t *) temp_buf)[len/4 - 1];

	/* get the latency of threaded irq */
	ktime_get_ts64(&ts);
	tl = timespec64_sub(ts, apu->intr_ts_end);

	ktime_get_ts64(&t1);
	mutex_lock(&apu->ipi_desc[id].lock);
	ktime_get_ts64(&t2);
	t_diff = timespec64_sub(t2, t1);
	t_mtx_lock_ns = timespec64_to_ns(&t_diff);

	handler = apu->ipi_desc[id].handler;
	if (!handler) {
		dev_info(dev, "IPI id=%d is not registered", id);
		mutex_unlock(&apu->ipi_desc[id].lock);
		goto out;
	}

	current_ipi_handler_id = id;

	if (apu->apusys_rv_trace_on)
		apusys_rv_trace_begin("apu_ipi_handle(%d)", id);
	ktime_get_ts64(&t1);
	handler(temp_buf, len, apu->ipi_desc[id].priv);
	ktime_get_ts64(&t2);
	if (apu->apusys_rv_trace_on)
		apusys_rv_trace_end();
	t_diff = timespec64_sub(t2, t1);
	t_handler_ns = timespec64_to_ns(&t_diff);

	ktime_get_ts64(&t1);
	ipi_usage_cnt_update(apu, id, -1);
	ktime_get_ts64(&t2);
	t_diff = timespec64_sub(t2, t1);
	t_usage_cnt_update_ns = timespec64_to_ns(&t_diff);

	current_ipi_handler_id = -1;

	mutex_unlock(&apu->ipi_desc[id].lock);

	apu->ipi_id_ack[id] = true;

	ktime_get_ts64(&t1);
	wake_up(&apu->ack_wq);
	ktime_get_ts64(&t2);
	t_diff = timespec64_sub(t2, t1);
	t_wakup_ns = timespec64_to_ns(&t_diff);

out:
	ktime_get_ts64(&te);
	t_elapse = timespec64_sub(te, ts);
	t_elapse_ns = timespec64_to_ns(&t_elapse);

	if (apu->platdata->flags & F_APUSYS_RV_TAG_SUPPORT)
		trace_apusys_rv_ipi_handle(id, len, apu->hdr.serial_no,
			apu->hdr.csum, user_id, ipi->usage_cnt,
			timespec64_to_ns(&apu->intr_ts_begin), timespec64_to_ns(&ts),
			timespec64_to_ns(&tl), t_elapse_ns, t_handler_ns);

	/* t_elapse_ns > 1s */
	if (t_elapse_ns > 1000000000)
		dev_info(dev, APUSYS_RV_IPI_HANDLE_PRINT_HANDLER_EXEC_LONG,
		 __func__, id, len, apu->hdr.csum, apu->hdr.serial_no, user_id,
		 timespec64_to_ns(&tl), t_elapse_ns, t_handler_ns,
		 t_mtx_lock_ns, t_usage_cnt_update_ns, t_wakup_ns);
	else
		apu_info_ratelimited(dev, APUSYS_RV_IPI_HANDLE_PRINT,
		 __func__, id, len, apu->hdr.csum, apu->hdr.serial_no, user_id,
		 timespec64_to_ns(&tl), t_elapse_ns, t_handler_ns,
		 t_mtx_lock_ns, t_usage_cnt_update_ns, t_wakup_ns);

	if (hw_ops->wake_unlock && ipi_attrs[id].direction == IPI_APU_INITIATE
		&& ipi_attrs[id].ack == IPI_WITH_ACK)
		hw_ops->wake_unlock(apu, id);

	return IRQ_HANDLED;
}

irqreturn_t apu_ipi_int_handler(int irq, void *priv)
{
	struct mtk_apu *apu = priv;
	struct device *dev = apu->dev;
	struct mtk_share_obj *recv_obj = apu->recv_buf;
	u32 id, len, calc_csum;
	bool finish = false;
	uint32_t status;
	ipi_top_handler_t top_handler;
	int ret;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;

	status = ioread32(apu->apu_mbox + 0xc4);
	if (status != ((1 << APU_MBOX_HDR_SLOTS) - 1))
		dev_info(dev, "WARN abnormal isr call(0x%x)\n", status);

	ktime_get_ts64(&apu->intr_ts_begin);

	apu_mbox_read_outbox(apu, &apu->hdr);
	id = apu->hdr.id;
	len = apu->hdr.len;

	if (id >= APU_IPI_MAX) {
		dev_info(dev, "no such IPI id = %d\n", id);
		finish = true;
	}

	if (len > APU_SHARE_BUFFER_SIZE) {
		dev_info(dev, "IPI message too long(len %d, max %d)\n",
			len, APU_SHARE_BUFFER_SIZE);
		finish = true;
	}

	if (apu->hdr.serial_no != rx_serial_no) {
		dev_info(dev, "unmatched serial_no: curr=%u, recv=%u\n",
			rx_serial_no, apu->hdr.serial_no);
		/* correct the serial no. */
		rx_serial_no = apu->hdr.serial_no;
		/* apu_regdump(); */
		dump_mbox0_reg(apu);
		apusys_rv_aee_warn("APUSYS_RV", "IPI rx_serial_no unmatch");
	}
	rx_serial_no++;

	if (finish)
		goto done;

	if (hw_ops->wake_lock && ipi_attrs[id].direction == IPI_APU_INITIATE
		&& ipi_attrs[id].ack == IPI_WITH_ACK)
		hw_ops->wake_lock(apu, id);

	memcpy_fromio(temp_buf, &recv_obj->share_buf, len);

	/* ack after data copied */
	apu_mbox_ack_outbox(apu);

	calc_csum = calculate_csum(temp_buf, len);
	if (calc_csum != apu->hdr.csum) {
		dev_info(dev, "csum error: recv=0x%08x, calc=0x%08x, skip\n",
			apu->hdr.csum, calc_csum);
		dump_msg_buf(apu, temp_buf, apu->hdr.len);
		/* apu_regdump(); */
		dump_mbox0_reg(apu);
		apusys_rv_aee_warn("APUSYS_RV", "IPI rx csum error");
		/* csum error data not valid */
		return IRQ_HANDLED;
	}

	/* excute top handler if exist */
	top_handler = apu->ipi_desc[id].top_handler;
	if (top_handler) {
		ret = top_handler(temp_buf, len, apu->ipi_desc[id].priv);
		if (ret == IRQ_HANDLED)
			return IRQ_HANDLED;
	}

	ktime_get_ts64(&apu->intr_ts_end);

	return IRQ_WAKE_THREAD;

done:
	apu_mbox_ack_outbox(apu);

	return IRQ_HANDLED;
}

static int apu_send_ipi(struct platform_device *pdev, u32 id, void *buf,
			unsigned int len, unsigned int wait)
{
	struct mtk_apu *apu = platform_get_drvdata(pdev);

	return apu_ipi_send(apu, id, buf, len, wait);
}

static int apu_register_ipi(struct platform_device *pdev, u32 id,
			    ipi_handler_t handler, void *priv)
{
	struct mtk_apu *apu = platform_get_drvdata(pdev);

	return apu_ipi_register(apu, id, NULL, handler, priv);
}

static void apu_unregister_ipi(struct platform_device *pdev, u32 id)
{
	struct mtk_apu *apu = platform_get_drvdata(pdev);

	apu_ipi_unregister(apu, id);
}

int apu_power_on_off(struct platform_device *pdev, u32 id, u32 on, u32 off)
{
	int ret, i;
	struct mtk_apu *apu = platform_get_drvdata(pdev);
	struct device *dev;
	struct mtk_apu_hw_ops *hw_ops;
	struct apu_ipi_desc *ipi;
	struct timespec64 t1, t2;
	struct timespec64 ts_polling, te_polling;
	uint64_t time_diff;

	ktime_get_ts64(&t1);

	if (!apu)
		return -EINVAL;

	dev = apu->dev;

	if (id >= APU_IPI_MAX) {
		dev_info(dev, "%s: invalid ipi id = %d", __func__, id);
		return -EINVAL;
	}

	hw_ops = &apu->platdata->ops;

	if ((apu->platdata->flags & F_FAST_ON_OFF) == 0 ||
		!hw_ops->power_on_off) {
		/* dev_info(dev, "%s: not support\n", __func__); */
		return -EOPNOTSUPP;
	}

	if (apu->apusys_rv_trace_on)
		apusys_rv_trace_begin("apu_pwr(%d/%d/%d)", id, on, off);

	if (pwr_profile_polling_mode && hw_ops->polling_rpc_status) {
		mutex_lock(&apu->power_profile_lock);
		ktime_get_ts64(&ts_polling);
	}

	ipi = &apu->ipi_desc[id];
	spin_lock(&apu->usage_cnt_lock);
	if (off == 1 && ipi_attrs[id].direction == IPI_HOST_INITIATE &&
		(ipi->usage_cnt != 0 && apu->ipi_pwr_ref_cnt[id] <= 1) &&
		!(ipi->usage_cnt == 1 && current_ipi_handler_id == id &&
			apu->ipi_pwr_ref_cnt[id] == 1)) {
		dev_info(dev, "%s: power off fail, ipi(%d) usage cnt(%d) not zero!\n",
			__func__, id, ipi->usage_cnt);
		spin_unlock(&apu->usage_cnt_lock);
		apusys_rv_aee_warn("APUSYS_RV", "APUSYS_RV_OFF_FAIL");
		if (pwr_profile_polling_mode && hw_ops->polling_rpc_status)
			mutex_unlock(&apu->power_profile_lock);
		ret = -EINVAL;
		goto out;
	}
	spin_unlock(&apu->usage_cnt_lock);

	ret = hw_ops->power_on_off(apu, id, on, off);

	if (pwr_profile_polling_mode && hw_ops->polling_rpc_status &&
		((on && apu->local_pwr_ref_cnt == 1) || (off && apu->local_pwr_ref_cnt == 0))) {
		if (on)
			hw_ops->polling_rpc_status(apu, 1, 1000000);
		else if (off)
			hw_ops->polling_rpc_status(apu, 0, 1000000);
		ktime_get_ts64(&te_polling);
		ts_polling = timespec64_sub(te_polling, ts_polling);
		time_diff = timespec64_to_ns(&ts_polling);
		apu_power_on_off_profile(on, off, time_diff, apu->smc_time_diff_ns);
	}

	ktime_get_ts64(&t2);
	t1 = timespec64_sub(t2, t1);
	time_diff = timespec64_to_ns(&t1);

	if (apu->platdata->flags & F_APUSYS_RV_TAG_SUPPORT)
		trace_apusys_rv_pwr_ctrl(id, on, off, time_diff,
			apu->sub_latency[0], apu->sub_latency[1], apu->sub_latency[2],
			apu->sub_latency[3], apu->sub_latency[4], apu->sub_latency[5],
			apu->sub_latency[6], apu->sub_latency[7]);

	for (i = 0; i < MAX_PWR_SUB_LATENCY; i++)
		apu->sub_latency[i] = 0;

	if (pwr_profile_polling_mode && hw_ops->polling_rpc_status)
		mutex_unlock(&apu->power_profile_lock);

	apu_info_ratelimited(dev, "%s: id(%u), on(%u), off(%u), latency = %llu ns\n",
		__func__, id, on, off, time_diff);

out:
	if (apu->apusys_rv_trace_on)
		apusys_rv_trace_end();

	return ret;
}

static struct mtk_apu_rpmsg_info mtk_apu_rpmsg_info = {
	.send_ipi = apu_send_ipi,
	.register_ipi = apu_register_ipi,
	.unregister_ipi = apu_unregister_ipi,
	.ns_ipi_id = APU_IPI_NS_SERVICE,
	.power_on_off = apu_power_on_off,
};

static void apu_add_rpmsg_subdev(struct mtk_apu *apu)
{
	apu->rpmsg_subdev =
		mtk_apu_rpmsg_create_rproc_subdev(to_platform_device(apu->dev),
						  &mtk_apu_rpmsg_info);

	if (apu->rpmsg_subdev)
		rproc_add_subdev(apu->rproc, apu->rpmsg_subdev);
}

static void apu_remove_rpmsg_subdev(struct mtk_apu *apu)
{
	if (apu->rpmsg_subdev) {
		rproc_remove_subdev(apu->rproc, apu->rpmsg_subdev);
		mtk_apu_rpmsg_destroy_rproc_subdev(apu->rpmsg_subdev);
		apu->rpmsg_subdev = NULL;
	}
}

void apu_ipi_config_remove(struct mtk_apu *apu)
{
	dma_free_coherent(apu->dev, APU_SHARE_BUF_SIZE,
			  apu->recv_buf, apu->recv_buf_da);
}

int apu_ipi_config_init(struct mtk_apu *apu)
{
	struct device *dev = apu->dev;
	struct apu_ipi_config *ipi_config;
	void *ipi_buf = NULL;
	dma_addr_t ipi_buf_da = 0;

	ipi_config = (struct apu_ipi_config *)
		get_apu_config_user_ptr(apu->conf_buf, eAPU_IPI_CONFIG);

	/* initialize shared buffer */
	ipi_buf = dma_alloc_coherent(dev, APU_SHARE_BUF_SIZE,
				     &ipi_buf_da, GFP_KERNEL);
	if (!ipi_buf || !ipi_buf_da) {
		dev_info(dev, "failed to allocate ipi share memory\n");
		return -ENOMEM;
	}

	dev_info(dev, "%s: ipi_buf=%p, ipi_buf_da=%llu\n",
		 __func__, ipi_buf, ipi_buf_da);

	memset_io(ipi_buf, 0, sizeof(struct mtk_share_obj)*2);

	apu->recv_buf = ipi_buf;
	apu->recv_buf_da = ipi_buf_da;
	apu->send_buf = ipi_buf + sizeof(struct mtk_share_obj);
	apu->send_buf_da = ipi_buf_da + sizeof(struct mtk_share_obj);

	ipi_config->in_buf_da = apu->send_buf_da;
	ipi_config->out_buf_da = apu->recv_buf_da;

	return 0;
}

int apu_ipi_affin_enable(void)
{
	struct mtk_apu *apu = g_apu;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	int ret = 0;

	mutex_lock(&affin_lock);

	if (affin_depth == 0)
		ret = hw_ops->irq_affin_set(apu);

	affin_depth++;

	mutex_unlock(&affin_lock);

	return ret;
}

int apu_ipi_affin_disable(void)
{
	struct mtk_apu *apu = g_apu;
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	int ret = 0;

	mutex_lock(&affin_lock);

	affin_depth--;

	if (affin_depth == 0)
		ret = hw_ops->irq_affin_unset(apu);

	mutex_unlock(&affin_lock);

	return ret;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)

#define APU_IPI_UT_MAX_DATA (32)
enum {
	CMD_UT = 0,
	CMD_UT_RANDOM,
	CMD_GET_PWR_ON_OFF_TIME,
	CMD_PWR_TIME_PROFILE_INTERNAL,
	CMD_UT_IPI_OUTBOX_FUZZ_WRITE,
	MAX_CMD_UT_ID,
};

struct apu_ipi_ut_ipi_data {
	uint32_t cmd_id;
	uint32_t data[APU_IPI_UT_MAX_DATA];
};

struct apu_ipi_ut_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct completion ack;
};

static struct apu_ipi_ut_rpmsg_device apu_ipi_ut_rpm_dev;
static struct mutex apu_ipi_ut_mtx;

static uint32_t rcx_on_ce_ts_last;
static uint32_t rcx_off_ce_ts_last;
static uint32_t rcx_on_ce_ts_avg;
static uint32_t rcx_off_ce_ts_avg;
static uint32_t rcx_on_ce_ts_max;
static uint32_t rcx_off_ce_ts_max;

static uint32_t warmboot_on_ts_last;
static uint32_t dpidle_off_ts_last;
static uint32_t smmu_hw_sem_ts_last;
static uint32_t dvfs_skip_ts_last;
static uint32_t warmboot_on_ts_avg;
static uint32_t dpidle_off_ts_avg;
static uint32_t smmu_hw_sem_ts_avg;
static uint32_t dvfs_skip_ts_avg;
static uint32_t warmboot_on_ts_max;
static uint32_t dpidle_off_ts_max;
static uint32_t smmu_hw_sem_ts_max;
static uint32_t dvfs_skip_ts_max;
static uint32_t smmu_hw_sem_ts_larger_1ms_cnt;

static uint64_t apu_on_ts_last;
static uint64_t apu_off_ts_last;
static uint64_t apu_on_ts_avg;
static uint64_t apu_off_ts_avg;
static uint64_t apu_on_ts_max;
static uint64_t apu_off_ts_max;
static uint64_t apu_on_ts_acc;
static uint64_t apu_off_ts_acc;

static uint64_t smc_on_ts_last;
static uint64_t smc_off_ts_last;
static uint64_t smc_on_ts_avg;
static uint64_t smc_off_ts_avg;
static uint64_t smc_on_ts_max;
static uint64_t smc_off_ts_max;
static uint64_t smc_on_ts_acc;
static uint64_t smc_off_ts_acc;

static uint64_t apu_on_over_1ms_cnt;
static uint64_t apu_off_over_1ms_cnt;

static uint64_t apu_on_ts_cnt;
static uint64_t apu_off_ts_cnt;

static uint32_t boot_count;

static void apu_power_on_off_profile(u32 on, u32 off,
	uint64_t time_diff_ns, uint64_t time_diff_ns2)
{
	if (on) {
		apu_on_ts_last = time_diff_ns/1000;
		apu_on_ts_cnt++;
		if (apu_on_ts_last >= 1000)
			apu_on_over_1ms_cnt++;
		apu_on_ts_acc += apu_on_ts_last;
		if (apu_on_ts_cnt != 0)
			apu_on_ts_avg = apu_on_ts_acc / apu_on_ts_cnt;
		apu_on_ts_max = max(apu_on_ts_max, apu_on_ts_last);

		smc_on_ts_last = time_diff_ns2/1000;
		smc_on_ts_acc += smc_on_ts_last;
		if (apu_on_ts_cnt != 0)
			smc_on_ts_avg = smc_on_ts_acc / apu_on_ts_cnt;
		smc_on_ts_max = max(smc_on_ts_max, smc_on_ts_last);
	} else if (off) {
		apu_off_ts_last = time_diff_ns/1000;
		apu_off_ts_cnt++;
		if (apu_off_ts_last >= 1000)
			apu_off_over_1ms_cnt++;
		apu_off_ts_acc += apu_off_ts_last;
		if (apu_off_ts_cnt != 0)
			apu_off_ts_avg = apu_off_ts_acc / apu_off_ts_cnt;
		apu_off_ts_max = max(apu_off_ts_max, apu_off_ts_last);

		smc_off_ts_last = time_diff_ns2/1000;
		smc_off_ts_acc += smc_off_ts_last;
		if (apu_off_ts_cnt != 0)
			smc_off_ts_avg = smc_off_ts_acc / apu_off_ts_cnt;
		smc_off_ts_max = max(smc_off_ts_max, smc_off_ts_last);
	}
}

static int apu_ipi_ut_send(struct apu_ipi_ut_ipi_data *d, bool wait_ack)
{
	int ret = 0, ret2 = 0, i = 0;
	int size = 256, num;
	uint8_t buf[256], *ptr = buf;
	struct mtk_apu *apu = g_apu;
	struct mtk_apu_hw_ops *hw_ops;
	struct timespec64 ts, te;
	bool polling_mode = false;

	if (!apu) {
		pr_info("%s: apu == NULL\n", __func__);
		return -1;
	}
	hw_ops = &apu->platdata->ops;

	if (d->cmd_id == CMD_PWR_TIME_PROFILE_INTERNAL &&
		hw_ops->polling_rpc_status && d->data[0] == 1)
		polling_mode = true;

	if (!apu_ipi_ut_rpm_dev.ept) {
		pr_info("%s: apu_ipi_ut_rpm_dev.ept == NULL\n", __func__);
		return -1;
	}

	if (wait_ack)
		mutex_lock(&apu_ipi_ut_mtx);

	num = snprintf(ptr, size, "%s: cmd_id = %d, data = ",
		__func__, d->cmd_id);
	if (num <= 0) {
		pr_info("%s: snprintf return error(num = %d)\n",
			__func__, num);
		return num;
	}
	size -= num;
	ptr += num;

	for (i = 0; i < APU_IPI_UT_MAX_DATA; i++) {
		num = snprintf(ptr, size, "%u ", d->data[i]);
		if (num <= 0) {
			pr_info("%s: snprintf return error(num = %d), i = %d\n",
				__func__, num, i);
			return num;
		}
		size -= num;
		ptr += num;
	}
	/* pr_info("%s\n", buf); */

	if (polling_mode)
		ktime_get_ts64(&ts);

	/* power on */
	ret = rpmsg_sendto(apu_ipi_ut_rpm_dev.ept, NULL, 1, 0);
	if (ret && ret != -EOPNOTSUPP) {
		pr_info("%s: rpmsg_sendto(power on) fail(%d)\n", __func__, ret);
		goto out;
	}

	if (polling_mode) {
		ret = hw_ops->polling_rpc_status(apu, 1, 1000000);
		if (ret) {
			pr_info("%s: polling rpc timeout(%d)\n", __func__, ret);
		} else {
			ktime_get_ts64(&te);
			ts = timespec64_sub(te, ts);
			pr_info("%s: diff = %llu ns\n", __func__, timespec64_to_ns(&ts));
		}
	}

	ret = rpmsg_send(apu_ipi_ut_rpm_dev.ept, d, sizeof(*d));
	if (ret) {
		pr_info("%s: rpmsg_send fail(%d)\n", __func__, ret);
		/* power off to restore ref cnt */
		ret2 = rpmsg_sendto(apu_ipi_ut_rpm_dev.ept, NULL, 0, 1);
		if (ret2 && ret2 != -EOPNOTSUPP)
			pr_info("%s: rpmsg_sendto(power off) fail(%d)\n", __func__, ret2);
		goto out;
	}

	if (wait_ack) {
		ret = wait_for_completion_timeout(
				&apu_ipi_ut_rpm_dev.ack, msecs_to_jiffies(1000));
		if (ret == 0) {
			pr_info("%s: wait for completion timeout\n", __func__);
			ret = -1;
		} else {
			ret = 0;
		}
	}

out:
	if (wait_ack)
		mutex_unlock(&apu_ipi_ut_mtx);

	return ret;
}

static int apu_ipi_ut_rpmsg_probe(struct rpmsg_device *rpdev)
{
	pr_info("%s: name=%s, src=%d\n",
			__func__, rpdev->id.name, rpdev->src);

	apu_ipi_ut_rpm_dev.ept = rpdev->ept;
	apu_ipi_ut_rpm_dev.rpdev = rpdev;

	pr_info("%s: rpdev->ept = %p\n", __func__, rpdev->ept);

	return 0;
}

static int apu_ipi_ut_rpmsg_cb(struct rpmsg_device *rpdev, void *data,
		int len, void *priv, u32 src)
{
	int ret;
	struct apu_ipi_ut_ipi_data *d = data;
	uint32_t rand_num = 0;
	uint32_t val;
	struct mtk_apu *apu = g_apu;
	struct mtk_apu_hw_ops *hw_ops;
	struct timespec64 ts, te;
	bool polling_mode = false;

	if (!apu) {
		pr_info("%s: apu == NULL\n", __func__);
		return -1;
	}
	hw_ops = &apu->platdata->ops;

	if (d->cmd_id == CMD_PWR_TIME_PROFILE_INTERNAL &&
		hw_ops->polling_rpc_status && d->data[0] == 1)
		polling_mode = true;

	if (d->cmd_id == CMD_UT_RANDOM) {
		get_random_bytes(&rand_num, sizeof(rand_num));
		rand_num = rand_num % 100;
		/* usleep_range(0, rand_num); */
		msleep(rand_num);
	}

	val = d->data[0];
	pr_info("%s: cmd_id = %u, val = %d, rand_num = %u\n",
		__func__, d->cmd_id, val, rand_num);
	if (d->cmd_id != CMD_UT_RANDOM)
		complete(&apu_ipi_ut_rpm_dev.ack);

	if (d->cmd_id == CMD_GET_PWR_ON_OFF_TIME ||
		d->cmd_id == CMD_PWR_TIME_PROFILE_INTERNAL) {
		rcx_on_ce_ts_last = d->data[1];
		rcx_off_ce_ts_last = d->data[2];
		rcx_on_ce_ts_avg = d->data[3];
		rcx_off_ce_ts_avg = d->data[4];
		rcx_on_ce_ts_max = d->data[5];
		rcx_off_ce_ts_max = d->data[6];
		warmboot_on_ts_last = d->data[7];
		dpidle_off_ts_last = d->data[8];
		smmu_hw_sem_ts_last = d->data[9];
		dvfs_skip_ts_last = d->data[10];
		warmboot_on_ts_avg = d->data[11];
		dpidle_off_ts_avg = d->data[12];
		smmu_hw_sem_ts_avg = d->data[13];
		dvfs_skip_ts_avg = d->data[14];
		warmboot_on_ts_max = d->data[15];
		dpidle_off_ts_max = d->data[16];
		smmu_hw_sem_ts_max = d->data[17];
		dvfs_skip_ts_max = d->data[18];
		smmu_hw_sem_ts_larger_1ms_cnt = d->data[19];
		boot_count = d->data[20];
	}

	if (polling_mode)
		ktime_get_ts64(&ts);

	/* power off */
	ret = rpmsg_sendto(apu_ipi_ut_rpm_dev.ept, NULL, 0, 1);
	if (ret && ret != -EOPNOTSUPP)
		pr_info("%s: rpmsg_sendto(power off) fail(%d)\n", __func__, ret);

	if (polling_mode) {
		ret = hw_ops->polling_rpc_status(apu, 0, 1000000);
		if (ret) {
			pr_info("%s: polling rpc timeout(%d)\n", __func__, ret);
		} else {
			ktime_get_ts64(&te);
			ts = timespec64_sub(te, ts);
			pr_info("%s: diff = %llu ns\n", __func__, timespec64_to_ns(&ts));
		}
	}

	return 0;
}

static void apu_ipi_ut_rpmsg_remove(struct rpmsg_device *rpdev)
{
}

static const struct of_device_id apu_ipi_ut_rpmsg_of_match[] = {
	{ .compatible = "mediatek,apu-ipi-ut-rpmsg", },
	{ },
};

static struct rpmsg_driver apu_ipi_ut_rpmsg_driver = {
	.drv = {
		.name = "apu-ipi-ut-rpmsg",
		.owner = THIS_MODULE,
		.of_match_table = apu_ipi_ut_rpmsg_of_match,
	},
	.probe = apu_ipi_ut_rpmsg_probe,
	.callback = apu_ipi_ut_rpmsg_cb,
	.remove = apu_ipi_ut_rpmsg_remove,
};

static int apu_ipi_ut_val, apu_ipi_ut_cmd;

static int apu_ipi_dbg_show(struct seq_file *s, void *unused)
{
	struct mtk_apu *apu = g_apu;

	seq_printf(s, "apu_ipi_ut_cmd = %d, apu_ipi_ut_val = %d\n", apu_ipi_ut_cmd, apu_ipi_ut_val);

	if (apu_ipi_ut_cmd == CMD_GET_PWR_ON_OFF_TIME) {
		seq_printf(s, "rcx_on_avg_time = %u us\n",
			warmboot_on_ts_avg + rcx_on_ce_ts_avg);
		seq_printf(s, "rcx_off_avg_time = %u us\n",
			dpidle_off_ts_avg + rcx_off_ce_ts_avg);
		seq_printf(s, "boot_count = %u\n", boot_count);
	} else if (apu_ipi_ut_cmd == CMD_PWR_TIME_PROFILE_INTERNAL) {
		seq_printf(s, "pwr_profile_polling_mode = %u\n", pwr_profile_polling_mode);
		seq_printf(s, "pwr_on_polling_dbg_mode = %u\n", apu->pwr_on_polling_dbg_mode);
		seq_printf(s, "ce_dbg_polling_dump_mode = %u\n", apu->ce_dbg_polling_dump_mode);
		seq_printf(s, "apusys_rv_trace_on = %u\n", apu->apusys_rv_trace_on);

		seq_printf(s, "rcx_on_avg_time = %u us\n",
			warmboot_on_ts_avg + rcx_on_ce_ts_avg);
		seq_printf(s, "rcx_off_avg_time = %u us\n",
			dpidle_off_ts_avg + rcx_off_ce_ts_avg);
		seq_printf(s, "boot_count = %u\n", boot_count);
		seq_printf(s, "rcx_on_ce_ts_last = %u us\n", rcx_on_ce_ts_last);
		seq_printf(s, "rcx_off_ce_ts_last = %u us\n", rcx_off_ce_ts_last);
		seq_printf(s, "rcx_on_ce_ts_avg = %u us\n", rcx_on_ce_ts_avg);
		seq_printf(s, "rcx_off_ce_ts_avg = %u us\n", rcx_off_ce_ts_avg);
		seq_printf(s, "rcx_on_ce_ts_max = %u us\n", rcx_on_ce_ts_max);
		seq_printf(s, "rcx_off_ce_ts_max = %u us\n", rcx_off_ce_ts_max);

		seq_puts(s, "--------------------------------\n");

		seq_printf(s, "warmboot_on_ts_last = %u us\n", warmboot_on_ts_last);
		seq_printf(s, "dpidle_off_ts_last = %u us\n", dpidle_off_ts_last);
		seq_printf(s, "smmu_hw_sem_ts_last = %u us\n", smmu_hw_sem_ts_last);
		seq_printf(s, "dvfs_skip_ts_last = %u us\n", dvfs_skip_ts_last);
		seq_printf(s, "warmboot_on_ts_avg = %u us\n", warmboot_on_ts_avg);
		seq_printf(s, "dpidle_off_ts_avg = %u us\n", dpidle_off_ts_avg);
		seq_printf(s, "smmu_hw_sem_ts_avg = %u us\n", smmu_hw_sem_ts_avg);
		seq_printf(s, "dvfs_skip_ts_avg = %u us\n", dvfs_skip_ts_avg);
		seq_printf(s, "warmboot_on_ts_max = %u us\n", warmboot_on_ts_max);
		seq_printf(s, "dpidle_off_ts_max = %u us\n", dpidle_off_ts_max);
		seq_printf(s, "smmu_hw_sem_ts_max = %u us\n", smmu_hw_sem_ts_max);
		seq_printf(s, "dvfs_skip_ts_max = %u us\n", dvfs_skip_ts_max);
		seq_printf(s, "smmu_hw_sem_ts_larger_1ms_cnt = %u\n", smmu_hw_sem_ts_larger_1ms_cnt);

		seq_puts(s, "--------------------------------\n");

		seq_printf(s, "apu_on_ts_last = %llu us\n", apu_on_ts_last);
		seq_printf(s, "apu_off_ts_last = %llu us\n", apu_off_ts_last);
		seq_printf(s, "apu_on_ts_avg = %llu us\n", apu_on_ts_avg);
		seq_printf(s, "apu_off_ts_avg = %llu us\n", apu_off_ts_avg);
		seq_printf(s, "apu_on_ts_max = %llu us\n", apu_on_ts_max);
		seq_printf(s, "apu_off_ts_max = %llu us\n", apu_off_ts_max);

		seq_printf(s, "smc_on_ts_last = %llu us\n", smc_on_ts_last);
		seq_printf(s, "smc_off_ts_last = %llu us\n", smc_off_ts_last);
		seq_printf(s, "smc_on_ts_avg = %llu us\n", smc_on_ts_avg);
		seq_printf(s, "smc_off_ts_avg = %llu us\n", smc_off_ts_avg);
		seq_printf(s, "smc_on_ts_max = %llu us\n", smc_on_ts_max);
		seq_printf(s, "smc_off_ts_max = %llu us\n", smc_off_ts_max);

		seq_printf(s, "apu_on_over_1ms_cnt = %llu\n", apu_on_over_1ms_cnt);
		seq_printf(s, "apu_off_over_1ms_cnt = %llu\n", apu_off_over_1ms_cnt);

		seq_printf(s, "apu_on_ts_cnt = %llu\n", apu_on_ts_cnt);
		seq_printf(s, "apu_off_ts_cnt = %llu\n", apu_off_ts_cnt);

	}

	return 0;
}

static int apu_ipi_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, apu_ipi_dbg_show, inode->i_private);
}

static int apu_ipi_dbg_exec_cmd(int cmd, unsigned int *args)
{
	struct apu_ipi_ut_ipi_data d;
	int ret = 0;

	switch (cmd) {
	case CMD_UT:
		apu_ipi_ut_val = args[0];
		d.cmd_id = cmd;
		d.data[0] = args[0];
		ret = apu_ipi_ut_send(&d, true);
		break;
	case CMD_UT_RANDOM:
		apu_ipi_ut_val = args[0];
		d.cmd_id = cmd;
		d.data[0] = args[0];
		ret = apu_ipi_ut_send(&d, false);
		break;
	case CMD_GET_PWR_ON_OFF_TIME:
	case CMD_PWR_TIME_PROFILE_INTERNAL:
		apu_ipi_ut_val = args[0];
		d.cmd_id = cmd;
		d.data[0] = args[0];
		ret = apu_ipi_ut_send(&d, false);
		break;
	case CMD_UT_IPI_OUTBOX_FUZZ_WRITE:
		apu_ipi_ut_val = args[0];
		d.cmd_id = cmd;
		d.data[0] = args[0];
		d.data[1] = args[1];
		d.data[2] = args[2];
		ret = apu_ipi_ut_send(&d, false);
		break;
	default:
		pr_info("%s: unknown cmd %d\n", __func__, cmd);
		ret = -EINVAL;
	}

	return ret;
}

#define IPI_DBG_MAX_ARGS	(3)
static ssize_t apu_ipi_dbg_write(struct file *flip, const char __user *buffer,
				 size_t count, loff_t *f_pos)
{
	struct mtk_apu *apu = g_apu;
	char *tmp, *ptr, *token;
	unsigned int args[IPI_DBG_MAX_ARGS];
	int cmd = 0;
	int ret, i;
	bool change_pwr_profile_polling_mode = false;
	bool change_pwr_on_polling_dbg_mode = false;
	bool ce_dbg_polling_dump_mode = false;
	bool change_apusys_rv_trace_on = false;

	/* to prevent integer overflow leading to undefined behavior */
	if (count == UINT_MAX) {
		pr_info("%s: invalid count = %lu\n", __func__, count);
		return -EINVAL;
	}

	tmp = kzalloc(count + 1, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	ret = copy_from_user(tmp, buffer, count);
	if (ret) {
		pr_info("%s: failed to copy user data, ret=%d\n",
			__func__, ret);
		goto out;
	}

	tmp[count] = '\0';
	ptr = tmp;

	token = strsep(&ptr, " ");
	if (strcmp(token, "ut") == 0) {
		cmd = CMD_UT;
	} else if (strcmp(token, "ut_rand") == 0) {
		cmd = CMD_UT_RANDOM;
	} else if (strcmp(token, "get_pwr_time") == 0) {
		cmd = CMD_GET_PWR_ON_OFF_TIME;
	} else if (strcmp(token, "pwr_time_internal") == 0) {
		cmd = CMD_PWR_TIME_PROFILE_INTERNAL;
	} else if (strcmp(token, "pwr_profile_polling_mode") == 0) {
		change_pwr_profile_polling_mode = true;
	} else if (strcmp(token, "pwr_on_polling_dbg_mode") == 0) {
		change_pwr_on_polling_dbg_mode = true;
	} else if (strcmp(token, "ce_dbg_polling_dump_mode") == 0) {
		ce_dbg_polling_dump_mode = true;
	} else if (strcmp(token, "apusys_rv_trace_on") == 0) {
		change_apusys_rv_trace_on = true;
	} else if (strcmp(token, "ut_ipi_outbox_check") == 0) {
		cmd = CMD_UT_IPI_OUTBOX_FUZZ_WRITE;
	} else {
		ret = -EINVAL;
		pr_info("%s: unknown ipi dbg cmd: %s\n", __func__, token);
		goto out;
	}
	apu_ipi_ut_cmd = cmd;

	for (i = 0; i < IPI_DBG_MAX_ARGS && (token = strsep(&ptr, " ")); i++) {
		ret = kstrtoint(token, 10, &args[i]);
		if (ret) {
			pr_info("%s: invalid parameter i=%d, p=%s\n",
				__func__, i, token);
			goto out;
		}
	}

	if (change_pwr_profile_polling_mode) {
		pwr_profile_polling_mode = args[0];
		ret = count;
		goto out;
	} else if (change_pwr_on_polling_dbg_mode) {
		apu->pwr_on_polling_dbg_mode = args[0];
		ret = count;
		goto out;
	} else if (ce_dbg_polling_dump_mode) {
		apu->ce_dbg_polling_dump_mode = args[0];
		ret = count;
		goto out;
	} else if (change_apusys_rv_trace_on) {
		apu->apusys_rv_trace_on = args[0];
		ret = count;
		goto out;
	}

	ret = apu_ipi_dbg_exec_cmd(cmd, args);
	if (!ret)
		ret = count;

out:
	kfree(tmp);

	pr_info("%s: ret = %d\n", __func__, ret);

	return ret;
}


#ifdef APUSYS_RV_FPGA_EP
#define APU_IPI_DNAME	"apu_ipi"
#define APU_IPI_FNAME	"ipi_dbg"
static struct proc_dir_entry *ipi_dbg_root, *ipi_dbg_file;

static const struct proc_ops apu_ipi_dbg_fops = {
	.proc_open = apu_ipi_dbg_open,
	.proc_read = seq_read,
	.proc_write = apu_ipi_dbg_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int apu_ipi_dbg_init(void)
{
	ipi_dbg_root = proc_mkdir(APU_IPI_DNAME, NULL);
	if (IS_ERR_OR_NULL(ipi_dbg_root)) {
		pr_info("%s: failed to create debug dir %s\n",
			__func__, APU_IPI_DNAME);
		return -EINVAL;
	}

	ipi_dbg_file = proc_create(APU_IPI_FNAME, (0644), ipi_dbg_root,
					   &apu_ipi_dbg_fops);
	if (IS_ERR_OR_NULL(ipi_dbg_file)) {
		pr_info("%s: failed to create debug file %s\n",
			__func__, APU_IPI_FNAME);
		remove_proc_entry(APU_IPI_DNAME, NULL);
		return -EINVAL;
	}

	return 0;
}

static void apu_ipi_dbg_exit(void)
{
	remove_proc_entry(APU_IPI_FNAME, ipi_dbg_root);
	remove_proc_entry(APU_IPI_DNAME, NULL);
}
#else
#define APU_IPI_DNAME	"apu_ipi"
#define APU_IPI_FNAME	"ipi_dbg"
static struct dentry *ipi_dbg_root, *ipi_dbg_file;
static const struct file_operations apu_ipi_dbg_fops = {
	.open = apu_ipi_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = apu_ipi_dbg_write,
};

static int apu_ipi_dbg_init(void)
{
	ipi_dbg_root = debugfs_create_dir(APU_IPI_DNAME, NULL);
	if (IS_ERR_OR_NULL(ipi_dbg_root)) {
		pr_info("%s: failed to create debug dir %s\n",
			__func__, APU_IPI_DNAME);
		return -EINVAL;
	}

	ipi_dbg_file = debugfs_create_file(APU_IPI_FNAME, (0644), ipi_dbg_root,
					   NULL, &apu_ipi_dbg_fops);
	if (IS_ERR_OR_NULL(ipi_dbg_file)) {
		pr_info("%s: failed to create debug file %s\n",
			__func__, APU_IPI_FNAME);
		debugfs_remove_recursive(ipi_dbg_root);
		return -EINVAL;
	}

	return 0;
}

static void apu_ipi_dbg_exit(void)
{
	debugfs_remove(ipi_dbg_file);
	debugfs_remove_recursive(ipi_dbg_root);
}
#endif /* #ifdef APUSYS_RV_FPGA_EP */

static int apu_ipi_ut_init(struct mtk_apu *apu)
{
	int ret;

	init_completion(&apu_ipi_ut_rpm_dev.ack);
	mutex_init(&apu_ipi_ut_mtx);

	dev_info(apu->dev, "%s: register rpmsg...\n", __func__);
	ret = register_rpmsg_driver(&apu_ipi_ut_rpmsg_driver);
	if (ret) {
		dev_info(apu->dev, "failed to register apu ipi ut rpmsg driver\n");
		return ret;
	}

	apu_ipi_dbg_init();

	return 0;
}

static void apu_ipi_ut_exit(void)
{
	unregister_rpmsg_driver(&apu_ipi_ut_rpmsg_driver);
}
#else
static void apu_ipi_dbg_exit(void) { }
static int apu_ipi_ut_init(struct mtk_apu *apu) { return 0; }
static void apu_ipi_ut_exit(void) { }
#endif /* IS_ENABLED(CONFIG_DEBUG_FS) */

int apu_ipi_init(struct platform_device *pdev, struct mtk_apu *apu)
{
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;
	struct device *dev = apu->dev;
	int i, ret;

	tx_serial_no = 0;
	rx_serial_no = 0;

	mutex_init(&apu->send_lock);
	mutex_init(&apu->power_lock);
	mutex_init(&apu->power_profile_lock);
	spin_lock_init(&apu->usage_cnt_lock);

	mutex_init(&affin_lock);
	affin_depth = 0;
	g_apu = apu;

	if (apu->platdata->flags & F_APU_IPI_UT_SUPPORT)
		apu_ipi_ut_init(apu);

	for (i = 0; i < APU_IPI_MAX; i++) {
		mutex_init(&apu->ipi_desc[i].lock);
		lockdep_set_class_and_name(&apu->ipi_desc[i].lock,
					   &ipi_lock_key[i],
					   ipi_attrs[i].name);
		apu->ipi_pwr_ref_cnt[i] = 0;
	}

	current_ipi_handler_id = -1;

	init_waitqueue_head(&apu->run.wq);
	init_waitqueue_head(&apu->ack_wq);

	/* APU initialization IPI register */
	if ((apu->platdata->flags & F_FAST_ON_OFF) == 0) {
		ret = apu_ipi_register(apu, APU_IPI_INIT, apu_init_ipi_top_handler, NULL, apu);
		if (ret) {
			dev_info(dev, "failed to register apu_init_ipi_top_handler for init, ret=%d\n",
				ret);
			return ret;
		}
	} else {
		ret = apu_ipi_register(apu, APU_IPI_INIT, NULL, apu_init_ipi_bottom_handler, apu);
		if (ret) {
			dev_info(dev, "failed to register apu_init_ipi_bottom_handler for init, ret=%d\n",
				ret);
			return ret;
		}
	}

	/* add rpmsg subdev */
	apu_add_rpmsg_subdev(apu);

	/* register mailbox IRQ */
	apu->mbox0_irq_number = platform_get_irq_byname(pdev, "mbox0_irq");
	dev_info(&pdev->dev, "%s: mbox0_irq = %d\n", __func__,
		 apu->mbox0_irq_number);

	ret = devm_request_threaded_irq(&pdev->dev, apu->mbox0_irq_number,
				apu_ipi_int_handler, apu_ipi_handler, IRQF_ONESHOT,
				"apu_ipi", apu);
	if (ret < 0) {
		dev_info(&pdev->dev, "%s: failed to request irq %d, ret=%d\n",
			__func__, apu->mbox0_irq_number, ret);
		goto remove_rpmsg_subdev;
	}

	hw_ops->irq_affin_init(apu);

	apu_mbox_hw_init(apu);

	return 0;

remove_rpmsg_subdev:
	apu_remove_rpmsg_subdev(apu);
	apu_ipi_unregister(apu, APU_IPI_INIT);

	return ret;
}

void apu_ipi_remove(struct mtk_apu *apu)
{
	struct mtk_apu_hw_ops *hw_ops = &apu->platdata->ops;

	apu_ipi_dbg_exit();
	apu_mbox_hw_exit(apu);
	apu_remove_rpmsg_subdev(apu);
	apu_ipi_unregister(apu, APU_IPI_INIT);
	if (hw_ops->irq_affin_clear(apu))
		hw_ops->irq_affin_clear(apu);
	if (apu->platdata->flags & F_APU_IPI_UT_SUPPORT)
		apu_ipi_ut_exit();
}

struct mtk_apu *get_mtk_apu(void)
{
	return g_apu;
}

