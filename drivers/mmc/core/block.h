/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _MMC_CORE_BLOCK_H
#define _MMC_CORE_BLOCK_H

#define MMC_SBC_OFFSET 0
#define MMC_CMD_OFFSET 2
#define MMC_DATA_OFFSET 4
#define MMC_STOP_OFFSET 6
#define MMC_BUSY_OFFSET 8

struct mmc_queue;
struct request;

void mmc_blk_issue_rq(struct mmc_queue *mq, struct request *req);
void mmc_error_count_log(struct mmc_card *card, int index, int error, u32 status);

#endif
