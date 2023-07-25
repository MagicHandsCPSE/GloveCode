#include <LiquidCrystal_PCF8574.h>
#include <SensorFusion.h>
#include <Adafruit_MPU6050.h>
#include <BLEDevice.h>
#include "task.h"
#include "filter.h"
#include "oic.h"
#include "screen.h"

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
Screen screen(&lcd);

#define FLEX1 36
#define FLEX2 39
#define FLEX3 34

bool oktoconnect = false;
bool connected = false;
class whenDisconnect : public BLEClientCallbacks {
    void onConnect(BLEClient* _) {
    }
    void onDisconnect(BLEClient* _) {
        connected = false;
        screen.set_conn_status(SCANNING);
    }
};

bool connect() {
    screen.set_conn_status(CONNECTING);
    BLEClient* client = BLEDevice::createClient();
    client->setClientCallbacks(new whenDisconnect());
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
    screen.set_conn_status(CONNECTED);
    return true;
error:
    client->disconnect();
    screen.set_conn_status(ERROR);
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

Task sendservoTask("sendservoTask", 5, NULL, [](void* arg) -> void {
    // TODO: make this more DRY
    uint16_t analog1 = analogRead(FLEX1);
    uint16_t analog2 = analogRead(FLEX2);
    uint16_t analog3 = analogRead(FLEX3);
    static LPF<16> filt1;
    static LPF<16> filt2;
    static LPF<16> filt3;
    uint8_t pos1 = map(filt1.filter(analog1), 0, 4095, 0, 180);
    uint8_t pos2 = map(filt2.filter(analog2), 0, 4095, 0, 180);
    uint8_t pos3 = map(filt3.filter(analog3), 0, 4095, 0, 180);
    static OIC<8, 100> oic1;
    static OIC<8, 100> oic2;
    static OIC<8, 100> oic3;
    if (oic1.didChange(pos1)) servo1Characteristic->writeValue(pos1);
    if (oic2.didChange(pos2)) servo2Characteristic->writeValue(pos2);
    if (oic3.didChange(pos3)) servo3Characteristic->writeValue(pos3);
});

void startScan() {
    screen.set_conn_status(SCANNING);
    BLEDevice::init("");
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new chooseWithService());
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);
    scan->start(10, false);
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
    accelTask.run();
    screen.blinky_status();
    if (oktoconnect && !connected) {
        connected = connect();
        sendservoTask.running = connected;
    }
    sendservoTask.run();
    yield();
}
