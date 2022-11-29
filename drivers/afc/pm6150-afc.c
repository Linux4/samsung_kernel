/* pm6150-afc.c
 *
 * Copyright (C) 2019 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sec_class.h>
#include <linux/sec_param.h>

#include <linux/afc/pm6150-afc.h>
#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
#include "../battery_qc/include/sec_charging_common_qc.h"
#endif

static int afc_gpios = 0;

#if defined(CONFIG_SEC_A90Q_PROJECT)
//set L2 for AFC_DET GPIO_51 
#define GENI_CFG_SEQ_START 0x84 
#endif

struct geni_afc_dev *gafc;

#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
int send_afc_result(int res)
{
	static struct power_supply *psy_bat = NULL;
	union power_supply_propval value = {0, };
	int ret = 0;

	if (psy_bat == NULL) {
		psy_bat = power_supply_get_by_name("battery");
		if (!psy_bat) {
			pr_err("%s: Failed to Register psy_bat\n", __func__);
			return 0;
		}
	}

	value.intval = res;
	ret = power_supply_set_property(psy_bat,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_AFC_RESULT, &value);
	if (ret < 0)
		pr_err("%s: POWER_SUPPLY_EXT_PROP_AFC_RESULT set failed\n", __func__);

	return 0;
}

int get_vbus(void)
{
	static struct power_supply *psy_usb = NULL;
	union power_supply_propval value = {0, };
	int ret = 0, input_vol_mV = 0;

	if (psy_usb == NULL) {
		psy_usb = power_supply_get_by_name("usb");
		if (!psy_usb) {
			pr_err("%s: Failed to Register psy_usb\n", __func__);
			return -1;
		}
	}
	ret = power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
	if(ret < 0) {
		pr_err("%s: Fail to get usb Input Voltage %d\n",__func__,ret);
	} else {     
		input_vol_mV = value.intval/1000;    // Input Voltage in mV
		pr_info("%s: Input Voltage(%d) \n", __func__, input_vol_mV);                
		return input_vol_mV;
	}

	return ret;
}
#endif

void afc_reset(struct geni_afc_dev *gafc)
{
	u32 m_param = 0, m_cmd = 0;

	//0xA is predefined command for firmware to reset AFC
	m_param = 0; m_cmd = 0xA; 
	geni_setup_m_cmd(gafc->base, m_cmd, m_param);

	/* wait for bus to low… i.e. wait time as per AFC spec 100UI
	 * As per AFC doc , 1UI = 160us => 100UI = 16000us =  16 ms
	*/
	msleep(16);
}

int afc_transfer(struct geni_afc_dev *gafc, unsigned int data)
{
	unsigned int geni_ios = 0, k = 0;
	u32 m_param = 0, m_cmd = 0, se_geni_m_cmd0 = 0;

	// call to geni_se_select_mode(....,..) before transfer
	geni_se_select_mode(gafc->base, FIFO_MODE);

	//To DO : take proper locks to avoid concurrent transfer

	do {
		//check RX_DATA IN,bit 0 is RX_DATA_IN
		//wait till bit 0 is zero. both tx & rx are not active
		geni_ios = geni_read_reg(gafc->base, SE_GENI_IOS);
		printk("geni_ios : 0x%x");
 		usleep_range(98000, 100000);

		if(k++ > 10) {
			pr_err("%s: Time out... geni_ios error!\n", __func__);
			return -1;
		}
	} while(geni_ios & 0x01);

	//send opcode basically cmd
	m_param = 0; m_cmd = 0x7; //as per AFC HPG
	geni_setup_m_cmd(gafc->base, m_cmd, m_param);
	
	se_geni_m_cmd0 = geni_read_reg(gafc->base, SE_GENI_M_CMD0);
	pr_err("%s: SE_GENI_M_CMD0: 0x%x\n", __func__, se_geni_m_cmd0);

	//Fill the buffer with desired AFC data. this will be used in irq handler on watermark interrupt. As per AFC HPG it should be single byte data
	gafc->afc_tx_buf[0] = data ;
	//write the TX WATERMARK, after this IRQ will be raised
	geni_write_reg(1, gafc->base, SE_GENI_TX_WATERMARK_REG);

	return 0;
}

static int end_afc(void)
{
	int ret = 0;

	gafc->afc_cnt = 0;
	gpio_direction_output(afc_gpios, 0);

	ret = gpio_get_value(afc_gpios);
	pr_info("%s, afc_gpios: %d\n", __func__, ret);

	mutex_unlock(&gafc->afc_mutex);
	pr_info("%s, afc_mutex has been  unlocked.\n", __func__);

	/* disable clocks */
	ret = se_geni_clks_off(&gafc->afc_rsc);
	if (ret) {
			dev_err(gafc->dev, "Error during clocks on %d\n", ret);
			mutex_unlock(&gafc->afc_mutex);
			pr_info("%s, afc_mutex has been unlocked.\n", __func__);
			return -1;
	}

	return 0;
}

static void afc_workq(struct work_struct *work)
{
	int vbus = 0, ret = 0;
	int check_vbus_cnt = 0;

	pr_err("%s\n", __func__);

	if (gafc->afc_disable) {
		pr_err("%s, AFC has been disabled.\n", __func__);
		send_afc_result(AFC_FAIL);
		return ;
	}

	pr_err("%s, afc_cnt: %d\n", __func__, gafc->afc_cnt);

	if (gafc->afc_cnt < 3) {
		gafc->afc_cnt++;
		if (gafc->vol == SET_5V) 
			ret = afc_transfer(gafc, 0x07);	/* 5V, 1.8A */
		else
			ret = afc_transfer(gafc, 0x46);	/* 9V, 1.65A */

			if (ret < 0) {
				pr_err("%s, Fail, afc_transfer\n", __func__);
				send_afc_result(AFC_FAIL);
				end_afc();
			}
	} else {
		while (vbus < 7500 && check_vbus_cnt++ < 5){
			usleep_range(18000, 20000);
			vbus = get_vbus();
			pr_info("%s, cnt= %d,vbus= %d\n", __func__,check_vbus_cnt,vbus);
		}

		if(gafc->purpose == CHECK_AFC) {	//for is_afc
			if (gafc->afc_error == 0) {
				pr_info("%s, AFC TA\n", __func__);
				send_afc_result(AFC_5V);
			} else {
				pr_info("%s, Not AFC TA\n", __func__);
				gafc->afc_error = 0;
				send_afc_result(NOT_AFC);
			}
		} else if(gafc->purpose == SET_VOLTAGE) { /* SET_VOLTAGE */
			if (gafc->vol == SET_9V) {
				if (vbus >= 7500) {
					pr_info("%s, return AFC 9V\n", __func__);
					send_afc_result(AFC_9V);
				} else {
					pr_info("%s, Voltage hasn't been update as 9V, return AFC fail\n", __func__);
					gafc->afc_error = 0;
					send_afc_result(AFC_FAIL);
				}
			} else {	/* SET_5V */
				if (vbus <= 5500) {
					pr_info("%s, return AFC 5V\n", __func__);
					send_afc_result(AFC_5V);
				} else {
					pr_info("%s, Voltage hasn't been update as 5V, return AFC fail\n", __func__);
					gafc->afc_error = 0;
					send_afc_result(AFC_FAIL);
				}
			}
		} else {
			pr_info("%s, Undefined communcation\n", __func__);
		}

		end_afc();
	}

	return;
}

static void start_afc_work(struct work_struct *work)
{
	int ret = 0;

	pr_err("%s\n", __func__);

	mutex_lock(&gafc->afc_mutex);
	pr_info("%s, afc_mutex has been locked.\n", __func__);

	//TO DO: delay should be changed as checking d- level
 	usleep_range(1450000, 1500000);

	if (gafc->afc_cnt == 0)
	{
		gpio_direction_output(afc_gpios, 1);

		ret = gpio_get_value(afc_gpios);
		pr_info("%s, afc_gpios: %d\n", __func__, ret);

		/* enable clocks */
		ret = se_geni_clks_on(&gafc->afc_rsc);
		if (ret) {
			dev_err(gafc->dev, "Error during clocks on %d\n", ret);
			mutex_unlock(&gafc->afc_mutex);
			pr_info("%s, afc_mutex has been unlocked.\n", __func__);
			return;
		}
		
	}

	pr_err("%s: M_GP_IRQ_0_EN: 0x%x, M_GP_IRQ_1_EN: 0x%x, M_GP_IRQ_2_EN: 0x%x, M_GP_IRQ_3_EN: 0x%x\n", __func__, 
		M_GP_IRQ_0_EN, M_GP_IRQ_1_EN, M_GP_IRQ_2_EN, M_GP_IRQ_3_EN);
	pr_err("%s: M_RX_FIFO_WATERMARK_EN: 0x%x, M_RX_FIFO_LAST_EN: 0x%x, M_TX_FIFO_WATERMARK_EN: 0x%x, M_CMD_DONE_EN: 0x%x\n", __func__, 
		M_RX_FIFO_WATERMARK_EN, M_RX_FIFO_LAST_EN, M_TX_FIFO_WATERMARK_EN, M_CMD_DONE_EN);

	gafc->afc_cnt++;
	if (gafc->vol == SET_5V)
		ret = afc_transfer(gafc, 0x07);	/* 5V, 1.8A */
	else
		ret = afc_transfer(gafc, 0x46);	/* 9V, 1.65A */

	if (ret < 0) {
		pr_err("%s, Fail, afc_transfer\n", __func__);
		send_afc_result(AFC_FAIL);
		end_afc();
	}

	return;
}

static irqreturn_t geni_afc_irq_handler(int irq, void *dev)
{
	struct geni_afc_dev *gafc = dev;
	int j;
	u32 geni_cgc_ctrl = 0, se_geni_m_cmd0 = 0, geni_fw_revision_ro = 0, geni_clk_ctrl_ro = 0;
	u32 rxcnt = 0, temp = 0, se_geni_clk_sel = 0, geni_ser_m_clk_cfg = 0;
	u32 m_stat = readl_relaxed(gafc->base + SE_GENI_M_IRQ_STATUS);
	u32 rx_st = readl_relaxed(gafc->base + SE_GENI_RX_FIFO_STATUS);

	pr_info("%s\n", __func__);
	
	pr_err("%s: m_stat: 0x%x\n", __func__, m_stat);
	pr_err("%s: rx_st: 0x%x\n", __func__, rx_st);

	geni_cgc_ctrl = geni_read_reg(gafc->base, GENI_CGC_CTRL);
	se_geni_m_cmd0 = geni_read_reg(gafc->base, SE_GENI_M_CMD0);

	geni_fw_revision_ro = geni_read_reg(gafc->base, GENI_FW_REVISION_RO);
	geni_clk_ctrl_ro = geni_read_reg(gafc->base, GENI_CLK_CTRL_RO);

	pr_err("%s: GENI_CGC_CTRL: 0x%x, SE_GENI_M_CMD0: 0x%x, GENI_FW_REVISION_RO: 0x%x, GENI_CLK_CTRL_RO: 0x%x\n", __func__, 
		geni_cgc_ctrl, se_geni_m_cmd0, geni_fw_revision_ro, geni_clk_ctrl_ro);

	se_geni_clk_sel = geni_read_reg(gafc->base, SE_GENI_CLK_SEL); 
	geni_ser_m_clk_cfg = geni_read_reg(gafc->base, GENI_SER_M_CLK_CFG); 

	pr_err("%s: SE_GENI_CLK_SEL: 0x%x, GENI_SER_M_CLK_CFG: 0x%x\n", __func__, 
		se_geni_clk_sel, geni_ser_m_clk_cfg);

	// error processing
	if (m_stat & M_CMD_FAILURE_EN) {
		if (m_stat & M_GP_IRQ_0_EN) {
			gafc->afc_error++;
			pr_info("%s: Error AFC_NO_SLAVE_PING\n", __func__);
			goto irqret;
		}
		if (m_stat & M_GP_IRQ_1_EN) {
			gafc->afc_error++;
			pr_info("%s: Error AFC_NO_SLAVE_RESP_ON_WRITE\n", __func__);
			goto irqret;
		}
		if (m_stat & M_GP_IRQ_2_EN) {
			gafc->afc_error++;
			pr_info("%s: Error PARITY_READ\n", __func__);
			goto irqret;
		}
		if (m_stat & M_GP_IRQ_3_EN) {
			gafc->afc_error++;
			pr_info("%s: Error PROTOCOL\n", __func__);
			goto irqret;
		}
	}

	if ((m_stat & M_RX_FIFO_WATERMARK_EN) ||
		(m_stat & M_RX_FIFO_LAST_EN)) {
		rxcnt = rx_st & RX_FIFO_WC_MSK;
		printk("rxcnt : 0x%x", rxcnt);

		for (j = 0; j < rxcnt; j++) { 
			/*	read from FIFO, every RX transfer will be stored in RX-FIFO as a word of 10 bits: 
			*	8 bits for the RX transfer. one bit for parity error indication. one bit for STOP. 
			*	The last word in transfer will be marked with STOP bit = 1 and it is a non-valid word, 
			*/ 
			temp = readl_relaxed(gafc->base + SE_GENI_RX_FIFOn); 
			printk("temp value : 0x%x", temp);

			if (temp & RX_STOP_BIT) 
				break;

			gafc->afc_rx_buf[j] = ((temp >> 2) & 0xff); 
			printk("gafc->afc_rx_buf[j] : 0x%x", gafc->afc_rx_buf[j]); 
		} 
	} else if (m_stat & M_TX_FIFO_WATERMARK_EN) {
		
		printk("inside tx interrupt handling condition : 0x%x", gafc->afc_tx_buf[0]);
		//write to FIFO
		writel_relaxed(gafc->afc_tx_buf[0], gafc->base + SE_GENI_TX_FIFOn);
		
		// clear watermark bit
		writel_relaxed(0, (gafc->base + SE_GENI_TX_WATERMARK_REG));
	}

irqret:
	//clear IRQ
	if (m_stat) {
		writel_relaxed(m_stat, gafc->base + SE_GENI_M_IRQ_CLEAR);
	}

	if (m_stat & M_CMD_DONE_EN) {
		pr_info("%s, M_CMD_DONE_EN\n", __func__);
		schedule_work(&gafc->afc_work);
	}

	return IRQ_HANDLED;
}

int afc_set_voltage(int vol)
{
	pr_info("%s, vol: %d\n", __func__, vol);

	if (!gafc) {
		pr_err("%s, gafc is null, just return\n",  __func__);
		return -1;
	} else if (gafc->afc_fw != true) {
		pr_err("%s, just return, afc_fw(%d)\n",  __func__, gafc->afc_fw);
		return -1;
	}

	if (gafc->afc_disable) {
		pr_err("%s, AFC has been disabled.\n", __func__);
		send_afc_result(AFC_DISABLE);
		return -1;
	}

	gafc->purpose = SET_VOLTAGE;
	gafc->vol = vol;
	schedule_work(&gafc->start_afc_work);

	return 0;
}

int is_afc()
{
	pr_info("%s\n", __func__);

	if ((gafc->afc_fw != true) || (!gafc)) {
		pr_err("%s, just return, afc_fw(%d)\n",  __func__, gafc->afc_fw);
		return -1;
	}

	gafc->purpose = CHECK_AFC;
	gafc->vol = SET_5V;
	schedule_work(&gafc->start_afc_work);

	return 0;
}

/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */
static int afc_mode = 0;
static int __init set_afc_mode(char *str)
{
	get_option(&str, &afc_mode);
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);

	return 0;
}
early_param("afc_disable", set_afc_mode);

int get_afc_mode(void)
{
	pr_info("%s: afc_mode is 0x%02x\n", __func__, (afc_mode & 0x1));

	if (afc_mode & 0x1)
		return 1;
	else
		return 0;
}

static ssize_t afc_disable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	pr_info("%s\n", __func__);

	if (gafc->afc_disable)
		return sprintf(buf, "AFC is disabled\n");
	else
		return sprintf(buf, "AFC is enabled\n");
}

static ssize_t afc_disable_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
	static struct power_supply *psy_usb = NULL;
	union power_supply_propval value = {0, };
#endif
	int val = 0, ret = 0;

	if (!strncasecmp(buf, "1", 1))
		gafc->afc_disable = true;
	else if (!strncasecmp(buf, "0", 1))
		gafc->afc_disable = false;
	else {
		pr_warn("%s:%s invalid value\n", __func__);
		return count;
	}

	val = gafc->afc_disable ? '1' : '0';
	pr_info("%s: param_val:%d\n", __func__, val);

#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
	if (psy_usb == NULL) {
		psy_usb = power_supply_get_by_name("usb");
		if (!psy_usb) {
			pr_err("%s: Failed to Register psy_usb\n", __func__);
		}
	}
	value.intval = gafc->afc_disable ? HV_DISABLE : HV_ENABLE;
	ret = power_supply_set_property(psy_usb,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_HV_DISABLE, &value);
	if(ret < 0) {
		pr_err("%s: Fail to set voltage max limit%d\n", __func__, ret);
	} else {     
		pr_info("%s: voltage max limit set to (%d) \n", __func__, value.intval);                
	}
#endif
	
	ret = sec_set_param(param_index_afc_disable, &val); 
	if (ret == false) {
		pr_err("%s: set_param failed!!\n", __func__);
		return ret;
	} else
		pr_info("%s: afc_disable:%d (AFC %s)\n", __func__, 
			gafc->afc_disable, gafc->afc_disable ? "Disabled": "Enabled");

	return count;
}
static DEVICE_ATTR(afc_disable, 0664, afc_disable_show, afc_disable_store);

static struct attribute *afc_attributes[] = {
	&dev_attr_afc_disable.attr,
	NULL
};

static const struct attribute_group afc_group = {
	.attrs = afc_attributes,
};

static int afc_sysfs_init(void)
{
	static struct device *afc;
	int ret = 0;

	/* sysfs */
	afc = sec_device_create(0, NULL, "afc");
	if (IS_ERR(afc)) {
		pr_err("%s Failed to create device(afc)!\n", __func__);
		ret = -ENODEV;
	}

	ret = sysfs_create_group(&afc->kobj, &afc_group);
	if (ret) {
		pr_err("%s: afc sysfs fail, ret %d", __func__, ret);
	}

	return 0;
}

static int afc_pm6150_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct platform_device *wrapper_pdev;

	struct device_node *wrapper_ph_node;
	struct device_node *np= dev->of_node;

	int ret = 0, gafc_tx_depth = 0, afc_port_num = 0;
	unsigned long cfg0 = 0, cfg1 = 0; 

	pr_info("%s\n", __func__);

	/* sysfs */
	afc_sysfs_init();

	/* afc switch gpio */
	afc_gpios = of_get_named_gpio(np,"afc-gpios", 0);
	gpio_request(afc_gpios, "GPIO1");

	gafc = devm_kzalloc(&pdev->dev, sizeof(*gafc), GFP_KERNEL);
	if (!gafc)
		return -ENOMEM;

	gafc->dev = &pdev->dev;

	gafc->afc_disable = get_afc_mode();

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	wrapper_ph_node = of_parse_phandle(pdev->dev.of_node,
			"qcom,wrapper-core", 0);
	if (IS_ERR_OR_NULL(wrapper_ph_node)) {
		ret = PTR_ERR(wrapper_ph_node);
		dev_err(&pdev->dev, "No wrapper core defined\n");
		return ret;
	}

	wrapper_pdev = of_find_device_by_node(wrapper_ph_node);
	of_node_put(wrapper_ph_node);
	if (IS_ERR_OR_NULL(wrapper_pdev)) {
		ret = PTR_ERR(wrapper_pdev);
		dev_err(&pdev->dev, "Cannot retrieve wrapper device\n");
		return ret;
	}

	gafc->wrapper_dev = &wrapper_pdev->dev;
	gafc->afc_rsc.wrapper_dev = &wrapper_pdev->dev;

	ret = geni_se_resources_init(&gafc->afc_rsc, AFC_CORE2X_VOTE,
				     (DEFAULT_SE_CLK * DEFAULT_BUS_WIDTH));
	if (ret) {
		dev_err(gafc->dev, "geni_se_resources_init\n");
		return ret;
	}
	
	gafc->afc_rsc.ctrl_dev = gafc->dev; 
	gafc->afc_rsc.se_clk = devm_clk_get(&pdev->dev, "se-clk");
	if (IS_ERR(gafc->afc_rsc.se_clk)) {
		ret = PTR_ERR(gafc->afc_rsc.se_clk);
		dev_err(&pdev->dev, "Err getting SE Core clk %d\n", ret);
		return ret;
	}

	gafc->afc_rsc.m_ahb_clk = devm_clk_get(&pdev->dev, "m-ahb");
	if (IS_ERR(gafc->afc_rsc.m_ahb_clk)) {
		ret = PTR_ERR(gafc->afc_rsc.m_ahb_clk);
		dev_err(&pdev->dev, "Err getting M AHB clk %d\n", ret);
		return ret;
	}

	gafc->afc_rsc.s_ahb_clk = devm_clk_get(&pdev->dev, "s-ahb");
	if (IS_ERR(gafc->afc_rsc.s_ahb_clk)) {
		ret = PTR_ERR(gafc->afc_rsc.s_ahb_clk);
		dev_err(&pdev->dev, "Err getting S AHB clk %d\n", ret);
		return ret;
	}

	gafc->base = devm_ioremap_resource(gafc->dev, res);
	if (IS_ERR(gafc->base))
		return PTR_ERR(gafc->base);

/********Adding GPIO****************/
	gafc->afc_rsc.geni_pinctrl = devm_pinctrl_get(&pdev->dev);
        if (IS_ERR_OR_NULL(gafc->afc_rsc.geni_pinctrl)) {
                dev_err(&pdev->dev, "No pinctrl config specified\n");
                ret = PTR_ERR(gafc->afc_rsc.geni_pinctrl);
                return ret;
        }
    	
    	gafc->afc_rsc.geni_gpio_active =
                pinctrl_lookup_state(gafc->afc_rsc.geni_pinctrl,
                                                        PINCTRL_DEFAULT);
        if (IS_ERR_OR_NULL(gafc->afc_rsc.geni_gpio_active)) {
                dev_err(&pdev->dev, "No default config specified\n");
                ret = PTR_ERR(gafc->afc_rsc.geni_gpio_active);
                return ret;
        }
        gafc->afc_rsc.geni_gpio_sleep =
                pinctrl_lookup_state(gafc->afc_rsc.geni_pinctrl,
                                                        PINCTRL_SLEEP);
        if (IS_ERR_OR_NULL(gafc->afc_rsc.geni_gpio_sleep)) {
                dev_err(&pdev->dev, "No sleep config specified\n");
                ret = PTR_ERR(gafc->afc_rsc.geni_gpio_sleep);
                return ret;
        }
/***********************************/

	gafc->irq = platform_get_irq(pdev, 0);
	if (gafc->irq < 0) {
		dev_err(gafc->dev, "IRQ error for afc-geni\n");
		return gafc->irq;
	}

	platform_set_drvdata(pdev, gafc);
	
	//before registering irq line we need to do slave clock setting to 19.2Mhz and clock divider setting as defined in step 1 and 2 
	// Also we need to enable clocks before accessing register 

	/* enable clocks */
	ret = se_geni_clks_on(&gafc->afc_rsc);
	if (ret) {
		dev_err(gafc->dev, "Error during clocks on %d\n", ret);
		return ret;
	}

	afc_port_num = get_se_proto(gafc->base);
	if (afc_port_num == AFC_FW_PORT_NUMBER) {
		gafc->afc_fw = true;
		printk(KERN_ERR "Protocl Number: %d, AFC F/W is enabled\n", afc_port_num);
	} else {
		gafc->afc_fw = false;
		printk(KERN_ERR "Protocl Number: %d, AFC F/W is disabled\n", afc_port_num);
	}

	//Step 1 : slave clock setting to 19.2Mhz . We can also replace step 1 and step 2 with a function
	geni_write_reg(0, gafc->base, SE_GENI_CLK_SEL);
#if defined(CONFIG_SEC_A90Q_PROJECT)
	geni_write_reg(0x00000A10, gafc->base, GENI_CFG_REG80); 
	wmb(); 
	geni_write_reg(0x1, gafc->base, GENI_CFG_SEQ_START); 
	wmb(); 
#endif

	//Step 2 :Write clock divider factor 19 to: GENI_SER_M_CLK_CFG.CLK_DIV_VALUE. 
	geni_write_reg(((19<<4)|1), gafc->base, GENI_SER_M_CLK_CFG);

	ret = devm_request_irq(gafc->dev, gafc->irq, geni_afc_irq_handler,
			       IRQF_TRIGGER_HIGH, "geni_afc_irq", gafc);
	if (ret) {
		dev_err(gafc->dev, "Request_irq failed:%d: err:%d\n",
				   gafc->irq, ret);
		return ret;
	}

	//Now we also need to do fifo set up . This is one time set up . 
	//Can be done in probe or via a function call before transferring data first time.
	gafc_tx_depth = get_tx_fifo_depth(gafc->base);
	gafc->tx_wm = gafc_tx_depth - 1;
	geni_se_init(gafc->base, gafc->tx_wm, gafc_tx_depth);

	//bits per word for AFC is 10bit. and packing is 1x32
	se_get_packing_config(8, 1, 1, &cfg0, &cfg1); 
	geni_write_reg(cfg0, gafc->base, SE_GENI_TX_PACKING_CFG0); 
	geni_write_reg(cfg1, gafc->base, SE_GENI_TX_PACKING_CFG1); 

	se_get_packing_config(10, 1, 1, &cfg0, &cfg1); 
	geni_write_reg(cfg0, gafc->base, SE_GENI_RX_PACKING_CFG0); 
	geni_write_reg(cfg1, gafc->base, SE_GENI_RX_PACKING_CFG1); 
	geni_write_reg(0, gafc->base, SE_GENI_BYTE_GRAN); 

	/* disable clocks */
	ret = se_geni_clks_off(&gafc->afc_rsc);
	if (ret) {
		dev_err(gafc->dev, "Error during clocks off %d\n", ret);
		return ret;
	}

	mutex_init(&gafc->afc_mutex);
	
	INIT_WORK(&gafc->afc_work, afc_workq);
	INIT_WORK(&gafc->start_afc_work, start_afc_work);

	printk("end of probe");

	return 0;
}

static int afc_pm6150_remove(struct platform_device *pdev)
{
	//...
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id afc_pm6150_of_match_table[] = {
    {
        .compatible = "pm6150_afc",
    },
    { },
};
MODULE_DEVICE_TABLE(of, afc_pm6150_of_match_table);
#endif

static struct platform_driver afc_pm6150_driver = {
	.probe      = afc_pm6150_probe,
	.remove		= afc_pm6150_remove,
	.driver     = {
		.name   = "pm6150_afc",
#if defined(CONFIG_OF)
		.of_match_table = afc_pm6150_of_match_table,
#endif
	}
};

static int __init afc_pm6150_init(void)
{
	return platform_driver_register(&afc_pm6150_driver);
}
module_init(afc_pm6150_init);

static void __exit afc_pm6150_exit(void)
{
	platform_driver_unregister(&afc_pm6150_driver);
}
module_exit(afc_pm6150_exit);

MODULE_DESCRIPTION("AFC PM6150 driver");
