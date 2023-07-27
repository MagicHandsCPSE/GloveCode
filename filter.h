#ifndef FILTER_H
#define FILTER_H

template <int N>
class LPF {
    uint16_t values[N] = {0};
    uint32_t sum = 0;
    uint8_t pos = 0;
    public:
    LPF() {}
    uint16_t filter(uint16_t val) {
        this->sum += val - this->values[this->pos];
        this->values[this->pos] = val;
        this->pos = (this->pos + 1) % N;
        return this->sum / N;
    }
};

#endif
