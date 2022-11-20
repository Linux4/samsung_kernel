#ifndef __SENSOR_H_
#define __SENSOR_H_

int init_accelerometer(bool en);
int init_accelerometer_uncal(bool en);
int init_magnetometer(bool en);
int init_magnetometer_uncal(bool en);
int init_flip_cover_detector(bool en);
int init_gyroscope(bool en);
int init_gyroscope_uncal(bool en);
int init_light(bool en);
int init_light_cct(bool en);
int init_light_ir(bool en);
int init_light_seamless(bool en);
int init_light_autobrightness(bool en);
int init_proximity(bool en);
int init_proximity_raw(bool en);
int init_proximity_calibration(bool en);
int init_pressure(bool en);
int init_step_counter(bool en);
int init_scontext(bool en);
int init_interrupt_gyroscope(bool en);
int init_vdis_gyroscope(bool en);
int init_step_detector(bool en);
int init_magnetometer_power(bool en);
int init_significant_motion(bool en);
int init_tilt_detector(bool en);
int init_pick_up_gesture(bool en);
int init_call_gesture(bool en);
int init_wake_up_motion(bool en);
int init_protos_motion(bool en);
int init_pocket_mode(bool en);
int init_pocket_mode_lite(bool en);
int init_super(bool en);
int init_hub_debugger(bool en);
int init_tap_tracker(bool en);
int init_shake_tracker(bool en);
int init_move_detector(bool en);
int init_led_cover_event(bool en);
int init_thermistor(bool en);
#endif
