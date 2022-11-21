/*
 *  drivers/switch/sec_jack.c
 *
 *  headset & hook detect driver for Spreadtrum (SAMSUNG KSND COMSTOM)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
*/

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <sec_jack.h>
//#include <mach/board.h>
#include <linux/input.h>

#include <mach/adc.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <linux/module.h>
#include <linux/wakelock.h>

#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <mach/sci_glb_regs.h>
#include <mach/arch_misc.h>
#include <linux/notifier.h>
#ifdef CONFIG_OF
#include <linux/slab.h>
#include <linux/of_device.h>
#endif
#include <asm/div64.h>
#include <linux/sched.h>
#include <linux/timer.h>
#if defined (SEC_HEADSET_ADC_ADJUST)
#include <linux/uaccess.h>
#include <linux/file.h>
#endif /* SEC_HEADSET_ADC_ADJUST */

#define MAX_ZONE_LIMIT		10
#define WAKE_LOCK_TIME		(HZ * 5)	/* 5 sec */

static struct headset_info *info;
static struct platform_device *this_pdev = NULL;

static BLOCKING_NOTIFIER_HEAD(hp_chain_list);
int hp_register_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&hp_chain_list, nb);
}
EXPORT_SYMBOL(hp_register_notifier);

int hp_unregister_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&hp_chain_list, nb);
}
EXPORT_SYMBOL(hp_unregister_notifier);

int get_hp_plug_state(void)
{
    return (info->cur_jack_type)? 1:0;
}
EXPORT_SYMBOL(get_hp_plug_state);

//static DEFINE_SPINLOCK(irq_button_lock);
static DEFINE_SPINLOCK(irq_detect_lock);

/* sysfs name HeadsetObserver.java looks for to track headset state
*/
struct switch_dev switch_jack_detection = {
    .name = "h2w",
};
/* Samsung factory test application looks for to track button state
*/
struct switch_dev switch_sendend = {
    .name = "send_end", /* /sys/class/switch/send_end/state */
};
static char *sec_jack_status[] = {
    "NONE",
    "HEADSET_4POLE",
    "HEADSET_3POLE",
};

#if defined (SEC_HEADSET_ADC_ADJUST)
#define ADC_PATH "data/data/com.sec.ksndtestmode/files/"
#define ADC_NAME "headset_adc.dat"
#define FILE_BUFFER_SIZE  256

enum {
    POLE3_LOW_ADC,
    POLE3_MAX_ADC,
    POLE4_LOW_ADC,
    POLE4_MAX_ADC,
    KEY_MEDIA_ADC,
    KEY_VOICECOMMAND_ADC,
    KEY_VOLUME_UP_ADC,
    KEY_VOLUME_DOWN_ADC,
    MAX_ARRAY
} sed_jack_adc_type;

static char file_path[64];
static bool is_init = false;

static int sec_jack_read_data(struct headset_info *hi, char * filename);
static int sec_jack_write_data(struct headset_info *hi, char * filename );
#endif /* SEC_HEADSET_ADC_ADJUST */


/*************************************************************/
// Mic bias control code
/*************************************************************/
static int headset_audio_block_is_running(struct device *dev)
{
    return regulator_is_enabled(sprd_hts_power.vcom_buf)
        || regulator_is_enabled(sprd_hts_power.vbo);
}

static int headset_audio_headmic_sleep_disable(struct device *dev, int on)
{
    int ret = 0;

    if (!sprd_hts_power.head_mic) {
        return -1;
    }

    if (on) {
        ret = regulator_enable(sprd_hts_power.vbo);
        ret = regulator_set_mode(sprd_hts_power.head_mic, REGULATOR_MODE_NORMAL);
    } else {
        ret = regulator_disable(sprd_hts_power.vbo);
        if (headset_audio_block_is_running(dev)) {
            ret = regulator_set_mode(sprd_hts_power.head_mic, REGULATOR_MODE_NORMAL);
        } else {
            ret = regulator_set_mode(sprd_hts_power.head_mic, REGULATOR_MODE_STANDBY);
        }
    }

    return ret;
}


static void headmic_sleep_disable(struct device *dev, int on)
{
    static int current_power_state = 0;

    if (1 == on) {
        if (0 == current_power_state) {
            headset_audio_headmic_sleep_disable(dev, 1);
            current_power_state = 1;
        }
    } else {
        if (1 == current_power_state) {
            headset_audio_headmic_sleep_disable(dev, 0);
            current_power_state = 0;
        }
    }
}

static int headset_headmic_bias_control(struct device *dev, int on)
{
    int ret = 0;

    if (!sprd_hts_power.head_mic) {
        return -1;
    }

    if (on) {
        ret = regulator_enable(sprd_hts_power.head_mic);
    } else {
        ret = regulator_disable(sprd_hts_power.head_mic);
    }

    if (!ret) {
        /* Set HEADMIC_SLEEP when audio block closed */
        if (headset_audio_block_is_running(dev)) {
            ret = regulator_set_mode(sprd_hts_power.head_mic, REGULATOR_MODE_NORMAL);
        } else {
            ret = regulator_set_mode(sprd_hts_power.head_mic, REGULATOR_MODE_STANDBY);
        }
    }

    return ret;
}

static void mic_set_power(struct device *dev, int on)
{
    static int current_power_state = 0;

    if (on) {
        if (current_power_state == 0) {
            headset_headmic_bias_control(dev, 1);
            current_power_state = 1;
        }
    } else {
        if (current_power_state == 1) {
            headset_headmic_bias_control(dev, 0);
            current_power_state = 0;
        }
    }
}

#if 1
/************************************************************/
/* ADC data code */
/************************************************************/
#define DELTA1_BLOCK9 9
#define DELTA2_BLOCK10 10
#define DELTA3_BLOCK11 11
#define BITCOUNT 8
#define BLK_WIDTH 8
#define SPRD_HEADSET_AUXADC_CAL_NO 0
#define SPRD_HEADSET_AUXADC_CAL_DO 1
extern u32 __adie_efuse_read_bits(int bit_index, int length);

struct sprd_headset_auxadc_cal_l {
    u32 A;
    u32 B;
    u32 E;
    u32 cal_type;
};

static struct sprd_headset_auxadc_cal_l adc_cal_headset = {
    0, 0,0,SPRD_HEADSET_AUXADC_CAL_NO,
};

static void adc_cal_from_efuse(void)
{
    u32 delta[3] = {0};
    u32 block0_bit7 = 128;
    u8 test[3] = {0};

    if (adc_cal_headset.cal_type == SPRD_HEADSET_AUXADC_CAL_NO) {

        delta[0] = __adie_efuse_read_bits(DELTA1_BLOCK9*BLK_WIDTH,BITCOUNT);
        delta[1] = __adie_efuse_read_bits(DELTA2_BLOCK10*BLK_WIDTH,BITCOUNT);
        delta[2] = __adie_efuse_read_bits(DELTA3_BLOCK11*BLK_WIDTH,BITCOUNT);

        test[0] = delta[0]&0xff;
        test[1] = delta[1]&0xff;
        test[2] = delta[2]&0xff;
        pr_info("test[0] 0x%x %d, test[1] 0x%x %d, test[2] 0x%x %d \n",test[0],test[0],test[1],test[1],test[2],test[2]);

        block0_bit7 = __adie_efuse_read_bits(0,BITCOUNT);
        pr_info("block_7 0x%08x \n",block0_bit7);
        if(block0_bit7&(1<<7)){
            pr_info("block 0 bit 7 set 1 no efuse data\n");
            return;
        } else {
            adc_cal_headset.cal_type = SPRD_HEADSET_AUXADC_CAL_DO;
            pr_info("block 0 bit 7 set 0 have efuse data\n");
        }

        pr_info("test[0] %d, test[1] %d, test[2] %d \n",test[0],test[1],test[2]);

        adc_cal_headset.A = (test[0]*4)-184;
        adc_cal_headset.B = (test[1]*4)+2764;
        adc_cal_headset.E = (test[2]*2)+2500;
        pr_info("adc_cal_headset.A %d, adc_cal_headset.B %d, adc_cal_headset.E %d \n",adc_cal_headset.A,adc_cal_headset.B,adc_cal_headset.E);
    } else {
        pr_info("efuse A,B,E has been calculated! \n");
    }

}

static int adc_get_ideal(u32 adc_mic)
{
    u64 numerator = 0;
    u32 denominator = 0;
    u32 adc_ideal = 0;
    u32 a,b,e;

    if (adc_cal_headset.cal_type == SPRD_HEADSET_AUXADC_CAL_DO){
        a = adc_cal_headset.A;
        b = adc_cal_headset.B;
        e = adc_cal_headset.E;

        if (9*adc_mic + b < 10*a){
            return adc_mic;
        }
        denominator = e*(b-a);
        numerator = 917280*(9*(u64)adc_mic+(u64)b-10*(u64)a);
        pr_info("denominator=%u, numerator=%llu\n", denominator, numerator);
        do_div(numerator,denominator);
        adc_ideal = (u32)numerator;
        pr_info("adc_mic=%d, adc_ideal=%d\n", adc_mic, adc_ideal);
        return adc_ideal;
    } else {
        return adc_mic;
    }
}

static void adc_en(int en)
{
    if (en) {
        headset_reg_set_bit(HEADMIC_DETECT_REG(ANA_HDT0), AUDIO_V2ADC_EN);
        headset_reg_set_val(HEADMIC_DETECT_REG(ANA_HDT0), AUDIO_HEAD2ADC_SEL_HEADMIC_IN, 
                            AUDIO_HEAD2ADC_SEL_MASK, AUDIO_HEAD2ADC_SEL_SHIFT);
        headmic_sleep_disable(&this_pdev->dev, 1);
    } else {
        headset_reg_clr_bit(HEADMIC_DETECT_REG(ANA_HDT0), AUDIO_V2ADC_EN);
        headset_reg_set_val(HEADMIC_DETECT_REG(ANA_HDT0), AUDIO_HEAD2ADC_SEL_DISABLE, 
                            AUDIO_HEAD2ADC_SEL_MASK, AUDIO_HEAD2ADC_SEL_SHIFT);
        headmic_sleep_disable(&this_pdev->dev, 0);
    }
}

/* is_set = 1, headset_mic to AUXADC */
static void adc_to_headmic(unsigned is_set)
{
    adc_en(1);

    if (is_set) {
        headset_reg_set_val(HEADMIC_DETECT_REG(ANA_HDT0), 
            AUDIO_HEAD2ADC_SEL_HEADMIC_IN, AUDIO_HEAD2ADC_SEL_MASK, AUDIO_HEAD2ADC_SEL_SHIFT);
    } else {
        headset_reg_set_val(HEADMIC_DETECT_REG(ANA_HDT0), 
            AUDIO_HEAD2ADC_SEL_HEADSET_L_INT, AUDIO_HEAD2ADC_SEL_MASK, AUDIO_HEAD2ADC_SEL_SHIFT);
    }
}

static inline uint32_t headset_reg_get_bit(uint32_t addr, uint32_t bit)
{
    uint32_t temp;
    temp = sci_adi_read(addr);
    temp = temp & bit;

    return temp;
}

static int wrap_sci_adc_get_value(unsigned int channel, int scale)
{
    int count = 0;
    int average = 0;

    while(count < SCI_ADC_GET_VALUE_COUNT) {
        average += sci_adc_get_value(channel, scale);
        count++;
    }
    average /= SCI_ADC_GET_VALUE_COUNT;

    return average;
}

static int sec_jack_get_adc_value(int gpio_num, int gpio_value)
{
    msleep(1);

    if(0 == headset_reg_get_bit(HEADMIC_DETECT_REG(ANA_PMU0), AUDIO_HEADMIC_SLEEP_EN))
        pr_info("AUDIO_HEADMIC_SLEEP_EN is disabled\n");

    if(gpio_get_value(gpio_num) != gpio_value) {
        pr_info("gpio value changed!!! the adc read operation aborted (step1)\n");
        return -1;
    }

    return wrap_sci_adc_get_value(ADC_CHANNEL_HEADMIC, 1);
}
#endif

#if defined (SEC_HEADSET_ADC_ADJUST)
static int sec_jack_read_data( struct headset_info *hi, char * filename )
{
    struct sec_jack_platform_data *pdata = hi->platform_data;
    struct file *flip;
    char *bufs, *token;
    int i=0,size,ret;
    unsigned long adc_array[MAX_ARRAY];

    // kernel memory access setting
    mm_segment_t oldfs = get_fs();
    set_fs(KERNEL_DS);

    // set default name if we don't set any data file name
    if(filename == NULL){
        filename = file_path;
    }

    flip = filp_open(filename, O_RDWR, S_IRUSR|S_IWUSR);
    if(IS_ERR(flip)){
        pr_err("%s : File not found or open error!\n",__func__);
        return -1;
    }

    //file size
    size = flip->f_dentry->d_inode->i_size;

    bufs = kmalloc( size, GFP_KERNEL);
    if( bufs == NULL ){
        pr_err("%s : kmalloc error!\n", __func__);
        return -1;
    }
    ret = vfs_read(flip, bufs, size, &flip->f_pos);

    //set to the array
    while ((token = strsep(&bufs, ",")) != NULL) {
    if ( i == MAX_ARRAY ||(strict_strtoul(token , 10, &adc_array[i]) < 0))
        break;
        i++;
    }

    /* Close the file */
    if( flip ){
        filp_close(flip, NULL);
    }
    set_fs(oldfs);
    kfree(bufs);

    //check the data buffer has all 0 or not.
    for( i=0;i<MAX_ARRAY;i++ ){
        if( adc_array[i] != 0 ){
            goto read_data;
        }
    }
    goto rewrite_data;

rewrite_data:
    //Restore the headset adc data table file
    is_init = false;
    sec_jack_write_data(info, NULL); //ksnd
    return 0;

read_data:
    //Save the headset adc data table file
    pdata->jack_zones[0].adc_high = adc_array[POLE3_MAX_ADC];
    pdata->jack_zones[0].adc_high = adc_array[POLE4_LOW_ADC];
    pdata->jack_zones[1].adc_high = adc_array[POLE4_MAX_ADC];
    pdata->buttons_zones[0].adc_high = adc_array[KEY_MEDIA_ADC];
    pdata->buttons_zones[0].adc_high = adc_array[KEY_VOICECOMMAND_ADC];
    pdata->buttons_zones[1].adc_high = adc_array[KEY_VOLUME_UP_ADC];
    pdata->buttons_zones[2].adc_high = adc_array[KEY_VOLUME_DOWN_ADC];

    pr_info("%s : Read the ADC data file. size:%d\n", __func__, ret);
    return 0;
 }

static int sec_jack_write_data(struct headset_info *hi, char * filename )
{
    struct sec_jack_platform_data *pdata = hi->platform_data;
    struct file     *flip;
    unsigned int adc_array[MAX_ARRAY];
    char    *result;
    int     i;
    char    buf[128];
    mm_segment_t    oldfs = get_fs();

    // kernel memory access setting
    set_fs(KERNEL_DS);

    // set default name if we don't set any data file name
    if(filename == NULL){
        filename = file_path;
    }

    //open the adc data file from user fs
    flip =  filp_open(filename, O_RDWR, S_IRUSR|S_IWUSR);
    if(IS_ERR(flip)){
        flip = NULL;
        pr_err("%s : File not found or open error!\n", __func__);
        return -1;
    }

    //init buffer to write adc data to the file
    adc_array[POLE3_LOW_ADC] = 0;
    adc_array[POLE3_MAX_ADC] = pdata->jack_zones[0].adc_high;
    adc_array[POLE4_LOW_ADC] = pdata->jack_zones[0].adc_high;
    adc_array[POLE4_MAX_ADC] = pdata->jack_zones[1].adc_high;
    adc_array[KEY_MEDIA_ADC] = pdata->buttons_zones[0].adc_high;
    adc_array[KEY_VOICECOMMAND_ADC] = pdata->buttons_zones[0].adc_high;
    adc_array[KEY_VOLUME_UP_ADC] = pdata->buttons_zones[1].adc_high;
    adc_array[KEY_VOLUME_DOWN_ADC] = pdata->buttons_zones[2].adc_high;

    result = kmalloc( FILE_BUFFER_SIZE, GFP_KERNEL);
    if( result == NULL ){
        pr_err("%s : kmalloc error!\n", __func__);
        return -1;
    }

    //make the data format with adc data.
    for(i=0; i < MAX_ARRAY; i++){
        sprintf(buf, "%d", adc_array[i] );
        strcat(result, buf);
        strcat(result, ",");
    }
    pr_info("%s : data : %s \n", __func__, result);

    vfs_write(flip, result, strlen(result), &flip->f_pos);
    set_fs(oldfs);
    kfree(result);

    // Close  the adc data file from user fs
    if( flip ){
        filp_close(flip, NULL);
    }
    is_init = true;

    pr_info("%s : Write the ADC data file\n", __func__);
    return 0;
}

static void sec_jack_adcData_init (void)
{
    //get the file path name
    strcpy(file_path, ADC_PATH);
    strcat(file_path, ADC_NAME);

    is_init = false;
    pr_info("%s : file path : %s \n", __func__, file_path);
}
#endif /* SEC_HEADSET_ADC_ADJUST */


/************************************************************/
/************************************************************/
static void sec_jack_hook_int(struct headset_info *info, int enable)
{
    //unsigned long spin_lock_flags;

    mutex_lock(&info->hs_mutex);
    //spin_lock_irqsave(&irq_button_lock, spin_lock_flags);
    if (enable && info->hook_count == 0) {
        enable_irq(info->irq_button);
        info->hook_count = 1;
    } else if (!enable && info->hook_count == 1) {
        disable_irq(info->irq_button);
        info->hook_count = 0;
    }
    mutex_unlock(&info->hs_mutex);
    //spin_unlock_irqrestore(&irq_button_lock, spin_lock_flags);
}

static void sec_jack_detect_int(struct headset_info *info, int enable)
{
    unsigned long spin_lock_flags;

    //mutex_lock(&info->dt_mutex);
    spin_lock_irqsave(&irq_detect_lock, spin_lock_flags);
    if (enable && info->detect_count == 0) {
        enable_irq(info->irq_detect);
        info->detect_count = 1;
    } else if (!enable && info->detect_count == 1) {
        disable_irq_nosync(info->irq_detect);
        info->detect_count = 0;
    }
    //mutex_unlock(&info->dt_mutex);
    spin_unlock_irqrestore(&irq_detect_lock, spin_lock_flags);
}

static void sec_jack_key_handle(struct headset_info *info, int voltage)
{
    struct sec_jack_buttons_zone *btn_zones = info->platform_data->buttons_zones;
    struct sec_jack_platform_data *pdata = info->platform_data;
    unsigned npolarity = !pdata->button_active_high;
    int push_button = 0;
    int i = 0;

    push_button = gpio_get_value(pdata->gpio_button) ^ npolarity;

    if( push_button == 1 ){
        /* Pressed headset key */
        for (i = 0; i < pdata->nbuttons; i++){
            if (voltage >= btn_zones[i].adc_low && voltage <= btn_zones[i].adc_high) {
                info->pressed_code = btn_zones[i].code;

                if( info->pressed_code == KEY_MEDIA ){
                    info->hook_vol_status = HOOK_PRESSED;
                } else if( info->pressed_code == KEY_VOLUMEDOWN ){
                    info->hook_vol_status = VOL_DOWN_PRESSED;
                } else {
                    info->hook_vol_status = VOL_UP_PRESSED;
                }
                input_event(info->idev, EV_KEY, info->pressed_code, 1);
                input_sync(info->idev);
                pr_info("%s: keycode=%d, is pressed, adc= %d mv\n", __func__, info->pressed_code, voltage);
                return;
            }
        }
        pr_warn("%s: key is skipped. ADC value is %d\n", __func__, voltage);
    } else {
        /* Released Headset key */
        if(info->hook_vol_status <= HOOK_VOL_ALL_RELEASED){
            /* No Need to sent the event */
            return;
        }

        if( info->pressed_code == KEY_MEDIA ){
            info->hook_vol_status = HOOK_RELEASED;
        } else if( info->pressed_code == KEY_VOLUMEDOWN ){
            info->hook_vol_status = VOL_DOWN_RELEASED;
        } else {
            info->hook_vol_status = VOL_UP_RELEASED;
        }
        input_event(info->idev, EV_KEY, info->pressed_code, 0);
        input_sync(info->idev);
        pr_info("%s: keycode=%d, is released\n", __func__,  info->pressed_code );
    }
}

static void sec_jack_set_type(struct headset_info *info, int jack_type)
{
    if( info == NULL ){
        pr_err("%s: Fail to get headset info point! \n", __func__ );
        return;
    }

    switch( jack_type ){
        case SEC_HEADSET_4POLE:
            sec_jack_hook_int(info, 1);
#if defined (SEC_HEADSET_ADC_ADJUST)
            /* Update and inform the adc date file */
            if( is_init ){
                sec_jack_read_data(info, NULL);
            } else {
                sec_jack_write_data(info, NULL);
            }
#endif /* SEC_HEADSET_ADC_ADJUST */
            break;

        case SEC_HEADSET_3POLE:
            mic_set_power(&this_pdev->dev, 0);  //mic bias off
            sec_jack_hook_int(info, 0);
            break;

        case SEC_JACK_NO_DEVICE:
            //mic bias off
            mic_set_power(&this_pdev->dev, 0);
            sec_jack_hook_int(info, 0);
            info->hook_vol_status = HOOK_VOL_ALL_RELEASED;
            break;

        case SEC_UNKNOWN_DEVICE:
        default:
            /* No excute */
            pr_info("%s: jack:SEC_UNKNOWN_DEVICE!\n",__func__);
            return;
    }

    info->cur_jack_type = jack_type;
    switch_set_state(&switch_jack_detection, jack_type);
    pr_info("%s: %s State \n", __func__ , sec_jack_status[jack_type] );
}

static void determine_jack_type( struct headset_info *info )
{
    struct sec_jack_platform_data *pdata = info->platform_data;
    int count[MAX_ZONE_LIMIT] = {0};
    struct sec_jack_zone *zones = pdata->jack_zones;
    int size = 2;
    int adc;
    int i;
    unsigned npolarity = !pdata->det_active_high;

    //mic bias on
    mic_set_power(&this_pdev->dev, 1);
    //msleep(100);

    while (gpio_get_value(pdata->gpio_detect) ^ npolarity) {
        //get adc value of mic
        adc_to_headmic(1);
        adc = adc_get_ideal(sec_jack_get_adc_value(info->platform_data->gpio_detect, 0));

        pr_info("%s : adc : %d\n", __func__,adc);

        /* determine the type of headset based on the
            * adc value.  An adc value can fall in various
            * ranges or zones.  Within some ranges, the type
            * can be returned immediately.  Within others, the
            * value is considered unstable and we need to sample
            * a few more types (up to the limit determined by
            * the range) before we return the type for that range.
           */
        for (i = 0; i < size; i++) {
            if (adc <= zones[i].adc_high) {
                if (++count[i] > zones[i].check_count) {
                    sec_jack_set_type(info, zones[i].jack_type);
                    return;
                }
                if (zones[i].delay_us > 0)
                    usleep_range(zones[i].delay_us, zones[i].delay_us);
                break;
            }
        }
    }
    sec_jack_set_type(info, SEC_JACK_NO_DEVICE);

    //mic bias off
    mic_set_power(&this_pdev->dev, 0);
}

/* thread run whenever the button of headset is pressed or released */
void sec_jack_buttons_work(struct work_struct *work)
{
    struct sec_jack_platform_data *pdata = info->platform_data;
    unsigned npolarity = !pdata->det_active_high;
    int voltage =0;

    msleep(50);

    if( (gpio_get_value(pdata->gpio_detect) ^ npolarity) == 0 ){
        sec_jack_hook_int(info, 0);
        return;
    }

    //get adc value of mic
    adc_to_headmic(1);
    voltage = adc_get_ideal(sec_jack_get_adc_value(info->platform_data->gpio_detect, 0));

    sec_jack_key_handle(info, voltage);
}

void sec_jack_detect_work(struct work_struct *work)
{
    struct sec_jack_platform_data *pdata = info->platform_data;
    unsigned npolarity = !pdata->det_active_high;

    pr_info("%s: detect_irq(%d)\n", __func__,	
                                gpio_get_value(pdata->gpio_detect) ^ npolarity);

    determine_jack_type(info);

    sec_jack_detect_int(info, 1);
}

static irqreturn_t sec_jack_button_irq(int irq, void *dev)
{
    struct headset_info *info = dev;

    pr_err("%s : Called!!!!\n", __func__);

    queue_work(info->button_queue, &info->button_work);
    return IRQ_HANDLED;
}

static irqreturn_t sec_jack_detect_irq(int irq, void *dev)
{
    struct headset_info *info = dev;

    pr_err("%s : Called!!!!\n", __func__);

    sec_jack_hook_int(info, 0);
    sec_jack_detect_int(info, 0);

    /* prevent suspend to allow user space to respond to switch */
    wake_lock_timeout(&info->det_wake_lock, WAKE_LOCK_TIME);

    //queue_work(info->queue, &info->detect_work);
    queue_delayed_work(info->queue, &info->detect_work, msecs_to_jiffies(70));
    return IRQ_HANDLED;
}

#if 1
static int headset_power_get(struct device *dev, struct regulator **regu, const char *id)
{
    if (!*regu) {
        *regu = regulator_get(dev, id);
        if (IS_ERR(*regu)) {
            pr_err("ERR:Failed to request %ld: %s\n", PTR_ERR(*regu), id);
            *regu = 0;
            return PTR_ERR(*regu);
        }
    }
    return 0;
}

#if 0
static void headset_detect_init(void)
{
    //address:0x4003_8800+0x00
    headset_reg_set_bit(HEADMIC_DETECT_GLB_REG(0x00), (HDT_EN | AUD_EN));
    //address:0x4003_8800+0x04
    headset_reg_set_bit(HEADMIC_DETECT_GLB_REG(0x04), (CLK_AUD_HID_EN | CLK_AUD_HBD_EN));

    /* set headset detect voltage */
    headset_reg_set_bit(HEADMIC_DETECT_REG(ANA_PMU1), AUDIO_PMUR1);
    headset_reg_set_val(HEADMIC_DETECT_REG(ANA_HDT0), AUDIO_HEAD_SDET_2P5, AUDIO_HEAD_SDET_MASK, AUDIO_HEAD_SDET_SHIFT);
    headset_reg_set_val(HEADMIC_DETECT_REG(ANA_HDT0), AUDIO_HEAD_INS_VREF_2P7, AUDIO_HEAD_INS_VREF_MASK, AUDIO_HEAD_INS_VREF_SHIFT);
}
#endif

static int headset_power_init(struct device *dev)
{
    int ret = 0;
    ret = headset_power_get(dev, &sprd_hts_power.head_mic, "HEADMICBIAS");
    if (ret || (sprd_hts_power.head_mic == NULL)) {
            sprd_hts_power.head_mic = 0;
            return ret;
    }
    regulator_set_voltage(sprd_hts_power.head_mic, 950000, 950000);

    ret = headset_power_get(dev, &sprd_hts_power.vcom_buf, "VCOM_BUF");
    if (ret) {
            sprd_hts_power.vcom_buf = 0;
            goto __err1;
    }

    ret = headset_power_get(dev, &sprd_hts_power.vbo, "VBO");
    if (ret) {
            sprd_hts_power.vbo = 0;
            goto __err2;
    }

    goto __ok;
__err2:
    regulator_put(sprd_hts_power.vcom_buf);
__err1:
    regulator_put(sprd_hts_power.head_mic);
__ok:
    return ret;
}
#endif 

#ifdef CONFIG_OF
static struct sec_jack_platform_data *sec_jack_parse_dt(struct device *dev)
{
    struct sec_jack_platform_data *pdata;
    struct device_node *np = dev->of_node,*buttons_np = NULL;
    struct sec_jack_buttons_zone *buttons_zones;
    int ret;

    pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
    if (!pdata) {
        dev_err(dev, "could not allocate memory for platform data\n");
        return NULL;
    }

    ret = of_property_read_u32(np, "gpio_detect", &pdata->gpio_detect);
    if(ret) {
        dev_err(dev, "fail to get gpio_detect\n");
        goto fail;
    }
    ret = of_property_read_u32(np, "gpio_button", &pdata->gpio_button);
    if(ret) {
        dev_err(dev, "fail to get gpio_button\n");
        goto fail;
    }

    ret = of_property_read_u32(np, "adc_threshold_3pole_detect", &pdata->adc_threshold_3pole_detect);
    if(ret) {
        dev_err(dev, "fail to get adc_threshold_3pole_detect\n");
        goto fail;
    }

    ret = of_property_read_u32(np, "adc_threshold_4pole_detect", &pdata->adc_threshold_4pole_detect);
    if(ret) {
        dev_err(dev, "fail to get adc_threshold_4pole_detect\n");
        goto fail;
    }

    pdata->jack_zones[0].adc_high = pdata->adc_threshold_3pole_detect;
    pdata->jack_zones[0].delay_us = 10000;
    pdata->jack_zones[0].check_count = 10;
    pdata->jack_zones[0].jack_type = SEC_HEADSET_3POLE;

    pdata->jack_zones[1].adc_high = pdata->adc_threshold_4pole_detect;
    pdata->jack_zones[1].delay_us = 10000;
    pdata->jack_zones[1].check_count = 10;
    pdata->jack_zones[1].jack_type = SEC_HEADSET_4POLE;

    ret = of_property_read_u32(np, "voltage_headmicbias", &pdata->voltage_headmicbias);
    if(ret) {
        dev_err(dev, "fail to get voltage_headmicbias\n");
        goto fail;
    }
    ret = of_property_read_u32(np, "nbuttons", &pdata->nbuttons);
    if(ret) {
        dev_err(dev, "fail to get nbuttons\n");
        goto fail;
    }

    buttons_zones = kzalloc(pdata->nbuttons*sizeof(*buttons_zones),GFP_KERNEL);
    if (!buttons_zones) {
        dev_err(dev, "could not allocate memory for headset_buttons\n");
        goto fail;
    }
    pdata->buttons_zones = buttons_zones;

    for_each_child_of_node(np,buttons_np) {
        ret = of_property_read_u32(buttons_np, "adc_min", &buttons_zones->adc_low);
        if (ret) {
                dev_err(dev, "fail to get adc_min\n");
                goto fail_buttons_data;
        }
        ret = of_property_read_u32(buttons_np, "adc_max", &buttons_zones->adc_high);
        if (ret) {
                dev_err(dev, "fail to get adc_max\n");
                goto fail_buttons_data;
        }
        ret = of_property_read_u32(buttons_np, "code", &buttons_zones->code);
        if (ret) {
                dev_err(dev, "fail to get code\n");
                goto fail_buttons_data;
        }
        printk("device tree data: adc_min = %d adc_max = %d code = %d \n", buttons_zones->adc_low,
               buttons_zones->adc_high, buttons_zones->code );
        buttons_zones++;
    };

    return pdata;

fail_buttons_data:
    kfree(buttons_zones);
    pdata->buttons_zones = NULL;
fail:
    kfree(pdata);
    return NULL;
}
#endif

static int sec_jack_probe(struct platform_device *pdev)
{
    struct sec_jack_platform_data *pdata = pdev->dev.platform_data;
    int i = 0;
    int ret = -1;

    info = devm_kzalloc(&pdev->dev, sizeof(struct headset_info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;

#ifdef CONFIG_OF
    if (pdev->dev.of_node && !pdata) {
        pdata = sec_jack_parse_dt(&pdev->dev);
        if(pdata)
            pdev->dev.platform_data = pdata;
        info->platform_data = pdata;
    }
    if (!pdata) {
        pr_info("headset_detect_probe get platform_data NULL\n");
        ret = -EINVAL;
        //goto fail_to_get_platform_data;
    }
#endif

    info->idev = input_allocate_device();
    if (!info->idev) {
        dev_err(&pdev->dev, "Failed to allocate input dev\n");
        ret = -ENOMEM;
        //goto input_allocate_fail;
    }

    info->idev->name = "headset-keyboard";
    info->idev->id.bustype = BUS_HOST;
    info->idev->dev.parent = &pdev->dev;
    info->hook_vol_status = HOOK_VOL_ALL_RELEASED;
    info->cur_jack_type = SEC_JACK_NO_DEVICE;

    for (i = 0; i < pdata->nbuttons; i++) {
        struct sec_jack_buttons_zone *buttons = &pdata->buttons_zones[i];
        input_set_capability(info->idev, EV_KEY, buttons->code);
    }
    ret = input_register_device(info->idev);
    if (ret) {
        pr_err("input_register_device for headset_button failed!\n");
        //goto failed_to_register_input_device;
    }

    this_pdev = pdev;

    //headset_detect_init();
    headset_power_init(&pdev->dev);
   
    ret = gpio_request(info->platform_data->gpio_detect, "headset_detect");
    if (ret < 0) {
        pr_info("failed to request GPIO_%d(headset_detect)\n", pdata->gpio_detect);
        goto failed_to_request_gpio;
    } else {
        gpio_direction_input(info->platform_data->gpio_detect);
    }
   
    ret = gpio_request(info->platform_data->gpio_button, "headset_button");
    if (ret < 0) {
        pr_info("failed to request GPIO_%d(headset_button)\n", pdata->gpio_button);
        goto failed_to_request_gpio;
    } else {
        gpio_direction_input(info->platform_data->gpio_button);
    }

    //gpio_set_debounce(info->platform_data->gpio_detect, 10000);
    //gpio_set_debounce(info->platform_data->gpio_button, 10000);

    info->irq_detect = gpio_to_irq(info->platform_data->gpio_detect);
    info->irq_button = gpio_to_irq(info->platform_data->gpio_button);
    info->platform_data->det_active_high = 0;
    info->platform_data->button_active_high = 0;
       
    ret = switch_dev_register(&switch_jack_detection);
    if (ret < 0) {
        pr_err("%s : Failed to register switch device\n", __func__);
        goto err_switch_dev_register;
    }

    ret = switch_dev_register(&switch_sendend);
    if (ret < 0) {
        pr_err("%s : Failed to register switch device\n", __func__);
        goto err_switch_dev_register;
    }

    wake_lock_init(&info->det_wake_lock, WAKE_LOCK_SUSPEND, "sec_jack_det");

    mutex_init(&info->hs_mutex);
    mutex_init(&info->dt_mutex);

    INIT_WORK(&info->button_work, sec_jack_buttons_work);
    INIT_DELAYED_WORK(&info->detect_work, sec_jack_detect_work);
    //INIT_WORK(&info->detect_work, sec_jack_detect_work);

    info->queue = create_singlethread_workqueue("sec_jack_wq");
    if (info->queue == NULL) {
        ret = -ENOMEM;
        pr_err("%s: Failed to create workqueue\n", __func__);
        //goto err_create_wq_failed;
    }

    //get the init headset state
    //queue_work(info->queue, &info->detect_work);
    queue_delayed_work(info->queue, &info->detect_work, msecs_to_jiffies(0));

    info->button_queue = create_singlethread_workqueue("sec_jack_buttons_wq");
    if (info->button_queue == NULL) {
        ret = -ENOMEM;
        pr_err("%s: Failed to create buttons workqueue\n", __func__);
        //goto err_create_wq_failed;
    }

    ret = devm_request_irq(&pdev->dev, info->irq_detect, sec_jack_detect_irq, 
                        IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING | IRQF_ONESHOT, 
                        "sec_jack_detect_irq", info);
    if (ret < 0) {
        pr_err("%s : Failed to request_irq.\n", __func__);
        goto failed_to_request_irq_headset;
    }
   
    ret = devm_request_irq(&pdev->dev, info->irq_button, sec_jack_button_irq,
                        IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING| IRQF_ONESHOT,
                        "sec_jack_button_irq", info);
    if (ret < 0) {
        pr_err("%s : Failed to request_irq.\n", __func__);
        goto failed_to_request_irq_headset;
    }

    /* to handle insert/removal when we're sleeping in a call */
    ret = enable_irq_wake(info->irq_detect);
    if (ret) {
        pr_err("%s : Failed to enable_irq_wake.\n", __func__);
        //goto err_enable_irq_wake;
    }

#if defined (SEC_HEADSET_ADC_ADJUST)
    sec_jack_adcData_init();
#endif /* SEC_HEADSET_ADC_ADJUST */

    /*
      * disable hook irq to ensure this irq will be enabled
      * after plugging in headset
      */
    info->detect_count = 1;

    info->hook_count = 1;
    sec_jack_hook_int(info, 0);
    adc_cal_from_efuse();

    return ret;

failed_to_request_irq_headset:
    free_irq(info->irq_detect, info);
    free_irq(info->irq_button, info);
err_switch_dev_register:
failed_to_request_gpio:
    devm_kfree(&pdev->dev, info);
    wake_lock_destroy(&info->det_wake_lock);

    return ret;
}


#ifdef CONFIG_PM
static int sec_jack_suspend(struct platform_device *dev, pm_message_t state)
{
    return 0;
}

static int sec_jack_resume(struct platform_device *dev)
{
    return 0;
}
#else
#define sec_jack_suspend NULL
#define sec_jack_resume NULL
#endif


static const struct of_device_id sec_jack_detect_of_match[] = {
    {.compatible = "sprd,headset_sprd_sc2723",},
    { }
};
static struct platform_driver sec_jack_driver = {
    .probe = sec_jack_probe,
    .suspend = sec_jack_suspend,
    .resume = sec_jack_resume,
    .driver = {
        .name = "headset_sprd_sc2723",
        .owner = THIS_MODULE,
        .of_match_table = sec_jack_detect_of_match,
    },
};

static int __init sec_jack_init(void)
{
    int ret;

    ret = platform_driver_register(&sec_jack_driver);
    return ret;
}

static void __exit sec_jack_exit(void)
{
    regulator_put(sprd_hts_power.head_mic);
    regulator_put(sprd_hts_power.vcom_buf);
    regulator_put(sprd_hts_power.vbo);

    free_irq(info->irq_detect, info);
    free_irq(info->irq_button, info);

    platform_driver_unregister(&sec_jack_driver);
}

module_init(sec_jack_init);
module_exit(sec_jack_exit);

MODULE_DESCRIPTION("SEC Jack device driver for SC2723");
MODULE_AUTHOR("Youngki Park <ykp74@samsung.com>");
MODULE_LICENSE("GPL");
