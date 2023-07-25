template <int N>
class LPF {
    int values[N] = {0};
    long sum = 0;
    unsigned int pos = 0;
    public:
    LPF() {}
    int filter(int val) {
        this->sum += val - this->values[this->pos];
        this->values[this->pos] = val;
        this->pos = (this->pos + 1) % N;
        return this->sum / N;
    }
};
