#ifndef _SENSOR_USER_NODE_H_
#define _SENSOR_USER_NODE_H_

#define NODE_NAME  "sensor_node"

struct sar_driver_func
{
    ssize_t (*set_onoff)(const char *buf, size_t count);
    ssize_t (*get_onoff)(char *buf);
};

extern struct sar_driver_func sar_func;

#endif