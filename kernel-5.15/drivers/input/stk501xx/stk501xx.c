//#include "stk501xx_mtk_i2c.h"
#include "stk501xx.h"

#include <linux/sensors.h>
#include <linux/hardware_info.h>

static struct stk_data* gStk = NULL;

static int32_t stk_init_flag = 0;
#define STK_NULL 0
#ifdef STK_BUS_I2C
    #include <linux/i2c.h>
#endif

#if (defined(STK_GPIO_MTK))
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#endif

//Timer
#include <linux/delay.h>
#if STK_NULL //open change
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
bool pause_mode = 0;
struct stk_data *global_stk;
static struct wake_lock stk501xx_wake_lock;
static volatile uint8_t stk501xx_irq_from_suspend_flag = 0;
#if SAR_IN_FRANCE
#define MAX_INT_COUNT 26
#endif

#ifdef STK_BUS_I2C
#define MAX_I2C_MANAGER_NUM     5

void stk_report_sar_data(struct stk_data* stk);

struct i2c_manager *pi2c_mgr[MAX_I2C_MANAGER_NUM] = {NULL};

int i2c_init(void* st)
{
    int i2c_idx = 0;

    if (!st)
    {
        return -1;
    }

    for (i2c_idx = 0; i2c_idx < MAX_I2C_MANAGER_NUM; i2c_idx ++)
    {
        if (pi2c_mgr[i2c_idx] == (struct i2c_manager*)st)
        {
            printk(KERN_INFO "%s: i2c is exist\n", __func__);
            return i2c_idx;
        }
        else if (pi2c_mgr[i2c_idx] == NULL)
        {
            pi2c_mgr[i2c_idx] = (struct i2c_manager*)st;
            return i2c_idx;
        }
    }

    return -1;
}

int i2c_reg_read(int i2c_idx, unsigned int reg, unsigned char *val)
{
    int error = 0;
    struct i2c_manager *_pi2c = pi2c_mgr[i2c_idx];
    I2C_REG_ADDR_TYPE addr_type = _pi2c->addr_type;
    mutex_lock(&_pi2c->lock);

    if (addr_type == ADDR_8BIT)
    {
        unsigned char reg_ = (unsigned char)(reg & 0xFF);
        error = i2c_smbus_read_byte_data(_pi2c->client, reg_);

        if (error < 0)
        {
            dev_err(&_pi2c->client->dev,
                    "%s: failed to read reg:0x%x\n",
                    __func__, reg);
        }
        else
        {
            *(unsigned char *)val = error & 0xFF;
        }
    }
    else if (addr_type == ADDR_16BIT)
    {
    }

    mutex_unlock(&_pi2c->lock);
    return error;
}

int i2c_reg_write(int i2c_idx, unsigned int reg, unsigned char val)
{
    int error = 0;
    struct i2c_manager *_pi2c = pi2c_mgr[i2c_idx];
    I2C_REG_ADDR_TYPE addr_type = _pi2c->addr_type;
    mutex_lock(&_pi2c->lock);

    if (addr_type == ADDR_8BIT)
    {
        unsigned char reg_ = (unsigned char)(reg & 0xFF);
        error = i2c_smbus_write_byte_data(_pi2c->client, reg_, val);
    }
    else if (addr_type == ADDR_16BIT)
    {
    }

    mutex_unlock(&_pi2c->lock);

    if (error < 0)
    {
        dev_err(&_pi2c->client->dev,
                "%s: failed to write reg:0x%x with val:0x%x\n",
                __func__, reg, val);
    }

    return error;
}

int i2c_reg_write_block(int i2c_idx, unsigned int reg, void *val, int length)
{
    int error = 0;
    struct i2c_manager *_pi2c = pi2c_mgr[i2c_idx];
    I2C_REG_ADDR_TYPE addr_type = _pi2c->addr_type;
    mutex_lock(&_pi2c->lock);

    if (addr_type == ADDR_8BIT)
    {
        unsigned char reg_ = (unsigned char)(reg & 0xFF);
        error = i2c_smbus_write_i2c_block_data(_pi2c->client, reg_, length, val);
    }
    else if (addr_type == ADDR_16BIT)
    {
        int i = 0;
        unsigned char *buffer_inverse;
        struct i2c_msg msgs;
        buffer_inverse = kzalloc((sizeof(unsigned char) * (length + 2)), GFP_KERNEL);
        buffer_inverse[0] = reg >> 8;
        buffer_inverse[1] = reg & 0xff;

        for (i = 0; i < length; i ++)
        {
            buffer_inverse[2 + i] = *(u8*)((u8*)val + ((length - 1) - i));
        }

        msgs.addr = _pi2c->client->addr;
        msgs.flags = _pi2c->client->flags & I2C_M_TEN;
        msgs.len = length + 2;
        msgs.buf = buffer_inverse;
#ifdef STK_RETRY_I2C
        i = 0;

        do
        {
            error = i2c_transfer(_pi2c->client->adapter, &msgs, 1);
        }
        while (error != 1 && ++i < 3);

#else
        error = i2c_transfer(_pi2c->client->adapter, &msgs, 1);
#endif //  STK_RETRY_I2C
        kfree(buffer_inverse);
    }

    mutex_unlock(&_pi2c->lock);

    if (error < 0)
    {
        dev_err(&_pi2c->client->dev,
                "%s: failed to write reg:0x%x\n",
                __func__, reg);
    }

    return error;
}

int i2c_reg_read_modify_write(int i2c_idx, unsigned int reg, unsigned char val, unsigned char mask)
{
    uint8_t rw_buffer = 0;
    int error = 0;
    struct i2c_manager *_pi2c = pi2c_mgr[i2c_idx];

    if ((mask == 0xFF) || (mask == 0x0))
    {
        error = i2c_reg_write(i2c_idx, reg, val);

        if (error < 0)
        {
            dev_err(&_pi2c->client->dev,
                    "%s: failed to write reg:0x%x with val:0x%x\n",
                    __func__, reg, val);
        }
    }
    else
    {
        error = (uint8_t)i2c_reg_read(i2c_idx, reg, &rw_buffer);

        if (error < 0)
        {
            dev_err(&_pi2c->client->dev,
                    "%s: failed to read reg:0x%x\n",
                    __func__, reg);
            return error;
        }
        else
        {
            rw_buffer = (rw_buffer & (~mask)) | (val & mask);
            error = i2c_reg_write(i2c_idx, reg, rw_buffer);

            if (error < 0)
            {
                dev_err(&_pi2c->client->dev,
                        "%s: failed to write reg(mask):0x%x with val:0x%x\n",
                        __func__, reg, val);
            }
        }
    }

    return error;
}

int i2c_reg_read_block(int i2c_idx, unsigned int reg, int count, void *buf)
{
    int ret = 0;
    // int loop_cnt = 0;
    struct i2c_manager *_pi2c = pi2c_mgr[i2c_idx];
    I2C_REG_ADDR_TYPE addr_type = _pi2c->addr_type;
    mutex_lock(&_pi2c->lock);

    if (addr_type == ADDR_8BIT)
    {
        struct i2c_msg msgs[2] =
        {
            {
                .addr = _pi2c->client->addr,
                .flags = 0,
                .len = 1,
                .buf = (u8*)&reg
            },
            {
                .addr = _pi2c->client->addr,
                .flags = I2C_M_RD,
                .len = count,
                .buf = buf
            }
        };
        ret = i2c_transfer(_pi2c->client->adapter, msgs, 2);

        if (2 == ret)
        {
            ret = 0;
        }

        // unsigned char reg_ = (unsigned char)(reg & 0xFF);
        // while (count)
        // {
        //     ret = i2c_smbus_read_i2c_block_data(_pi2c->client, reg_,
        //                                         (count > I2C_SMBUS_BLOCK_MAX) ? I2C_SMBUS_BLOCK_MAX : count,
        //                                         (buf + (loop_cnt * I2C_SMBUS_BLOCK_MAX))
        //                                        );
        //     (count > I2C_SMBUS_BLOCK_MAX) ? (count -= I2C_SMBUS_BLOCK_MAX) : (count -= count);
        //     loop_cnt ++;
        // }
    }
    else if (addr_type == ADDR_16BIT)
    {
        int i = 0;
        u16 reg_inverse = (reg & 0x00FF) << 8 | (reg & 0xFF00) >> 8;
        int read_length = count;
        u8 buffer_inverse[99] = { 0 };
        struct i2c_msg msgs[2] =
        {
            {
                .addr = _pi2c->client->addr,
                .flags = 0,
                .len = 2,
                .buf = (u8*)&reg_inverse
            },
            {
                .addr = _pi2c->client->addr,
                .flags = I2C_M_RD,
                .len = read_length,
                .buf = buffer_inverse
            }
        };
#ifdef STK_RETRY_I2C
        i = 0;

        do
        {
            ret = i2c_transfer(_pi2c->client->adapter, msgs, 2);
        }
        while (ret != 2 && ++i < 3);

#else
        ret = i2c_transfer(_pi2c->client->adapter, msgs, 2);
#endif //  STK_RETRY_I2C

        if (2 == ret)
        {
            ret = 0;

            for (i = 0; i < read_length; i ++)
            {
                *(u8*)((u8*)buf + i) = ((buffer_inverse[read_length - 1 - i]));
            }
        }
    }

    mutex_unlock(&_pi2c->lock);
    return ret;
}

int i2c_remove(void* st)
{
    int i2c_idx = 0;

    if (!st)
    {
        return -1;
    }

    for (i2c_idx = 0; i2c_idx < MAX_I2C_MANAGER_NUM; i2c_idx ++)
    {
        printk(KERN_INFO "%s: i2c_idx = %d\n", __func__, i2c_idx);

        if (pi2c_mgr[i2c_idx] == (struct i2c_manager*)st)
        {
            printk(KERN_INFO "%s: release i2c_idx = %d\n", __func__, i2c_idx);
            pi2c_mgr[i2c_idx] = NULL;
            break;
        }
    }

    return 0;
}

const struct stk_bus_ops stk_i2c_bops =
{
    .bustype            = BUS_I2C,
    .init               = i2c_init,
    .write              = i2c_reg_write,
    .write_block        = i2c_reg_write_block,
    .read               = i2c_reg_read,
    .read_block         = i2c_reg_read_block,
    .read_modify_write  = i2c_reg_read_modify_write,
    .remove             = i2c_remove,
};
#endif

typedef struct timer_manager timer_manager;

struct timer_manager
{
    struct work_struct          stk_work;
    struct hrtimer              stk_hrtimer;
    struct workqueue_struct     *stk_wq;
    ktime_t                     timer_interval;

    stk_timer_info              *timer_info;
} timer_mgr_default = {.timer_info = 0};

#define MAX_LINUX_TIMER_MANAGER_NUM     5

timer_manager linux_timer_mgr[MAX_LINUX_TIMER_MANAGER_NUM];

static timer_manager* parser_timer(struct hrtimer *timer)
{
    int timer_idx = 0;

    if (timer == NULL)
    {
        return NULL;
    }

    for (timer_idx = 0; timer_idx < MAX_LINUX_TIMER_MANAGER_NUM; timer_idx ++)
    {
        if (&linux_timer_mgr[timer_idx].stk_hrtimer == timer)
        {
            return &linux_timer_mgr[timer_idx];
        }
    }

    return NULL;
}

static enum hrtimer_restart timer_func(struct hrtimer *timer)
{
    timer_manager *timer_mgr = parser_timer(timer);

    if (timer_mgr == NULL)
    {
        return HRTIMER_NORESTART;
    }

    queue_work(timer_mgr->stk_wq, &timer_mgr->stk_work);
    hrtimer_forward_now(&timer_mgr->stk_hrtimer, timer_mgr->timer_interval);
    return HRTIMER_RESTART;
}

static timer_manager* timer_parser_work(struct work_struct *work)
{
    int timer_idx = 0;

    if (work == NULL)
    {
        return NULL;
    }

    for (timer_idx = 0; timer_idx < MAX_LINUX_TIMER_MANAGER_NUM; timer_idx ++)
    {
        if (&linux_timer_mgr[timer_idx].stk_work == work)
        {
            return &linux_timer_mgr[timer_idx];
        }
    }

    return NULL;
}

static void timer_callback(struct work_struct *work)
{
    timer_manager *timer_mgr = timer_parser_work(work);

    if (timer_mgr == NULL)
    {
        return;
    }

    timer_mgr->timer_info->timer_cb(timer_mgr->timer_info->user_data);
}

int register_timer(stk_timer_info *t_info)
{
    int timer_idx = 0;

    if (t_info == NULL)
    {
        return -1;
    }

    for (timer_idx = 0; timer_idx < MAX_LINUX_TIMER_MANAGER_NUM; timer_idx ++)
    {
        if (!linux_timer_mgr[timer_idx].timer_info)
        {
            linux_timer_mgr[timer_idx].timer_info = t_info;
            break;
        }
        else
        {
            if (linux_timer_mgr[timer_idx].timer_info == t_info)
            {
                //already register
                if (linux_timer_mgr[timer_idx].timer_info->change_interval_time)
                {
                    linux_timer_mgr[timer_idx].timer_info->change_interval_time = 0;
                    printk(KERN_ERR "%s: change interval time\n", __func__);

                    switch (linux_timer_mgr[timer_idx].timer_info->timer_unit)
                    {
                        case N_SECOND:
                            linux_timer_mgr[timer_idx].timer_interval = ns_to_ktime(linux_timer_mgr[timer_idx].timer_info->interval_time);
                            break;

                        case U_SECOND:
                            linux_timer_mgr[timer_idx].timer_interval = ns_to_ktime(linux_timer_mgr[timer_idx].timer_info->interval_time * NSEC_PER_USEC);
                            break;

                        case M_SECOND:
                            linux_timer_mgr[timer_idx].timer_interval = ns_to_ktime(linux_timer_mgr[timer_idx].timer_info->interval_time * NSEC_PER_MSEC);
                            break;

                        case SECOND:
                            break;
                    }

                    return 0;
                }

                printk(KERN_ERR "%s: this timer is registered\n", __func__);
                return -1;
            }
        }
    }

    // if search/register timer manager not successfully
    if (timer_idx == MAX_LINUX_TIMER_MANAGER_NUM)
    {
        printk(KERN_ERR "%s: timer_idx out of range %d\n", __func__, timer_idx);
        return -1;
    }

    printk(KERN_ERR "%s: register timer name %s\n", __func__, linux_timer_mgr[timer_idx].timer_info->wq_name);
    linux_timer_mgr[timer_idx].stk_wq = create_singlethread_workqueue(linux_timer_mgr[timer_idx].timer_info->wq_name);

    if (linux_timer_mgr[timer_idx].stk_wq == NULL)
    {
        printk(KERN_ERR "%s: create single thread workqueue fail\n", __func__);
        linux_timer_mgr[timer_idx].timer_info = 0;
        return -1;
    }

    INIT_WORK(&linux_timer_mgr[timer_idx].stk_work, timer_callback);
    hrtimer_init(&linux_timer_mgr[timer_idx].stk_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

    switch (linux_timer_mgr[timer_idx].timer_info->timer_unit)
    {
        case N_SECOND:
            linux_timer_mgr[timer_idx].timer_interval = ns_to_ktime(linux_timer_mgr[timer_idx].timer_info->interval_time);
            break;

        case U_SECOND:
            linux_timer_mgr[timer_idx].timer_interval = ns_to_ktime(linux_timer_mgr[timer_idx].timer_info->interval_time * NSEC_PER_USEC);
            break;

        case M_SECOND:
            linux_timer_mgr[timer_idx].timer_interval = ns_to_ktime(linux_timer_mgr[timer_idx].timer_info->interval_time * NSEC_PER_MSEC);
            break;

        case SECOND:
            break;
    }

    linux_timer_mgr[timer_idx].stk_hrtimer.function = timer_func;
    linux_timer_mgr[timer_idx].timer_info->is_exist = true;
    return 0;
}

int start_timer(stk_timer_info *t_info)
{
    int timer_idx = 0;

    for (timer_idx = 0; timer_idx < MAX_LINUX_TIMER_MANAGER_NUM; timer_idx ++)
    {
        if (linux_timer_mgr[timer_idx].timer_info == t_info)
        {
            if (linux_timer_mgr[timer_idx].timer_info->is_exist)
            {
                if (!linux_timer_mgr[timer_idx].timer_info->is_active)
                {
                    hrtimer_start(&linux_timer_mgr[timer_idx].stk_hrtimer, linux_timer_mgr[timer_idx].timer_interval, HRTIMER_MODE_REL);
                    linux_timer_mgr[timer_idx].timer_info->is_active = true;
                    printk(KERN_ERR "%s: start timer name %s\n", __func__, linux_timer_mgr[timer_idx].timer_info->wq_name);
                }
                else
                {
                    printk(KERN_INFO "%s: %s was already running\n", __func__, linux_timer_mgr[timer_idx].timer_info->wq_name);
                }
            }

            return 0;
        }
    }

    return -1;
}

int stop_timer(stk_timer_info *t_info)
{
    int timer_idx = 0;

    for (timer_idx = 0; timer_idx < MAX_LINUX_TIMER_MANAGER_NUM; timer_idx ++)
    {
        if (linux_timer_mgr[timer_idx].timer_info == t_info)
        {
            if (linux_timer_mgr[timer_idx].timer_info->is_exist)
            {
                if (linux_timer_mgr[timer_idx].timer_info->is_active)
                {
                    hrtimer_cancel(&linux_timer_mgr[timer_idx].stk_hrtimer);
                    linux_timer_mgr[timer_idx].timer_info->is_active = false;
                    printk(KERN_ERR "%s: stop timer name %s\n", __func__, linux_timer_mgr[timer_idx].timer_info->wq_name);
                }
                else
                {
                    printk(KERN_ERR "%s: %s stop already stop\n", __func__, linux_timer_mgr[timer_idx].timer_info->wq_name);
                }
            }

            return 0;
        }
    }

    return -1;
}

int remove_timer(stk_timer_info *t_info)
{
    int timer_idx = 0;

    for (timer_idx = 0; timer_idx < MAX_LINUX_TIMER_MANAGER_NUM; timer_idx ++)
    {
        if (linux_timer_mgr[timer_idx].timer_info == t_info)
        {
            if (linux_timer_mgr[timer_idx].timer_info->is_exist)
            {
                if (linux_timer_mgr[timer_idx].timer_info->is_active)
                {
                    hrtimer_try_to_cancel(&linux_timer_mgr[timer_idx].stk_hrtimer);
                    destroy_workqueue(linux_timer_mgr[timer_idx].stk_wq);
                    cancel_work_sync(&linux_timer_mgr[timer_idx].stk_work);
                    linux_timer_mgr[timer_idx].timer_info->is_active = false;
                    linux_timer_mgr[timer_idx].timer_info->is_exist = false;
                    linux_timer_mgr[timer_idx].timer_info = 0;
                }
            }

            return 0;
        }
    }

    return -1;
}

void busy_wait(unsigned long delay, BUSY_WAIT_TYPE mode)
{
    if ((!delay))
    {
        return;
    }

    if (mode == US_DELAY)
    {
        msleep(delay / 1000);
    }

    if (mode == MS_DELAY)
    {
        msleep(delay);
    }
}
const struct stk_timer_ops stk_t_ops =
{
    .register_timer         = register_timer,
    .start_timer            = start_timer,
    .stop_timer             = stop_timer,
    .remove                 = remove_timer,
    .busy_wait              = busy_wait,
};

typedef struct gpio_manager gpio_manager;

struct gpio_manager
{
    struct work_struct          stk_work;
    struct workqueue_struct     *stk_wq;

    stk_gpio_info                *gpio_info;
} gpio_mgr_default = {.gpio_info = 0};

#define MAX_LINUX_GPIO_MANAGER_NUM      5

gpio_manager linux_gpio_mgr[MAX_LINUX_GPIO_MANAGER_NUM];

static gpio_manager* gpio_parser_work(struct work_struct *work)
{
    int gpio_idx = 0;

    if (!work)
    {
        return NULL;
    }

    for (gpio_idx = 0; gpio_idx < MAX_LINUX_GPIO_MANAGER_NUM; gpio_idx ++)
    {
        if (&linux_gpio_mgr[gpio_idx].stk_work == work)
        {
            return &linux_gpio_mgr[gpio_idx];
        }
    }

    return NULL;
}

static void gpio_callback(struct work_struct *work)
{
    gpio_manager *gpio_mgr = gpio_parser_work(work);

    if (!gpio_mgr)
    {
        return;
    }

    gpio_mgr->gpio_info->gpio_cb(gpio_mgr->gpio_info->user_data);
    enable_irq(gpio_mgr->gpio_info->irq);
}

static irqreturn_t stk_gpio_irq_handler(int irq, void *data)
{
    gpio_manager *pData = data;
    disable_irq_nosync(irq);
    schedule_work(&pData->stk_work);
    return IRQ_HANDLED;
}
int register_gpio_irq(stk_gpio_info *gpio_info)
{
    int gpio_idx = 0;
    int irq = 0;
    int err = 0;

    if (!gpio_info)
    {
        return -1;
    }

    for (gpio_idx = 0; gpio_idx < MAX_LINUX_GPIO_MANAGER_NUM; gpio_idx ++)
    {
        if (!linux_gpio_mgr[gpio_idx].gpio_info)
        {
            linux_gpio_mgr[gpio_idx].gpio_info = gpio_info;
            break;
        }
        else
        {
            if (linux_gpio_mgr[gpio_idx].gpio_info == gpio_info)
            {
                //already register
                return -1;
            }
        }
    }

    // if search/register timer manager not successfully
    if (gpio_idx == MAX_LINUX_GPIO_MANAGER_NUM)
    {
        printk(KERN_ERR "%s: gpio_idx out of range %d\n", __func__, gpio_idx);
        return -1;
    }

    printk(KERN_INFO "%s: irq num = %d \n", __func__, gpio_info->int_pin);

    if (err < 0)
    {
        printk(KERN_ERR "STK %s: gpio_request, err=%d", __func__, err);
        return err;
    }

    linux_gpio_mgr[gpio_idx].stk_wq = create_singlethread_workqueue(linux_gpio_mgr[gpio_idx].gpio_info->wq_name);
    INIT_WORK(&linux_gpio_mgr[gpio_idx].stk_work, gpio_callback);
    err = gpio_direction_input(linux_gpio_mgr[gpio_idx].gpio_info->int_pin);

    if (err < 0)
    {
        printk(KERN_ERR "STK %s: gpio_direction_input, err=%d", __func__, err);
        return err;
    }

    switch (linux_gpio_mgr[gpio_idx].gpio_info->trig_type)
    {
        case TRIGGER_RISING:
            err = request_irq(linux_gpio_mgr[gpio_idx].gpio_info->irq, stk_gpio_irq_handler, \
                              IRQF_TRIGGER_RISING, linux_gpio_mgr[gpio_idx].gpio_info->device_name, &linux_gpio_mgr[gpio_idx]);
            break;

        case TRIGGER_FALLING:
            err = request_irq(linux_gpio_mgr[gpio_idx].gpio_info->irq, stk_gpio_irq_handler, \
                              IRQF_TRIGGER_FALLING, linux_gpio_mgr[gpio_idx].gpio_info->device_name, &linux_gpio_mgr[gpio_idx]);
            break;

        case TRIGGER_HIGH:
        case TRIGGER_LOW:
            err = request_irq(linux_gpio_mgr[gpio_idx].gpio_info->irq, stk_gpio_irq_handler, \
                              IRQF_TRIGGER_LOW, linux_gpio_mgr[gpio_idx].gpio_info->device_name, &linux_gpio_mgr[gpio_idx]);
            break;
    }

    if (err < 0)
    {
        printk(KERN_WARNING "STK %s: request_any_context_irq(%d) failed for (%d)\n", __func__, irq, err);
        goto err_request_any_context_irq;
    }

    linux_gpio_mgr[gpio_idx].gpio_info->is_exist = true;
    return 0;
err_request_any_context_irq:
    gpio_free(linux_gpio_mgr[gpio_idx].gpio_info->int_pin);
    return err;
}

int start_gpio_irq(stk_gpio_info *gpio_info)
{
    int gpio_idx = 0;

    for (gpio_idx = 0; gpio_idx < MAX_LINUX_GPIO_MANAGER_NUM; gpio_idx ++)
    {
        if (linux_gpio_mgr[gpio_idx].gpio_info == gpio_info)
        {
            if (linux_gpio_mgr[gpio_idx].gpio_info->is_exist)
            {
                if (!linux_gpio_mgr[gpio_idx].gpio_info->is_active)
                {
                    linux_gpio_mgr[gpio_idx].gpio_info->is_active = true;
                }
            }

            return 0;
        }
    }

    return -1;
}

int stop_gpio_irq(stk_gpio_info *gpio_info)
{
    int gpio_idx = 0;

    for (gpio_idx = 0; gpio_idx < MAX_LINUX_GPIO_MANAGER_NUM; gpio_idx ++)
    {
        if (linux_gpio_mgr[gpio_idx].gpio_info == gpio_info)
        {
            if (linux_gpio_mgr[gpio_idx].gpio_info->is_exist)
            {
                if (linux_gpio_mgr[gpio_idx].gpio_info->is_active)
                {
                    linux_gpio_mgr[gpio_idx].gpio_info->is_active = false;
                }
            }

            return 0;
        }
    }

    return -1;
}

int remove_gpio_irq(stk_gpio_info *gpio_info)
{
    int gpio_idx = 0;

    for (gpio_idx = 0; gpio_idx < MAX_LINUX_GPIO_MANAGER_NUM; gpio_idx ++)
    {
        if (linux_gpio_mgr[gpio_idx].gpio_info == gpio_info)
        {
            if (linux_gpio_mgr[gpio_idx].gpio_info->is_exist)
            {
                if (linux_gpio_mgr[gpio_idx].gpio_info->is_active)
                {
                    linux_gpio_mgr[gpio_idx].gpio_info->is_active = false;
                    linux_gpio_mgr[gpio_idx].gpio_info->is_exist = false;
                    free_irq(linux_gpio_mgr[gpio_idx].gpio_info->irq, &linux_gpio_mgr[gpio_idx]);
                    gpio_free(linux_gpio_mgr[gpio_idx].gpio_info->int_pin);
                    cancel_work_sync(&linux_gpio_mgr[gpio_idx].stk_work);
                    linux_gpio_mgr[gpio_idx].gpio_info = NULL;
                }
            }

            return 0;
        }
    }

    return -1;
}

const struct stk_gpio_ops stk_g_ops =
{
    .register_gpio_irq      = register_gpio_irq,
    .start_gpio_irq         = start_gpio_irq,
    .stop_gpio_irq          = stop_gpio_irq,
    .remove                 = remove_gpio_irq,

};



int64_t stk_pow(int64_t base, int32_t exp)
{
    int64_t result = 1;

    while (exp)
    {
        if (exp & 1)
            result *= base;

        exp >>= 1;
        base *= base;
    }

    return result;
}

int stk_log_cal(uint8_t value, uint8_t base)
{
    int sqrtCounter = 0;

    if (value <= 1)
        return 0;

    while (value > 1)
    {
        value = value >> base;
        sqrtCounter++;
    }

    return sqrtCounter;
}



stk501xx_register_table stk501xx_default_register_table[] =
{
    //Trigger_CMD
    {STK_ADDR_TRIGGER_REG,          STK_TRIGGER_REG_PHEN_DISABLE_ALL},
    {STK_ADDR_TRIGGER_CMD,          STK_TRIGGER_CMD_REG_INIT_ALL    },

    {0x0A00, 0x0000000A},  //0529add
    {0x0010, 0x000000A5},
    {0x0768, 0x00110010},
    {0x076C, 0x00001000},

    //WRITE TIMING
    {0x0740, 0x000413AA},
    {0x0744, 0x0505000F},
    {0x0748, 0x00000505},
    {0x074C, 0x00030200},
    {0x0750, 0x0E0E001D},
    {0x0754, 0x00000E0E},
    {0x0758, 0x00030200},
    {0x075C, 0x1D1D002F},
    {0x0760, 0x10001D1D},
    {0x0764, 0x00030200},

    //RXIO 0~7
    {STK_ADDR_RXIO0_MUX_REG,        STK_RXIO0_MUX_REG_VALUE}, //mapping ph1
    {STK_ADDR_RXIO1_MUX_REG,        STK_RXIO1_MUX_REG_VALUE}, //mapping ph2
    {STK_ADDR_RXIO2_MUX_REG,        STK_RXIO2_MUX_REG_VALUE},
    {STK_ADDR_RXIO3_MUX_REG,        STK_RXIO3_MUX_REG_VALUE},
    {STK_ADDR_RXIO4_MUX_REG,        STK_RXIO4_MUX_REG_VALUE},
    {STK_ADDR_RXIO5_MUX_REG,        STK_RXIO5_MUX_REG_VALUE},
    {STK_ADDR_RXIO6_MUX_REG,        STK_RXIO6_MUX_REG_VALUE},
    {STK_ADDR_RXIO7_MUX_REG,        STK_RXIO7_MUX_REG_VALUE},

    //SCAN_PERIOD
    {STK_ADDR_SCAN_PERIOD,         STK_SCAN_PERIOD_VALUE},

    //I2C WDT
    {STK_ADDR_WATCH_DOG,   (STK_SNS_WATCH_DOG_VALUE | STK_I2C_WDT_VALUE)},

    //below by function to set each phase
    //SCAN OPTION
    {STK_ADDR_SCAN_OPT_PH0,        STK_SCAN_OPT_PH0_VALUE},
    {STK_ADDR_SCAN_OPT_PH1,        STK_SCAN_OPT_PH1_VALUE},
    {STK_ADDR_SCAN_OPT_PH2,        STK_SCAN_OPT_PH2_VALUE},
    {STK_ADDR_SCAN_OPT_PH3,        STK_SCAN_OPT_PH3_VALUE},
    {STK_ADDR_SCAN_OPT_PH4,        STK_SCAN_OPT_PH4_VALUE},
    {STK_ADDR_SCAN_OPT_PH5,        STK_SCAN_OPT_PH5_VALUE},
    {STK_ADDR_SCAN_OPT_PH6,        STK_SCAN_OPT_PH6_VALUE},
    {STK_ADDR_SCAN_OPT_PH7,        STK_SCAN_OPT_PH7_VALUE},

    //TX CTRL
    {STK_ADDR_TX_CTRL_PH0,         STK_TX_CTRL_PH0_VALUE},
    {STK_ADDR_TX_CTRL_PH1,         STK_TX_CTRL_PH1_VALUE},
    {STK_ADDR_TX_CTRL_PH2,         STK_TX_CTRL_PH2_VALUE},
    {STK_ADDR_TX_CTRL_PH3,         STK_TX_CTRL_PH3_VALUE},
    {STK_ADDR_TX_CTRL_PH4,         STK_TX_CTRL_PH4_VALUE},
    {STK_ADDR_TX_CTRL_PH5,         STK_TX_CTRL_PH5_VALUE},
    {STK_ADDR_TX_CTRL_PH6,         STK_TX_CTRL_PH6_VALUE},
    {STK_ADDR_TX_CTRL_PH7,         STK_TX_CTRL_PH7_VALUE},

    //SENS_CTRL
    {STK_ADDR_SENS_CTRL_PH0,       STK_SENS_CTRL_PH0_VALUE},
    {STK_ADDR_SENS_CTRL_PH1,       STK_SENS_CTRL_PH1_VALUE},
    {STK_ADDR_SENS_CTRL_PH2,       STK_SENS_CTRL_PH2_VALUE},
    {STK_ADDR_SENS_CTRL_PH3,       STK_SENS_CTRL_PH3_VALUE},
    {STK_ADDR_SENS_CTRL_PH4,       STK_SENS_CTRL_PH4_VALUE},
    {STK_ADDR_SENS_CTRL_PH5,       STK_SENS_CTRL_PH5_VALUE},
    {STK_ADDR_SENS_CTRL_PH6,       STK_SENS_CTRL_PH6_VALUE},
    {STK_ADDR_SENS_CTRL_PH7,       STK_SENS_CTRL_PH7_VALUE},

    //FILTER_CFG_SETTING
    {STK_ADDR_FILT_CFG_PH0,       STK_FILT_CFG_PH0_VALUE},
    {STK_ADDR_FILT_CFG_PH1,       STK_FILT_CFG_PH1_VALUE},
    {STK_ADDR_FILT_CFG_PH2,       STK_FILT_CFG_PH2_VALUE},
    {STK_ADDR_FILT_CFG_PH3,       STK_FILT_CFG_PH3_VALUE},
    {STK_ADDR_FILT_CFG_PH4,       STK_FILT_CFG_PH4_VALUE},
    {STK_ADDR_FILT_CFG_PH5,       STK_FILT_CFG_PH5_VALUE},
    {STK_ADDR_FILT_CFG_PH6,       STK_FILT_CFG_PH6_VALUE},
    {STK_ADDR_FILT_CFG_PH7,       STK_FILT_CFG_PH7_VALUE},

    //CORRECTION
    {STK_ADDR_CORRECTION_PH0,     STK_CORRECTION_PH0_VALUE},
    {STK_ADDR_CORRECTION_PH1,     STK_CORRECTION_PH1_VALUE},
    {STK_ADDR_CORRECTION_PH2,     STK_CORRECTION_PH2_VALUE},
    {STK_ADDR_CORRECTION_PH3,     STK_CORRECTION_PH3_VALUE},
    {STK_ADDR_CORRECTION_PH4,     STK_CORRECTION_PH4_VALUE},
    {STK_ADDR_CORRECTION_PH5,     STK_CORRECTION_PH5_VALUE},
    {STK_ADDR_CORRECTION_PH6,     STK_CORRECTION_PH6_VALUE},
    {STK_ADDR_CORRECTION_PH7,     STK_CORRECTION_PH7_VALUE},

    {STK_ADDR_CORR_ENGA_0,        0x4},//PH5&6  ref PH4
    {STK_ADDR_CORR_ENGA_1,        0x0},
    {STK_ADDR_CORR_ENGB_0,        0x2},//PH3    ref PH2
    {STK_ADDR_CORR_ENGB_1,        0x0},
    {STK_ADDR_CORR_ENGC_0,        0x0},//PH1    ref PH0
    {STK_ADDR_CORR_ENGC_1,        0x0},
    {STK_ADDR_CORR_ENGD_0,        0x0},
    {STK_ADDR_CORR_ENGD_1,        0x0},

    //NOISE DET
    {STK_ADDR_NOISE_DECT_PH0,     STK_NOISE_DECT_PH0_VALUE},
    {STK_ADDR_NOISE_DECT_PH1,     STK_NOISE_DECT_PH1_VALUE},
    {STK_ADDR_NOISE_DECT_PH2,     STK_NOISE_DECT_PH2_VALUE},
    {STK_ADDR_NOISE_DECT_PH3,     STK_NOISE_DECT_PH3_VALUE},
    {STK_ADDR_NOISE_DECT_PH4,     STK_NOISE_DECT_PH4_VALUE},
    {STK_ADDR_NOISE_DECT_PH5,     STK_NOISE_DECT_PH5_VALUE},
    {STK_ADDR_NOISE_DECT_PH6,     STK_NOISE_DECT_PH6_VALUE},
    {STK_ADDR_NOISE_DECT_PH7,     STK_NOISE_DECT_PH7_VALUE},

    //CADC_OPTION
    {STK_ADDR_CADC_OPT0_PH0,     STK_CADC_OPT0_PH0_VALUE},
    {STK_ADDR_CADC_OPT0_PH1,     STK_CADC_OPT0_PH1_VALUE},
    {STK_ADDR_CADC_OPT0_PH2,     STK_CADC_OPT0_PH2_VALUE},
    {STK_ADDR_CADC_OPT0_PH3,     STK_CADC_OPT0_PH3_VALUE},
    {STK_ADDR_CADC_OPT0_PH4,     STK_CADC_OPT0_PH4_VALUE},
    {STK_ADDR_CADC_OPT0_PH5,     STK_CADC_OPT0_PH5_VALUE},
    {STK_ADDR_CADC_OPT0_PH6,     STK_CADC_OPT0_PH6_VALUE},
    {STK_ADDR_CADC_OPT0_PH7,     STK_CADC_OPT0_PH7_VALUE},

    //START UP THERSHOLD
    {STK_ADDR_STARTUP_THD_PH0,   STK_STARTUP_THD_PH0_VALUE},
    {STK_ADDR_STARTUP_THD_PH1,   STK_STARTUP_THD_PH1_VALUE},
    {STK_ADDR_STARTUP_THD_PH2,   STK_STARTUP_THD_PH2_VALUE},
    {STK_ADDR_STARTUP_THD_PH3,   STK_STARTUP_THD_PH3_VALUE},
    {STK_ADDR_STARTUP_THD_PH4,   STK_STARTUP_THD_PH4_VALUE},
    {STK_ADDR_STARTUP_THD_PH5,   STK_STARTUP_THD_PH5_VALUE},
    {STK_ADDR_STARTUP_THD_PH6,   STK_STARTUP_THD_PH6_VALUE},
    {STK_ADDR_STARTUP_THD_PH7,   STK_STARTUP_THD_PH7_VALUE},

    //PROX_CTRL_0
    {STK_ADDR_PROX_CTRL0_PH0,   STK_PROX_CTRL0_PH0_VALUE},
    {STK_ADDR_PROX_CTRL0_PH1,   STK_PROX_CTRL0_PH1_VALUE},
    {STK_ADDR_PROX_CTRL0_PH2,   STK_PROX_CTRL0_PH2_VALUE},
    {STK_ADDR_PROX_CTRL0_PH3,   STK_PROX_CTRL0_PH3_VALUE},
    {STK_ADDR_PROX_CTRL0_PH4,   STK_PROX_CTRL0_PH4_VALUE},
    {STK_ADDR_PROX_CTRL0_PH5,   STK_PROX_CTRL0_PH5_VALUE},
    {STK_ADDR_PROX_CTRL0_PH6,   STK_PROX_CTRL0_PH6_VALUE},
    {STK_ADDR_PROX_CTRL0_PH7,   STK_PROX_CTRL0_PH7_VALUE},

    //PROX_CTRL_1
    {STK_ADDR_PROX_CTRL1_PH0,   STK_PROX_CTRL1_PH0_VALUE},
    {STK_ADDR_PROX_CTRL1_PH1,   STK_PROX_CTRL1_PH1_VALUE},
    {STK_ADDR_PROX_CTRL1_PH2,   STK_PROX_CTRL1_PH2_VALUE},
    {STK_ADDR_PROX_CTRL1_PH3,   STK_PROX_CTRL1_PH3_VALUE},
    {STK_ADDR_PROX_CTRL1_PH4,   STK_PROX_CTRL1_PH4_VALUE},
    {STK_ADDR_PROX_CTRL1_PH5,   STK_PROX_CTRL1_PH5_VALUE},
    {STK_ADDR_PROX_CTRL1_PH6,   STK_PROX_CTRL1_PH6_VALUE},
    {STK_ADDR_PROX_CTRL1_PH7,   STK_PROX_CTRL1_PH7_VALUE},
    //set each phase end

    //ADAPTIVE BASELINE FILTER
    {STK_ADDR_ADP_BASELINE_0,   STK_ADP_BASELINE_0_VALUE},
    {STK_ADDR_ADP_BASELINE_1,   STK_ADP_BASELINE_1_VALUE},
    {STK_ADDR_ADP_BASELINE_2,   STK_ADP_BASELINE_2_VALUE},

    //DELTA DES CTRL
    {STK_ADDR_DELTADES_A_CTRL,   0x0},
    {STK_ADDR_DELTADES_B_CTRL,   0x0},
    {STK_ADDR_DELTADES_C_CTRL,   0x0},

    //RX NL CTRL
    {STK_ADDR_RX_NL_CTRL,        0x03FF03A0},

    //CUSTOM_SETTING
    {STK_ADDR_CUSTOM_A_CTRL0,   0x0},
    {STK_ADDR_CUSTOM_A_CTRL1,   0x0},
    {STK_ADDR_CUSTOM_B_CTRL0,   0x0},
    {STK_ADDR_CUSTOM_B_CTRL1,   0x0},
    {STK_ADDR_CUSTOM_C_CTRL0,   0x0},
    {STK_ADDR_CUSTOM_C_CTRL1,   0x0},
    {STK_ADDR_CUSTOM_D_CTRL0,   0x0},
    {STK_ADDR_CUSTOM_D_CTRL1,   0x0},

    //DISABLE SMOTH CADC , unlock OTP
    {STK_ADDR_INHOUSE_CMD,   0xA},
    {STK_ADDR_TRIM_LOCK,     0xA5},
    {STK_ADDR_CADC_SMOOTH,   0x0},
    {0x0740,                 0x0413AA},
    {STK_ADDR_TRIM_LOCK,     0x5A},
    {STK_ADDR_INHOUSE_CMD,   0x5},

    //CADC DEGLITCH
    {STK_ADDR_FAIL_STAT_DET_2, STK_FAIL_STAT_DET_2_VALUE}, //update when CADC change more than 5 times

    //IRQ
    {
        STK_ADDR_IRQ_SOURCE_ENABLE_REG, (1 << STK_IRQ_SOURCE_ENABLE_REG_CLOSE_ANY_IRQ_EN_SHIFT) |
        (1 << STK_IRQ_SOURCE_ENABLE_REG_FAR_ANY_IRQ_EN_SHIFT) | (1 << STK_IRQ_SOURCE_ENABLE_REG_PHRST_IRQ_EN_SHIFT)
        | (1 << STK_IRQ_SOURCE_ENABLE_REG_SATURATION_IRQ_EN_SHIFT)
#ifdef STK_STARTUP_CALI
        | (1 << STK_IRQ_SOURCE_ENABLE_REG_CONVDONE_IRQ_EN_SHIFT)
#endif
#ifdef TEMP_COMPENSATION
        | (1 << STK_IRQ_SOURCE_ENABLE_REG_DELTA_DES_IRQ_EN_SHIFT)
#endif
    },
#ifdef TEMP_COMPENSATION
    {STK_ADDR_DELTADES_A_CTRL, STK_ADDR_DELTADES_A_CTRL_VALUE}, //enable delta descend A
    {STK_ADDR_DELTADES_B_CTRL, STK_ADDR_DELTADES_B_CTRL_VALUE}, //enable delta descend B
#endif
};

/****************************************************************************************************
* 16bit register address function
****************************************************************************************************/
int32_t stk501xx_read(struct stk_data* stk, unsigned short addr, void *buf)
{
    return STK_REG_READ_BLOCK(stk, addr, 4, buf);
}

int32_t stk501xx_write(struct stk_data* stk, unsigned short addr, unsigned char* val)
{
    return STK_REG_WRITE_BLOCK(stk, addr, val, 4);
}

/****************************************************************************************************
* SAR control API
****************************************************************************************************/
static int32_t stk_register_queue(struct stk_data *stk)
{
#ifdef STK_INTERRUPT_MODE
    int8_t err = 0;
//#ifdef STK_MTK
//
//    // need to request int_pin in sar and use common_gpio_mtk.c
//    if (gpio_request(stk->int_pin, "stk_sar_int"))
//    {
//        STK_ERR("gpio_request failed");
//        return -1;
//    }
//
//#endif
    STK_ERR("gpio_request int32_t=%d", stk->gpio_info.int_pin);
    strcpy(stk->gpio_info.wq_name, "stk_sar_int");
    strcpy(stk->gpio_info.device_name, "stk_sar_irq");
    stk->gpio_info.gpio_cb = stk_work_queue;
    stk->gpio_info.trig_type = TRIGGER_FALLING;
    stk->gpio_info.is_active = false;
    stk->gpio_info.is_exist = false;
    stk->gpio_info.user_data = stk;
    err = STK_GPIO_IRQ_REGISTER(stk, &stk->gpio_info);
    err |= STK_GPIO_IRQ_START(stk, &stk->gpio_info);

    if (0 > err)
    {
        return -1;
    }

#endif /* STK_INTERRUPT_MODE */
#ifdef STK_POLLING_MODE
    strcpy(stk->stk_timer_info.wq_name, "stk_wq");
    stk->stk_timer_info.timer_unit = U_SECOND;
    stk->stk_timer_info.interval_time = STK_POLLING_TIME;
    stk->stk_timer_info.timer_cb = stk_work_queue;
    stk->stk_timer_info.is_active = false;
    stk->stk_timer_info.is_exist = false;
    stk->stk_timer_info.user_data = stk;
    STK_TIMER_REGISTER(stk, &stk->stk_timer_info);
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
    return 0;
}

#ifdef TEMP_COMPENSATION
void temperature_compensation(struct stk_data *stk, uint32_t int_flag, uint16_t prox_flag)
{
    uint32_t delta_des = 0, val = 0;
    uint8_t descent_work = 0;

    if (int_flag & STK_IRQ_SOURCE_FAR_IRQ_MASK)
    {
        if (~(prox_flag) & (1 << DELTA_A_MEASURE_PHASE))
        {
            STK_LOG("DELTA_A_MEASURE_PHASE far\n");
            stk->last_prox_a_state = 0;
        }

        if (~(prox_flag) & (1 << DELTA_B_MEASURE_PHASE))
        {
            STK_LOG("DELTA_B_MEASURE_PHASE far\n");
            stk->last_prox_b_state = 0;
        }
    }

    if (int_flag & STK_IRQ_SOURCE_CLOSE_IRQ_MASK)
    {
        if ((prox_flag & (1 << DELTA_A_MEASURE_PHASE)) &&
            (stk->last_prox_a_state != 1))
        {
            stk501xx_read_temp_data(stk, DELTA_A_MAPPING_PHASE_REG, &stk->prev_temperature_ref_a);
            STK_LOG("DELTA_A_MEASURE_PHASE close, 1st temp is %d\n", stk->prev_temperature_ref_a);
            stk->last_prox_a_state = 1;
        }

        if ((prox_flag & (1 << DELTA_B_MEASURE_PHASE)) &&
            (stk->last_prox_b_state != 1))
        {
            stk501xx_read_temp_data(stk, DELTA_B_MAPPING_PHASE_REG, &stk->prev_temperature_ref_b);
            STK_LOG("DELTA_B_MEASURE_PHASE close, 1st temp is %d\n", stk->prev_temperature_ref_b);
            stk->last_prox_b_state = 1;
        }
    }
    else if (int_flag & STK_IRQ_SOURCE_ENABLE_REG_DELTA_DES_IRQ_EN_MASK)
    {
        STK_REG_READ(stk, STK_ADDR_DETECT_STATUS_4, (uint8_t*)&delta_des);

        if (delta_des & STK_DETECT_STATUS_4_DES_STAT_A_MASK)
        {
            stk501xx_read_temp_data(stk, DELTA_A_MAPPING_PHASE_REG, &stk->next_temperature_ref_a);
            STK_LOG("DELTA_A_MEASURE_PHASE descent, 2nd temp is %d\n", stk->next_temperature_ref_a);

            if ( STK_ABS(STK_ABS(stk->prev_temperature_ref_a) - STK_ABS(stk->next_temperature_ref_a)) >= DELTA_TEMP_THD_A)
            {
                STK_REG_READ(stk, STK_ADDR_FILT_CFG_PH1, (uint8_t*)&val);
                val |= BASE_REINIT_DELTA_DES;
                STK_REG_WRITE(stk, STK_ADDR_FILT_CFG_PH1, (uint8_t*)&val);

                STK_REG_READ(stk, STK_ADDR_FILT_CFG_PH2, (uint8_t*)&val);
                val |= BASE_REINIT_DELTA_DES;
                STK_REG_WRITE(stk, STK_ADDR_FILT_CFG_PH2, (uint8_t*)&val);

                stk->reinit[1] = stk->reinit[2] = 1;
                stk->last_prox_a_state = 0;
            }
            else //reset descent
            {
                STK_REG_READ(stk, STK_ADDR_IRQ_SOURCE_ENABLE_REG, (uint8_t*)&val);
                val = ~((~val) | STK_IRQ_SOURCE_ENABLE_REG_DELTA_DES_IRQ_EN_SHIFT);
                STK_REG_WRITE(stk, STK_ADDR_IRQ_SOURCE_ENABLE_REG, (uint8_t*)&val);
                //waiting 1~2 frame
                STK_TIMER_BUSY_WAIT(stk, 300, MS_DELAY);

                val |= STK_IRQ_SOURCE_ENABLE_REG_DELTA_DES_IRQ_EN_SHIFT;
                STK_REG_WRITE(stk, STK_ADDR_IRQ_SOURCE_ENABLE_REG, (uint8_t*)&val);
                descent_work = 1;
            }
        }

        if (delta_des & STK_DETECT_STATUS_4_DES_STAT_B_MASK)
        {
            stk501xx_read_temp_data(stk, DELTA_B_MAPPING_PHASE_REG, &stk->next_temperature_ref_b);
            STK_LOG("DELTA_B_MEASURE_PHASE descent, 2nd temp is %d\n", stk->next_temperature_ref_b);

            if ( STK_ABS(STK_ABS(stk->prev_temperature_ref_b) - STK_ABS(stk->next_temperature_ref_b)) >= DELTA_TEMP_THD_B)
            {
                STK_REG_READ(stk, STK_ADDR_FILT_CFG_PH3, (uint8_t*)&val);
                val |= BASE_REINIT_DELTA_DES;
                STK_REG_WRITE(stk, STK_ADDR_FILT_CFG_PH3, (uint8_t*)&val);

                STK_REG_READ(stk, STK_ADDR_FILT_CFG_PH5, (uint8_t*)&val);
                val |= BASE_REINIT_DELTA_DES;
                STK_REG_WRITE(stk, STK_ADDR_FILT_CFG_PH5, (uint8_t*)&val);

                STK_REG_READ(stk, STK_ADDR_FILT_CFG_PH6, (uint8_t*)&val);
                val |= BASE_REINIT_DELTA_DES;
                STK_REG_WRITE(stk, STK_ADDR_FILT_CFG_PH6, (uint8_t*)&val);

                stk->reinit[3] = stk->reinit[5] = stk->reinit[6] = 1;
                stk->last_prox_b_state = 0;
            }
            else //reset descent
            {
                if(!descent_work)
                {
                    STK_REG_READ(stk, STK_ADDR_IRQ_SOURCE_ENABLE_REG, (uint8_t*)&val);
                    val = ~((~val) | STK_IRQ_SOURCE_ENABLE_REG_DELTA_DES_IRQ_EN_SHIFT);
                    STK_REG_WRITE(stk, STK_ADDR_IRQ_SOURCE_ENABLE_REG, (uint8_t*)&val);
                    //waiting 1~2 frame
                    STK_TIMER_BUSY_WAIT(stk, 300, MS_DELAY);

                    val |= STK_IRQ_SOURCE_ENABLE_REG_DELTA_DES_IRQ_EN_SHIFT;
                    STK_REG_WRITE(stk, STK_ADDR_IRQ_SOURCE_ENABLE_REG, (uint8_t*)&val);
                }
            }
        }
    }
}

static void clr_temp(struct stk_data* stk)
{
    stk->prev_temperature_ref_a = 0;
    stk->next_temperature_ref_a = 0;
    stk->prev_temperature_ref_b = 0;
    stk->next_temperature_ref_b = 0;
    stk->last_prox_a_state = 0;
    stk->last_prox_b_state = 0;
}
#endif

#ifdef STK_STARTUP_CALI

uint32_t fac_raw_avg[4] = {0}; // 0=ref_0, 1=measure_0, 2=ref_1, 3=measure_1, stored from factory.

void stk501xx_update_startup(struct stk_data* stk)
{
    uint32_t new_stup_thd[2] = {0};
    int32_t prox_thd[2] = {0};
    uint16_t reg = 0 ;
    uint32_t raw_data[4] = {0};// 0=ref_0, 1=measure_0, 2=ref_1, 3=measure_1
    uint32_t val = 0;
    int32_t raw_diff, temp_adjust;

    //dis conv done
    STK_REG_READ(stk, STK_ADDR_IRQ_SOURCE_ENABLE_REG, (uint8_t*)&val);
    val = ~((~val) | STK_IRQ_SOURCE_ENABLE_REG_CONVDONE_IRQ_EN_MASK);
    STK_REG_WRITE(stk, STK_ADDR_IRQ_SOURCE_ENABLE_REG, (uint8_t*)&val);

    // read raw
    for (int8_t i = 0; i < 4; i++)
    {
        reg = (uint16_t)(STK_ADDR_REG_RAW_PH0_REG + (i * 0x04));
        STK_REG_READ(stk, reg, (uint8_t*)&raw_data[i]);
        STK_LOG("stk501xx_update_startup:: now raw[%d]=%d", i, raw_data[i]);
    }

    prox_thd[0] = STK_SAR_THD_1;
    prox_thd[1] = STK_SAR_THD_3;

    if (fac_raw_avg[0] && fac_raw_avg[1] && fac_raw_avg[2] && fac_raw_avg[3])
    {
        STK_LOG("stk501xx_update_startup:: fac_raw_avg(%d, %d, %d, %d)",
                (uint32_t)fac_raw_avg[0], (uint32_t)fac_raw_avg[1], (uint32_t)fac_raw_avg[2], (uint32_t)fac_raw_avg[3]);

        //caculate startup thd
        raw_diff = (int32_t)(raw_data[0] - fac_raw_avg[0]);
        if (raw_data[0] > fac_raw_avg[0])
        {
            temp_adjust = (int32_t)((float)(STK_COEF_T_POS_PH1 / 128.0f) * raw_diff);
        }
        else
        {
            temp_adjust = (int32_t)((float)(STK_COEF_T_NEG_PH1 / 128.0f) * raw_diff);
        }
        new_stup_thd[0] = ((uint32_t)((int32_t)fac_raw_avg[1] + temp_adjust + prox_thd[0]));

        raw_diff = (int32_t)(raw_data[2] - fac_raw_avg[2]);
        if (raw_data[2] > fac_raw_avg[2])
        {
            temp_adjust = (int32_t)((float)(STK_COEF_T_POS_PH3 / 128.0f) * raw_diff);
        }
        else
        {
            temp_adjust = (int32_t)((float)(STK_COEF_T_NEG_PH3 / 128.0f) * raw_diff);
        }
        new_stup_thd[1] = ((uint32_t)((int32_t)fac_raw_avg[3] + temp_adjust + prox_thd[1]));

        STK_LOG(HIGH, instance, "stk501xx_update_startup:: clr_raw+thd=%d, %d", new_stup_thd[0], new_stup_thd[1]);
        new_stup_thd[0] = (uint32_t)((new_stup_thd[0] / 256) << 8);
        new_stup_thd[1] = (uint32_t)((new_stup_thd[1] / 256) << 8);
        reg = STK_ADDR_STARTUP_THD_PH1;
        val = new_stup_thd[0] | 0x20;
        STK_REG_WRITE(stk, reg, (uint8_t*)&val);
        reg = STK_ADDR_STARTUP_THD_PH3;
        val = new_stup_thd[1] | 0x20;
        STK_REG_WRITE(stk, reg, (uint8_t*)&val);
        reg = STK_ADDR_TRIGGER_CMD;
        val = STK_TRIGGER_CMD_REG_BY_PHRST;
        STK_REG_WRITE(stk, reg, (uint8_t*)&val);
        //force read again
        STK_REG_READ(stk, STK_ADDR_TRIGGER_CMD, (uint8_t*)&val);
    }
    else
    {
        STK_LOG("stk501xx_update_startup:: no factory rawdata found, skip startup");
    }
}
#endif

static uint32_t stk_sqrt(int32_t delta_value)
{
    int32_t root, bit;
    root = 0;

    for (bit = 0x40000000; bit > 0; bit >>= 2)
    {
        int32_t trial = root + bit;
        root >>= 1;

        if (trial <= delta_value)
        {
            root += bit;
            delta_value -= trial;
        }
    }

    return (uint32_t)root;
}

static uint8_t stk501xx_check_thd(struct stk_data* stk, uint32_t sar_thd, int8_t denominator, uint8_t idx)
{
    int8_t deno;
    uint32_t set_val = 0;
    uint16_t reg;
    uint32_t val;
    
    for(deno = denominator; deno >=0; deno--)// 7 -> 0
    {
        set_val = stk_sqrt(sar_thd >> (9 - deno));
        if(set_val > 0xFF)
            continue;
        else
            break;
    }
    if(deno < 0)
    {
        STK_LOG("ph%d, sar_thd %d so huge, using default value 5000(gain is 4)\n", idx, sar_thd);
        set_val = 35;
        return set_val;
    }

    reg = (uint16_t)(STK_ADDR_PROX_CTRL1_PH0 + (idx * 0x40));
    STK_REG_READ(stk, reg, (uint8_t*)&val);
    val &= 0xFFFFFFF8;
    val |= deno;
    STK_LOG("ph%d,reg =0x%x, denominator =%d, ready set val = %d\n", idx, reg, deno, set_val);
    STK_REG_WRITE(stk, reg, (uint8_t*)&val);

    return set_val;
}

int8_t stk501xx_set_each_thd(struct stk_data* stk, uint8_t idx, uint32_t thd)
{
    uint16_t reg;
    int8_t denominator = 0;
    uint32_t val = 0;
    STK_LOG("stk501xx_set_each_thd\n");

    STK_REG_READ(stk, STK_ADDR_PROX_CTRL1_PH0, (uint8_t*)&val);
    val &= 0x07;

    denominator = (int8_t)val;

    //specific PH threshold
    reg = STK_ADDR_PROX_CTRL0_PH0 + (idx * 0x40);
    val = stk501xx_check_thd(stk, thd, denominator, idx);
    STK_REG_WRITE(stk, reg, (uint8_t*)&val);

    return 0;
}

static int8_t stk501xx_set_thd(struct stk_data* stk)
{
    uint8_t  i = 0;
    uint16_t reg;
    int8_t denominator = 0;
    uint32_t val = 0;
    STK_LOG("stk_sar_set_thd");

    //set threshold gain
    for (i = 0; i < 8; i++)
    {
        reg = (uint16_t)(STK_ADDR_PROX_CTRL1_PH0 + (i * 0x40));
        STK_REG_READ(stk, reg, (uint8_t*)&val);
        val |= DIST_GAIN_4;
        STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    }

    STK_REG_READ(stk, STK_ADDR_PROX_CTRL1_PH0, (uint8_t*)&val);
    val &= 0x07; //0x7 : *4 , 0x6: *8, 0x5: *16, 0x4: *32, 0x3: *64, 0x2: *128, 0x1: *256, 0x0: *512

    denominator = (int8_t)val;

    //PH0 threshold
    stk->sar_def_thd[0] = STK_SAR_THD_0;
    reg = STK_ADDR_PROX_CTRL0_PH0;
    val = stk501xx_check_thd(stk, STK_SAR_THD_0, denominator, 0);

    STK_REG_WRITE(stk, reg, (uint8_t*)&val);

    //PH1 threshold
    stk->sar_def_thd[1] = STK_SAR_THD_1;
    reg = STK_ADDR_PROX_CTRL0_PH1;
    val = stk501xx_check_thd(stk, STK_SAR_THD_1, denominator, 1);

    STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    //PH2 threshold
    stk->sar_def_thd[2] = STK_SAR_THD_2;
    reg = STK_ADDR_PROX_CTRL0_PH2;
    val = stk501xx_check_thd(stk, STK_SAR_THD_2, denominator, 2);

    STK_REG_WRITE(stk, reg, (uint8_t*)&val);

    //PH3 threshold
    stk->sar_def_thd[3] = STK_SAR_THD_3;
    reg = STK_ADDR_PROX_CTRL0_PH3;
    val = stk501xx_check_thd(stk, STK_SAR_THD_3, denominator, 3);

    STK_REG_WRITE(stk, reg, (uint8_t*)&val);

    //PH4 threshold
    stk->sar_def_thd[4] = STK_SAR_THD_4;
    reg = STK_ADDR_PROX_CTRL0_PH4;
    val = stk501xx_check_thd(stk, STK_SAR_THD_4, denominator, 4);

    STK_REG_WRITE(stk, reg, (uint8_t*)&val);

    //PH5 threshold
    stk->sar_def_thd[5] = STK_SAR_THD_5;
    reg = STK_ADDR_PROX_CTRL0_PH5;
    val = stk501xx_check_thd(stk, STK_SAR_THD_5, denominator, 5);

    STK_REG_WRITE(stk, reg, (uint8_t*)&val);

    //PH6 threshold
    stk->sar_def_thd[6] = STK_SAR_THD_6;
    reg = STK_ADDR_PROX_CTRL0_PH6;
    val = stk501xx_check_thd(stk, STK_SAR_THD_6, denominator, 6);

    STK_REG_WRITE(stk, reg, (uint8_t*)&val);

    //PH7 threshold
    stk->sar_def_thd[7] = STK_SAR_THD_7;
    reg = STK_ADDR_PROX_CTRL0_PH7;
    val = stk501xx_check_thd(stk, STK_SAR_THD_7, denominator, 7);

    STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    return 0;
}
static void stk_force_report_data(struct stk_data* stk, int8_t nf_flag)
{
    stk501xx_wrapper *stk_wrapper = container_of(stk, stk501xx_wrapper, stk);
    int32_t i = 0;

    if (!stk_wrapper->channels[i].input_dev)
    {
        STK_ERR("No input device for sar data");
        return;
    }

    wake_lock_timeout(&stk501xx_wake_lock, HZ * 1);
    for (i = 0; i < ch_num; i ++)
    {
        /*with ss sensor hal*/
        input_report_rel(stk_wrapper->channels[i].input_dev, REL_MISC, nf_flag);
        input_report_rel(stk_wrapper->channels[i].input_dev, REL_X, nf_flag);
//        input_report_abs(stk_wrapper->channels[i].input_dev, ABS_DISTANCE, SAR_STATE_FAR);
        input_sync(stk_wrapper->channels[i].input_dev);
#if SAR_IN_FRANCE
        stk->ch_status[i] = SAR_STATE_FAR;
#endif
        stk->grip_status[i] = SAR_STATE_FAR;
        STK_ERR("stk->grip_status[%d] = %d", i, stk->grip_status[i]);
    }
}


static void stk_chk_sar_en_irq(stk501xx_wrapper *stk_wrapper, uint8_t ch_idx)
{
    uint8_t i = 0;
    uint8_t has_ph_en = 0;

    //dis irq
    if(stk_wrapper->channels[ch_idx].enabled == 0)
    {
        stk501xx_set_each_thd(global_stk, mapping_phase[ch_idx], 33000000);
    }
    //en irq
    if(stk_wrapper->channels[ch_idx].enabled == 1)
    {
        stk501xx_set_each_thd(global_stk, mapping_phase[ch_idx], global_stk->sar_def_thd[mapping_phase[ch_idx]]);
    }

    //check sar enable or not
    for (i = 0; i < ch_num; i ++)
    {
        has_ph_en |= stk_wrapper->channels[i].enabled ;
    }

    if( !global_stk->enabled && has_ph_en) //SAR disable but any channel report enable
    {
        STK_ERR("Sar disabled but any channel report enable, enable Sar\n");
        stk501xx_set_enable(global_stk, 1);
        stk_force_report_data(global_stk, SAR_STATE_NEAR);
    }

    if( global_stk->enabled && !has_ph_en) //SAR enable but all channel report disable
    {
        STK_ERR("Sar enabled but all channel report disable, disable Sar\n");
        stk501xx_set_enable(global_stk, 0);
    }
}

#if SAR_IN_FRANCE
static int32_t stk501xx_read_offset_fadc(struct stk_data* stk, uint8_t idx)
{
    uint16_t rreg, creg;
    uint32_t raw_val, cadc_val;
    int32_t fadc = 0, err;

    if(stk->cadc_weight == 0)
    {
        //read CADC weight
        rreg = 0x0800;
        err = STK_REG_READ(stk, rreg, (uint8_t*)&raw_val);

        if (err < 0)
        {
            STK_ERR("read 0x800 fail");
            return 0;
        }
        stk->cadc_weight = raw_val & (0x7FF);
        STK_ERR("stk->cadc_weight =%d\n", stk->cadc_weight);
    }

    if(idx == 0)
    {
        rreg = (uint16_t)(STK_ADDR_REG_RAW_PH0_REG + ( 5 * 0x04)); //phase 5
        creg = (uint16_t)(STK_ADDR_REG_CADC_PH0_REG + ( 5 * 0x04)); //phase 5
    }
    else if(idx == 1)
    {
        rreg = (uint16_t)(STK_ADDR_REG_RAW_PH0_REG + ( 6 * 0x04)); //phase 6
        creg = (uint16_t)(STK_ADDR_REG_CADC_PH0_REG + ( 6 * 0x04)); //phase 6
    }
    else if(idx == 2)
    {
        rreg = (uint16_t)(STK_ADDR_REG_RAW_PH0_REG + ( 3 * 0x04)); //phase 3
        creg = (uint16_t)(STK_ADDR_REG_CADC_PH0_REG + ( 3 * 0x04)); //phase 3
    }
    else if(idx == 3)
    {
        rreg = (uint16_t)(STK_ADDR_REG_RAW_PH0_REG + ( 1 * 0x04)); //phase 1
        creg = (uint16_t)(STK_ADDR_REG_CADC_PH0_REG + ( 1 * 0x04)); //phase 1
    }
    //read raw data
    err = STK_REG_READ(stk, rreg, (uint8_t*)&raw_val);

    if (err < 0)
    {
        STK_ERR("read STK_ADDR_REG_RAW_PH0_REG fail");
        return 0;
    }
    STK_ERR("raw = %d", (int32_t)(raw_val));


    //read CADC data
    err = STK_REG_READ(stk, creg, (uint8_t*)&cadc_val);

    if (err < 0)
    {
        STK_ERR("read STK_ADDR_REG_CADC_PH0_REG fail");
        return 0;
    }
    STK_ERR("cadc = %d", (int32_t)(cadc_val));

    fadc = (raw_val/256) - (cadc_val * stk->cadc_weight);

    return fadc;
}
#endif
static void stk_clr_intr(struct stk_data* stk, uint32_t* flag)
{
    if (0 > STK_REG_READ(stk, STK_ADDR_IRQ_SOURCE, (uint8_t*)flag))
    {
        STK_ERR("read STK_ADDR_IRQ_SOURCE fail");
        return;
    }

    STK_ERR("stk_clr_intr:: state = 0x%x", *flag);
}

int32_t stk_read_prox_flag(struct stk_data* stk, uint32_t* prox_flag)
{
    int32_t ret = 0;
    ret = STK_REG_READ(stk, STK_ADDR_DETECT_STATUS_1, (uint8_t*)prox_flag);

    if (0 > ret)
    {
        STK_ERR("read STK_ADDR_DETECT_STATUS_1 fail");
        return ret;
    }

    STK_ERR("stk_read_prox_flag:: state = 0x%x", *prox_flag);
    *prox_flag &= STK_DETECT_STATUS_1_PROX_STATE_MASK;
    return ret;
}

void stk501xx_set_enable(struct stk_data* stk, char enable)
{
    uint8_t num = (sizeof(stk->state_change) / sizeof(uint8_t)), i;
    stk501xx_wrapper *stk_wrapper = container_of(stk, stk501xx_wrapper, stk);
    uint16_t reg = 0;
    uint32_t val = 0, flag = 0;
    STK_ERR("stk501xx_set_enable en=%d", enable);

    if (enable)
    {
        if(pause_mode == 1) {
            reg = STK_ADDR_IRQ_CONFIG;
            val = 0x0;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);

            reg = STK_ADDR_TRIM_LOCK;
            val = 0xA5;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);

            reg = STK_ADDR_INHOUSE_CMD;
            val = 0xA;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);

            reg = 0x800;
            STK_REG_READ(stk, reg, (uint8_t*)&val);
            val &= ~((~val) | 0x30000000);
            STK_ERR("stk501xx_set_enable after 0x800 = 0x%x\n", val);
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);

            reg = STK_ADDR_INHOUSE_CMD;
            val = 0x0;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);

            reg = STK_ADDR_TRIM_LOCK;
            val = 0x5A;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);
        } else {
            reg = STK_ADDR_TRIGGER_REG;
            val = STK_TRIGGER_REG_INIT_ALL;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);
            reg = STK_ADDR_TRIGGER_CMD;
            val = STK_TRIGGER_CMD_REG_INIT_ALL;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);
            //force read again
            STK_REG_READ(stk, STK_ADDR_TRIGGER_CMD, (uint8_t*)&val);
            pause_mode = 1;
        }
#ifdef STK_INTERRUPT_MODE
            /* do nothing */
#elif defined STK_POLLING_MODE
            STK_TIMER_START(stk, &stk->stk_timer_info);
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
#ifdef TEMP_COMPENSATION
            stk->last_prox_a_state = 0;
            stk->last_prox_b_state = 0;
#endif
            stk_wrapper->channels[0].enabled = stk_wrapper->channels[1].enabled = \
            stk_wrapper->channels[2].enabled = stk_wrapper->channels[3].enabled = 1;
    }
    else
    {

#ifdef STK_INTERRUPT_MODE
        /* do nothing */
#elif defined STK_POLLING_MODE
        STK_TIMER_STOP(stk, &stk->stk_timer_info);
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
        if(pause_mode == 1) {
            reg = STK_ADDR_TRIM_LOCK;
            val = 0xA5;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);

            reg = STK_ADDR_INHOUSE_CMD;
            val = 0xA;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);

            reg = 0x800;
            STK_REG_READ(stk, reg, (uint8_t*)&val);
            val |= 0x30000000;
            STK_ERR("stk501xx_set_enable after 0x800 = 0x%x\n", val);
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);

            reg = STK_ADDR_INHOUSE_CMD;
            val = 0x0;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);

            reg = STK_ADDR_TRIM_LOCK;
            val = 0x5A;
            STK_REG_WRITE(stk, reg, (uint8_t*)&val);

            STK_REG_READ(stk, STK_ADDR_IRQ_CONFIG, (uint8_t*)&val);
            val |= (1 << STK_IRQ_CONFIG_SENS_RATE_OPT_SHIFT);
            STK_REG_WRITE(stk, STK_ADDR_IRQ_CONFIG, (uint8_t*)&val);
        } else {
        for (i = 0; i < num; i++)
        {
            stk->last_nearby[i] = STK_SAR_NEAR_BY_UNKNOWN;
            stk->state_change[i] = 0;
        }
        //disable phase
        reg = STK_ADDR_TRIGGER_REG;
        val = STK_TRIGGER_REG_PHEN_DISABLE_ALL;
        STK_REG_WRITE(stk, reg, (uint8_t*)&val);
        reg = STK_ADDR_TRIGGER_CMD;
        val = STK_TRIGGER_CMD_REG_INIT_ALL;
        STK_REG_WRITE(stk, reg, (uint8_t*)&val);
        //force read again
        STK_REG_READ(stk, STK_ADDR_TRIGGER_CMD, (uint8_t*)&val);
        }
#ifdef TEMP_COMPENSATION
        clr_temp(stk);
#endif

    stk_wrapper->channels[0].enabled = stk_wrapper->channels[1].enabled = \
    stk_wrapper->channels[2].enabled = stk_wrapper->channels[3].enabled = 0;
    }

    stk->enabled = enable;
    stk_clr_intr(stk, &flag);
    STK_ERR("stk501xx_set_enable DONE");
}

void stk501xx_phase_reset(struct stk_data* stk, uint32_t phase_reset_reg)
{
    uint16_t reg = 0;
    uint32_t val = 0;
    STK_ERR("stk501xx_phase_reset");
    reg = STK_ADDR_TRIGGER_REG;
    STK_REG_WRITE(stk, reg, (uint8_t*)&phase_reset_reg);
    reg = STK_ADDR_TRIGGER_CMD;
    val = STK_TRIGGER_CMD_REG_BY_PHRST;
    STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    //force read again
    STK_REG_READ(stk, STK_ADDR_TRIGGER_CMD, (uint8_t*)&val);
}

void stk501xx_read_temp_data(struct stk_data* stk, uint16_t reg, int32_t *temperature)
{
    uint32_t val = 0;
    int32_t output_data = 0;
    int32_t err = 0;
    err = STK_REG_READ(stk, reg, (uint8_t*)&val);

    if (err < 0)
    {
        STK_ERR("read STK_ADDR_REG_RAW_PH1_REG fail");
        return;
    }

    if (val & 0x80000000)
    {
        //2's complement = 1's complement +1
        output_data = (int32_t)(~val + 1);
        output_data *= -1;
    }
    else
    {
        output_data = (int32_t)(val);
    }

    *temperature = output_data;
    STK_ERR("stk501xx_read_temp_data:: temp = %d(0x%X)", output_data, val);
}
void stk501xx_read_sar_data(struct stk_data* stk , uint32_t prox_flag)
{
    uint16_t reg;
    uint32_t raw_val[8], delta_val[8], cadc_val[8];
    int32_t delta_conv_data[8] = { 0 };
    int32_t i = 0;
    int32_t err = 0;
    uint8_t  num = (sizeof(stk->state_change) / sizeof(uint8_t));
    STK_LOG("stk501xx_read_sar_data start");

    for (i = 0; i < num; i++)
    {
        //read raw data
        reg = (uint16_t)(STK_ADDR_REG_RAW_PH0_REG + (i * 0x04));
        err = STK_REG_READ(stk, reg, (uint8_t*)&raw_val[i]);

        if (err < 0)
        {
            STK_ERR("read STK_ADDR_REG_RAW_PH0_REG fail");
            return;
        }

        STK_ERR("stk501xx_read_sar_data:: raw[%d] = %d", i, (int32_t)(raw_val[i]));
        stk->last_raw_data[i] = raw_val[i];
        //read delta data
        reg = (uint16_t)(STK_ADDR_REG_DELTA_PH0_REG + (i * 0x04));
        err = STK_REG_READ(stk, reg, (uint8_t*)&delta_val[i]);

        if (err < 0)
        {
            STK_ERR("read STK_ADDR_REG_DELTA_PH0_REG fail");
            return;
        }

        if (delta_val[i] & 0x80000000)
        {
            //2's complement = 1's complement +1
            delta_conv_data[i] = (int32_t)(~delta_val[i] + 1);
            delta_conv_data[i] *= -1;
        }
        else
        {
            delta_conv_data[i] = (int32_t)(delta_val[i]);
        }

        stk->last_data[i] = delta_conv_data[i];
        STK_ERR("stk501xx_read_sar_data:: delta[%d] = %d", i, delta_conv_data[i]);
        //read CADC data
        reg = (uint16_t)(STK_ADDR_REG_CADC_PH0_REG + (i * 0x04));
        err = STK_REG_READ(stk, reg, (uint8_t*)&cadc_val[i]);

        if (err < 0)
        {
            STK_ERR("read STK_ADDR_REG_CADC_PH0_REG fail");
            return;
        }

        STK_ERR("stk501xx_read_sar_data:: CADC[%d] = %d", i, cadc_val[i]);

        // prox_flag state
        if (prox_flag & (uint32_t)((1 << i) << 8) ) //near
        {
            if (STK_SAR_NEAR_BY != stk->last_nearby[i])
            {
                stk->state_change[i] = 1;
                stk->last_nearby[i] = STK_SAR_NEAR_BY;
            }
            else
            {
                stk->state_change[i] = 0;
            }
        }
        else //far
        {
            if (STK_SAR_FAR_AWAY != stk->last_nearby[i])
            {
                stk->state_change[i] = 1;
                stk->last_nearby[i] = STK_SAR_FAR_AWAY;
            }
            else
            {
                stk->state_change[i] = 0;
            }
        }
    }
#ifdef TEMP_COMPENSATION
    for (i = 0; i < num; i++)
    {
        if(stk->reinit[i])
        {
            if(stk->last_nearby[i] == STK_SAR_FAR_AWAY)
            {
                reg = STK_ADDR_FILT_CFG_PH0 + (i * 0x40);
                STK_REG_READ(stk, reg, (uint8_t*)&raw_val[0]);
                raw_val[0] = ~((~raw_val[0]) | BASE_REINIT_DELTA_DES);
                STK_REG_WRITE(stk, reg, (uint8_t*)&raw_val[0]);
                stk->reinit[i] = 0;
            }
        }
    }
#endif
}

/*
 * @brief: Initialize some data in stk_data.
 *
 * @param[in/out] stk: struct stk_data *
 */
void stk501xx_data_initialize(struct stk_data* stk)
{
    int32_t i = 0;
    uint8_t num = (sizeof(stk->state_change) / sizeof(uint8_t));

    stk->enabled = 0;
    memset(stk->last_data, 0, sizeof(stk->last_data));

    for (i = 0; i < num; i++)
    {
        stk->last_nearby[i] = STK_SAR_NEAR_BY_UNKNOWN;
        stk->state_change[i] = 0;
#ifdef TEMP_COMPENSATION
        stk->reinit[i] = 0;
#endif
    }

    STK_LOG("sar initial done");
}

/*
 * @brief: Read PID and write to stk_data.pid.
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return: Success or fail.
 *          0: Success
 *          others: Fail
 */
static int32_t stk_get_pid(struct stk_data* stk)
{
    int32_t err = 0;
    uint32_t val = 0;
    err = STK_REG_READ(stk, STK_ADDR_CHIP_INDEX, (uint8_t*)&val);

    if (err < 0)
    {
        STK_ERR("read STK_ADDR_CHIP_INDEX fail");
        return -1;
    }

    if ((val >> STK_CHIP_INDEX_CHIP_ID__SHIFT) != STK501XX_ID)
        return -1;

    stk->chip_id = (val & STK_CHIP_INDEX_CHIP_ID__MASK) >> STK_CHIP_INDEX_CHIP_ID__SHIFT;
    stk->chip_index = val & STK_CHIP_INDEX_F__MASK;
    return err;
}

/*
 * @brief: Read all register (0x0 ~ 0x3F)
 *
 * @param[in/out] stk: struct stk_data *
 * @param[out] show_buffer: record all register value
 *
 * @return: buffer length or fail
 *          positive value: return buffer length
 *          -1: Fail
 */
int32_t stk501xx_show_all_reg(struct stk_data* stk)
{
    int32_t reg_num, reg_count = 0;
    int32_t err = 0;
    uint64_t val = 0;
    uint64_t reg_array[] =
    {
        STK_ADDR_CHIP_INDEX,
        STK_ADDR_IRQ_SOURCE,
        STK_ADDR_IRQ_SOURCE_ENABLE_REG,
        STK_ADDR_TRIGGER_REG,
        STK_ADDR_RXIO0_MUX_REG,
        STK_ADDR_RXIO1_MUX_REG,
        STK_ADDR_RXIO2_MUX_REG,
        STK_ADDR_RXIO3_MUX_REG,
        STK_ADDR_RXIO4_MUX_REG,
        STK_ADDR_RXIO5_MUX_REG,
        STK_ADDR_RXIO6_MUX_REG,
        STK_ADDR_RXIO7_MUX_REG,
        STK_ADDR_PROX_CTRL0_PH0,
        STK_ADDR_PROX_CTRL0_PH1,
        STK_ADDR_PROX_CTRL0_PH2,
        STK_ADDR_PROX_CTRL0_PH3,
        STK_ADDR_PROX_CTRL0_PH4,
        STK_ADDR_PROX_CTRL0_PH5,
        STK_ADDR_PROX_CTRL0_PH6,
        STK_ADDR_PROX_CTRL0_PH7,
        STK_ADDR_DELTADES_A_CTRL,
        //0622 add
        STK_ADDR_SCAN_PERIOD,
        STK_ADDR_FAIL_STAT_DET_2,
        STK_ADDR_TX_CTRL_PH0,
        STK_ADDR_TX_CTRL_PH1,
        STK_ADDR_TX_CTRL_PH2,
        STK_ADDR_TX_CTRL_PH3,
        STK_ADDR_TX_CTRL_PH4,
        STK_ADDR_TX_CTRL_PH5,
        STK_ADDR_TX_CTRL_PH6,
        STK_ADDR_SENS_CTRL_PH0,
        STK_ADDR_SENS_CTRL_PH1,
        STK_ADDR_SENS_CTRL_PH2,
        STK_ADDR_SENS_CTRL_PH3,
        STK_ADDR_SENS_CTRL_PH4,
        STK_ADDR_SENS_CTRL_PH5,
        STK_ADDR_SENS_CTRL_PH6,

        STK_ADDR_CORRECTION_PH0,
        STK_ADDR_CORRECTION_PH1,
        STK_ADDR_CORRECTION_PH2,
        STK_ADDR_CORRECTION_PH3,
        STK_ADDR_CORRECTION_PH4,
        STK_ADDR_CORRECTION_PH5,
        STK_ADDR_CORRECTION_PH6,
    };
    reg_num = sizeof(reg_array) / sizeof(uint64_t);
    STK_ERR("stk501xx_show_all_reg::");

    for (reg_count = 0; reg_count < reg_num; reg_count++)
    {
        err = STK_REG_READ(stk, reg_array[reg_count], (uint8_t*)&val);

        if (err < 0)
        {
            return -1;
        }

        STK_ERR("reg_array[0x%04x] = 0x%x", reg_array[reg_count], val);
    }

    return 0;
}

static int32_t stk_reg_init(struct stk_data* stk)
{
    int32_t err = 0;
    uint16_t reg, reg_count = 0, reg_num = 0;
    uint32_t val;
    reg_num = sizeof(stk501xx_default_register_table) / sizeof(stk501xx_register_table);

    for (reg_count = 0; reg_count < reg_num; reg_count++)
    {
        reg = stk501xx_default_register_table[reg_count].address;
        val = stk501xx_default_register_table[reg_count].value;

        if ( reg == STK_ADDR_CADC_SMOOTH && stk->chip_index >= 0x1)
            val = 0xFF;

        err = STK_REG_WRITE(stk, reg, (uint8_t*)&val);

        if (err < 0)
            return err;
    }

    //enable phase
    reg = STK_ADDR_TRIGGER_REG;
    val = STK_TRIGGER_REG_INIT_ALL;
    STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    reg = STK_ADDR_TRIGGER_CMD;
    val = STK_TRIGGER_CMD_REG_INIT_ALL;
    STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    //force read again
    STK_REG_READ(stk, STK_ADDR_TRIGGER_CMD, (uint8_t*)&val);
    STK_TIMER_BUSY_WAIT(stk, 250, MS_DELAY); //wait one time scan.
#if SAR_IN_FRANCE
    if (stk->sar_first_boot)
    {
        stk->sar_first_fadc[0] = stk501xx_read_offset_fadc(stk, 0);
        stk->sar_first_fadc[1] = stk501xx_read_offset_fadc(stk, 1);
        stk->sar_first_fadc[2] = stk501xx_read_offset_fadc(stk, 2);
        stk->sar_first_fadc[3] = stk501xx_read_offset_fadc(stk, 3);
        //debug
        STK_ERR("ch0:backgrand_cap = %d", stk->sar_first_fadc[0]);
        STK_ERR("ch1:backgrand_cap = %d", stk->sar_first_fadc[1]);
        STK_ERR("ch2:backgrand_cap = %d", stk->sar_first_fadc[2]);
        STK_ERR("ch3:backgrand_cap = %d", stk->sar_first_fadc[3]);
        STK_ERR("stk->cadc_weight =%d\n", stk->cadc_weight);
    }
#endif
    // set power down for default
    stk501xx_set_enable(stk, 0);

    stk501xx_set_thd(stk);

    return 0;
}

/*
 * @brief: SW reset for stk501xx
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return: Success or fail.
 *          0: Success
 *          others: Fail
 */
int32_t stk501xx_sw_reset(struct stk_data* stk)
{
    int32_t err = 0, i = 0;
    uint16_t reg = STK_ADDR_TRIGGER_REG;
    uint32_t val = STK_TRIGGER_REG_PHEN_DISABLE_ALL;
    err = STK_REG_WRITE(stk, reg, (uint8_t*)&val);

    if (err < 0)
        return err;

    reg = STK_ADDR_TRIGGER_CMD;
    val = STK_TRIGGER_CMD_REG_INIT_ALL;
    STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    //force read again
    STK_REG_READ(stk, STK_ADDR_TRIGGER_CMD, &val);
    reg = STK_ADDR_CHIP_INDEX;

    for (i = 0; i < 2; i++)
    {
        STK_REG_READ(stk, reg, (uint8_t*)&val);
    }

    reg = STK_ADDR_INHOUSE_CMD;
    val = 0xA;
    STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    reg = 0x1000;
    val = 0xA;
    STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    reg = STK_ADDR_TRIGGER_REG;
    val = 0xFF;
    STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    reg = STK_ADDR_TRIGGER_CMD;
    val = 0xF;
    STK_REG_WRITE(stk, reg, (uint8_t*)&val);
    //force read again
    STK_REG_READ(stk, STK_ADDR_TRIGGER_CMD, &val);
    reg = 0x1004;

    for (i = 0; i < 9; i++)
    {
        STK_REG_READ(stk, reg, (uint8_t*)&val);

        if ( val & 0x10)
            break;
    }

    reg = 0x100C;
    val = 0x00;

    for (i = 0; i < 8; i++)
    {
        STK_REG_WRITE(stk, reg, (uint8_t*)&val);
        stk->last_nearby[i] = STK_SAR_NEAR_BY_UNKNOWN;
        stk->state_change[i] = 0;
    }

    reg = STK_ADDR_SOFT_RESET;
    val = STK_SOFT_RESET_CMD;
    err = STK_REG_WRITE(stk, reg, (uint8_t*)&val);

    if (err < 0)
        return err;

    STK_TIMER_BUSY_WAIT(stk, 20, MS_DELAY);
    return 0;
}

#if defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE
void stk_work_queue(void *stkdata)
{
    struct stk_data *stk = (struct stk_data*)stkdata;
    uint32_t flag = 0, prox_flag = 0;
#ifdef STK_INTERRUPT_MODE
    STK_ERR("stk_work_queue:: Interrupt mode");
#elif defined STK_POLLING_MODE
    STK_ERR("stk_work_queue:: Polling mode");
#endif // STK_INTERRUPT_MODE

    if (stk501xx_irq_from_suspend_flag == 1)
    {
        stk501xx_irq_from_suspend_flag = 0;
        msleep(50); // Ensure I2C is work after irq comming and current state is suspend ;
    }

    stk_clr_intr(stk, &flag);
    //read prox flag
    stk_read_prox_flag(stk, &prox_flag);
#ifdef STK_STARTUP_CALI

    if ( flag & STK_IRQ_SOURCE_ENABLE_REG_CONVDONE_IRQ_EN_MASK)
    {
        stk501xx_update_startup(stk);
        return ;
    }

#endif

    if (flag & STK_IRQ_SOURCE_SENSING_WDT_IRQ_MASK)
    {
        STK_ERR("sensing wdt trigger, (en: %d)\n", stk->enabled);
        if(stk->enabled)
        {
            stk501xx_sw_reset(stk);
            stk_clr_intr(stk, &flag);
            stk_reg_init(stk);
            stk501xx_set_enable(stk, true);
        }
        return;
    }

    if (flag & STK_IRQ_SOURCE_ENABLE_REG_SATURATION_IRQ_EN_MASK)
    {
        stk501xx_phase_reset(stk, STK_TRIGGER_REG_INIT_ALL);
#ifdef TEMP_COMPENSATION
        stk->last_prox_a_state = 0;
        stk->last_prox_b_state = 0;
#endif
    }

#ifdef TEMP_COMPENSATION
    temperature_compensation(stk, flag, (uint16_t)(prox_flag >> 8));
#endif
    stk501xx_read_sar_data(stk , prox_flag);

    if (flag & STK_IRQ_SOURCE_FAR_IRQ_MASK ||
        flag & STK_IRQ_SOURCE_CLOSE_IRQ_MASK)
    {
        STK501XX_SAR_REPORT(stk);
    }
}
#endif /* defined STK_INTERRUPT_MODE || defined STK_POLLING_MODE */
int32_t stk501xx_init_client(struct stk_data * stk)
{
    int32_t err = 0;
    uint32_t flag;
    STK_LOG("Start Initial stk501xx");
    /* SW reset */
    err = stk501xx_sw_reset(stk);

    if (err < 0)
    {
        STK_ERR("software reset error, err=%d", err);
        return err;
    }

    stk_clr_intr(stk, &flag);
    stk501xx_data_initialize(stk);
    err = stk_get_pid(stk);

    if (err < 0)
    {
        STK_ERR("stk_get_pid error, err=%d", err);
        return err;
    }

    STK_LOG("PID 0x%x index=0x%x", stk->chip_id, stk->chip_index);
    err = stk_reg_init(stk);

    if (err < 0)
    {
        STK_ERR("stk501xx reg initialization failed");
        return err;
    }

    stk_register_queue(stk);
    err = stk501xx_show_all_reg(stk);

    if (err < 0)
    {
        STK_ERR("stk501xx_show_all_reg error, err=%d", err);
        return err;
    }

    return 0;
}


/*class define */
static ssize_t class_stk_sar_enable_show(struct class *class,
                                     struct class_attribute *attr, char *buf)
{
    char en;
    en = global_stk->enabled;
    return scnprintf(buf, PAGE_SIZE, "enable = %d\n", en);
}

static ssize_t class_stk_sar_enable_store(struct class *class,
                                      struct class_attribute *attr, const char *buf, size_t count)
{
    unsigned int data;
    int error;
    error = kstrtouint(buf, 10, &data);

    if (error)
    {
        STK_ERR("kstrtoul failed, error=%d", error);
        return error;
    }

    STK_ERR("class_stk_sar_enable_store, data=%d", data);

    if ((1 == data) || (0 == data))
        stk501xx_set_enable(global_stk, data);
    else
        STK_ERR("invalid argument, en=%d", data);

    return count;
}


static ssize_t class_stk_value_show_ch0(struct class *class,
                                        struct class_attribute *attr, char *buf)
{
    int32_t ch0_diff = 0;
    int32_t ch0_cadc = 0;
    int32_t ch0_ref_cadc = 0;
    int32_t ret = 0;
    STK_ERR("class_stk_value_show_ch0");

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_DELTA_PH5_REG, (uint8_t*)&ch0_diff);

    if (0 > ret)
    {
        STK_ERR("read CH0_diff fail");
    }

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_CADC_PH5_REG, (uint8_t*)&ch0_cadc);//add 0628

    if (0 > ret)
    {
        STK_ERR("read ch0_cadc fail");
    }

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_CADC_PH4_REG, (uint8_t*)&ch0_ref_cadc);//add 0628

    if (0 > ret)
    {
        STK_ERR("read ch0_ref_cadc fail");
    }

    STK_ERR("CH0_background_cap=%d;CH0_refer_channel_cap=%d;CH0_diff=%d;\n",ch0_cadc, ch0_ref_cadc, ch0_diff);
    return scnprintf(buf, PAGE_SIZE, "CH0_background_cap=%d;CH0_refer_channel_cap=%d;CH0_diff=%d;\n", ch0_cadc, ch0_ref_cadc, ch0_diff);
}

static ssize_t class_stk_value_show_ch1(struct class *class,
                                        struct class_attribute *attr, char *buf)
{

    int32_t ch1_diff = 0;
    int32_t ch1_cadc = 0;
    int32_t ch1_ref_cadc = 0;
    int32_t ret = 0;
    STK_ERR("class_stk_value_show_ch1");

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_DELTA_PH6_REG, (uint8_t*)&ch1_diff);

    if (0 > ret)
    {
        STK_ERR("read CH1_diff fail");
    }

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_CADC_PH6_REG, (uint8_t*)&ch1_cadc);//add 0628

    if (0 > ret)
    {
        STK_ERR("read ch1_cadc fail");
    }

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_CADC_PH4_REG, (uint8_t*)&ch1_ref_cadc);//add 0628

    if (0 > ret)
    {
        STK_ERR("read ch1_ref_cadc fail");
    }

    STK_ERR("CH1_background_cap=%d;CH1_refer_channel_cap=%d;CH1_diff=%d;\n",ch1_cadc, ch1_ref_cadc, ch1_diff);
    return scnprintf(buf, PAGE_SIZE, "CH1_background_cap=%d;CH1_refer_channel_cap=%d;CH1_diff=%d;\n", ch1_cadc, ch1_ref_cadc, ch1_diff);
}

static ssize_t class_stk_value_show_ch2(struct class *class,
                                        struct class_attribute *attr, char *buf)
{
    int32_t ch2_diff = 0;
    int32_t ch2_cadc = 0;
    int32_t ch2_ref_cadc = 0;
    int32_t ret = 0;
    STK_ERR("class_stk_value_show_ch2");

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_DELTA_PH3_REG, (uint8_t*)&ch2_diff);

    if (0 > ret)
    {
        STK_ERR("read CH2_diff fail");
    }

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_CADC_PH3_REG, (uint8_t*)&ch2_cadc);//add 0628

    if (0 > ret)
    {
        STK_ERR("read ch2_cadc fail");
    }

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_CADC_PH2_REG, (uint8_t*)&ch2_ref_cadc);//add 0628

    if (0 > ret)
    {
        STK_ERR("read ch2_ref_cadc fail");
    }

    STK_ERR("CH2_background_cap=%d;CH2_refer_channel_cap=%d;CH2_diff=%d;\n",ch2_cadc, ch2_ref_cadc, ch2_diff);
    return scnprintf(buf, PAGE_SIZE, "CH2_background_cap=%d;CH2_refer_channel_cap=%d;CH2_diff=%d;\n", ch2_cadc, ch2_ref_cadc, ch2_diff);
}

static ssize_t class_stk_value_show_ch3(struct class *class,
                                        struct class_attribute *attr, char *buf)
{
    int32_t ch3_diff = 0;
    int32_t ch3_cadc = 0;
    int32_t ch3_ref_cadc = 0;
    int32_t ret = 0;
    STK_ERR("class_stk_value_show_ch3");

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_DELTA_PH1_REG, (uint8_t*)&ch3_diff);

    if (0 > ret)
    {
        STK_ERR("read CH3_diff fail");
    }

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_CADC_PH1_REG, (uint8_t*)&ch3_cadc);//add 0628

    if (0 > ret)
    {
        STK_ERR("read ch3_cadc fail");
    }

    ret = STK_REG_READ(global_stk, STK_ADDR_REG_CADC_PH0_REG, (uint8_t*)&ch3_ref_cadc);//add 0628

    if (0 > ret)
    {
        STK_ERR("read ch3_ref_cadc fail");
    }

    STK_ERR("CH3_background_cap=%d;CH3_refer_channel_cap=%d;CH3_diff=%d;\n",ch3_cadc, ch3_ref_cadc, ch3_diff);
    return scnprintf(buf, PAGE_SIZE, "CH3_background_cap=%d;CH3_refer_channel_cap=%d;CH3_diff=%d;\n", ch3_cadc, ch3_ref_cadc, ch3_diff);
}
static ssize_t stk_enable_store_power_enable(struct stk501xx_wrapper *stk_wrapper, uint8_t count, uint32_t en)
{
    stk_data *stk = &stk_wrapper->stk;
    uint32_t val = 0;
    uint8_t i;

    STK_ERR("stk_enable_store_power_enable, en=%d\n", en);
    i = count;
    if(count == 1){
        return 0;
    }
    stk_wrapper->channels[i].enabled = en;
    STK_ERR("stk_wrapper->channels[%d].enabled = %d\n",i ,stk_wrapper->channels[i].enabled);
    if(en)
    {
        STK_ERR("stk->grip_status[%d] = %d", i, global_stk->grip_status[i]);
        global_stk->grip_status[i] = SAR_STATE_NONE;
        STK_ERR("stk->grip_status[%d] = %d", i, global_stk->grip_status[i]);

        stk->state_change[mapping_phase[i]] = 1;       //force update
        stk->ch_status[i] = SAR_STATE_FAR;
#if SAR_IN_FRANCE
        stk->ch_status[i] = SAR_STATE_FAR;
#endif
    } else {
        if(stk->last_nearby[mapping_phase[i]] == STK_SAR_NEAR_BY)
            stk_wrapper->channels[i].prev_nearby = SAR_STATE_NEAR;
        else
            stk_wrapper->channels[i].prev_nearby = SAR_STATE_FAR;
    }
    //debug
    STK_ERR("stk_enable_store_power_enable\n");
    if(stk_wrapper->channels[0].enabled != stk_wrapper->channels[1].enabled) {
        STK_ERR("stk_chag11 en[0] = %d\n",stk_wrapper->channels[0].enabled);
        stk_wrapper->channels[1].enabled = stk_wrapper->channels[0].enabled;
        STK_ERR("stk_chag11 en[1] = %d\n",stk_wrapper->channels[1].enabled);
    }

    stk_report_sar_data(stk);
    stk_clr_intr(stk, &val);
    return 0;
}

static ssize_t class_stk_power_en(struct class *class,
        struct class_attribute *attr, const char *buf, size_t count)
{
    unsigned int en;
    uint8_t i = 0;
    int error;
    stk501xx_wrapper *stk_wrapper = container_of(global_stk, stk501xx_wrapper, stk);
    error = kstrtouint(buf, 10, &en);
    if (error)
    {
        STK_ERR("kstrtoul failed, error=%d", error);
        return error;
    }
    if ((1 == en) )
    {
        //stk501xx_set_enable(global_stk, en);
        for (i = 0; i < ch_num; i ++)
        {
            stk_enable_store_power_enable(stk_wrapper, i, en);
        }

        global_stk->power_en = en;
        STK_ERR("class_stk_power_en, en=%d", global_stk->power_en);
        if(en)
        stk_force_report_data(global_stk, SAR_STATE_FAR);
    } else if (0 == en) {
        for (i = 0; i < ch_num; i ++)
        {
            stk_enable_store_power_enable(stk_wrapper, i, en);
        }
        global_stk->power_en = en;
        STK_ERR("class_stk_power_en, en=%d", global_stk->power_en);
    }
    else
    {
        STK_ERR("invalid argument, en=%d", en);
    }

    return count;
}
static ssize_t class_stk_power_en_show(struct class *class,
                                   struct class_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", global_stk->power_en);
}

static ssize_t class_stk_flag_show(struct class *class,
                                   struct class_attribute *attr, char *buf)
{
    int i = 0;
    uint32_t prox_flag = 0;
    STK_ERR("stk_flag_show");
    //read prox flag
    stk_read_prox_flag(global_stk, &prox_flag);
    stk501xx_read_sar_data(global_stk, prox_flag);

    for ( i = 0; i < 8; i++)
    {
        STK_ERR("ph[%d] near/far flag=%d\n", i, global_stk->last_nearby[i]);
    }

    return scnprintf(buf, PAGE_SIZE, "prox flag=0x%d\n", prox_flag);
}
static ssize_t class_stk_phase_cali(struct class *class,
                                    struct class_attribute *attr, char *buf)
{
    //int result = 0;
    STK_ERR("class_stk_phase_cali , reset all phase\n");
    stk501xx_phase_reset(global_stk, STK_TRIGGER_REG_INIT_ALL);
    //return (ssize_t)result;
    return sprintf(buf, "%d\n", global_stk->user_test);
}

static ssize_t class_stk_phase_cali_store(struct class *class,
        struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    ret = kstrtouint(buf, 10, &global_stk->user_test);
    if (0 != ret) {
        STK_ERR("kstrtoint failed\n");
    }
    if(global_stk->user_test)
    {
        global_stk->sar_first_boot = false;
        STK_ERR("global_stk->sar_first_boot:%d\n", global_stk->sar_first_boot);
        STK_ERR("class_stk_phase_cali_store , reset all phase\n");
        stk501xx_phase_reset(global_stk, STK_TRIGGER_REG_INIT_ALL);
    }
    return count;
}

static ssize_t class_stk_set_thd(struct class *class,
                                    struct class_attribute *attr, const char *buf, size_t count)
{
    u8  ph_idx;
    u32 thd;

    if (sscanf(buf, "%d,%d", &ph_idx, &thd) != 2)
    {
        STK_ERR("please input two DEC numbers: ph_id,thd (ph_id: phase number, thd)\n");
        return -EINVAL;
    }

    STK_ERR("set ph[%x] = %d\n", ph_idx, thd);

    stk501xx_set_each_thd(global_stk, ph_idx, thd);

    return count;
}

static ssize_t class_stk_sar_cali(struct class *class,
        struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    unsigned int en;

    ret = kstrtouint(buf, 10, &en);

    if (0 != ret)
    {
        STK_ERR("kstrtoint failed\n");
    }

    if (en)
    {
        STK_ERR("class_stk_sar_cali , reset all phase\n");
        stk501xx_phase_reset(global_stk, STK_TRIGGER_REG_INIT_ALL);
    }
    else
    {
        STK_ERR("class_stk_sar_cali, input : %d, nothing work\n", en);
    }

    return count;
}

static ssize_t stk_channel_en_show(struct class *class,
                                    struct class_attribute *attr, char *buf)
{
    int8_t i = 0;
    char *p = buf;
    stk501xx_wrapper *stk_wrapper = container_of(global_stk, stk501xx_wrapper, stk);

    for(i = 0; i < ch_num; i++)
    {
        STK_ERR("ch[%i]: enabled = %d\n", i, stk_wrapper->channels[i].enabled);
        p += snprintf(p, PAGE_SIZE, "ch[%i]: enabled = %d\n", i, stk_wrapper->channels[i].enabled);
    }

    return (p - buf);
}

static ssize_t stk_channel_en_store(struct class *class,
        struct class_attribute *attr, const char *buf, size_t count)
{
    uint8_t ch_idx, en;
    u32 val;
    bool prev_states = false;

    stk501xx_wrapper *stk_wrapper = container_of(global_stk, stk501xx_wrapper, stk);

    if (sscanf(buf, "%d,%d", &ch_idx, &en) != 2)
    {
        STK_ERR("please input two DEC numbers: ch_id,en \
                (ch_id: channel number, en: 1=enable, 0=disable)\n");
        return -EINVAL;
    }

    if ((ch_idx >= ch_num) || (ch_idx < 0))
    {
        STK_ERR("chanel index over %d\n", ch_num -1);
        return count;
    }

    STK_ERR("set ch[%x] = %d\n", ch_idx, en);

    prev_states = stk_wrapper->channels[ch_idx].enabled ;
    stk_wrapper->channels[ch_idx].enabled = en ? 1 : 0;
    if(stk_wrapper->channels[0].enabled != stk_wrapper->channels[1].enabled) {
        STK_ERR("stk_chag en[0] = %d\n",stk_wrapper->channels[0].enabled);
        stk_wrapper->channels[1].enabled = stk_wrapper->channels[0].enabled;
        STK_ERR("stk_chag en[1] = %d\n",stk_wrapper->channels[1].enabled);
    }

    stk_chk_sar_en_irq(stk_wrapper, ch_idx);

    if(en)
    {
        STK_ERR("stk->grip_status[%d] = %d", ch_idx, global_stk->grip_status[ch_idx]);
        global_stk->grip_status[ch_idx] = SAR_STATE_NONE;
        STK_ERR("stk->grip_status[%d] = %d", ch_idx, global_stk->grip_status[ch_idx]);

        global_stk->state_change[mapping_phase[ch_idx]] = 1;       //force update
        global_stk->ch_status[ch_idx] = SAR_STATE_FAR;
#if SAR_IN_FRANCE
        global_stk->ch_status[ch_idx] = SAR_STATE_FAR;
#endif
#ifdef STK_POLLING_MODE
        STK_TIMER_START(global_stk, &global_stk->stk_timer_info);
#endif
#ifdef TEMP_COMPENSATION
        global_stk->last_prox_a_state = 0;
        global_stk->last_prox_b_state = 0;
#endif
    }
    else
    {
        if(global_stk->last_nearby[mapping_phase[ch_idx]] == STK_SAR_NEAR_BY)
            stk_wrapper->channels[ch_idx].prev_nearby = SAR_STATE_NEAR;
        else
            stk_wrapper->channels[ch_idx].prev_nearby = SAR_STATE_FAR;
    }

    stk_report_sar_data(global_stk);
    stk_clr_intr(global_stk, &val);

    return count;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
static struct class_attribute class_attr_enable =
    __ATTR(enable, 0664, class_stk_sar_enable_show, class_stk_sar_enable_store);
static struct class_attribute class_attr_ch0_cap_diff_dump =
    __ATTR(ch0_cap_diff_dump, 0444, class_stk_value_show_ch0, NULL);
static struct class_attribute class_attr_ch1_cap_diff_dump =
    __ATTR(ch1_cap_diff_dump, 0444, class_stk_value_show_ch1, NULL);
static struct class_attribute class_attr_ch2_cap_diff_dump =
    __ATTR(ch2_cap_diff_dump, 0444, class_stk_value_show_ch2, NULL);
static struct class_attribute class_attr_ch3_cap_diff_dump =
    __ATTR(ch3_cap_diff_dump, 0444, class_stk_value_show_ch3, NULL);
static struct class_attribute class_attr_power_enable =
    __ATTR(power_enable, 0664, class_stk_power_en_show, class_stk_power_en);
static struct class_attribute class_attr_user_test =
    __ATTR(user_test, 0664, class_stk_phase_cali, class_stk_phase_cali_store);
static struct class_attribute class_attr_flag =
    __ATTR(flag, 0444, class_stk_flag_show, NULL);
static struct class_attribute class_attr_set_thd =
    __ATTR(set_thd, 0220, NULL, class_stk_set_thd);
static struct class_attribute class_attr_sar_cali =
    __ATTR(sar_cali, 0220, NULL, class_stk_sar_cali);
static struct class_attribute class_attr_channel_en =
    __ATTR(channel_en, 0664, stk_channel_en_show, stk_channel_en_store);

static struct attribute *capsense_class_attrs[] =
{
    &class_attr_enable.attr,
	&class_attr_ch0_cap_diff_dump.attr,
	&class_attr_ch1_cap_diff_dump.attr,
	&class_attr_ch2_cap_diff_dump.attr,
    &class_attr_ch3_cap_diff_dump.attr,
    &class_attr_power_enable.attr,
    &class_attr_user_test.attr,
    &class_attr_flag.attr,
    &class_attr_set_thd.attr,
    &class_attr_sar_cali.attr,
    &class_attr_channel_en.attr,
    NULL,
};

ATTRIBUTE_GROUPS(capsense_class);
#else
static struct class_attribute capsense_class_attributes[] =
{
    __ATTR(enable, 0664, class_stk_sar_enable_show, class_stk_sar_enable_store),
    __ATTR(ch0_cap_diff_dump, 0444, class_stk_value_show_ch0, NULL),
    __ATTR(ch1_cap_diff_dump, 0444, class_stk_value_show_ch1, NULL),
    __ATTR(ch2_cap_diff_dump, 0444, class_stk_value_show_ch2, NULL),
    __ATTR(ch3_cap_diff_dump, 0444, class_stk_value_show_ch3, NULL),
    __ATTR(power_enable, 0664, class_stk_power_en_show, class_stk_power_en),
    __ATTR(user_test, 0664, class_stk_phase_cali, class_stk_phase_cali_store),
    __ATTR(flag, 0444, class_stk_flag_show, NULL),
    __ATTR(set_thd, 0220, NULL, class_stk_set_thd),
    __ATTR(sar_cali, 0220, NULL, class_stk_sar_cali),
    __ATTR(channel_en, 0664, stk_channel_en_show, stk_channel_en_store),
    __ATTR_NULL,
};
#endif



struct class capsense_class =
    {
        .name                   = "capsense",
        .owner                  = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
        .class_groups           = capsense_class_groups,
#else
        .class_attrs            = capsense_class_attributes,
#endif
    };
/*end of class define*/


/**
 * @brief: Get power status
 *          Send 0 or 1 to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_enable_show(struct device* dev,
                               struct device_attribute* attr, char* buf)
{
    stk501xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    char en = 0;
    uint8_t i;
    struct input_dev* temp_input_dev;
    temp_input_dev = container_of(dev, struct input_dev, dev);

    for (i = 0; i < ch_num; i ++)
    {
        if(!strcmp(temp_input_dev->name, input_dev_name[i]))
        {
            en = stk_wrapper->channels[i].enabled;
            break;
        }
    }

    return scnprintf(buf, PAGE_SIZE, "%d\n", en);
}

/**
 * @brief: Set power status
 *          Get 0 or 1 from userspace, then set stk8xxx power status.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_enable_store(struct device *dev,
                                struct device_attribute *attr, const char *buf, size_t count)
{
    stk501xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    struct input_dev* temp_input_dev;
    uint32_t en, val = 0;
    int32_t error;
    uint8_t i;
    bool prev_states = false;
#ifdef STK_POLLING_MODE
    bool total_states = false;
#endif
    error = kstrtouint(buf, 10, &en);

    if (error)
    {
        STK_ERR("kstrtoul failed, error=%d", error);
        return error;
    }

    temp_input_dev = container_of(dev, struct input_dev, dev);

    STK_ERR("stk_enable_store, en=%d\n", en);

    for (i = 0; i < ch_num; i ++)
    {
        if(!strcmp(temp_input_dev->name, input_dev_name[i]))
        {
            prev_states = stk_wrapper->channels[i].enabled ;
            stk_wrapper->channels[i].enabled = en ? 1 : 0;

            stk_chk_sar_en_irq(stk_wrapper, i);

            if(en)
            {
                STK_ERR("stk->grip_status[%d] = %d", i, global_stk->grip_status[i]);
                global_stk->grip_status[i] = SAR_STATE_NONE;
                STK_ERR("stk->grip_status[%d] = %d", i, global_stk->grip_status[i]);

                stk->state_change[mapping_phase[i]] = 1;       //force update
                stk->ch_status[i] = SAR_STATE_FAR;
#if SAR_IN_FRANCE
                stk->ch_status[i] = SAR_STATE_FAR;
#endif

#ifdef STK_POLLING_MODE
                STK_TIMER_START(stk, &stk->stk_timer_info);
#endif
#ifdef TEMP_COMPENSATION
                stk->last_prox_a_state = 0;
                stk->last_prox_b_state = 0;
#endif
                }
            else
                {
                if(stk->last_nearby[mapping_phase[i]] == STK_SAR_NEAR_BY)
                    stk_wrapper->channels[i].prev_nearby = SAR_STATE_NEAR;
                else
                    stk_wrapper->channels[i].prev_nearby = SAR_STATE_FAR;
#ifdef TEMP_COMPENSATION
                clr_temp(stk);
#endif
            }
            //debug
            STK_ERR("stk_enable_store\n");

            break;
        }
    }
    if(stk_wrapper->channels[0].enabled != stk_wrapper->channels[1].enabled) {
        STK_ERR("stk_chag11 en[0] = %d\n",stk_wrapper->channels[0].enabled);
        stk_wrapper->channels[1].enabled = stk_wrapper->channels[0].enabled;
        STK_ERR("stk_chag11 en[1] = %d\n",stk_wrapper->channels[1].enabled);
    }
#ifdef STK_POLLING_MODE
    for (i = 0; i < ch_num; i ++)
    {
        total_states |= stk_wrapper->channels[i].enabled;
    }
    if(!total_states)
        STK_TIMER_STOP(stk, &stk->stk_timer_info);
#endif

    stk_report_sar_data(stk);
    stk_clr_intr(stk, &val);
    return count;
}

/**
 * @brief: Get sar data
 *          Send sar data to userspce.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_value_show(struct device* dev,
                              struct device_attribute* attr, char* buf)
{
    stk501xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    uint32_t prox_flag = 0;
    stk_data *stk = &stk_wrapper->stk;
    STK_ERR("stk_value_show");
    //read prox flag
    stk_read_prox_flag(stk, &prox_flag);
    stk501xx_read_sar_data(stk, prox_flag);
    return scnprintf(buf, PAGE_SIZE, "val[0]=%d\n", stk->last_data[0]);
}

static ssize_t stk_flag_show(struct device *dev,
                             struct device_attribute *attr, char *buf)
{
    stk501xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    uint8_t i = 0;
    uint32_t prox_flag = 0;
    stk_data *stk = &stk_wrapper->stk;
    STK_ERR("stk_flag_show");
    //read prox flag
    stk_read_prox_flag(stk, &prox_flag);
    stk501xx_read_sar_data(stk, prox_flag);

    for ( i = 0; i < 6; i++)
    {
        STK_ERR("ph[%d] prox flag=%d", i, stk->last_nearby[i]);
    }

    return scnprintf(buf, PAGE_SIZE, "prox flag=%x\n", prox_flag);
}

/**
 * @brief: Register writting
 *          Get address and content from userspace, then write to register.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 * @param[in] count: size_t
 *
 * @return: ssize_t
 */
static ssize_t stk_send_store(struct device *dev,
                              struct device_attribute *attr, const char *buf, size_t count)
{
    stk501xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    char *token[10];
    int32_t err, i;
    u32 addr, cmd;
    bool enable = false;

    for (i = 0; i < 2; i++)
        token[i] = strsep((char **)&buf, " ");

    err = kstrtouint(token[0], 16, &addr);

    if (err)
    {
        STK_ERR("kstrtouint failed, err=%d", err);
        return err;
    }

    err = kstrtouint(token[1], 16, &cmd);

    if (err)
    {
        STK_ERR("kstrtouint failed, err=%d", err);
        return err;
    }

    STK_ERR("write reg[0x%X]=0x%X", addr, cmd);

    if (!stk->enabled)
        stk501xx_set_enable(stk, 1);
    else
        enable = true;

    if (0 > STK_REG_WRITE_BLOCK(stk, (uint16_t)addr, (uint8_t*)&cmd, 4))
    {
        err = -1;
        goto exit;
    }

exit:

    if (!enable)
        stk501xx_set_enable(stk, 0);

    if (err)
        return -1;

    return count;
}
/**
 * @brief: Read all register value, then send result to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_allreg_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    stk501xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    int32_t result;

    if (!buf)
        return -1;

    result = stk501xx_show_all_reg(stk);

    if (0 > result)
        return result;

    return (ssize_t)result;
}

/**
 * @brief: Check PID, then send chip number to userspace.
 *
 * @param[in] dev: struct device *
 * @param[in] attr: struct device_attribute *
 * @param[in/out] buf: char *
 *
 * @return: ssize_t
 */
static ssize_t stk_chipinfo_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    stk501xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    stk_data *stk = &stk_wrapper->stk;
    STK_ERR("chip id=0x%x, index=0x%x", stk->chip_id, stk->chip_index);
    return scnprintf(buf, PAGE_SIZE, "pid=0x%x,index=0x%x\n", stk->chip_id, stk->chip_index);
}

static ssize_t stk_phase_cali(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
    stk501xx_wrapper *stk_wrapper = dev_get_drvdata(dev);
    int32_t result = 0;
    stk_data *stk = &stk_wrapper->stk;
    stk501xx_phase_reset(stk, STK_TRIGGER_REG_INIT_ALL);
    return (ssize_t)result;
}

static DEVICE_ATTR(enable, 0664, stk_enable_show, stk_enable_store);
static DEVICE_ATTR(value, 0444, stk_value_show, NULL);
static DEVICE_ATTR(send, 0220, NULL, stk_send_store);
static DEVICE_ATTR(flag, 0444, stk_flag_show, NULL);
static DEVICE_ATTR(allreg, 0444, stk_allreg_show, NULL);
static DEVICE_ATTR(chipinfo, 0444, stk_chipinfo_show, NULL);
static DEVICE_ATTR(phcali, 0444, stk_phase_cali, NULL);

static struct attribute *stk_attr_list[] =
{
    &dev_attr_enable.attr,
    &dev_attr_value.attr,
    &dev_attr_send.attr,
    &dev_attr_flag.attr,
    &dev_attr_allreg.attr,
    &dev_attr_chipinfo.attr,
    &dev_attr_phcali.attr,
    NULL
};

#ifdef STK_SENSORS_DEV
/* SAR information read by HAL */
static struct sensors_classdev stk_cdev =
{
    .name = "stk501xx",
    .vendor = "Sensortek",
    .version = 1,
    .type = 65560,
    .max_range = "5",
    .resolution = "5.0",
    .sensor_power = "3",
    .min_delay = 0,
    .max_delay = 0,
    .delay_msec = 100,
    .fifo_reserved_event_count = 0,
    .fifo_max_event_count = 0,
    .enabled = 0,
    .max_latency = 0,
    .flags = 0, /* SENSOR_FLAG_CONTINUOUS_MODE */
    .sensors_enable = NULL,
    .sensors_poll_delay = NULL,
    .sensors_enable_wakeup = NULL,
    .sensors_set_latency = NULL,
    .sensors_flush = NULL,
    .sensors_calibrate = NULL,
    .sensors_write_cal_params = NULL,
};

/*
 * @brief: The handle for enable and disable sensor.
 *          include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 * @param[in] enabled:
 */
static int stk_cdev_sensors_enable(struct sensors_classdev *sensors_cdev,
                                   unsigned int enabled)
{
    struct stk501xx_wrapper *stk_wrapper = container_of(sensors_cdev, stk501xx_wrapper, channels[0].sar_cdev);
    struct stk_data *stk = &stk_wrapper->stk;

    if (0 == enabled)
    {
        stk501xx_set_enable(stk, 0);
    }
    else if (1 == enabled)
    {
        stk501xx_set_enable(stk, 1);
    }
    else
    {
        STK_ERR("Invalid vlaue of input, input=%d", enabled);
        return -EINVAL;
    }

    return 0;
}

/*
 * @brief: The handle for set the sensor polling delay time.
 *          include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 * @param[in] delay_msec:
 */
static int stk_cdev_sensors_poll_delay(struct sensors_classdev *sensors_cdev,
                                       unsigned int delay_msec)
{
#ifdef STK_INTERRUPT_MODE
    /* do nothing */
#elif defined STK_POLLING_MODE
    struct stk501xx_wrapper *stk_wrapper = container_of(sensors_cdev, stk501xx_wrapper, channels[0].sar_cdev);
    struct stk_data *stk = &stk_wrapper->stk;
    stk->stk_timer_info.interval_time = delay_msec * 1000;
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
    STK_LOG("stk_cdev_sensors_poll_delay ms=%d", delay_msec);
    return 0;
}

/*
 * @brief:
 * include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 * @param[in] enable:
 */
static int stk_cdev_sensors_enable_wakeup(struct sensors_classdev *sensors_cdev,
        unsigned int enable)
{
    STK_LOG("enable=%d", enable);
    return 0;
}


/*
 * @brief: Flush sensor events in FIFO and report it to user space.
 * include/linux/sensors.h
 *
 * @param[in] *sensors_cdev: struct sensors_classdev
 */
static int stk_cdev_sensors_flush(struct sensors_classdev *sensors_cdev, unsigned char flush)
{
    STK_LOG("stk_cdev_sensors_flush");
    return 0;
}
#endif // STK_SENSORS_DEV



static const struct attribute_group stk501xx_attr_group =
{
    .attrs = stk_attr_list,
};

static int32_t stk501xx_input_open(struct input_dev* input)
{
    return 0;
}

static void stk501xx_input_close(struct input_dev* input)
{
}

static int stk_input_setup(stk501xx_wrapper *stk_wrapper)
{
    int err = 0;
    uint8_t i = 0;

    for (i = 0; i < ch_num; i++)
    {
        /* input device: setup for sar */
        stk_wrapper->channels[i].input_dev = input_allocate_device();

        if (!stk_wrapper->channels[i].input_dev)
        {
            STK_ERR("input_allocate_device for sar failed");
            return -ENOMEM;
        }

        stk_wrapper->channels[i].input_dev->name = input_dev_name[i];
        stk_wrapper->channels[i].input_dev->id.bustype = BUS_I2C;
        stk_wrapper->channels[i].input_dev->open = stk501xx_input_open;
        stk_wrapper->channels[i].input_dev->close = stk501xx_input_close;

        /*with ss senosr hal*/
        __set_bit(EV_REL, stk_wrapper->channels[i].input_dev->evbit);
        input_set_capability(stk_wrapper->channels[i].input_dev, EV_REL, REL_MISC);
        __set_bit(EV_REL, stk_wrapper->channels[i].input_dev->evbit);
        input_set_capability(stk_wrapper->channels[i].input_dev, EV_REL, REL_X);
        __set_bit(EV_REL, stk_wrapper->channels[i].input_dev->evbit);
        input_set_capability(stk_wrapper->channels[i].input_dev, EV_REL, REL_MAX);
/*
        input_set_capability(stk_wrapper->channels[i].input_dev, EV_ABS, ABS_DISTANCE);
        input_set_drvdata(stk_wrapper->channels[i].input_dev, stk_wrapper);
*/
        input_set_drvdata(stk_wrapper->channels[i].input_dev, stk_wrapper);
        err = input_register_device(stk_wrapper->channels[i].input_dev);

        if (err)
        {
            STK_ERR("Unable to register input device: %s", stk_wrapper->channels[i].input_dev->name);
            input_free_device(stk_wrapper->channels[i].input_dev);
            return err;
        }
        input_report_rel(stk_wrapper->channels[i].input_dev, REL_MISC, 2);
        input_sync(stk_wrapper->channels[i].input_dev);
        
//        STK_ERR("[%d] name =%s, type =%d\n", stk_sensord_dev[i].name, stk_sensord_dev[i].type);

#ifdef STK_SENSORS_DEV
        stk_wrapper->channels[i].sar_cdev = stk_cdev;
        stk_wrapper->channels[i].sar_cdev.name = "STK501";
        stk_wrapper->channels[i].sar_cdev.sensor_name = stk_sensord_dev[i].name;
        stk_wrapper->channels[i].sar_cdev.type = stk_sensord_dev[i].type;
        stk_wrapper->channels[i].sar_cdev.sensors_enable = stk_cdev_sensors_enable;
        stk_wrapper->channels[i].sar_cdev.sensors_poll_delay = stk_cdev_sensors_poll_delay;
        stk_wrapper->channels[i].sar_cdev.sensors_enable_wakeup = stk_cdev_sensors_enable_wakeup;
        stk_wrapper->channels[i].sar_cdev.sensors_flush = stk_cdev_sensors_flush;
        err = sensors_classdev_register(&stk_wrapper->channels[i].input_dev->dev, &stk_wrapper->channels[i].sar_cdev);
        if (err) {
            STK_ERR("Error to register class device: %s,error id is %d", stk_wrapper->channels[i].input_dev->name, err);
        } else {
            STK_ERR("Success to register class device: %s", stk_wrapper->channels[i].input_dev->name);
        }
#endif
    }

    return 0;
}

/*
 * @brief:
 *
 * @param[in/out] stk: struct stk_data *
 *
 * @return:
 *      0: Success
 *      others: Fail
 */
static int32_t stk_init_mtk(stk501xx_wrapper *stk_wrapper)
{
    int32_t err = 0;
    uint8_t i = 0;

    /*Create fsys class*/
    err = class_register(&capsense_class);
    if (err)
    {
        STK_ERR("Create fsys class, err=%d", err);
        return err;
    }

    err = stk_input_setup(stk_wrapper);
    if (err)
    {
        return -1;
    }

    /* sysfs: create file system */
    for (i = 0; i < ch_num; i++)
    {
        err = sysfs_create_group(&stk_wrapper->channels[i].input_dev->dev.kobj,
                             &stk501xx_attr_group);
        stk_wrapper->channels[i].enabled = false;
    }

    if (err)
        goto err_sensors_classdev_register;

    return 0;
/*
err_free_mem:
    for (i = 0; i < ch_num; i++)
    {   if()
        input_free_device(stk_wrapper->input_dev);
    }
*/
err_sensors_classdev_register:
    for (i = 0; i < ch_num; i++)
    {
        sysfs_remove_group(&stk_wrapper->channels[i].input_dev->dev.kobj, &stk501xx_attr_group);
    }
    return -1;
}

/*
 * @brief: Exit mtk related settings safely.
 *
 * @param[in/out] stk: struct stk_data *
 */
static void stk_exit_mtk(struct stk501xx_wrapper *stk_wrapper)
{
    int8_t i = 0;

    for (i = 0; i < ch_num; i++)
    {
#ifdef STK_SENSORS_DEV
        sensors_classdev_unregister(&stk_wrapper->channels[i].sar_cdev);
#endif // STK_SENSORS_DEV

        sysfs_remove_group(&stk_wrapper->channels[i].input_dev->dev.kobj, &stk501xx_attr_group);
        input_unregister_device(stk_wrapper->channels[i].input_dev);
#ifndef STK_SENSORS_DEV
        input_free_device(stk_wrapper->channels[i].input_dev);
#endif
    }
}

#if SAR_IN_FRANCE
void exit_anfr_stk501xx(void)
{
    if (global_stk->sar_first_boot)
    {
        global_stk->sar_first_boot = false;
        STK_ERR("global_stk->sar_first_boot:%d\n", global_stk->sar_first_boot);
        STK_ERR("stk501xx:exit force input near mode, and report sar once!!\n");
        global_stk->gpio_info.gpio_cb(global_stk->gpio_info.user_data);
        stk501xx_phase_reset(global_stk, STK_TRIGGER_REG_INIT_ALL);
    }
    STK_ERR("exit_anfr_stk501xx!!!\n");
}
EXPORT_SYMBOL(exit_anfr_stk501xx);
#endif
void stk_report_sar_data(struct stk_data* stk)
{
    stk501xx_wrapper *stk_wrapper = container_of(stk, stk501xx_wrapper, stk);
    int32_t i = 0;
    uint8_t is_change = 0;
    uint8_t nf_flag = SAR_STATE_FAR;
#if SAR_IN_FRANCE
    int32_t ch0_result = 0;
    int32_t ch1_result = 0;
    int32_t ch2_result = 0;
    int32_t ch3_result = 0;
#endif

    if (!stk_wrapper->channels[i].input_dev)
    {
        STK_ERR("No input device for sar data");
        return;
    }

#if SAR_IN_FRANCE
    if(stk->sar_first_boot)
    {
        STK_ERR("stk->interrupt_cnt=%d\n",stk->interrupt_cnt);

        ch0_result = stk501xx_read_offset_fadc(stk, 0);
        ch1_result = stk501xx_read_offset_fadc(stk, 1);
        ch2_result = stk501xx_read_offset_fadc(stk, 2);
        ch3_result = stk501xx_read_offset_fadc(stk, 3);
        //debug
        STK_ERR("ch0_result: %d\n", ch0_result);
        STK_ERR("ch1_result: %d\n", ch1_result);
        STK_ERR("ch2_result: %d\n", ch2_result);
        STK_ERR("ch3_result: %d\n", ch3_result);
        STK_ERR("stk->cadc_weight =%d\n", stk->cadc_weight);
//endof debug
    }
#endif
    STK_ERR("stk_report_sar_data:: change ph[5] =%d, ph[6] =%d", stk->state_change[5], stk->state_change[6]);
    if((stk_wrapper->channels[0].enabled || stk_wrapper->channels[1].enabled) && (stk->state_change[5] || stk->state_change[6]))
    {
        STK_ERR("stk_report_sar_data:: state ph[5] =%d, ph[6] =%d", stk->last_nearby[5], stk->last_nearby[6]);
        if(((STK_SAR_NEAR_BY == stk->last_nearby[5]) || (STK_SAR_NEAR_BY == stk->last_nearby[6])) && (stk->grip_status[0] != SAR_STATE_NEAR)) {
            nf_flag = SAR_STATE_NEAR;
            is_change = 1;
            stk->grip_status[0] = SAR_STATE_NEAR;
            STK_ERR("stk->grip_status = %d", stk->grip_status[0]);
            STK_ERR("ch%d : %s report_sar_status %d !\n", 0, &stk_sensord_dev[0].name, nf_flag);
        } else if(((STK_SAR_FAR_AWAY == stk->last_nearby[5]) && (STK_SAR_FAR_AWAY == stk->last_nearby[6])) && (stk->grip_status[0] != SAR_STATE_FAR)) {
            nf_flag = SAR_STATE_FAR;
            is_change = 1;
            stk->grip_status[0] = SAR_STATE_FAR;
            STK_ERR("stk->grip_status = %d", stk->grip_status[0]);
            STK_ERR("ch%d : %s report_sar_status %d !\n", 0, &stk_sensord_dev[0].name, nf_flag);
        } else {
            is_change = 0;
            STK_ERR("ch0 : %s report_sar_status error!\n", &stk_sensord_dev[0].name);
        }
#if SAR_IN_FRANCE
        stk->ch_status[0] = stk->ch_status[1] = nf_flag;
#endif
        STK_ERR("class_stk_power_en, en=%d", global_stk->power_en);
        if (is_change != 0 && global_stk->power_en == 0)
        {
            // with ss sensor hal
            wake_lock_timeout(&stk501xx_wake_lock, HZ * 1);
#if SAR_IN_FRANCE
            if (stk->sar_first_boot)
            {
                stk->interrupt_cnt++ ;
                if(stk->ch_status[0] == SAR_STATE_FAR && (stk->interrupt_cnt >=18)) //Ready to cali near statues
                {
                    stk501xx_phase_reset(stk, STK_TRIGGER_REG_INIT_ALL);
                }
            }
            if (stk->sar_first_boot && (stk->interrupt_cnt < MAX_INT_COUNT) && \
                ((ch0_result - stk->sar_first_fadc[0]) >= CH0_FADC_DIFF) &&   \
                ((ch1_result - stk->sar_first_fadc[1]) >= CH1_FADC_DIFF) &&   \
                ((ch2_result - stk->sar_first_fadc[2]) >= CH2_FADC_DIFF) &&   \
                ((ch3_result - stk->sar_first_fadc[3]) >= CH3_FADC_DIFF))
            {
                STK_ERR("stk->interrupt_cnt=%d\n" ,stk->interrupt_cnt);
                input_report_rel(stk_wrapper->channels[0].input_dev, REL_MISC, 1);
                input_report_rel(stk_wrapper->channels[0].input_dev, REL_X, 1);
                input_sync(stk_wrapper->channels[0].input_dev);
            }
            else // first_boot false or int num more than 20 or floating
            {
                if(stk->sar_first_boot)
                {
                    STK_ERR("exit force input near mode!!!\n");
//debug
                    STK_ERR("stk->interrupt_cnt=%d\n" ,stk->interrupt_cnt);
                    STK_ERR("ch0_result: %d\n", ch0_result);
                    STK_ERR("ch1_result: %d\n", ch1_result);
                    STK_ERR("ch2_result: %d\n", ch2_result);
                    STK_ERR("ch3_result: %d\n", ch3_result);
                    STK_ERR("ch0_result_1st: %d\n", stk->sar_first_fadc[0]);
                    STK_ERR("ch1_result_1st: %d\n", stk->sar_first_fadc[1]);
                    STK_ERR("ch2_result_1st: %d\n", stk->sar_first_fadc[2]);
                    STK_ERR("ch3_result_1st: %d\n", stk->sar_first_fadc[3]);
                    STK_ERR("ch0_result_diff: %d\n", ch0_result - stk->sar_first_fadc[0]);
                    STK_ERR("ch1_result_diff: %d\n", ch1_result - stk->sar_first_fadc[1]);
                    STK_ERR("ch2_result_diff: %d\n", ch2_result - stk->sar_first_fadc[2]);
                    STK_ERR("ch3_result_diff: %d\n", ch3_result - stk->sar_first_fadc[3]);
//endof debug
                    stk->sar_first_boot = false;
                    stk501xx_phase_reset(stk, STK_TRIGGER_REG_INIT_ALL);

                }
#endif
            STK_ERR("exit force input near mode!!!, SAR stk->interrupt_cnt=%d\n" ,stk->interrupt_cnt);
            input_report_rel(stk_wrapper->channels[0].input_dev, REL_MISC, nf_flag);
            input_report_rel(stk_wrapper->channels[0].input_dev, REL_X, 2);
//            input_report_abs(stk_wrapper->channels[0].input_dev, ABS_DISTANCE, nf_flag);
            input_sync(stk_wrapper->channels[0].input_dev);
#if SAR_IN_FRANCE
            }
#endif
        }
    }

    for (i = 2; i < ch_num; i ++)// report channel[1] channel[2] refer each IC phase
    {
        if(stk_wrapper->channels[i].enabled)
        {
            is_change = stk->state_change[mapping_phase[i]];
            STK_ERR("stk_report_sar_data:: change ph[%d] =%d,(%d)", i, stk->state_change[mapping_phase[i]], is_change);

            if ((STK_SAR_NEAR_BY == stk->last_nearby[mapping_phase[i]]) && (stk->grip_status[i] != SAR_STATE_NEAR)) {
                nf_flag = SAR_STATE_NEAR;
                stk->grip_status[i] = SAR_STATE_NEAR;
                STK_ERR("ch%d : %s report_sar_status %d !\n", i, &stk_sensord_dev[i].name, nf_flag);
            } else if ((STK_SAR_FAR_AWAY == stk->last_nearby[mapping_phase[i]]) && (stk->grip_status[i] != SAR_STATE_FAR)) {
                nf_flag = SAR_STATE_FAR;
                stk->grip_status[i] = SAR_STATE_FAR;
                STK_ERR("ch%d : %s report_sar_status %d !\n", i, &stk_sensord_dev[i].name, nf_flag);
            } else {
                STK_ERR("ch%d : %s report_sar_status error!\n", i,  &stk_sensord_dev[i].name);
                is_change = 0;
            }
#if SAR_IN_FRANCE
            stk->ch_status[i] = nf_flag;
#endif
            STK_ERR("class_stk_power_en, en=%d", global_stk->power_en);
            if (is_change != 0 && global_stk->power_en == 0)
            {
                /*with ss sensor hal*/
                wake_lock_timeout(&stk501xx_wake_lock, HZ * 1);
#if SAR_IN_FRANCE
                if (stk->sar_first_boot)
                {
                    stk->interrupt_cnt++ ;
                    if(stk->ch_status[i] == SAR_STATE_FAR && (stk->interrupt_cnt >=18)) //Ready to cali near statues
                    {
                        stk501xx_phase_reset(stk, STK_TRIGGER_REG_INIT_ALL);
                    }
                }
                if (stk->sar_first_boot && (stk->interrupt_cnt < MAX_INT_COUNT) && \
                    ((ch0_result - stk->sar_first_fadc[0]) >= CH0_FADC_DIFF) &&   \
                    ((ch1_result - stk->sar_first_fadc[1]) >= CH1_FADC_DIFF) &&   \
                    ((ch2_result - stk->sar_first_fadc[2]) >= CH2_FADC_DIFF) &&   \
                    ((ch3_result - stk->sar_first_fadc[3]) >= CH3_FADC_DIFF))
                {
                     STK_ERR("stk->interrupt_cnt=%d\n" ,stk->interrupt_cnt);
                    input_report_rel(stk_wrapper->channels[i].input_dev, REL_MISC, 1);
                    input_report_rel(stk_wrapper->channels[i].input_dev, REL_X, 1);
                    input_sync(stk_wrapper->channels[i].input_dev);
                }
                else // first_boot false or int num more than 20 or floating
                {
                    if(stk->sar_first_boot)
                    {
                        STK_ERR("exit force input near mode!!!, SAR stk->interrupt_cnt=%d\n" ,stk->interrupt_cnt);
//debug
                        STK_ERR("ch0_result: %d\n", ch0_result);
                        STK_ERR("ch1_result: %d\n", ch1_result);
                        STK_ERR("ch2_result: %d\n", ch2_result);
                        STK_ERR("ch3_result: %d\n", ch3_result);
                        STK_ERR("ch0_result_1st: %d\n", stk->sar_first_fadc[0]);
                        STK_ERR("ch1_result_1st: %d\n", stk->sar_first_fadc[1]);
                        STK_ERR("ch2_result_1st: %d\n", stk->sar_first_fadc[2]);
                        STK_ERR("ch3_result_1st: %d\n", stk->sar_first_fadc[2]);
                        STK_ERR("ch0_result_diff: %d\n", ch0_result - stk->sar_first_fadc[0]);
                        STK_ERR("ch1_result_diff: %d\n", ch1_result - stk->sar_first_fadc[1]);
                        STK_ERR("ch2_result_diff: %d\n", ch2_result - stk->sar_first_fadc[2]);
                        STK_ERR("ch3_result_diff: %d\n", ch3_result - stk->sar_first_fadc[3]);
//endof debug
                        stk->sar_first_boot = false;
                        stk501xx_phase_reset(stk, STK_TRIGGER_REG_INIT_ALL);
                    }
#endif
                STK_ERR("exit force input near mode!!!, SAR stk->interrupt_cnt=%d\n" ,stk->interrupt_cnt);
                input_report_rel(stk_wrapper->channels[i].input_dev, REL_MISC, nf_flag);
                input_report_rel(stk_wrapper->channels[i].input_dev, REL_X, 2);
//                input_report_abs(stk_wrapper->channels[i].input_dev, ABS_DISTANCE, nf_flag);
                input_sync(stk_wrapper->channels[i].input_dev);
#if SAR_IN_FRANCE
                }
#endif
            }
        }
    }
}

void stk501xx_parse_dt(struct stk_data* stk, struct device *dev)
{
    //struct device_node* node;
    u32 int_pin = 0;
    stk->gpio_info.int_pin = 0;
    of_property_read_u32_array(dev->of_node, "interrupts", &int_pin, 1);
    STK_ERR("interrupts = %d\n", int_pin);

    stk->gpio_info.int_pin = int_pin;
    stk->gpio_info.irq = irq_of_parse_and_map(dev->of_node, 0);
    STK_ERR("irq #=%d, interrupt pin=%d", stk->gpio_info.irq, stk->gpio_info.int_pin);
    return;
}

/**
 * @brief: Probe function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 * @param[in] stk_bus_ops: const struct stk_bus_ops *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
int32_t stk_i2c_probe(struct i2c_client *client, struct common_function *common_fn)
{
    int32_t err = 0;
    stk501xx_wrapper *stk_wrapper;
    struct stk_data *stk;
    STK_LOG("STK_HEADER_VERSION: %s ", STK_HEADER_VERSION);
    STK_LOG("STK_C_VERSION: %s ", STK_C_VERSION);
    STK_LOG("STK_DRV_I2C_VERSION: %s ", STK_DRV_I2C_VERSION);
    STK_LOG("STK_MTK_VERSION: %s ", STK_MTK_VERSION);

    if (NULL == client)
    {
        return -ENOMEM;
    }
    else if (!common_fn)
    {
        STK_ERR("cannot get common function. EXIT");
        return -EIO;
    }

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
    {
        err = i2c_get_functionality(client->adapter);
        STK_ERR("i2c_check_functionality error, functionality=0x%x", err);
        return -EIO;
    }

    /* kzalloc: allocate memory and set to zero. */
    stk_wrapper = kzalloc(sizeof(stk501xx_wrapper), GFP_KERNEL);

    if (!stk_wrapper)
    {
        STK_ERR("memory allocation error");
        return -ENOMEM;
    }

    stk = &stk_wrapper->stk;
    global_stk = stk;

    if (!stk)
    {
        STK_ERR("failed to allocate stk3a8x_data");
        return -ENOMEM;
    }

    gStk = stk;
    stk_wrapper->i2c_mgr.client = client;
    stk_wrapper->i2c_mgr.addr_type = ADDR_16BIT;
    stk->bops   = common_fn->bops;
    stk->tops   = common_fn->tops;
    stk->gops   = common_fn->gops;
#if SAR_IN_FRANCE
    stk->ch_status[0] = stk->ch_status[1] = stk->ch_status[2] = stk->ch_status[3] = STK_SAR_FAR_AWAY;
#endif
    stk->sar_report_cb = stk_report_sar_data;
    i2c_set_clientdata(client, stk_wrapper);
    mutex_init(&stk_wrapper->i2c_mgr.lock);
    stk->bus_idx = stk->bops->init(&stk_wrapper->i2c_mgr);

    if (stk->bus_idx < 0)
    {
        goto err_free_mem;
    }

#if SAR_IN_FRANCE
    stk->sar_first_boot = true;
    stk->user_test = 0;
#endif
    stk501xx_parse_dt(stk, &client->dev);
    err = stk501xx_init_client(stk);

    if (err < 0)
    {
        STK_ERR("stk501xx_init_client failed");
        goto err_free_mem;
    }

    if (stk_init_mtk(stk_wrapper))
    {
        STK_ERR("stk_init_mtk failed");
        goto err_free_mem;
    }

    wake_lock_init(&stk501xx_wake_lock, WAKE_LOCK_SUSPEND, "stk501xx_wakelock");
    STK_LOG("Success");
    stk_init_flag = 1;
    hardwareinfo_set_prop(HARDWARE_SAR, "stk501xx_sar");
    return 0;
//err_exit:
#ifdef STK_INTERRUPT_MODE
//    stk_exit_irq_setup(stk);
//err_cancel_work_sync:
    STK_GPIO_IRQ_REMOVE(stk, &stk->gpio_info);
#elif defined STK_POLLING_MODE
    STK_TIMER_REMOVE(stk, &stk->stk_timer_info);
#endif /* STK_INTERRUPT_MODE, STK_POLLING_MODE */
#ifdef STK_SENSING_WATCHDOG
    STK_TIMER_REMOVE(stk, &stk->sensing_watchdog_timer_info);
#endif // STK_SENSING_WATCHDOG
    stk_exit_mtk(stk_wrapper);
err_free_mem:
    mutex_destroy(&stk_wrapper->i2c_mgr.lock);
    kfree(stk_wrapper);
    stk_init_flag = -1;
    return err;
}

/*
 * @brief: Remove function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 *
 * @return: 0
 */
int32_t stk_i2c_remove(struct i2c_client *client)
{
    stk501xx_wrapper *stk_wrapper = i2c_get_clientdata(client);
    struct stk_data *stk = &stk_wrapper->stk;
    stk_exit_mtk(stk_wrapper);
    stk->bops->remove(&stk_wrapper->i2c_mgr);
    mutex_destroy(&stk_wrapper->i2c_mgr.lock);
    wake_lock_destroy(&stk501xx_wake_lock);
    kfree(stk_wrapper);
    stk_init_flag = -1;
    return 0;
}

int32_t stk501xx_suspend(struct device* dev)
{

    stk501xx_irq_from_suspend_flag = 1;

    return 0;
}

int32_t stk501xx_resume(struct device* dev)
{

    stk501xx_irq_from_suspend_flag = 0;
    return 0;
}

#ifdef CONFIG_OF
static struct of_device_id stk501xx_match_table[] =
{
    { .compatible = "mediatek,stk501xx", },
    {}
};
#endif /* CONFIG_OF */

/*
 * @brief: Proble function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 * @param[in] id: struct i2c_device_id *
 *
 * @return: Success or fail
 *          0: Success
 *          others: Fail
 */
static int32_t stk501xx_i2c_probe(struct i2c_client* client,
                                  const struct i2c_device_id* id)
{
    struct common_function common_fn =
    {
        .bops = &stk_i2c_bops,
        .tops = &stk_t_ops,
        .gops = &stk_g_ops,
    };

    return stk_i2c_probe(client, &common_fn);
}

/*
 * @brief: Remove function for i2c_driver.
 *
 * @param[in] client: struct i2c_client *
 *
 * @return: 0
 */
static int32_t stk501xx_i2c_remove(struct i2c_client* client)
{
    return stk_i2c_remove(client);
}

/**
 * @brief:
 */
static int32_t stk501xx_i2c_detect(struct i2c_client* client, struct i2c_board_info* info)
{
    strcpy(info->type, STK501XX_NAME);
    return 0;
}

#ifdef CONFIG_PM_SLEEP
/*
 * @brief: Suspend function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
static int32_t stk501xx_i2c_suspend(struct device* dev)
{
    return stk501xx_suspend(dev);
}

/*
 * @brief: Resume function for dev_pm_ops.
 *
 * @param[in] dev: struct device *
 *
 * @return: 0
 */
static int32_t stk501xx_i2c_resume(struct device* dev)
{
    return stk501xx_resume(dev);
}

static const struct dev_pm_ops stk501xx_pm_ops =
{
    .suspend = stk501xx_i2c_suspend,
    .resume = stk501xx_i2c_resume,
};
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_ACPI
static const struct acpi_device_id stk501xx_acpi_id[] =
{
    {"STK501XX", 0},
    {}
};
MODULE_DEVICE_TABLE(acpi, stk501xx_acpi_id);
#endif /* CONFIG_ACPI */

static const struct i2c_device_id stk501xx_i2c_id[] =
{
    {STK501XX_NAME, 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, stk501xx_i2c_id);

static struct i2c_driver stk501xx_i2c_driver =
{
    .probe = stk501xx_i2c_probe,
    .remove = stk501xx_i2c_remove,
    .detect = stk501xx_i2c_detect,
    .id_table = stk501xx_i2c_id,
    .class = I2C_CLASS_HWMON,
    .driver = {
        .owner = THIS_MODULE,
        .name = STK501XX_NAME,
#ifdef CONFIG_PM_SLEEP
        .pm = &stk501xx_pm_ops,
#endif
#ifdef CONFIG_ACPI
        .acpi_match_table = ACPI_PTR(stk501xx_acpi_id),
#endif /* CONFIG_ACPI */
#ifdef CONFIG_OF
        .of_match_table = stk501xx_match_table,
#endif /* CONFIG_OF */
    }
};
static int __init stk501xx_module_init(void)
{

    STK_ERR("driver version:%s\n", STK_HEADER_VERSION);

    return i2c_add_driver(&stk501xx_i2c_driver);
}

static void __exit stk501xx_module_exit(void)
{
    i2c_del_driver(&stk501xx_i2c_driver);
}

module_init(stk501xx_module_init);
module_exit(stk501xx_module_exit);

//module_i2c_driver(stk501xx_i2c_driver);

MODULE_AUTHOR("Sensortek");
MODULE_DESCRIPTION("stk501xx sar driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(STK_MTK_VERSION);
