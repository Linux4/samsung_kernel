/****************************************************************************
*
* gc_hal_kernel_sysfs_test.c
*
* Author: Watson Wang <zswang@marvell.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version. *
*
*****************************************************************************/

#include "gc_hal_kernel_linux.h"
#include "gc_hal_kernel_sysfs.h"

#if MRVL_CONFIG_POWER_VALIDATION
#include <linux/sysfs.h>

static gckGALDEVICE galDevice = NULL;
static struct kobject *gpu_kobj = NULL;

static ssize_t show_power_state (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "[pm_test] power_state usage:\n"
                        "\techo state,core,broadcast > /sys/devices/.../pm_test/state\n"
                        "\tstate=0, 1, 2 (0:on, 1:off, 2:suspend)\n"
                        "\tcore=0,1\n"
                        "\tbroadcast=0,1\n");
}

static ssize_t store_power_state (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int state, core, broadcast, gpu_count, i;
    gceSTATUS status;
    gctUINT32 tgid;

    /* count ocre numbers */
    for (i=0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
        if (galDevice->kernels[i] != gcvNULL)
            gpu_count++;

    /* scan input value and verify */
    SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%d,%d", &state, &core, &broadcast), 3);
    SYSFS_VERIFY_INPUT_RANGE(state, 0, 2);
    SYSFS_VERIFY_INPUT_RANGE(core, 0, (gpu_count-1));
    SYSFS_VERIFY_INPUT_RANGE(broadcast, 0, 1);

    gckOS_GetProcessID(&tgid);

    switch (state)
    {
        case 0:
            /* on */
            printk("[pm_test %d] %s core power state on\n", tgid, (core == gcvCORE_MAJOR)?"3D":"2D");
            if (broadcast)
                /* noting done here */
                gcmkONERROR(gckOS_Broadcast(galDevice->kernels[core]->os, galDevice->kernels[core]->hardware, gcvBROADCAST_FIRST_PROCESS));
            else
                gcmkONERROR(gckHARDWARE_SetPowerManagementState(galDevice->kernels[core]->hardware, gcvPOWER_ON));
            printk("[pm_test %d] %s core power state on - done\n", tgid, (core == gcvCORE_MAJOR)?"3D":"2D");
            break;

        case 1:
            /* off */
            printk("[pm_test %d] %s core power state off\n", tgid, (core == gcvCORE_MAJOR)?"3D":"2D");
            if (broadcast)
                gcmkONERROR(gckOS_Broadcast(galDevice->kernels[core]->os, galDevice->kernels[core]->hardware, gcvBROADCAST_LAST_PROCESS));
            else
                gcmkONERROR(gckHARDWARE_SetPowerManagementState(galDevice->kernels[core]->hardware, gcvPOWER_OFF));
            printk("[pm_test %d] %s core power state off - done\n", tgid, (core == gcvCORE_MAJOR)?"3D":"2D");
            break;

        case 2:
            /* suspend */
            printk("[pm_test %d] %s core power state suspend\n", tgid, (core == gcvCORE_MAJOR)?"3D":"2D");
            if (broadcast)
                gcmkONERROR(gckOS_Broadcast(galDevice->kernels[core]->os, galDevice->kernels[core]->hardware, gcvBROADCAST_GPU_IDLE));
            else
                gcmkONERROR(gckHARDWARE_SetPowerManagementState(galDevice->kernels[core]->hardware, gcvPOWER_SUSPEND));
            printk("[pm_test %d] %s core power state suspend - done\n", tgid, (core == gcvCORE_MAJOR)?"3D":"2D");
            break;

        default:
            break;
    }
    return count;

OnError:
    printk("[pm_test %d] %s core power state %s - bail out\n", tgid, (core == gcvCORE_MAJOR)?"3D":"2D", (state==1)?"off":"on");
    return (ssize_t)-EINVAL;

}


static ssize_t show_reset (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "[pm_test] reset usage:\n"
                        "\techo [0|1] > /sys/devices/.../pm_test/reset\n");
}

static ssize_t store_reset (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int core, gpu_count, i;
    gceSTATUS status;

    /* count core numbers */
    for (i = 0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
        if (galDevice->kernels[i] != gcvNULL)
            gpu_count++;

    /* scan input value and verify */
    SYSFS_VERIFY_INPUT(sscanf(buf, "%d", &core), 1);
    SYSFS_VERIFY_INPUT_RANGE(core, 0, (gpu_count-1));

    /* reset */
    printk("[pm_test] reset %s core\n", (core == gcvCORE_MAJOR)?"3D":"2D");
    gcmkONERROR(gckKERNEL_Recovery(galDevice->kernels[core]));

    return count;

OnError:
    return (ssize_t)-EINVAL;
}

static ssize_t show_clkreg (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    char *str = buf;
    int i;
    unsigned int clkreg;
    gckHARDWARE hardware;
    gceSTATUS status;

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            hardware = galDevice->kernels[i]->hardware;
            /* read clk control register */
            gcmkONERROR(gckOS_ReadRegisterEx(hardware->os, hardware->core, 0x00000, &clkreg));
            str += sprintf(str, "%x ", clkreg);
        }
    }

    if (str != buf)
    {
        *(str-1) = '\n';
    }

    return (str-buf);

OnError:
    return 0;

}

static ssize_t show_idle (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    char *str = buf;
    gceSTATUS status;
    gctBOOL isIdle;
    int i;

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gcmkONERROR(gckHARDWARE_QueryIdle(galDevice->kernels[i]->hardware, &isIdle));
            str += sprintf(str, "%s ", (gcvTRUE == isIdle)?"idle":"busy");
        }
    }

    if (str != buf)
    {
        *(str-1) = '\n';
    }

    return (str-buf);

OnError:
    return 0;
}

static ssize_t show_freq (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "[pm_test] reset usage:\n"
                        "\techo [0|1],[scale] > /sys/devices/.../pm_test/freq\n");

}

static ssize_t store_freq (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int core, scale, gpu_count, i;
    gceSTATUS status;
    gckHARDWARE hardware;

    /* count core numbers */
    for (i = 0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
        if (galDevice->kernels[i] != gcvNULL)
            gpu_count++;

    /* scan input value and verify */
    SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%d", &core, &scale), 2);
    SYSFS_VERIFY_INPUT_RANGE(core, 0, (gpu_count-1));
    SYSFS_VERIFY_INPUT_RANGE(scale, 1, 64);

    /* internal frequency scaling */
    printk("[pm_test] freq %s core 1/%d\n", (core == gcvCORE_MAJOR)?"3D":"2D", scale);
    hardware = galDevice->kernels[core]->hardware;

    switch (scale)
    {
        case 1:
            /* frequency change to full speed */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x300));
            /* Loading the frequency scaler. */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x100));
            break;

        case 2:
            /* frequency change to 1/2 speed */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x280));
            /* Loading the frequency scaler. */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x080));
            break;

        case 4:
            /* frequency change to 1/4 speed */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x240));
            /* Loading the frequency scaler. */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x040));
            break;

        case 8:
            /* frequency change to 1/8 speed */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x220));
            /* Loading the frequency scaler. */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x020));
            break;

        case 16:
            /* frequency change to 1/16 speed */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x210));
            /* Loading the frequency scaler. */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x010));
            break;

        case 32:
            /* frequency change to 1/32 speed */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x208));
            /* Loading the frequency scaler. */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x008));
            break;

        case 64:
            /* frequency change to 1/64 speed */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x204));
            /* Loading the frequency scaler. */
            gcmkONERROR(gckOS_WriteRegisterEx(hardware->os, hardware->core, 0x00000, 0x004));
            break;

        default:
            gcmkPRINT("[pm_test]: freq unknown scaler\n");
            break;
    }

    return count;

OnError:
    return (ssize_t)-EINVAL;

}

static ssize_t show_mutex (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "[pm_test] mutex usage:\n"
                            "\techo [0|1],[time] > /sys/devices/.../pm_test/mutex\n");

}

static ssize_t store_mutex (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int core, time, gpu_count, i;
    gceSTATUS status;
    gckHARDWARE hardware;

    /* count core numbers */
    for (i = 0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
        if (galDevice->kernels[i] != gcvNULL)
            gpu_count++;

    /* scan input value and verify */
    SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%d", &core, &time), 2);
    SYSFS_VERIFY_INPUT_RANGE(core, 0, 1);
    SYSFS_VERIFY_INPUT_RANGE(time, 0, 1000);

    /* lock recMutexPower for some time */
    printk("[pm_test] mutex lock %s core %d\n", (core == gcvCORE_MAJOR)?"3D":"2D", time);
    hardware = galDevice->kernels[core]->hardware;

    /* Grab the mutex. */
    gcmkONERROR(gckOS_AcquireRecMutex(hardware->os, hardware->recMutexPower, gcvINFINITE));
    /* sleep some time */
    gcmkONERROR(gckOS_Delay(hardware->os, time));
    /* Release the mutex */
    gcmkONERROR(gckOS_ReleaseRecMutex(hardware->os, hardware->recMutexPower));

    return count;

OnError:
    return (ssize_t)-EINVAL;
}

gc_sysfs_attr_rw(power_state);
gc_sysfs_attr_rw(reset);
gc_sysfs_attr_ro(clkreg);
gc_sysfs_attr_ro(idle);
gc_sysfs_attr_rw(freq);
gc_sysfs_attr_rw(mutex);

static struct attribute *gc_pm_test_attrs[] = {
    &gc_attr_power_state.attr,
    &gc_attr_reset.attr,
    &gc_attr_clkreg.attr,
    &gc_attr_idle.attr,
    &gc_attr_freq.attr,
    &gc_attr_mutex.attr,
    NULL
};

static struct attribute_group gc_pm_test_attr_group = {
    .attrs = gc_pm_test_attrs,
    .name = "pm_test"
};

int create_sysfs_file_pm_test(struct platform_device *pdev, struct kobject *kobj)
{
    int ret = 0;

    galDevice = (gckGALDEVICE) platform_get_drvdata(pdev);
    if(!galDevice)
    {
        printk("error: failed in getting drvdata\n");
        return -1;
    }

    if(!kobj)
    {
        printk("error: failed to get kobject of gpu\n");
        return -1;
    }

    gpu_kobj = kobj;

    if((ret = sysfs_create_group(gpu_kobj, &gc_pm_test_attr_group)))
        printk("fail to create sysfs group %s\n", gc_pm_test_attr_group.name);

    return ret;
}

void remove_sysfs_file_pm_test(struct platform_device *pdev)
{
    if(!galDevice)
        return;

    if(!gpu_kobj)
    return;

    sysfs_remove_group(gpu_kobj, &gc_pm_test_attr_group);
}


#endif
