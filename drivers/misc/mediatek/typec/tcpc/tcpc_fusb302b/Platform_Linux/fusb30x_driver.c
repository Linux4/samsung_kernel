/*
 * File:   fusb30x_driver.c
 * Company: Fairchild Semiconductor
 *
 * Created on September 2, 2015, 10:22 AM
 */

/* Standard Linux includes */
#include <linux/init.h>															// __init, __initdata, etc
#include <linux/module.h>														// Needed to be a module
#include <linux/kernel.h>														// Needed to be a kernel module
#include <linux/i2c.h>															// I2C functionality
#include <linux/slab.h>															// devm_kzalloc
#include <linux/types.h>														// Kernel datatypes
#include <linux/errno.h>														// EINVAL, ERANGE, etc
#include <linux/of_device.h>													// Device tree functionality
#include <linux/extcon.h>
#include <linux/usb/role.h>
#include <linux/extcon-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>

/* Driver-specific includes */
#include "fusb30x_global.h"														// Driver-specific structures/types
#include "platform_helpers.h"													// I2C R/W, GPIO, misc, etc
#include "../core/core.h"														// GetDeviceTypeCStatus
#include "../core/TypeC.h"
#include "class-dual-role.h"
#ifdef FSC_DEBUG
#include "dfs.h"
#endif // FSC_DEBUG
#include "fusb30x_driver.h"
#include <linux/delay.h>
//modfied by zjb
#include <linux/version.h>

extern int typec_mode;
extern int typec_rotation;
/******************************************************************************
* Driver functions
******************************************************************************/
static int __init fusb30x_init(void)
{
	pr_info("FUSB  %s - Start driver initialization...\n", __func__);

	return i2c_add_driver(&fusb30x_driver);
}

static void __exit fusb30x_exit(void)
{
	i2c_del_driver(&fusb30x_driver);
	pr_debug("FUSB  %s - Driver deleted...\n", __func__);
}

static int fusb302_i2c_resume(struct device* dev)
{
	struct fusb30x_chip *chip;
		struct i2c_client *client = to_i2c_client(dev);

		if (client) {
			chip = i2c_get_clientdata(client);
				if (chip)
				up(&chip->suspend_lock);
		}
		return 0;
}

static int fusb302_i2c_suspend(struct device* dev)
{
	struct fusb30x_chip* chip;
		struct i2c_client* client =  to_i2c_client(dev);

		if (client) {
				chip = i2c_get_clientdata(client);
					if (chip)
					down(&chip->suspend_lock);
		}
		return 0;
}

/* hs14 code for SR-AL6528A-01-322|AL6528ADEU-342 by wenyaqi at 2022/10/12 start */
/* hs14 code for AL6528ADEU-2820 by lina at 2022/11/23 start */
static void fusb_delay_attach_check_work(struct work_struct *work)
{
	fusb_InitializeCore();
}
/* hs14 code for AL6528ADEU-2820 by lina at 2022/11/23 end */
enum dual_role_property fusb_drp_properties[] = {
	DUAL_ROLE_PROP_SUPPORTED_MODES,
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};
static const unsigned int usbpd_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_DISP_DP,
	EXTCON_NONE,
};


bool is_poweroff_charge(void)
{

	char *ptr;
	pr_err("=====================saved_command_line=%s (%s)\n", __func__, saved_command_line);
	ptr = strstr(saved_command_line, "androidboot.mode=charger");
	if (ptr != NULL)
	{
		pr_err("==========power_off_charge=======true====\r\n");
		return true;
	}
		pr_err("==========power_off_charge=====false======\r\n");
	return false;
}

/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/11 start */
enum {
	TYPEC_STATUS_UNKNOWN = 0,
	TYPEC_STATUS_CC1,
	TYPEC_STATUS_CC2,
};

static int typec_port_get_property(struct power_supply *psy, enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	struct fusb30x_chip* chip = container_of(psy->desc, struct fusb30x_chip, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION:
		if (chip->port.CCPin == CC1) {
			val->intval = TYPEC_STATUS_CC1;
		} else if (chip->port.CCPin == CC2) {
			val->intval = TYPEC_STATUS_CC2;
		} else {
			val->intval = TYPEC_STATUS_UNKNOWN;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property typec_port_properties[] = {
	POWER_SUPPLY_PROP_TYPEC_CC_ORIENTATION,
};
/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/11 end */
/* HS14_U/TabA7 Lite U for AL6528AU-249/AX3565AU-309 by liufurong at 20231212 start */
int fusb302_send_5v_source_caps(int ma)
{
	struct fusb30x_chip* chip = fusb30x_GetChip();
	struct Port *port = &chip->port;

	port->PolicyMsgTxSop = SOP_TYPE_SOP;
	port->PDTransmitHeader.word = 0;
	port->PDTransmitHeader.MessageType = DMTSourceCapabilities;
	port->PDTransmitHeader.NumDataObjects = 1;
	port->PDTransmitHeader.PortDataRole = port->PolicyIsDFP;
	port->PDTransmitHeader.PortPowerRole = port->PolicyIsSource;
	port->PDTransmitHeader.SpecRevision = DPM_SpecRev(port, SOP_TYPE_SOP);

	port->src_caps[0].FPDOSupply.MaxCurrent = ma/10;

	port->USBPDTxFlag = TRUE;
	if (!chip->queued)
	{
		chip->queued = TRUE;
		queue_work(chip->highpri_wq, &chip->sm_worker);
	}
	return 0;
}
/* HS14_U/TabA7 Lite U for AL6528AU-249/AX3565AU-309 by liufurong at 20231212 end */

#define FUSB302_PROBE_CNT_MAX 3

/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/07 start */
static int fusb30x_remove(struct i2c_client* client);
/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/07 end */
static int fusb30x_probe(struct i2c_client* client,
							const struct i2c_device_id* id)
{
	int ret = 0;
	struct fusb30x_chip* chip = NULL;
	struct i2c_adapter* adapter = NULL;
	struct power_supply *usb_psy = NULL;
	struct platform_device *conn_pdev = NULL;
	struct device_node *conn_np = NULL;
	// struct power_supply *pogo_psy;
//#ifdef CONFIG_DUAL_ROLE_USB_INTF
	struct dual_role_phy_desc *dual_desc = NULL;
	struct dual_role_phy_instance *dual_role = NULL;
//#endif
	static int probe_cnt = 0;
	pr_info("FUSB - %s start\n", __func__);
	pr_err("%s probe_cnt = %d\n", __func__, ++probe_cnt);
	if (!client) {
		pr_err("FUSB  %s - Error: Client structure is NULL!\n", __func__);
		return -EINVAL;
	}

	dev_info(&client->dev, "%s\n", __func__);

	/* Make sure probe was called on a compatible device */
	if (!of_match_device(fusb302_match_table, &client->dev)) {
		dev_err(&client->dev,
			"FUSB  %s - Error: Device tree mismatch!\n",
			__func__);
		return -EINVAL;
	}
	pr_debug("FUSB  %s - Device tree matched!\n", __func__);

	/* register power supply */
	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_info("FUSB - %s Could not get USB power_supply, deferring probe\n",
			__func__);
		return -EPROBE_DEFER;
	}

#if 0
	pogo_psy = power_supply_get_by_name("customer");
	if (!pogo_psy) {
		pr_info("FUSB - %s Could not get USB power_supply, deferring probe\n",
			__func__);
		return -EPROBE_DEFER;
	}
#endif

	/* Allocate space for our chip structure (devm_* is managed by the device) */
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev,
			"FUSB  %s - Error: Unable to allocate memory for g_chip!\n",
			__func__);

		power_supply_put(usb_psy);
		// power_supply_put(pogo_psy); //modfied by zjb
		if (probe_cnt >= FUSB302_PROBE_CNT_MAX)
			goto out;
		else
			return -EIO;
	}
	chip->client = client;														// Assign our client handle to our chip
	fusb30x_SetChip(chip);														// Set our global chip's address to the newly allocated memory
	pr_debug("FUSB  %s - Chip structure is set! Chip: %p ... g_chip: %p\n", __func__, chip, fusb30x_GetChip());

	/* Verify that the system has our required I2C/SMBUS functionality (see <linux/i2c.h> for definitions) */
	adapter = to_i2c_adapter(client->dev.parent);
	if (i2c_check_functionality(adapter, FUSB30X_I2C_SMBUS_BLOCK_REQUIRED_FUNC))
	{
		chip->use_i2c_blocks = true;
	}
	else
	{
	// If the platform doesn't support block reads, try with block writes and single reads (works with eg. RPi)
	// NOTE: It is likely that this may result in non-standard behavior, but will often be 'close enough' to work for most things
		dev_warn(&client->dev, "FUSB  %s - Warning: I2C/SMBus block read/write functionality not supported, checking single-read mode...\n", __func__);
		if (!i2c_check_functionality(adapter, FUSB30X_I2C_SMBUS_REQUIRED_FUNC))
		{
			dev_err(&client->dev, "FUSB  %s - Error: Required I2C/SMBus functionality not supported!\n", __func__);
			dev_err(&client->dev, "FUSB  %s - I2C Supported Functionality Mask: 0x%x\n", __func__, i2c_get_functionality(adapter));
			if (probe_cnt >= FUSB302_PROBE_CNT_MAX)
				goto out;
			else
				return -EIO;
		}
	}
	pr_debug("FUSB  %s - I2C Functionality check passed! Block reads: %s\n", __func__, chip->use_i2c_blocks ? "YES" : "NO");

	/* Assign our struct as the client's driverdata */
	i2c_set_clientdata(client, chip);
	pr_debug("FUSB  %s - I2C client data set!\n", __func__);

	/* Verify that our device exists and that it's what we expect */
	if (!fusb_IsDeviceValid())
	{
		dev_err(&client->dev, "FUSB  %s - Error: Unable to communicate with device!\n", __func__);
		if (probe_cnt >= FUSB302_PROBE_CNT_MAX)
			goto out;
		else
			return -EIO;
	}
	pr_debug("FUSB  %s - Device check passed!\n", __func__);

	/* Initialize the chip lock */
	mutex_init(&chip->lock);
	/* hs14 code for AL6528ADEU-1931 by wenyaqi at 2022/12/01 start */
	mutex_init(&chip->event_lock);
	/* hs14 code for AL6528ADEU-1931 by wenyaqi at 2022/12/01 end */

	/* Initialize the chip's data members */
	fusb_InitChipData();
	pr_debug("FUSB  %s - Chip struct data initialized!\n", __func__);

	/* Add QRD extcon */
	chip->extcon = devm_extcon_dev_allocate(&client->dev, usbpd_extcon_cable);
	if (!chip->extcon) {
		dev_err(&client->dev,
			"FUSB %s - Error: Unable to allocate memory for extcon!\n",
			__func__);
		if (probe_cnt >= FUSB302_PROBE_CNT_MAX)
			goto out;
		else
			return -EIO;
	}
	ret = devm_extcon_dev_register(&client->dev, chip->extcon);
	if (ret) {
		dev_err(&client->dev, "FUSB failed to register extcon device\n");
		if (probe_cnt >= FUSB302_PROBE_CNT_MAX)
			goto out;
		else
			return -EIO;
	}
	extcon_set_property_capability(chip->extcon, EXTCON_USB,
					EXTCON_PROP_USB_TYPEC_POLARITY);
	extcon_set_property_capability(chip->extcon, EXTCON_USB,
					EXTCON_PROP_USB_SS);
//modfied by zjb
//#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
//	extcon_set_property_capability(rpmd->extcon, EXTCON_USB,
//						EXTCON_PROP_USB_TYPEC_MED_HIGH_CURRENT);
//#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)) */

	extcon_set_property_capability(chip->extcon, EXTCON_USB_HOST,
					EXTCON_PROP_USB_TYPEC_POLARITY);
	extcon_set_property_capability(chip->extcon, EXTCON_USB_HOST,
					EXTCON_PROP_USB_SS);

	/* usb role switich */
	conn_np = of_parse_phandle(client->dev.of_node, "dev-conn", 0);
	if (!conn_np) {
		dev_info(&client->dev, "failed to get dev-conn node \n");
		goto END_USB_ROLE_SWITCH;
	}

	conn_pdev = of_find_device_by_node(conn_np);
	if (!conn_pdev) {
		dev_info(&client->dev, "failed to get dev-conn pdev \n");
		goto END_USB_ROLE_SWITCH;
	}

	chip->dev_conn.endpoint[0] = kasprintf(GFP_KERNEL, "%s-role-switch", dev_name(&conn_pdev->dev));
	chip->dev_conn.endpoint[1] = dev_name(&client->dev);
	chip->dev_conn.id = "usb-role-switch";
	device_connection_add(&chip->dev_conn);

	chip->role_sw = usb_role_switch_get(&client->dev);
	if (IS_ERR(chip->role_sw)) {
		dev_err(&client->dev, "failed to get usb role\n");
		chip->role_sw = NULL;
	}
END_USB_ROLE_SWITCH:
//#ifdef CONFIG_DUAL_ROLE_USB_INTF
  //  if (IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)) {
		dual_desc = devm_kzalloc(&client->dev, sizeof(struct dual_role_phy_desc),
					GFP_KERNEL);
		if (!dual_desc) {
			dev_err(&client->dev,
				"%s: unable to allocate dual role descriptor\n",
				__func__);
			if (probe_cnt >= FUSB302_PROBE_CNT_MAX)
				goto out;
			else
				return -EIO;
		}
		dual_desc->name = "otg_default";
		dual_desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
		dual_desc->get_property = dual_role_get_local_prop;
		dual_desc->set_property = dual_role_set_prop;
		dual_desc->properties = fusb_drp_properties;
		dual_desc->num_properties = ARRAY_SIZE(fusb_drp_properties);
		dual_desc->property_is_writeable = dual_role_is_writeable;
		dual_role = devm_dual_role_instance_register(&client->dev, dual_desc);
		dual_role->drv_data = client;
		chip->dual_role = dual_role;
		chip->dr_desc = dual_desc;
	//}
//#endif

	chip->is_vbus_present = FALSE;
	chip->usb_psy = usb_psy;
	// chip->pogo_psy = pogo_psy; //zjb
	fusb_init_event_handler();

	fusb_init_mtk_pd_adapter();

	/* reset fusb302*/
	fusb_reset();

	/* Initialize semaphore*/
	sema_init(&chip->suspend_lock, 1);

	/* Initialize the platform's GPIO pins and IRQ */
	ret = fusb_InitializeGPIO();
	if (ret)
	{
		dev_err(&client->dev, "FUSB  %s - Error: Unable to initialize GPIO!\n", __func__);
		return ret;
	}
	pr_debug("FUSB  %s - GPIO initialized!\n", __func__);

	/* Initialize sysfs file accessors */
	fusb_Sysfs_Init();
	pr_debug("FUSB  %s - Sysfs nodes created!\n", __func__);

#ifdef FSC_DEBUG
	/* Initialize debugfs file accessors */
	fusb_DFS_Init();
	pr_debug("FUSB  %s - DebugFS nodes created!\n", __func__);
#endif // FSC_DEBUG

	/* Enable interrupts after successful core/GPIO initialization */
	ret = fusb_EnableInterrupts();
	if (ret)
	{
		dev_err(&client->dev, "FUSB  %s - Error: Unable to enable interrupts! Error code: %d\n", __func__, ret);
		if (probe_cnt >= FUSB302_PROBE_CNT_MAX)
			goto out;
		else
			return -EIO;
	}

	/* Initialize the core and enable the state machine (NOTE: timer and GPIO must be initialized by now)
	*  Interrupt must be enabled before starting 302 initialization */
	/* hs14 code for AL6528ADEU-2820 by lina at 2022/11/23 start */
	INIT_DELAYED_WORK(&chip->delay_attach_check_work, fusb_delay_attach_check_work);
	schedule_delayed_work(&chip->delay_attach_check_work, msecs_to_jiffies(3*1000));
	/* hs14 code for SR-AL6528A-01-322|AL6528ADEU-342 by wenyaqi at 2022/10/12 end */
	//pr_debug("FUSB  %s - Core is initialized!\n", __func__);
	/* hs14 code for AL6528ADEU-2820 by lina at 2022/11/23 end */
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/11 start */
	chip->dev = &client->dev;
	chip->psd.name = "typec_port";
	chip->psd.properties = typec_port_properties;
	chip->psd.type = POWER_SUPPLY_TYPE_USB_TYPE_C;
	chip->psd.num_properties = ARRAY_SIZE(typec_port_properties);
	chip->psd.get_property = typec_port_get_property;
	chip->typec_psy = devm_power_supply_register(chip->dev, &chip->psd, NULL);
	if (IS_ERR(chip->typec_psy)) {
		pr_err("FUSB failed to register power supply: %ld\n",
			PTR_ERR(chip->typec_psy));
	}
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/11 end */

	dev_info(&client->dev, "FUSB  %s - FUSB30X Driver loaded successfully!\n", __func__);

	/* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 start */
	tcpc_info = FUSB302;
	/* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 start */

	return ret;

out:
	pr_err("%s %s!!\n", __func__, ret == -EPROBE_DEFER ?
				"Over probe cnt max" : "OK");
	/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/07 start */
	fusb30x_remove(client);
	/* hs14 code for SR-AL6528A-01-312 by wenyaqi at 2022/09/07 end */
	return 0;
}

static int fusb30x_remove(struct i2c_client* client)
{
	pr_debug("FUSB  %s - Removing fusb30x device!\n", __func__);

#ifdef FSC_DEBUG
	/* Remove debugfs file accessors */
	fusb_DFS_Cleanup();
	pr_debug("FUSB  %s - DebugFS nodes removed.\n", __func__);
#endif // FSC_DEBUG

	fusb_GPIO_Cleanup();
	pr_debug("FUSB  %s - FUSB30x device removed from driver...\n", __func__);
	return 0;
}

static void fusb30x_shutdown(struct i2c_client *client)
{
	FSC_U8 reset = 0x01; /* regaddr is 0x01 */
	FSC_U8 data = 0x40; /* data is 0x40 */
	FSC_U8 length = 0x01; /* length is 0x01 */
	FSC_BOOL ret = 0;
	struct fusb30x_chip *chip = fusb30x_GetChip();

	if (!chip) {
		pr_err("FUSB shutdown - Chip structure is NULL!\n");
		return;
	}
	/* hs14 code for AL6528A-1048 by wenyaqi at 2022/12/27 start */
	chip->typec_port = NULL;
	/* hs14 code for AL6528A-1048 by wenyaqi at 2022/12/27 end */
	core_enable_typec(&chip->port, false);
	ret = DeviceWrite(chip->port.I2cAddr, regControl3, length, &data);
	if (ret != 0)
		pr_err("send hardreset failed, ret = %d\n", ret);

	SetStateUnattached(&chip->port);
	/* Enable the pull-up on CC1 */
	chip->port.Registers.Switches.PU_EN1 = 1;
	/* Disable the pull-down on CC1 */
	chip->port.Registers.Switches.PDWN1 = 0;
	/* Enable the pull-up on CC2 */
	chip->port.Registers.Switches.PU_EN2 = 1;
	/* Disable the pull-down on CC2 */
	chip->port.Registers.Switches.PDWN2 = 0;
	/* Commit the switch state */
	DeviceWrite(chip->port.I2cAddr, regSwitches0, 1,
		&chip->port.Registers.Switches.byte[0]);
	fusb_GPIO_Cleanup();
	/* keep the cc open status 20ms */
	mdelay(20);
	ret = fusb_I2C_WriteData((FSC_U8)regReset, length, &reset);
	if (ret != 0)
		pr_err("device Reset failed, ret = %d\n", ret);

	pr_info("FUSB shutdown - FUSB30x device shutdown!\n");
}


/*******************************************************************************
 * Driver macros
 ******************************************************************************/
module_init(fusb30x_init);														// Defines the module's entrance function
module_exit(fusb30x_exit);														// Defines the module's exit function

MODULE_LICENSE("GPL");															// Exposed on call to modinfo
MODULE_DESCRIPTION("Fairchild FUSB30x Driver");									// Exposed on call to modinfo
MODULE_AUTHOR("Fairchild");														// Exposed on call to modinfo
