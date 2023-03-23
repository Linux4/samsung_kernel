/*
 * Copyright (c) 2015, 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SDHCI_BH201_H__
#define __SDHCI_BH201_H__
#define IFLYTEK 1


#if IFLYTEK
#include "sprd-sdhcr11.h"
#else
#include "sdhci-msm.h"
#include <linux/leds.h>
#endif 


#define SDHCI_SOFTWARE_RESET	0x2F
#define  SDHCI_RESET_ALL	0x01
#define  SDHCI_RESET_CMD	0x02
#define  SDHCI_RESET_DATA	0x04

#if IFLYTEK
#define GPIO_SD_CARD_PWR	(31)
#define GPIO_SD_CARD_PRESENT (78)
#else
#define GPIO_SD_CARD_PWR	(12)
#define GPIO_SD_CARD_PRESENT (69)
#endif

#define	SD_FNC_AM_SDR12		0x0
#define	SD_FNC_AM_SDR25		0x1
#define	SD_FNC_AM_HS		0x1
#define	SD_FNC_AM_DS		0x0

#define	SD_FNC_AM_SDR50		0x2
#define	SD_FNC_AM_SDR104	0x3
#define	SD_FNC_AM_DDR50		0x4

#define BIT_PASS_MASK  (0x7ff )
#define SDR104_MANUAL_INJECT 0x3ff
#define SDR50_MANUAL_INJECT  0x77f

#define  TRUNING_RING_IDX(x)  ((x)% TUNING_PHASE_SIZE)
#define  GET_IDX_VALUE(tb, x)  (tb &(1 << (x)))
#define  GENERATE_IDX_VALUE( x)  (1 << (x))
#define  GET_TRUNING_RING_IDX_VALUE(tb, x)  (tb &(1 << TRUNING_RING_IDX(x)))
#define  GENERATE_TRUNING_RING_IDX_VALUE( x)  (1 << TRUNING_RING_IDX(x))

#if IFLYTEK
/* Allow for a a command request and a data request at the same time */
#define SDHCI_MAX_MRQS		2



struct sdhci_host {
	/* Data set by hardware interface driver */
	const char *hw_name;	/* Hardware bus name */

	unsigned int quirks;	/* Deviations from spec. */

/* Controller doesn't honor resets unless we touch the clock register */
#define SDHCI_QUIRK_CLOCK_BEFORE_RESET			(1<<0)
/* Controller has bad caps bits, but really supports DMA */
#define SDHCI_QUIRK_FORCE_DMA				(1<<1)
/* Controller doesn't like to be reset when there is no card inserted. */
#define SDHCI_QUIRK_NO_CARD_NO_RESET			(1<<2)
/* Controller doesn't like clearing the power reg before a change */
#define SDHCI_QUIRK_SINGLE_POWER_WRITE			(1<<3)
/* Controller has flaky internal state so reset it on each ios change */
#define SDHCI_QUIRK_RESET_CMD_DATA_ON_IOS		(1<<4)
/* Controller has an unusable DMA engine */
#define SDHCI_QUIRK_BROKEN_DMA				(1<<5)
/* Controller has an unusable ADMA engine */
#define SDHCI_QUIRK_BROKEN_ADMA				(1<<6)
/* Controller can only DMA from 32-bit aligned addresses */
#define SDHCI_QUIRK_32BIT_DMA_ADDR			(1<<7)
/* Controller can only DMA chunk sizes that are a multiple of 32 bits */
#define SDHCI_QUIRK_32BIT_DMA_SIZE			(1<<8)
/* Controller can only ADMA chunks that are a multiple of 32 bits */
#define SDHCI_QUIRK_32BIT_ADMA_SIZE			(1<<9)
/* Controller needs to be reset after each request to stay stable */
#define SDHCI_QUIRK_RESET_AFTER_REQUEST			(1<<10)
/* Controller needs voltage and power writes to happen separately */
#define SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER		(1<<11)
/* Controller provides an incorrect timeout value for transfers */
#define SDHCI_QUIRK_BROKEN_TIMEOUT_VAL			(1<<12)
/* Controller has an issue with buffer bits for small transfers */
#define SDHCI_QUIRK_BROKEN_SMALL_PIO			(1<<13)
/* Controller does not provide transfer-complete interrupt when not busy */
#define SDHCI_QUIRK_NO_BUSY_IRQ				(1<<14)
/* Controller has unreliable card detection */
#define SDHCI_QUIRK_BROKEN_CARD_DETECTION		(1<<15)
/* Controller reports inverted write-protect state */
#define SDHCI_QUIRK_INVERTED_WRITE_PROTECT		(1<<16)
/* Controller does not like fast PIO transfers */
#define SDHCI_QUIRK_PIO_NEEDS_DELAY			(1<<18)
/* Controller has to be forced to use block size of 2048 bytes */
#define SDHCI_QUIRK_FORCE_BLK_SZ_2048			(1<<20)
/* Controller cannot do multi-block transfers */
#define SDHCI_QUIRK_NO_MULTIBLOCK			(1<<21)
/* Controller can only handle 1-bit data transfers */
#define SDHCI_QUIRK_FORCE_1_BIT_DATA			(1<<22)
/* Controller needs 10ms delay between applying power and clock */
#define SDHCI_QUIRK_DELAY_AFTER_POWER			(1<<23)
/* Controller uses SDCLK instead of TMCLK for data timeouts */
#define SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK		(1<<24)
/* Controller reports wrong base clock capability */
#define SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN		(1<<25)
/* Controller cannot support End Attribute in NOP ADMA descriptor */
#define SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC		(1<<26)
/* Controller is missing device caps. Use caps provided by host */
#define SDHCI_QUIRK_MISSING_CAPS			(1<<27)
/* Controller uses Auto CMD12 command to stop the transfer */
#define SDHCI_QUIRK_MULTIBLOCK_READ_ACMD12		(1<<28)
/* Controller doesn't have HISPD bit field in HI-SPEED SD card */
#define SDHCI_QUIRK_NO_HISPD_BIT			(1<<29)
/* Controller treats ADMA descriptors with length 0000h incorrectly */
#define SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC		(1<<30)
/* The read-only detection via SDHCI_PRESENT_STATE register is unstable */
#define SDHCI_QUIRK_UNSTABLE_RO_DETECT			(1<<31)

	unsigned int quirks2;	/* More deviations from spec. */

#define SDHCI_QUIRK2_HOST_OFF_CARD_ON			(1<<0)
#define SDHCI_QUIRK2_HOST_NO_CMD23			(1<<1)
/* The system physically doesn't support 1.8v, even if the host does */
#define SDHCI_QUIRK2_NO_1_8_V				(1<<2)
#define SDHCI_QUIRK2_PRESET_VALUE_BROKEN		(1<<3)
#define SDHCI_QUIRK2_CARD_ON_NEEDS_BUS_ON		(1<<4)
/* Controller has a non-standard host control register */
#define SDHCI_QUIRK2_BROKEN_HOST_CONTROL		(1<<5)
/* Controller does not support HS200 */
#define SDHCI_QUIRK2_BROKEN_HS200			(1<<6)
/* Controller does not support DDR50 */
#define SDHCI_QUIRK2_BROKEN_DDR50			(1<<7)
/* Stop command (CMD12) can set Transfer Complete when not using MMC_RSP_BUSY */
#define SDHCI_QUIRK2_STOP_WITH_TC			(1<<8)
/* Controller does not support 64-bit DMA */
#define SDHCI_QUIRK2_BROKEN_64_BIT_DMA			(1<<9)
/* need clear transfer mode register before send cmd */
#define SDHCI_QUIRK2_CLEAR_TRANSFERMODE_REG_BEFORE_CMD	(1<<10)
/* Capability register bit-63 indicates HS400 support */
#define SDHCI_QUIRK2_CAPS_BIT63_FOR_HS400		(1<<11)
/* forced tuned clock */
#define SDHCI_QUIRK2_TUNING_WORK_AROUND			(1<<12)
/* disable the block count for single block transactions */
#define SDHCI_QUIRK2_SUPPORT_SINGLE			(1<<13)
/* Controller broken with using ACMD23 */
#define SDHCI_QUIRK2_ACMD23_BROKEN			(1<<14)
/* Broken Clock divider zero in controller */
#define SDHCI_QUIRK2_CLOCK_DIV_ZERO_BROKEN		(1<<15)
/* Controller has CRC in 136 bit Command Response */
#define SDHCI_QUIRK2_RSP_136_HAS_CRC			(1<<16)

	int irq;		/* Device IRQ */
	void __iomem *ioaddr;	/* Mapped address */
	char *bounce_buffer;	/* For packing SDMA reads/writes */
	dma_addr_t bounce_addr;
	unsigned int bounce_buffer_size;

//	const struct sdhci_ops *ops;	/* Low level hw interface */

	/* Internal data */
	struct mmc_host *mmc;	/* MMC structure */
	struct mmc_host_ops mmc_host_ops;	/* MMC host ops */
	u64 dma_mask;		/* custom DMA mask */

#if IS_ENABLED(CONFIG_LEDS_CLASS)
//	struct led_classdev led;	/* LED control */
//	char led_name[32];
#endif

	spinlock_t lock;	/* Mutex */

	int flags;		/* Host attributes */
#define SDHCI_USE_SDMA		(1<<0)	/* Host is SDMA capable */
#define SDHCI_USE_ADMA		(1<<1)	/* Host is ADMA capable */
#define SDHCI_REQ_USE_DMA	(1<<2)	/* Use DMA for this req. */
#define SDHCI_DEVICE_DEAD	(1<<3)	/* Device unresponsive */
#define SDHCI_SDR50_NEEDS_TUNING (1<<4)	/* SDR50 needs tuning */
#define SDHCI_AUTO_CMD12	(1<<6)	/* Auto CMD12 support */
#define SDHCI_AUTO_CMD23	(1<<7)	/* Auto CMD23 support */
#define SDHCI_PV_ENABLED	(1<<8)	/* Preset value enabled */
#define SDHCI_SDIO_IRQ_ENABLED	(1<<9)	/* SDIO irq enabled */
#define SDHCI_USE_64_BIT_DMA	(1<<12)	/* Use 64-bit DMA */
#define SDHCI_HS400_TUNING	(1<<13)	/* Tuning for HS400 */
#define SDHCI_SIGNALING_330	(1<<14)	/* Host is capable of 3.3V signaling */
#define SDHCI_SIGNALING_180	(1<<15)	/* Host is capable of 1.8V signaling */
#define SDHCI_SIGNALING_120	(1<<16)	/* Host is capable of 1.2V signaling */
#define CORE_FREQ_100MHZ  (100 * 1000 * 1000)

	unsigned int version;	/* SDHCI spec. version */

	unsigned int max_clk;	/* Max possible freq (MHz) */
	unsigned int timeout_clk;	/* Timeout freq (KHz) */
	unsigned int clk_mul;	/* Clock Muliplier value */

	unsigned int clock;	/* Current clock (MHz) */
	u8 pwr;			/* Current voltage */

	bool runtime_suspended;	/* Host is runtime suspended */
	bool bus_on;		/* Bus power prevents runtime suspend */
	bool preset_enabled;	/* Preset is enabled */
	bool pending_reset;	/* Cmd/data reset is pending */

	struct mmc_request *mrqs_done[SDHCI_MAX_MRQS];	/* Requests done */
	struct mmc_command *cmd;	/* Current command */
	struct mmc_command *data_cmd;	/* Current data command */
	struct mmc_data *data;	/* Current data request */
	unsigned int data_early:1;	/* Data finished before cmd */

	struct sg_mapping_iter sg_miter;	/* SG state for PIO */
	unsigned int blocks;	/* remaining PIO blocks */

	int sg_count;		/* Mapped sg entries */

	void *adma_table;	/* ADMA descriptor table */
	void *align_buffer;	/* Bounce buffer */

	size_t adma_table_sz;	/* ADMA descriptor table size */
	size_t align_buffer_sz;	/* Bounce buffer size */

	dma_addr_t adma_addr;	/* Mapped ADMA descr. table */
	dma_addr_t align_addr;	/* Mapped bounce buffer */

	unsigned int desc_sz;	/* ADMA descriptor size */

	struct tasklet_struct finish_tasklet;	/* Tasklet structures */

	struct timer_list timer;	/* Timer for timeouts */
	struct timer_list data_timer;	/* Timer for data timeouts */

	u32 caps;		/* CAPABILITY_0 */
	u32 caps1;		/* CAPABILITY_1 */
	bool read_caps;		/* Capability flags have been read */

	unsigned int            ocr_avail_sdio;	/* OCR bit masks */
	unsigned int            ocr_avail_sd;
	unsigned int            ocr_avail_mmc;
	u32 ocr_mask;		/* available voltages */

	unsigned		timing;		/* Current timing */

	u32			thread_isr;

	/* cached registers */
	u32			ier;

	bool			cqe_on;		/* CQE is operating */
	u32			cqe_ier;	/* CQE interrupt mask */
	u32			cqe_err_ier;	/* CQE error interrupt mask */

	wait_queue_head_t	buf_ready_int;	/* Waitqueue for Buffer Read Ready interrupt */
	unsigned int		tuning_done;	/* Condition flag set when CMD19 succeeds */

	unsigned int		tuning_count;	/* Timer count for re-tuning */
	unsigned int		tuning_mode;	/* Re-tuning mode supported by host */
#define SDHCI_TUNING_MODE_1	0
#define SDHCI_TUNING_MODE_2	1
#define SDHCI_TUNING_MODE_3	2
	/* Delay (ms) between tuning commands */
	int			tuning_delay;

	/* Host SDMA buffer boundary. */
	u32			sdma_boundary;

	unsigned long private[0] ____cacheline_aligned;
};

struct sdhci_ops {
#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS
	u32		(*read_l)(struct sdhci_host *host, int reg);
	u16		(*read_w)(struct sdhci_host *host, int reg);
	u8		(*read_b)(struct sdhci_host *host, int reg);
	void		(*write_l)(struct sdhci_host *host, u32 val, int reg);
	void		(*write_w)(struct sdhci_host *host, u16 val, int reg);
	void		(*write_b)(struct sdhci_host *host, u8 val, int reg);
#endif

	void	(*set_clock)(struct sdhci_host *host, unsigned int clock);
	void	(*set_power)(struct sdhci_host *host, unsigned char mode,
			     unsigned short vdd);

	u32		(*irq)(struct sdhci_host *host, u32 intmask);

	int		(*enable_dma)(struct sdhci_host *host);
	unsigned int	(*get_max_clock)(struct sdhci_host *host);
	unsigned int	(*get_min_clock)(struct sdhci_host *host);
	/* get_timeout_clock should return clk rate in unit of Hz */
	unsigned int	(*get_timeout_clock)(struct sdhci_host *host);
	unsigned int	(*get_max_timeout_count)(struct sdhci_host *host);
	void		(*set_timeout)(struct sdhci_host *host,
				       struct mmc_command *cmd);
	void		(*set_bus_width)(struct sdhci_host *host, int width);
	void (*platform_send_init_74_clocks)(struct sdhci_host *host,
					     u8 power_mode);
	unsigned int    (*get_ro)(struct sdhci_host *host);
	void		(*reset)(struct sdhci_host *host, u8 mask);
	int	(*platform_execute_tuning)(struct sdhci_host *host, u32 opcode);
	void	(*set_uhs_signaling)(struct sdhci_host *host, unsigned int uhs);
	void	(*hw_reset)(struct sdhci_host *host);
	void    (*adma_workaround)(struct sdhci_host *host, u32 intmask);
	void    (*card_event)(struct sdhci_host *host);
	void	(*voltage_switch)(struct sdhci_host *host);
};
#endif
// tb list
u32 all_selb_failed_tb_get(struct sdhci_host *host, int sela);
void all_selb_failed_tb_update(struct sdhci_host *host, int sela, u32 val);

u32 tx_selb_failed_tb_get(struct sdhci_host *host, int sela);
void tx_selb_failed_tb_update(struct sdhci_host *host, int sela, u32 val);

u32 tx_selb_failed_history_get(struct sdhci_host *host);
void tx_selb_failed_history_update(struct sdhci_host *host , u32 val);

void _ggc_reset_tuning_result_for_dll(struct sdhci_host * host);
void ggc_reset_selx_failed_tb(struct sdhci_host *host);
//dbg dump
void vct_2_string(u8 * tb, u32 n);
void dump_selx_vct_with_comment(char * info, u32 vct);
//func
int get_best_window_phase(u32 vct, int * max_pass_win)  ;
int get_best_window_phase_shift_left(u32 vct, int * pmax_pass_win);
bool  get_refine_sel(struct sdhci_host *host, u32 tga, u32 tgb, u32 * rfa, u32 * rfb);
bool selx_failure_point_exist(u32 val);
int get_selx_weight(u32  val);
void tx_selb_calculate_valid_phase_range(u32 val, int* start, int *pass_cnt);
int get_selb_failure_point(int start, u64 raw_tx_selb, int tuning_cnt);
void tx_selb_inject_policy(struct sdhci_host *host, int tx_selb);
void ggc_tuning_result_reset(struct sdhci_host *host); //TQY
void set_gg_reg_cur_val(u8 * data);
void get_gg_reg_cur_val(u8 * data);
void ggc_dll_voltage_init(struct sdhci_host *  host);
void ggc_chip_init(struct sdhci_host *host);



#if IFLYTEK
int sdhci_bht_execute_tuning(struct mmc_host *mmc,u32 opcode);
#else
int sdhci_bht_execute_tuning(struct sdhci_host *host, u32 opcode);
#endif

#endif
