#ifndef _FFU_H_
#define _FFU_H_


#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/ffu.h>
#include <linux/slab.h>

 /*
 * eMMC5.0 Field Firmware Update (FFU) opcodes
 */
#define MMC_FFU_DOWNLOAD_OP 302
#define MMC_FFU_INSTALL_OP  303

#define MMC_FFU_MODE_SET    0x1
#define MMC_FFU_MODE_NORMAL 0x0
#define MMC_FFU_INSTALL_SET 0x1


#ifdef CONFIG_MMC_FFU_FUNCTION
#define MMC_FFU_ENABLE      0x0
#define MMC_FFU_FW_CONFIG      0x1
#define MMC_FFU_SUPPORTED_MODES 0x1
#define MMC_FFU_FEATURES    0x1

void mmc_wait_for_ffu_req(struct mmc_host *host, struct mmc_request *mrq);

#endif


#endif