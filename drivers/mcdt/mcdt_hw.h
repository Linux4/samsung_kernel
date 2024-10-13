#ifndef _mcdt_hw_
#define _mcdt_hw_

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <soc/sprd/board.h>
#include <asm/io.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/adi.h>
#include <linux/module.h>
#include <linux/wakelock.h>

#include <soc/sprd/arch_misc.h>
#include "mcdt_phy_v0.h"

#ifdef CONFIG_OF
#include <linux/slab.h>
#include <linux/of_device.h>
#endif

#define FIFO_LENGTH 512
/* #define INT_REQ_MCDT_AGCP    (48 + 32) */
#define INT_REQ_MCDT_AGCP    (48)


struct irq_desc_mcdt {
void *data;
void (*irq_handler)(void* , unsigned int);
unsigned int cpu_id;
};

struct channel_status {
int int_enabled;
int dma_enabled;
int dma_channel;
int int_count;

};

enum {
dac_channel0 = 0,
dac_channel1,
dac_channel2,
dac_channel3,
dac_channel4,
dac_channel5,
dac_channel6,
dac_channel7,
dac_channel8,
dac_channel9,
dac_channel_max,
};

enum {
adc_channel0 = 0,
adc_channel1,
adc_channel2,
adc_channel3,
adc_channel4,
adc_channel5,
adc_channel6,
adc_channel7,
adc_channel8,
adc_channel9,
adc_channel_max,
};


int mcdt_dac_int_enable(unsigned int channel, void (*callback)(void*, unsigned int), void *data, unsigned int emptymark);
int mcdt_adc_int_enable(unsigned int channel, void (*callback)(void*, unsigned int), void *data, unsigned int fullmark);

/*space available,return bytes*/
unsigned int mcdt_dac_buffer_size_avail(unsigned int channel);
/*data available,return bytes*/
unsigned int mcdt_adc_data_size_avail(unsigned int channel);



int mcdt_write(unsigned int channel, char *pTxBuf, unsigned int size);
int mcdt_read(unsigned int channel, char *pRxBuf, unsigned int size);



int mcdt_dac_dma_enable(unsigned int channel, unsigned int emptymark);

int mcdt_adc_dma_enable(unsigned int channel, unsigned int fullmark);

int get_mcdt_adc_dma_uid(MCDT_CHAN_NUM mcdt_chan, MCDT_AP_DMA_CHAN mcdt_ap_dma_chan ,unsigned int fullmark);
int get_mcdt_dac_dma_uid(MCDT_CHAN_NUM mcdt_chan, MCDT_AP_DMA_CHAN mcdt_ap_dma_chan ,unsigned int emptymark);

void mcdt_dac_dma_disable(unsigned int channel);
void mcdt_adc_dma_disable(unsigned int channel);


unsigned long mcdt_dac_phy_addr(unsigned int channel);
unsigned long mcdt_adc_phy_addr(unsigned int channel);

unsigned long mcdt_adc_dma_phy_addr(unsigned int channel);
unsigned long mcdt_dac_dma_phy_addr(unsigned int channel);

int mcdt_dac_disable(unsigned int channel);
int mcdt_adc_disable(unsigned int channel);



unsigned int mcdt_is_dac_full(unsigned int channel);
unsigned int mcdt_is_dac_empty(unsigned int channel);



unsigned int mcdt_is_adc_full(unsigned int channel);
unsigned int mcdt_is_adc_empty(unsigned int channel);


void mcdt_da_fifo_clr(unsigned int chan_num);
void mcdt_ad_fifo_clr(unsigned int chan_num);


#endif
