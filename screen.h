#include <LiquidCrystal_PCF8574.h>

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

class Screen {
    int calibrate_option = 0;
    bool onHome = true;
    bt_status status = SCANNING;
    int g_battery = -1;
    int d_battery = -1;
    LiquidCrystal_PCF8574* lcd;
    public:
    Screen(LiquidCrystal_PCF8574* screen): lcd(screen) {}
    void begin() {
        this->switchScreen(true);
    }
    void scroll(int8_t dir) {
        if (this->onHome) return;
        this->calibrate_option = (this->calibrate_option + dir) % num_options;
        this->lcd->setCursor(0, 1);
        this->lcd->print("                "); // Clear the row
        this->lcd->setCursor(0, 1);
        this->lcd->print(calib_options[this->calibrate_option]);
    }
    bool is_home() { return this->onHome; }
    int get_calibrate_option() { return this->calibrate_option; }
    void switchScreen(bool screen) {
        this->onHome = screen;
        this->lcd->clear();
        this->lcd->home();
        if (screen) {
            this->lcd->print("Drone: ???%");
            this->lcd->setCursor(0, 1);
            this->lcd->print("Glove: ???%");
            this->set_d_battery(this->d_battery);
            this->set_g_battery(this->g_battery);
            this->blinky_status();
        } else {
            this->lcd->print("Calibration");
            this->scroll(0);
        }
    }

    void set_d_battery(int b) {
        this->d_battery = b;
        if (this->onHome) {
            this->lcd->setCursor(7, 0);
            if (b > -1) this->lcd->printf("%3i%%", b);
            else this->lcd->print("???%");
        }
    }
    void set_g_battery(int b) {
        this->g_battery = b;
        if (this->onHome) {
            this->lcd->setCursor(7, 1);
            if (b > -1) this->lcd->printf("%3i%%", b);
            else this->lcd->print("???%");
        }
    }

    void set_conn_status(bt_status s) {
        this->status = s;
    }

    void blinky_status() {
        if (!this->onHome) return;
        this->lcd->setCursor(13, 0);
        bool blink = (millis() / 500) & 1;
        switch (this->status) {
            case CONNECTED:
                this->lcd->print("B<>");
                break;
            case ERROR:
                this->lcd->print("B?!");
                break;
            case CONNECTING:
                this->lcd->print(blink ? "B< " : "B  ");
                break;
            case SCANNING:
                this->lcd->print(blink ? "B  " : "   ");
                break;
        }
    }
};
