#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/param.h>
#include <linux/stat.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include "sprd_2701.h"


#define SPRD_2701_DEBUG
#ifdef SPRD_2701_DEBUG
#define SPRD_EX_DEBUG(format, arg...) printk("sprd 2701: " "@@@" format, ## arg)
#else
#define SPRD_EX_DEBUG(format, arg...)
#endif
#define sprd_2701_DEBUG_FS

static struct i2c_client *this_client;

static BLOCKING_NOTIFIER_HEAD(sprd_2701_chain_head);

int sprd_2701_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&sprd_2701_chain_head, nb);
}

int sprd_2701_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&sprd_2701_chain_head, nb);
}
static int sprd_2701_write_reg(int reg, u8 val)
{
	int ret;
	SPRD_EX_DEBUG("#### writereg reg = %d val = %d\n",reg,val);
	ret = i2c_smbus_write_byte_data(this_client, reg, val);

	if (ret < 0)
	{
		SPRD_EX_DEBUG("%s: error = %d", __func__, ret);
	}
    return ret;
}

int sprd_2701_read_reg(int reg, u8 *dest)
{
	int ret;

	ret = i2c_smbus_read_byte_data(this_client, reg);
	if (ret < 0) {
		SPRD_EX_DEBUG("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	//SPRD_EX_DEBUG("######sprd_2701readreg reg  = %d value =%d/%x\n",reg, ret, ret);
	return 0;
}

static void sprd_2701_set_value(BYTE reg, BYTE reg_bit,BYTE reg_shift, BYTE val)
{
	BYTE tmp;
	sprd_2701_read_reg(reg, &tmp);
	tmp = tmp & (~reg_bit) |(val << reg_shift);
	SPRD_EX_DEBUG("continue test tmp =0x%x,val=0x%x\n",tmp,val);
	sprd_2701_write_reg(reg,tmp);
}

static BYTE sprd_2701_get_value(BYTE reg, BYTE reg_bit, BYTE reg_shift)
{
	BYTE data = 0;
	int ret = 0 ;
	ret = sprd_2701_read_reg(reg, &data);
	ret = (data & reg_bit) >> reg_shift;

	return ret;
}

void  sprd_2701_reset_timer(void)
{
	SPRD_EX_DEBUG("sprd_2701 reset timer\n");
	int i =0;
	BYTE data = 0;
#if 0
	for(i = 0;  i < 11;  i++){
		sprd_2701_read_reg(i, &data);
		SPRD_EX_DEBUG("aaaaaaasprd_2701_ReadReg i = %d, data = %x\n",i,data);
	}
	//sprd_2701_set_value(POWER_ON_CTL, TMR_RST_BIT, TMR_RST_SHIFT, 1);
#endif
}
EXPORT_SYMBOL_GPL(sprd_2701_reset_timer);
void  sprd_2701_sw_reset(void)
{
	SPRD_EX_DEBUG("sprd_2701_sw_reset\n");
	sprd_2701_set_value(POWER_ON_CTL, SW_RESET_BIT, SW_RESET_SHIFT, 1);

}
EXPORT_SYMBOL_GPL(sprd_2701_sw_reset);

void sprd_2701_set_vindpm(BYTE reg_val)
{
	sprd_2701_set_value(INPUT_SRC_CTL, VIN_DPM_BIT, VIN_DPM_SHIFT, reg_val);
}
EXPORT_SYMBOL_GPL(sprd_2701_set_vindpm);

void sprd_2701_termina_cur_set(BYTE reg_val)
{
	sprd_2701_set_value(TRICK_CHG_CTL, TERMINA_CUR_BIT, TERMINA_CUR_SHIFT, reg_val);
}
EXPORT_SYMBOL_GPL(sprd_2701_termina_cur_set);

void sprd_2701_termina_vol_set(BYTE reg_val)
{
	sprd_2701_set_value(CHG_VOL_CTL, CHG_VOL_LIMIT_BIT, CHG_VOL_LIMIT_SHIFT, reg_val);
}
EXPORT_SYMBOL_GPL(sprd_2701_termina_vol_set);

void sprd_2701_termina_time_set(BYTE reg_val)
{
	sprd_2701_set_value(TIMER_CTL, CHG_SAFE_TIMER_BIT, CHG_SAFE_TIMER_SHIFT, reg_val);
}
EXPORT_SYMBOL_GPL(sprd_2701_termina_time_set);
void sprd_2701_init(void)
{
	BYTE data = 0;
	int i = 0;
	SPRD_EX_DEBUG("sprd_2701_init \n");
	sprd_2701_sw_reset();
	for(i = 0;  i < 11;  i++){
		sprd_2701_read_reg(i, &data);
		SPRD_EX_DEBUG("sprd_2701_ReadReg i = %d, data = %x\n",i,data);
	}

	sprd_2701_set_value(TIMER_CTL,WATCH_DOG_TIMER_BIT,WATCH_DOG_TIMER_SHIFT,0); // disable whatchdog 
	//sprd_2701_set_value(MISC_CTL,JEITA_ENABLE_BIT,JEITA_ENABLE_SHIFT,1);//enable jeita
	sprd_2701_set_value(TIMER_CTL,CHG_TERMINA_EN_BIT,CHG_TERMINA_EN_SHIFT,0); //disable chg termina
	sprd_2701_set_value(TIMER_CTL,CHG_SAFE_TIMER_EN_BIT,CHG_SAFE_TIMER_EN_SHIFT,0); //disable safety timer
	
}
EXPORT_SYMBOL_GPL(sprd_2701_init);
void sprd_2701_otg_enable(int enable)
{
	SPRD_EX_DEBUG("sprd_2701_OTG_Enable enable =%d\n",enable);
	if(enable){
		sprd_2701_set_value(POWER_ON_CTL, CHG_EN_BIT, CHG_EN_SHIFT, CHG_OTG_VAL);
	}else{
		sprd_2701_set_value(POWER_ON_CTL, CHG_EN_BIT, CHG_EN_SHIFT, CHG_DISABLE_VAL);
	}
}
EXPORT_SYMBOL_GPL(sprd_2701_otg_enable);
void sprd_2701_enable_flash(int enable)
{
	SPRD_EX_DEBUG("sprd_2701_enable_flash enable =%d\n",enable);

	if(enable){
		sprd_2701_set_value(LED_DRV_CTL, LED_MODE_CTL_BIT, LED_MODE_CTL_SHIFT, LED_FLASH_EN_VAL);
	}else{
		sprd_2701_set_value(LED_DRV_CTL, LED_MODE_CTL_BIT, LED_MODE_CTL_SHIFT, 0);
	}
}

EXPORT_SYMBOL_GPL(sprd_2701_enable_flash);
void sprd_2701_set_flash_brightness(BYTE  brightness)
{
	SPRD_EX_DEBUG("sprd_2701_set_flash_brightness brightness =%d\n",brightness);

	if(brightness > 0xf ){
		brightness = 0xf;
	}
	sprd_2701_set_value(LED_CUR_REF_CTL, FLASH_MODE_CUR_BIT, FLASH_MODE_CUR_SHIFT, brightness);
}

EXPORT_SYMBOL_GPL(sprd_2701_set_flash_brightness);

void sprd_2701_enable_torch(int enable)
{
	SPRD_EX_DEBUG("sprd_2701_enable_torch enable =%d\n",enable);

	if(enable){
		sprd_2701_set_value(LED_DRV_CTL, LED_MODE_CTL_BIT, LED_MODE_CTL_SHIFT, LED_TORCH_EN_VAL);
	}else{
		sprd_2701_set_value(LED_DRV_CTL, LED_MODE_CTL_BIT, LED_MODE_CTL_SHIFT, 0);
	}
}

EXPORT_SYMBOL_GPL(sprd_2701_enable_torch);
void sprd_2701_set_torch_brightness(BYTE brightness)
{
	SPRD_EX_DEBUG("sprd_2701_set_torch_brightness brightness =%d\n",brightness);

	if(brightness >  9){
		brightness = 0x9;
	}
	sprd_2701_set_value(LED_CUR_REF_CTL, TORCH_MODE_CUR_BIT, TORCH_MODE_CUR_SHIFT, brightness);

}

EXPORT_SYMBOL_GPL(sprd_2701_set_torch_brightness);
void sprd_2701_stop_charging(void)
{
	struct sprd_2701_platform_data *pdata = i2c_get_clientdata(this_client);
	int vbat_detect = gpio_get_value(pdata->vbat_detect);
	SPRD_EX_DEBUG("pdata->vbat_detect=%d,vbat_detect=%d\n",pdata->vbat_detect,vbat_detect);
	if(vbat_detect){
		sprd_2701_set_value(POWER_ON_CTL, CHG_EN_BIT , CHG_EN_SHIFT, CHG_DISABLE_VAL);
	}else{
		sprd_2701_set_value(POWER_ON_CTL, CHG_EN_BIT , CHG_EN_SHIFT, 3);
	}
}
EXPORT_SYMBOL_GPL(sprd_2701_stop_charging);

BYTE sprd_2701_get_chg_config(void)
{
	BYTE data = 0;
	data = sprd_2701_get_value(POWER_ON_CTL, CHG_EN_BIT, CHG_EN_SHIFT);
	return data ;
}

EXPORT_SYMBOL_GPL(sprd_2701_get_chg_config);
BYTE sprd_2701_get_sys_status(void)
{
	BYTE data = 0;
	sprd_2701_read_reg(SYS_STATUS_REG, &data);
	return data ;
}

EXPORT_SYMBOL_GPL(sprd_2701_get_sys_status);

BYTE sprd_2701_get_fault_val(void)
{
	BYTE data = 0;
	sprd_2701_read_reg(FAULT_REG, &data);
	return data ;
}

EXPORT_SYMBOL_GPL(sprd_2701_get_fault_val);

 void sprd_2701_enable_chg()
{
	sprd_2701_set_value(POWER_ON_CTL, CHG_EN_BIT , CHG_EN_SHIFT , CHG_BAT_VAL);
}
EXPORT_SYMBOL_GPL(sprd_2701_enable_chg);
 void sprd_2701_set_chg_current(BYTE reg_val)
{
 	if(reg_val == 0){
              sprd_2701_set_value(INPUT_SRC_CTL,IN_CUR_LIMIT_BIT ,IN_CUR_LIMIT_SHIFT ,1 );
      }	
	sprd_2701_set_value(CHG_CUR_CTL,CHG_CUR_BIT ,CHG_CUR_SHIFT , reg_val);
}
EXPORT_SYMBOL_GPL(sprd_2701_set_chg_current);
#ifdef  sprd_2701_DEBUG_FS

static ssize_t set_regs_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long set_value;
	int reg;  // bit 16:8
	int val;  //bit7:0
	int ret;
	set_value = simple_strtoul(buf, NULL, 16);

	reg = (set_value & 0xff00) >> 8;
	val = set_value & 0xff;
	printk("sprd_2701 set reg = %d value = %d\n",reg, val);
	ret = sprd_2701_write_reg(reg, val);

	if (ret < 0){
		printk("set_regs_store error\n");
		return -EINVAL;
		}

	return count;
}


static ssize_t dump_regs_show(struct device *dev, struct device_attribute *attr,
                char *buf)
{
	const int regaddrs[] = {0x00, 0x01, 0x02, 0x03, 0x4, 0x05, 0x06, 0x07,0x08,0x09,0x0a };
	const char str[] = "0123456789abcdef";
	BYTE sprd_2701_regs[0x60];

	int i = 0, index;
	char val = 0;
	for (i=0; i<0x60; i++) {
		if ((i%3)==2)
			buf[i]=' ';
		else
			buf[i] = 'x';
	}
	buf[0x5d] = '\n';
	buf[0x5e] = 0;
	buf[0x5f] = 0;

	for ( i = 0; i < 11; i++) {
		 sprd_2701_read_reg(i,&sprd_2701_regs[i]);
	}

	for (i=0; i<ARRAY_SIZE(regaddrs); i++) {
		index = regaddrs[i];
		val = sprd_2701_regs[index];
		buf[3*index] = str[(val&0xf0)>>4];
		buf[3*index+1] = str[val&0x0f];
		buf[3*index+1] = str[val&0x0f];
	}

	return 0x60;
}


static DEVICE_ATTR(dump_regs, S_IRUGO , dump_regs_show, NULL);
static DEVICE_ATTR(set_regs,  S_IWUSR, NULL, set_regs_store);
#endif

static __used irqreturn_t sprd_2701_fault_handler(int irq, void *dev_id)
{
	struct sprd_2701 *p_sprd_2701 = (struct sprd_2701 *) dev_id;
	struct sprd_2701_platform_data *pdata = i2c_get_clientdata(p_sprd_2701->client);
	int value = 0;
	SPRD_EX_DEBUG("enter sprdbat_vbat_chg_fault\n");
	value = gpio_get_value(pdata->irq_gpio_number);

	SPRD_EX_DEBUG("gpio_number = %d,value=%d\n",pdata->irq_gpio_number,value);
	if(value){
		SPRD_EX_DEBUG("gpio is high charge ic ok\n");
		irq_set_irq_type(p_sprd_2701->client->irq, IRQ_TYPE_LEVEL_LOW);
	}else{
		SPRD_EX_DEBUG("gpio is low some fault\n");
		irq_set_irq_type(p_sprd_2701->client->irq, IRQ_TYPE_LEVEL_HIGH);
	}
	schedule_work(&p_sprd_2701->chg_fault_irq_work);
	return IRQ_HANDLED;
}
static void sprdbat_chg_fault_irq_works(struct work_struct *work)
{
	SPRD_EX_DEBUG("enter sprdbat_chg_fault_irq_works \n");
	blocking_notifier_call_chain(&sprd_2701_chain_head, 0 , NULL);
}
static int sprd_2701_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{

	int rc = 0;
	int err = -1;
	int value;
	struct sprd_2701 *sprd_2701_data = NULL;
	struct sprd_2701_platform_data *pdata = client->dev.platform_data;
	SPRD_EX_DEBUG("@@@@@@@sprd_2701_probe\n");
#ifdef CONFIG_OF
		struct device_node *np = client->dev.of_node;
		if (np && !pdata){
			pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
			if(!pdata){
				err = -ENOMEM;
				goto exit_alloc_platform_data_failed;
			}
			pdata->irq_gpio_number = of_get_gpio(np, 0);
			SPRD_EX_DEBUG("pdata->irq_gpio_number=%d\n ",pdata->irq_gpio_number );
			if(pdata->irq_gpio_number < 0){
				SPRD_EX_DEBUG("fail to get irq_gpio_number\n");
				goto exit_get_gpio_failed;
			}
			pdata->vbat_detect = of_get_named_gpio(np,"vbat_gpio",0);
			SPRD_EX_DEBUG("pdata->vbat_detect =%d\n",pdata->vbat_detect);
			if(pdata->vbat_detect < 0){
				SPRD_EX_DEBUG("fail to get vbat_detect\n");
				goto exit_get_gpio_failed;
			}
		}
#else
	if (pdata == NULL) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL) {
			rc = -ENOMEM;
			pr_err("%s: platform data is NULL\n", __func__);
			goto err_alloc_data_failed;
		}
	}
#endif
	rc = gpio_request(pdata->irq_gpio_number, "sprd_2701_gpio");
	if(rc) {
		SPRD_EX_DEBUG("gpio_request failed!\n");
		goto exit_gpio_request_failed;
	}
	gpio_direction_input(pdata->irq_gpio_number);
	client->irq = gpio_to_irq(pdata->irq_gpio_number);
	value = gpio_get_value(pdata->irq_gpio_number);
	SPRD_EX_DEBUG("value =%d\n",value);
	sprd_2701_data = kzalloc(sizeof(struct sprd_2701), GFP_KERNEL);
	if (!sprd_2701_data) {
		SPRD_EX_DEBUG("kzalloc failed!\n");
		rc = -ENOMEM;
		goto err_alloc_data_failed;
	}
	pdata->sprd_2701_ops = sprd_get_ext_ic_ops();
	sprdbat_register_ext_ops(pdata->sprd_2701_ops);
	i2c_set_clientdata(client, pdata);
	sprd_2701_data->client = client;
	this_client = client;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: i2c check functionality error\n", __func__);
		rc = -ENODEV;
		goto check_funcionality_failed;
	}
	INIT_WORK(&sprd_2701_data->chg_fault_irq_work, sprdbat_chg_fault_irq_works);
	if (client->irq > 0) {
		rc = request_irq(client->irq, sprd_2701_fault_handler,
				IRQ_TYPE_LEVEL_LOW | IRQF_NO_SUSPEND,
				"sprd_2701_chg_fault", sprd_2701_data);
		if (rc < 0) {
			SPRD_EX_DEBUG("request_irq failed!\n");
			goto exit_request_irq_failed;
		}
	}
#ifdef sprd_2701_DEBUG_FS
	device_create_file(&client->dev, &dev_attr_dump_regs);
	device_create_file(&client->dev, &dev_attr_set_regs);
#endif
	SPRD_EX_DEBUG("@@@@@@@sprd_2701_probe ok\n");
	return rc;

exit_request_irq_failed:
check_funcionality_failed:
	kfree(sprd_2701_data);
err_alloc_data_failed:
	gpio_free(pdata->irq_gpio_number);
#ifdef CONFIG_OF
exit_get_gpio_failed:
	kfree(pdata);
exit_gpio_request_failed:
exit_alloc_platform_data_failed:
	SPRD_EX_DEBUG("@@@@@@@sprd_2701_probe fail\n");
	return err;
#endif
}
static int sprd_2701_remove(struct i2c_client *client)
{
	struct sprd_2701_platform_data *pdata = i2c_get_clientdata(client);
	struct sprd_2701 *sprd_2701_data = container_of(client, struct sprd_2701, client);
	SPRD_EX_DEBUG("@@@@@@@sprd_2701_remove \n");
	free_irq(client->irq, sprd_2701_data);
	gpio_free(pdata->irq_gpio_number);
	kfree(pdata);
	kfree(sprd_2701_data);
	return 0;
}

static int  sprd_2701_suspend(struct i2c_client *client, pm_message_t message)
{
	return 0;
}

static int sprd_2701_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id sprd_2701_i2c_id[] = {
	{ "2701_chg", 0 },
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id sprd_2701_of_match[] = {
	{.compatible = "sprd, 2701_chg",},
	{}
};
#endif

static struct i2c_driver sprd_2701_i2c_driver = {
	.driver = {
		.name = "sprd_2701_chg",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(sprd_2701_of_match),
#endif
	},
	.probe    = sprd_2701_probe,
	.remove   = sprd_2701_remove,
	.suspend  = sprd_2701_suspend,
	.resume = sprd_2701_resume,
	.id_table = sprd_2701_i2c_id,
};

static __init int sprd_2701_i2c_init(void)
{
	return i2c_add_driver(&sprd_2701_i2c_driver);
}

static __exit sprd_2701_i2c_exit(void)
{
	i2c_del_driver(&sprd_2701_i2c_driver);
}

subsys_initcall_sync(sprd_2701_i2c_init);
module_exit(sprd_2701_i2c_exit);
