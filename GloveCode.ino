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

Task accelTask("accelTask", 0, [](void* arg) -> void {
    sensors_event_t a, g, trash;
    imu.getEvent(&a, &g, &trash);
    (void)trash;
    sensor_fusion.MahonyUpdate(
        g.gyro.x, g.gyro.y, g.gyro.z,
        a.acceleration.x, a.acceleration.y, a.acceleration.z,
        sensor_fusion.deltatUpdate());
});

Task testTask("testTask", 500, [](void* arg) -> {
    lcd.clear();
    lcd.home();
    lcd.printf("yaw: %3f", sensor_fusion.getYaw());
});

TaskManager mgr;
void setup() {
    Serial.begin(115200);
    Serial.print("Connecting IMU...         ");
    if (!imu.begin()) {
       Serial.println("[ FAIL ]\nFATAL ERROR: IMU disconnected!!");
       ESP.restart();
    } else
        Serial.println("[  OK  ]");
    Serial.print("Starting LCD...           ");
    lcd.begin(16, 2);
    lcd.setBacklight(255);
    Serial.println("[  OK  ]");
    Serial.print("Configuring IMU accel...  ");
    imu.setAccelerometerRange(MPU6050_RANGE_8_G);
    Serial.println("[  OK  ]");
    Serial.print("Configuring IMU gyro...   ");
    imu.setGyroRange(MPU6050_RANGE_500_DEG);
    Serial.println("[  OK  ]");
    Serial.print("Configuring IMU filter... ");
    imu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("[  OK  ]");
    Serial.print("Starting tasks...         ");
    mgr.add(&accelTask);
    mgr.add(&testTask);
    Serial.println("[  OK  ]");
}



void loop() {
    mgr.run_all();
}
