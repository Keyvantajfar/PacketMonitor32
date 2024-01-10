#include "setup.h"
#include "icons.h"

#include <sys/stat.h>

bool sd_is_available = false;
int brightness = 30;

u8g2_t u8g2;
// Declare SD_card necessary objects
sdmmc_host_t host;
sdspi_device_config_t slot_config;
sdmmc_card_t* card;

// Function to initialize the SPI bus and the SD_card:
esp_err_t init_sd_card(sdmmc_host_t* out_host, sdspi_device_config_t* out_slot_config) {
    assert(out_host != NULL);
    assert(out_slot_config != NULL);

    esp_err_t ret;

    ESP_LOGI("SD", "Initializing SD card");
    ESP_LOGI("SD", "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE("SD", "Failed to initialize bus.");
        // sd_is_available = false;
        ESP_LOGE("SD", "Default Brightness Value will be used.");
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    *out_host = host;
    *out_slot_config = slot_config;
    
    return ESP_OK;
}

esp_err_t mount_filesystem(sdmmc_host_t* host, sdspi_device_config_t* slot_config, sdmmc_card_t** out_card) {
    assert(host != NULL);
    assert(slot_config != NULL);
    assert(out_card != NULL);
    
    const char mount_point[] = MOUNT_POINT;
    esp_err_t ret;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    ESP_LOGI("SD", "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, host, slot_config, &mount_config, out_card);

    if (ret == ESP_OK) {
        ESP_LOGI("SD", "Filesystem mounted");
        sdmmc_card_print_info(stdout, *out_card);
    } else {
        if (ret == ESP_FAIL) {
            ESP_LOGE("SD", "Failed to mount filesystem.");
        } else {
            ESP_LOGE("SD", "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
    }

    return ret;
}

// Function to check SD_card's availability
bool SD_is_available() {
    if (mount_filesystem(&host, &slot_config, &card) == ESP_OK) {
        sd_is_available = true;
        // unmount_sdcard(); // Unmount right after checking to avoid interfering in case you need to mount elsewhere
        return true;
    } else {
        return false;
    }
}

// Function to check if a directory exists and create it if it doesn't
esp_err_t mkdir_if_not_exist(const char* dirpath) {
    struct stat st;
    if (stat(dirpath, &st) != 0) {
        // LOG Directory does not exist, let's create it
        ESP_LOGW("dir", "Directory does not exist, Creating directory: %s", dirpath);
        if (mkdir(dirpath, 0777) != 0) {
            ESP_LOGE("dir", "Failed to create directory: %s", dirpath);
            return ESP_FAIL;
        }
        ESP_LOGI("dir", "Directory '%s' created successfully", dirpath);
        return ESP_OK;
    } // LOGI created directory successfully
    ESP_LOGI("dir", "Directory '%s' Exists Already!", dirpath);
    return ESP_OK;
}

// Function to splash the loading screen while the computer is booting
void drawLoadingScreen(u8g2_t *u8g2) {
    const uint8_t yPosition = 57;
    const uint8_t pixels[] = {87, 91, 95, 99, 103, 107};  // X positions for pixels

    u8g2_ClearBuffer(u8g2);  // clear the internal memory

    u8g2_SetBitmapMode(u8g2, 1);
    u8g2_SetFontMode(u8g2, 1);
    
    // Set fonts
    // u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    u8g2_SetFont(u8g2, u8g2_font_helvB08_tr);
    u8g2_DrawStr(u8g2, 17, 30, "PacketMonitor32");
    // u8g2_SetFont(u8g2, u8g2_font_helvB08_tr);
    u8g2_DrawStr(u8g2, 20, 42, "By @Spacehuhn");
    // u8g2_SetFont(u8g2, u8g2_font_helvB08_tr);
    u8g2_DrawStr(u8g2, 30, 56, "IDF Port By K1");

    // Draw XBM images
    u8g2_DrawXBM(u8g2, 2, 4, 29, 14, image_FaceCharging_29x14_bits);
    u8g2_DrawXBM(u8g2, 115, 2, 10, 10, image_ir_10px_bits);
    u8g2_DrawXBM(u8g2, 106, 4, 7, 8, image_Unlock_7x8_bits);
    u8g2_DrawXBM(u8g2, 92, -1, 16, 16, image_Battery_16x16_bits);

    u8g2_SendBuffer(u8g2);  // transfer internal memory to the display

    // Initialize the SD card
    esp_err_t ret = init_sd_card(&host, &slot_config);
    if (ret == ESP_OK && SD_is_available()) {
        ESP_LOGI("setup", "Initiallized on SD card successfully and SD is available");
        // Attempt to read the brightness from the SD card
        // if (!read_brightness_from_sd(&brightness)) {
        //     // SD card not available, or reading failed -- use default and save the value
        //     ESP_LOGI(TAG, "Using default brightness value: %d", brightness);
        // } else {
        //     // Successfully read brightness value from SD card
        //     ESP_LOGI(TAG, "Brightness set to configured value: %d", brightness);
        // }
        // Attempt to read the cpu frequency from the SD card
        // read_settings(&brightness, &cpuFrequency);
    }

    // Set the display brightness to a value between 0 (lowest) and 255 (highest) and Set CPU_Speed.
    u8g2_SetContrast(u8g2, brightness);
    // Used esp_pm_config_t as per the new version of the ESP-IDF
    // esp_pm_config_t pm_config = {
    //     .max_freq_mhz = cpuFrequency,
    //     .min_freq_mhz = cpuFrequency,
    //     // .light_sleep_enable = (cpuFrequency < 240) ? true : false // Or false, depending on your needs
    // };
    // if (esp_pm_configure(&pm_config) != ESP_OK) {
    //     ESP_LOGE("SD", "Failed to adjust CPU speed at %i MHz", cpuFrequency);
    // } else {
    //     ESP_LOGI("SD", "Power settings set, CPU frequency is %i MHz", cpuFrequency);
    // }

    // Loop over each pixel in the loading animation
    for (int i = 0; i < sizeof(pixels); i++) {
        // Draw the current pixel
        u8g2_DrawPixel(u8g2, pixels[i], yPosition);
        u8g2_SendBuffer(u8g2);
        
        // Delay for 0.2 second before drawing the next pixel
        // vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// GPIO configuration for buttons, I2C initialization, etc.
void setup(void) {

    // ***********U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0); equivalent in this block************
    // Initialize your u8g2 display setup here
    // For ESP32, assign I2C pins and port
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.i2c.sda = PIN_SDA; // Set your SDA pin number
    hal.bus.i2c.scl = PIN_SCL; // Set your SCL pin number
    u8g2_esp32_hal_init(hal);

    // Initialization for SSD1306 128x64 display (noname version) using the I2C HW interface
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2,                  // u8g2 structure pointer
        U8G2_R0,                // rotation
        // u8x8_byte_hw_i2c,       // byte communication callback
        u8g2_esp32_i2c_byte_cb, // Using HW instead of SW
        // u8x8_gpio_and_delay_esp32 // GPIO and delay callback
        u8g2_esp32_gpio_and_delay_cb
    );
    
    // Initialize the u8g2 display
    u8g2_InitDisplay(&u8g2); // u8g2_Begin(&u8g2) // u8g2->begin();
    u8g2_SetPowerSave(&u8g2, 0);

    // set the color to white
    // u8g2_SetColorIndex(&u8g2, 1);
    // Bitmap mode on
    u8g2_SetBitmapMode(&u8g2, 1);

    // Your display is now initialized and ready for use.

    /* ... Existing GPIO setup ... */
    
    // Define pins for buttons (Setup GPIOs)
    // INPUT_PULLUP means the button is HIGH when not pressed, and LOW when pressed
    // since it´s connected between some pin and GND
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_UP_PIN) | (1ULL << BUTTON_SELECT_PIN) | (1ULL << BUTTON_DOWN_PIN) | (1ULL << BUTTON_BACK_PIN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Button pin interrupt callback setup, if using interrupts
    // gpio_install_isr_service(0);
    // gpio_isr_handler_add(BUTTON_UP_PIN, button_isr_handler, (void*) BUTTON_UP_PIN);
    // repeat for other buttons...

    // Set the display brightness to a value between 0 (lowest) and 255 (highest) **
    u8g2_SetContrast(&u8g2, 50); // this is deprecated, it is implemented in drawLoadingScreen now **
    
    // testing screen pixels
    // for (int i = 0; i < 4; i++) {
    //     flashScreenTest(&u8g2);
    // }


    // Draw and display the loading screen
    drawLoadingScreen(&u8g2);
}