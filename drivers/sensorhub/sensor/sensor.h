#ifndef __SENSOR_H_
#define __SENSOR_H_

void init_accelerometer(bool en);
void init_accelerometer_uncal(bool en);
void init_magnetometer(bool en);
void init_magnetometer_uncal(bool en);
void init_flip_cover_detector(bool en);
void init_gyroscope(bool en);
void init_gyroscope_uncal(bool en);
void init_light(bool en);
void init_light_cct(bool en);
void init_light_ir(bool en);
void init_light_seamless(bool en);
void init_light_autobrightness(bool en);
void init_proximity(bool en);
void init_proximity_raw(bool en);
void init_proximity_calibration(bool en);
void init_pressure(bool en);
void init_step_counter(bool en);
void init_scontext(bool en);
void init_interrupt_gyroscope(bool en);
void init_vdis_gyroscope(bool en);
void init_step_detector(bool en);
void init_magnetometer_power(bool en);
void init_significant_motion(bool en);
void init_tilt_detector(bool en);
void init_pick_up_gesture(bool en);
void init_call_gesture(bool en);
void init_wake_up_motion(bool en);
void init_protos_motion(bool en);
void init_pocket_mode(bool en);
void init_pocket_mode_lite(bool en);
void init_super(bool en);
void init_tap_tracker(bool en);
void init_shake_tracker(bool en);
void init_move_detector(bool en);
void init_led_cover_event(bool en);
void init_thermistor(bool en);
#endif
