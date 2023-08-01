#ifndef SCREEN_H
#define SCREEN_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include "calibrate.h"

const char* calib_options[] = {
    "Accelerometer",
    "Thumb OPEN",
    "Thumb CLOSED",
    "Index OPEN",
    "Index CLOSED",
    "Middle OPEN",
    "Middle CLOSED"
};
const size_t num_options = sizeof(calib_options) / sizeof(calib_options[0]);

enum bt_status { SCANNING, CONNECTING, CONNECTED, ERROR };

class ScreenManager;

class Screen {
    public:
    Adafruit_SSD1306* screen;
    ScreenManager controller;
    Screen(ScreenManager* manager): screen(manager->screen), controller(manager) {}
    virtual void draw() {}
    virtual void button() {}
    virtual void up() {}
    virtual void down() {}
};

class HomeScreen: public Screen {
    public:
    bt_status status = SCANNING;
    int g_battery = -1;
    int d_battery = -1;
    void draw() override {
        bool blink = (millis() / 500) & 1;
        int dots = (millis() / 200) & 3;
        Adafruit_SSD1306* d = this->screen;
        d->print("glove: ");
        if (this->g_battery == -1) d->print("???%");
        else if (this->g_battery == -2) d->print("---%");
        else d->printf("%3i%%", this->g_battery);
        if (this->g_battery > 0 && this->g_battery < 10) {
            d->setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
            d->print(blink ? "!!!" : "LOW");
            d->setTextColor(SSD1306_WHITE);
        }
        d->println();
        d->print("drone: ");
        if (this->d_battery == -1) d->print("???%");
        else if (this->d_battery == -2) d->print("---%");
        else d->printf("%3i%%", this->d_battery);
        if (this->d_battery > 0 && this->d_battery < 10) {
            d->setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
            d->print(blink ? "\x10\x10\x10" : "LOW");
            d->setTextColor(SSD1306_WHITE);
        }
        d->println();
        switch (this->status) {
            case CONNECTED:
                d->print("connected");
                break;
            case ERROR:
                if (blink) d->setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Draw 'inverse' text
                d->print("connection error");
                d->setTextColor(SSD1306_WHITE);
                break;
            case SCANNING:
                d->print("scanning");
                for (int i = 0; i < dots; i++) d->print(".");
                break;
            case CONNECTING:
                d->print("connecting");
                for (int i = 0; i < dots; i++) d->print(".");
                break;
        }
        d->println();
        this->drawMore();
    }
    virtual void drawMore() {}
};

class CalibScreen: public Screen {
    public:
    int calibrate_option = 0;
    int scroll_amount = 0;
    void scroll(int8_t dir) {
        if (this->onHome) return;
        if ((dir < 0 && this->calibrate_option > -dir - 1) || (dir > 0 && this->calibrate_option < num_options - dir)) this->calibrate_option += dir;
        if (this->calibrate_option - this->scroll_amount > 2) this->scroll_amount = this->calibrate_option - 2;
        if (this->scroll_amount - this->calibrate_option > 0) this->scroll_amount = this->calibrate_option;
    }
    void up() override { this->scroll(-1); }
    void down() override { this->scroll(1); }
    void draw() override {
        Adafruit_SSD1306* d = this->screen;
        d->println("CALIBRATE:");
        for (int i = 0, j = this->scroll_amount; i < 3; i++, j++) {
            d->printf("%c%-13s %4i%c\n",
                        j == this->calibrate_option ? '\x1a' : ' ',
                        calib_options[j],
                        get_calibrate(j),
                        i == 0 && j != 0 ? '\x18' : (i == 2 && j != num_options - 1 ? '\x19' : ' '));
        }
    }
};

class ScreenManager {
    public:
    std::vector<Screen*> screens;
    size_t index = 0;
    Adafruit_SSD1306* display;
    ScreenManager(Adafruit_SSD1306* screen): display(screen) {}
    void begin() {
        this->display->cp437(true);
    }
    void addScreen(Screen* screen) {
        this->screens.push_back(screen);
    }
    void nextScreen() {
        this->index = (this->index + 1) % this->screens.size();
    }
    void prevScreen() {
        this->index = (this->index + this->screens.size() - 1) % this->screens.size();
    }
    Screen* activeScreen() { return this->screens[this->index]; }
    void button() { this->activeScreen()->button(); }
    void up() { this->activeScreen()->up(); }
    void down() { this->activeScreen()->down(); }
    void draw() {
        Adafruit_SSD1306* d = this->display;
        d->setTextSize(1);
        d->setCursor(0, 0);
        d->setTextColor(SSD1306_WHITE);
        d->clearDisplay();
        this->activeScreen()->draw();
        d->display();
    }
}

#endif
