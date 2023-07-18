#include <SensorFusion.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>

Adafruit_MPU6050 imu;
SF sensor_fusion;

#define TEST_POT 36

void setup() {
    Serial.begin(115200);
    Serial.print("Connecting IMU...         ");
    if (!imu.begin()) {
       Serial.println("[ FAIL ]\nFATAL ERROR: IMU disconnected!!");
       ESP.restart();
    } else
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
}

void loop() {
    int pot_val = analogRead(TEST_POT);
    sensors_event_t a, g, trash;
    imu.getEvent(&a, &g, &trash);
    (void)trash;
    sensor_fusion.MahonyUpdate(
        g.gyro.x, g.gyro.y, g.gyro.z,
        a.acceleration.x, a.acceleration.y, a.acceleration.z,
        sensor_fusion.deltatUpdate());
    Serial.println("Pot\tAX\tAY\tAZ\tGX\tGY\tGZ\tpitch\troll\tyaw");
    Serial.printf(
        "%i\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\t%g\n",
        pot_val,
        a.acceleration.x, a.acceleration.y, a.acceleration.z,
        g.gyro.x, g.gyro.y, g.gyro.z,
        sensor_fusion.getPitch(), sensor_fusion.getRoll(), sensor_fusion.getYaw());
}
