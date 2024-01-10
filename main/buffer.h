/*Spacehuhn's program ported to ESP-IDF by K1*/
#ifndef BUFFER_h
#define BUFFER_h

#define BUF_SIZE 24 * 1024
#define SNAP_LEN 2324 // max len of each recieved packet

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

extern bool useSD;

void buffer_init();
void buffer_open();
void buffer_close();
// void buffer_add_packet(uint8_t* packet, uint32_t len);
void buffer_add_packet(const uint8_t* packet, uint32_t len);
void buffer_save();
void buffer_force_save();

void buffer_deinit();

#endif