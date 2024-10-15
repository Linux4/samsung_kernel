#include <android-base/logging.h>

#include <unistd.h>
#include <fcntl.h>

#include "applaunch_hint.h"

int setApplaunch(bool enabled)
{
	int fd;
	pid_t pid;
	int ret = 0;

	fd = open(APPLAUNCH_SYSFS_PATH, O_RDWR);
	if (fd < 0) {
		LOG(INFO) << "PowerHAL: fd open error for AppLaunch: " << fd;
		ret = -1;
	}
	else {
		char send_buf[10];
		snprintf(send_buf, sizeof(send_buf), "%d", enabled);
		write(fd, send_buf, strlen(send_buf));
		close(fd);
	}
	return ret;
}


