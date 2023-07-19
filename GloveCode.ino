// https://github.com/MajicDesigns/MD_Menu/blob/main/examples/Menu_Test/Menu_Test_Disp.cpp

#include <LiquidCrystal_PCF8574.h>
#include <SensorFusion.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>
#include "task.h"

Adafruit_MPU6050 imu;
SF sensor_fusion;

LiquidCrystal_PCF8574 lcd(0x27);

#define TEST_POT 36

Task accelTask("accelTask", 0, NULL, [](void* arg) -> void {
    sensors_event_t a, g, trash;
    imu.getEvent(&a, &g, &trash);
    (void)trash;
    sensor_fusion.MahonyUpdate(
        g.gyro.x, g.gyro.y, g.gyro.z,
        a.acceleration.x, a.acceleration.y, a.acceleration.z,
        sensor_fusion.deltatUpdate());
});

Task testTask("testTask", 500, NULL, [](void* arg) -> void {
    lcd.clear();
    lcd.home();
    lcd.printf("yaw: %3f", sensor_fusion.getYaw());
});

TaskManager mgr;
void setup() {
    Serial.begin(115200);
    // Connect IMU
    if (!imu.begin()) {
       Serial.println("ERROR: IMU disconnected!!");
       ESP.restart();
    }
    // Start LCD
    lcd.begin(16, 2);
    lcd.setBacklight(255);

    // Configure IMU
    imu.setAccelerometerRange(MPU6050_RANGE_8_G);
    imu.setGyroRange(MPU6050_RANGE_500_DEG);
    imu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    // Setup tasks
    mgr.add(&accelTask);
    mgr.add(&testTask);
}



void loop() {
    mgr.run_all();
}
