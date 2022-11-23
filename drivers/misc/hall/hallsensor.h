#ifndef __HALL_H__
#define __HALL_H__

#define HALL_NEAR  (0)
#define HALL_FAR   (1)


struct hall_t {
    unsigned int irq;
    unsigned int gpiopin;
    unsigned int curr_mode;
    struct input_dev * dev;
    spinlock_t   spinlock;
};

#endif /* __HALL_H__*/
