#include <linux/module.h>
#include <linux/kernel.h>
#include "ssp_motor.h"
#include "ssp_dev.h"
#include "ssp_cmd_define.h"
#include "ssp_comm.h"

int ssp_motor_state;
struct work_struct ssp_work_motor;

static void ssp_motor_work_func(struct work_struct *work)
{
	int ret = 0;
	struct ssp_data *data = get_ssp_data();
	char buf = data->accel_motor_coef;

	if (ssp_motor_state)
		ret = ssp_send_command(data, CMD_SETVALUE, TYPE_MCU, MOTOR_ON, 0, &buf, sizeof(buf), NULL, NULL);
	else
		ret = ssp_send_command(data, CMD_SETVALUE, TYPE_MCU, MOTOR_OFF, 0, &buf, sizeof(buf), NULL, NULL);

	if (ret < 0)
		ssp_errf("send motor state failed : %d", ssp_motor_state);
	else
		ssp_infof("send ssp_motor_state success : %d(%d)", ssp_motor_state, buf);
}

static void ssp_motor_callback(int state)
{
	struct ssp_data *data = get_ssp_data();

	//ssp_infof("ssp_motor_state : %d", state);
	ssp_motor_state = state;

	queue_work(data->debug_wq, &ssp_work_motor);
}

void init_motor_control(void)
{
	ssp_infof();
	INIT_WORK(&ssp_work_motor, ssp_motor_work_func);
	ssp_motor_state = 0;
}

void sync_motor_state(void)
{
	ssp_infof("ssp_motor_state : %d", ssp_motor_state);
	ssp_motor_callback(ssp_motor_state);
}

void setSensorCallback(bool flag, int duration)
{
	if ((flag ==  true && duration >= 20) || (flag == false && ssp_motor_state)) {
		ssp_infof("ssp_motor_state : %d, duration : %d", flag, duration);
		ssp_motor_callback(flag);
	}
}
