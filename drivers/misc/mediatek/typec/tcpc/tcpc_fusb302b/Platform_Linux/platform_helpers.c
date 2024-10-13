#include <linux/kernel.h>
#include <linux/stat.h>															// File permission masks
#include <linux/types.h>														// Kernel datatypes
#include <linux/i2c.h>															// I2C access, mutex
#include <linux/errno.h>														// Linux kernel error definitions
#include <linux/hrtimer.h>														// hrtimer
#include <linux/workqueue.h>													// work_struct, delayed_work
#include <linux/delay.h>														// udelay, usleep_range, msleep
#include <linux/pm_wakeup.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/extcon.h>
#include <linux/usb/role.h>
#include <linux/extcon-provider.h>
#include "class-dual-role.h"
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/17 start */
#include <mt-plat/v1/charger_type.h>

#include "fusb30x_global.h"		// Chip structure access
#include "../core/core.h"				// Core access
#include "platform_helpers.h"

#include "../core/modules/HostComm.h"
#include "../core/Log.h"
#include "../core/Port.h"
#include "../core/PD_Types.h"			// State Log states
#include "../core/TypeC_Types.h"		// State Log states
#include "sysfs_header.h"
#include "../core/modules/observer.h"
#include "../core/platform.h"
#include <linux/version.h>
#include "../core/TypeC.h"

//#include "../../../../power/supply/qcom/pogo.h"

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************		GPIO Interface			******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
const char* FUSB_DT_INTERRUPT_INTN =	"fsc_interrupt_int_n";		// Name of the INT_N interrupt in the Device Tree
#define FUSB_DT_GPIO_INTN				"fairchild,int_b"			// Name of the Int_N GPIO pin in the Device Tree
#define FUSB_DT_GPIO_INTS				"fairchild_enable_int"		// Name of the pinctrl state
#define FUSB_DT_GPIO_VBUS_5V			"fairchild,vbus"			// Name of the VBus 5V GPIO pin in the Device Tree
#define FUSB_DT_GPIO_VBUS_OTHER			"fairchild,vbusOther"		// Name of the VBus Other GPIO pin in the Device Tree
#define FUSB_DT_GPIO_DISCHARGE			"fairchild,discharge"		// Name of the Discharge GPIO pin in the Device Tree

#ifdef FSC_DEBUG
#define FUSB_DT_GPIO_DEBUG_SM_TOGGLE	"fairchild,dbg_sm"			// Name of the debug State Machine toggle GPIO pin in the Device Tree
#endif  // FSC_DEBUG

/* Internal forward declarations */
static irqreturn_t _fusb_isr_intn(int irq, void *dev_id);
static void work_function(struct work_struct *work);
static enum hrtimer_restart fusb_sm_timer_callback(struct hrtimer *timer);
static void fusb_register_port(void);

/* add by yangdi start */
static u8 g_pd_flag = 0;
static int g_cc_polarity = 0;
/* add by yangdi end */


#if LINUX_VERSION_CODE > KERNEL_VERSION(4,14,0)
static void wakeup_source_init(struct wakeup_source *ws,
										const char *name)
{
	if (ws) {
			memset(ws, 0, sizeof(*ws));
			ws->name = name;
	}

		wakeup_source_add(ws);
}

static void wakeup_source_trash(struct wakeup_source *ws)
{
		wakeup_source_remove(ws);
		__pm_relax(ws);
}
#endif

/* add by yangdi start */
bool fusb_get_pd_flag(void)
{
	return g_pd_flag?1:0;
}
EXPORT_SYMBOL(fusb_get_pd_flag);

int fusb_typec_online_flag(void)
{
	return g_cc_polarity;
}
EXPORT_SYMBOL(fusb_typec_online_flag);
/* add by yangdi end */

FSC_S32 fusb_InitializeGPIO(void)
{
	FSC_S32 ret = 0;
	struct device_node* node;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return -ENOMEM;
	}
	/* Get our device tree node */
	node = chip->client->dev.of_node;

	/* Get our GPIO pins from the device tree, allocate them, and then set their direction (input/output) */
	chip->gpio_IntN = of_get_named_gpio(node, FUSB_DT_GPIO_INTN, 0);
	if (!gpio_is_valid(chip->gpio_IntN))
	{
		dev_err(&chip->client->dev, "%s - Error: Could not get named GPIO for Int_N! Error code: %d\n", __func__, chip->gpio_IntN);
		return chip->gpio_IntN;
	}

	// Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
	ret = gpio_request(chip->gpio_IntN, FUSB_DT_GPIO_INTN);
	if (ret < 0)
	{
		dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for Int_N! Error code: %d\n", __func__, ret);
		return ret;
	}

	ret = gpio_direction_input(chip->gpio_IntN);
	if (ret < 0)
	{
		dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to input for Int_N! Error code: %d\n", __func__, ret);
		return ret;
	}

#ifdef FSC_DEBUG
	/* Export to sysfs */
	gpio_export(chip->gpio_IntN, false);
	gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_INTN, chip->gpio_IntN);
#endif // FSC_DEBUG

#if 0 //#ifdef CONFIG_PINCTRL
	chip->pinctrl_int = devm_pinctrl_get(&chip->client->dev);
	if (IS_ERR(chip->pinctrl_int)) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not find/create pinctrl for Int_N!\n",
			__func__);
		return -EINVAL;
	}
	chip->pinctrl_state_int = pinctrl_lookup_state(chip->pinctrl_int,
		FUSB_DT_GPIO_INTS);
	if (IS_ERR(chip->pinctrl_state_int)) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not find  pinctrl enable state for Int_N!\n",
			__func__);
		return -EINVAL;
	}
	ret = pinctrl_select_state(chip->pinctrl_int, chip->pinctrl_state_int);
	if (ret) {
		dev_err(&chip->client->dev,
			"%s - Error: Could not set enable state for Int_N! Error code : %d\n",
			__func__, ret);
		return ret;
	}
#endif

	pr_info("FUSB  %s - INT_N GPIO initialized as pin '%d'\n", __func__, chip->gpio_IntN);

	//modfied by zjb
	// // VBus 5V
	// chip->gpio_VBus5V = of_get_named_gpio(node, FUSB_DT_GPIO_VBUS_5V, 0);
	// if (!gpio_is_valid(chip->gpio_VBus5V))
	// {
	//		dev_err(&chip->client->dev, "%s - Error: Could not get named GPIO for VBus5V! Error code: %d\n", __func__, chip->gpio_VBus5V);
	//		fusb_GPIO_Cleanup();
	//		return chip->gpio_VBus5V;
	// }

	// // Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
	// ret = gpio_request(chip->gpio_VBus5V, FUSB_DT_GPIO_VBUS_5V);
	// if (ret < 0)
	// {
	//		dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for VBus5V! Error code: %d\n", __func__, ret);
	//		return ret;
	// }

	// ret = gpio_direction_output(chip->gpio_VBus5V, 0);
	// if (ret < 0)
	// {
	//		dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to output for VBus5V! Error code: %d\n", __func__, ret);
	//		fusb_GPIO_Cleanup();
	//		return ret;
	// }
	//
	// #ifdef FSC_DEBUG
	//		// Export to sysfs
	//		gpio_export(chip->gpio_VBus5V, false);
	//		gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_VBUS_5V, chip->gpio_VBus5V);
	// #endif // FSC_DEBUG
	//
	//pr_info("FUSB  %s - VBus 5V initialized as pin '%d' and is set to '%d'\n", __func__, chip->gpio_VBus5V, chip->gpio_VBus5V_value ? 1 : 0);

#ifdef FSC_DEBUG
	// State Machine Debug Notification
	// Optional GPIO - toggles each time the state machine is called
	chip->dbg_gpio_StateMachine = of_get_named_gpio(node, FUSB_DT_GPIO_DEBUG_SM_TOGGLE, 0);
	if (!gpio_is_valid(chip->dbg_gpio_StateMachine))
	{
		// Soft fail - provide a warning, but don't quit because we don't really need this VBus if only using VBus5v
		pr_info("%s - Warning: Could not get GPIO for Debug GPIO! Error code: %d\n", __func__, chip->dbg_gpio_StateMachine);
	}
	else
	{
		// Request our GPIO to reserve it in the system - this should help ensure we have exclusive access (not guaranteed)
		ret = gpio_request(chip->dbg_gpio_StateMachine, FUSB_DT_GPIO_DEBUG_SM_TOGGLE);
		if (ret < 0)
		{
			dev_err(&chip->client->dev, "%s - Error: Could not request GPIO for Debug GPIO! Error code: %d\n", __func__, ret);
			return ret;
		}

		ret = gpio_direction_output(chip->dbg_gpio_StateMachine, chip->dbg_gpio_StateMachine_value);
		if (ret != 0)
		{
			dev_err(&chip->client->dev, "%s - Error: Could not set GPIO direction to output for Debug GPIO! Error code: %d\n", __func__, ret);
			return ret;
		}
		else
		{
			pr_info("FUSB  %s - Debug GPIO initialized as pin '%d' and is set to '%d'\n", __func__, chip->dbg_gpio_StateMachine, chip->dbg_gpio_StateMachine_value ? 1 : 0);

		}

		// Export to sysfs
		gpio_export(chip->dbg_gpio_StateMachine, true); // Allow direction to change to provide max debug flexibility
		gpio_export_link(&chip->client->dev, FUSB_DT_GPIO_DEBUG_SM_TOGGLE, chip->dbg_gpio_StateMachine);
	}
#endif  // FSC_DEBUG

	/* add by yangdi start */
	ret = of_property_read_u8(node, "fusb,usb-extern-pd", &g_pd_flag);
	pr_err("[%s]fusb,usb-extern-pd:[%d] [%d] \n", __func__, g_pd_flag, ret);
	/* add by yangdi end */

	return 0;   // Success!
}

void fusb_GPIO_Set_VBus5v(FSC_BOOL set)
{
#if 0
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
	}

	// GPIO must be valid by this point
	if (gpio_cansleep(chip->gpio_VBus5V))
	{
		/*
			* If your system routes GPIO calls through a queue of some kind, then
			* it may need to be able to sleep. If so, this call must be used.
			*/
		gpio_set_value_cansleep(chip->gpio_VBus5V, set ? 1 : 0);
	}
	else
	{
		gpio_set_value(chip->gpio_VBus5V, set ? 1 : 0);
	}
	chip->gpio_VBus5V_value = set;

	pr_debug("FUSB  %s - VBus 5V set to: %d\n", __func__, chip->gpio_VBus5V_value ? 1 : 0);
#endif
}

void fusb_GPIO_Set_VBusOther(FSC_BOOL set)
{
#if 0
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
	}

	// Only try to set if feature is enabled, otherwise just fake it
	if (gpio_is_valid(chip->gpio_VBusOther))
	{
		/*
		* If your system routes GPIO calls through a queue of some kind, then
		* it may need to be able to sleep. If so, this call must be used.
		*/
		if (gpio_cansleep(chip->gpio_VBusOther))
		{
			gpio_set_value_cansleep(chip->gpio_VBusOther, set ? 1 : 0);
		}
		else
		{
			gpio_set_value(chip->gpio_VBusOther, set ? 1 : 0);
		}
	}
	chip->gpio_VBusOther_value = set;
#endif
}

FSC_BOOL fusb_GPIO_Get_VBus5v(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return false;
	}

	if (!gpio_is_valid(chip->gpio_VBus5V))
	{
		pr_debug("FUSB  %s - Error: VBus 5V pin invalid! Pin value: %d\n", __func__, chip->gpio_VBus5V);
	}

	return chip->gpio_VBus5V_value;
}

FSC_BOOL fusb_GPIO_Get_VBusOther(void)
{
#if 0
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return false;
	}

	return chip->gpio_VBusOther_value;
#endif
	return true;
}

void fusb_GPIO_Set_Discharge(FSC_BOOL set)
{
#if 0
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
	}

	// GPIO must be valid by this point
	if (gpio_cansleep(chip->gpio_Discharge))
	{
		/*
			* If your system routes GPIO calls through a queue of some kind, then
			* it may need to be able to sleep. If so, this call must be used.
			*/
		gpio_set_value_cansleep(chip->gpio_Discharge, set ? 1 : 0);
	}
	else
	{
		gpio_set_value(chip->gpio_Discharge, set ? 1 : 0);
	}
	chip->gpio_Discharge_value = set;

	pr_debug("FUSB  %s - Discharge set to: %d\n", __func__, chip->gpio_Discharge_value ? 1 : 0);
#endif
}

FSC_BOOL fusb_GPIO_Get_IntN(void)
{
	FSC_S32 ret = 0;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return false;
	}
	else
	{
		/*
		* If your system routes GPIO calls through a queue of some kind, then
		* it may need to be able to sleep. If so, this call must be used.
		*/
		if (gpio_cansleep(chip->gpio_IntN))
		{
			ret = !gpio_get_value_cansleep(chip->gpio_IntN);
		}
		else
		{
			ret = !gpio_get_value(chip->gpio_IntN); // Int_N is active low
		}
		return (ret != 0);
	}
}

#ifdef FSC_DEBUG
void dbg_fusb_GPIO_Set_SM_Toggle(FSC_BOOL set)
{
#if 0
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
	}

	if (gpio_is_valid(chip->dbg_gpio_StateMachine))
	{
		/*
		* If your system routes GPIO calls through a queue of some kind, then
		* it may need to be able to sleep. If so, this call must be used.
		*/
		if (gpio_cansleep(chip->dbg_gpio_StateMachine))
		{
			gpio_set_value_cansleep(chip->dbg_gpio_StateMachine, set ? 1 : 0);
		}
		else
		{
			gpio_set_value(chip->dbg_gpio_StateMachine, set ? 1 : 0);
		}
		chip->dbg_gpio_StateMachine_value = set;
	}
#endif
}

FSC_BOOL dbg_fusb_GPIO_Get_SM_Toggle(void)
{
#if 0
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return false;
	}
	return chip->dbg_gpio_StateMachine_value;
#endif
	return true;
}
#endif  // FSC_DEBUG

void fusb_GPIO_Cleanup(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

	if (gpio_is_valid(chip->gpio_IntN) && chip->gpio_IntN_irq != -1)	// -1 indicates that we don't have an IRQ to free
	{
		devm_free_irq(&chip->client->dev, chip->gpio_IntN_irq, chip);
	}
	wakeup_source_trash(&chip->fusb302_wakelock);

	if (gpio_is_valid(chip->gpio_IntN))
	{
#ifdef FSC_DEBUG
		gpio_unexport(chip->gpio_IntN);
#endif // FSC_DEBUG

		gpio_free(chip->gpio_IntN);
	}

	if (gpio_is_valid(chip->gpio_VBus5V))
	{
#ifdef FSC_DEBUG
		gpio_unexport(chip->gpio_VBus5V);
#endif // FSC_DEBUG

		gpio_free(chip->gpio_VBus5V);
	}

	if (gpio_is_valid(chip->gpio_VBusOther))
	{
		gpio_free(chip->gpio_VBusOther);
	}

#ifdef FSC_DEBUG
	if (gpio_is_valid(chip->dbg_gpio_StateMachine))
	{
		gpio_unexport(chip->dbg_gpio_StateMachine);
		gpio_free(chip->dbg_gpio_StateMachine);
	}
#endif  // FSC_DEBUG
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************			I2C Interface			******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
FSC_BOOL fusb_I2C_WriteData(FSC_U8 address, FSC_U8 length, FSC_U8* data)
{
	FSC_S32 i = 0;
	FSC_S32 ret = 0;

	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL)
	{
		pr_err("%s - Error: %s is NULL!\n", __func__,
			(chip == NULL ? "Internal chip structure"
				: (chip->client == NULL ? "I2C Client"
					: "Write data buffer")));
		return FALSE;
	}

	mutex_lock(&chip->lock);

	// Retry on failure up to the retry limit
	for (i = 0; i <= chip->numRetriesI2C; i++)
	{
		ret = i2c_smbus_write_i2c_block_data(chip->client, address,
			length, data);

		if (ret < 0)
		{
			// Errors report as negative
			dev_err(&chip->client->dev,
				"%s - I2C error block writing byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n", __func__,
				address, ret, i, chip->numRetriesI2C);
		}
		else
		{
			// Successful i2c writes should always return 0
			break;
		}
	}

	mutex_unlock(&chip->lock);

	return (ret >= 0);
}

FSC_BOOL fusb_I2C_ReadData(FSC_U8 address, FSC_U8* data)
{
	FSC_S32 i = 0;
	FSC_S32 ret = 0;

	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL)
	{
		pr_err("%s - Error: %s is NULL!\n", __func__,
			(chip == NULL ? "Internal chip structure"
				: (chip->client == NULL ? "I2C Client"
					: "read data buffer")));
		return FALSE;
	}

	mutex_lock(&chip->lock);

	// Retry on failure up to the retry limit
	for (i = 0; i <= chip->numRetriesI2C; i++)
	{
		// Read a byte of data from address
		ret = i2c_smbus_read_byte_data(chip->client, (u8)address);

		if (ret < 0)
		{
			// Errors report as a negative 32-bit value
			dev_err(&chip->client->dev,
				"%s - I2C error reading byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n", __func__,
				address, ret, i, chip->numRetriesI2C);
		}
		else
		{
			// On success, the low 8-bits holds the byte read from the device
			*data = (FSC_U8)ret;
			break;
		}
	}

	mutex_unlock(&chip->lock);

	return (ret >= 0);
}

FSC_BOOL fusb_I2C_ReadBlockData(FSC_U8 address, FSC_U8 length, FSC_U8* data)
{
	FSC_S32 i = 0;
	FSC_S32 ret = 0;

	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL || chip->client == NULL || data == NULL)
	{
		pr_err("%s - Error: %s is NULL!\n", __func__,
			(chip == NULL ? "Internal chip structure"
				: (chip->client == NULL ? "I2C Client"
					: "block read data buffer")));
		return FALSE;
	}

	mutex_lock(&chip->lock);

	// Retry on failure up to the retry limit
	for (i = 0; i <= chip->numRetriesI2C; i++)
	{
		// Read a block of byte data from address
		ret = i2c_smbus_read_i2c_block_data(chip->client, (u8)address,
			(u8)length, (u8*)data);

		if (ret < 0)
		{
			// Errors report as a negative 32-bit value
			dev_err(&chip->client->dev,
				"%s - I2C error block reading byte data. Address: '0x%02x', Return: '%d'.  Attempt #%d / %d...\n", __func__,
				address, ret, i, chip->numRetriesI2C);
		}
		else if (ret != length)
		{
			// Slave didn't provide the full read response
			dev_err(&chip->client->dev,
				"%s - Error: Block read request of %u bytes truncated to %u bytes.\n", __func__,
				length, I2C_SMBUS_BLOCK_MAX);
		}
		else
		{
			// Success, don't retry
			break;
		}
	}

	mutex_unlock(&chip->lock);

	return (ret == length);
}


/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************		Timer Interface		******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

/*******************************************************************************
* Function:		_fusb_TimerHandler
* Input:			timer: hrtimer struct to be handled
* Return:			HRTIMER_RESTART to restart the timer, or HRTIMER_NORESTART otherwise
* Description:		Ticks state machine timer counters and rearms itself
********************************************************************************/

// Get the max value that we can delay in 10us increments at compile time
static const FSC_U32 MAX_DELAY_10US = (UINT_MAX / 10);
void fusb_Delay10us(FSC_U32 delay10us)
{
	FSC_U32 us = 0;
	if (delay10us > MAX_DELAY_10US)
	{
		pr_err("%s - Error: Delay of '%u' is too long! Must be less than '%u'.\n", __func__, delay10us, MAX_DELAY_10US);
		return;
	}

	us = delay10us * 10;									// Convert to microseconds (us)

	if (us <= 10)											// Best practice is to use udelay() for < ~10us times
	{
		udelay(us);											// BLOCKING delay for < 10us
	}
	else if (us < 20000)									// Best practice is to use usleep_range() for 10us-20ms
	{
		// TODO - optimize this range, probably per-platform
		usleep_range(us, us + (us / 10));					// Non-blocking sleep for at least the requested time, and up to the requested time + 10%
	}
	else													// Best practice is to use msleep() for > 20ms
	{
		msleep(us / 1000);									// Convert to ms. Non-blocking, low-precision sleep
	}
}

/*******************************************************************************
* Function:		fusb_Sysfs_Handle_Read
* Input:			output: Buffer to which the output will be written
* Return:			Number of chars written to output
* Description:		Reading this file will output the most recently saved hostcomm output buffer
* NOTE: Not used right now - separate functions for state logs.
********************************************************************************/
#define FUSB_MAX_BUF_SIZE 256   // Arbitrary temp buffer for parsing out driver data to sysfs

/* Reinitialize the FUSB302 */
static ssize_t _fusb_Sysfs_Reinitialize_fusb302(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL)
	{
		return sprintf(buf, "FUSB302 Error: Internal chip structure pointer is NULL!\n");
	}

	/* Make sure that we are doing this in a thread-safe manner */
	/* Waits for current IRQ handler to return, then disables it */
	disable_irq(chip->gpio_IntN_irq);

	core_initialize(&chip->port, 0x00);
	pr_debug ("FUSB  %s - Core is initialized!\n", __func__);
	core_enable_typec(&chip->port, TRUE);
	pr_debug ("FUSB  %s - Type-C State Machine is enabled!\n", __func__);

	enable_irq(chip->gpio_IntN_irq);

	return sprintf(buf, "FUSB302 Reinitialized!\n");
}

// Define our device attributes to export them to sysfs
static DEVICE_ATTR(reinitialize, S_IRUSR | S_IRGRP | S_IROTH, _fusb_Sysfs_Reinitialize_fusb302, NULL);

#ifdef FSC_DEBUG
static ssize_t hcom_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	FSC_U8 *outBuf = HCom_OutBuf();
	memcpy(buf, outBuf, HCMD_SIZE);
	return HCMD_SIZE;
}

static ssize_t hcom_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	FSC_U8 *inBuf  = HCom_InBuf();
	memcpy(inBuf, buf, HCMD_SIZE);
	HCom_Process();
	return HCMD_SIZE;
}
#endif /* FSC_DEBUG */

static ssize_t typec_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip->port.ConnState < NUM_TYPEC_STATES) {
		return sprintf(buf, "%d %s\n", chip->port.ConnState,
				TYPEC_STATE_TBL[chip->port.ConnState]);
	}
	return 0;
}

static ssize_t pe_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip->port.PolicyState < NUM_PE_STATES) {
		return sprintf(buf, "%d %s\n", chip->port.PolicyState,
				PE_STATE_TBL[chip->port.PolicyState]);
	}
	return 0;
}

static ssize_t port_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%d\n", chip->port.PortConfig.PortType);
}

static ssize_t cc_term_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();

	return sprintf(buf, "%s\n", CC_TERM_TBL[chip->port.CCTerm % NUM_CC_TERMS]);
}

static ssize_t vconn_term_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();

	return sprintf(buf, "%s\n", CC_TERM_TBL[chip->port.VCONNTerm % NUM_CC_TERMS]);
}

static ssize_t cc_pin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();

	return sprintf(buf, "%s\n",
		(chip->port.CCPin == CC1) ? "CC1" :
		(chip->port.CCPin == CC2) ? "CC2" : "None");
}

static ssize_t pwr_role_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%s\n",
		chip->port.PolicyIsSource == TRUE ? "Source" : "Sink");
}

static ssize_t data_role_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%s\n",
		chip->port.PolicyIsDFP == TRUE ? "DFP" : "UFP");
}

static ssize_t vconn_source_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%d\n", chip->port.IsVCONNSource);
}

static ssize_t pe_enabled_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%d\n", chip->port.USBPDEnabled);
}

static ssize_t pe_enabled_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int enabled;
	if (sscanf(buf, "%d", &enabled)) {
		chip->port.USBPDEnabled = enabled;
	}
	return count;
}

static ssize_t pd_specrev_show(struct device *dev, struct device_attribute *attr, char *buf)
{
#if 0
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int len = 0;
	switch(chip->port.PdRevContract) {
	case USBPDSPECREV1p0:
		len = sprintf(buf, "1\n");
		break;
	case USBPDSPECREV2p0:
		len = sprintf(buf, "2\n");
		break;
	case USBPDSPECREV3p0:
		len = sprintf(buf, "3\n");
		break;
	default:
		len = sprintf(buf, "Unknown\n");
		break;
	}
#endif
	int len = 0;
	len = sprintf(buf, "3\n");
	return len;
}

static ssize_t sink_op_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%d\n", chip->port.PortConfig.SinkRequestOpPower);
}

static ssize_t sink_op_power_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int op_pwr;
	if (sscanf(buf, "%d", &op_pwr)) {
		chip->port.PortConfig.SinkRequestOpPower = op_pwr;
	}
	return count;
}

static ssize_t sink_max_power_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%d\n", chip->port.PortConfig.SinkRequestMaxPower);
}

static ssize_t sink_max_power_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int snk_pwr;
	if (sscanf(buf, "%d", &snk_pwr)) {
		chip->port.PortConfig.SinkRequestMaxPower = snk_pwr;
	}
	return count;
}

static ssize_t src_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%d\n", chip->port.SourceCurrent);
}

static ssize_t src_current_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int src_cur;
	if (sscanf(buf, "%d", &src_cur)) {
		core_set_advertised_current(&chip->port, src_cur);
	}
	return count;
}

static ssize_t snk_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%d\n", chip->port.SinkCurrent);
}

static ssize_t snk_current_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int amp;
	if (sscanf(buf, "%d", &amp)) {
		chip->port.SinkCurrent = amp;
	}

	return count;
}

static ssize_t acc_support_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%d\n", chip->port.PortConfig.audioAccSupport);
}

static ssize_t acc_support_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (buf[0] == '1') {
		chip->port.PortConfig.audioAccSupport = TRUE;
	} else if (buf[0] == '0') {
		chip->port.PortConfig.audioAccSupport = FALSE;
	}
	return count;
}

static ssize_t src_pref_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%d\n", chip->port.PortConfig.SrcPreferred);
}

static ssize_t src_pref_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (buf[0] == '1') {
		chip->port.PortConfig.SrcPreferred = TRUE;
		chip->port.PortConfig.SnkPreferred = FALSE;
	} else if (buf[0] == '0'){
		chip->port.PortConfig.SrcPreferred = FALSE;
	}
	return count;
}

static ssize_t snk_pref_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	return sprintf(buf, "%d\n", chip->port.PortConfig.SnkPreferred);
}

static ssize_t snk_pref_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (buf[0] == '1') {
		chip->port.PortConfig.SnkPreferred = TRUE;
		chip->port.PortConfig.SrcPreferred = FALSE;
	} else if (buf[0] == '0'){
		chip->port.PortConfig.SnkPreferred = FALSE;
	}

	return count;
}

#ifdef FSC_DEBUG
static DEVICE_ATTR(hostcom, S_IRUGO | S_IWUSR, hcom_show, hcom_store);
#endif /* FSC_DEBUG */

static DEVICE_ATTR(typec_state, S_IRUGO | S_IWUSR, typec_state_show, 0);
static DEVICE_ATTR(port_type, S_IRUGO | S_IWUSR, port_type_show, 0);
static DEVICE_ATTR(cc_term, S_IRUGO | S_IWUSR, cc_term_show, 0);
static DEVICE_ATTR(vconn_term, S_IRUGO | S_IWUSR, vconn_term_show, 0);
static DEVICE_ATTR(cc_pin, S_IRUGO | S_IWUSR, cc_pin_show, 0);

static DEVICE_ATTR(pe_state, S_IRUGO | S_IWUSR, pe_state_show, 0);
static DEVICE_ATTR(pe_enabled, S_IRUGO | S_IWUSR, pe_enabled_show, pe_enabled_store);
static DEVICE_ATTR(pwr_role, S_IRUGO | S_IWUSR, pwr_role_show, 0);
static DEVICE_ATTR(data_role, S_IRUGO | S_IWUSR, data_role_show, 0);
static DEVICE_ATTR(pd_specrev, S_IRUGO | S_IWUSR, pd_specrev_show, 0);
static DEVICE_ATTR(vconn_source, S_IRUGO | S_IWUSR, vconn_source_show, 0);

static DEVICE_ATTR(sink_op_power, S_IRUGO | S_IWUSR, sink_op_power_show, sink_op_power_store);
static DEVICE_ATTR(sink_max_power, S_IRUGO | S_IWUSR, sink_max_power_show, sink_max_power_store);

static DEVICE_ATTR(src_current, S_IRUGO | S_IWUSR, src_current_show, src_current_store);
static DEVICE_ATTR(sink_current, S_IRUGO | S_IWUSR, snk_current_show, snk_current_store);

static DEVICE_ATTR(acc_support, S_IRUGO | S_IWUSR, acc_support_show, acc_support_store);
static DEVICE_ATTR(src_pref, S_IRUGO | S_IWUSR, src_pref_show, src_pref_store);
static DEVICE_ATTR(sink_pref, S_IRUGO | S_IWUSR, snk_pref_show, snk_pref_store);

static struct attribute *fusb302_sysfs_attrs[] = {
	&dev_attr_reinitialize.attr,
#ifdef FSC_DEBUG
	&dev_attr_hostcom.attr,
#endif /* FSC_DEBUG */
	&dev_attr_typec_state.attr,
	&dev_attr_port_type.attr,
	&dev_attr_cc_term.attr,
	&dev_attr_vconn_term.attr,
	&dev_attr_cc_pin.attr,
	&dev_attr_pe_state.attr,
	&dev_attr_pe_enabled.attr,
	&dev_attr_pwr_role.attr,
	&dev_attr_data_role.attr,
	&dev_attr_pd_specrev.attr,
	&dev_attr_vconn_source.attr,
	&dev_attr_sink_op_power.attr,
	&dev_attr_sink_max_power.attr,
	&dev_attr_src_current.attr,
	&dev_attr_sink_current.attr,
	&dev_attr_acc_support.attr,
	&dev_attr_src_pref.attr,
	&dev_attr_sink_pref.attr,
	NULL
};

static struct attribute_group fusb302_sysfs_attr_grp = {
	.name = "control",
	.attrs = fusb302_sysfs_attrs,
};

void fusb_Sysfs_Init(void)
{
	FSC_S32 ret = 0;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL)
	{
		pr_err("%s - Chip structure is null!\n", __func__);
		return;
	}

#ifdef FSC_DEBUG
	HCom_Init(&chip->port, 1);
#endif /* FSC_DEBUG */

	/* create attribute group for accessing the FUSB302 */
	ret = sysfs_create_group(&chip->client->dev.kobj, &fusb302_sysfs_attr_grp);
	if (ret)
	{
		pr_err("FUSB %s - Error creating sysfs attributes!\n", __func__);
	}
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/********************************************		Driver Helpers			******************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
void fusb_InitializeCore(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

	fusb_register_port();

	core_initialize(&chip->port, 0x00);
	chip->during_port_type_swap = FALSE;
	chip->during_drpr_swap = FALSE;

	pr_debug("FUSB  %s - Core is initialized!\n", __func__);
}

FSC_BOOL fusb_IsDeviceValid(void)
{
	FSC_U8 val = 0;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return FALSE;
	}

	// Test to see if we can do a successful I2C read
	if (!fusb_I2C_ReadData((FSC_U8)0x01, &val))
	{
		pr_err("FUSB  %s - Error: Could not communicate with device over I2C!\n", __func__);
		return FALSE;
	}
	/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/12/12 start */
	if (val != 0x91) {
		pr_err("FUSB  %s - Error: ChipId(0x%2x) is wrong\n", __func__, val);
		return FALSE;
	}
	/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/12/12 end */

	pr_info("FUSB %s - FUSB302B ChipId is 0x%2x\n", __func__, val);
	return TRUE;
}

FSC_BOOL fusb_reset(void)
{
	FSC_U8 val = 1;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return FALSE;
	}

	// Reset the FUSB302 and including the I2C registers to their default value.
	if (fusb_I2C_WriteData((FSC_U8)0x0C, 1, &val) == FALSE)
	{
		pr_err("FUSB  %s - Error: Could not communicate with device over I2C!\n", __func__);
		return FALSE;
	}
	return TRUE;
}

void fusb_InitChipData(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip == NULL)
	{
		pr_err("%s - Chip structure is null!\n", __func__);
		return;
	}

#ifdef FSC_DEBUG
	chip->dbgTimerTicks = 0;
	chip->dbgTimerRollovers = 0;
	chip->dbgSMTicks = 0;
	chip->dbgSMRollovers = 0;
	chip->dbg_gpio_StateMachine = -1;
	chip->dbg_gpio_StateMachine_value = false;
#endif  // FSC_DEBUG

	/* GPIO Defaults */
	chip->gpio_VBus5V = -1;
	chip->gpio_VBus5V_value = false;
	chip->gpio_VBusOther = -1;
	chip->gpio_VBusOther_value = false;
	chip->gpio_IntN = -1;

	/* DPM Setup - TODO - Not the best place for this. */
	chip->port.PortID = 0;
	DPM_Init(&chip->dpm);
	DPM_AddPort(chip->dpm, &chip->port);
	chip->port.dpm = chip->dpm;

	chip->gpio_IntN_irq = -1;

	/* I2C Configuration */
	chip->InitDelayMS = INIT_DELAY_MS;												// Time to wait before device init
	chip->numRetriesI2C = RETRIES_I2C;												// Number of times to retry I2C reads and writes
	chip->use_i2c_blocks = false;													// Assume failure

	/* Worker thread setup */
	INIT_WORK(&chip->sm_worker, work_function);

	chip->queued = FALSE;

	chip->highpri_wq = alloc_workqueue("FUSB WQ", WQ_HIGHPRI|WQ_UNBOUND, 1);

	if (chip->highpri_wq == NULL)
	{
		pr_err("%s - Unable to allocate new work queue!\n", __func__);
		return;
	}

	/* HRTimer Setup */
	hrtimer_init(&chip->sm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	chip->sm_timer.function = fusb_sm_timer_callback;
}


/*********************************************************************************************************************/
/*********************************************************************************************************************/
/******************************************		IRQ/Threading Helpers		*****************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/
FSC_S32 fusb_EnableInterrupts(void)
{
	FSC_S32 ret = 0;
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return -ENOMEM;
	}

	wakeup_source_init(&chip->fusb302_wakelock, "fusb302wakelock");
	/* Set up IRQ for INT_N GPIO */
	ret = gpio_to_irq(chip->gpio_IntN); // Returns negative errno on error
	if (ret < 0)
	{
		dev_err(&chip->client->dev, "%s - Error: Unable to request IRQ for INT_N GPIO! Error code: %d\n", __func__, ret);
		chip->gpio_IntN_irq = -1;   // Set to indicate error
		fusb_GPIO_Cleanup();
		return ret;
	}
	chip->gpio_IntN_irq = ret;
	pr_info("%s - Success: Requested INT_N IRQ: '%d'\n", __func__, chip->gpio_IntN_irq);

	/* Use NULL thread_fn as we will be queueing a work function in the handler.
		* Trigger is active-low, don't handle concurrent interrupts.
		* devm_* allocation/free handled by system
		*/
	ret = devm_request_threaded_irq(&chip->client->dev, chip->gpio_IntN_irq,
		_fusb_isr_intn, NULL, IRQF_ONESHOT | IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND | IRQF_NO_THREAD,
		FUSB_DT_INTERRUPT_INTN, chip);

	if (ret)
	{
		dev_err(&chip->client->dev, "%s - Error: Unable to request threaded IRQ for INT_N GPIO! Error code: %d\n", __func__, ret);
		fusb_GPIO_Cleanup();
		return ret;
	}

	enable_irq_wake(chip->gpio_IntN_irq);

	return 0;
}

void fusb_StartTimer(struct hrtimer *timer, FSC_U32 time_us)
{
	ktime_t ktime;
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
		return;
	}

	/* Set time in (seconds, nanoseconds) */
	ktime = ktime_set(0, time_us * 1000);
	hrtimer_start(timer, ktime, HRTIMER_MODE_REL);

	return;
}

void fusb_StopTimer(struct hrtimer *timer)
{
	struct fusb30x_chip *chip = fusb30x_GetChip();
	if (!chip) {
		pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
		return;
	}

	hrtimer_cancel(timer);

	return;
}

FSC_U32 get_system_time_us(void)
{
	unsigned long us;
	us = jiffies * 1000*1000 / HZ;
	return (FSC_U32)us;
}

/*******************************************************************************
* Function:		_fusb_isr_intn
* Input:			irq - IRQ that was triggered
*					dev_id - Ptr to driver data structure
* Return:			irqreturn_t - IRQ_HANDLED on success, IRQ_NONE on failure
* Description:		Activates the core
********************************************************************************/
static irqreturn_t _fusb_isr_intn(FSC_S32 irq, void *dev_id)
{
	struct fusb30x_chip* chip = dev_id;

	//pr_err("FUSB %s\n", __func__);

	if (!chip)
	{
		pr_err("FUSB  %s - Error: Chip structure is NULL!\n", __func__);
		return IRQ_NONE;
	}

	fusb_StopTimer(&chip->sm_timer);

	/* Schedule the process to handle the state machine processing */
	if (!chip->queued)
	{
		chip->queued = TRUE;
		queue_work(chip->highpri_wq, &chip->sm_worker);
	}

	return IRQ_HANDLED;
}

static enum hrtimer_restart fusb_sm_timer_callback(struct hrtimer *timer)
{
	struct fusb30x_chip* chip =
		container_of(timer, struct fusb30x_chip, sm_timer);

	if (!chip)
	{
		pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
		return HRTIMER_NORESTART;
	}

	/* Schedule the process to handle the state machine processing */
	if (!chip->queued)
	{
		chip->queued = TRUE;
		queue_work(chip->highpri_wq, &chip->sm_worker);
	}

	return HRTIMER_NORESTART;
}

static void work_function(struct work_struct *work)
{
	FSC_U32 timeout = 0;

	struct fusb30x_chip* chip =
		container_of(work, struct fusb30x_chip, sm_worker);

	if (!chip)
	{
		pr_err("FUSB  %s - Chip structure is NULL!\n", __func__);
		return;
	}

	/* Disable timer while processing */
	fusb_StopTimer(&chip->sm_timer);

	__pm_stay_awake(&chip->fusb302_wakelock);
	down(&chip->suspend_lock);

	/* Run the state machine */
	core_state_machine(&chip->port);

	/* Double check the interrupt line before exiting */
	if (platform_get_device_irq_state(chip->port.PortID))
	{
		queue_work(chip->highpri_wq, &chip->sm_worker);
	}
	else
	{
		chip->queued = FALSE;

		/* Scan through the timers to see if we need a timer callback */
		timeout = core_get_next_timeout(&chip->port);

		if (timeout > 0)
		{
			if (timeout == 1)
			{
				/* A value of 1 indicates that a timer has expired
					* or is about to expire and needs further processing.
					*/
				queue_work(chip->highpri_wq, &chip->sm_worker);
			}
			else
			{
				/* A non-zero time requires a future timer interrupt */
				fusb_StartTimer(&chip->sm_timer, timeout);
			}
		}
	}

	up(&chip->suspend_lock);
	__pm_relax(&chip->fusb302_wakelock);
}

//Add interface for kernel 5.4

static int usbpd_typec_dr_set(
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,19,191)
const struct typec_capability *caps,
#else
struct typec_port *port,
#endif
				enum typec_data_role role)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip->during_drpr_swap == TRUE)
		return 0;

	pr_info("Setting data role to %d, old data role:%d\n", role, chip->port.PolicyIsDFP );

	if (role == TYPEC_HOST) {
		if (chip->port.PolicyIsDFP == FALSE) {
			if (chip->port.PolicyState == peSinkReady) {
				SetPEState(&chip->port, peSinkSendDRSwap);
			} else if (chip->port.PolicyState == peSourceReady) {
				SetPEState(&chip->port, peSourceSendDRSwap);
			}
			/* hs14 code for SR-AL6528A-01-253 by wenyaqi at 2022/09/29 start */
			chip->during_drpr_swap = TRUE;
			/* hs14 code for SR-AL6528A-01-253 by wenyaqi at 2022/09/29 end */
			chip->port.PEIdle = FALSE;
			queue_work(chip->highpri_wq, &chip->sm_worker);
			pr_info("FUSB %s-%d: run to host---SendDRSwap\n", __func__, __LINE__);
		}
	} else if (role == TYPEC_DEVICE) {
		if (chip->port.PolicyIsDFP == TRUE) {
			if (chip->port.PolicyState == peSinkReady) {
				SetPEState(&chip->port, peSinkSendDRSwap);
			} else if (chip->port.PolicyState == peSourceReady) {
				SetPEState(&chip->port, peSourceSendDRSwap);
			}
			/* hs14 code for SR-AL6528A-01-253 by wenyaqi at 2022/09/29 start */
			chip->during_drpr_swap = TRUE;
			/* hs14 code for SR-AL6528A-01-253 by wenyaqi at 2022/09/29 end */
			chip->port.PEIdle = FALSE;
			queue_work(chip->highpri_wq, &chip->sm_worker);
			pr_info("FUSB %s-%d: run to device---SendDRSwap\n", __func__, __LINE__);
		}

	} else {
		pr_info("invalid role\n");
		return -EINVAL;
	}
	return 0;
}

static int usbpd_typec_pr_set(
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,19,191)
const struct typec_capability *caps,
#else
struct typec_port *port,
#endif
				enum typec_role role)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	if (chip->during_drpr_swap == TRUE)
		return 0;

	pr_info("Setting power role to %d\n", role);

	if (role == TYPEC_SOURCE) {
		if (chip->port.PolicyState == peSinkReady) {
			SetPEState(&chip->port, peSinkSendPRSwap);
			/* hs14 code for SR-AL6528A-01-253 by wenyaqi at 2022/09/29 start */
			chip->during_drpr_swap = TRUE;
			/* hs14 code for SR-AL6528A-01-253 by wenyaqi at 2022/09/29 start */
			chip->port.PEIdle = FALSE;
			queue_work(chip->highpri_wq, &chip->sm_worker);
			pr_err("FUSB %s: run peSourceSendPRSwap\n", __func__);
		}
	} else if (role == TYPEC_SINK) {
		if (chip->port.PolicyState == peSourceReady){
			SetPEState(&chip->port, peSourceSendPRSwap);
			/* hs14 code for SR-AL6528A-01-253 by wenyaqi at 2022/09/29 start */
			chip->during_drpr_swap = TRUE;
			/* hs14 code for SR-AL6528A-01-253 by wenyaqi at 2022/09/29 start */
			chip->port.PEIdle = FALSE;
			queue_work(chip->highpri_wq, &chip->sm_worker);
			pr_err("FUSB %s: run peSourceSendPRSwap\n", __func__);
		}
	} else {
		pr_err("invalid role\n");
		return -EINVAL;
	}
	return 0;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,19,191)
#define TYPEC_PORT_SRC TYPEC_PORT_DFP
#define TYPEC_PORT_SNK TYPEC_PORT_UFP
#define typec_set_orientation(x, y)
#endif

static int usbpd_typec_port_type_set(
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,19,191)
const struct typec_capability *caps,
#else
struct typec_port *port,
#endif
				enum typec_port_type type)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();

	pr_info("Setting mode to %d\n", type);

	if (type == TYPEC_PORT_DRP) {
		if (chip->during_port_type_swap) {
			/*
			chip->port.PortConfig.PortType = USBTypeC_DRP;
			switch (chip->typec_port.prefer_role)
			{
				case TYPE_SINK:
					chip->port.PortConfig.SnkPreferred = TRUE;
					break;
				case TYPE_SOURCE:
					chip->port.PortConfig.SrcPreferred = TRUE;
					break;
				default:
					break;
			}*/
			chip->port.PortConfig = chip->bak_port_config;
			if (chip->port.PortConfig.SnkPreferred)
				core_set_try_snk(&chip->port);
			else if(chip->port.PortConfig.SrcPreferred)
				core_set_try_src(&chip->port);
			else
				core_set_drp(&chip->port);
			/* hs14 code for SR-AL6528A-01-255|AL6528ADEU-1931 by wenyaqi at 2022/11/08 start */
			if (!chip->queued) {
				chip->queued = TRUE;
				queue_work(chip->highpri_wq, &chip->sm_worker);
			}
			/* hs14 code for SR-AL6528A-01-255|AL6528ADEU-1931 by wenyaqi at 2022/11/08 end */
		}
		return 0;
	}

	/*
		* Forces disconnect on CC and re-establishes connection.
		* This does not use PD-based PR/DR swap
		*/
	chip->during_port_type_swap = TRUE;
	chip->bak_port_config = chip->port.PortConfig;
	if (type == TYPEC_PORT_SNK)
		core_set_try_snk(&chip->port);
	else if (type == TYPEC_PORT_SRC)
		core_set_try_src(&chip->port);

	return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,19,191)

static struct typec_operations tcpc_ops = {
	.dr_set = usbpd_typec_dr_set,
	.pr_set = usbpd_typec_pr_set,
	.port_type_set = usbpd_typec_port_type_set,
};
#endif

static void fusb_register_port(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	chip->typec_caps.type = TYPEC_PORT_DRP;

/* hs14 code for SR-AL6528A-01-253 by wenyaqi at 2022/09/29 start */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,191)
/* hs14 code for SR-AL6528A-01-253 by wenyaqi at 2022/09/29 end */
	chip->typec_caps.data = TYPEC_PORT_DRD;
#endif

	chip->typec_caps.revision = 0x0130;
	chip->typec_caps.pd_revision = 0x0300;

	chip->typec_caps.prefer_role = TYPEC_NO_PREFERRED_ROLE;
	if (chip->port.PortConfig.SnkPreferred)
		chip->typec_caps.prefer_role = TYPEC_SINK;
	else if (chip->port.PortConfig.SrcPreferred)
		chip->typec_caps.prefer_role = TYPEC_SOURCE;

	chip->partner_desc.identity = &chip->partner_identity;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,19,191)
	chip->typec_caps.dr_set = usbpd_typec_dr_set;
	chip->typec_caps.pr_set = usbpd_typec_pr_set;
	chip->typec_caps.port_type_set = usbpd_typec_port_type_set;
#else
	chip->typec_caps.ops = &tcpc_ops;
#endif
	chip->typec_port = typec_register_port(&chip->client->dev, &chip->typec_caps);
}

/* add for platform system interface */

#if 1

void fusb_force_source(struct dual_role_phy_instance *dual_role)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	pr_debug("FUSB - %s\n", __func__);
	core_set_source(&chip->port);

	if (dual_role)
		dual_role_instance_changed(dual_role);
}
void fusb_force_sink(struct dual_role_phy_instance *dual_role)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	pr_debug("FUSB - %s\n", __func__);
	core_set_sink(&chip->port);
	if (dual_role)
		dual_role_instance_changed(dual_role);
}
unsigned int fusb_get_dual_role_mode(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int mode = DUAL_ROLE_PROP_MODE_NONE;

	if (chip->port.CCPin != CCNone) {
		if (chip->port.sourceOrSink == SOURCE) {
			mode = DUAL_ROLE_PROP_MODE_DFP;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_MODE_DFP, mode = %d\n",
					__func__, mode);
		} else {
			mode = DUAL_ROLE_PROP_MODE_UFP;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_MODE_UFP, mode = %d\n",
					__func__, mode);
		}
	}
	pr_debug("FUSB - %s mode = %d\n", __func__, mode);
	return mode;
}
unsigned int fusb_get_dual_role_power(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int current_pr = DUAL_ROLE_PROP_PR_NONE;

	pr_debug("FUSB %s\n", __func__);

	if (chip->port.CCPin != CCNone) {
		if (chip->port.sourceOrSink == SOURCE) {
			current_pr = DUAL_ROLE_PROP_PR_SRC;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_PR_SRC, current_pr = %d\n",
					__func__, current_pr);
		} else {
			current_pr = DUAL_ROLE_PROP_PR_SNK;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_PR_SNK, current_pr = %d\n",
					__func__, current_pr);
		}
	}
	pr_debug("FUSB - %s current_pr = %d\n", __func__, current_pr);
	return current_pr;
}
unsigned int fusb_get_dual_role_data(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int current_dr = DUAL_ROLE_PROP_DR_NONE;

	pr_debug("FUSB %s\n", __func__);

	if (chip->port.CCPin != CCNone) {
		if (chip->port.PolicyIsDFP) {
			current_dr = DUAL_ROLE_PROP_DR_HOST;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_DR_HOST, current_dr = %d\n",
					__func__, current_dr);
		} else {
			current_dr = DUAL_ROLE_PROP_DR_DEVICE;
			pr_debug("FUSB - %s DUAL_ROLE_PROP_DR_DEVICE, current_dr = %d\n",
					__func__, current_dr);
		}
	}
	pr_debug("FUSB - %s current_dr = %d\n", __func__, current_dr);
	return current_dr;
}
int dual_role_get_local_prop(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop, unsigned int *val)
{
	unsigned int mode = DUAL_ROLE_PROP_MODE_NONE;
	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		mode = fusb_get_dual_role_mode();
		*val = mode;
		break;
	case DUAL_ROLE_PROP_PR:
		mode = fusb_get_dual_role_power();
		*val = mode;
		break;
	case DUAL_ROLE_PROP_DR:
		mode = fusb_get_dual_role_data();
		*val = mode;
		break;
	default:
		pr_err("FUSB unsupported property %d\n", prop);
		return -ENODATA;
	}
	pr_debug("FUSB %s + prop=%d, val=%d\n", __func__, prop, *val);
	return 0;
}
int dual_role_set_prop(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop, const unsigned int *val)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	unsigned int mode = fusb_get_dual_role_mode();

	pr_debug("FUSB %s\n", __func__);

	if (!chip) {
		pr_err("FUSB %s - Error: Chip structure is NULL!\n", __func__);
		return -1;
	}
	pr_debug("FUSB %s + prop=%d,val=%d,mode=%d\n",
			__func__, prop, *val, mode);
	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		if (*val != mode) {
			if (mode == DUAL_ROLE_PROP_MODE_UFP)
				fusb_force_source(dual_role);
			else if (mode == DUAL_ROLE_PROP_MODE_DFP)
				fusb_force_sink(dual_role);
		}
		break;
	case DUAL_ROLE_PROP_PR:
		pr_debug("FUSB - %s DUAL_ROLE_PROP_PR\n", __func__);
		break;
	case DUAL_ROLE_PROP_DR:
		pr_debug("FUSB - %s DUAL_ROLE_PROP_DR\n", __func__);
		break;
	default:
		pr_debug("FUSB - %s default case\n", __func__);
		break;
	}
	return 0;
}
int dual_role_is_writeable(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop)
{
	pr_debug("FUSB - %s\n", __func__);
	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		return 1;
		break;
	case DUAL_ROLE_PROP_DR:
	case DUAL_ROLE_PROP_PR:
		return 0;
		break;
	default:
		break;
	}
	return 1;
}
#endif
void stop_usb_host(struct fusb30x_chip* chip)
{
	pr_info("FUSB - %s\n", __func__);
	extcon_set_state_sync(chip->extcon, EXTCON_USB_HOST, 0);
	if (chip->role_sw)
		usb_role_switch_set_role(chip->role_sw,  USB_ROLE_NONE);
}

void start_usb_host(struct fusb30x_chip* chip, bool ss)
{
	union extcon_property_value val;

	pr_info("FUSB - %s, ss=%d\n", __func__, ss);

	val.intval = (chip->port.CCPin == CC2);
	extcon_set_property(chip->extcon, EXTCON_USB_HOST,
			EXTCON_PROP_USB_TYPEC_POLARITY, val);

	val.intval = ss;
	extcon_set_property(chip->extcon, EXTCON_USB_HOST,
			EXTCON_PROP_USB_SS, val);

	extcon_set_state_sync(chip->extcon, EXTCON_USB_HOST, 1);
	if (chip->role_sw)
		usb_role_switch_set_role(chip->role_sw,  USB_ROLE_HOST);
}

void stop_usb_peripheral(struct fusb30x_chip* chip)
{
	pr_info("FUSB - %s\n", __func__);
	extcon_set_state_sync(chip->extcon, EXTCON_USB, 0);
	if (chip->role_sw)
		usb_role_switch_set_role(chip->role_sw,  USB_ROLE_NONE);
}

void fusb_register_partner(struct fusb30x_chip* chip)
{
	if (!chip->partner) {
		memset(&chip->partner_identity, 0, sizeof(chip->partner_identity));
		chip->partner_desc.usb_pd = false;
//		chip->partner_desc.accessory = TYPEC_ACCESSORY_NONE;
		chip->partner = typec_register_partner(chip->typec_port, &chip->partner_desc);
	}
}

void start_usb_peripheral(struct fusb30x_chip* chip)
{
	union extcon_property_value val;

	pr_info("FUSB - %s\n", __func__);
	val.intval = (chip->port.CCPin == CC2);
	extcon_set_property(chip->extcon, EXTCON_USB,
			EXTCON_PROP_USB_TYPEC_POLARITY, val);
	pr_debug("FUSB - %s, EXTCON_PROP_USB_TYPEC_POLARITY=%d\n",
		__func__, val.intval);

	val.intval = 1;
	extcon_set_property(chip->extcon, EXTCON_USB, EXTCON_PROP_USB_SS, val);
	pr_debug("FUSB - %s, EXTCON_PROP_USB_SS=%d\n", __func__, val.intval);

	val.intval = chip->port.SinkCurrent > utccDefault ? 1 : 0;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	extcon_set_property_capability(chip->extcon, EXTCON_USB,
						EXTCON_PROP_USB_TYPEC_MED_HIGH_CURRENT);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)) */
	pr_debug("FUSB - %s, EXTCON_PROP_USB_TYPEC_MED_HIGH_CURRENT=%d\n",
		__func__, val.intval);

	extcon_set_state_sync(chip->extcon, EXTCON_USB, 1);
	if (chip->role_sw)
		usb_role_switch_set_role(chip->role_sw,  USB_ROLE_DEVICE);
}

void handle_core_event(FSC_U32 event, FSC_U8 portId,
		void *usr_ctx, void *app_ctx)
{
	// union power_supply_propval val = {0};
	static bool start_power_swap = FALSE;
	/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/12 start */
	static int mtk_pd_noti = 0;
	/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/12 end */
	FSC_U32 set_voltage;
	FSC_U32 op_current;
	struct fusb30x_chip* chip = fusb30x_GetChip();

	if (!chip) {
		pr_err("FUSB %s - Error: Chip structure is NULL!\n", __func__);
		return;
	}

	pr_debug("FUSB %s - Notice, event=0x%x\n", __func__, event);
	/* hs14 code for AL6528ADEU-1931 by wenyaqi at 2022/12/01 start */
	mutex_lock(&chip->event_lock);
	/* hs14 code for AL6528ADEU-1931 by wenyaqi at 2022/12/01 end */
	switch (event) {
	case CC1_ORIENT:
	case CC2_ORIENT:
		pr_info("FUSB %s:CC Changed=0x%x\n", __func__, event);
		if (chip->during_port_type_swap) {
			chip->during_port_type_swap = FALSE;
			chip->port.PortConfig = chip->bak_port_config;
		}
		if (chip->port.ConnState == AudioAccessory) {
			/* hs14 code for P221201-05420 by hualei at 2022/12/1 start */
			if (chip->partner_desc.accessory != TYPEC_ACCESSORY_AUDIO) {
				chip->partner_desc.accessory = TYPEC_ACCESSORY_AUDIO;
				fusb_register_partner(chip);
			}
			/* hs14 code for P221201-05420 by hualei at 2022/12/1 end */
		} else {
			chip->partner_desc.accessory = TYPEC_ACCESSORY_NONE;
			if (chip->port.sourceOrSink == SINK) {
				cancel_delayed_work_sync(&chip->apsd_recheck_work);
				schedule_delayed_work(&chip->apsd_recheck_work, 200);

				typec_set_data_role(chip->typec_port, TYPEC_DEVICE);
				typec_set_pwr_role(chip->typec_port, TYPEC_SINK);
				typec_set_pwr_opmode(chip->typec_port, chip->port.SinkCurrent - utccDefault);
				typec_set_vconn_role(chip->typec_port, TYPEC_SINK);
				typec_set_orientation(chip->typec_port,
					(chip->port.CCPin==CC1)?TYPEC_ORIENTATION_NORMAL:TYPEC_ORIENTATION_REVERSE);
				fusb_register_partner(chip);
			} else if (chip->port.sourceOrSink == SOURCE) {
				start_usb_host(chip, true);
				typec_set_data_role(chip->typec_port, TYPEC_HOST);
				typec_set_pwr_role(chip->typec_port, TYPEC_SOURCE);
				typec_set_pwr_opmode(chip->typec_port, chip->port.SourceCurrent - utccDefault);
				typec_set_vconn_role(chip->typec_port, TYPEC_SOURCE);
				typec_set_orientation(chip->typec_port,
					(chip->port.CCPin==CC1)?TYPEC_ORIENTATION_NORMAL:TYPEC_ORIENTATION_REVERSE);
				fusb_register_partner(chip);
				//dock_operate(); //zjb
				chip->usb_state = 2;
				pr_info("FUSB %s start_usb_host\n", __func__);
			}
			/* add by yangdi start */
			g_cc_polarity = chip->port.CCPin;
			pr_err("FUSB %s g_cc_polarity[%d]\n", __func__, g_cc_polarity);
		}
		/* add by yangdi end */
		__pm_stay_awake(&chip->fusb302_wakelock);
		break;
	case CC_NO_ORIENT:
		typec_set_pwr_opmode(chip->typec_port, TYPEC_PWR_MODE_USB);

		pr_info("FUSB %s:CC_NO_ORIENT=0x%x\n", __func__, event);
		if (chip->usb_state != 0) {
			notify_adapter_event(MTK_PD_ADAPTER, MTK_PD_CONNECT_NONE, NULL);
			/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/12 start */
			mtk_pd_noti = 1;
			/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/12 end */
		}
		cancel_delayed_work_sync(&chip->apsd_recheck_work);
		start_power_swap = false;
		if (chip->usb_state == 1) {
			stop_usb_peripheral(chip);
			chip->usb_state = 0;
			pr_info("FUSB - %s stop_usb_peripheral,event=0x%x,usb_state=%d\n",
				__func__, event, chip->usb_state);
			// dock_operate(); //zjb
		} else if (chip->usb_state == 2) {
			stop_usb_host(chip);
			chip->usb_state = 0;
			pr_info("FUSB - %s stop_usb_host,event=0x%x,usb_state=%d\n",
				__func__, event, chip->usb_state);
		}

		/* hs14 code for P221201-05420 by hualei at 2022/12/1 start */
		if (chip->partner_desc.accessory == TYPEC_ACCESSORY_AUDIO) {
			/* disable AudioAccessory connection */
			chip->partner_desc.accessory = TYPEC_ACCESSORY_NONE;
		}
		/* hs14 code for P221201-05420 by hualei at 2022/12/1 end */
		if (chip->partner) {
			typec_unregister_partner(chip->partner);
			chip->partner = NULL;
		}
		chip->during_drpr_swap = FALSE;
		/* add by yangdi start */
		g_cc_polarity = 0;
		pr_err("FUSB %s g_cc_polarity[%d], disconnect\n", __func__, g_cc_polarity);
		/* add by yangdi end */
		__pm_relax(&chip->fusb302_wakelock);
		break;
	case PD_STATE_CHANGED:
		pr_info("FUSB %s:PD_STATE_CHANGED=0x%x, PE_ST=%d\n",
			__func__, event, chip->port.PolicyState);

		if (chip->port.PolicyState == peSinkReady ||
			chip->port.PolicyState == peSourceReady)
			typec_set_pwr_opmode(chip->typec_port, TYPEC_PWR_MODE_PD);

		if (chip->port.PolicyState == peSinkReady &&
			chip->port.PolicyHasContract == TRUE) {
			pr_info("FUSB %s req_obj=0x%x, sel_src_caps=0x%x\n",
				__func__, chip->port.USBPDContract.object,
				chip->port.SrcCapsReceived[
				chip->port.USBPDContract.FVRDO.ObjectPosition - 1].object);

			set_voltage = chip->port.SrcCapsReceived[
				chip->port.USBPDContract.FVRDO.ObjectPosition - 1].FPDOSupply.Voltage;
			op_current = chip->port.SinkRequest.FVRDO.OpCurrent;

			pr_info("FUSB %s set_voltage=%d, op_current=%d\n",
				__func__, set_voltage, op_current);
			/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/12 start */
			if (mtk_pd_noti == 0) {
				schedule_delayed_work(&chip->delay_init_work, msecs_to_jiffies(2*1000));
				mtk_pd_noti = 2;
			} else if (mtk_pd_noti == 1) {
				notify_adapter_event(MTK_PD_ADAPTER, MTK_PD_CONNECT_PE_READY_SNK, NULL);
				mtk_pd_noti = 2;
			}
			/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/12 end */
		}
		break;
	case PD_NEW_CONTRACT:
		pr_info("FUSB %s: PD_NEW_CONTRACT\n", __func__);
		if (chip->port.PolicyIsDFP && chip->usb_state != 2) {
			stop_usb_peripheral(chip);
			start_usb_host(chip, true);
			chip->usb_state = 2;
			typec_set_data_role(chip->typec_port, TYPEC_HOST);
			chip->during_drpr_swap = FALSE;
		} else if (!chip->port.PolicyIsDFP && chip->usb_state != 1) {
			if (chip->during_drpr_swap) {
				stop_usb_host(chip);
				start_usb_peripheral(chip);
			}
			chip->usb_state = 1;
			typec_set_data_role(chip->typec_port, TYPEC_DEVICE);
			chip->during_drpr_swap = FALSE;
		}
		break;
	case HARD_RESET:
		pr_info("FUSB %s:  - HARD_RESET", __func__);
		//notify_adapter_event(MTK_PD_ADAPTER, MTK_PD_CONNECT_HARD_RESET, NULL);
		if (chip->port.PolicyHasContract) {
			if (chip->port.PolicyIsSource) {
				stop_usb_peripheral(chip);
				start_usb_host(chip, true);
				chip->usb_state = 2;
				typec_set_data_role(chip->typec_port, TYPEC_HOST);
				typec_set_pwr_role(chip->typec_port, TYPEC_SOURCE);
				typec_set_vconn_role(chip->typec_port, TYPEC_SOURCE);
				chip->during_drpr_swap = FALSE;
			} else {
				stop_usb_host(chip);
				start_usb_peripheral(chip);
				chip->usb_state = 1;
				typec_set_data_role(chip->typec_port, TYPEC_DEVICE);
				typec_set_pwr_role(chip->typec_port, TYPEC_SINK);
				typec_set_vconn_role(chip->typec_port, TYPEC_SINK);
				chip->during_drpr_swap = FALSE;
			}
		}
		break;
	case PD_NO_CONTRACT:
		pr_debug("FUSB %s:PD_NO_CONTRACT=0x%x, PE_ST=%d\n",
			__func__, event, chip->port.PolicyState);
		break;
#if 0
	case SVID_EVENT:
		break;
	case DP_EVENT:
		pr_debug("FUSB %s:DP_EVENT=0x%x\n", __func__, event);
		pr_debug("FUSB %s:chip->port.AutoVdmState=%d\n",
			__func__, chip->port.AutoVdmState);
		break;
#endif
	case DATA_ROLE:
		if (chip->during_drpr_swap)
			chip->during_drpr_swap = FALSE;

		if (chip->port.PolicyIsDFP == FALSE) {
			typec_set_data_role(chip->typec_port, TYPEC_DEVICE);
			stop_usb_host(chip);
			start_usb_peripheral(chip);
			chip->usb_state = 1;
		} else if (chip->port.PolicyIsDFP == TRUE) {
			typec_set_data_role(chip->typec_port, TYPEC_HOST);
			stop_usb_peripheral(chip);
			start_usb_host(chip, true);
			chip->usb_state = 2;
		}
		pr_info("FUSB %s:DATA_ROLE=0x%x\n", __func__, event);
		break;
	case POWER_ROLE:
		if (chip->during_drpr_swap)
			chip->during_drpr_swap = FALSE;
		pr_debug("FUSB - %s:POWER_ROLE=0x%x", __func__, event);
		if (chip->port.PolicyIsSource) {
			typec_set_pwr_role(chip->typec_port, TYPEC_SOURCE);
			typec_set_pwr_opmode(chip->typec_port, chip->port.SourceCurrent - utccDefault);
		} else {
			typec_set_pwr_role(chip->typec_port, TYPEC_SINK);
			typec_set_pwr_opmode(chip->typec_port, chip->port.SourceCurrent - utccDefault);
		}
		if (start_power_swap == FALSE) {
			start_power_swap = true;
//			val.intval = 1;
//			power_supply_set_property(chip->usb_psy,
//				POWER_SUPPLY_PROP_PR_SWAP, &val);
//
		} else {
			start_power_swap = false;
//			val.intval = 0;
//			power_supply_set_property(chip->usb_psy,
//				POWER_SUPPLY_PROP_PR_SWAP, &val);
		}

		break;
	default:
		pr_debug("FUSB - %s:default=0x%x", __func__, event);
		break;
	}
	/* hs14 code for AL6528ADEU-1931 by wenyaqi at 2022/12/01 start */
	mutex_unlock(&chip->event_lock);
	/* hs14 code for AL6528ADEU-1931 by wenyaqi at 2022/12/01 end */
}

static void fusb_apsd_recheck_work(struct work_struct *work)
{
	struct fusb30x_chip *chip = container_of(work, struct fusb30x_chip,
							apsd_recheck_work.work);
	static u8 check_cnt = 0;
#if 1
	int ret = 0;
	struct power_supply *chr_psy = NULL;
	union power_supply_propval val = {.intval = 0};
#endif

	pr_info("FUSB %s: sourceOrSink:%d, CCPin:%d, check_cnt:%d\n",
					__func__, chip->port.sourceOrSink, chip->port.CCPin, check_cnt);

	if ((chip->port.sourceOrSink == SINK) && (chip->port.CCPin != CCNone)) {
#if 1
	chr_psy = power_supply_get_by_name("charger");
	ret = power_supply_get_property(chr_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &val);
	if(ret < 0  ||  val.intval == CHARGER_UNKNOWN) {
			if (check_cnt <= 2) {
				schedule_delayed_work(&chip->apsd_recheck_work, msecs_to_jiffies(200));
				check_cnt++;
			}
			return;
		} else {
			pr_err("FUSB %s:Charger type:%d\n", __func__, val.intval);
		/* hs14 code for SR-AL6528A-01-308|AL6528ADEU-28 by gaozhengwei at 2022/09/29 start */
		if (val.intval == STANDARD_HOST ||
				val.intval == CHARGING_HOST) {
		/* hs14 code for SR-AL6528A-01-308|AL6528ADEU-28 by gaozhengwei at 2022/09/29 end */
				pr_info("FUSB %s start_usb_peripheral\n", __func__);
				start_usb_peripheral(chip);
				chip->usb_state = 1;
			}
		}
#else
		if (check_cnt <=2){
			schedule_delayed_work(&chip->apsd_recheck_work, msecs_to_jiffies(200));
			check_cnt++;
			return;
		}else {
			pr_info("FUSB %s start_usb_peripheral\n", __func__);
			start_usb_peripheral(chip);
			chip->usb_state = 1;
		}
#endif
	}
	check_cnt = 0;
}

int usbpd_send_request(u8 reqPos)
{
	struct Port* port;
	struct fusb30x_chip* chip = fusb30x_GetChip();

	pr_info("FUSB enter: %s, reqPos=%8x\n", __func__, reqPos);
	if (!chip || !chip->port.PolicyHasContract) {
		pr_info("FUSB:%s No PDPHY or PD isn't supported\n", __func__);
		return -1;
	}
	port = &chip->port;

	port->PolicyMsgTxSop = SOP_TYPE_SOP;

	port->PDTransmitHeader.word = 0;
	port->PDTransmitHeader.MessageType = DMTRequest;
	port->PDTransmitHeader.NumDataObjects = 1;
	port->PDTransmitHeader.PortDataRole = port->PolicyIsDFP;
	port->PDTransmitHeader.PortPowerRole = port->PolicyIsSource;
	port->PDTransmitHeader.SpecRevision = DPM_SpecRev(port, SOP_TYPE_SOP);

	port->PDTransmitObjects[0].object = 0;
	port->PDTransmitObjects[0].FVRDO.ObjectPosition = reqPos & 0x07;
	port->PDTransmitObjects[0].FVRDO.GiveBack = port->PortConfig.SinkGotoMinCompatible;
	port->PDTransmitObjects[0].FVRDO.NoUSBSuspend = port->PortConfig.SinkUSBSuspendOperation;
	port->PDTransmitObjects[0].FVRDO.USBCommCapable = port->PortConfig.SinkUSBCommCapable;

	port->PDTransmitObjects[0].FVRDO.OpCurrent =
		port->SrcCapsReceived[reqPos-1].FPDOSupply.MaxCurrent;
	pr_info("FUSB : current_1[%d]\n",port->PDTransmitObjects[0].FVRDO.OpCurrent);
	port->PDTransmitObjects[0].FVRDO.MinMaxCurrent =
		port->SrcCapsReceived[reqPos-1].FPDOSupply.MaxCurrent;
	pr_info("FUSB : current_2[%d]\n",port->PDTransmitObjects[0].FVRDO.MinMaxCurrent);
	port->PDTransmitObjects[0].FVRDO.CapabilityMismatch = FALSE;

	port->USBPDTxFlag = TRUE;
	chip->port.PEIdle = FALSE;
	pr_info("FUSB queued\n");
	queue_work(chip->highpri_wq, &chip->sm_worker);

	return 0;
}


#include <mt-plat/v1/charger_class.h>

static int pd_get_property(struct adapter_device *dev,
	enum adapter_property sta)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	switch (sta) {
	case TYPEC_RP_LEVEL:
		{
			return core_get_advertised_current(&chip->port);
		}
		break;
	default:
		{
		}
		break;
	}
	return -1;
}

static int pd_set_cap(struct adapter_device *dev, enum adapter_cap_type type,
		int mV, int mA)
{
	int ret = MTK_ADAPTER_OK;

	pr_info("[%s] type:%d mV:%d mA:%d\n", __func__, type, mV, mA);
	 if (type == MTK_PD) {
		if (mV == 5000)
			usbpd_send_request(1);
		else if (mV == 9000)
			usbpd_send_request(2);
	}
	return ret;
}

static int pd_get_output(struct adapter_device *dev, int *mV, int *mA)
{
	return MTK_ADAPTER_NOT_SUPPORT;
}

static int pd_get_status(struct adapter_device *dev,
	struct adapter_status *sta)
{
	return MTK_ADAPTER_NOT_SUPPORT;
}

static int pd_get_cap(struct adapter_device *dev,
	enum adapter_cap_type type,
	struct adapter_power_cap *tacap)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	int i;

	if (type == MTK_PD) {
		if (chip->port.SrcCapsHeaderReceived.NumDataObjects != 0) {
			tacap->nr = chip->port.SrcCapsHeaderReceived.NumDataObjects;
			tacap->selected_cap_idx = chip->port.USBPDContract.FVRDO.ObjectPosition - 1;
			pr_info("[%s] nr:%d idx:%d\n", __func__, chip->port.SrcCapsHeaderReceived.NumDataObjects,
				chip->port.USBPDContract.FVRDO.ObjectPosition - 1);
			for (i = 0; i < tacap->nr; i++) {
				tacap->ma[i] = chip->port.SrcCapsReceived[i].FPDOSupply.MaxCurrent * 10;
				tacap->max_mv[i] = chip->port.SrcCapsReceived[i].FPDOSupply.Voltage * 50;
				tacap->min_mv[i] = tacap->min_mv[i];
				tacap->maxwatt[i] = tacap->max_mv[i] * tacap->ma[i];

				if (chip->port.SrcCapsReceived[i].PDO.SupplyType == 0)
					tacap->type[i] = MTK_PD;
				else if (chip->port.SrcCapsReceived[i].PDO.SupplyType == 3)
					tacap->type[i] = MTK_PD_APDO;
				else
					tacap->type[i] = MTK_CAP_TYPE_UNKNOWN;
				tacap->type[i] = chip->port.SrcCapsReceived[i].PDO.SupplyType; //FIXME

				pr_info("[%s]:%d mv:[%d,%d] %d max:%d min:%d type:%d %d\n",
					__func__, i, tacap->min_mv[i],
					tacap->max_mv[i], tacap->ma[i],
					tacap->maxwatt[i], tacap->minwatt[i],
					tacap->type[i], chip->port.SrcCapsReceived[i].PDO.SupplyType);
			}
		}
	}

	return MTK_ADAPTER_OK;
}

static struct adapter_ops adapter_ops = {
	.get_status = pd_get_status,
	.set_cap = pd_set_cap,
	.get_output = pd_get_output,
	.get_property = pd_get_property,
	.get_cap = pd_get_cap,
};

void fusb_init_mtk_pd_adapter()
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	chip->pd_adapter = adapter_device_register("pd_adapter",
		&chip->client->dev, NULL, &adapter_ops, NULL);
	if (chip->pd_adapter) {
		pr_info("FUSB, registered pd_adapter!\n");
	}
	else {
		pr_err("Failed to register pd_adapter.\n");
	}
}
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/17 end */

/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/12 start */
static void fusb_delay_init_work(struct work_struct *work)
{
	notify_adapter_event(MTK_PD_ADAPTER, MTK_PD_CONNECT_PE_READY_SNK, NULL);
}
void fusb_init_event_handler(void)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	INIT_DELAYED_WORK(&chip->apsd_recheck_work, fusb_apsd_recheck_work);
	INIT_DELAYED_WORK(&chip->delay_init_work, fusb_delay_init_work);
	/* hs14 code for AL6528ADEU-342 by wenyaqi at 2022/10/12 end */
	register_observer(CC_ORIENT_ALL|PD_CONTRACT_ALL|POWER_ROLE|
			PD_STATE_CHANGED|DATA_ROLE|EVENT_ALL,
			handle_core_event, NULL);
}

