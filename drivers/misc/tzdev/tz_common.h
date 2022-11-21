#ifndef __TZ_COMMON_H__
#define __TZ_COMMON_H__

#include <linux/types.h>
#include <linux/limits.h>

/*
 * CA_ID is a full basename of CA executable.
 * It can not be longer than max supported filename length defined in Linux.
 */
#define CA_ID_LEN			(256)

#define TZ_IOC_MAGIC			'c'

#define TZIO_SMC			_IOWR(TZ_IOC_MAGIC, 101, struct tzio_smc_cmd *)
#define TZIO_WAIT_EVT			_IOR(TZ_IOC_MAGIC, 112, int *)
#define TZIO_GET_PIPE			_IOR(TZ_IOC_MAGIC, 113, int *)
#define TZIO_GET_ACCESS_INFO		_IOWR(TZ_IOC_MAGIC, 119, struct tzio_access_info *)
#define TZIO_MEM_REGISTER		_IOWR(TZ_IOC_MAGIC, 120, struct tzio_mem_register *)
#define TZIO_MEM_RELEASE		_IOW(TZ_IOC_MAGIC, 121, int)

struct tzio_access_info {
	pid_t pid;			/* CA PID */
	pid_t gid;			/* CA GID - to be received from TZDEV */
	char ca_name[CA_ID_LEN];	/* CA identity - to be received from TZDEV */
};

struct tzio_smc_cmd {
	__s32 args[2];			/* SMC args (in) */
	__s32 ret;			/* SMC return value (out) */
};

struct tzio_mem_register {
	pid_t pid;			/* Memory region owner's PID (in) */
	const void *ptr;		/* Memory region start (in) */
	size_t size;			/* Memory region size (in) */
	int id;				/* Memory region ID (out) */
	int write;			/* 1 - rw, 0 - ro */
};

#endif /*!__TZ_COMMON_H__*/
