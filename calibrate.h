#ifndef CALIBRATE_H
#define CALIBRATE_H


#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Preferences.h>

// MAKE SURE THESE NAMES ARE IN THE SAME ORDER AS OPTIONS IN screen.h !!!

const char* calib_keys[] = {
    "AC",
    "TO",
    "TC",
    "IO",
    "IC",
    "MO",
    "MC"
};
#define CALIB_ACCEL 0
#define CALIB_THUMB_LO 1
#define CALIB_THUMB_HI 2
#define CALIB_INDEX_LO 3
#define CALIB_INDEX_HI 4
#define CALIB_MIDDLE_LO 5
#define CALIB_MIDDLE_HI 6
const size_t num_keys = sizeof(calib_keys) / sizeof(calib_keys[0]);
int fast_calib_values[] = {
    -1,
    0,
    4095,
    0,
    4095,
    0,
    4095
};

Preferences calibration_options;
int fpins[3] = {0};
Adafruit_MPU6050* accel_calibrate;
void setup_calibrate(int finger1Pin, int finger2Pin, int finger3Pin, Adafruit_MPU6050* a) {
    calibration_options.begin("MagicHands", true);
    for (int i = 0; i < num_keys; i++) {
        if (calibration_options.isKey(calib_keys[i])) fast_calib_values[i] = calibration_options.getInt(calib_keys[i]);
    }
    calibration_options.end();
    calibration_options.begin("MagicHands", false);
    fpins[0] = finger1Pin;
    fpins[1] = finger2Pin;
    fpins[2] = finger3Pin;
    accel_calibrate = a;
}

void do_calibrate(int what) {
    if (what == 0) {
        // Don't brick the accelerometer
//        accel_calibrate->reset();
//        accel_calibrate->setAccelerometerRange(MPU6050_RANGE_8_G);
//        accel_calibrate->setGyroRange(MPU6050_RANGE_500_DEG);
//        accel_calibrate->setFilterBandwidth(MPU6050_BAND_21_HZ);
        return;
    }
    static const int what_pin[] = {-1, 0, 0, 1, 1, 2, 2};
    int pin = fpins[what_pin[what]];
    int val = analogRead(pin);
    fast_calib_values[what] = val;
    calibration_options.putInt(calib_keys[what], val);
}

int get_calibrate(int what) {
    return fast_calib_values[what];
}

#endif
