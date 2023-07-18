#include <SensorFusion.h>
#include <Adafruit_MPU6050.h>

#define TEST_POT 36

void setup() {
    Serial.begin(115200);
}

void loop() {
    Serial.printf("potentiometer reading: %i\n", analogRead(TEST_POT));
}
