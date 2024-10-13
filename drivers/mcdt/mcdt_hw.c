#include <linux/debugfs.h>
#include "mcdt_hw.h"
#include "mcdt_phy_v0.h"

#define MCDT_REG_SIZE			(0x160 + 0x10)
#define MCDT_DMA_AP_CHANNEL		5

static unsigned long membase;
static unsigned long memphys;
static unsigned long mem_dma_phys;
static unsigned long mcdt_reg_size;
static unsigned int  mcdt_irq_no;

static struct channel_status g_dac_channel[dac_channel_max] = {0};
static struct channel_status g_adc_channel[adc_channel_max] = {0};
static struct irq_desc_mcdt gadc_irq_desc[adc_channel_max] = {0};
static struct irq_desc_mcdt gdac_irq_desc[dac_channel_max] = {0};
static int mcdt_adc_dma_channel[MCDT_DMA_AP_CHANNEL] = {0};
static int mcdt_dac_dma_channel[MCDT_DMA_AP_CHANNEL] = {0};

static DEFINE_SPINLOCK(mcdt_lock);

static inline int mcdt_reg_read(unsigned long reg)
{
	return __raw_readl((void *__iomem)reg);
}

static inline void mcdt_reg_raw_write(unsigned long reg, int val)
{
	__raw_writel(val, (void *__iomem)reg);
}

static int mcdt_reg_update(unsigned int offset, int val, int mask)
{
	unsigned long reg;
	reg = membase + offset;
	int new, old;
	spin_lock(&mcdt_lock);
	old = mcdt_reg_read(reg);
	new = (old & ~mask) | (val & mask);
	mcdt_reg_raw_write(reg, new);
	spin_unlock(&mcdt_lock);
	return old != new;
}

void __mcdt_da_set_watermark(MCDT_CHAN_NUM chan_num, unsigned int full, unsigned int empty)
{
	unsigned int reg = MCDT_DAC0_WTMK + (chan_num * 4);
	mcdt_reg_update(reg, BIT_DAC0_FIFO_AF_LVL(full) | BIT_DAC0_FIFO_AE_LVL(empty), BIT_DAC0_FIFO_AF_LVL(0x1FF) | BIT_DAC0_FIFO_AE_LVL(0x1FF));
}

void __mcdt_ad_set_watermark(MCDT_CHAN_NUM chan_num, unsigned int full, unsigned int empty)
{
	unsigned int reg = MCDT_ADC0_WTMK + (chan_num * 4);
	mcdt_reg_update(reg, BIT_ADC0_FIFO_AF_LVL(full) | BIT_ADC0_FIFO_AE_LVL(empty), BIT_ADC0_FIFO_AF_LVL(0x1FF) | BIT_ADC0_FIFO_AE_LVL(0x1FF));
}


void __mcdt_da_dma_enable(MCDT_CHAN_NUM chan_num, unsigned int en)
{
	unsigned int shift = 16 + chan_num;
	mcdt_reg_update(MCDT_DMA_EN, en ? (1 << shift) : 0, 1 << shift);
}

void __mcdt_ad_dma_enable(MCDT_CHAN_NUM chan_num, unsigned int en)
{
	unsigned int shift = chan_num;
	mcdt_reg_update(MCDT_DMA_EN, en ? (1 << shift) : 0, 1 << shift);
}

void __mcdt_chan_fifo_int_enable(MCDT_CHAN_NUM chan_num, MCDT_FIFO_INT int_type, unsigned int en)
{
	unsigned int reg;
	unsigned int shift;
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN3))
		reg = MCDT_INT_EN0;
	else if ((chan_num >= MCDT_CHAN4) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_INT_EN1;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_INT_EN2;
	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN4:
	case MCDT_CHAN8:
		shift = 0 + int_type;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN5:
	case MCDT_CHAN9:
		shift = 8 + int_type;
		break;
	case MCDT_CHAN2:
	case MCDT_CHAN6:
		shift = 16 + int_type;
		break;
	case MCDT_CHAN3:
	case MCDT_CHAN7:
		shift = 24 + int_type;
		break;
	default:
		return;
	}
	mcdt_reg_update(reg, en ? (1<<shift) : 0, 1 << shift);
}

void __mcdt_chan_fifo_int_clr(MCDT_CHAN_NUM chan_num, MCDT_FIFO_INT int_type)
{
	unsigned int reg;
	unsigned int shift;
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN3))
		reg = MCDT_INT_CLR0;
	else if ((chan_num >= MCDT_CHAN4) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_INT_CLR1;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_INT_CLR2;
	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN4:
	case MCDT_CHAN8:
		shift = 0 + int_type;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN5:
	case MCDT_CHAN9:
		shift = 8 + int_type;
		break;
	case MCDT_CHAN2:
	case MCDT_CHAN6:
		shift = 16 + int_type;
		break;
	case MCDT_CHAN3:
	case MCDT_CHAN7:
		shift = 24 + int_type;
		break;
	default:
		return;
	}
	mcdt_reg_update(reg, 1<<shift, 1<<shift);
}

int __mcdt_is_chan_fifo_int_raw(MCDT_CHAN_NUM chan_num, MCDT_FIFO_INT int_type)
{
	unsigned long reg;
	unsigned int shift;
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN3))
		reg = MCDT_INT_RAW1;
	else if ((chan_num >= MCDT_CHAN4) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_INT_RAW2;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_INT_RAW3;
	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN4:
	case MCDT_CHAN8:
		shift = 0 + int_type;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN5:
	case MCDT_CHAN9:
		shift = 8 + int_type;
		break;
	case MCDT_CHAN2:
	case MCDT_CHAN6:
		shift = 16 + int_type;
		break;
	case MCDT_CHAN3:
	case MCDT_CHAN7:
		shift = 24 + int_type;
		break;
	}
	reg = reg + membase;
	return !!(mcdt_reg_read(reg)&(1<<shift));
}

int __mcdt_is_chan_fifo_int_sts(MCDT_CHAN_NUM chan_num, MCDT_FIFO_INT int_type)
{
	unsigned long reg;
	unsigned int shift;
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN3))
		reg = MCDT_INT_MSK1;
	else if ((chan_num >= MCDT_CHAN4) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_INT_MSK2;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_INT_MSK3;
	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN4:
	case MCDT_CHAN8:
		shift = 0 + int_type;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN5:
	case MCDT_CHAN9:
		shift = 8 + int_type;
		break;
	case MCDT_CHAN2:
	case MCDT_CHAN6:
		shift = 16 + int_type;
		break;
	case MCDT_CHAN3:
	case MCDT_CHAN7:
		shift = 24 + int_type;
		break;
	}
	reg = reg + membase;
	return !!(mcdt_reg_read(reg)&(1<<shift));
}

unsigned int __mcdt_is_da_empty_int(MCDT_CHAN_NUM chan_num)
{
	return __mcdt_is_chan_fifo_int_raw(chan_num, MCDT_DAC_FIFO_AE_INT);
}

unsigned int __mcdt_is_ad_full_int(MCDT_CHAN_NUM chan_num)
{
	return __mcdt_is_chan_fifo_int_raw(chan_num, MCDT_ADC_FIFO_AF_INT);
}

void __mcdt_da_int_en(MCDT_CHAN_NUM chan_num, unsigned int en)
{
	__mcdt_chan_fifo_int_enable(chan_num, MCDT_DAC_FIFO_AE_INT, en);
}

void __mcdt_ad_int_en(MCDT_CHAN_NUM chan_num, unsigned int en)
{
	__mcdt_chan_fifo_int_enable(chan_num, MCDT_ADC_FIFO_AF_INT, en);
}

void __mcdt_clr_da_int(MCDT_CHAN_NUM chan_num)
{
	__mcdt_chan_fifo_int_clr(chan_num, MCDT_DAC_FIFO_AE_INT);
}

void __mcdt_clr_ad_int(MCDT_CHAN_NUM chan_num)
{
	__mcdt_chan_fifo_int_clr(chan_num, MCDT_ADC_FIFO_AF_INT);
}

int __mcdt_is_chan_fifo_sts(unsigned int chan_num, MCDT_FIFO_STS fifo_sts)
{
	unsigned long reg = 0;
	unsigned int shift;
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN3))
		reg = MCDT_CH_FIFO_ST0;
	else if ((chan_num >= MCDT_CHAN4) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_CH_FIFO_ST1;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_CH_FIFO_ST2;
	else
		return 0;
	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN4:
	case MCDT_CHAN8:
		shift = 0 + fifo_sts;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN5:
	case MCDT_CHAN9:
		shift = 8 + fifo_sts;
		break;
	case MCDT_CHAN2:
	case MCDT_CHAN6:
		shift = 16 + fifo_sts;
		break;
	case MCDT_CHAN3:
	case MCDT_CHAN7:
		shift = 24 + fifo_sts;
		break;
	}
	reg = reg + membase;
	return !!(mcdt_reg_read(reg) & (1<<shift));
}

void __mcdt_int_to_ap_enable(MCDT_CHAN_NUM chan_num, unsigned int en)
{
	unsigned int reg = MCDT_INT_MSK_CFG0;
	unsigned int shift = 0 + chan_num;
	mcdt_reg_update(reg, en ? (1 << shift) : 0, 1 << shift);
}

unsigned int __mcdt_is_ap_int(MCDT_CHAN_NUM chan_num)
{
	unsigned long reg = MCDT_INT_MSK_CFG0 + membase;
	unsigned int shift = 0 + chan_num;
	return mcdt_reg_read(reg) & (1 << shift);
}

void __mcdt_ap_dac_dma_ch0_sel(unsigned int chan_num)
{
	mcdt_reg_update(MCDT_DMA_CFG0, BIT_MCDT_DAC_DMA_CH0_SEL0(chan_num), BIT_MCDT_DAC_DMA_CH0_SEL0(0xf));
}

void __mcdt_ap_dac_dma_ch1_sel(unsigned int chan_num)
{
	mcdt_reg_update(MCDT_DMA_CFG0, BIT_MCDT_DAC_DMA_CH1_SEL0(chan_num), BIT_MCDT_DAC_DMA_CH1_SEL0(0xf));
}

void __mcdt_ap_dac_dma_ch2_sel(unsigned int chan_num)
{
	mcdt_reg_update(MCDT_DMA_CFG0, BIT_MCDT_DAC_DMA_CH2_SEL0(chan_num), BIT_MCDT_DAC_DMA_CH2_SEL0(0xf));
}

void __mcdt_ap_dac_dma_ch3_sel(unsigned int chan_num)
{
	mcdt_reg_update(MCDT_DMA_CFG0, BIT_MCDT_DAC_DMA_CH3_SEL0(chan_num), BIT_MCDT_DAC_DMA_CH3_SEL0(0xf));
}

void __mcdt_ap_dac_dma_ch4_sel(unsigned int chan_num)
{
	mcdt_reg_update(MCDT_DMA_CFG0, BIT_MCDT_DAC_DMA_CH4_SEL0(chan_num), BIT_MCDT_DAC_DMA_CH4_SEL0(0xf));
}

void __mcdt_ap_adc_dma_ch0_sel(unsigned int chan_num)
{
	mcdt_reg_update(MCDT_DMA_CFG1, BIT_MCDT_ADC_DMA_CH0_SEL0(chan_num), BIT_MCDT_ADC_DMA_CH0_SEL0(0xf));
}

void __mcdt_ap_adc_dma_ch1_sel(unsigned int chan_num)
{
	mcdt_reg_update(MCDT_DMA_CFG1, BIT_MCDT_ADC_DMA_CH1_SEL0(chan_num), BIT_MCDT_ADC_DMA_CH1_SEL0(0xf));
}

void __mcdt_ap_adc_dma_ch2_sel(unsigned int chan_num)
{
	mcdt_reg_update(MCDT_DMA_CFG1, BIT_MCDT_ADC_DMA_CH2_SEL0(chan_num), BIT_MCDT_ADC_DMA_CH2_SEL0(0xf));
}

void __mcdt_ap_adc_dma_ch3_sel(unsigned int chan_num)
{
	mcdt_reg_update(MCDT_DMA_CFG1, BIT_MCDT_ADC_DMA_CH3_SEL0(chan_num), BIT_MCDT_ADC_DMA_CH3_SEL0(0xf));
}

void __mcdt_ap_adc_dma_ch4_sel(unsigned int chan_num)
{
	mcdt_reg_update(MCDT_DMA_CFG1, BIT_MCDT_ADC_DMA_CH4_SEL0(chan_num), BIT_MCDT_ADC_DMA_CH4_SEL0(0xf));
}

void __mcdt_dac_dma_chan_ack_sel(unsigned int chan_num, MCDT_DMA_ACK ack_num)
{
	unsigned int reg;
	unsigned int shift;
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_DMA_CFG2;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_DMA_CFG3;
	else
		return;
	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN8:
		shift = 0;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN9:
		shift = 4;
		break;
	case MCDT_CHAN2:
		shift = 8;
		break;
	case MCDT_CHAN3:
		shift = 12;
		break;
	case MCDT_CHAN4:
		shift = 16;
		break;
	case MCDT_CHAN5:
		shift = 20;
		break;
	case MCDT_CHAN6:
		shift = 24;
		break;
	case MCDT_CHAN7:
		shift = 28;
		break;
	default:
		return;
	}
	mcdt_reg_update(reg, ack_num<<shift, 0xf<<shift);
}

void __mcdt_adc_dma_chan_ack_sel(unsigned int chan_num, MCDT_DMA_ACK ack_num)
{
	unsigned int reg;
	unsigned int shift;
	if ((chan_num >= MCDT_CHAN0) && (chan_num <= MCDT_CHAN7))
		reg = MCDT_DMA_CFG4;
	else if ((chan_num >= MCDT_CHAN8) && (chan_num <= MCDT_CHAN9))
		reg = MCDT_DMA_CFG5;
	else
		return;
	switch (chan_num) {
	case MCDT_CHAN0:
	case MCDT_CHAN8:
		shift = 0;
		break;
	case MCDT_CHAN1:
	case MCDT_CHAN9:
		shift = 4;
		break;
	case MCDT_CHAN2:
		shift = 8;
		break;
	case MCDT_CHAN3:
		shift = 12;
		break;
	case MCDT_CHAN4:
		shift = 16;
		break;
	case MCDT_CHAN5:
		shift = 20;
		break;
	case MCDT_CHAN6:
		shift = 24;
		break;
	case MCDT_CHAN7:
		shift = 28;
		break;
	default:
		return;
	}
	mcdt_reg_update(reg, ack_num<<shift, 0xf<<shift);
}

void mcdt_da_fifo_clr(unsigned int chan_num)
{
	unsigned int shift = 0 + chan_num;
	mcdt_reg_update(MCDT_FIFO_CLR, 1<<shift, 1<<shift);
}

void mcdt_ad_fifo_clr(unsigned int chan_num)
{
	unsigned int shift = 16 + chan_num;
	mcdt_reg_update(MCDT_FIFO_CLR, 1<<shift, 1<<shift);
}

unsigned int __mcdt_is_da_fifo_real_full(MCDT_CHAN_NUM chan_num)
{
	return __mcdt_is_chan_fifo_sts(chan_num, MCDT_DAC_FIFO_REAL_FULL);
}

unsigned int __mcdt_is_da_fifo_real_empty(MCDT_CHAN_NUM chan_num)
{
	return __mcdt_is_chan_fifo_sts(chan_num, MCDT_DAC_FIFO_REAL_EMPTY);
}

unsigned int __mcdt_is_da_fifo_full(MCDT_CHAN_NUM chan_num)
{
	return __mcdt_is_chan_fifo_sts(chan_num, MCDT_DAC_FIFO_AF);
}

unsigned int __mcdt_is_da_fifo_empty(MCDT_CHAN_NUM chan_num)
{
	return __mcdt_is_chan_fifo_sts(chan_num, MCDT_DAC_FIFO_AE);
}


unsigned int __mcdt_is_ad_fifo_real_full(MCDT_CHAN_NUM chan_num)
{
	return __mcdt_is_chan_fifo_sts(chan_num, MCDT_ADC_FIFO_REAL_FULL);
}

unsigned int __mcdt_is_ad_fifo_real_empty(MCDT_CHAN_NUM chan_num)
{
	return __mcdt_is_chan_fifo_sts(chan_num, MCDT_ADC_FIFO_REAL_EMPTY);
}

unsigned int __mcdt_is_ad_fifo_full(MCDT_CHAN_NUM chan_num)
{
	return __mcdt_is_chan_fifo_sts(chan_num, MCDT_ADC_FIFO_AF);
}

unsigned int __mcdt_is_ad_fifo_empty(MCDT_CHAN_NUM chan_num)
{
	return __mcdt_is_chan_fifo_sts(chan_num, MCDT_ADC_FIFO_AE);
}

unsigned int wait_dac_tx_fifo_valid(MCDT_CHAN_NUM chan_num)
{
	while (__mcdt_is_da_fifo_real_full(chan_num)) {
	}
	return 0;
}

unsigned int wait_adc_rx_fifo_valid(MCDT_CHAN_NUM chan_num)
{
	while (__mcdt_is_ad_fifo_real_empty(chan_num)) {
	}
	return 0;
}

static unsigned int VAL_FLG;

unsigned int mcdt_dac_phy_write(MCDT_CHAN_NUM chan_num, unsigned int val, unsigned int *errCode)
{
	unsigned int ret = 0;
	unsigned int write = 0;
	unsigned int reg;

	*errCode = 0;
	reg = membase + MCDT_CH0_TXD + chan_num * 4;
	switch (VAL_FLG) {
	case MCDT_I2S_RW_FIFO_DEF:
	default:
		write = ret = val;
		break;
	case MCDT_I2S_RW_FIFO_I2S_16:
		write = (ret = (val & 0xFFFF))<<16;
		break;
	}
	if (__mcdt_is_da_fifo_real_full(chan_num)) {
		*errCode = -1;
	} else {
		mcdt_reg_raw_write(reg, write);
	}
	return ret;
}

unsigned int mcdt_adc_phy_read(MCDT_CHAN_NUM chan_num, unsigned int *errCode)
{
	volatile unsigned int read = 0;
	unsigned int ret = 0;
	unsigned long reg;

	*errCode = 0;
	reg = membase + MCDT_CH0_RXD + chan_num * 4;
	if (__mcdt_is_ad_fifo_real_empty(chan_num)) {
		*errCode = -1;
	} else {
		read = mcdt_reg_read(reg);
	}
	switch (VAL_FLG) {
	case MCDT_I2S_RW_FIFO_DEF:
	default:
		ret = read;
		break;
	case MCDT_I2S_RW_FIFO_I2S_16:
		ret = (read >> 16)&0xFFFF;
		break;
	}
	return ret;
}

static unsigned int mcdt_dac_int_init(MCDT_CHAN_NUM id)
{
	__mcdt_da_int_en(id, 1);
	if (g_adc_channel[id].int_enabled == 0) {
		__mcdt_int_to_ap_enable(id, 1);
		enable_irq(mcdt_irq_no);
	}
	return 0;
}

static unsigned int mcdt_adc_int_init(MCDT_CHAN_NUM id)
{
	__mcdt_ad_int_en(id, 1);
	if (g_dac_channel[id].int_enabled == 0) {
		__mcdt_int_to_ap_enable(id, 1);
		enable_irq(mcdt_irq_no);
	}
	return 0;
}

static void isr_tx_handler(volatile struct irq_desc_mcdt *tx_para, unsigned int channel)
{
	tx_para->irq_handler(tx_para->data, channel);

	g_dac_channel[channel].int_count++;
	__mcdt_clr_da_int(channel);/* TBD */
}

static void isr_rx_handler(volatile struct irq_desc_mcdt *rx_para, unsigned int channel)
{
	rx_para->irq_handler(rx_para->data, channel);

	g_adc_channel[channel].int_count++;
	__mcdt_clr_da_int(channel);/* TBD */
}

static void isr_mcdt_all_handler()
{
	unsigned int i;
	for (i = MCDT_CHAN0; i <= MCDT_CHAN9; i++) {
		if (g_dac_channel[i].int_enabled && __mcdt_is_da_empty_int(i)) {
			isr_tx_handler(&gdac_irq_desc[i], i);
		}
		if (g_adc_channel[i].int_enabled && __mcdt_is_ad_full_int(i)) {
			isr_rx_handler(&gadc_irq_desc[i], i);
		}
	}

}

static int _mcdt_send_data_use_dma(unsigned int channel, MCDT_AP_DMA_CHAN dma_chan)
{
	int uid = -1;
	__mcdt_da_dma_enable(channel, 1);
	switch (dma_chan) {
	case MCDT_AP_DMA_CH0:{
			__mcdt_ap_dac_dma_ch0_sel(channel);
			__mcdt_dac_dma_chan_ack_sel(channel, MCDT_AP_ACK0);
			uid = MCDT_AP_DAC_CH0_WR_REQ + 1;
		}
		break;
	case MCDT_AP_DMA_CH1:{
			__mcdt_ap_dac_dma_ch1_sel(channel);
			__mcdt_dac_dma_chan_ack_sel(channel, MCDT_AP_ACK1);
			uid = MCDT_AP_DAC_CH1_WR_REQ + 1;
		}
		break;
	case MCDT_AP_DMA_CH2:{
			__mcdt_ap_dac_dma_ch2_sel(channel);
			__mcdt_dac_dma_chan_ack_sel(channel, MCDT_AP_ACK2);
			uid = MCDT_AP_DAC_CH2_WR_REQ + 1;
		}
		break;
	case MCDT_AP_DMA_CH3:{
			__mcdt_ap_dac_dma_ch3_sel(channel);
			__mcdt_dac_dma_chan_ack_sel(channel, MCDT_AP_ACK3);
			uid = MCDT_AP_DAC_CH3_WR_REQ + 1;
		}
		break;
	case MCDT_AP_DMA_CH4:{
			__mcdt_ap_dac_dma_ch4_sel(channel);
			__mcdt_dac_dma_chan_ack_sel(channel, MCDT_AP_ACK4);
			uid = MCDT_AP_DAC_CH4_WR_REQ + 1;
		}
		break;
	default:{
			uid = -1;
		}
		break;
	}
	return uid;
}

/*
*return : dma request uid,err return -1.
*/
static int _mcdt_rev_data_use_dma(unsigned int channel, MCDT_AP_DMA_CHAN dma_chan)
{
	int uid = -1;
	__mcdt_ad_dma_enable(channel, 1);
	switch (dma_chan) {
	case MCDT_AP_DMA_CH0:{
			__mcdt_ap_adc_dma_ch0_sel(channel);
			__mcdt_adc_dma_chan_ack_sel(channel, MCDT_AP_ACK0);
			uid = MCDT_AP_ADC_CH0_RD_REQ + 1;
		}
		break;
	case MCDT_AP_DMA_CH1:{
			__mcdt_ap_adc_dma_ch1_sel(channel);
			__mcdt_adc_dma_chan_ack_sel(channel, MCDT_AP_ACK1);
			uid = MCDT_AP_ADC_CH1_RD_REQ + 1;
		}
		break;
	case MCDT_AP_DMA_CH2:{
			__mcdt_ap_adc_dma_ch2_sel(channel);
			__mcdt_adc_dma_chan_ack_sel(channel, MCDT_AP_ACK2);
			uid = MCDT_AP_ADC_CH2_RD_REQ + 1;
		}
		break;
	case MCDT_AP_DMA_CH3:{
			__mcdt_ap_adc_dma_ch3_sel(channel);
			__mcdt_adc_dma_chan_ack_sel(channel, MCDT_AP_ACK3);
			uid = MCDT_AP_ADC_CH3_RD_REQ + 1;
		}
		break;
	case MCDT_AP_DMA_CH4:{
			__mcdt_ap_adc_dma_ch4_sel(channel);
			__mcdt_adc_dma_chan_ack_sel(channel, MCDT_AP_ACK4);
			uid = MCDT_AP_ADC_CH4_RD_REQ + 1;
		}
		break;
	default:{
			uid = -1;
		}
		break;
	}
	return uid;
}

static unsigned int _sent_to_mcdt(unsigned int channel, unsigned int *pTxBuf)
{
	unsigned int errCode = 0;
	if (!pTxBuf)
		return -1;
	wait_dac_tx_fifo_valid(channel);
	mcdt_dac_phy_write(channel, *pTxBuf, &errCode);
	return errCode;
}

int mcdt_write(unsigned int channel, char *pTxBuf, unsigned int size)
{
	unsigned int i = 0;
	unsigned int size_dword = size/4;
	unsigned int *temp_buf = (unsigned int *)pTxBuf;
	while (i < size_dword) {
		if (_sent_to_mcdt(channel, temp_buf + i))
			return -1;
		i++;
	}
	return 0;
}

static unsigned int _recv_from_mcdt(unsigned int channel, unsigned int *pRxBuf)
{
	unsigned int errCode = 0;
	if (!pRxBuf)
		return -1;
	wait_adc_rx_fifo_valid(channel);
	*pRxBuf = mcdt_adc_phy_read(channel, &errCode);
	return errCode;
}

int mcdt_read(unsigned int channel, char *pRxBuf, unsigned int size)
{
	unsigned int i = 0;
	unsigned int size_dword = size/4;
	unsigned int *temp_buf = (unsigned int *)pRxBuf;
	while (i < size_dword) {
		if (_recv_from_mcdt(channel, temp_buf + i))
			return -1;
		i++;
	}

	return 0;
}

unsigned long mcdt_adc_phy_addr(unsigned int channel)
{
	return memphys + MCDT_CH0_RXD + channel * 4;
}

unsigned long mcdt_dac_phy_addr(unsigned int channel)
{
	return memphys + MCDT_CH0_TXD + channel * 4;
}

unsigned long mcdt_adc_dma_phy_addr(unsigned int channel)
{
	return mem_dma_phys + MCDT_CH0_RXD + channel * 4;
}

unsigned long mcdt_dac_dma_phy_addr(unsigned int channel)
{
	return mem_dma_phys + MCDT_CH0_TXD + channel * 4;
}

unsigned int mcdt_is_adc_full(unsigned int channel)
{
	return __mcdt_is_chan_fifo_sts(channel, MCDT_ADC_FIFO_REAL_FULL);
}

unsigned int mcdt_is_adc_empty(unsigned int channel)
{
	return __mcdt_is_chan_fifo_sts(channel, MCDT_ADC_FIFO_REAL_EMPTY);
}

unsigned int mcdt_is_dac_full(unsigned int channel)
{
	return __mcdt_is_chan_fifo_sts(channel, MCDT_DAC_FIFO_REAL_FULL);
}

unsigned int mcdt_is_dac_empty(unsigned int channel)
{
	return __mcdt_is_chan_fifo_sts(channel, MCDT_DAC_FIFO_REAL_EMPTY);
}

unsigned int mcdt_dac_buffer_size_avail(unsigned int channel)
{
	unsigned long reg = MCDT_DAC0_FIFO_ADDR_ST + channel * 8 + membase;
	unsigned int r_addr = (mcdt_reg_read(reg)>>16)&0x3FF;
	unsigned int w_addr = mcdt_reg_read(reg)&0x3FF;
	if (w_addr > r_addr)
		return 4 * (FIFO_LENGTH - w_addr + r_addr);
	else
		return 4 * (r_addr - w_addr);
}

unsigned int mcdt_adc_data_size_avail(unsigned int channel)
{
	unsigned long reg = MCDT_ADC0_FIFO_ADDR_ST + channel * 8 + membase;
	unsigned int r_addr = (mcdt_reg_read(reg)>>16)&0x3FF;
	unsigned int w_addr = mcdt_reg_read(reg)&0x3FF;
	if (w_addr > r_addr)
		return 4 * (w_addr - r_addr);
	else
		return 4 * (FIFO_LENGTH - r_addr + w_addr);
}

int mcdt_dac_int_enable(unsigned int channel, void (*callback)(void*, unsigned int), void *data, unsigned int emptymark)
{
	if (g_dac_channel[channel].int_enabled == 1 || g_dac_channel[channel].dma_enabled == 1)
		return -1;

	mcdt_da_fifo_clr(channel);
	__mcdt_da_set_watermark(channel, FIFO_LENGTH - 1, emptymark);

	if (callback != NULL) {
		gdac_irq_desc[channel].irq_handler = callback;
		gdac_irq_desc[channel].data = data;
		mcdt_dac_int_init(channel);
	}
	g_dac_channel[channel].int_enabled = 1;
	return 0;
}
int mcdt_adc_int_enable(unsigned int channel, void (*callback)(void*, unsigned int), void *data, unsigned int fullmark)
{
	if (g_adc_channel[channel].int_enabled == 1 || g_adc_channel[channel].dma_enabled == 1)
		return -1;
	mcdt_ad_fifo_clr(channel);
	__mcdt_ad_set_watermark(channel, fullmark, FIFO_LENGTH - 1);

	if (callback != NULL) {
		gadc_irq_desc[channel].irq_handler = callback;
		gadc_irq_desc[channel].data = data;
		mcdt_adc_int_init(channel);
	}

	g_adc_channel[channel].int_enabled = 1;
	return 0;
}

/*
*return: uid, error if < 0
*/
int mcdt_dac_dma_enable(unsigned int channel, unsigned int emptymark)
{
	int i = 0;
	int uid = -1;
	if (g_dac_channel[channel].int_enabled == 1 || g_dac_channel[channel].dma_enabled == 1)
		return -1;
	for (i = 0; i < MCDT_DMA_AP_CHANNEL; i++) {
		if (!mcdt_dac_dma_channel[i]) {
			mcdt_dac_dma_channel[i] = 1;
			break;
		}
	}
	if (i == MCDT_DMA_AP_CHANNEL)
		return -1;
	mcdt_da_fifo_clr(channel);
	__mcdt_da_set_watermark(channel, FIFO_LENGTH - 1, emptymark);
	uid = _mcdt_send_data_use_dma(channel, i);
	if (uid > 0) {
		pr_info("%s %d mcdt_dma_ap_channel=%d\n", __func__, __LINE__, i);
		g_dac_channel[channel].dma_enabled = 1;
		g_dac_channel[channel].dma_channel = i;
	}
#if 0
	mcdt_dac_int_init(channel);
	g_dac_channel[channel].int_enabled = 1;
#endif

	return uid;
}
/*
*return: uid, error if < 0
*/
int mcdt_adc_dma_enable(unsigned int channel, unsigned int fullmark)
{
	int uid = -1;
	int i = 0;
	if (g_adc_channel[channel].int_enabled == 1 || g_adc_channel[channel].dma_enabled == 1)
		return -1;
	for (i = 0; i < MCDT_DMA_AP_CHANNEL; i++) {
		if (!mcdt_adc_dma_channel[i]) {
			mcdt_adc_dma_channel[i] = 1;
			break;
		}
	}
	if (i == MCDT_DMA_AP_CHANNEL)
		return -1;
	mcdt_ad_fifo_clr(channel);
	__mcdt_ad_set_watermark(channel, fullmark, FIFO_LENGTH - 1);
	uid = _mcdt_rev_data_use_dma(channel, i);
	if (uid > 0) {
		pr_info("%s %d mcdt_dma_ap_channel=%d\n", __func__, __LINE__, i);
		g_adc_channel[channel].dma_enabled = 1;
		g_adc_channel[channel].dma_channel = i;
	}
#if 0
	mcdt_adc_int_init(channel);
	g_adc_channel[channel].int_enabled = 1;
#endif

	return uid;
}


int get_mcdt_adc_dma_uid(MCDT_CHAN_NUM mcdt_chan, MCDT_AP_DMA_CHAN mcdt_ap_dma_chan ,unsigned int fullmark)
{
	int uid = -1;
	mcdt_ad_fifo_clr(mcdt_chan);
	__mcdt_ad_set_watermark(mcdt_chan, fullmark, FIFO_LENGTH - 1);
	uid = _mcdt_rev_data_use_dma(mcdt_chan, mcdt_ap_dma_chan);
	if(uid < 0){
		return -EIO;
	}
	return uid;
}

int get_mcdt_dac_dma_uid(MCDT_CHAN_NUM mcdt_chan, MCDT_AP_DMA_CHAN mcdt_ap_dma_chan ,unsigned int emptymark)
{
	int uid = -1;
	mcdt_da_fifo_clr(mcdt_chan);
	__mcdt_da_set_watermark(mcdt_chan, FIFO_LENGTH - 1, emptymark);
	uid = _mcdt_send_data_use_dma(mcdt_chan, mcdt_ap_dma_chan);
	if(uid < 0){
		return -EIO;
	}
	return uid;
}

void mcdt_dac_dma_disable(unsigned int channel)
{
	__mcdt_ad_dma_enable(channel, 1);
	mcdt_da_fifo_clr(channel);
	mcdt_dac_dma_channel[g_dac_channel[channel].dma_channel] = 0;
	g_dac_channel[channel].dma_enabled = 0;
	g_dac_channel[channel].dma_channel = 0;
#if 0
	g_dac_channel[channel].int_enabled = 0;
#endif
}

void mcdt_adc_dma_disable(unsigned int channel)
{
	__mcdt_ad_dma_enable(channel, 0);
	mcdt_ad_fifo_clr(channel);
	mcdt_adc_dma_channel[g_adc_channel[channel].dma_channel] = 0;
	g_adc_channel[channel].dma_enabled = 0;
	g_adc_channel[channel].dma_channel = 0;
#if 0
	g_adc_channel[channel].int_enabled = 0;
#endif
}

int mcdt_dac_disable(unsigned int channel)
{
	if (g_dac_channel[channel].int_enabled) {
		if (gdac_irq_desc[channel].irq_handler) {
			__mcdt_da_int_en(channel, 0);
			__mcdt_clr_da_int(channel);
			gdac_irq_desc[channel].irq_handler = NULL;
			gdac_irq_desc[channel].data = NULL;
			if (g_adc_channel[channel].int_enabled == 0) {
				__mcdt_int_to_ap_enable(channel, 0);;
				disable_irq_nosync(mcdt_irq_no);
			}
		}
		g_dac_channel[channel].int_enabled = 0;
	}
	if (g_dac_channel[channel].dma_enabled) {
		__mcdt_da_dma_enable(channel, 0);
		mcdt_dac_dma_channel[g_dac_channel[channel].dma_channel] = 0;
		g_dac_channel[channel].dma_enabled = 0;
	}
	mcdt_da_fifo_clr(channel);
	return 0;
}

int mcdt_adc_disable(unsigned int channel)
{
	if (g_adc_channel[channel].int_enabled) {
		if (gadc_irq_desc[channel].irq_handler) {
			__mcdt_ad_int_en(channel, 0);
			__mcdt_clr_ad_int(channel);
			gadc_irq_desc[channel].irq_handler = NULL;
			gadc_irq_desc[channel].data = NULL;
			if (g_dac_channel[channel].int_enabled == 0) {
				__mcdt_int_to_ap_enable(channel, 0);
				disable_irq_nosync(mcdt_irq_no);
			}
		}
		g_adc_channel[channel].int_enabled = 0;
	}
	if (g_adc_channel[channel].dma_enabled) {
		__mcdt_ad_dma_enable(channel, 0);
		mcdt_adc_dma_channel[g_adc_channel[channel].dma_channel] = 0;
		g_adc_channel[channel].dma_enabled = 0;
	}
	mcdt_da_fifo_clr(channel);
	return 0;

}

#if defined(CONFIG_DEBUG_FS)
static int mcdt_debug_info_show(struct seq_file *m, void *private)
{
	int i = 0;

	seq_printf(m, "\nmembase=0x%lx, memphys=0x%8x, mem_dma_phys=0x%8x, mcdt_reg_size=0x%x, mcdt_irq_no=%d\n",
		membase, memphys, mem_dma_phys, mcdt_reg_size, mcdt_irq_no);
	seq_printf(m, "\n");

	for (i = 0; i < dac_channel_max; i++) {
		seq_printf(m, "g_dac_channel[%d]: int_enabled=%d, dma_enabled=%d, dma_channel=%d\n",
			i, g_dac_channel[i].int_enabled, g_dac_channel[i].dma_enabled, g_dac_channel[i].dma_channel);
	}
	seq_printf(m, "\n");

	for (i = 0; i < adc_channel_max; i++) {
		seq_printf(m, "g_adc_channel[%d]: int_enabled=%d, dma_enabled=%d, dma_channel=%d\n",
			i, g_adc_channel[i].int_enabled, g_adc_channel[i].dma_enabled, g_adc_channel[i].dma_channel);
	}
	seq_printf(m, "\n");

	seq_printf(m, "mcdt_dac_dma_channel:\n");
	for (i = 0; i < MCDT_DMA_AP_CHANNEL; i++) {
		seq_printf(m, "%d ", mcdt_dac_dma_channel[i]);
	}
	seq_printf(m, "\n");

	seq_printf(m, "mcdt_adc_dma_channel:\n");
	for (i = 0; i < MCDT_DMA_AP_CHANNEL; i++) {
		seq_printf(m, "%d ", mcdt_adc_dma_channel[i]);
	}
	seq_printf(m, "\n");
}

static int mcdt_debug_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, mcdt_debug_info_show, inode->i_private);
}

static const struct file_operations mcdt_debug_info_fops = {
	.open = mcdt_debug_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
int mcdt_init_debugfs_info(void *root)
{
	if (!root)
		return -ENXIO;
	debugfs_create_file("info", S_IRUGO, (struct dentry *)root, NULL, &mcdt_debug_info_fops);
	return 0;
}

static int mcdt_debug_reg_show(struct seq_file *m, void *private)
{
	int i = 0;
	int val = 0, *pval;
	pval = &val;
	seq_printf(m, "MCDT register dump: phy_addr=0x%08x\n", memphys);
	for (i = 0; i < mcdt_reg_size; i += 16) {
		seq_printf(m, "0x%04x: | 0x%04x_%04x 0x%04x_%04x 0x%04x_%04x 0x%04x_%04x\n",
			i,
			((*pval = mcdt_reg_read(membase + i + 0x00)) & 0xffff0000) >> 16, val & 0x0000ffff,
			((*pval = mcdt_reg_read(membase + i + 0x04)) & 0xffff0000) >> 16, val & 0x0000ffff,
			((*pval = mcdt_reg_read(membase + i + 0x08)) & 0xffff0000) >> 16, val & 0x0000ffff,
			((*pval = mcdt_reg_read(membase + i + 0x0c)) & 0xffff0000) >> 16, val & 0x0000ffff
			);
	}
}


static int mcdt_debug_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, mcdt_debug_reg_show, inode->i_private);
}

static const struct file_operations sblock_debug_reg_fops = {
	.open = mcdt_debug_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int mcdt_init_debugfs_reg(void *root)
{
	if (!root)
		return -ENXIO;
	debugfs_create_file("reg", S_IRUGO, (struct dentry *)root, NULL, &sblock_debug_reg_fops);
	return 0;
}

static int __init mcdt_init_debugfs(void)
{
	struct dentry *root = debugfs_create_dir("mcdt", NULL);
	if (!root)
		return -ENXIO;

	mcdt_init_debugfs_info(root);
	mcdt_init_debugfs_reg(root);
	return 0;
}

__initcall(mcdt_init_debugfs);


#endif /* CONFIG_DEBUG_FS */


static int mcdt_probe(struct platform_device *pdev)
{
	int ret = -1;

	uint32_t array[2];
	struct device_node *node = pdev->dev.of_node;

	if (node) {
		ret = of_property_read_u32_array(node, "reg_base", &array[0], 2);
		if (!ret) {
			mem_dma_phys = array[0];
			memphys = array[1];
		} else {
			pr_err("mcdt reg parse error!\n");
			return -EINVAL;
		}
		if (of_property_read_u32(node, "reg_size", &mcdt_reg_size)) {
			pr_err("mcdt reg_size parse error!\n");
			return -EINVAL;
		}
		if (of_property_read_u32(node, "irq_no", &mcdt_irq_no)) {
			pr_err("mcdt irq_no parse error!\n");
			return -EINVAL;
		}
	}

	ret =
		request_irq(mcdt_irq_no, isr_mcdt_all_handler, 0,
			"sprd_mcdt_ap", NULL);
	if (ret) {
		pr_err("mcdt ERR:Request irq ap failed!\n");
		goto err_irq;
	}
	disable_irq_nosync(mcdt_irq_no);

	membase = ioremap_nocache(memphys, mcdt_reg_size);

	if (!membase) {
		pr_err("ERR:mcdt reg address ioremap_nocache error !\n");
		return -EINVAL;
	}

	return 0;

err_irq:
	return -EINVAL;
}


static const struct of_device_id mcdt_of_match[] = {
  {.compatible = "sprd, mcdt_v1", },
  { }
};
static struct platform_driver mcdt_driver = {
  .driver = {
    .name = "mcdt_sprd",
    .owner = THIS_MODULE,
    .of_match_table = mcdt_of_match,
  },
  .probe = mcdt_probe,
};

static int __init mcdt_init(void)
{
	int ret;
	ret = platform_driver_register(&mcdt_driver);
	return ret;
}

static void __exit mcdt_exit(void)
{
	free_irq(mcdt_irq_no, NULL);
	platform_driver_unregister(&mcdt_driver);
}

module_init(mcdt_init);
module_exit(mcdt_exit);

MODULE_DESCRIPTION("mcdt driver v1");
MODULE_AUTHOR("Zuo Wang <zuo.wang@spreadtrum.com>");
MODULE_LICENSE("GPL");

