#include "device/camera_sensor.h"

extern const camera_sensor_t camera_sensor_hi704;
extern const camera_sensor_t camera_sensor_gc0310;

const camera_sensor_t *const camera_sensors[] = {
    &camera_sensor_hi704,
    &camera_sensor_gc0310,
};

const size_t camera_sensor_count = sizeof(camera_sensors) / sizeof(camera_sensors[0]);
