/*Spacehuhn's program ported to ESP-IDF by K1*/
#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"


// Define variables
static uint8_t* bufA;
static uint8_t* bufB;
static size_t bufSizeA = 0;
static size_t bufSizeB = 0;


static bool writing = false; // writing to bufA or bufB
static bool useA = true; // acceppting writes to buffer
static bool saving = false; // currently saving onto the SD card

static FILE* file;
static const char* TAG = "buffer";
char fileName[128] = "/sdcard/monitor/%dpcap.txt";

// Function prototypes
void buffer_write(const uint8_t* data, size_t len);
void buffer_write_uint32(uint32_t value);
void buffer_write_uint16(uint16_t value);
void buffer_write_int32(int32_t value);

/*********************************************/

void buffer_init() {
    bufA = (uint8_t*)malloc(BUF_SIZE);
    bufB = (uint8_t*)malloc(BUF_SIZE);
    if (!bufA || !bufB) {
        ESP_LOGE(TAG, "Failed to allocate memory for buffers");
    }
}

void buffer_open() {
    // Ensure the filesystem has been initialized before this
    // char fileName[128];
    int i = 0;
    
    // Create a new file with a unique name
    do {
        sprintf(fileName, "/sdcard/monitor/%dpcap.txt", i);
        struct stat st;
        if (stat(fileName, &st) == 0) {
            ++i;
        } else {
            break;
        }
    } while (1);
    
    ESP_LOGI(TAG, "Opening file: %s", fileName);

    // 'w' to create an empty file for writing.
    // The file will be created if it doesn't exist.
    file = fopen(fileName, "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    fclose(file);

    bufSizeA = 0;
    bufSizeB = 0;
    writing = true;

    buffer_write_uint32((uint32_t)0xa1b2c3d4); // magic number
    buffer_write_uint16((uint16_t)2); // major version number
    buffer_write_uint16((uint16_t)4); // minor version number
    buffer_write_int32((int32_t)0); // GMT to local correction
    buffer_write_uint32((uint32_t)0); // accuracy of timestamps
    buffer_write_uint32((uint32_t)SNAP_LEN); // max length of captured packets, in octets
    buffer_write_uint32((uint32_t)105); // data link type

    useSD = true;
}

// void buffer_write_uint32(uint32_t value) {
//     if (bufSizeA + 4 <= BUF_SIZE) {
//         memcpy(&bufA[bufSizeA], &value, sizeof(uint32_t));
//         bufSizeA += 4;
//     } else {
//         ESP_LOGW(TAG, "Buffer A overflow");
//     }
//     // Implement buffer switching and saving logic similar to what you have in Buffer::write
// }

void buffer_close() {
    if (!writing) return; // log maybe later
    buffer_force_save();
    writing = false;
    ESP_LOGI(TAG, "File closed");
}

// void buffer_force_save() {
//     if (writing) {
//         // Actual saving code from the buffer to the SD card goes here
//         // Ensure to open the file in binary append mode ("ab" mode in fopen)
//         // Also check available space and open correct buffer
//     }
// }

// Rest of the Buffer methods converted into C functions...

// Make sure to free the buffers when no longer needed
void buffer_deinit() {
    free(bufA);
    free(bufB);
}

// void buffer_add_packet(uint8_t* packet, uint32_t len) {
void buffer_add_packet(const uint8_t* packet, uint32_t len) {
    // buffer is full -> drop packet
    if ((useA && (bufSizeA + len >= BUF_SIZE && bufSizeB > 0)) || (!useA && (bufSizeB + len >= BUF_SIZE && bufSizeA > 0))) {
        // ESP_LOGI(TAG, ";");
        return;
    }

    /* ################### Below is the implementation of the commented code: ###################

    if(useA && bufSizeA + len + 16 >= BUF_SIZE && bufSizeB == 0){
        useA = false;
        Serial.println("\nswitched to buffer B");
    }
    else if(!useA && bufSizeB + len + 16 >= BUF_SIZE && bufSizeA == 0){
        useA = true;
        Serial.println("\nswitched to buffer A");
    } 

    */
    if ((useA && (bufSizeA + len + 16 >= BUF_SIZE && bufSizeB == 0)) || (!useA && (bufSizeB + len + 16 >= BUF_SIZE && bufSizeA == 0))) {
        useA = !useA;
        ESP_LOGI(TAG, "\nswitched to buffer %s", useA ? "A" : "B");
    }

    uint64_t microSeconds = (uint64_t)esp_timer_get_time(); // Get time in microseconds // e.g. 45200400 => 45s 200ms 400us
    uint32_t seconds = (uint32_t)(microSeconds / 1000000); // e.g. 45200400/1000/1000 = 45200 / 1000 = 45s
    microSeconds -= (uint64_t)seconds * 1000000; // Get the remainder microseconds // e.g. 45200400 - 45*1000*1000 = 45200400 - 45000000 = 400us (because we only need the offset)

    buffer_write_uint32(seconds); // ts_sec
    buffer_write_uint32((uint32_t)microSeconds); // ts_usec
    buffer_write_uint32(len); // incl_len
    buffer_write_uint32(len); // orig_len

    buffer_write(packet, len); // packet payload
}

void buffer_write_uint32(uint32_t value) {
    // Handle endianess and buffer switching here
    uint8_t buf[4];
    buf[0] = value;
    buf[1] = value >> 8;
    buf[2] = value >> 16;
    buf[3] = value >> 24;
    buffer_write(buf, 4);
}

void buffer_write_uint16(uint16_t value) {
    // Handle endianess and buffer switching here
    uint8_t buf[2];
    buf[0] = value;
    buf[1] = value >> 8;
    buffer_write(buf, 2);
}

void buffer_write_int32(int32_t value) {
    // Handle endianess and buffer switching here
    uint8_t buf[4];
    buf[0] = value;
    buf[1] = value >> 8;
    buf[2] = value >> 16;
    buf[3] = value >> 24;
    buffer_write(buf, 4);
}

void buffer_write(const uint8_t* data, size_t len) {
    // Write to one of the buffers based on 'useA' and manage the size
    if (!writing) return; // Ensures the function isn't called simultaneously on different cores log maybe later

    if (useA) {
        if (bufSizeA + len <= BUF_SIZE) {
            memcpy(&bufA[bufSizeA], data, len);
            bufSizeA += len;
        } else {
            ESP_LOGW(TAG, "Buffer A overflow");
            // Handle the buffer overflow case here
        }
    } else {
        if (bufSizeB + len <= BUF_SIZE) {
            memcpy(&bufB[bufSizeB], data, len);
            bufSizeB += len;
        } else {
            ESP_LOGW(TAG, "Buffer B overflow");
            // Handle the buffer overflow case here
        }
    }
}

void buffer_save() {
    if(saving) return; // Ensures the function isn't called simultaneously on different cores log maybe later

    // buffers are already emptied, therefor saving is unecessary
    if((useA && bufSizeB == 0) || (!useA && bufSizeA == 0)) {
        // Nothing to save
        // ESP_LOGI(TAG, "useA: %s, bufA %u, bufB %u\n",useA ? "true" : "false", bufSizeA, bufSizeB); // for debug porpuses
        return;
    }

    ESP_LOGI(TAG, "Saving file");
    int64_t startTime = esp_timer_get_time();
    int64_t finishTime;

    FILE* f = fopen(fileName, "a");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file '%s' for appending", fileName);
        // writing = false;
        useSD = false;
        return;
    }

    saving = true;
    size_t len;

    if(useA){
        fwrite(bufB, 1, bufSizeB, f);
        len = bufSizeB;
        bufSizeB = 0;
    } else {
        fwrite(bufA, 1, bufSizeA, f);
        len = bufSizeA;
        bufSizeA = 0;
    }

    fclose(f);

    finishTime = (esp_timer_get_time() - startTime) / 1000; // converting to millis()
    
    ESP_LOGI(TAG, "%u bytes written for %lld ms", len, finishTime);
    saving = false;
}

void buffer_force_save() {
    size_t len = bufSizeA + bufSizeB;
    if(len == 0) return;

    FILE* f = fopen(fileName, "a");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file '%s' for appending", fileName);
        // writing = false;
        useSD = false;
        return;
    }

    saving = true;
    writing = false;

    if(useA){
        if(bufSizeB > 0){
            fwrite(bufB, 1, bufSizeB, f);
            bufSizeB = 0;
        }
        if(bufSizeA > 0){
            fwrite(bufA, 1, bufSizeA, f);
            bufSizeA = 0;
        }
    } else {
        if(bufSizeA > 0){
            fwrite(bufA, 1, bufSizeA, f);
            bufSizeA = 0;
        }
        if(bufSizeB > 0){
            fwrite(bufB, 1, bufSizeB, f);
            bufSizeB = 0;
        }
    }

    fclose(f);

    ESP_LOGI(TAG, "Saved %u bytes", len);
    saving = false;
    writing = true;
}
