//
// FILENAME.
//      fps_trigger.h - FPS trigger routine
//
//      $PATH:
//
// FUNCTIONAL DESCRIPTION.
//      1. FPS trigger control
//
// MODIFICATION HISTORY.
//
// NOTICE.
//      Copyright (C) 2000-2014 EgisTec All Rights Reserved.
//

#ifndef _FPS_TRIGGER_CONTROL_H
#define _FPS_TRIGGER_CONTROL_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
//#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>

#include <linux/gpio.h>
//#include <mach/gpio.h>
//#include <plat/gpio-cfg.h>

//
// interrupt init
//
int Interrupt_Init(void);

//
// interrupt free
//
int Interrupt_Free(void);

int Interrupt_Exit(void);

//
// interrupt read status
//
int fps_interrupt_read(
  struct file *filp,
  char __user *buff,
  size_t count,
  loff_t *offp
);

//
// interrupt polling
//
unsigned int fps_interrupt_poll(
  struct file *file,
  struct poll_table_struct *wait
);





#endif
