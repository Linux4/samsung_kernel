/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/bug.h>
#include <linux/input.h>
#include <linux/slab.h>

#include <linux/ctype.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/io.h>

#include <mach/irqs.h>
#include <mach/adi.h>

/* #define TP_DEBUG */
#ifdef  TP_DEBUG
#define TP_PRINT  printk
#else
#define TP_PRINT(...)
#endif

#define SCI_PASSERT(condition, format...)  \
	do {		\
		if(!(condition)) { \
			pr_err("function :%s\r\n", __FUNCTION__);\
			BUG();	\
		} \
	}while(0)

#define ANA_REG_BASE                SPRD_ANA_BASE
#define SPRD_ANA_BASE               (SPRD_MISC_BASE + 0x600)
#define ANA_AGEN                    (ANA_REG_BASE + 0x00)

#define TPC_REG_BASE                (SPRD_MISC_BASE+0x280)
#define TPC_CTRL                    (TPC_REG_BASE + 0x0000)
#define TPC_SAMPLE_CTRL0            (TPC_REG_BASE + 0x0004)
#define TPC_SAMPLE_CTRL1            (TPC_REG_BASE + 0x0008)
#define TPC_BOUNCE_CTRL             (TPC_REG_BASE + 0x000C)
#define TPC_FILTER_CTRL             (TPC_REG_BASE + 0x0010)
#define TPC_CALC_CTRL               (TPC_REG_BASE + 0x0014)
#define TPC_CALC_X_COEF_A           (TPC_REG_BASE + 0x0018)
#define TPC_CALC_X_COEF_B           (TPC_REG_BASE + 0x001C)
#define TPC_CALC_Y_COEF_A           (TPC_REG_BASE + 0x0020)
#define TPC_CALC_Y_COEF_B           (TPC_REG_BASE + 0x0024)
#define TPC_INT_EN                  (TPC_REG_BASE + 0x0028)
#define TPC_INT_STS                 (TPC_REG_BASE + 0x002C)
#define TPC_INT_RAW                 (TPC_REG_BASE + 0x0030)
#define TPC_INT_CLR                 (TPC_REG_BASE + 0x0034)
#define TPC_BUF_CTRL                (TPC_REG_BASE + 0x0038)
#define TPC_X_DATA                  (TPC_REG_BASE + 0x003C)
#define TPC_Y_DATA                  (TPC_REG_BASE + 0x0040)
#define TPC_Z_DATA                  (TPC_REG_BASE + 0x0044)

/* TPC_CTRL BIT map */
#define TPC_STOP_BIT                BIT(5)
#define TPC_RUN_BIT                 BIT(4)
#define TPC_TPC_MODE_BIT            BIT(3)
#define TPC_PEN_REQ_POL_BIT         BIT(1)
#define TPC_EN_BIT                  BIT(0)

#define TPC_PRESCALE_OFFSET         0x08
#define TPC_PRESCALE_MSK            (0xFF << TPC_PRESCALE_OFFSET)

/* TPC_SAMPLE_CTRL0 BIT map */
#define TPC_SAMPLE_INTERVAL_OFFSET    0
#define TPC_SAMPLE_INTERVAL_MSK      (0xFF << TPC_SAMPLE_INTERVAL_OFFSET)
#define TPC_POINT_INTERVAL_OFFSET     8
#define TPC_POINT_INTERVAL_MSK       (0xFF << TPC_POINT_INTERVAL_OFFSET)

/* TPC_SAMPLE_CTRL1 BIT map */
#define TPC_DATA_INTERVAL_OFFSET     0
#define TPC_DATA_INTERVAL_MSK        (0xFFF << TPC_DATA_INTERVAL_OFFSET)
#define TPC_SAMPLE_NUM_OFFSET        12
#define TPC_SAMPLE_NUM_MSK           (0xF << TPC_SAMPLE_NUM_OFFSET)

/* TPC_BOUNCE_CTRL BIT map */
#define TPC_DEBOUNCE_EN_BIT         BIT(0)
#define TPC_DEBOUNCE_NUM_OFFSET     1
#define TPC_DEBOUNCE_NUM_MSK        (0xFF << TPC_DEBOUNCE_NUM_OFFSET)


/* TPC_FILTER_CTRL BIT map */
#define TPC_FILTER_EN_BIT           BIT(0)
#define TPC_FILTER_MODE_BIT         BIT(1)
#define TPC_FILTER_MODE_OFFSET      2
#define TPC_FILTER_MODE_MSK         (0xF << TPC_FILTER_MODE_OFFSET)

/* TPC_BUF_CTRL BIT map */
#define TPC_BUF_EMP_BIT          BIT(5)
#define TPC_BUF_FULL             BIT(4)
#define TPC_BUF_LENGTH_OFFSET    0
#define TPC_BUF_LENGTH_MSK       (0xF << TPC_BUF_LENGTH_OFFSET)

#define TPC_DONE_IRQ_MSK_BIT     BIT(2)
#define TPC_UP_IRQ_MSK_BIT       BIT(1)
#define TPC_DOWN_IRQ_MSK_BIT     BIT(0)

#define ADC_REG_BASE                (SPRD_MISC_BASE+0x300)
#define ADC_CTRL                    (ADC_REG_BASE + 0x0000)
#define ADC_CS                      (ADC_REG_BASE + 0x0004)
#define ADC_TPC_CH_CTRL             (ADC_REG_BASE + 0x0008)
#define ADC_DAT                     (ADC_REG_BASE + 0x00C)
#define ADC_INT_EN                  (ADC_REG_BASE + 0x0010)
#define ADC_INT_CLR                 (ADC_REG_BASE + 0x0014)
#define ADC_INT_STAT                (ADC_REG_BASE + 0x0018)
#define ADC_INT_SRC                 (ADC_REG_BASE + 0x001C)

/* ADC_CTRL */
#define ADC_STATUS_BIT               BIT(4)
#define ADC_TPC_CH_ON_BIT            BIT(2)
#define SW_CH_ON_BIT                 BIT(1)
#define ADC_EN_BIT                   BIT(0)

/* ADC_CS bit map */
#define ADC_SCALE_BIT                BIT(4)
#define ADC_SLOW_BIT                 BIT(5)
#define ADC_CS_BIT_MSK               0x0F

#define ADC_SCALE_3V       0
#define ADC_SCALE_1V2      1

/* ADC TPC channel control */
#define ADC_TPC_CH_DELAT_OFFSET 8
#define ADC_TPC_CH_DELAY_MASK  (0xFF << ADC_TPC_CH_DELAT_OFFSET)

#define TPC_BUF_LENGTH      4
#define TPC_DELTA_DATA      15
#define CLEAR_TPC_INT(msk) \
    do{ \
        sci_adi_raw_write(TPC_INT_CLR, (msk));\
        while(sci_adi_read(TPC_INT_RAW) & (msk));    \
    }while(0)

#define X_MIN	0
#define X_MAX	0x3ff
#define Y_MIN	0
#define Y_MAX	0x3ff

#define TPC_CHANNEL_X       2
#define TPC_CHANNEL_Y       3
#define ADC_CH_MAX_NUM      8

#define SCI_TRUE	1
#define SCI_FALSE	0

/* fit touch panel to LCD size */
#define TP_LCD_WIDTH    320
#define TP_LCD_HEIGHT   480
/*
int a= 35337;
int b= -305;
int c= -8622664;
int d= -105;
int e= 43086;
int f= -8445728;
int g= 65536;
*/

typedef union
{
    struct
    {
        int16_t y;
        int16_t x;
    } data;
    int32_t dwValue;
} TPC_DATA_U;


typedef struct{
    uint16_t x_coef_a;
    uint16_t x_coef_b;
    uint16_t y_coef_a;
    uint16_t y_coef_b;
}TPC_COEF;

struct sprd_tp{
	struct input_dev *input;

	unsigned int irq;
	unsigned int is_first_down;

	struct timer_list timer;
	TPC_DATA_U tp_data;
};


static uint8_t          cal_mode_en = 0;
static TPC_DATA_U       cal_data;
static TPC_COEF         cal_coef;
static struct sprd_tp  *sprd_tp_data = NULL;
static uint16_t         tp_fix_cx = (TP_LCD_WIDTH>>1);
static uint16_t         tp_fix_cy = (TP_LCD_HEIGHT>>1);

static void ADC_Init(void)
{
#if CONFIG_ARCH_SC8810
/* 8810	TODO */
	sci_adi_set (ANA_AGEN, (BIT(5)  | BIT(13)  | BIT(14) )); /* AUXAD controller APB clock enable */
#else
	sci_adi_set (ANA_AGEN, AGEN_ADC_EN); /* AUXAD controller APB clock enable */
    sci_adi_set (ANA_CLK_CTL, ACLK_CTL_AUXAD_EN | ACLK_CTL_AUXADC_EN);//enable AUXAD clock generation
    sci_adi_set (ADC_CTRL, ADC_EN_BIT);/* ADC module enable */
#endif
    /* Set ADC conversion speed to slow mode, this bit is used for TPC
       channel only */
    sci_adi_set(ADC_CS, ADC_SLOW_BIT);

    /* Set TPC channel sampling delay */
    sci_adi_set(ADC_TPC_CH_CTRL, ADC_TPC_CH_DELAY_MASK);
}
static void ADC_SetScale (uint32_t scale)
{
    if (scale == ADC_SCALE_1V2)
    {
        /* Set ADC small scalel */
        sci_adi_clr (ADC_CS, ADC_SCALE_BIT);
    }
    else if ( (scale == ADC_SCALE_3V))
    {
        /* Set ADC large scalel */
        sci_adi_set (ADC_CS, ADC_SCALE_BIT);
    }
}

static void sci_adi_msk_or(uint32_t addr,uint32_t value,uint32_t msk)
{
    unsigned short adi_tmp_val;
    adi_tmp_val = sci_adi_read(addr);
    adi_tmp_val &= (unsigned short)(~(msk));
    adi_tmp_val |= (unsigned short)((value)&(msk));
    sci_adi_raw_write(addr, adi_tmp_val);
}

static void ADC_SetCs (uint32_t source)
{
    SCI_PASSERT( (source < ADC_CH_MAX_NUM), ("error: source =%d",source));
    sci_adi_msk_or (ADC_CS, source, ADC_CS_BIT_MSK);


}
 void ADC_OpenTPC (void)
{
    /* Open TPC channel */
    sci_adi_set (ADC_CTRL, ADC_TPC_CH_ON_BIT);

    TP_PRINT( "ADC:ADC_OpenTPC\n");

}
 void ADC_CloseTPC (void)
{
    /* Close TPC channel */
    sci_adi_clr (ADC_CTRL, ADC_TPC_CH_ON_BIT);

    TP_PRINT( "ADC:ADC_CloseTPC\n");
}
static void tp_init(void)
{
#if CONFIG_ARCH_SC8810
    sci_adi_set (ANA_AGEN, BIT(12)  | BIT(4) );/* Touch panel controller APB clock enable */
#else
	sci_adi_set (ANA_AGEN, AGEN_TPC_EN | AGEN_RTC_TPC_EN);/* Touch panel controller APB clock enable */
#endif
    /* Enable TPC module */
    sci_adi_set (TPC_CTRL, TPC_EN_BIT);
    //Config pen request polarity
    sci_adi_clr (TPC_CTRL, TPC_PEN_REQ_POL_BIT);

    /* Config tpc mode */
    sci_adi_clr ( TPC_CTRL, TPC_TPC_MODE_BIT);

    /* 500KHz */
    sci_adi_msk_or (TPC_CTRL, (0x0D<< TPC_PRESCALE_OFFSET), TPC_PRESCALE_MSK);/* prescale */

    /* Config ADC channel */
    ADC_SetScale (ADC_SCALE_3V);

    /* Config TPC sample properity */
    sci_adi_raw_write (TPC_SAMPLE_CTRL0,
                (0x10 << TPC_SAMPLE_INTERVAL_OFFSET) | (0x30 <<TPC_POINT_INTERVAL_OFFSET));//(0x80 << 8) | 0x14
    sci_adi_raw_write (TPC_SAMPLE_CTRL1,
                 (0x200 << TPC_DATA_INTERVAL_OFFSET) | (4 <<TPC_SAMPLE_NUM_OFFSET));

      /*
     *Config TPC filter properity
     *Config TPC debounce properity
       */
    sci_adi_raw_write (TPC_BOUNCE_CTRL,
                 TPC_DEBOUNCE_EN_BIT | (5 << TPC_DEBOUNCE_NUM_OFFSET));
    /* Config TPC buffer length */
    sci_adi_msk_or (TPC_BUF_CTRL, (TPC_BUF_LENGTH << TPC_BUF_LENGTH_OFFSET), TPC_BUF_LENGTH_MSK);
    /* Clear TPC interrupt */
    CLEAR_TPC_INT ( (TPC_DONE_IRQ_MSK_BIT | TPC_UP_IRQ_MSK_BIT |TPC_DOWN_IRQ_MSK_BIT));

}
static inline void tp_run (void)
{
    /* Enable TPC module */
    sci_adi_set (TPC_CTRL, TPC_RUN_BIT);

    CLEAR_TPC_INT (TPC_DONE_IRQ_MSK_BIT);

    /* Set ADC cs to TPC_CHANNEL_X */
    ADC_SetCs (TPC_CHANNEL_X);

    /* Set ADC scale */
    ADC_SetScale (ADC_SCALE_3V);

    /* Open ADC channel */
    ADC_OpenTPC();

}
static inline void tp_stop (void)
{
    /* Disable TPC module */
    sci_adi_set (TPC_CTRL, TPC_STOP_BIT);
    /* Close ADC channel */
    ADC_CloseTPC();

}
static int tp_fetch_data (TPC_DATA_U *tp_data)
{
    uint16_t i;
    uint16_t buf_status = 0;
    TPC_DATA_U pre_data,cur_data;
    int delta_x, delta_y;
    int result = SCI_TRUE;

    buf_status = sci_adi_read (TPC_BUF_CTRL);

    /* TPC data buffer is empty */
    if (! (buf_status & TPC_BUF_FULL))
    {
        TP_PRINT ("func[%s]: buffer not full!buf_status=0x%x\n",__FUNCTION__,buf_status);
        return SCI_FALSE;
    }

    pre_data.dwValue = 0;

    /* Fecth data from TPC data buffer */
    for (i=0; i<TPC_BUF_LENGTH; i++)
    {
        /* Get data from buffer */
        cur_data.data.x  = sci_adi_read (TPC_X_DATA);
        cur_data.data.y  = sci_adi_read (TPC_Y_DATA);

        TP_PRINT("x=%d,y=%d\n",cur_data.data.x,cur_data.data.y);

        if(cur_data.data.x <= X_MIN || cur_data.data.x >= X_MAX ||
            cur_data.data.y <= Y_MIN || cur_data.data.y >= Y_MAX)
                return SCI_FALSE;

        if (pre_data.dwValue != 0)
        {
            delta_x = pre_data.data.x - cur_data.data.x;
            delta_x = (delta_x > 0) ? delta_x : (- delta_x);
            delta_y = pre_data.data.y - cur_data.data.y;
            delta_y = (delta_y > 0) ? delta_y : (- delta_y);

            TP_PRINT("delta_x=%d,delta_y=%d\n",delta_x,delta_y);

            if ( (delta_x + delta_y) >= TPC_DELTA_DATA)
            {
                TP_PRINT ("func[%s]: fetch data failed !delta_x = %d delta_y = %d\n",__FUNCTION__,delta_x, delta_y);
                result  = SCI_FALSE;
            }
        }

        pre_data.dwValue = cur_data.dwValue;

    }

    /* Get the last data */
    tp_data->dwValue = cur_data.dwValue;

    return result;

}

static irqreturn_t tp_irq(int irq, void *dev_id)
{
	struct sprd_tp *tp=(struct sprd_tp *)dev_id;
	uint32_t int_status;
    int32_t xd,yd;
    int32_t xl,yl;

	TP_PRINT ("TP: enter tp_irq!\n");

	int_status = sci_adi_read (TPC_INT_STS);

    /* Pen Down interrtup */
    if (int_status & TPC_DOWN_IRQ_MSK_BIT)
    {
        TP_PRINT("DOWN\n");
        tp->is_first_down = 1;

        /* Clear down interrupt*/
        CLEAR_TPC_INT (TPC_DOWN_IRQ_MSK_BIT);
        /* Run TPC */
        tp_run();

        /* enable done interrupt */
        sci_adi_set (TPC_INT_EN, TPC_DONE_IRQ_MSK_BIT);

    }
    /* Pen Up interrupt */
    else if(int_status & TPC_UP_IRQ_MSK_BIT)
    {
        TP_PRINT("UP\n");

        /* disable interrupt */
        sci_adi_clr (TPC_INT_EN, TPC_DONE_IRQ_MSK_BIT);
        /* Clear up interrupt*/
        CLEAR_TPC_INT ( (TPC_UP_IRQ_MSK_BIT|TPC_DONE_IRQ_MSK_BIT));
        /* Stop TPC */
        tp_stop();

	    input_report_key(tp->input, BTN_TOUCH, 0);
	    input_report_abs(tp->input, ABS_PRESSURE, 0);
	    input_sync(tp->input);
    }
    /* Sample Done interrupt */
    else if(int_status & TPC_DONE_IRQ_MSK_BIT)
    {
            TP_PRINT("DONE\n");
               /*
            *trace_TP_PRINT("DONE\n");
            *Clear done interrupt
                */
            CLEAR_TPC_INT (TPC_DONE_IRQ_MSK_BIT);

            /* Get data from TP buffer */
            if (tp_fetch_data(&(tp->tp_data))) {
                    /* TP_PRINT("fetch data finish\n"); */
                    /* xd=765-tp->tp_data.data.x; */
                    /* yd=816-tp->tp_data.data.y; */
                    if (cal_mode_en == 0) {
                        #if 0
                        xd=X_MAX-tp->tp_data.data.x;
                        yd=Y_MAX-tp->tp_data.data.y;
                        if(0 == a+b+c+d+e+f+g){
                          input_report_abs(tp->input, ABS_X, xd);
                          input_report_abs(tp->input, ABS_Y, yd);
                          TP_PRINT("cal params all zero.\n");
                        }else{
                          xl=(a*xd+b*yd+c)/g;
                          yl=(d*xd+e*yd+f)/g;
                          xl=(xl+20)*3;
                          yl=(yl+15)*2;

                        }
                        #endif
                        xl = tp->tp_data.data.x;
                        yl = tp->tp_data.data.y;

                        input_report_abs(tp->input, ABS_X, xl);
                        input_report_abs(tp->input, ABS_Y, yl);

                    }
                    else {
                        /* calibration enable, 1: sampling mode
                         2: verification mode. */
                        cal_data.data.x = tp->tp_data.data.x;
                        cal_data.data.y = tp->tp_data.data.y;
                        xd = tp_fix_cx;/* tp->tp_data.data.x; */
                        yd = tp_fix_cy;/* tp->tp_data.data.y; */
                        input_report_abs(tp->input, ABS_X, xd);
                        input_report_abs(tp->input, ABS_Y, yd);

                        /* TP_PRINT("TP cal mode: x = %d, y = %d\n", xd, yd); */
                    }

                    /* TP_PRINT("xd=%d,yd=%d\n",xd,yd); */
                    TP_PRINT("xl=%d,yl=%d\n",xl,yl);
                    input_report_abs(tp->input, ABS_PRESSURE,1);
                    input_report_key(tp->input, BTN_TOUCH, 1);
                    input_sync(tp->input);
                    /* TP_PRINT("report finish\n"); */

            }
            else{
                    TP_PRINT("func[%s]: done interrupt rise,but can not fetch data!\n",__FUNCTION__);
                    /* sci_adi_clr (TPC_INT_EN, TPC_DONE_IRQ_MSK_BIT); */
            }
            /* sci_adi_clr (TPC_INT_EN, TPC_DONE_IRQ_MSK_BIT); */
    }
    /* Error occured */
    else{
        TP_PRINT("func[%s]: uncundefined irq\n",__FUNCTION__);
    }
	return IRQ_HANDLED;
}


static ssize_t cal_mode_sysfs_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	char *endp,*startp;
    uint8_t cal_en = 0;

	startp = endp = (char*)buf;

    cal_en = simple_strtol(startp, &endp, 10);
    if (cal_en > 2) return size;

    if (cal_en == 1) {
        sci_adi_clr (TPC_CALC_CTRL, BIT(0));  /* calibration mode */
    }
    else {
        sci_adi_set (TPC_CALC_CTRL, BIT(0));   /*  normal mode */
    }

    cal_mode_en = cal_en;

	return size;
}

static ssize_t cal_mode_sysfs_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int len;

    len = sprintf(buf, "%d\n", cal_mode_en);

	return len;
}

static DEVICE_ATTR(cal_mode, 0666, cal_mode_sysfs_show, cal_mode_sysfs_store);


static ssize_t cal_data_sysfs_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int len;

    if (cal_mode_en == 0)
        len = sprintf(buf, "0 0 \n");
    else
        len = sprintf(buf, "%d %d \n", cal_data.data.x, cal_data.data.y);

	return len;
}


static DEVICE_ATTR(cal_data, 0444, cal_data_sysfs_show, NULL);


static ssize_t cal_coef_sysfs_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int len;
    len = sprintf(buf, "%#x %#x %#x %#x\n",
        cal_coef.x_coef_a,
        cal_coef.x_coef_b,
        cal_coef.y_coef_a,
        cal_coef.y_coef_b);

	return len;
}

static ssize_t cal_coef_sysfs_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	long para[4];
	int num;
	char *endp,*startp;

	startp = endp = (char*)buf;

	num=0;
	while(*startp && num < 4){

		para[num++] =simple_strtol(startp, &endp, 16);
		if(endp){
           endp++;
		   startp=endp;
        }
	}

    sci_adi_raw_write(TPC_CALC_X_COEF_A, para[0]);
    sci_adi_raw_write(TPC_CALC_X_COEF_B, para[1]);
    sci_adi_raw_write(TPC_CALC_Y_COEF_A, para[2]);
    sci_adi_raw_write(TPC_CALC_Y_COEF_B, para[3]);

	cal_coef.x_coef_a = para[0];
    cal_coef.x_coef_b = para[1];
    cal_coef.y_coef_a = para[2];
    cal_coef.y_coef_b = para[3];

	return size;
}

static DEVICE_ATTR(cal_coef, 0666, cal_coef_sysfs_show, cal_coef_sysfs_store);


/*
 * The functions for inserting/removing us as a module.
 */
static int sprd_tp_probe(struct platform_device *pdev)
{
	struct sprd_tp *tp;
	struct input_dev *input_dev;
	int ret;

        TP_PRINT("func[%s]: touch panel device  will be probed\n", __func__);
	tp = kzalloc(sizeof(struct sprd_tp), GFP_KERNEL);
	if (!tp){
		TP_PRINT("func[%s]:kzalloc failed!\n",__FUNCTION__);
		return  -ENOMEM;
	}

	tp->irq = IRQ_ANA_TPC_INT;

	TP_PRINT("before enter input_allocate_device\n");
    input_dev = input_allocate_device();
	if (!input_dev) {
		TP_PRINT("alloc input device failed \n");
		ret = -ENOMEM;
		goto err_alloc;
	}
    sprd_tp_data = tp;
	tp->input=input_dev;
	tp->input->name = "sprd_touch_screen";
	tp->input->phys = "TP";
	tp->input->id.bustype = BUS_HOST;
	tp->input->id.vendor = 0x8800;
	tp->input->id.product = 0xCAFE;
	tp->input->id.version = 0x0001;
	tp->input->dev.parent = &pdev->dev;

	tp->input->evbit[0] = tp->input->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	tp->input->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(tp->input, ABS_X, X_MIN, TP_LCD_WIDTH, 0, 0);
	input_set_abs_params(tp->input, ABS_Y, Y_MIN, TP_LCD_HEIGHT, 0, 0);
	input_set_abs_params(tp->input, ABS_PRESSURE, 0, 1, 0, 0);
	/*init adc*/
	ADC_Init();
	/*init tpc*/
	tp_init();

	TP_PRINT("before enter request_irq\n");
	ret = request_irq(tp->irq,tp_irq, IRQF_DISABLED, "touch_screen", tp);
	if (ret != 0) {
		TP_PRINT("func[%s]: Could not allocate tp IRQ!\n",__FUNCTION__);
		ret =  -EIO;
		goto err_irq;
	}

	TP_PRINT("before enter input_register_device\n");
	/* All went ok, so register to the input system */
	ret = input_register_device(tp->input);
	if(ret) {
		TP_PRINT("func[%s]: Could not register input device(touchscreen)!\n",__FUNCTION__);
		ret = -EIO;
		goto err_reg;
	}
	platform_set_drvdata(pdev, tp);
#if 0
	if(tp_create_proc())
		TP_PRINT("create touch panel proc file failed!\n");

	if(device_create_file(&tp->input->dev, &dev_attr_tp)) {
	TP_PRINT("create touch panel sysfs file tp failed!\n");
	}
#endif
	if(device_create_file(&tp->input->dev, &dev_attr_cal_mode)) {
	        TP_PRINT("create touch panel sysfs file cal_mode failed!\n");
	}
	if(device_create_file(&tp->input->dev, &dev_attr_cal_data)) {
		TP_PRINT("create touch panel sysfs file cal_data failed!\n");
	}

	if(device_create_file(&tp->input->dev, &dev_attr_cal_coef)) {
		TP_PRINT("create touch panel sysfs file cal_coef failed!\n");
	}

      sci_adi_set(TPC_INT_EN,(TPC_UP_IRQ_MSK_BIT |TPC_DOWN_IRQ_MSK_BIT));


	return 0;

err_reg:
	input_free_device(tp->input);
err_irq:
	free_irq(tp->irq, tp);
err_alloc:
	kfree(tp);

	return ret;
}

static int sprd_tp_remove(struct platform_device *pdev)
{
	struct sprd_tp *tp = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	input_unregister_device(tp->input);

	free_irq(tp->irq, tp);

	kfree(tp);
#if 0
	tp_remove_proc();
	device_remove_file(&tp->input->dev, &dev_attr_tp);
#endif
	device_remove_file(&tp->input->dev, &dev_attr_cal_mode);
	device_remove_file(&tp->input->dev, &dev_attr_cal_data);
	device_remove_file(&tp->input->dev, &dev_attr_cal_coef);

	return 0;
}


#ifdef CONFIG_PM
static int sprd_tp_suspend(struct platform_device *pdev, pm_message_t mesg)
{
    TP_PRINT("func[%s]: touch panel will be disabled\n", __func__);

    /*
    * Disable tp interrupt(up, down and done) and clear these interrupt bits,
    * Then stop tp module and close ADC(tp_stop())
     */
    sci_adi_clr(TPC_INT_EN, (TPC_UP_IRQ_MSK_BIT|TPC_DOWN_IRQ_MSK_BIT|TPC_DONE_IRQ_MSK_BIT));
    CLEAR_TPC_INT(TPC_UP_IRQ_MSK_BIT|TPC_DOWN_IRQ_MSK_BIT|TPC_DONE_IRQ_MSK_BIT);

    tp_stop();

    return 0;
}

static int sprd_tp_resume(struct platform_device *pdev)
{
    TP_PRINT("func[%s]: touch panel will be enabled\n", __func__);

    /* Enable tp interrupt(up and down) */
    sci_adi_set(TPC_INT_EN,(TPC_UP_IRQ_MSK_BIT |TPC_DOWN_IRQ_MSK_BIT));

    return 0;
}
#else
#define	sprd_tp_suspend NULL
#define	sprd_tp_resume NULL
#endif

static struct platform_driver sprd_tp_driver = {
    .probe      = sprd_tp_probe,
    .remove     = sprd_tp_remove,
    .suspend	= sprd_tp_suspend,
    .resume		= sprd_tp_resume,
    .driver		= {
        .owner	= THIS_MODULE,
        .name	= "sprd-tp",
    },
};

static int __init tp_sprd_init(void)
{
	TP_PRINT("TP:SC8800G Touch Panel driver $Revision:1.0 $\n");

	return platform_driver_register(&sprd_tp_driver);
}

static void __exit tp_sprd_exit(void)
{
	TP_PRINT("TP:SC8800G Touch Panel remove!\n");
	platform_driver_unregister(&sprd_tp_driver);
}

module_init(tp_sprd_init);
module_exit(tp_sprd_exit);

MODULE_DESCRIPTION("SC8800 Touch Panel driver");
MODULE_AUTHOR("Ke Wang, <ke.wang@spreadtrum.com>");
MODULE_LICENSE("GPL");
