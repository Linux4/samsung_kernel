/* arch/arm/plat-s3c/include/plat/udc-hs.h
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * S3C USB2.0 High-speed / OtG platform information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

enum s3c_hsotg_dmamode {
        S3C_HSOTG_DMA_NONE,     /* do not use DMA at-all */
        S3C_HSOTG_DMA_ONLY,     /* always use DMA */
        S3C_HSOTG_DMA_DRV,      /* DMA is chosen by driver */
};

/**
 * struct s3c_hsotg_plat - platform data for high-speed otg/udc
 * @dma: Whether to use DMA or not.
 * @is_osc: The clock source is an oscillator, not a crystal
 */
/* USB Device (Gadget)*/
#if 0
static struct resource s3c_usbgadget_resource[] = {
 
       [0] = {
                .start = S3C24XX_PA_USBDEV,
                .end   = S3C24XX_PA_USBDEV + S3C24XX_SZ_USBDEV - 1,
                .flags = IORESOURCE_MEM,
        },
        [1] = {
                .start = IRQ_USBD,
                .end   = IRQ_USBD,
                .flags = IORESOURCE_IRQ,
        }

};

struct platform_device s3c_device_usbgadget = {

        .name             = "s3c2410-usbgadget",
        .id               = -1,
        .num_resources    = ARRAY_SIZE(s3c_usbgadget_resource),
        .resource         = s3c_usbgadget_resource,
};
#endif
//EXPORT_SYMBOL(s3c_device_usbgadget);


typedef enum usb_cable_status {
        USB_CABLE_DETACHED = 0,
        USB_CABLE_ATTACHED,
        USB_OTGHOST_DETACHED,
        USB_OTGHOST_ATTACHED,
        USB_POWERED_HOST_DETACHED,
        USB_POWERED_HOST_ATTACHED,
        USB_CABLE_DETACHED_WITHOUT_NOTI,
} usb_cable_status;
