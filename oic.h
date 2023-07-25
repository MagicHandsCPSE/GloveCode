#include <Arduino.h>

template <typename T>
class OnlyIfChange {
    T last = 0;
    const T threshold;
    public:
    OnlyIfChange(T threshold) : threshold(threshold) {}
    bool didChange(T val) {
        if (abs(val - this->last) > this->threshold) {
            this->last = val;
            return true;
        }
        return false;
    }
};
