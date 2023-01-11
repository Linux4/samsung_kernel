/****************************************************************************
*
* gc_hal_kernel_sysfs.c
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

#if MRVL_CONFIG_SYSFS
#include <linux/sysfs.h>

static gckGALDEVICE galDevice = NULL;
static struct device * parent_device = NULL;

static inline int __create_sysfs_file_debug(void);
static inline void __remove_sysfs_file_debug(void);

static struct kset *kset_gpu = NULL;
static int registered_debug = 0;
static int registered_common = 0;
static int registered_gpufreq = 0;
static int registered_pm_test = 0;

static const char* const _core_desc[] = {
    [gcvCORE_MAJOR]     = "3D",
    [gcvCORE_2D]        = "2D",
    [gcvCORE_VG]        = "VG",
};

static const char* const _power_states[] = {
    [gcvPOWER_ON]       = "ON",
    [gcvPOWER_OFF]      = "OFF",
    [gcvPOWER_IDLE]     = "IDLE",
    [gcvPOWER_SUSPEND]  = "SUSPEND",
};

/* *********************************************************** */
static ssize_t show_pm_state (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    gceSTATUS status;
    gceCHIPPOWERSTATE states;
    int i, len = 0;

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            status = gckHARDWARE_QueryPowerManagementState(
                            galDevice->kernels[i]->hardware,
                            &states);

            if (gcmIS_ERROR(status))
            {
                len += sprintf(buf+len, "[%s] querying chip power state failed\n", _core_desc[i]);
                continue;
            }

            len += sprintf(buf+len, "[%s] current chipPowerState = %s\n", _core_desc[i], _power_states[states]);
        }
    }

    len += sprintf(buf+len, "\n* Usage:\n"
                            " $ cat /sys/devices/.../pm_state\n"
                            " $ echo [core],[state] > /sys/devices/.../pm_state\n"
                            "   e.g. core[3D] power [ON]\n"
                            " $ echo 0,0 > /sys/devices/.../pm_state\n"
                            );

    len += sprintf(buf+len, "* Core options:\n");
    if (galDevice->kernels[gcvCORE_MAJOR] != gcvNULL)
        len += sprintf(buf+len, " - %d   core [%s]\n", gcvCORE_MAJOR, _core_desc[gcvCORE_MAJOR]);
    if (galDevice->kernels[gcvCORE_2D] != gcvNULL)
        len += sprintf(buf+len, " - %d   core [%s]\n", gcvCORE_2D, _core_desc[gcvCORE_2D]);
    if (galDevice->kernels[gcvCORE_VG] != gcvNULL)
        len += sprintf(buf+len, " - %d   core [%s]\n", gcvCORE_VG, _core_desc[gcvCORE_VG]);

    len += sprintf(buf+len, "* Power state options:\n"
                            " - 0   to power [   ON  ]\n"
                            " - 1   to power [  OFF  ]\n"
                            " - 2   to power [  IDLE ]\n"
                            " - 3   to power [SUSPEND]\n");

    return len;
}

static ssize_t store_pm_state (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int core, state, i, gpu_count;
    gceSTATUS status;

    /* count core numbers */
    for (i = 0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
        if (galDevice->kernels[i] != gcvNULL)
            gpu_count++;

    /* read input and verify */
    SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%d", &core, &state), 2);
    SYSFS_VERIFY_INPUT_RANGE(core, 0, (gpu_count-1));
    SYSFS_VERIFY_INPUT_RANGE(state, 0, 3);

    /* power state transition */
    status = gckHARDWARE_SetPowerManagementState(galDevice->kernels[core]->hardware, state);
    if (gcmIS_ERROR(status))
    {
        printk("[%d] failed in transfering power state to %d\n", core, state);
    }

    return count;
}

static ssize_t show_clock_gating_state (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    int len = 0;

#if 1 
    int i;
    gctUINT32 state, data1, data2;
    gckHARDWARE hardware;

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            len += sprintf(buf+len, "[%s] clock gating state:\n", _core_desc[i]);

            hardware = galDevice->kernels[i]->hardware;
            state = hardware->chipClockGatingState;
            len += sprintf(buf+len, "\tExpected: %s 0x%x\n",
                            state&0x80000000?(state&0x40000000?"disabled":"enabled"):"default",
                            state&0x3FFFFFFF);

            gcmkVERIFY_OK(
                gckOS_ReadRegisterEx(hardware->os,
                                     hardware->core,
                                     hardware->powerBaseAddress +
                                     0x00100,
                                     &data1));
	data1 = (((((gctUINT32)(data1)) >> (0 ? 0:0)) &((gctUINT32) ((((1 ? 0:0) - (0 ? 0:0) + 1) == 32) ? ~0 : (~(~0 << ((1 ? 0:0) - (0 ? 0:0) + 1)))))));
	            gcmkVERIFY_OK(
                gckOS_ReadRegisterEx(hardware->os,
                                     hardware->core,
                                     hardware->powerBaseAddress +
                                     0x00104,
                                     &data2));
            len += sprintf(buf+len, "\tActual:   %s 0x%x\n", data1?"enabled":"disabled", data2);
        }
    }
#else
    len += sprintf(buf+len, "GC module power contorl is disabled.\n");

#endif

    return len;
}

static ssize_t store_clock_gating_state (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int core, i, gpu_count;
    gctUINT32 state;

    /* count core numbers */
    for (i = 0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gpu_count++;
        }
    }

    /* Input: core,clock gating set. */
    SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%x", &core, &state), 2);
    SYSFS_VERIFY_INPUT_RANGE(core, 0, (gpu_count-1));

    galDevice->kernels[core]->hardware->chipClockGatingState = state;

    return count;
}

static ssize_t show_profiler_debug (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "%d\n", galDevice->profilerDebug);
}

static ssize_t store_profiler_debug (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int value;

    SYSFS_VERIFY_INPUT(sscanf(buf, "%d", &value), 1);
    SYSFS_VERIFY_INPUT_RANGE(value, 0, 1);

    galDevice->profilerDebug = value;

    return count;
}

static ssize_t show_power_debug (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "%d\n", galDevice->powerDebug);
}

static ssize_t store_power_debug (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int value;

    SYSFS_VERIFY_INPUT(sscanf(buf, "%d", &value), 1);
    SYSFS_VERIFY_INPUT_RANGE(value, 0, 1);

    galDevice->powerDebug = value;

    return count;
}

static ssize_t show_runtime_debug (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "%d\n", galDevice->pmrtDebug);
}

static ssize_t store_runtime_debug (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int value;

    SYSFS_VERIFY_INPUT(sscanf(buf, "%d", &value), 1);
    SYSFS_VERIFY_INPUT_RANGE(value, 0, 2);

    galDevice->pmrtDebug = value;

    return count;
}

static ssize_t show_show_commands (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "Current value: %d\n"
                        "options:\n"
                        " 0    disable show commands\n"
                        " 1    show 3D commands\n"
                        " 2    show 2D commands\n"
                        " 3    show 2D&3D commands\n"
#if gcdENABLE_VG
                        " 4    show VG commands\n"

#endif
                        ,galDevice->printPID);
}

static ssize_t store_show_commands (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int value;

    SYSFS_VERIFY_INPUT(sscanf(buf, "%d", &value), 1);
#if gcdENABLE_VG
    /* 3D, 2D, 2D&3D, VG. */
    SYSFS_VERIFY_INPUT_RANGE(value, 0, 4);
#else
    /* 3D, 2D, 2D&3D. */
    SYSFS_VERIFY_INPUT_RANGE(value, 0, 3);

#endif

    galDevice->printPID = value;

    return count;
}

static ssize_t show_register_stats (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    gckKERNEL kernel = gcvNULL;
    gctUINT32 clockControl, clockRate, idle, len = 0, i = 0;
    gctBOOL isIdle;

    for(; i< gcdMAX_GPU_COUNT; i++)
    {
        kernel = galDevice->kernels[i];

        if(kernel != gcvNULL)
        {
            if(i == gcvCORE_MAJOR)
            {
                gcmkVERIFY_OK(gckOS_DirectReadRegister(galDevice->os, gcvCORE_MAJOR, 0x00000, &clockControl));
                len += sprintf(buf+len, "clock register: [0x%02x]\n", clockControl);
                if(has_feat_3d_shader_clock())
                {
                    gctUINT32 shClkRate;

                    gcmkVERIFY_OK(gckOS_QueryClkRate(galDevice->os, gcvCORE_SH, gcvFALSE, &shClkRate));
                    len += sprintf(buf+len, "shader clock rate: [%d] MHz\n", (gctUINT32)shClkRate/1000/1000);
                }
            }

            len += sprintf(buf+len, "[%s]\n", _core_desc[i]);
            gcmkVERIFY_OK(gckHARDWARE_QueryIdleEx(kernel->hardware, &idle, &idle, &isIdle));
            gcmkVERIFY_OK(gckOS_QueryClkRate(galDevice->os, i, gcvFALSE, &clockRate));

            len += sprintf(buf+len, "   idle register: [0x%02x][%s]\n",
                       idle, (gcvTRUE == isIdle)?"idle":"busy");
            len += sprintf(buf+len, "   clock rate: [%d] MHz\n", (gctUINT32)clockRate/1000/1000);
         }

    }

    return len +=sprintf(buf+len, "options:\n"
                                  "   echo Core 0xAddr > register_stats\n"
                                  "   e.g: echo 0 0x664 > register_stats\n"
                                  "     # 0 means core 0\n"
                                  "     # 0x664 means register address\n");
}

static ssize_t store_register_stats (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    gctUINT32 core, offset, value;

    SYSFS_VERIFY_INPUT(sscanf(buf, "%d 0x%x", &core, &offset), 2);
    SYSFS_VERIFY_INPUT_RANGE(core, 0, gcdMAX_GPU_COUNT - 1);
    SYSFS_VERIFY_INPUT_RANGE(offset, 0, 0x30001);

    if(gcvCORE_SH == core)
    {
        core = gcvCORE_MAJOR;
    }

    gcmkVERIFY_OK(gckOS_ReadRegisterEx(galDevice->os, core, offset, &value));

    gcmkPRINT("Core(%d) Register[0x%x] value is 0x%08x\n", core, offset, value);

    return count;
}


static ssize_t show_clk_off_when_idle (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "options:\n"
                        " 1          enable clock off when idle\n"
                        " 0          disable clock off when idle\n");
}

static ssize_t store_clk_off_when_idle (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int value;

    SYSFS_VERIFY_INPUT(sscanf(buf, "%d", &value), 1);

    if(value == 1)
    {
        gcmkPRINT("galcore: enable clock off when idle");

        gcmkVERIFY_OK(gckOS_SetClkOffWhenIdle(galDevice->os, gcvTRUE));
    }
    else if(value == 0)
    {
        gcmkPRINT("galcore: disable clock off when idle");

        gcmkVERIFY_OK(gckOS_SetClkOffWhenIdle(galDevice->os, gcvFALSE));
    }
    else
    {
        gcmkPRINT("Invalid argument");
        return count;
    }

    return count;
}

gc_sysfs_attr_rw(clk_off_when_idle);

#if gcdPOWEROFF_TIMEOUT
static ssize_t show_poweroff_timeout (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    gctUINT32 timeout, i;
    ssize_t len = 0;

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gcmkVERIFY_OK(gckHARDWARE_QueryPowerOffTimeout(
                                    galDevice->kernels[i]->hardware,
                                    &timeout));

            len += sprintf(buf+len, "[%s] poweroff_timeout = %d ms\n", _core_desc[i], timeout);
        }
    }

    len += sprintf(buf+len, "\n* Usage:\n"
                            "  $ cat /sys/devices/.../poweroff_timeout\n"
                            "  $ echo [core],[timeout] > /sys/devices/.../poweroff_timeout\n"
                            );
    return len;
}

static ssize_t store_poweroff_timeout (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int core, timeout, i, gpu_count;

    /* count core numbers */
    for (i = 0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
        if (galDevice->kernels[i] != gcvNULL)
            gpu_count++;

    /* read input and verify */
    SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%d", &core, &timeout), 2);
    SYSFS_VERIFY_INPUT_RANGE(core, 0, (gpu_count-1));
    SYSFS_VERIFY_INPUT_RANGE(timeout, 0, 3600000);

    gcmkVERIFY_OK(gckHARDWARE_SetPowerOffTimeout(
                            galDevice->kernels[core]->hardware,
                            timeout));

    return count;
}

gc_sysfs_attr_rw(poweroff_timeout);

static ssize_t show_poweroff_timeout_enable (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    int len = 0, i;
    gctBOOL enable;

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gcmkVERIFY_OK(gckHARDWARE_QueryPowerOffTimeoutEnable(
                                    galDevice->kernels[i]->hardware,
                                    &enable));

            len += sprintf(buf+len, "[%s] poweroff_timeout_enable = %d\n", _core_desc[i], enable);
        }
    }

    len += sprintf(buf+len, "\n* Usage:\n"
                            " $ cat /sys/devices/.../poweroff_timeout_enable\n"
                            " $ echo [core],[state] > /sys/devices/.../poweroff_timeout_enable\n"
                            " $   e.g. echo 0,1 > /sys/devices/.../poweroff_timeout_enable\n"
                            );

    len += sprintf(buf+len, "* Core options:\n");
    if (galDevice->kernels[gcvCORE_MAJOR] != gcvNULL)
        len += sprintf(buf+len, " - %d   core [%s]\n", gcvCORE_MAJOR, _core_desc[gcvCORE_MAJOR]);
    if (galDevice->kernels[gcvCORE_2D] != gcvNULL)
        len += sprintf(buf+len, " - %d   core [%s]\n", gcvCORE_2D, _core_desc[gcvCORE_2D]);
    if (galDevice->kernels[gcvCORE_SH] != gcvNULL)
        len += sprintf(buf+len, " - %d   core [%s]\n", gcvCORE_SH, _core_desc[gcvCORE_SH]);

    len += sprintf(buf+len, "* State options:\n"
                            " - 0   to disable power off when idle timeout\n"
                            " - 1   to enable power off when idle timeout\n");

    return len;
}

static ssize_t store_poweroff_timeout_enable (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int core, enable, i, gpu_count;

    /* count core numbers */
    for (i = 0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
        if (galDevice->kernels[i] != gcvNULL)
            gpu_count++;

    /* read input and verify */
    SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%d", &core, &enable), 2);
    SYSFS_VERIFY_INPUT_RANGE(core, 0, (gpu_count-1));
    SYSFS_VERIFY_INPUT_RANGE(enable, 0, 1);

    if((galDevice->kernels[core] != gcvNULL)
        && (galDevice->kernels[core]->hardware != gcvNULL))
    {
        gcmkVERIFY_OK(gckHARDWARE_SetPowerOffTimeoutEnable(
                                galDevice->kernels[core]->hardware,
                                enable ? gcvTRUE: gcvFALSE));
    }

    return count;
}

gc_sysfs_attr_rw(poweroff_timeout_enable);

#endif

static ssize_t show_clk_rate (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    gceSTATUS status;
    unsigned int clockRate = 0;
    int i = 0, len = 0;

    for(i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(galDevice->kernels[i] != gcvNULL)
        {
            status = gckOS_QueryClkRate(galDevice->os, i, gcvFALSE, &clockRate);
            if(status == gcvSTATUS_OK)
            {
                len += sprintf(buf+len, "[%s] current frequency: %u MHZ\n", _core_desc[i], clockRate/1000);
            }
            else
            {
                len += sprintf(buf+len, "[%s] failed to get clock rate\n", _core_desc[i]);
            }

            if(has_feat_3d_shader_clock()
               && (i == gcvCORE_MAJOR))
            {
                unsigned int shClkRate = 0;

                status = gckOS_QueryClkRate(galDevice->os, gcvCORE_SH, gcvFALSE, &shClkRate);
                if(status == gcvSTATUS_OK)
                {
                    len += sprintf(buf+len, "[%s] current frequency: %u MHZ\n", "SH", shClkRate/1000);
                }
                else
                {
                    len += sprintf(buf+len, "[%s] failed to get clock rate\n", "SH");
                }
            }
        }
    }

    if(has_feat_axi_freq_change())
    {
        unsigned int axiClkRate = 0;

        status = gckOS_QueryClkRate(galDevice->os, gcvCORE_MAJOR, gcvTRUE, &axiClkRate);
        if(status == gcvSTATUS_OK)
        {
            len += sprintf(buf+len, "[%s] current frequency: %u MHZ\n", "AXI", axiClkRate/1000);
        }
        else
        {
            len += sprintf(buf+len, "[%s] failed to get clock rate\n", "AXI");
        }
    }

    return len;
}

static ssize_t store_clk_rate (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    gceSTATUS status;
    int core, frequency, i, gpu_count;
    int axi=0, min_freq = 156, max_freq = 624;

    for (i = 0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
        if (galDevice->kernels[i] != gcvNULL)
            gpu_count++;

    if (has_feat_3d_shader_clock())
        gpu_count++;

    /* read input and verify */
    if(has_feat_axi_freq_change())
    {
        SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%d,%d", &core, &frequency, &axi), 3);
    }
    else
    {
        SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%d", &core, &frequency), 2);
    }
    SYSFS_VERIFY_INPUT_RANGE(core, 0, (gpu_count-1));

    if(galDevice->max_freq[core] != 0 &&
        galDevice->min_freq[core] != 0)
    {
        min_freq = galDevice->min_freq[core];
        max_freq = galDevice->max_freq[core];
    }

    SYSFS_VERIFY_INPUT_RANGE(frequency, min_freq, max_freq);

    /* acquire clock mutex */
    if(has_feat_dfc_protect_clk_op())
    {
        int realcore = core;
        if(has_feat_3d_shader_clock() && core == gcvCORE_SH)
            realcore = gcvCORE_MAJOR;

        gcmkVERIFY_OK(gckOS_AcquireClockMutex(galDevice->os, realcore));
    }

    status = gckOS_SetClkRate(galDevice->os, core, axi, frequency*1000);

    if(gcmIS_ERROR(status))
    {
        printk("fail to set core[%d] frequency to %d MHZ\n", core, frequency);
    }

    /* release clock mutex */
    if(has_feat_dfc_protect_clk_op())
    {
        int realcore = core;
        if(has_feat_3d_shader_clock() && core == gcvCORE_SH)
            realcore = gcvCORE_MAJOR;

        gcmkVERIFY_OK(gckOS_ReleaseClockMutex(galDevice->os, realcore));
    }

    return count;
}

static ssize_t show_stuck_debug (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    int i = 0, len = 0;

    len += sprintf(buf+len, "stuck dump set:\n");

    for(i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(galDevice->kernels[i] != gcvNULL)
        {
            len += sprintf(buf+len, "\t[%s] : recovery is %s, Dump level is %u\n",
                        _core_desc[i],
                        galDevice->kernels[i]->recovery ? "enabled" : "disabled",
                        galDevice->kernels[i]->stuckDump);
        }
    }

    return len;
}

static ssize_t store_stuck_debug (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    int core, recovery, stuckDump;
    int i, gpu_count;

    for (i = 0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gpu_count++;
        }
    }

    SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%d,%d", &core, &recovery, &stuckDump), 3);

    SYSFS_VERIFY_INPUT_RANGE(core, 0, (gpu_count-1));
    SYSFS_VERIFY_INPUT_RANGE(recovery, 0, 1);
    SYSFS_VERIFY_INPUT_RANGE(stuckDump, 0, 4);

    galDevice->kernels[core]->recovery = recovery;

    if (recovery == gcvFALSE)
    {
        galDevice->kernels[core]->stuckDump = gcmMAX(stuckDump, gcvSTUCK_DUMP_NEARBY_MEMORY);
    }
    else
    {
        galDevice->kernels[core]->stuckDump = stuckDump;
    }

    return count;
}

static ssize_t show_command_debug (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    int i = 0, len = 0;

    len += sprintf(buf+len, "commandbuffer set:\n");

    for(i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(galDevice->kernels[i] != gcvNULL)
        {
            len += sprintf(buf+len, "\t[%s] : Dump level is %u\n trace print: %d\n process is:%d\n",
                        _core_desc[i],
                        galDevice->kernels[i]->commandLevel,
                        galDevice->kernels[i]->commandTraceprint,
                        galDevice->kernels[i]->commandProc);
        }
    }

    return len;
}

static ssize_t store_command_debug (struct device *dev,
    struct device_attribute *attr,
    const char *buf, size_t count)
{
    int core, level, trace, process;
    int i, gpu_count;

    for (i = 0, gpu_count = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gpu_count++;
        }
    }

    SYSFS_VERIFY_INPUT(sscanf(buf, "%d,%d,%d,%d", &core, &level, &trace, &process), 4);
    SYSFS_VERIFY_INPUT_RANGE(core, 0, (gpu_count-1));
    SYSFS_VERIFY_INPUT_RANGE(level, 0, 2);
    SYSFS_VERIFY_INPUT_RANGE(trace, 0, 1);
    galDevice->kernels[core]->commandLevel= level;
    galDevice->kernels[core]->commandTraceprint= trace;
    galDevice->kernels[core]->commandProc= process;

    return count;
}

gc_sysfs_attr_rw(pm_state);
gc_sysfs_attr_rw(profiler_debug);
gc_sysfs_attr_rw(power_debug);
gc_sysfs_attr_rw(runtime_debug);
gc_sysfs_attr_rw(show_commands);
gc_sysfs_attr_rw(register_stats);
gc_sysfs_attr_rw(clk_rate);
gc_sysfs_attr_rw(stuck_debug);
gc_sysfs_attr_rw(clock_gating_state);
gc_sysfs_attr_rw(command_debug);

static struct attribute *gc_debug_attrs[] = {
    &gc_attr_pm_state.attr,
    &gc_attr_profiler_debug.attr,
    &gc_attr_power_debug.attr,
    &gc_attr_runtime_debug.attr,
    &gc_attr_show_commands.attr,
    &gc_attr_register_stats.attr,
    &gc_attr_clk_rate.attr,
    &gc_attr_clk_off_when_idle.attr,
#if gcdPOWEROFF_TIMEOUT
    &gc_attr_poweroff_timeout.attr,
    &gc_attr_poweroff_timeout_enable.attr,

#endif
    &gc_attr_stuck_debug.attr,
    &gc_attr_clock_gating_state.attr,
    &gc_attr_command_debug.attr,
    NULL
};

static struct attribute_group gc_debug_attr_group = {
    .attrs = gc_debug_attrs,
    .name = "debug"
};

// -------------------------------------------------
static ssize_t show_mem_stats (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    gctUINT32 len = 0;
    gctUINT32 tmpLen = 0;
    int i = 0;

    gcmkVERIFY_OK(gckOS_ShowVidMemUsage(galDevice->os, buf, &len));

    len += sprintf(buf + len, "\n======== Memory usage discarding shared memory ========\n");

    for (i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if (galDevice->kernels[i] != gcvNULL)
        {
            gckKERNEL_ShowVidMemUsageDetails(galDevice->kernels[i],
                                             0,
                                             gcvSURF_NUM_TYPES,
                                             buf + len,
                                             &tmpLen);

            len += tmpLen;
        }
    }

    return (ssize_t)len;
}

static ssize_t store_mem_stats (struct device *dev,
    struct device_attribute *attr,
    const char *buf, size_t count)
{
    gctCHAR value1[16];
    gctINT value2 = -1;
    int i;

    sscanf(buf, "%s %d", value1, &value2);

    if(strstr(value1, "type"))
    {
        for (i = 0; i < gcdMAX_GPU_COUNT; i++)
        {
            if (galDevice->kernels[i] != gcvNULL)
            {
                gckKERNEL_ShowVidMemUsageDetails(galDevice->kernels[i], 0, value2, gcvNULL, gcvNULL);
            }
        }
    }
    else if(strstr(value1, "pid"))
    {
        for (i = 0; i < gcdMAX_GPU_COUNT; i++)
        {
            if (galDevice->kernels[i] != gcvNULL)
            {
                gckKERNEL_ShowVidMemUsageDetails(galDevice->kernels[i], 1, value2, gcvNULL, gcvNULL);
            }
        }
    }

    return count;
}

// TODO: finish *ticks* and *dutycycle* implementation
static ssize_t show_ticks (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "Oops, %s is under working\n", __func__);
}

static ssize_t store_ticks (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    printk("Oops, %s is under working\n", __func__);
    return count;
}

static ssize_t show_dutycycle (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    if(has_feat_pulse_eater_profiler())
    {
        return sprintf(buf, "options:\n"
                            "(1|0) (0|1)      Start/End 3D/2D\n"
                            "e.g.\n"
                            "step1 # echo 1 0 > /sys/.../dutycycle\n"
                            "step2 # echo 0 0 > /sys/.../dutycycle\n");
    }
    else
    {
        return sprintf(buf, "Oops, %s is under working\n", __func__);
    }
}

static ssize_t store_dutycycle (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    if(has_feat_pulse_eater_profiler())
    {
        gctBOOL start;
        gctUINT core;
        gctUINT32 busyRatio[4], totalTime[4], timeGap = 0;
        gcePulseEaterDomain domain = gcvPulse_Core;
        gceSTATUS status = gcvSTATUS_OK;

        SYSFS_VERIFY_INPUT(sscanf(buf, "%d %d", &start, &core), 2);
        SYSFS_VERIFY_INPUT_RANGE(start, 0, 1);
#if gcdENABLE_VG
        SYSFS_VERIFY_INPUT_RANGE(core, 0, 2);
#else
        SYSFS_VERIFY_INPUT_RANGE(core, 0, 1);

#endif

        for(; domain < 2; domain++)
        {
            status = gckKERNEL_QueryPulseEaterIdleProfile(galDevice->kernels[core],
                                                          start,
                                                          domain,
                                                          busyRatio,
                                                          &timeGap,
                                                          totalTime);

            if(!start && !status)
            {
                gctINT i = 0;
                gcmkPRINT("\n|Domain: %6s    totalTime: %8d|", domain ==0 ?"Core":"Shader" ,timeGap);
                gcmkPRINT("|Freq   RunTime|    BusyRatio|     DutyCycle|\n");
                for(; i < gcmCOUNTOF(busyRatio); i++)
                {
                    gcmkPRINT("|%dM    %6u|     %8u|     %8d%%|\n", i==2?416: 156*(i+1),
                                                                   totalTime[i],
                                                                   busyRatio[i],
                                                                   totalTime[i]==0?0: ((100*((gctINT)busyRatio[i]))/(gctINT)totalTime[i]));
                }
            }
            else if(status)
            {
                switch(status)
                {
                    case gcvSTATUS_INVALID_ARGUMENT:
                        printk("Invalidate argument: %d, cat /sys/../dutycycle for more info\n", status);
                        break;
                    case gcvSTATUS_INVALID_REQUEST:
                        printk("Statistics has started alreay, echo 0 x > /sys/.../dutycycle to stop it\n");
                        break;
                    default:
                        printk("cat /sys/../dutycycle for more info, status: %d\n", status);
                }
            }

        }
    }
    else
    {
        printk("Oops, %s is under working\n", __func__);
    }

    return count;
}

static ssize_t show_current_freq (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return show_clk_rate(dev, attr, buf);
}

static ssize_t show_control (struct device *dev,
                    struct device_attribute *attr,
                    char * buf)
{
    return sprintf(buf, "options:\n"
                        " 0,(0|1)      debug, disable/enable\n"
                        " 1,(0|1)    gpufreq, disable/enable\n");
}

static ssize_t store_control (struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    unsigned int option, value;

    SYSFS_VERIFY_INPUT(sscanf(buf, "%u,%u", &option, &value), 2);
    SYSFS_VERIFY_INPUT_RANGE(option, 0, 1);
    SYSFS_VERIFY_INPUT_RANGE(value, 0, 1);

    switch(option) {
    case 0:
        if(value == 0)
        {
            if(registered_debug)
            {
                __remove_sysfs_file_debug();
                registered_debug = 0;
            }
        }
        else if(value == 1)
        {
            if(registered_debug == 0)
            {
                __create_sysfs_file_debug();
                registered_debug = 1;
            }
        }
        break;
/*
    case 1:
        if(value == 0)
            __disable_gpufreq(galDevice);
        else if(value == 1)
            __enable_gpufreq(galDevice);
        break;
*/
    }

    return count;
}

gc_sysfs_attr_rw(mem_stats);
gc_sysfs_attr_rw(ticks);
gc_sysfs_attr_rw(dutycycle);
gc_sysfs_attr_ro(current_freq);
gc_sysfs_attr_rw(control);

static struct attribute *gc_default_attrs[] = {
    &gc_attr_control.attr,
    &gc_attr_mem_stats.attr,
    &gc_attr_ticks.attr,
    &gc_attr_dutycycle.attr,
    &gc_attr_current_freq.attr,
    NULL
};

/* *********************************************************** */
static inline int __create_sysfs_soft_link(struct platform_device *pdev)
{
    int ret = 0;
    struct device * tmp = pdev->dev.parent;
    struct device * parent = tmp;

    do
    {
        if((ret = !strnicmp(parent->kobj.name, "platform", 9)))
            break;
        parent = tmp->parent;
        tmp = parent;
    }while(gcvTRUE);

    parent_device = parent;
    if((ret = sysfs_create_link(&parent->kobj, &pdev->dev.kobj, "galcore")))
        printk("failed to create soft link\n");

    return ret;
}

static inline int __remove_sysfs_soft_link(struct platform_device *pdev)
{
    struct device *parent = NULL;

    if(!parent_device)
        return 1;

    parent = parent_device;
    sysfs_remove_link(&parent->kobj, "galcore");

    parent_device = NULL;

    return 0;
}

static inline int __create_sysfs_file_debug(void)
{
    int ret = 0;

    if((ret = sysfs_create_group(&kset_gpu->kobj, &gc_debug_attr_group)))
        printk("fail to create sysfs group %s\n", gc_debug_attr_group.name);

    return ret;
}

static inline void __remove_sysfs_file_debug(void)
{
    sysfs_remove_group(&kset_gpu->kobj, &gc_debug_attr_group);
}

static inline int __create_sysfs_file_common(void)
{
    int ret = 0;

    if((ret = sysfs_create_files(&kset_gpu->kobj, (const struct attribute **)gc_default_attrs)))
        printk("fail to create sysfs files gc_default_attrs\n");

    return ret;
}

static inline void __remove_sysfs_file_common(void)
{
    sysfs_remove_files(&kset_gpu->kobj, (const struct attribute **)gc_default_attrs);
}

#if MRVL_CONFIG_ENABLE_GPUFREQ
static inline int __create_sysfs_file_gpufreq(void)
{
    int i, len = 0;
    char buf[8] = {0};
    struct kobject *kobj;

    for(i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(galDevice->kernels[i] != gcvNULL)
        {
            gckHARDWARE hardware = galDevice->kernels[i]->hardware;

            len = sprintf(buf, "gpu%d", i);
            buf[len] = '\0';

            kobj = kobject_create_and_add(buf,&kset_gpu->kobj);
            if (!kobj)
            {
                printk("error: allocate kobj for core %d\n", i);
                continue;
            }

            hardware->devObj.kobj = kobj;

#if MRVL_CONFIG_SHADER_CLK_CONTROL
            if(has_feat_shader_indept_dfc() && i == gcvCORE_MAJOR)
            {
                len = sprintf(buf, "gpu%d", (gctINT32)gcvCORE_SH);
                buf[len] = '\0';

                kobj = kobject_create_and_add(buf,&kset_gpu->kobj);
                if (!kobj)
                {
                    printk("error: allocate kobj for shader core\n");
                }
                else
                {
                    hardware->devShObj.kobj = kobj;
                }
            }

#endif
        }
    }


    return 0;
}

static inline void __remove_sysfs_file_gpufreq(void)
{
    int i;

    for(i = 0; i < gcdMAX_GPU_COUNT; i++)
    {
        if(galDevice->kernels[i] != gcvNULL)
        {
            gckHARDWARE hardware = galDevice->kernels[i]->hardware;

            if(!hardware->devObj.kobj)
                continue ;

            kobject_put((struct kobject *)hardware->devObj.kobj);

#if MRVL_CONFIG_SHADER_CLK_CONTROL
            if(has_feat_shader_indept_dfc() && i == gcvCORE_MAJOR)
            {
                if(!hardware->devShObj.kobj)
                    continue ;

                kobject_put((struct kobject *)hardware->devShObj.kobj);
            }

#endif
        }
    }
}
#else
static inline int __create_sysfs_file_gpufreq(void)
{
    return 0;
}
static inline void __remove_sysfs_file_gpufreq(void)
{
    return;
}

#endif

void create_gc_sysfs_file(struct platform_device *pdev)
{
    int ret = 0;

    galDevice = (gckGALDEVICE) platform_get_drvdata(pdev);
    if(!galDevice)
    {
        printk("error: failed in getting drvdata\n");
        return ;
    }

    /* create a kset and register it for 'gpu' */
    kset_gpu = kset_create_and_add("gpu", NULL, &pdev->dev.kobj);
    if(!kset_gpu)
    {
        printk("error: failed in creating kset for 'gpu'\n");
        return ;
    }
    /* FIXME: force kset of kobject 'gpu' linked to itself. */
    kset_gpu->kobj.kset = kset_gpu;

    /* No need for current pxa1U88
    * Remove this control after
    * enabling power-domain and DT on pxa1U88
    */
#if MRVL_GPU_RESOURCE_DT
    __create_sysfs_soft_link(pdev);

#endif

    ret = __create_sysfs_file_common();
    if(ret == 0)
        registered_common = 1;

    ret = __create_sysfs_file_gpufreq();
    if(ret == 0)
        registered_gpufreq = 1;

    ret = create_sysfs_file_pm_test(pdev, &kset_gpu->kobj);
    if(ret == 0)
        registered_pm_test = 1;
}

void remove_gc_sysfs_file(struct platform_device *pdev)
{
    if(!galDevice)
        return;

    if(!kset_gpu)
        return;

    if(registered_pm_test)
    {
        remove_sysfs_file_pm_test(pdev);
        registered_pm_test = 0;
    }

    if(registered_gpufreq)
    {
        __remove_sysfs_file_gpufreq();
        registered_gpufreq = 0;
    }

    if(registered_debug)
    {
        __remove_sysfs_file_debug();
        registered_debug = 0;
    }

    if(registered_common)
    {
#if MRVL_GPU_RESOURCE_DT
        __remove_sysfs_soft_link(pdev);

#endif
        __remove_sysfs_file_common();
        registered_common = 0;
    }

    /* release a kset. */
    kset_unregister(kset_gpu);
}


#endif
