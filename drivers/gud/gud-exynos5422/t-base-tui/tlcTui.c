/** @addtogroup TLC_TUI
 * @{
 * @file tlcTui.c
 * Application main for the sample TUI Driver connector.
 *
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <linux/string.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/clk.h>
#include <linux/clk.h>
#include "trustedui.h"
#include <linux/gpio.h>
#if defined(CONFIG_SOC_EXYNOS5422)
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <fimd_fb.h>
#endif
#if defined(CONFIG_SOC_EXYNOS5420)
#include <s3c-fb-struct.h>
#endif

#include "mobicore_driver_api.h"
#include "tlcTui.h"
#include "dciTui.h"
#include "tui_ioctl.h"


/* ------------------------------------------------------------- */
/* Externs */
extern struct completion ioComp;

/* ------------------------------------------------------------- */
/* Globals */
const struct mc_uuid_t drUuid = DR_TUI_UUID;
struct mc_session_handle drSessionHandle = {0, 0};
tuiDciMsg_ptr pDci;
DECLARE_COMPLETION(dciComp);
uint32_t gCmdId = TLC_TUI_CMD_NONE;
tlcTuiResponse_t gUserRsp = {TLC_TUI_CMD_NONE, TLC_TUI_ERR_UNKNOWN_CMD};

/* ------------------------------------------------------------- */
/* Static */
static const uint32_t DEVICE_ID = MC_DEVICE_ID_DEFAULT;
static uint32_t *tuiFrameBuffer = NULL;


/* Functions */

/* ------------------------------------------------------------- */
/*static bool allocateTuiBuffer(tuiDciMsg_ptr pDci); */
static bool allocateTuiBuffer(tuiDciMsg_ptr pDci)
{
	bool ret = false;
	uint32_t requestedSize = 0;

	if (!pDci) {
		pr_debug("%s(%d): pDci is null\n", __func__, __LINE__);
		return false;
	}

	requestedSize = pDci->cmdNwd.payload.allocSize;
	pr_debug("%s(%d): Requested size=%u\n", __func__, __LINE__,
			requestedSize);

	if (0 == requestedSize) {
		pr_debug("TUI frame buffer: nothing to allocate.");
		ret = true;
	} else {
		tuiFrameBuffer = kmalloc(requestedSize, GFP_KERNEL);

		if (!tuiFrameBuffer) {
			pr_err("Could not allocate TUI frame buffer");
			ret = false;
		} else if (ksize(tuiFrameBuffer) < requestedSize) {
			pr_err("TUI frame buffer allocated size is smaller than required: %d iso %d",
					ksize(tuiFrameBuffer), requestedSize);
			kfree(tuiFrameBuffer);
			ret = false;
		} else {
			pDci->nwdRsp.allocBuffer.pa =
				(void *) virt_to_phys(tuiFrameBuffer);

			pDci->nwdRsp.allocBuffer.size = ksize(tuiFrameBuffer);
			ret = true;
		}
	}

	return ret;
}

static void freeTuiBuffer(void)
{
	kfree(tuiFrameBuffer);
	tuiFrameBuffer = NULL;
}

/* ------------------------------------------------------------- */
static bool tlcOpenDriver(void)
{
	bool ret = false;
	enum mc_result mcRet;

	/* Allocate WSM buffer for the DCI */
	mcRet = mc_malloc_wsm(DEVICE_ID, 0, sizeof(tuiDciMsg_t),
			(uint8_t **)&pDci, 0);
	if (MC_DRV_OK != mcRet) {
		pr_debug("ERROR tlcOpenDriver: Allocation of DCI WSM failed: %d\n",
				mcRet);
		return false;
	}

	/* Clear the session handle */
	memset(&drSessionHandle, 0, sizeof(drSessionHandle));
	/* The device ID (default device is used */
	drSessionHandle.device_id = DEVICE_ID;
	/* Open session with the Driver */
	mcRet = mc_open_session(&drSessionHandle, &drUuid, (uint8_t *)pDci,
			(uint32_t)sizeof(tuiDciMsg_t));
	if (MC_DRV_OK != mcRet) {
		pr_debug("ERROR tlcOpenDriver: Open driver session failed: %d\n",
				mcRet);
		ret = false;
	} else {
		ret = true;
	}

	return ret;
}


/* ------------------------------------------------------------- */
static bool tlcOpen(void)
{
	bool ret = false;
	enum mc_result mcRet;

	/* Open the tbase device */
	pr_debug("tlcOpen: Opening tbase device\n");
	mcRet = mc_open_device(DEVICE_ID);

	/* In case the device is already open, mc_open_device will return an
	 * error (MC_DRV_ERR_INVALID_OPERATION).  But in this case, we can
	 * continue, even though mc_open_device returned an error.  Stop in all
	 * other case of error
	 */
	if (MC_DRV_OK != mcRet && MC_DRV_ERR_INVALID_OPERATION != mcRet) {
		pr_debug("ERROR tlcOpen: Error %d opening device\n", mcRet);
		return false;
	}

	pr_debug("tlcOpen: Opening driver session\n");
	ret = tlcOpenDriver();

	return ret;
}


/* ------------------------------------------------------------- */
static void tlcWaitCmdFromDriver(void)
{
	uint32_t ret = TUI_DCI_ERR_INTERNAL_ERROR;

	/* Wait for a command from secure driver */
	ret = mc_wait_notification(&drSessionHandle, -1);
	if (MC_DRV_OK == ret)
		pr_debug("tlcWaitCmdFromDriver: Got a command\n");
	else
		pr_debug("ERROR tlcWaitCmdFromDriver: mc_wait_notification() failed: %d\n",
			ret);
}


static uint32_t sendCmdToUser(uint32_t commandId)
{
	dciReturnCode_t ret = TUI_DCI_ERR_NO_RESPONSE;

	/* Init shared variables */
	gCmdId = commandId;
	gUserRsp.id = TLC_TUI_CMD_NONE;
	gUserRsp.returnCode = TLC_TUI_ERR_UNKNOWN_CMD;
	
	/* Give way to ioctl thread */
	complete(&dciComp);
	pr_debug("sendCmdToUser: give way to ioctl thread\n");

	/* Wait for ioctl thread to complete */
	wait_for_completion_interruptible(&ioComp);
	pr_debug("sendCmdToUser: Got an answer from ioctl thread.\n");
	INIT_COMPLETION(ioComp);

	/* Check id of the cmd processed by ioctl thread (paranoïa) */
	if (gUserRsp.id != commandId) {
		pr_debug("sendCmdToUser ERROR: Wrong response id 0x%08x iso 0x%08x\n",
				pDci->nwdRsp.id, RSP_ID(commandId));
		ret = TUI_DCI_ERR_INTERNAL_ERROR;
	} else {
		/* retrieve return code */
		switch (gUserRsp.returnCode) {
		case TLC_TUI_OK:
			ret = TUI_DCI_OK;
			break;
		case TLC_TUI_ERROR:
			ret = TUI_DCI_ERR_INTERNAL_ERROR;
			break;
		case TLC_TUI_ERR_UNKNOWN_CMD:
			ret = TUI_DCI_ERR_UNKNOWN_CMD;
			break;
		}
	}

	return ret;
}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static int is_device_ok(struct device *fbdev, const void *p)
{
	return 1;
}

/*
 * clocks needed by the display
 */
char *disp_clock_name[]  = {
//#ifdef CONFIG_K3G
	"lcd", "axi_disp1", "sclk_fimd",
	"clk_fimd1", "aclk_axi_disp1x", "sclk_fimd1"	
//#endif

#if defined(CONFIG_SOC_EXYNOS5420)
	"lcd", "axi_disp1", "sclk_fimd"
#endif
};
char *other_clock_name[] = {
//#if defined(CONFIG_SOC_EXYNOS5420)
	"mdnie1", "sclk_mdnie", "sclk_mdnie_pwm1",
	"dsim1", "dp", "fimd1", "fimd1a"
//#endif
};

static struct device *get_fb_dev(void)
{
	struct device *fbdev = NULL;

	/* get the first framebuffer device */
	/* [TODO] Handle properly when there are more than one framebuffer */
	fbdev = class_find_device(fb_class, NULL, NULL, is_device_ok);
	if (NULL == fbdev) {
		pr_debug("ERROR cannot get framebuffer device\n");
		return NULL;
	}
	return fbdev;
}

static struct fb_info *get_fb_info(struct device *fbdev)
{
	struct fb_info *fb_info;

	if (!fbdev->p) {
		pr_debug("ERROR framebuffer device has no private data\n");
		return NULL;
	}

	fb_info = (struct fb_info *) dev_get_drvdata(fbdev);
	if (!fb_info) {
		pr_debug("ERROR framebuffer device has no fb_info\n");
		return NULL;
	}

	return fb_info;
}

#if 0
static void enable_clocks(struct device *dev, char **clock_name,
		size_t nb_clock, int enable)
{
	int i;
	for (i = 0; i < nb_clock; i++) {
		struct clk *clock = clk_get(dev, clock_name[i]);
		pr_debug("clk_get returned 0x%08x\n", (int)clock);
		if (IS_ERR(clock)) {
			if (dev) {
				pr_err("Cannot clk_enable %s clock because I cannot get it, for device %s\n",
						clock_name[i],
						dev_name(dev));
			} else {
				pr_err("Cannot clk_enable %s clock because I cannot get it\n",
						other_clock_name[i]);
			}
		} else {
			if (enable)
				clk_enable(clock);
			else
				clk_disable(clock);
		}
	}
}
#endif 
static void blank_framebuffer(int getref)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct s3c_fb_win *win;
	struct s3c_fb *sfb;
	struct s3c_fb_platdata *pd;

	fbdev = get_fb_dev();
	if (!fbdev)
		return;

	fb_info = get_fb_info(fbdev);
	if (!fb_info)
		return;

	/*
	 * hold a reference to the dsim device, to prevent it from going into
	 * power management during tui session
	 */
	win = fb_info->par;
	sfb = win->parent;
	pd = sfb->pdata;

	/* Re-enable the clocks */
	//enable_clocks(fbdev->parent,
		//	disp_clock_name, ARRAY_SIZE(disp_clock_name), 1);
	//enable_clocks(NULL, other_clock_name, ARRAY_SIZE(other_clock_name), 1);

#if defined(CONFIG_SOC_EXYNOS5420)
	pm_runtime_get_sync(pd->dsim1_device);
#endif
	if (getref)
		pm_runtime_get_sync(sfb->dev);

	/* blank the framebuffer */
	lock_fb_info(fb_info);
	console_lock();
	fb_info->flags |= FBINFO_MISC_USEREVENT;
	fb_blank(fb_info, FB_BLANK_POWERDOWN);
	fb_info->flags &= ~FBINFO_MISC_USEREVENT;
	console_unlock();
	unlock_fb_info(fb_info);

#if defined(CONFIG_SOC_EXYNOS5420)
	/*
	 * part of the init of dsim device is not done in the TEE.  So do it
	 * here
	 */
	gpio_request_one(EXYNOS5420_GPF1(6),
			GPIOF_OUT_INIT_HIGH, "GPIO_MIPI_18V_EN");
	usleep_range(5000, 6000);
	gpio_free(EXYNOS5420_GPF1(6));
#endif

}

static void unblank_framebuffer(int releaseref)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct s3c_fb_win *win;
	struct s3c_fb *sfb;
	struct s3c_fb_platdata *pd;

	fbdev = get_fb_dev();
	if (!fbdev)
		return;

	fb_info = get_fb_info(fbdev);
	if (!fb_info)
		return;

	/*
	 * Release the reference we took at the beginning of the TUI session
	 */
	win = fb_info->par;
	sfb = win->parent;
	pd = sfb->pdata;

	/*
	 * Unblank the framebuffer
	 */
	console_lock();
	fb_info->flags |= FBINFO_MISC_USEREVENT;
	fb_blank(fb_info, FB_BLANK_UNBLANK);
	fb_info->flags &= ~FBINFO_MISC_USEREVENT;
	console_unlock();

	if (releaseref)
		pm_runtime_put_sync(sfb->dev);
#if defined(CONFIG_SOC_EXYNOS5420)
	pm_runtime_put_sync(pd->dsim1_device);
#endif
	/* We longer need the clock to be up */
	//enable_clocks(NULL, other_clock_name, ARRAY_SIZE(other_clock_name), 0);
	//enable_clocks(fbdev->parent,
			//disp_clock_name, ARRAY_SIZE(disp_clock_name), 0);
}
#endif

/* ------------------------------------------------------------- */
static void tlcProcessCmd(void)
{
	uint32_t ret = TUI_DCI_ERR_INTERNAL_ERROR;
	uint32_t commandId = CMD_TUI_SW_NONE;

	if (NULL == pDci) {
		pr_debug("ERROR tlcProcessCmd: DCI has not been set up properly - exiting\n");
		return;
	} else {
		commandId = pDci->cmdNwd.id;
	}

	/* Warn if previous response was not acknowledged */
	if (CMD_TUI_SW_NONE == commandId) {
		pr_debug("ERROR tlcProcessCmd: Notified without command\n");
		return;
	} else {
		if (pDci->nwdRsp.id != CMD_TUI_SW_NONE)
			pr_debug("tlcProcessCmd: Warning, previous response not ack\n");
	}

	/* Handle command */
	switch (commandId) {
	case CMD_TUI_SW_OPEN_SESSION:
		pr_debug("tlcProcessCmd: CMD_TUI_SW_OPEN_SESSION.\n");

		/* Start android TUI activity */
		ret = sendCmdToUser(TLC_TUI_CMD_START_ACTIVITY);
		if (TUI_DCI_OK == ret) {

			/* allocate TUI frame buffer */
			if (!allocateTuiBuffer(pDci))
				ret = TUI_DCI_ERR_INTERNAL_ERROR;
		}

		break;

	case CMD_TUI_SW_STOP_DISPLAY:
		pr_debug("tlcProcessCmd: CMD_TUI_SW_STOP_DISPLAY.\n");

#ifndef CONFIG_SOC_EXYNOS5420
		/* Set linux TUI flag */
		trustedui_set_mask(TRUSTEDUI_MODE_TUI_SESSION);
#endif
		trustedui_blank_set_counter(0);
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
		blank_framebuffer(1);
		disable_irq(gpio_to_irq(190));
#endif

		trustedui_set_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);

		ret = TUI_DCI_OK;
		break;
		
	case CMD_TUI_SW_CLOSE_SESSION:
		pr_debug("tlcProcessCmd: CMD_TUI_SW_CLOSE_SESSION.\n");

		freeTuiBuffer();
		// Protect NWd
		trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
		pr_info("Unblanking");
		enable_irq(gpio_to_irq(190));
		unblank_framebuffer(1);
#endif

		/* Clear linux TUI flag */
		trustedui_set_mode(TRUSTEDUI_MODE_OFF);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
		pr_info("Unsetting TUI flag (blank counter=%d)", trustedui_blank_get_counter());
		if (0 < trustedui_blank_get_counter()) {
			blank_framebuffer(0);		
		}
#endif

		/* Stop android TUI activity */
		ret = sendCmdToUser(TLC_TUI_CMD_STOP_ACTIVITY);
		break;

	default:
		pr_debug("ERROR tlcProcessCmd: Unknown command %d\n",
				commandId);
		break;
	}

	/* Fill in response to SWd, fill ID LAST */
	pr_debug("tlcProcessCmd: return 0x%08x to cmd 0x%08x\n",
			ret, commandId);
	pDci->nwdRsp.returnCode = ret;
	pDci->nwdRsp.id = RSP_ID(commandId);

	/* Acknowledge command */
	pDci->cmdNwd.id = CMD_TUI_SW_NONE;

	/* Notify SWd */
	pr_debug("DCI RSP NOTIFY CORE\n");
	ret = mc_notify(&drSessionHandle);
	if (MC_DRV_OK != ret)
		pr_debug("ERROR tlcProcessCmd: Notify failed: %d\n", ret);
}


/* ------------------------------------------------------------- */
static void tlcCloseDriver(void)
{
	enum mc_result ret;

	/* Close session with the Driver */
	ret = mc_close_session(&drSessionHandle);
	if (MC_DRV_OK != ret) {
		pr_debug("ERROR tlcCloseDriver: Closing driver session failed: %d\n",
				ret);
	}
}


/* ------------------------------------------------------------- */
static void tlcClose(void)
{
	enum mc_result ret;

	pr_debug("tlcClose: Closing driver session\n");
	tlcCloseDriver();

	pr_debug("tlcClose: Closing tbase\n");
	/* Close the tbase device */
	ret = mc_close_device(DEVICE_ID);
	if (MC_DRV_OK != ret) {
		pr_debug("ERROR tlcClose: Closing tbase device failed: %d\n",
			ret);
	}
}

/* ------------------------------------------------------------- */
bool tlcNotifyEvent(uint32_t eventType)
{
	bool ret = false;
	enum mc_result result;

	if (NULL == pDci) {
		pr_debug("ERROR tlcNotifyEvent: DCI has not been set up properly - exiting\n");
		return false;
	}

	/* Wait for previous notification to be acknowledged */
	while (pDci->nwdNotif != NOT_TUI_NONE) {
		pr_debug("TLC waiting for previous notification ack\n");
		usleep_range(10000, 10000);
	};

	/* Prepare notification message in DCI */
	pr_debug("tlcNotifyEvent: eventType = %d\n", eventType);
	pDci->nwdNotif = eventType;

	/* Signal the Driver */
	pr_debug("DCI EVENT NOTIFY CORE\n");
	result = mc_notify(&drSessionHandle);
	if (MC_DRV_OK != result) {
		pr_debug("ERROR tlcNotifyEvent: mcNotify failed: %d\n", result);
		ret = false;
	} else {
		ret = true;
	}

	return ret;
}

/* ------------------------------------------------------------- */
/**
 */
int mainThread(void *uarg)
{

	pr_debug("mainThread: TlcTui start!\n");

	/* Open session on the driver */
	if (!tlcOpen()) {
		pr_debug("ERROR mainThread: open driver failed!\n");
		return 1;
	}

	/* TlcTui main thread loop */
	for (;;) {
		/* Wait for a command from the DrTui on DCI*/
		tlcWaitCmdFromDriver();
		/* Something has been received, process it. */
		tlcProcessCmd();
	}

	/* Close tlc. Note that this frees the DCI pointer.
	 * Do not use this pointer after tlcClose().*/
	tlcClose();

	return 0;
}

/** @} */
