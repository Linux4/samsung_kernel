#ifndef _SENSOR_USER_NODE_H_
#define _SENSOR_USER_NODE_H_
/*A06 code for SR-AL7160A-01-183 | SR-AL7160A-01-178 by huofudong at 20240417 start*/
typedef struct sar_driver_func {
    char *sar_name_drv;
    ssize_t (*set_onoff)(const char *buf, size_t count);
    ssize_t (*get_onoff)(char *buf);
    ssize_t (*set_dumpinfo)(const char *buf, size_t count);
    ssize_t (*get_dumpinfo)(char *buf);
    ssize_t (*set_enable)(const char *buf, size_t count);
    ssize_t (*get_enable)(char *buf);
    ssize_t (*set_receiver_turnon)(const char *buf, size_t count);
    void (*get_cali)(void);
} sar_driver_func_t;
/*A06 code for SR-AL7160A-01-183 | SR-AL7160A-01-178 by huofudong at 20240417 end*/
extern sar_driver_func_t g_usernode;

#endif
