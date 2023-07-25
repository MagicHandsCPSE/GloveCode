#include <LiquidCrystal_PCF8574.h>

const char* calib_options = {
    "Accelerometer",
    "Thumb OPEN",
    "Thumb CLOSED",
    "Index OPEN",
    "Index CLOSED",
    "Middle OPEN",
    "Middle CLOSED"
}
const size_t num_options = sizeof(calib_options) / sizeof(calib_options[0]);

enum bt_status { SCANNING, CONNECTING, CONNECTED, ERROR }

class Screen {
    bool onHome = true;
    int calibrate_option = 0;
    bt_status status = SCANNING;
    int g_battery = 0;
    int d_battery = -1;
    LiquidCrystal_PCF8574* lcd;
    public:
    Screen(LiquidCrystal_PCF8574* screen): lcd(screen) {}
    void scroll(int8_t dir) {
        if (this->onHome) return;
        this->calibrate_option = (this->calibrate_option + dir) % num_options;
        this->lcd.setCursor(0, 1);
        this->lcd.print(calib_options[this->calibrate_option]);
    }

    calibrate_option() { return this->calibrate_option; }
    switchScreen() {
        this->onHome = !this->onHome;
        this->lcd.clear();
        this->lcd.home();
        if (this->onHome) {
            this->lcd.print("Drone: ???%");
            this->lcd.setCursor(0, 1);
            this->lcd.print("Glove: ???%");
            this->set_d_battery(this->d_battery);
            this->set_g_battery(this->g_battery);
            this->blinky_status();
        } else {
            this->lcd.print("Calibration");
            this->scroll(0);
        }
    }

    void set_d_battery(int b) {
        this->d_battery = b;
        if (this->onHome) {
            this->lcd.setCursor(7, 0);
            if (b > -1) this->lcd.printf("%3i%%", b);
            else this->lcd.print("???%");
        }
    }
    void set_g_battery(int b) {
        this->g_battery = b;
        if (this->onHome) {
            this->lcd.setCursor(7, 1);
            if (b > -1) this->lcd.printf("%3i%%", b);
            else this->lcd.print("???%");
        }
    }

    void set_conn_status(bt_status s) {
        this->bt_status = s;
    }

    void blinky_status() {
        if (!this->onHome) return;
        this->lcd.setCursor(13, 0);
        switch (this->bt_status) {
            case CONNECTED:
                this->lcd.print("B<>");
                break;
            case ERROR:
                this->lcd.print("B?!");
                break;
            case CONNECTING:
                this->lcd.printf("B%c ", ((millis() % 1000) < 500) ? '<' : ' ');
                break;
            case SCANNING:
                this->lcd.printf("%c  ", ((millis() % 1000) < 500) ? '<' : ' ');
                break;
        }
    }
};
