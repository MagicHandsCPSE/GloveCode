// https://github.com/tinkersprojects/LCD-Simple-Menu-Library

#include <LiquidCrystal_PCF8574.h>
#include <SensorFusion.h>
#include <Adafruit_MPU6050.h>
#include <BLEDevice.h>
#include "task.h"

/////////////////////////////
// MAKE SURE THESE ARE SAME AS DRONE HAND CODE COPY PASTE
#define DEVICE_NAME         "6a2017c6-d018-4930-8d1a-913289dfebf7"
#define SERVICE_UUID        "778f40a1-2937-4639-a10b-e0c112b04b0e"
#define SERVO1_CHAR_UUID    "086bf84b-736f-45e0-8e35-6adcd6cc0ec4"
#define SERVO2_CHAR_UUID    "f676b321-31a8-4179-8640-ce5699cf0721"
#define SERVO3_CHAR_UUID    "c0b627e5-c0ce-4b5f-b590-a096c3514db7"
/////////////////////////////

static BLEUUID serviceUUID(SERVICE_UUID);
static BLEUUID servo1CharacteristicUUID(SERVO1_CHAR_UUID);
static BLEUUID servo2CharacteristicUUID(SERVO2_CHAR_UUID);
static BLEUUID servo3CharacteristicUUID(SERVO3_CHAR_UUID);
static BLERemoteCharacteristic* servo1Characteristic;
static BLERemoteCharacteristic* servo2Characteristic;
static BLERemoteCharacteristic* servo3Characteristic;
static BLEAdvertisedDevice* glove;

Adafruit_MPU6050 imu;
SF sensor_fusion;

LiquidCrystal_PCF8574 lcd(0x27);

#define TEST_POT 36

bool oktoconnect = false;
bool connected = false;
class freezeOnDisconnect : public BLEClientCallbacks {
    void onConnect(BLEClient* _) {
    }
    void onDisconnect(BLEClient* _) {
        connected = false;
    }
};

bool connect() {
    lcd.clear();
    lcd.home();
    lcd.print("connecting...");
    BLEClient* client = BLEDevice::createClient();
    client->setClientCallbacks(new freezeOnDisconnect());
    client->connect(glove);
    client->setMTU(517); // maximum
    BLERemoteService* service = client->getService(serviceUUID);
    if (service == NULL) goto error;
    servo1Characteristic = service->getCharacteristic(servo1CharacteristicUUID);
    servo2Characteristic = service->getCharacteristic(servo2CharacteristicUUID);
    servo3Characteristic = service->getCharacteristic(servo3CharacteristicUUID);
    if (servo1Characteristic == NULL) goto error;
    if (servo2Characteristic == NULL) goto error;
    if (servo3Characteristic == NULL) goto error;
    lcd.clear();
    lcd.home();
    lcd.print("connected!");
    return true;
error:
    client->disconnect();
    lcd.clear();
    lcd.home();
    lcd.print("error");
    return false;
}

class chooseWithService : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) {
        if (device.haveServiceUUID() && device.isAdvertisingService(serviceUUID)) {
            BLEDevice::getScan()->stop();
            glove = new BLEAdvertisedDevice(device);
            oktoconnect = true;
        }
    }
};

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
    int servoPos = analogRead(TEST_POT);
    servoPos = map(servoPos, 0, 4095, 0, 180);
    // TODO add read code
    uint8_t val = (uint8_t) servoPos;
    servo1Characteristic->writeValue(val);
    servo2Characteristic->writeValue(val);
    servo3Characteristic->writeValue(val);
});

void startScan() {
    BLEDevice::init("");
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new chooseWithService());
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);
    scan->start(9999, false);
}

void setup() {
    // Connect IMU
    imu.begin();
    
    // Start LCD
    lcd.begin(16, 2);
    lcd.setBacklight(255);

    // Configure IMU
    imu.setAccelerometerRange(MPU6050_RANGE_8_G);
    imu.setGyroRange(MPU6050_RANGE_500_DEG);
    imu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    startScan();
}

void loop() {
    sendservoTask.run();
    accelTask.run();
    if (oktoconnect && !connected) {
        connected = connect();
        sendservoTask.running = connected;
    }
    yield();
}
