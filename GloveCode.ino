// https://github.com/tinkersprojects/LCD-Simple-Menu-Library

#include <LiquidCrystal_PCF8574.h>
#include <SensorFusion.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>
#include <WiFi.h>
#include "task.h"

/////////////////////////////
// MAKE SURE THESE ARE SAME AS DRONE HAND CODE COPY PASTE
const char* name = "24f7ad15-fdde-4c83-8327-400a87de818c";
const char* pass = "9e267cbc-7e40-4d95-be9a-c8f75ba197fa";
/////////////////////////////

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

Task sendThumbTask("sendThumbTask", 100, NULL, [](void* arg) -> void {
    lcd.clear();
    lcd.home();
    int thumbPos = analogRead(TEST_POT);
    thumbPos = map(thumbPos, 0, 4095, 0, 180);
    lcd.printf("thumb: %i", thumbPos);
    // prepare for sending OK
    lcd.setCursor(0, 1);

    // Send it
    WiFiClient client;
    bool connected = client.connect("foo.com", 80); // any domain will do as long as it's port 80
    if (!connected) {
        lcd.print("disconnected!");
        return;
    }
    client.printf("thumb=%i\n", thumbPos);
    lcd.print("> ");
    lcd.print(client.readStringUntil('\n'));
    client.stop();
});

void connectToWifi() {
    lcd.clear();
    lcd.home();
    lcd.print("Connect to hand");
    lcd.setCursor(0, 1);
    WiFi.begin(name, pass);
    int x = 0;
    while (WiFi.status() != WL_CONNECTED && x <= 16) {
        delay(1000);
        lcd.print(".");
        x++;
    }
    if (x > 16) {
        lcd.clear();
        lcd.home();
        lcd.print("Failed to");
        lcd.setCursor(0, 1);
        lcd.print("connect");
        for(;;)yield();
    }
    lcd.clear();
    lcd.home();
    lcd.print("connect ok");
    delay(100);
}

TaskManager mgr;
void setup() {
    Serial.begin(115200);
    // Connect IMU
    imu.begin();
    
    // Start LCD
    lcd.begin(16, 2);
    lcd.setBacklight(255);

    // Configure IMU
    imu.setAccelerometerRange(MPU6050_RANGE_8_G);
    imu.setGyroRange(MPU6050_RANGE_500_DEG);
    imu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    connectToWifi();

    // Setup tasks
    mgr.add(&accelTask);
    mgr.add(&sendThumbTask);
}



void loop() {
    mgr.run_all();
}
