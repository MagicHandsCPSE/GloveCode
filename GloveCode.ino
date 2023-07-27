
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SensorFusion.h>
#include <Adafruit_MPU6050.h>
#include <BLEDevice.h>
#include <ESP32Encoder.h>
#include <Bounce2.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
#include "task.h"
#include "filter.h"
#include "oic.h"
#include "screen.h"
#include "calibrate.h"

/////////////////////////////
// MAKE SURE THESE ARE SAME AS DRONE HAND CODE COPY PASTE
#define DEVICE_NAME         "6a2017c6-d018-4930-8d1a-913289dfebf7"
#define SERVICE_UUID        "778f40a1-2937-4639-a10b-e0c112b04b0e"
#define SERVO1_CHAR_UUID    "086bf84b-736f-45e0-8e35-6adcd6cc0ec4"
#define SERVO2_CHAR_UUID    "f676b321-31a8-4179-8640-ce5699cf0721"
#define SERVO3_CHAR_UUID    "c0b627e5-c0ce-4b5f-b590-a096c3514db7"
// These are well-known UUIDs - don't change
#define BATTERY_SERVICE 0x180F
#define BATTERY_CHARACT 0x2A19 
/////////////////////////////

static BLEUUID serviceUUID(SERVICE_UUID);
static BLEUUID servo1CharacteristicUUID(SERVO1_CHAR_UUID);
static BLEUUID servo2CharacteristicUUID(SERVO2_CHAR_UUID);
static BLEUUID servo3CharacteristicUUID(SERVO3_CHAR_UUID);
static BLEUUID batteryServiceUUID((uint16_t)BATTERY_SERVICE);
static BLEUUID batteryCharacteristicUUID((uint16_t)BATTERY_CHARACT);
static BLERemoteCharacteristic* servo1Characteristic;
static BLERemoteCharacteristic* servo2Characteristic;
static BLERemoteCharacteristic* servo3Characteristic;
static BLEAdvertisedDevice* glove;

Adafruit_MPU6050 imu;
SF sensor_fusion;

#define OLED_DC     16
#define OLED_CS     17
#define OLED_RESET  4
Adafruit_SSD1306 display(128, 32, &SPI, OLED_DC, OLED_RESET, OLED_CS);
Screen screen(&display);

ESP32Encoder selector;
#define ENC1 35
#define ENC2 32
Bounce select_btn;
Bounce back;
#define ENC_SELECT 33
#define ENC_BACK 25

#define FLEX1 36
#define FLEX2 39
#define FLEX3 34

SFE_MAX1704X battery(MAX1704X_MAX17048);

// Forward declare
class chooseWithService;
bool connect();
void startScan();
Task* sendservoTask;
Task* draw_task;
Task* btTask;
Task* accelTask;
Task* nav_calibrate_task;
Task* battery_task;


bool oktoconnect = false;
bool connected = false;

class chooseWithService : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) {
        if (device.haveServiceUUID() && device.isAdvertisingService(serviceUUID)) {
            BLEDevice::getScan()->stop();
            glove = new BLEAdvertisedDevice(device);
            oktoconnect = true;
        }
    }
};

void show_remote_battery(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t len, bool notify) {
    screen.set_d_battery(*data);
}

void dummy(BLEScanResults _) {}

void startScan() {
    screen.set_conn_status(SCANNING);
    BLEDevice::init("");
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new chooseWithService());
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);
    scan->start(10, dummy, false);
}

class whenDisconnect : public BLEClientCallbacks {
    void onConnect(BLEClient* _) {
    }
    void onDisconnect(BLEClient* _) {
        connected = false;
        oktoconnect = false;
        delete glove;
        glove = NULL;
        btTask->running = true;
        sendservoTask->running = false;
        Serial.println("Re-scanning for devices after connection lost");
        startScan();
    }
};

bool connect() {
    BLERemoteService* bservice;
    BLERemoteCharacteristic* bchar;
    Serial.println("called connect()");
    screen.set_conn_status(CONNECTING);
    Serial.println("calling create client");
    BLEClient* client = BLEDevice::createClient();
    client->setClientCallbacks(new whenDisconnect());
    Serial.println("calling client->connect()");
    client->connect(glove);
    client->setMTU(517); // maximum
    Serial.println("Called ->connect() returned");
    BLERemoteService* service = client->getService(serviceUUID);
    if (service == NULL) goto error;
    servo1Characteristic = service->getCharacteristic(servo1CharacteristicUUID);
    servo2Characteristic = service->getCharacteristic(servo2CharacteristicUUID);
    servo3Characteristic = service->getCharacteristic(servo3CharacteristicUUID);
    if (servo1Characteristic == NULL) goto error;
    if (servo2Characteristic == NULL) goto error;
    if (servo3Characteristic == NULL) goto error;
    // Get battery
    bservice = client->getService(batteryServiceUUID);
    if (bservice == NULL) goto error;
    bchar = bservice->getCharacteristic(batteryCharacteristicUUID);
    if (bchar == NULL) goto error;
    bchar->registerForNotify(show_remote_battery);
    screen.set_conn_status(CONNECTED);
    return true;
error:
    client->disconnect();
    screen.set_conn_status(ERROR);
    return false;
}


uint8_t pos1 = 0, pos2 = 0, pos3 = 0;


TaskHandle_t btTaskHandle;

void runBtTask(void* arg) {
    startScan();
    for (;;) btTask->run();
}
void startBtTask() {
    sendservoTask->running = false;
    xTaskCreatePinnedToCore(runBtTask, "bluetoothTaskLoop", 8192, NULL, 0, &btTaskHandle, 0);
}

void setup() {
    btTask = new Task("bluetooth", 0, NULL, [](void* arg) -> void {
        if (oktoconnect && !connected) {
            connected = connect();
            sendservoTask->running = connected;
            btTask->running = !connected;
        }
    });
    
    accelTask = new Task("accelTask", 0, NULL, [](void* arg) -> void {
        sensors_event_t a, g, trash;
        imu.getEvent(&a, &g, &trash);
        (void)trash;
        sensor_fusion.MahonyUpdate(
            g.gyro.x, g.gyro.y, g.gyro.z,
            a.acceleration.x, a.acceleration.y, a.acceleration.z,
            sensor_fusion.deltatUpdate());
    });
    
    nav_calibrate_task = new Task("nav", 50, NULL, [](void* arg) -> void {
        select_btn.update();
        back.update();
        if (screen.is_home()) { // Don't do calibration or scroll on home screen
            if (select_btn.fell()) screen.switchScreen(false);
            return; 
        }
        if (select_btn.fell()) { screen.switchScreen(true); return; }
        int change = selector.getCount();
        selector.clearCount();
        if (change > 0) screen.scroll(1);
        else if (change < 0) screen.scroll(-1);
        if (back.fell()) {
            // do calibration
            Serial.printf("Calibrate %s here\n", calib_options[screen.get_calibrate_option()]);
            do_calibrate(screen.get_calibrate_option());
        }
    });

    draw_task = new Task("draw", 0, NULL, [](void* arg) -> void {
        display.clearDisplay();
        screen.redraw();
        if (screen.is_home()) {
            display.setCursor(0, 24);
            display.printf("%3i %3i %3i", pos1, pos2, pos3);
        }
        display.display();
    });
    
    sendservoTask = new Task("sendservoTask", 5, NULL, [](void* arg) -> void {
        // TODO: make this more DRY
        uint16_t analog1 = analogRead(FLEX1);
        uint16_t analog2 = analogRead(FLEX2);
        uint16_t analog3 = analogRead(FLEX3);
        static LPF<32> filt1;
        static LPF<32> filt2;
        static LPF<32> filt3;
        pos1 = constrain(map(filt1.filter(analog1), get_calibrate(CALIB_THUMB_LO), get_calibrate(CALIB_THUMB_HI), 0, 180), 0, 180);
        pos2 = constrain(map(filt2.filter(analog2), get_calibrate(CALIB_INDEX_LO), get_calibrate(CALIB_INDEX_HI), 0, 180), 0, 180);
        pos3 = constrain(map(filt3.filter(analog3), get_calibrate(CALIB_MIDDLE_LO), get_calibrate(CALIB_MIDDLE_HI), 0, 180), 0, 180);
        static OIC<16, 200> oic1;
        static OIC<16, 200> oic2;
        static OIC<16, 200> oic3;
        if (oic1.didChange(pos1)) servo1Characteristic->writeValue(pos1);
        if (oic2.didChange(pos2)) servo2Characteristic->writeValue(pos2);
        if (oic3.didChange(pos3)) servo3Characteristic->writeValue(pos3);
    });

    battery_task = new Task("battery", 500, NULL, [](void* arg) -> void {
        int percent = (int)battery.getSOC();
        screen.set_g_battery(percent);
    });
    
    Serial.begin(115200);
    // Connect IMU
    imu.begin();

    bool has_battery = battery.begin();
    battery_task->running = has_battery;
    if (has_battery) {
        battery.quickStart();
    } else {
        screen.set_g_battery(-2);
    }
    
    selector.attachSingleEdge(ENC1, ENC2);
    selector.setFilter(1023);
    select_btn.attach(ENC_SELECT, INPUT);
    back.attach(ENC_BACK, INPUT);
    select_btn.interval(50);
    back.interval(50);

    // Start LCD
    display.begin(SSD1306_SWITCHCAPVCC);
    display.display();
    screen.begin();

    // Configure IMU
    imu.setAccelerometerRange(MPU6050_RANGE_8_G);
    imu.setGyroRange(MPU6050_RANGE_500_DEG);
    imu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    startBtTask();

    setup_calibrate(FLEX1, FLEX2, FLEX3, &imu);
}

void loop() {
    accelTask->run();
    nav_calibrate_task->run();
    draw_task->run();
    sendservoTask->run();
    yield();
}
