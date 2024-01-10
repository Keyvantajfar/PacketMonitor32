#ifndef SETUP_H
#define SETUP_H


#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define MOUNT_POINT "/sdcard"
#define MAX_LINE_LENGTH 128
// Defining GPIOs
#define BUTTON_UP_PIN 12 // pin for UP(UPPER) button
#define BUTTON_SELECT_PIN 14 // pin for SELECT button
#define BUTTON_DOWN_PIN 27 // pin for DOWN(BUTTOM) button
#define BUTTON_BACK_PIN 26 // pin for BACK button
#define PIN_SDA 21 // Make sure defined to the correct GPIOs connected to your OLED display’s data line.
#define PIN_SCL 22 // Make sure defined to the correct GPIOs connected to your OLED display’s clock line.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO  CONFIG_EXAMPLE_PIN_MISO
#define PIN_NUM_MOSI  CONFIG_EXAMPLE_PIN_MOSI
#define PIN_NUM_CLK   CONFIG_EXAMPLE_PIN_CLK
#define PIN_NUM_CS    CONFIG_EXAMPLE_PIN_CS

extern bool sd_is_available;

extern u8g2_t u8g2;
// Declaring SD_card necessary objects
extern sdmmc_host_t host;
extern sdspi_device_config_t slot_config;
extern sdmmc_card_t* card;

void setup(void);
esp_err_t mkdir_if_not_exist(const char* dirpath);

#endif // SETUP_H