/*
* @file sgpu.h
* @copyright 2021 Samsung Electronics
*/

#ifndef __SGPU_H__
#define __SGPU_H__


#ifdef CONFIG_DRM_AMDGPU_DUMP
# if defined(CONFIG_DRM_SGPU_DUMP_LOG)
#  define sgpu_dump_print(buf, size, format, ...) printk(KERN_ALERT format, ##__VA_ARGS__)
# elif defined(CONFIG_DRM_SGPU_DUMP_PSTORE)
#  define sgpu_dump_print(buf, size, format, ...) snprintf(buf, size, format, ##__VA_ARGS__)
# else
#  error "Either CONFIG_DRM_SGPU_DUMP_LOG or CONFIG_DRM_SGPU_DUMP_PSTORE must be defined."
# endif
#else /* !CONFIG_DRM_AMDGPU_DUMP */
# define sgpu_dump_print(buf, size, format, ...)
#endif


#endif /* __SGPU_H__ */
