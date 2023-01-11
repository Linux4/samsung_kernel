#ifndef _DT_BINDINGS_MMC_PXA_SDHCI_H
#define _DT_BINDINGS_MMC_PXA_SDHCI_H

/*
 * PXA platform special preset/src clock rate
 * use to preset the MMC defined clock rate
 */
#define PXA_SDH_DTR_25M 25000000
#define PXA_SDH_DTR_26M 26000000
#define PXA_SDH_DTR_45M 45000000
#define PXA_SDH_DTR_52M 52000000
#define PXA_SDH_DTR_89M 89142857
#define PXA_SDH_DTR_104M 104000000
#define PXA_SDH_DTR_156M 156000000
#define PXA_SDH_DTR_208M 208000000
#define PXA_SDH_DTR_416M 416000000
#define PXA_SDH_DTR_624M 624000000
#define PXA_SDH_DTR_PS_NONE 0xffffffff

#define PXA_MMC_TIMING_LEGACY		0
#define PXA_MMC_TIMING_UHS_SDR12	1
#define PXA_MMC_TIMING_MMC_HS		2
#define PXA_MMC_TIMING_SD_HS		3
#define PXA_MMC_TIMING_UHS_SDR25	4
#define PXA_MMC_TIMING_UHS_SDR50	5
#define PXA_MMC_TIMING_UHS_SDR104	6
#define PXA_MMC_TIMING_UHS_DDR50	7
#define PXA_MMC_TIMING_MMC_HS200	8
#define PXA_MMC_TIMING_MAX		9

/* pxa specific flag */
/* Require clock free running */
#define PXA_FLAG_ENABLE_CLOCK_GATING (1<<0)
/* card always wired to host, like on-chip emmc */
#define PXA_FLAG_CARD_PERMANENT (1<<1)
/* Board design supports 8-bit data on SD/SDIO BUS */
#define PXA_FLAG_SD_8_BIT_CAPABLE_SLOT (1<<2)
/* SDIO device/SD Card detect wakeup host sleep feature */
#define PXA_FLAG_WAKEUP_HOST (1<<3)
/* disable card scanning in probe procedure */
#define PXA_FLAG_DISABLE_PROBE_CDSCAN (1<<4)
/* whether supports RPM in driver, it can used for source clock gating */
#define PXA_FLAG_EN_PM_RUNTIME (1<<5)
/* whether Rx configure Reg is changed, like pxa988, 1088 */
#define PXA_FLAG_NEW_RX_CFG_REG (1<<6)
/* whether Tx configure support bus clock as internal clock */
#define PXA_FLAG_TX_SEL_BUS_CLK (1<<7)

/* for eMMC chip */
#define MMC_CAP2_CACHE_CTRL	(1 << 1)	/* Allow cache control */
#define SDHCI_QUIRK_BROKEN_ADMA                         (1<<6)
/* Controller enable HW bus clock gating by default */
#define SDHCI_QUIRK2_BUS_CLK_GATE_ENABLED		(1<<12)
#define MMC_CAP_1_8V_DDR        (1 << 11)
#define MMC_CAP2_HS200_1_8V_SDR				(1 << 5) /* can support */
#define MMC_CAP2_HS200_1_2V_SDR				(1 << 6) /* can support */
#define MMC_CAP2_DISABLE_BLK_ASYNC			(1 << 20)
#define MMC_CAP2_BKOPS_EN				(1 << 24)

/* Controller does not support HS200 */
#define SDHCI_QUIRK2_BROKEN_HS200			(1<<6)

/* for SD card */
#define MMC_CAP_UHS_SDR12	(1 << 15)	/* Host supports UHS SDR12 mode */
#define MMC_CAP_UHS_SDR25	(1 << 16)	/* Host supports UHS SDR25 mode */
#define MMC_CAP_UHS_SDR50	(1 << 17)	/* Host supports UHS SDR50 mode */
#define MMC_CAP_UHS_SDR104	(1 << 18)	/* Host supports UHS SDR104 mode */
#define MMC_CAP_UHS_DDR50	(1 << 19)	/* Host supports UHS DDR50 mode */

#define SDHCI_QUIRK_INVERTED_WRITE_PROTECT              (1<<16)

/* some SD host need to set IO capability by SOC part register */
#define SDHCI_QUIRK2_SET_AIB_MMC			(1<<14)

/* for SDIO card */
/* Controller must enable clock gate by software during CMDs */
#define SDHCI_QUIRK2_SDIO_SW_CLK_GATE			(1<<13)
/* After SD host request, prevent system to suspend state for a while */
#define SDHCI_QUIRK2_HOLDSUSPEND_AFTER_REQUEST		(1<<15)
/* Some PXA SDH, fake interrupt would happen during CMD53 in UHS mode */
#define SDHCI_QUIRK2_FAKE_SDIO_IRQ_IN_UHS		(1<<19)

#define MMC_PM_KEEP_POWER       (1 << 0)

#define MMC_CAP2_SKIP_MMC	(1 << 16)	/* If don't need to support emmc */
#define MMC_CAP2_SKIP_SD	(1 << 17)	/* If don't need to support sd card */
#define MMC_CAP2_SKIP_SDIO	(1 << 18)	/* If don't need to support sdio */
/* Don't need voltage switch */
#define MMC_CAP2_NO_VOLTAGE_SWITCH			(1 << 23)

/* common flag */
#define SDHCI_QUIRK_BROKEN_CARD_DETECTION               (1<<15)

/* HS200/SDR104 SW tuning can't use ADMA */
#define SDHCI_QUIRK2_TUNING_ADMA_BROKEN			(1<<16)
/* HS200/SDR104 SW tuning not supported */
#define SDHCI_QUIRK2_TUNING_SW_BROKEN			(1<<17)
/* Controller supported max data timeout is too short (~10s for 52Mhz bus clock) */
#define SDHCI_QUIRK2_TIMEOUT_SHORT			(1<<18)
/* HS200/SDR104 tuning broken and must use predefined fixed delay */
#define SDHCI_QUIRK2_TUNING_BROKEN                      (1<<20)
/* Vqmmc needs always on to keep host power on for card detection */
#define SDHCI_QUIRK2_VQMMC_ALWAYS_ON			(1<<21)
/* DMA need internal clock force on */
#define SDHCI_QUIRK2_DMA_CLOCK_FORCE_ON			(1<<23)

/* Tuning mode define */
#define PXA_SDH_TUNING_DEFAULT 0
#define PXA_SDH_TUNING_DVFS 1

#endif
