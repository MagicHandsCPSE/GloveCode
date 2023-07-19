#include <Arduino.h>
#include <vector>

typedef unsigned long ul;
typedef void (*voidvoid)(void*);

class Task {
    public:
    const char* name;
    void* param;
    const voidvoid handler;
    ul last;
    ul interval;
    bool running;
    Task(const char* name, ul interval, void* arg, voidvoid fun)
    : name(name),
      interval(interval),
      handler(fun),
      running(true),
      param(arg) {
    }
    void run(bool debug = false) {
        if (!this->running) {
            if (debug) Serial.printf("%s task is stopped\n", this->name);
            return;
        }
        ul now = millis();
        if (now - this->last > this->interval) {
            if (debug) Serial.printf("%s task is running\n", this->name);
            this->last = now;
            this->handler(this->param);
        } else {
            if (debug) Serial.printf("%s task is waiting\n", this->name);
        }
    }
};

class TaskManager {
    std::vector<Task*> tasks;
    public:
    TaskManager() {}
    void add(Task* task) {
        this->tasks.push_back(task);
    }
    void run_all(bool debug = false) {
        for (Task* t : this->tasks) t->run(debug);
    }
};
