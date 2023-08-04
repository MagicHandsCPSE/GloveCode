#include "pins.h"
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
#define DEVICE_NAME         "MagicHands"
#define SERVICE_UUID        "778f40a1-2937-4639-a10b-e0c112b04b0e"
#define SERVO_CHAR_UUID     "086bf84b-736f-45e0-8e35-6adcd6cc0ec4"
#define DRONE_SERVICE_UUID  "6a2017c6-d018-4930-8d1a-913289dfebf7"
#define DRONE_CHAR_UUID     "23312560-c77d-4022-90f0-341837850a5c"
// These are well-known UUIDs - don't change
#define BATTERY_SERVICE BLEUUID((uint16_t)0x180F)
#define BATTERY_CHARACT BLEUUID((uint16_t)0x2A19)
/////////////////////////////

// Forward declare
class chooseWithService;
bool connect();
void startScan();
Task* sendcontrolsTask;
Task* draw_task;
Task* btTask;
Task* accelTask;
Task* nav_calibrate_task;
Task* battery_task;

uint8_t pos1 = 0, pos2 = 0, pos3 = 0;

static BLERemoteCharacteristic* servoCharacteristic;
static BLERemoteCharacteristic* droneCharacteristic;
static BLEAdvertisedDevice* glove;

Adafruit_MPU6050 imu;
SF sensor_fusion;

class CalibrateDoScreen: public CalibScreen {
    public:
    using CalibScreen::CalibScreen;
    void button() override {
        Serial.printf("Calibrate %s here\n", calib_options[this->calibrate_option]);
        do_calibrate(this->calibrate_option);
    }
};

bool hold = false;
int droneAltThrust = 0;
class HomeScreenActions: public HomeScreen {
    public:
    using HomeScreen::HomeScreen;
    void button() override {
        hold = !hold;
        Serial.printf("Button on home screen, hold=%s\n", hold ? "ON" : "OFF");
    }
    void up() override {
        Serial.println("Up on home screen");
        droneAltThrust = 1;
    }
    void down() override {
        Serial.println("Down on home screen");
        droneAltThrust = -1;
    }
    void drawMore() override {
        Adafruit_SSD1306* d = this->screen;
        // Draw bars to represent the servo positions
        d->drawFastHLine(0, 25, map(pos1, 0, 180, 0, 60), SSD1306_WHITE);
        d->drawFastHLine(0, 26, map(pos2, 0, 180, 0, 60), SSD1306_WHITE);
        d->drawFastHLine(0, 27, map(pos3, 0, 180, 0, 60), SSD1306_WHITE);
        // Draw bars to represent the x and y
        static int xs;
        static int ys;
        if (!hold) xs = (int)sensor_fusion.getPitch(), ys = (int)sensor_fusion.getRoll();
        int mx = map(xs, -90, 90, -30, 30);
        int my = map(ys, -90, 90, -30, 30);
        int ax = abs(mx);
        int ay = abs(my);
        int sx = mx < 0 ? 30 - mx : 30;
        int sy = my < 0 ? 30 - my : 30;
        d->drawFastHLine(sx, 29, mx, SSD1306_WHITE);
        d->drawFastHLine(sy, 30, my, SSD1306_WHITE);
        // Display "HOLD" if on hold
        if (!hold) return;
        d->setTextSize(2);
        d->setCursor(70, 16);
        d->print("HOLD");
    }
};

Adafruit_SSD1306 display(128, 32, &SPI, OLED_DC, OLED_RESET, OLED_CS);
ScreenManager screens(&display);
HomeScreenActions homeScreen(&screens);
CalibrateDoScreen calibScreen(&screens);

ESP32Encoder selector;
Bounce select_btn;
Bounce back;
Bounce forward;

SFE_MAX1704X battery(MAX1704X_MAX17048);

bool oktoconnect = false;
bool connected = false;

class chooseWithService : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) {
        if (device.haveServiceUUID() && device.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
            BLEDevice::getScan()->stop();
            glove = new BLEAdvertisedDevice(device);
            oktoconnect = true;
        }
    }
};

void show_remote_battery(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t len, bool notify) {
    homeScreen.d_battery = (int)(*data);
}

void dummy(BLEScanResults _) {}

void startScan() {
    homeScreen.status = SCANNING;
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
        sendcontrolsTask->running = false;
        homeScreen.d_battery = -1; // ?'s
        Serial.println("Re-scanning for devices after connection lost");
        startScan();
    }
};

bool connect() {
    BLERemoteService* bservice;
    BLERemoteCharacteristic* bchar;
    BLERemoteService* dservice;
    Serial.println("called connect()");
    homeScreen.status = CONNECTING;
    Serial.println("calling create client");
    BLEClient* client = BLEDevice::createClient();
    client->setClientCallbacks(new whenDisconnect());
    Serial.println("calling client->connect()");
    client->connect(glove);
    client->setMTU(517); // maximum
    Serial.println("Called ->connect() returned");
    BLERemoteService* service = client->getService(SERVICE_UUID);
    if (service == NULL) {
        Serial.println("failed to find servo service");
        goto error;
    }
    servoCharacteristic = service->getCharacteristic(SERVO_CHAR_UUID);
    if (servoCharacteristic == NULL) {
        Serial.println("failed to find servo char");
        goto error;
    }
    dservice = client->getService(DRONE_SERVICE_UUID);
    if (dservice == NULL) {
        Serial.println("failed to find drone service");
        goto error;
    }
    droneCharacteristic = dservice->getCharacteristic(DRONE_CHAR_UUID);
    if (droneCharacteristic == NULL) {
        Serial.println("failed to find drone char");
        goto error;
    }
    // Get battery
    bservice = client->getService(BATTERY_SERVICE);
    if (bservice == NULL) {
        Serial.println("failed to find battery service");
        goto error;
    }
    bchar = bservice->getCharacteristic(BATTERY_CHARACT);
    if (bchar == NULL) {
        Serial.println("failed to find battery char");
        goto error;
    }
    bchar->registerForNotify(show_remote_battery);
    homeScreen.status = CONNECTED;
    return true;
error:
    client->disconnect();
    homeScreen.status = ERROR;
    return false;
}


TaskHandle_t btTaskHandle;

void runBtTask(void* arg) {
    startScan();
    for (;;) {
        btTask->run();
        yield();
    }
}
void startBtTask() {
    sendcontrolsTask->running = false;
    xTaskCreatePinnedToCore(runBtTask, "bluetoothTaskLoop", 8192, NULL, 0, &btTaskHandle, 0);
}

void setup() {
    btTask = new Task("bluetooth", 0, NULL, [](void* arg) -> void {
        if (oktoconnect && !connected) {
            connected = connect();
            sendcontrolsTask->running = connected;
            btTask->running = !connected;
        }
    });

    accelTask = new Task("accelTask", 0, NULL, [](void* arg) -> void {
        sensors_event_t a, g, trash;
        imu.getEvent(&a, &g, &trash);
        (void)trash;
        sensor_fusion.MadgwickUpdate(
            g.gyro.x, g.gyro.y, g.gyro.z,
            a.acceleration.x, a.acceleration.y, a.acceleration.z,
            sensor_fusion.deltatUpdate());
    });

    nav_calibrate_task = new Task("nav", 50, NULL, [](void* arg) -> void {
        select_btn.update();
        back.update();
        forward.update();
        int change = selector.getCount();
        selector.clearCount();
        if (change > 0) screens.up();
        else if (change < 0) screens.down();
        if (back.fell()) screens.prevScreen();
        if (forward.fell()) screens.nextScreen();
        if (select_btn.fell()) screens.button();
    });

    draw_task = new Task("draw", 0, NULL, [](void* arg) -> void {
        screens.draw();
    });

    sendcontrolsTask = new Task("sendcontrolsTask", 5, NULL, [](void* arg) -> void {
        if (hold) return;
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
        int pack = (pos1 << 16) | (pos2 << 8) | pos3;
        if (oic1.didChange(pos1) || oic2.didChange(pos2) || oic3.didChange(pos3)) {
            servoCharacteristic->writeValue((uint8_t*)&pack, 4, false);
            //Serial.printf("Sent %#x to servo control\n", pack);
        }
        // Send drone command values
        int rawX = (int)sensor_fusion.getPitch();
        int rawY = (int)sensor_fusion.getRoll();
        int8_t x = (int8_t)rawX;
        int8_t y = (int8_t)rawY;
        static OIC<8, 1000> oicX;
        static OIC<8, 1000> oicY;
        int8_t droneA = (int8_t)droneAltThrust;
        droneAltThrust = 0;
        pack = ((*(uint8_t*)&x) << 16) | ((*(uint8_t*)&y) << 8) | (*(uint8_t*)&droneA);
        if (oicX.didChange(x) || oicY.didChange(y)) {
            droneCharacteristic->writeValue((uint8_t*)&pack, 4, false);
            //Serial.printf("Sent %#x to drone control\n", pack);
        }
    });

    battery_task = new Task("battery", 500, NULL, [](void* arg) -> void {
        int percent = (int)battery.getSOC();
        Serial.printf("Battery: %i\n", percent);
        homeScreen.g_battery = percent;
    });

    Serial.begin(115200);
    Wire.begin();
    // Connect IMU
    imu.begin();

    bool has_battery = battery.begin();
    battery_task->running = has_battery;
    if (has_battery) {
        battery.reset();
        battery.quickStart();
    } else {
        homeScreen.g_battery = -2;
        Serial.println("No battery ??");
    }

    selector.attachSingleEdge(ENC1, ENC2);
    selector.setFilter(1023);
    select_btn.attach(ENC_SELECT, INPUT);
    back.attach(BTN_BACK, INPUT);
    forward.attach(BTN_FORWARD, INPUT);
    select_btn.interval(50);
    back.interval(50);
    forward.interval(50);

    // Start OLED
    display.begin(SSD1306_SWITCHCAPVCC);
    display.display();
    screens.addScreen(&homeScreen);
    screens.addScreen(&calibScreen);
    screens.begin();

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
    sendcontrolsTask->run();
    battery_task->run();
    yield();
}
