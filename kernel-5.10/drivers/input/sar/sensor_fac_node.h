#ifndef _SENSOR_FAC_NODE_H_
#define _SENSOR_FAC_NODE_H_

typedef struct sar_driver {
    char *sar_name_drv;
    void (*get_cali)(void);
    ssize_t (*set_enable)(const char *buf, size_t count);
    ssize_t (*get_enable)(char *buf);
} sar_driver_t;

#endif