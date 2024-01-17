/*Spacehuhn's program ported to ESP-IDF by K1*/
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* ===== compile settings ===== */
#define SNAP_LEN 2324 // max length of each received packet
#define MAX_CH 14     // 1 - 14 channels (1-11 for US, 1-13 for EU and 1-14 for Japan)
#define MAX_X 128     // Adjust according to your display's width (128)
#define MAX_Y 50      // Adjust according to your display's height (64)

#define BUTTON_PIN 5 // button to change the channel (we use SELECT pin in this code)

#define BUTTON_PRESS_TIME 2000 // Milliseconds
// #define DEBOUNCE_TIME 50    // Milliseconds to stabilize the button state // will be hanlded manually and also has the hold button
#define STORAGE_NAMESPACE "packetmonitor32"

#if CONFIG_FREERTOS_UNICORE
#define RUNNING_CORE 0
#else
#define RUNNING_CORE 1
#endif

#include "buffer.h"
#include "setup.h"
#include "esp_vfs_fat.h"

/* ===== run-time variables ===== */
// buffer_init(); // i assume it might be called in the beginning of the main loop of this program

bool useSD = false;
bool buttonPressed = false;
bool buttonEnabled = true;

// uint32_t lastDrawTime;
int64_t lastDrawTime;       // Use int64_t for time in microseconds
// uint32_t lastButtonTime;
int64_t lastButtonTime = 0; // Use int64_t for time in microseconds
uint32_t tmpPacketCounter;  // Temporary packet counter
uint32_t pkts[MAX_X];       // Total packets //here the packets per second will be saved
uint32_t deauths = 0;       // deauth frames per second
// unsigned int ch = 1;        // current 802.11 channel
int32_t ch = 1;             // current 802.11 channel
int rssiSum;                // Sum of RSSI values

static const char* TAG = "my_wifi_sniffer"; // For ESP logging

/* Display and SD_CARD Stuff
u8g2_t u8g2;
// Declare SD_card necessary objects
sdmmc_host_t host;
sdspi_device_config_t slot_config;
sdmmc_card_t* card;
Display and SD_CARD Stuff DONE */

// float get_multiplicator(); // Function to get the multiplicator (adjusted for C)
/*****************************************************************************************************************/
// Assume each character is 6 pixels wide
const int charWidth = 6;

// Spaces are considered 2 pixels wide for padding
const int spaceWidth = 2;

// Define the start position for each item
const int chStartPos = 5;
const int rssiStartPos = chStartPos + (3 * charWidth) + spaceWidth; // 3 characters for channel + padding
const int pktsLabelStartPos = rssiStartPos + (3 * charWidth) + spaceWidth; // 3 characters for RSSI + padding
const int pktsStartPos = pktsLabelStartPos + (5 * charWidth) + spaceWidth; // "Pkts:" label is 5 characters
const int deauthsStartPos = pktsStartPos + (3 * charWidth) + spaceWidth; // 3 characters for packts + padding
const int sdStartPos = (deauthsStartPos + (3 * charWidth) + (2 * spaceWidth) + charWidth); // 3 characters for deauths [ ] + padding
    /*
        Channel starts at position 5.
        RSSI starts at position 25.
        “Pkts:” label starts at position 45.
        Packet counter starts at position 77.
        Deauthorization count starts at position 97.
        SD card status starts at position 125.
    */
/*****************************************************************************************************************/ 

// Declare your coreTask function
static void coreTask(void *pvParameters);

static void wifi_promiscuous(void* buf, wifi_promiscuous_pkt_type_t type);

// Event handler prototype
// static void P_monitor_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
//     return;
// }

/* ===== functions ===== */
//
float get_multiplicator() {
    uint32_t maxVal = 1;
    for (int i = 0; i < MAX_X; i++) {
        if (pkts[i] > maxVal) {
            maxVal = pkts[i];
        }
    }
    if (maxVal > MAX_Y) {
        return (float)MAX_Y / (float)maxVal;
    } else {
        return 1.0f;
    }
}

//
void set_channel(int newChannel) {
    // Check valid range for the new channel
    if (newChannel > MAX_CH || newChannel < 1) {
        newChannel = 1;
    }
    ch = newChannel;

    // Initialize NVS
    nvs_handle my_handle;
    esp_err_t err = nvs_open("channel", NVS_READWRITE, &my_handle);

    if (err == ESP_OK) {
        ESP_LOGI("SET_CHANNEL", "NVS opened for 'channel' namespace");

        // Write the value to NVS
        err = nvs_set_i32(my_handle, "channel", ch);
        if (err == ESP_OK) {
            ESP_LOGI("NVS", "Channel value: %ld written to NVS", ch);
        } else {
            ESP_LOGE("NVS", "Failed to write channel to NVS (error %d)", err);
        }

        // Commit the write to NVS. This is important to ensure the value is saved.
        err = nvs_commit(my_handle);
        if (err == ESP_OK) {
            ESP_LOGI("NVS", "NVS commit successful");
        } else {
            ESP_LOGE("NVS", "Failed to commit to NVS (error %d)", err);
        }

        // Close NVS
        nvs_close(my_handle);
        ESP_LOGI("NVS", "NVS closed");
    } else {
        // handle error
        ESP_LOGE("NVS", "Failed to open NVS (error %d)", err);
    }

    // Actually set the Wi-Fi channel
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous);
    esp_wifi_set_promiscuous(true);
}

// bool setupSD() { // implemented in other parts of the code
//   if (!SD_MMC.begin()) {
//     Serial.println("Card Mount Failed");
//     return false;
//   }

//   uint8_t cardType = SD_MMC.cardType();

//   if (cardType == CARD_NONE) {
//     Serial.println("No SD_MMC card attached");
//     return false;
//   }

//   Serial.print("SD_MMC Card Type: ");
//   if (cardType == CARD_MMC) {
//     Serial.println("MMC");
//   } else if (cardType == CARD_SD) {
//     Serial.println("SDSC");
//   } else if (cardType == CARD_SDHC) {
//     Serial.println("SDHC");
//   } else {
//     Serial.println("UNKNOWN");
//   }

//   uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
//   Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

//   return true;
// }

// Wifi promiscuous callback function with type casting fixed
static void wifi_promiscuous(void* buf, wifi_promiscuous_pkt_type_t type) {
    // if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) {
    //     return; // Skip packet types we're not interested in
    // }
    if (type == WIFI_PKT_MISC) { // wrong packet type
        ESP_LOGV(TAG, "Wrong packet type");
        return;
    }

    const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    const wifi_pkt_rx_ctrl_t ctrl = pkt->rx_ctrl;

    // Check for deauthentication and disassociation frames
    if (type == WIFI_PKT_MGMT && (pkt->payload[0] == 0xA0 || pkt->payload[0] == 0xC0)) {
        deauths++;
    }

    if (ctrl.sig_len > SNAP_LEN) {
        ESP_LOGW(TAG, "Packet too long: %d bytes", ctrl.sig_len);
        return; // Packet is too long, ignore
    }

    uint32_t packetLength = ctrl.sig_len;

    /**/
    // Adjust packet length due to known bug in ESP-IDF 
    // (https://github.com/espressif/esp-idf/issues/886)
    if (type == WIFI_PKT_MGMT) {
        packetLength -= 4;
    }
    /**/

    tmpPacketCounter++;
    rssiSum += ctrl.rssi;

    // Example call to handle the packet, you may need to implement this
    if (useSD) {
        buffer_add_packet(pkt->payload, packetLength);
    }
}

void drawMonitorScreen(u8g2_t *u8g2) { /**************************needs a little bit of tweaking on the last loop****************************************/
    float multiplicator = get_multiplicator();
    char buffer[10]; // Buffer for string conversion

    int len;    
    int rssi;

    if (pkts[MAX_X - 1] > 0) {
        rssi = rssiSum / (int)pkts[MAX_X - 1];
    } else {
        rssi = rssiSum;
    }

    u8g2_ClearBuffer(u8g2);

    // Channel
    sprintf(buffer, "%ld", ch);
    u8g2_SetFont(u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(u8g2, chStartPos, 10, buffer);
    u8g2_DrawStr(u8g2, rssiStartPos - charWidth, 10,"|");

    // RSSI
    sprintf(buffer, "%d", rssi);
    u8g2_DrawStr(u8g2, rssiStartPos, 10, buffer);
    u8g2_DrawStr(u8g2, pktsLabelStartPos - charWidth +2, 10,"|");

    // Static Label for Packets
    u8g2_DrawStr(u8g2, pktsLabelStartPos, 10, "Pkts:");

    // Packets counter
    sprintf(buffer, "%lu", tmpPacketCounter);
    u8g2_DrawStr(u8g2, pktsStartPos, 10, buffer);
    // u8g2_DrawStr(u8g2, deauthsStartPos - charWidth - spaceWidth +2, 10, "[");
    u8g2_DrawStr(u8g2, deauthsStartPos - charWidth - spaceWidth, 10, "[");

    // Deauths
    sprintf(buffer, "%lu", deauths);
    u8g2_DrawStr(u8g2, deauthsStartPos -2, 10, buffer);
    u8g2_DrawStr(u8g2, sdStartPos - (2 * spaceWidth) - charWidth -12, 10, "]");
    u8g2_DrawStr(u8g2, sdStartPos - (2 * spaceWidth) - charWidth -8, 10, "|");

    // SD card status
    sprintf(buffer, "%s", useSD ? "SD" : "");
    u8g2_DrawStr(u8g2, sdStartPos -10, 10, buffer);

    // Draw packets graph
    for (int i = 0; i < MAX_X; i++) {
        len = pkts[i] * multiplicator;
        int corrected_len = len > MAX_Y ? MAX_Y : len;
        u8g2_DrawVLine(u8g2, i, MAX_Y - corrected_len + 14, corrected_len);
        if (i < MAX_X - 1) pkts[i] = pkts[i + 1];
    }
    
    u8g2_SendBuffer(u8g2);
}

// void app_main(void) {
void packet_monitor(void) {
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_LOGW("MY_NVS", "couldn't init... NVS partition was truncated and needs to be erased");
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // System & WiFi
    // tcpip_adapter_init(); // Deprecated in newer versions of ESP-IDF
    ESP_ERROR_CHECK(esp_netif_init()); // Use esp_netif_init for newer versions

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Restore WiFi channel state from NVS
    nvs_handle my_handle;

    // err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &my_handle);
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Opened the NVS; Done\n");
        // Read
        printf("Reading previous channel from NVS ... ");
        err = nvs_get_i32(my_handle, "channel", &ch);
        switch (err) {
            case ESP_OK:
                printf("Done\n");
                printf("Last Channel = %" PRIu32 "\n", ch);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("The value is not initialized yet! .. so we'll initialize it\n");
                err = nvs_set_i32(my_handle, "channel", ch);
                if (err != ESP_OK) ESP_LOGE("MY_NVS", "Could not save channel 1 for the first time in the nvs.");
                else ESP_LOGW("MY_NVS", "saved channel 1 for the first time in the nvs.");
                break;
            default :
                printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
        
    }
    // ESP_ERROR_CHECK(nvs_get_i32(my_handle, "channel", &ch));
    nvs_close(my_handle);

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGW("42SEXY42", "current channel is being set to: %ld", ch); // am aware of this guy :_)
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

    // Setup button as an input
    // gpio_pad_select_gpio(BUTTON_PIN);
    // gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    // gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);

    setup(); // Set up GPIO, display, etc.

    buffer_init(); // SD card; // sdBuffer = Buffer();
    if (sd_is_available) {
        mkdir_if_not_exist("/sdcard/monitor/");
        // buffer_open();
    }

    // Create a task that will be pinned to the second core
    xTaskCreatePinnedToCore(coreTask, "coreTask", 2500, NULL, 1, NULL, 1); // Mimics the code below
    // second core
    // xTaskCreatePinnedToCore(
    // coreTask,               /* Function to implement the task */
    // "coreTask",             /* Name of the task */
    // 2500,                   /* Stack size in words */
    // NULL,                   /* Task input parameter */
    // 0,                      /* Priority of the task */
    // NULL,                   /* Task handle. */
    // RUNNING_CORE);          /* Core where the task should run */

    // Start the Wifi sniffer
    esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous);
    esp_wifi_set_promiscuous(true);
}

void coreTask(void *pvParameters) {
    uint64_t currentTimeUs;

    while (true) {
        currentTimeUs = esp_timer_get_time(); // Get time in microseconds

        /* bit of spaghetti code, have to clean this up later :D */

        // Check button with debounce logic
        // if (gpio_get_level(BUTTON_PIN) == 0) { // Assume active low for the button
        if (gpio_get_level(BUTTON_SELECT_PIN) == 0) { // Assume active low for the button
            if (buttonEnabled) {
                if (!buttonPressed) {
                    // Button is pressed. Start counting time.
                    buttonPressed = true;
                    lastButtonTime = currentTimeUs;
                } else if (currentTimeUs - lastButtonTime >= BUTTON_PRESS_TIME * 1000) {
                    // Button has been pressed for 2 seconds.
                    // useSD = !useSD; this code could achieve a similiar functionality of useSD was handled outside buffer_open and buffer_close
                    if (useSD) {
                        useSD = false;
                        buffer_close();
                        drawMonitorScreen(&u8g2);
                    } else {
                        if (sd_is_available) {
                            buffer_open();
                        }
                        drawMonitorScreen(&u8g2);
                    }
                    buttonPressed = false;
                    buttonEnabled = false;
                }
            }
        } else {
            // If the button state has stabilized, enable button functionality and cycle channels.
            if (buttonPressed) {
                // vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME)); // Debouncing
                // if (gpio_get_level(BUTTON_PIN) == 1) { // Check to confirm button state
                // if (gpio_get_level(BUTTON_SELECT_PIN) == 1) { // Check to confirm button state
                //     setChannel(ch + 1);
                set_channel(ch + 1);
                drawMonitorScreen(&u8g2);
                }
            buttonPressed = false;
            buttonEnabled = true;
        }

        // save buffer to SD
        if (useSD) buffer_save();

        // Update display every second (refresh rate)
        if ((currentTimeUs - lastDrawTime) > 1000000) { // Compare against 1 second in microseconds
            lastDrawTime = currentTimeUs;
            // log_heap_usage();

            pkts[MAX_X - 1] = tmpPacketCounter;
            ESP_LOGV(TAG, "%lu", pkts[MAX_X - 1]);

            drawMonitorScreen(&u8g2);

            tmpPacketCounter = 0;
            deauths = 0;
            rssiSum = 0;

        }
        // TODO:
        // For serial input, consider using UART driver APIs to read serial data in ESP-IDF
        // Serial input
        // if (Serial.available()) {
        //     ch = Serial.readString().toInt();
        //     // if (ch < 1 || ch > 14) ch = 1;
        //     setChannel(ch);
        // }
        // Avoid spinning too fast, let other tasks work
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
