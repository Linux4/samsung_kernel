//
// FILENAME.
//      fps_trigger.c - FringerPrint Sensor signal trigger routine
//
//      $PATH:
//
// FUNCTIONAL DESCRIPTION.
//      FingerPrint Sensor trigger routine
//
// MODIFICATION HISTORY.
//
// NOTICE.
//      Copyright (C) 2000-2014 EgisTec All Rights Reserved.
//

//#include <linux/spi/egistec/fps_trigger.h>
#include "fps_trigger.h"
#include <linux/delay.h>
//
// interrupt description
//

struct interrupt_desc {
	int gpio;
	int number;
	char *name;
	struct timer_list timer;
};

//
// FPS interrupt table
//
#define ET300_DRIVER_GPIO_FINGER_INT		(85)
static struct interrupt_desc fps_ints[] = {
	{ ET300_DRIVER_GPIO_FINGER_INT , 0, "BUT0" }      // for TINY4412 platform's CON15 XEINT12 pin 
};

static volatile char interrupt_values[] = {
	'0', '0', '0', '0', '0', '0', '0', '0'
};

static DECLARE_WAIT_QUEUE_HEAD(interrupt_waitq);

static volatile int ev_press = 0;

//
// FUNCTION NAME.
//      interrupt_timer_routine
//
// FUNCTIONAL DESCRIPTION.
//      basic interrupt timer inital routine
//
//
// ENTRY PARAMETERS.
//      gpio             - gpio address
//
// EXIT PARAMETERS.
//      Function Return          – int,
//

static void interrupt_timer_routine(
  unsigned long _data
)
{
  struct interrupt_desc *bdata = (struct interrupt_desc *)_data;
  int down;
  int number;
  unsigned tmp;

  tmp = gpio_get_value(bdata->gpio);

  //
  // active low
  //
  down = !tmp;
  printk(KERN_DEBUG "FPS interrupt pin %d: %08x\n", bdata->number, down);

  number = bdata->number;
  if (down != (interrupt_values[number] & 1))
  {
    interrupt_values[number] = '0' + down;

    ev_press = 1;
    wake_up_interruptible(&interrupt_waitq);
  }
}

//
// FUNCTION NAME.
//      fingerprint_interrupt
//
// FUNCTIONAL DESCRIPTION.
//      finger print interrupt callback routine
//
//
// ENTRY PARAMETERS.
//      irq
//      dev_id
//
// EXIT PARAMETERS.
//      Function Return          – int,
//

static irqreturn_t fingerprint_interrupt(
  int irq,
  void *dev_id
)
{
	//interrupt
	//struct interrupt_desc *bdata = (struct interrupt_desc *)dev_id;

	//mod_timer(&bdata->timer, jiffies + msecs_to_jiffies(40));
        printk(KERN_ERR "enter to modify the finger interrupt routine\n");
		//interrupt
		ev_press = 1;
		wake_up_interruptible(&interrupt_waitq);//
	return IRQ_HANDLED;
}

//
// FUNCTION NAME.
//      Interrupt_Init
//
// FUNCTIONAL DESCRIPTION.
//      button initial routine
//
//
// ENTRY PARAMETERS.
//      gpio             - gpio address
//
// EXIT PARAMETERS.
//      Function Return          – int,
//

int Interrupt_Init(void)
{
  int i;
  int irq;
  int err = 0;

  for (i = 0; i < ARRAY_SIZE(fps_ints); i++)
  {
    if (!fps_ints[i].gpio) { continue; }

    //
    // set timer function to handle the interrupt
    //
    setup_timer(&fps_ints[i].timer, interrupt_timer_routine,(unsigned long)&fps_ints[i]);

    //
    // set the IRQ function and GPIO. then setting the interrupt trigger type
    //
    irq = gpio_to_irq(fps_ints[i].gpio);
    err = request_irq(irq, fingerprint_interrupt, IRQ_TYPE_EDGE_BOTH,fps_ints[i].name, (void *)&fps_ints[i]);
    if (err)
    {
      break;
    }
  }

  if (err)
  {
    i--;
    for (; i >= 0; i--)
    {
      if (!fps_ints[i].gpio) { continue; }

      irq = gpio_to_irq(fps_ints[i].gpio);
      disable_irq(irq);
      free_irq(irq, (void *)&fps_ints[i]);
      del_timer_sync(&fps_ints[i].timer);
    }
    return -EBUSY;
  }
  ev_press = 1;
  return 0;
}

//
// FUNCTION NAME.
//      Interrupt_Free
//
// FUNCTIONAL DESCRIPTION.
//      free all interrupt resource
//
//
// ENTRY PARAMETERS.
//      gpio             - gpio address
//
// EXIT PARAMETERS.
//      Function Return          – int,
//

int Interrupt_Free(void)
{
  int irq, i;

  for (i = 0; i < ARRAY_SIZE(fps_ints); i++)
  {
    if (!fps_ints[i].gpio)
    {
      continue;
    }

    irq = gpio_to_irq(fps_ints[i].gpio);
    free_irq(irq, (void *)&fps_ints[i]);

    del_timer_sync(&fps_ints[i].timer);
  }

  return 0;
}

//
// FUNCTION NAME.
//      fps_interrupt_read
//
// FUNCTIONAL DESCRIPTION.
//      FPS interrupt read status
//
//
// ENTRY PARAMETERS.
//
//
// EXIT PARAMETERS.
//      Function Return          – int,
//

int fps_interrupt_read(
  struct file *filp,
  char __user *buff,
  size_t count,
  loff_t *offp
)
{
  unsigned long err;
  int ret;
  if (!ev_press)
  {
    if (filp->f_flags & O_NONBLOCK)
    {
      return -EAGAIN;
    }
    else
    {
      //interrupt
      //wait_event_interruptible(interrupt_waitq, ev_press);
      ret = wait_event_interruptible_timeout(interrupt_waitq, ev_press, msecs_to_jiffies(1000));//
    }
  }
  //interrupt
   if(ret > 0 && ret <1000 )
	interrupt_values[0]=1;
  else
	interrupt_values[0]=0;//

  //interrupt
  //printk(KERN_ERR "interrupt read condition %d\n",ev_press);
  //printk(KERN_ERR "interrupt value  %d\n",interrupt_values[0]);
  printk(KERN_ERR "interrupt_timeout ret=%d !!!!!!!\n",ret); //
  ev_press = 0;

  err = copy_to_user((void *)buff, (const void *)(&interrupt_values),min(sizeof(interrupt_values), count));
  return err ? -EFAULT : min(sizeof(interrupt_values), count);
}

int Interrupt_Exit(void){

  //if (interrupt_waitq==NULL)
	//return -1;
  //wake_up_interruptible(&interrupt_waitq);
/*
	int retval;
	retval=gpio_get_value(EXYNOS4_GPX1(4));
	if(retval<0){
		printk("Interrupt_Exit get_value fail = %d\n",retval);
		return -1;
	}
	else if(retval==0)
		gpio_set_value(EXYNOS4_GPX1(4),1);
	else{
		gpio_set_value(EXYNOS4_GPX1(4),0);
		udelay(10);
		gpio_set_value(EXYNOS4_GPX1(4),1);
	}
  return 0;
*/
ev_press=1;return 0;
}

//
// FUNCTION NAME.
//      fps_interrupt_read
//
// FUNCTIONAL DESCRIPTION.
//      FPS interrupt read status
//
//
// ENTRY PARAMETERS.
//
//      wait      poll table structure
//
// EXIT PARAMETERS.
//      Function Return          – int,
//

unsigned int fps_interrupt_poll(
  struct file *file,
  struct poll_table_struct *wait
)
{
  unsigned int mask = 0;

  poll_wait(file, &interrupt_waitq, wait);
  if (ev_press)
  {
    mask |= POLLIN | POLLRDNORM;
  }

  return mask;
}


