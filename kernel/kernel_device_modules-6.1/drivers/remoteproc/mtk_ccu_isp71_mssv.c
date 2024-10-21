// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2023 MediaTek Inc.

#include <linux/arm-smccc.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/pm_runtime.h>
#include <linux/iommu.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/fdtable.h>
#include <linux/interrupt.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <soc/mediatek/smi.h>

#include "mtk_ccu_isp71_mssv.h"
#include "mtk_ccu_common_mssv.h"
#include "mtk-interconnect.h"
#include "remoteproc_internal.h"
#include <mtk-smmu-v3.h>
#include <mtk_heap.h>

#include "mtk_ccu_isp7sp_mssv_pmem.h"
#include "mtk_ccu_isp7sp_mssv_pmem_b.h"
#include "mtk_ccu_isp7sp_mssv_dmem.h"
#include "mtk_ccu_isp7sp_mssv_dmem_b.h"
#include "mtk_ccu_isp7sp_mssv_ddr.h"
#include "mtk_ccu_isp7sp_mssv_ddr_b.h"
#include "mtk_ccu_isp7spl_mssv_pmem.h"

#define CCU_SET_MMQOS
#define MTK_CCU_MB_RX_TIMEOUT_SPEC    1000  /* 10ms */
#define ERROR_DESC_LEN	80
#define CCU1_IRQ   0

#define MTK_CCU_TAG "[ccu_rproc]"
#define LOG_DBG(format, args...) \
	pr_info(MTK_CCU_TAG "[%s] " format, __func__, ##args)

static uint32_t ccu_cores;
static char *buf_name = "CCU_LOG_DBGDUMP";

struct mtk_ccu_clk_name ccu_clk_name_isp71[] = {
	{true, "CLK_TOP_CCUSYS_SEL"},
	{true, "CLK_TOP_CCU_AHB_SEL"},
	{true, "CLK_CCU_LARB"},
	{true, "CLK_CCU_AHB"},
	{true, "CLK_CCUSYS_CCU0"},
	{true, "CAM_LARB14"},
	{true, "CAM_MM1_GALS"},
	{false, ""}};

struct mtk_ccu_clk_name ccu_clk_name_isp7s[] = {
	{true, "CLK_TOP_CCUSYS_SEL"},
	{true, "CLK_TOP_CCU_AHB_SEL"},
	{true, "CLK_CCU_LARB"},
	{true, "CLK_CCU_AHB"},
	{true, "CLK_CCUSYS_CCU0"},
	{true, "CAM_CCUSYS"},
	{true, "CAM_CAM2SYS_GALS"},
	{true, "CAM_CAM2MM1_GALS"},
	{true, "CAM_CAM2MM0_GALS"},
	{true, "CAM_MRAW"},
	{true, "CAM_CG"},
	{false, ""}};

struct mtk_ccu_clk_name ccu_clk_name_isp7sp[] = {
	{true, "TOP_CAM"},
	{true, "TOP_CCU_AHB"},
	{true, "VLP_CCUSYS"},
	{true, "VLP_CCUTM"},
	{true, "CCU2MM0_GALS"},
	{true, "CCU_LARB"},
	{true, "CCU_AHB"},
	{true, "CCUSYS_CCU0"},
	{true, "CCUSYS_CCU1"},
	{true, "CAM_CG"},
	{true, "CAM_VCORE_CG"},
	{false, ""}};

struct mtk_ccu_clk_name ccu_clk_name_isp7spl[] = {
	{true, "TOP_CAM"},
	{true, "TOP_CCU_AHB"},
	{true, "TOP_CCUSYS"},
	{true, "TOP_CCUTM"},
	{true, "CCU2MM0_GALS"},
	{true, "CCU_LARB"},
	{true, "CCU_AHB"},
	{true, "CCUSYS_CCU0"},
	{true, "CAM_CG"},
	{true, "CAM_VCORE_CG"},
	{false, ""}};

char ccu_err_str[ERR_CCU_MAX][16] = {
	"ERR_NULL",
	"ERR_INFRA",
	"ERR_SW_SW",
	"ERR_LW_LW",
	"ERR_SW_LW",
	"ERR_SRAM_RW",
	"ERR_WDMA_TOUT",
	"ERR_WDMA_DRAM",
	"ERR_RDMA_TOUT",
	"ERR_RDMA_DRAM"
};

char ccu_err_abnormal[] = "ERR_ABNORMAL";

#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG)
extern struct rproc *ccu_rproc_ipifuzz; /* IPIFuzz, to export symbol. */
struct rproc *ccu_rproc_ipifuzz; /* IPIFuzz */
#endif
static int mtk_ccu_probe(struct platform_device *dev);
static int mtk_ccu_remove(struct platform_device *dev);
static int mtk_ccu_read_platform_info_from_dt(struct device_node
	*node, struct mtk_ccu *ccu);
static int mtk_ccu_get_power(struct mtk_ccu *ccu, struct device *dev);
static void mtk_ccu_put_power(struct mtk_ccu *ccu, struct device *dev);
static int mtk_ccu_stopx(struct rproc *rproc, bool normal_stop);

irqreturn_t mtk_ccu_isr_handler(int irq, void *priv)
{
	struct mtk_ccu *ccu = (struct mtk_ccu *)priv;
	uint32_t errno;
#if (!CCU1_IRQ)
	uint32_t errno1;
#endif
	char *msg;

	if (!spin_trylock(&ccu->ccu_poweron_lock)) {
		LOG_DBG("trylock failed.\n");
		goto ISR_EXIT;
	}

	if (!ccu->poweron) {
		LOG_DBG("ccu->poweron false.\n");
#ifdef REQUEST_IRQ_IN_INIT
		if (spin_trylock(&ccu->ccu_irq_lock)) {
			if (ccu->disirq) {
				ccu->disirq = false;
				spin_unlock(&ccu->ccu_irq_lock);
				disable_irq_nosync(ccu->irq_num);
			} else
				spin_unlock(&ccu->ccu_irq_lock);
		}
#endif
		spin_unlock(&ccu->ccu_poweron_lock);
		goto ISR_EXIT;
	}

	if (ccu->ccu_version > CCU_VER_ISP71)
		writel(0xFF, ccu->ccu_base + MTK_CCU_INT_CLR_EXCH);
	else
		writel(0xFF, ccu->ccu_base + MTK_CCU_INT_CLR);

	errno = readl(ccu->ccu_base + MTK_CCU_SPARE_REG29);
#if (!CCU1_IRQ)
	if (ccu->ccu_cores == 2)
		errno1 = readl(ccu->ccu_base + MTK_CCU_SPARE_REG30);
#endif

	spin_unlock(&ccu->ccu_poweron_lock);

	if (errno) {
		msg = (errno < ERR_CCU_MAX) ? ccu_err_str[errno] : ccu_err_abnormal;
		dev_err(ccu->dev, "CCU0 error %d %s\n", errno, msg);
	}
#if (!CCU1_IRQ)
	if ((ccu->ccu_cores == 2) && (errno1)) {
		msg = (errno1 < ERR_CCU_MAX) ? ccu_err_str[errno1] : ccu_err_abnormal;
		dev_err(ccu->dev, "CCU1 error %d %s\n", errno1, msg);
	}
#endif

	if ((errno)
#if (!CCU1_IRQ)
		|| ((ccu->ccu_cores == 2) && (errno1))
#endif
		)
		WARN_ON(1);

ISR_EXIT:
	return IRQ_HANDLED;
}

irqreturn_t mtk_ccu1_isr_handler(int irq, void *priv)
{
	struct mtk_ccu *ccu = (struct mtk_ccu *)priv;
	uint32_t errno;
	char *msg;

	if (!spin_trylock(&ccu->ccu_poweron_lock)) {
		LOG_DBG("trylock failed.\n");
		goto ISR1_EXIT;
	}

	if (!ccu->poweron) {
		LOG_DBG("ccu->poweron false.\n");
#ifdef REQUEST_IRQ_IN_INIT
		if (spin_trylock(&ccu->ccu_irq_lock)) {
			if (ccu->disirq) {
				ccu->disirq = false;
				spin_unlock(&ccu->ccu_irq_lock);
#if (CCU1_IRQ)
				disable_irq_nosync(ccu->irq1_num);
#endif
			} else
				spin_unlock(&ccu->ccu_irq_lock);
		}
#endif
		spin_unlock(&ccu->ccu_poweron_lock);
		goto ISR1_EXIT;
	}

	if (ccu->ccu_version > CCU_VER_ISP71)
		writel(0xFF, ccu->ccu1_base + MTK_CCU_INT_CLR_EXCH);
	else
		writel(0xFF, ccu->ccu1_base + MTK_CCU_INT_CLR);

	errno = readl(ccu->ccu1_base + MTK_CCU_SPARE_REG29);

	spin_unlock(&ccu->ccu_poweron_lock);

	msg = (errno < ERR_CCU_MAX) ? ccu_err_str[errno] : ccu_err_abnormal;
	dev_err(ccu->dev, "CCU1 error %d %s\n", errno, msg);

	if (errno)
		WARN_ON(1);

ISR1_EXIT:
	return IRQ_HANDLED;
}

static int
mtk_ccu_deallocate_mem(struct device *dev, struct mtk_ccu_mem_handle *memHandle, bool smmu)
{
	if ((!dev) || (!memHandle))
		return -EINVAL;

	if (smmu)
		goto dealloc_with_smmu;

	dma_free_attrs(dev, memHandle->meminfo.size, memHandle->meminfo.va,
		memHandle->meminfo.mva, DMA_ATTR_WRITE_COMBINE);

	goto dealloc_end;

dealloc_with_smmu:
	if ((memHandle->dmabuf) && (memHandle->meminfo.va)) {
		dma_buf_vunmap(memHandle->dmabuf, &memHandle->map);
		memHandle->meminfo.va = NULL;
	}
	if ((memHandle->attach) && (memHandle->sgt)) {
		dma_buf_unmap_attachment(memHandle->attach, memHandle->sgt,
			DMA_FROM_DEVICE);
		memHandle->sgt = NULL;
	}
	if ((memHandle->dmabuf) && (memHandle->attach)) {
		dma_buf_detach(memHandle->dmabuf, memHandle->attach);
		memHandle->attach = NULL;
	}
	if (memHandle->dmabuf) {
		dma_heap_buffer_free(memHandle->dmabuf);
		memHandle->dmabuf = NULL;
	}

dealloc_end:
	memset(memHandle, 0, sizeof(struct mtk_ccu_mem_handle));

	return 0;
}

static int
mtk_ccu_allocate_mem(struct device *dev, struct mtk_ccu_mem_handle *memHandle, bool smmu)
{
	struct dma_heap *dmaheap;
	struct device *sdev;
	int ret;

	if ((!dev) || (!memHandle))
		return -EINVAL;

	if (memHandle->meminfo.size <= 0)
		return -EINVAL;

	if (smmu)
		goto alloc_with_smmu;

	/* get buffer virtual address */
	memHandle->meminfo.va = dma_alloc_attrs(dev, memHandle->meminfo.size,
		&memHandle->meminfo.mva, GFP_KERNEL, DMA_ATTR_WRITE_COMBINE);

	if (memHandle->meminfo.va == NULL) {
		dev_err(dev, "fail to get buffer kernel virtual address");
		return -EINVAL;
	}

	goto alloc_show;

alloc_with_smmu:
	LOG_DBG("alloc by DMA-BUF\n");

	/* dma_heap_find() will fail if mtk_ccu.ko is in ramdisk. */
	dmaheap = dma_heap_find("mtk_mm-uncached");
	if (!dmaheap) {
		dev_err(dev, "fail to find dma_heap");
		return -EINVAL;
	}

	memHandle->dmabuf = dma_heap_buffer_alloc(dmaheap, memHandle->meminfo.size,
		O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	dma_heap_put(dmaheap);
	if (IS_ERR(memHandle->dmabuf)) {
		dev_err(dev, "fail to alloc dma_buf");
		memHandle->dmabuf = NULL;
		goto err_out;
	}

	mtk_dma_buf_set_name(memHandle->dmabuf, buf_name);

	sdev = mtk_smmu_get_shared_device(dev);
	if (!sdev) {
		dev_err(dev, "fail to mtk_smmu_get_shared_device()");
		goto err_out;
	}

	memHandle->attach = dma_buf_attach(memHandle->dmabuf, sdev);
	if (IS_ERR(memHandle->attach)) {
		dev_err(dev, "fail to attach dma_buf");
		memHandle->attach = NULL;
		goto err_out;
	}

	memHandle->sgt = dma_buf_map_attachment(memHandle->attach, DMA_FROM_DEVICE);
	if (IS_ERR(memHandle->sgt)) {
		dev_err(dev, "fail to map attachment");
		memHandle->sgt = NULL;
		goto err_out;
	}

	memHandle->meminfo.mva = sg_dma_address(memHandle->sgt->sgl);

	ret = dma_buf_vmap(memHandle->dmabuf, &memHandle->map);
	memHandle->meminfo.va = memHandle->map.vaddr;
	if ((ret) || (memHandle->meminfo.va == NULL)) {
		dev_err(dev, "fail to map va");
		memHandle->meminfo.va = NULL;
		goto err_out;
	}

alloc_show:
	dev_info(dev, "success: size(0x%x), va(0x%lx), mva(%pad)\n",
		memHandle->meminfo.size, (unsigned long)memHandle->meminfo.va,
		&memHandle->meminfo.mva);

	return 0;

err_out:
	mtk_ccu_deallocate_mem(dev, memHandle, smmu);
	return -EINVAL;
}

int mtk_ccu_sw_hw_reset(struct mtk_ccu *ccu)
{
	uint32_t duration = 0;
	/* ISP_7SPL with RV33 use HALT_MASK_RV55. */
	uint32_t halt_mask = (ccu->ccu_version >= CCU_VER_ISP7SP) ?
				HALT_MASK_RV55 : HALT_MASK_RV33;
	uint32_t ccu_status, ccu1_status = halt_mask;

	/* check halt is up */
	ccu_status = readl(ccu->ccu_base + MTK_CCU_MON_ST);
	if (ccu->ccu_cores == 2)
		ccu1_status = readl(ccu->ccu1_base + MTK_CCU_MON_ST);
	LOG_DBG("polling CCU halt(0x%08x) halt(0x%08x)\n", ccu_status, ccu1_status);
	duration = 0;
	while (((ccu_status & halt_mask) != halt_mask)	||
			((ccu1_status & halt_mask) != halt_mask)) {
		duration++;
		if (duration > 1000) {
			LOG_DBG("CCU0 AXIREMAP %08x\n",
				readl(ccu->ccu_base + MTK_CCU_REG_AXI_REMAP));
			LOG_DBG("CCU0 reg0-7: %08x %08x %08x %08x %08x %08x %08x %08x\n",
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG00),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG01),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG02),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG03),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG04),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG05),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG06),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG07));
			LOG_DBG("CCU0 reg8-15: %08x %08x %08x %08x %08x %08x %08x %08x\n",
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG08),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG09),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG10),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG11),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG12),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG13),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG14),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG15));
			LOG_DBG("CCU0 reg16-23: %08x %08x %08x %08x %08x %08x %08x %08x\n",
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG16),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG17),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG18),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG19),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG20),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG21),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG22),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG23));
			LOG_DBG("CCU0 reg24-31: %08x %08x %08x %08x %08x %08x %08x %08x\n",
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG24),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG25),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG26),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG27),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG28),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG29),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG30),
				readl(ccu->ccu_base + MTK_CCU_SPARE_REG31));
			if (ccu->ccu_cores == 2) {
				LOG_DBG("CCU1 AXIREMAP %08x\n",
					readl(ccu->ccu1_base + MTK_CCU_REG_AXI_REMAP));
				LOG_DBG("CCU1 reg0-7: %08x %08x %08x %08x %08x %08x %08x %08x\n",
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG00),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG01),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG02),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG03),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG04),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG05),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG06),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG07));
				LOG_DBG("CCU1 reg8-15: %08x %08x %08x %08x %08x %08x %08x %08x\n",
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG08),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG09),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG10),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG11),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG12),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG13),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG14),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG15));
				LOG_DBG("CCU1 reg16-23: %08x %08x %08x %08x %08x %08x %08x %08x\n",
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG16),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG17),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG18),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG19),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG20),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG21),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG22),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG23));
				LOG_DBG("CCU1 reg24-31: %08x %08x %08x %08x %08x %08x %08x %08x\n",
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG24),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG25),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG26),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG27),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG28),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG29),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG30),
					readl(ccu->ccu1_base + MTK_CCU_SPARE_REG31));
			}
			dev_err(ccu->dev,
			"polling CCU halt, timeout: (0x%08x) (0x%08x)\n", ccu_status, ccu1_status);
			/* mtk_smi_dbg_hang_detect("CCU"); */
			break;
		}
		udelay(10);
		ccu_status = readl(ccu->ccu_base + MTK_CCU_MON_ST);
		if (ccu->ccu_cores == 2)
			ccu1_status = readl(ccu->ccu1_base + MTK_CCU_MON_ST);
	}
	LOG_DBG("polling CCU halt done(0x%08x)(0x%08x)\n", ccu_status, ccu1_status);

	return true;
}

struct platform_device *mtk_ccu_get_pdev(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *ccu_node;
	struct platform_device *ccu_pdev;

	ccu_node = of_parse_phandle(dev->of_node, "mediatek,ccu_rproc", 0);
	if (!ccu_node) {
		dev_err(dev, "failed to get ccu node\n");
		return NULL;
	}

	ccu_pdev = of_find_device_by_node(ccu_node);
	if (WARN_ON(!ccu_pdev)) {
		dev_err(dev, "failed to get ccu pdev\n");
		of_node_put(ccu_node);
		return NULL;
	}

	return ccu_pdev;
}
EXPORT_SYMBOL_GPL(mtk_ccu_get_pdev);

#define CCU_SPARE_REGS	32
static void clear_ccu_spare_regs(uint32_t *regs)
{
	uint32_t i;

	for (i = 0; i < CCU_SPARE_REGS; ++i)
		writel(0, regs + i);
}

static int mtk_ccu_run(struct mtk_ccu *ccu)
{
	int32_t timeout = 100;
	struct arm_smccc_res res;
#if !defined(SECURE_CCU)
	dma_addr_t remapOffset;
	bool all_bootup = true;
	int ret;
#endif

	LOG_DBG("+\n");

#if defined(SECURE_CCU)
	writel(CCU_GO_TO_RUN, ccu_base + MTK_CCU_SPARE_REG06);
#ifdef CONFIG_ARM64
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u64) CCU_SMC_REQ_RUN, 0, 0, 0, 0, 0, 0, &res);
#endif
#ifdef CONFIG_ARM_PSCI
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u32) CCU_SMC_REQ_RUN, 0, 0, 0, 0, 0, 0, &res);
#endif
#else  /* !defined(SECURE_CCU) */
	if (ccu->compact_ipc)
		writel(CCU_DMA_DOMAIN, ccu->ccu_base + 0x8098); /* EXCH_INFO30 */
	else
		writel(CCU_DMA_DOMAIN, ccu->ccu_base + 0x8038); /* EXCH_INFO06 */
#ifdef CONFIG_ARM64
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u64) CCU_SMC_REQ_DMA_DOMAIN, 0, 0, 0, 0, 0, 0, &res);
#endif
#ifdef CONFIG_ARM_PSCI
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u32) CCU_SMC_REQ_DMA_DOMAIN, 0, 0, 0, 0, 0, 0, &res);
#endif
	ret = (int)(res.a0);
	if (ret)
		dev_err(ccu->dev, "CCU_SMC_REQ_DMA_DOMAIN ret=%d\n", ret);

	clear_ccu_spare_regs(ccu->ccu_base + MTK_CCU_SPARE_REG00);
	/* Set MSSV cmd, 00: Task, 01: CCU_Stop, 02: ap_input, 03: ccu_output */
	writel(STRESS_TEST, ccu->ccu_base + MTK_CCU_SPARE_REG00);
	writel(0, ccu->ccu_base + MTK_CCU_SPARE_REG01);
	writel(0, ccu->ccu_base + MTK_CCU_SPARE_REG02); /* 0:infinite */
	writel(0, ccu->ccu_base + MTK_CCU_SPARE_REG03);
	writel(0, ccu->ccu_base + MTK_CCU_SPARE_REG06);
	writel(0, ccu->ccu_base + MTK_CCU_SPARE_REG31);
	/* Set CCU remap offset */
	remapOffset = ccu->buffer_handle[0].meminfo.mva - MTK_CCU_CACHE_BASE;
	writel(remapOffset >> 4, ccu->ccu_base + MTK_CCU_REG_AXI_REMAP);
	/* Set CCU_RESET. CCU_HW_RST=0*/
	writel(0x0, ccu->ccu_base + MTK_CCU_REG_RESET);
	if (ccu->ccu_cores == 2) {
		clear_ccu_spare_regs(ccu->ccu1_base + MTK_CCU_SPARE_REG00);
		/* Set MSSV cmd, 00: Task, 01: CCU_Stop, 02: ap_input, 03: ccu_output */
		writel(STRESS_TEST, ccu->ccu1_base + MTK_CCU_SPARE_REG00);
		writel(0, ccu->ccu1_base + MTK_CCU_SPARE_REG01);
		writel(0, ccu->ccu1_base + MTK_CCU_SPARE_REG02); /* 0:infinite */
		writel(0, ccu->ccu1_base + MTK_CCU_SPARE_REG03);
		writel(0, ccu->ccu1_base + MTK_CCU_SPARE_REG06);
		writel(0, ccu->ccu1_base + MTK_CCU_SPARE_REG31);
		/* Set CCU remap offset */
		remapOffset = ccu->buffer_handle[1].meminfo.mva - MTK_CCU_CACHE_BASE;
		writel(remapOffset >> 4, ccu->ccu1_base + MTK_CCU_REG_AXI_REMAP);
		/* Set CCU_RESET. CCU_HW_RST=0*/
		writel(0x0, ccu->ccu1_base + MTK_CCU_REG_RESET);
	}
#endif

	/*3. Pulling CCU init done spare register*/
	while (
		((readl(ccu->ccu_base + MTK_CCU_SPARE_REG08) != CCU_STATUS_INIT_DONE)
		|| ((ccu->ccu_cores == 2) &&
			(readl(ccu->ccu1_base + MTK_CCU_SPARE_REG08) != CCU_STATUS_INIT_DONE))
		) && (timeout >= 0)) {
		usleep_range(50, 100);
		timeout = timeout - 1;
	}

	if (readl(ccu->ccu_base + MTK_CCU_SPARE_REG08) != CCU_STATUS_INIT_DONE) {
		dev_err(ccu->dev, "CCU init timeout\n");
		all_bootup = false;
	} else
		LOG_DBG("CCU init done 0x%08x", readl(ccu->ccu_base + MTK_CCU_SPARE_REG08));

	if (ccu->ccu_cores == 2) {
		if (readl(ccu->ccu1_base + MTK_CCU_SPARE_REG08) != CCU_STATUS_INIT_DONE) {
			dev_err(ccu->dev, "CCU1 init timeout\n");
			all_bootup = false;
		} else
			LOG_DBG("CCU1 init done 0x%08x", readl(ccu->ccu1_base + MTK_CCU_SPARE_REG08));
	}

	LOG_DBG("-\n");

	return (all_bootup) ? 0 : -ETIMEDOUT;
}

static int mtk_ccu_clk_prepare(struct mtk_ccu *ccu)
{
	int ret;
	int i = 0;
	struct device *dev = ccu->dev;

	LOG_DBG("Power on CCU0.\n");
	ret = mtk_ccu_get_power(ccu, dev);
	if (ret) {
		dev_err(ccu->dev, "Power on CCU0 fail\n");
		WARN_ON(1);
		return ret;
	}

	LOG_DBG("Clock on CCU(%d)\n", ccu->clock_num);
	for (i = 0; (i < ccu->clock_num) && (i < MTK_CCU_CLK_PWR_NUM); ++i) {
		ret = clk_prepare_enable(ccu->ccu_clk_pwr_ctrl[i]);
		if (ret) {
			dev_err(dev, "failed to enable CCU clocks #%d %s\n",
				i, ccu->clock_name[i].name);
			goto ERROR;
		}
	}

	return 0;

ERROR:
	for (--i; i >= 0 ; --i)
		clk_disable_unprepare(ccu->ccu_clk_pwr_ctrl[i]);

	mtk_ccu_put_power(ccu, dev);

	WARN_ON(1);

	return ret;
}

static void mtk_ccu_clk_unprepare(struct mtk_ccu *ccu)
{
	int i;

	LOG_DBG("Clock off CCU(%d)\n", ccu->clock_num);
	for (i = 0; (i < ccu->clock_num) && (i < MTK_CCU_CLK_PWR_NUM); ++i)
		clk_disable_unprepare(ccu->ccu_clk_pwr_ctrl[i]);
	mtk_ccu_put_power(ccu, ccu->dev);
}

static int mtk_ccu_start(struct rproc *rproc)
{
	struct mtk_ccu *ccu = (struct mtk_ccu *)rproc->priv;
	int ret;
#ifndef REQUEST_IRQ_IN_INIT
	int rc;
#endif

#if defined(CCU_SET_MMQOS)
	if (ccu->ccu_version < CCU_VER_ISP7SP) {
		mtk_icc_set_bw(ccu->path_ccuo, MBps_to_icc(20), MBps_to_icc(30));
		mtk_icc_set_bw(ccu->path_ccui, MBps_to_icc(20), MBps_to_icc(30));
		mtk_icc_set_bw(ccu->path_ccug, MBps_to_icc(300), MBps_to_icc(400));
	} else {
		mtk_icc_set_bw(ccu->path_ccuo, MBps_to_icc(320), MBps_to_icc(430));
		mtk_icc_set_bw(ccu->path_ccui, MBps_to_icc(320), MBps_to_icc(430));
	}
#endif

	spin_lock(&ccu->ccu_poweron_lock);
	ccu->poweron = true;
	spin_unlock(&ccu->ccu_poweron_lock);

	ccu->disirq = false;

#ifdef REQUEST_IRQ_IN_INIT
	enable_irq(ccu->irq_num);
	if (ccu->ccu_cores == 2)
		enable_irq(ccu->irq1_num);
#else
	rc = devm_request_threaded_irq(ccu->dev, ccu->irq_num, NULL,
		mtk_ccu_isr_handler, IRQF_ONESHOT, "ccu_rproc", ccu);
	if (rc) {
		dev_err(ccu->dev, "fail to request ccu irq(%d, %d)!\n", ccu->irq_num, rc);
		WARN_ON(1);
		return -ENODEV;
	}
#if (CCU1_IRQ)
	if (ccu->ccu_cores == 2) {
		rc = devm_request_threaded_irq(ccu->dev, ccu->irq1_num, NULL,
			mtk_ccu1_isr_handler, IRQF_ONESHOT, "ccu_rproc", ccu);
		if (rc) {
			dev_err(ccu->dev, "fail to request ccu1 irq(%d, %d)!\n", ccu->irq1_num, rc);
			WARN_ON(1);
			return -ENODEV;
		}
	}
#endif
#endif

	/*1. Set CCU run*/
	ret = mtk_ccu_run(ccu);

	if (ret) {
		mtk_ccu_stopx(rproc, false);
		WARN_ON(1);
	}

	return ret;
}

void *mtk_ccu_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	struct mtk_ccu *ccu = (struct mtk_ccu *)rproc->priv;
	struct device *dev = ccu->dev;
	int offset = 0;
#if !defined(SECURE_CCU)
	struct mtk_ccu_mem_info *bin_mem = mtk_ccu_get_meminfo(ccu, MTK_CCU_DDR);
#endif

	if (da < ccu->ccu_sram_offset) {
		offset = da;
		if (offset >= 0 && (offset + len) <= ccu->ccu_sram_size)
			return ccu->pmem_base + offset;
	} else if (da < MTK_CCU_CACHE_BASE) {
		offset = da - ccu->ccu_sram_offset;
		if (offset >= 0 && (offset + len) <= ccu->ccu_sram_size)
			return ccu->dmem_base + offset;
	}
#if !defined(SECURE_CCU)
	else {
		offset = da - MTK_CCU_CACHE_BASE;
		if (!bin_mem) {
			dev_err(dev, "get binary memory info failed\n");
			return NULL;
		}
		if (offset >= 0 && (offset + len) <= MTK_CCU_CACHE_SIZE)
			return bin_mem->va + offset;
	}
#endif

	dev_err(dev, "failed lookup da(0x%llx) len(0x%zx) to va, offset(%x)\n",
		da, len, offset);
	return NULL;
}
EXPORT_SYMBOL_GPL(mtk_ccu_da_to_va);

static void mtk_ccu_stop_mssv(void *ccu_base)
{
	int32_t timeout = 100;

	if (readl(ccu_base + MTK_CCU_SPARE_REG08) != CCU_STATUS_INIT_DONE) {
		LOG_DBG("ccu_stop: CCU@%llx not init OK\n", (uint64_t) ccu_base);
		return;
	}

	writel(0xFFFFFFFF, ccu_base + MTK_CCU_SPARE_REG09);
	while ((readl(ccu_base + MTK_CCU_SPARE_REG09)) && (timeout >= 0)) {
		usleep_range(50, 100);
		timeout = timeout - 1;
	}

	LOG_DBG("ccu_stop: CCU@%llx stop_reg:%08x\n",
		(uint64_t)ccu_base, readl(ccu_base + MTK_CCU_SPARE_REG09));
}


static int mtk_ccu_stopx(struct rproc *rproc, bool normal_stop)
{
	struct mtk_ccu *ccu = (struct mtk_ccu *)rproc->priv;
	/* struct device *dev = &rproc->dev; */
	int ret, i;
#if defined(SECURE_CCU)
	struct arm_smccc_res res;
#else
	int ccu_reset;
#endif

	/* notify CCU to shutdown*/
	mtk_ccu_stop_mssv(ccu->ccu_base);
	LOG_DBG("CCU do 0x%08x times\n", readl(ccu->ccu_base + MTK_CCU_SPARE_REG03));
	if (ccu->ccu_cores == 2) {
		mtk_ccu_stop_mssv(ccu->ccu1_base);
		LOG_DBG("CCU1 do 0x%08x times\n", readl(ccu->ccu1_base + MTK_CCU_SPARE_REG03));
	}

	mtk_ccu_sw_hw_reset(ccu);

	ccu->disirq = true;

#if !defined(SECURE_CCU)
	for (i = 0; i < ccu->ccu_cores; ++i) {
		ret = mtk_ccu_deallocate_mem(ccu->dev, &ccu->buffer_handle[i],
			ccu->smmu_enabled);
		if (ret)
			dev_err(ccu->dev, "dealloc ccu%d mem 0x%x failed\n",
				i, ccu->buffer_handle[i].meminfo.size);
	}

#endif

#if defined(SECURE_CCU)
	writel(CCU_GO_TO_STOP, ccu->ccu_base + MTK_CCU_SPARE_REG06);
#ifdef CONFIG_ARM64
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u64) CCU_SMC_REQ_STOP,
		0, 0, 0, 0, 0, 0, &res);
#endif
#ifdef CONFIG_ARM_PSCI
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u32) CCU_SMC_REQ_STOP,
		0, 0, 0, 0, 0, 0, &res);
#endif
	if (res.a0 != 0)
		dev_err(ccu->dev, "stop CCU failed (%lu).\n", res.a0);
	else
		LOG_DBG("stop CCU OK\n");
#else
	ccu_reset = readl(ccu->ccu_base + MTK_CCU_REG_RESET);
	writel(ccu_reset | MTK_CCU_HW_RESET_BIT, ccu->ccu_base + MTK_CCU_REG_RESET);
	if (ccu->ccu_cores == 2) {
		ccu_reset = readl(ccu->ccu1_base + MTK_CCU_REG_RESET);
		writel(ccu_reset | MTK_CCU_HW_RESET_BIT, ccu->ccu1_base + MTK_CCU_REG_RESET);
	}
#endif

#if defined(CCU_SET_MMQOS)
	mtk_icc_set_bw(ccu->path_ccuo, MBps_to_icc(0), MBps_to_icc(0));
	mtk_icc_set_bw(ccu->path_ccui, MBps_to_icc(0), MBps_to_icc(0));
	if (ccu->ccu_version < CCU_VER_ISP7SP)
		mtk_icc_set_bw(ccu->path_ccug, MBps_to_icc(0), MBps_to_icc(0));
#endif

#ifdef REQUEST_IRQ_IN_INIT
	spin_lock(&ccu->ccu_irq_lock);
	if (ccu->disirq) {
		ccu->disirq = false;
		spin_unlock(&ccu->ccu_irq_lock);
		disable_irq(ccu->irq_num);
#if (CCU1_IRQ)
		if (ccu->ccu_cores == 2)
			disable_irq(ccu->irq1_num);
#endif
	} else
		spin_unlock(&ccu->ccu_irq_lock);
#else
	devm_free_irq(ccu->dev, ccu->irq_num, ccu);
	if (ccu->ccu_cores == 2)
		devm_free_irq(ccu->dev, ccu->irq1_num, ccu);
#endif

	spin_lock(&ccu->ccu_poweron_lock);
	ccu->poweron = false;
	spin_unlock(&ccu->ccu_poweron_lock);

	mtk_ccu_clk_unprepare(ccu);
	return 0;
}

static int mtk_ccu_stop(struct rproc *rproc)
{
	return mtk_ccu_stopx(rproc, true);
}

#if !defined(SECURE_CCU)
static int ccu_load_binary(struct rproc *rproc, uint32_t ccu_no,
	const unsigned int *pmem_ptr, uint32_t pmem_size,
	const unsigned int *dmem_ptr, uint32_t dmem_size,
	const unsigned int *ddr_ptr, uint32_t ddr_size)
{
	struct device *dev = &rproc->dev;
	struct mtk_ccu *ccu = rproc->priv;
	uint8_t *ccu_base;
	void *pmem, *dmem, *ddr;
	unsigned int status;
	int timeout = 10;

	if (!ccu)
		return -1;

	if (ccu_no >= ccu->ccu_cores)
		return -2;

	if (ccu_no) {
		ccu_base = (uint8_t *)ccu->ccu1_base;
		pmem = (void *)ccu->pmem1_base;
		dmem = (void *)ccu->dmem1_base;
	} else {
		ccu_base = (uint8_t *)ccu->ccu_base;
		pmem = (void *)ccu->pmem_base;
		dmem = (void *)ccu->dmem_base;
	}

	ddr = (void *)ccu->buffer_handle[ccu_no].meminfo.va;

	/* 1. Halt CCU HW before load binary */
	writel(((ccu->ccu_version >= CCU_VER_ISP7SP)) ?
		MTK_CCU_HW_RESET_BIT_ISP7SP : MTK_CCU_HW_RESET_BIT,
		ccu_base + MTK_CCU_REG_RESET);
	udelay(10);

	/* 2. Polling CCU HW status until ready */
	status = readl(ccu_base + MTK_CCU_REG_RESET);
	while ((status & 0x3) != 0x3) {
		status = readl(ccu_base + MTK_CCU_REG_RESET);
		udelay(300);
		if (timeout < 0 && ((status & 0x3) != 0x3)) {
			dev_err(dev, "wait ccu%d halt before load bin, timeout", ccu_no);
			return -EFAULT;
		}
		timeout--;
	}

	mtk_ccu_memcpy(pmem, pmem_ptr, pmem_size);
	mtk_ccu_memcpy(dmem, dmem_ptr, dmem_size);
	mtk_ccu_memcpy(ddr, ddr_ptr, ddr_size);

	return 0;
}
#endif

static int mtk_ccu_load(struct rproc *rproc, const struct firmware *fw)
{
	struct mtk_ccu *ccu = rproc->priv;
	int i, ret;
#if defined(SECURE_CCU)
	unsigned int clks;
	struct arm_smccc_res res;
	int rc;
	char error_desc[ERROR_DESC_LEN];
#endif

	/*1. prepare CCU's clks & power*/
	ret = mtk_ccu_clk_prepare(ccu);
	if (ret) {
		dev_err(ccu->dev, "failed to prepare ccu clocks\n");
		return ret;
	}

	LOG_DBG("Load CCU binary start\n");

#if defined(SECURE_CCU)
	writel(CCU_GO_TO_LOAD, ccu->ccu_base + MTK_CCU_SPARE_REG06);
#ifdef CONFIG_ARM64
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u64) CCU_SMC_REQ_LOAD,
		0, 0, 0, 0, 0, 0, &res);
#endif
#ifdef CONFIG_ARM_PSCI
	arm_smccc_smc(MTK_SIP_KERNEL_CCU_CONTROL, (u32) CCU_SMC_REQ_LOAD,
		0, 0, 0, 0, 0, 0, &res);
#endif
	ret = (int)(res.a0);
	if (ret != 0) {
		for (i = 0, clks = 0; i < ccu->clock_num; ++i)
			if (__clk_is_enabled(ccu->ccu_clk_pwr_ctrl[i]))
				clks |= (1 << i);
		rc = snprintf(error_desc, ERROR_DESC_LEN, "load CCU binary fail(%d), clock: 0x%x",
			ret, clks);
		if (rc < 0) {
			strncpy(error_desc, "load CCU binary fail", ERROR_DESC_LEN);
			error_desc[ERROR_DESC_LEN - 1] = 0;
		}
		dev_err(ccu->dev, "%s\n", error_desc);
		goto ccu_load_err;
	} else
		LOG_DBG("load CCU binary OK\n");
#else
	/*2. allocate CCU's dram memory if needed*/
	if (ccu->ccu_cores > MTK_CCU_BUF_MAX) {
		dev_err(ccu->dev, "core number %d > MTK_CCU_BUF_MAX(%d)\n",
			ccu->ccu_cores, MTK_CCU_BUF_MAX);
		ccu->ccu_cores = MTK_CCU_BUF_MAX;
	}

	for (i = 0; i < ccu->ccu_cores; ++i) {
		ccu->buffer_handle[i].meminfo.size = CCU_DDR_SIZE;
		ccu->buffer_handle[i].meminfo.cached	= false;
		ret = mtk_ccu_allocate_mem(ccu->dev, &ccu->buffer_handle[i],
			ccu->smmu_enabled);
		if (ret) {
			dev_err(ccu->dev, "alloc ccu%d mem 0x%x failed\n",
				i, ccu->buffer_handle[i].meminfo.size);
			goto ccu_load_err;
		}
	}

	/*3. load binary*/
	for (i = 0; i < ccu->ccu_cores; ++i) {
		if (i)
			ret = ccu_load_binary(rproc, i,
				ccu_isp7sp_pmem_b, sizeof(ccu_isp7sp_pmem_b),
				ccu_isp7sp_dmem_b, sizeof(ccu_isp7sp_dmem_b),
				ccu_isp7sp_ddr_b, sizeof(ccu_isp7sp_ddr_b));
		else
			switch (ccu->ccu_version) {
			case CCU_VER_ISP7SP:
				ret = ccu_load_binary(rproc, i,
					ccu_isp7sp_pmem, sizeof(ccu_isp7sp_pmem),
					ccu_isp7sp_dmem, sizeof(ccu_isp7sp_dmem),
					ccu_isp7sp_ddr, sizeof(ccu_isp7sp_ddr));
				break;
			case CCU_VER_ISP7SPL:
				ret = ccu_load_binary(rproc, i,
					ccu_isp7spl_pmem, sizeof(ccu_isp7spl_pmem),
					ccu_isp7sp_dmem, sizeof(ccu_isp7sp_dmem),
					ccu_isp7sp_ddr, sizeof(ccu_isp7sp_ddr));
				break;
			}

		if (ret) {
			i = ccu->ccu_cores;
			goto ccu_load_err;
		}
	}
#endif

	return ret;

ccu_load_err:
	for (; i > 0;) {
		--i;
		mtk_ccu_deallocate_mem(ccu->dev, &ccu->buffer_handle[i],
			ccu->smmu_enabled);
	}
	mtk_ccu_clk_unprepare(ccu);

	WARN_ON(1);

	return ret;
}

static int
mtk_ccu_sanity_check(struct rproc *rproc, const struct firmware *fw)
{
#if !defined(SECURE_CCU) && (0)
	struct mtk_ccu *ccu = rproc->priv;
	const char *name = rproc->firmware;
	struct elf32_hdr *ehdr;
	char class;

	if (!fw) {
		dev_err(ccu->dev, "failed to load %s\n", name);
		return -EINVAL;
	}

	if (fw->size < sizeof(struct elf32_hdr)) {
		dev_err(ccu->dev, "Image is too small\n");
		return -EINVAL;
	}

	ehdr = (struct elf32_hdr *)fw->data;

	/* We only support ELF32 at this point */
	class = ehdr->e_ident[EI_CLASS];
	if (class != ELFCLASS32) {
		dev_err(ccu->dev, "Unsupported class: %d\n", class);
		return -EINVAL;
	}

	/* We assume the firmware has the same endianness as the host */
# ifdef __LITTLE_ENDIAN
	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
# else /* BIG ENDIAN */
	if (ehdr->e_ident[EI_DATA] != ELFDATA2MSB) {
# endif
		dev_err(ccu->dev, "Unsupported firmware endianness\n");
		return -EINVAL;
	}

	if (fw->size < ehdr->e_shoff + sizeof(struct elf32_shdr)) {
		dev_err(ccu->dev, "Image is too small\n");
		return -EINVAL;
	}

	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		dev_err(ccu->dev, "Image is corrupted (bad magic)\n");
		return -EINVAL;
	}

	if (ehdr->e_phnum == 0) {
		dev_err(ccu->dev, "No loadable segments\n");
		return -EINVAL;
	}

	if (ehdr->e_phoff > fw->size) {
		dev_err(ccu->dev, "Firmware size is too small\n");
		return -EINVAL;
	}
#endif

	return 0;
}

static const struct rproc_ops ccu_ops = {
	.start = mtk_ccu_start,
	.stop  = mtk_ccu_stop,
	.da_to_va = mtk_ccu_da_to_va,
	.load = mtk_ccu_load,
	.sanity_check = mtk_ccu_sanity_check,
};

static int mtk_ccu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct mtk_ccu *ccu;
	struct rproc *rproc;
	struct device_node *smi_node;
	struct platform_device *smi_pdev;
	uint32_t clki;
	int ret = 0;
	uint32_t phy_addr;
	uint32_t phy_size;
	static struct lock_class_key ccu_lock_key;
	const char *ccu_lock_name = "ccu_lock_class";
	struct device_link *link;
	struct device_node *node_cammainpwr;
	phandle phandle_cammainpwr;
	struct device_node *node1;
	phandle ccu_rproc1_phandle;

	rproc = rproc_alloc(dev, node->name, &ccu_ops,
		CCU_FW_NAME, sizeof(*ccu));
	if ((!rproc) || (!rproc->priv)) {
		dev_err(dev, "rproc or rproc->priv is NULL.\n");
		return -EINVAL;
	}
	lockdep_set_class_and_name(&rproc->lock, &ccu_lock_key, ccu_lock_name);
	ccu = (struct mtk_ccu *)rproc->priv;
	ccu->pdev = pdev;
	ccu->dev = &pdev->dev;
	ccu->rproc = rproc;
	ccu->log_level = LOG_DEFAULT_LEVEL;
	ccu->log_taglevel = LOG_DEFAULT_TAG;
#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG)
	ccu_rproc_ipifuzz = rproc;	/* IPIFuzz */
#endif

	platform_set_drvdata(pdev, ccu);
	ret = mtk_ccu_read_platform_info_from_dt(node, ccu);
	if (ret) {
		dev_err(ccu->dev, "Get CCU DT info fail.\n");
		return ret;
	}

	/*remap ccu_base*/
	phy_addr = ccu->ccu_hw_base;
	phy_size = ccu->ccu_hw_size;
	ccu->ccu_base = devm_ioremap(dev, phy_addr, phy_size);
	LOG_DBG("ccu_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
	LOG_DBG("ccu_base va: 0x%llx\n", (uint64_t)ccu->ccu_base);
	if (ccu->ccu_cores == 2) {
		phy_addr |= 0x100000;
		ccu->ccu1_base = devm_ioremap(dev, phy_addr, phy_size);
		LOG_DBG("ccu1_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
		LOG_DBG("ccu1_base va: 0x%llx\n", (uint64_t)ccu->ccu1_base);
	}

	/*remap dmem_base*/
	phy_addr = (ccu->ccu_hw_base & MTK_CCU_BASE_MASK) + ccu->ccu_sram_offset;
	phy_size = ccu->ccu_sram_size;
	ccu->dmem_base = devm_ioremap(dev, phy_addr, phy_size);
	LOG_DBG("dmem_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
	LOG_DBG("dmem_base va: 0x%llx\n", (uint64_t)ccu->dmem_base);
	if (ccu->ccu_cores == 2) {
		phy_addr |= 0x100000;
		ccu->dmem1_base = devm_ioremap(dev, phy_addr, phy_size);
		LOG_DBG("dmem1_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
		LOG_DBG("dmem1_base va: 0x%llx\n", (uint64_t)ccu->dmem1_base);
	}

	/*remap pmem_base*/
	phy_addr = ccu->ccu_hw_base & MTK_CCU_BASE_MASK;
	phy_size = ccu->ccu_sram_size;
	ccu->pmem_base = devm_ioremap(dev, phy_addr, phy_size);
	LOG_DBG("pmem_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
	LOG_DBG("pmem_base va: 0x%llx\n", (uint64_t)ccu->pmem_base);
	if (ccu->ccu_cores == 2) {
		phy_addr |= 0x100000;
		ccu->pmem1_base = devm_ioremap(dev, phy_addr, phy_size);
		LOG_DBG("pmem1_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
		LOG_DBG("pmem1_base va: 0x%llx\n", (uint64_t)ccu->pmem1_base);
	}

	/*remap spm_base*/
	if (ccu->ccu_version >= CCU_VER_ISP7SP) {
		phy_addr = SPM_BASE;
		phy_size = SPM_SIZE;
		ccu->spm_base = devm_ioremap(dev, phy_addr, phy_size);
		LOG_DBG("spm_base pa: 0x%x, size: 0x%x\n", phy_addr, phy_size);
		LOG_DBG("spm_base va: 0x%llx\n", (uint64_t)ccu->spm_base);
	}

	/* Get other power node if needed. */
	if (ccu->ccu_version >= CCU_VER_ISP7SP) {
		ret = of_property_read_u32(node, "mediatek,cammainpwr",
			&phandle_cammainpwr);
		node_cammainpwr = of_find_node_by_phandle(phandle_cammainpwr);
		if (node_cammainpwr)
			ccu->pdev_cammainpwr = of_find_device_by_node(node_cammainpwr);
		if (WARN_ON(!ccu->pdev_cammainpwr))
			dev_err(ccu->dev, "failed to get ccu cammainpwr pdev\n");
		else
			ccu->dev_cammainpwr = &ccu->pdev_cammainpwr->dev;
		of_node_put(node_cammainpwr);
	}
	/* get Clock control from device tree.  */
	/*
	 * SMI definition is usually not ready at bring-up stage of new platform.
	 * Continue initialization if SMI is not defined.
	 */
	smi_node = of_parse_phandle(node, "mediatek,larbs", 0);
	if (!smi_node) {
		dev_err(ccu->dev, "get smi larb from DTS fail!\n");
		/* return -ENODEV; */
	} else {
		smi_pdev = of_find_device_by_node(smi_node);
		if (WARN_ON(!smi_pdev)) {
			of_node_put(smi_node);
			return -ENODEV;
		}
		of_node_put(smi_node);

		link = device_link_add(ccu->dev, &smi_pdev->dev, DL_FLAG_PM_RUNTIME |
					DL_FLAG_STATELESS);
		if (!link) {
			dev_notice(ccu->dev, "ccu_rproc Unable to link SMI LARB\n");
			return -ENODEV;
		}
	}
	pm_runtime_enable(ccu->dev);

	ccu->clock_num = 0;
	if (ccu->ccu_version == CCU_VER_ISP7SPL)
		ccu->clock_name = ccu_clk_name_isp7spl;
	else if (ccu->ccu_version == CCU_VER_ISP7SP)
		ccu->clock_name = ccu_clk_name_isp7sp;
	else if (ccu->ccu_version == CCU_VER_ISP7S)
		ccu->clock_name = ccu_clk_name_isp7s;
	else
		ccu->clock_name = ccu_clk_name_isp71;

	for (clki = 0; clki < MTK_CCU_CLK_PWR_NUM; ++clki) {
		if (!ccu->clock_name[clki].enable)
			break;
		ccu->ccu_clk_pwr_ctrl[clki] = devm_clk_get(ccu->dev,
			ccu->clock_name[clki].name);
		if (IS_ERR(ccu->ccu_clk_pwr_ctrl[clki])) {
			dev_err(ccu->dev, "Get %s fail.\n", ccu->clock_name[clki].name);
			return PTR_ERR(ccu->ccu_clk_pwr_ctrl[clki]);
		}
	}

	if (clki >= MTK_CCU_CLK_PWR_NUM) {
		dev_err(ccu->dev, "ccu_clk_pwr_ctrl[] too small(%d).\n", clki);
		return -EINVAL;
	}

	ccu->clock_num = clki;
	LOG_DBG("CCU got %d clocks\n", clki);

#if defined(CCU_SET_MMQOS)
	if (ccu->ccu_version < CCU_VER_ISP7SP)
		ccu->path_ccug = of_mtk_icc_get(ccu->dev, "ccu_g");
	ccu->path_ccuo = of_mtk_icc_get(ccu->dev, "ccu_o");
	ccu->path_ccui = of_mtk_icc_get(ccu->dev, "ccu_i");
#endif

	ret = of_property_read_u32(node, "mediatek,ccu_rproc1",
		&ccu_rproc1_phandle);
	node1 = of_find_node_by_phandle(ccu_rproc1_phandle);
	if (node1)
		ccu->pdev1 = of_find_device_by_node(node1);
	if (WARN_ON(!ccu->pdev1)) {
		dev_err(ccu->dev, "failed to get ccu rproc1 pdev\n");
		of_node_put(node1);
	}

	/* get irq from device irq*/
	ccu->irq_num = irq_of_parse_and_map(node, 0);
	LOG_DBG("got ccu irq_num: %d\n", ccu->irq_num);
#if (CCU1_IRQ)
	if (ccu->ccu_cores == 2) {
		ccu->irq1_num = irq_of_parse_and_map(node, 1);
		LOG_DBG("got ccu1 irq_num: %d\n", ccu->irq1_num);
	}
#endif

	/*prepare mutex & log's waitqueuehead*/
	mutex_init(&ccu->ipc_desc_lock);
	spin_lock_init(&ccu->ipc_send_lock);
	spin_lock_init(&ccu->ccu_poweron_lock);
	spin_lock_init(&ccu->ccu_irq_lock);
	init_waitqueue_head(&ccu->WaitQueueHead);

#ifdef REQUEST_IRQ_IN_INIT
	ccu->disirq = true;
	ret = devm_request_threaded_irq(ccu->dev, ccu->irq_num, NULL,
		mtk_ccu_isr_handler, IRQF_ONESHOT, "ccu_rproc", ccu);
	if (ret) {
		dev_err(ccu->dev, "fail to req ccu irq(%d,%d)!\n", ccu->irq_num, ret);
		return -ENODEV;
	}
#if (CCU1_IRQ)
	if (ccu->ccu_cores == 2) {
		ret = devm_request_threaded_irq(ccu->dev, ccu->irq1_num, NULL,
			mtk_ccu1_isr_handler, IRQF_ONESHOT, "ccu_rproc", ccu);
		if (ret) {
			dev_err(ccu->dev, "fail to req ccu1 irq(%d,%d)!\n", ccu->irq1_num, ret);
			return -ENODEV;
		}
	}
#endif

	spin_lock(&ccu->ccu_irq_lock);
	if (ccu->disirq) {
		ccu->disirq = false;
		spin_unlock(&ccu->ccu_irq_lock);
		disable_irq(ccu->irq_num);
#if (CCU1_IRQ)
		if (ccu->ccu_cores == 2)
			disable_irq(ccu->irq1_num);
#endif
	} else
		spin_unlock(&ccu->ccu_irq_lock);
#endif

#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG) && (0)
	/*register char dev for log ioctl*/
	ret = mtk_ccu_reg_chardev(ccu);
	if (ret)
		dev_err(ccu->dev, "failed to regist char dev");
#endif

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
	if (ret) {
		dev_err(ccu->dev, "CCU 34-bit DMA enable failed\n");
		return ret;
	}

	ccu->smmu_enabled = smmu_v3_enabled();

	rproc->auto_boot = false;

	ret = rproc_add(rproc);
	return ret;
}

static int mtk_ccu_remove(struct platform_device *pdev)
{
	struct mtk_ccu *ccu = platform_get_drvdata(pdev);

	/*
	 * WARNING:
	 * With mrdump, remove CCU module will cause access violation
	 * at KE/SystemAPI.
	 */
	mtk_ccu_deallocate_mem(ccu->dev, &ccu->ext_buf, ccu->smmu_enabled);
#if defined(REQUEST_IRQ_IN_INIT)
	devm_free_irq(ccu->dev, ccu->irq_num, ccu);
#if (CCU1_IRQ)
	if (ccu->ccu_cores == 2)
		devm_free_irq(ccu->dev, ccu->irq1_num, ccu);
#endif
#endif
	rproc_del(ccu->rproc);
	rproc_free(ccu->rproc);
	if (ccu->ccu_version >= CCU_VER_ISP7SP)
		pm_runtime_disable(ccu->dev_cammainpwr);
	pm_runtime_disable(ccu->dev);
#if IS_ENABLED(CONFIG_MTK_CCU_DEBUG) && (0)
	mtk_ccu_unreg_chardev(ccu);
#endif
	return 0;
}

static int mtk_ccu_read_platform_info_from_dt(struct device_node
		*node, struct mtk_ccu *ccu)
{
	uint32_t reg[4] = {0, 0, 0, 0};
	int ret = 0;

	ret = of_property_read_u32_array(node, "reg", reg, 4);
	if (ret < 0)
		return -1;

	ccu->ccu_hw_base = reg[1];
	ccu->ccu_hw_size = reg[3];

	ret = of_property_read_u32(node, "ccu_version", reg);
	ccu->ccu_version = (ret < 0) ? CCU_VER_ISP71 : reg[0];

	ret = of_property_read_u32(node, "ccu_sramSize", reg);
	ccu->ccu_sram_size = (ret < 0) ? MTK_CCU_PMEM_SIZE : reg[0];

	ret = of_property_read_u32(node, "ccu_sramOffset", reg);
	ccu->ccu_sram_offset = (ret < 0) ?
		((ccu->ccu_version >= CCU_VER_ISP7SP) ?
		MTK_CCU_CORE_DMEM_BASE_ISP7SP : MTK_CCU_CORE_DMEM_BASE) : reg[0];

	if (ccu->ccu_version >= CCU_VER_ISP7SP) {
		ret = of_property_read_u32(node, "ccu-sramcon-offset", reg);
		ccu->ccu_sram_con_offset = (ret < 0) ? CCU_SLEEP_SRAM_CON : reg[0];
	}

	ret = of_property_read_u32(node, "ccu-cores", reg);
	ccu->ccu_cores = (ret < 0) ? 1 : reg[0];

	ret = of_property_read_u32(node, "compact-ipc", reg);
	ccu->compact_ipc = (ret < 0) ? false : (reg[0] != 0);

	return 0;
}

static int mtk_ccu_cam_main_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pm_runtime_enable(dev);
	return 0;
}

static int mtk_ccu_cam_main_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pm_runtime_disable(dev);
	return 0;
}

static int mtk_ccu1_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	uint32_t reg[4] = {0, 0, 0, 0};
	int ret;

	ret = of_property_read_u32(node, "ccu-cores", reg);
	ccu_cores = (ret < 0) ? 1 : reg[0];

	LOG_DBG("ccu1 ccu_cores = %d\n", ccu_cores);

	if (ccu_cores == 2) {
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
		if (ret) {
			dev_err(dev, "CCU1 34-bit DMA enable failed\n");
			return ret;
		}

		pm_runtime_enable(dev);
	}

	return 0;
}

static int mtk_ccu1_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	if (ccu_cores == 2)
		pm_runtime_disable(dev);

	return 0;
}

static int mtk_ccu_get_power(struct mtk_ccu *ccu, struct device *dev)
{
	uint8_t *sram_con;
	int rc, ret = pm_runtime_get_sync(dev);

	if (ret < 0) {
		dev_err(dev, "pm_runtime_get_sync failed %d", ret);
		return ret;
	}

	if (ccu->ccu_version >= CCU_VER_ISP7SP) {
		rc = pm_runtime_get_sync(ccu->dev_cammainpwr);
		LOG_DBG("CCU power-on cammainpwr %d\n", rc);
		ccu->cammainpwr_powered = (rc >= 0);

		sram_con = ((uint8_t *)ccu->spm_base)+ccu->ccu_sram_con_offset;
		writel(readl(sram_con) & ~CCU_SLEEP_SRAM_PDN, sram_con);
	}

	return ret;
}

static void mtk_ccu_put_power(struct mtk_ccu *ccu, struct device *dev)
{
	uint8_t *sram_con;
	int ret;

	if (ccu->ccu_version >= CCU_VER_ISP7SP) {
		sram_con = ((uint8_t *)ccu->spm_base)+ccu->ccu_sram_con_offset;
		writel(readl(sram_con) | CCU_SLEEP_SRAM_PDN, sram_con);

		if (ccu->cammainpwr_powered) {
			ret = pm_runtime_put_sync(ccu->dev_cammainpwr);
			if (ret < 0)
				dev_err(dev, "pm_runtime_put_sync cammainpwr failed %d", ret);
			ccu->cammainpwr_powered = false;
		}
	}

	ret = pm_runtime_put_sync(dev);
	if (ret < 0)
		dev_err(dev, "pm_runtime_put_sync failed %d", ret);
}

static const struct of_device_id mtk_ccu_of_ids[] = {
	{.compatible = "mediatek,ccu_rproc", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_ccu_of_ids);

static struct platform_driver ccu_rproc_driver = {
	.probe = mtk_ccu_probe,
	.remove = mtk_ccu_remove,
	.driver = {
		.name = MTK_CCU_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_ccu_of_ids),
	},
};

static const struct of_device_id mtk_ccu_cam_main_of_ids[] = {
	{.compatible = "mediatek,ccucammain", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_ccu_cam_main_of_ids);

static struct platform_driver ccu_cam_main_driver = {
	.probe = mtk_ccu_cam_main_probe,
	.remove = mtk_ccu_cam_main_remove,
	.driver = {
		.name = MTK_CCU_CAM_MAIN_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_ccu_cam_main_of_ids),
	},
};

static const struct of_device_id mtk_ccu1_of_ids[] = {
	{.compatible = "mediatek,ccu_rproc1", },
	{},
};
MODULE_DEVICE_TABLE(of, mtk_ccu1_of_ids);

static struct platform_driver ccu_rproc1_driver = {
	.probe = mtk_ccu1_probe,
	.remove = mtk_ccu1_remove,
	.driver = {
		.name = MTK_CCU1_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_ccu1_of_ids),
	},
};

static int __init ccu_init(void)
{
	platform_driver_register(&ccu_cam_main_driver);
	platform_driver_register(&ccu_rproc_driver);
	platform_driver_register(&ccu_rproc1_driver);
	return 0;
}

static void __exit ccu_exit(void)
{
	platform_driver_unregister(&ccu_cam_main_driver);
	platform_driver_unregister(&ccu_rproc_driver);
	platform_driver_unregister(&ccu_rproc1_driver);
}

module_init(ccu_init);
module_exit(ccu_exit);

MODULE_IMPORT_NS(DMA_BUF);

MODULE_DESCRIPTION("MTK CCU Rproc Driver");
MODULE_LICENSE("GPL");
