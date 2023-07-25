#include <Arduino.h>

template <int8_t T, int D>
class OIC {
    uint8_t last = 0;
    unsigned long lastMove = 0;
    public:
    OIC() {}
    bool didChange(uint8_t val) {
        if (abs(val - this->last) > T) {
            this->last = val;
            this->lastMove = millis();
            return true;
        }
        return millis() - this->lastMove < D;
    }
};
