// https://github.com/tinkersprojects/LCD-Simple-Menu-Library

#include <LiquidCrystal_PCF8574.h>
#include <SensorFusion.h>
#include <Adafruit_MPU6050.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "task.h"

/////////////////////////////
// MAKE SURE THESE ARE SAME AS DRONE HAND CODE COPY PASTE
#define DEVICE_NAME         "24f7ad15-fdde-4c83-8327-400a87de818c"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SERVO_CHAR_UUID     "beb5483e-36e1-4688-b7f5-ea07361b26a8"
/////////////////////////////

Adafruit_MPU6050 imu;
SF sensor_fusion;

LiquidCrystal_PCF8574 lcd(0x27);

#define TEST_POT 36

BLECharacteristic* servoChar;


Task accelTask("accelTask", 0, NULL, [](void* arg) -> void {
    sensors_event_t a, g, trash;
    imu.getEvent(&a, &g, &trash);
    (void)trash;
    sensor_fusion.MahonyUpdate(
        g.gyro.x, g.gyro.y, g.gyro.z,
        a.acceleration.x, a.acceleration.y, a.acceleration.z,
        sensor_fusion.deltatUpdate());
});

Task sendservoTask("sendservoTask", 100, NULL, [](void* arg) -> void {
    lcd.clear();
    lcd.home();
    int servoPos = analogRead(TEST_POT);
    servoPos = map(servoPos, 0, 4095, 0, 180);
    lcd.printf("servo: %i", servoPos);
    // prepare for sending OK
    lcd.setCursor(0, 1);

    // Send it
    servoChar->setValue(servoPos);
    servoChar->notify();
});

void setupBLE() {
    BLEDevice::init(DEVICE_NAME);
    BLEServer* server = BLEDevice::createServer();
    BLEService* service = server->createService(SERVICE_UUID);
    servoChar = service->createCharacteristic(
                    SERVO_CHAR_UUID,
                    BLECharacteristic::PROPERTY_READ |
                    BLECharacteristic::PROPERTY_WRITE |
                    BLECharacteristic::PROPERTY_NOTIFY
                );
    service->start();
    BLEAdvertising* ad = BLEDevice::getAdvertising();
    ad->addServiceUUID(SERVICE_UUID);
    ad->setScanResponse(true);
    ad->setMinPreferred(0x06);  
    ad->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();
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

    setupBLE();

    // Setup tasks
    mgr.add(&accelTask);
    mgr.add(&sendservoTask);
}



void loop() {
    mgr.run_all();
}
