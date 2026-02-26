/**
 * Sprite One - Core Definitions
 * 
 * The heart of the Sprite Microelectronics accelerator.
 * Open source hardware for everyone.
 * 
 * License: MIT
 */

#ifndef SPRITE_CORE_H
#define SPRITE_CORE_H

#include <stdint.h>

// Version
#define SPRITE_VERSION_MAJOR 2
#define SPRITE_VERSION_MINOR 1
#define SPRITE_VERSION_PATCH 0

// Protocol Constants
#define SPRITE_HEADER       0xAA
#define SPRITE_MAX_PAYLOAD  255

// Response codes
#define SPRITE_ACK          0x00
#define SPRITE_NAK          0x01
#define SPRITE_BUSY         0x02
#define SPRITE_DATA         0xFF

// Command Definitions

// System commands (0x00 - 0x0F)
#define CMD_NOP             0x00
#define CMD_INIT            0x01
#define CMD_RESET           0x02
#define CMD_VERSION         0x0F

// Graphics commands (0x10 - 0x3F)
#define CMD_CLEAR           0x10
#define CMD_PIXEL           0x11
#define CMD_RECT            0x12
#define CMD_RECT_OUTLINE    0x13
#define CMD_LINE            0x14
#define CMD_CIRCLE          0x15
#define CMD_SPRITE          0x20
#define CMD_TEXT            0x21
#define CMD_FLUSH           0x2F

// Asset commands (0x40 - 0x4F)
#define CMD_LOAD_SPRITE     0x40
#define CMD_LOAD_FONT       0x41
#define CMD_STORE_FLASH     0x42

// AI commands (0x50 - 0x5F)
#define CMD_AI_INFER        0x50  // Run inference on loaded model
#define CMD_AI_TRAIN        0x51  // Train model with provided data
#define CMD_AI_GET_RESULT   0x52  // Get inference result
#define CMD_AI_SAVE         0x53  // Save model to flash
#define CMD_AI_LOAD         0x54  // Load model from flash
#define CMD_AI_LIST         0x55  // List saved models
#define CMD_AI_DELETE       0x56  // Delete a saved model
#define CMD_AI_STATUS       0x57  // Get AI engine status
#define CMD_AI_CONFIG       0x58  // Configure AI parameters

// Display Driver IDs
#define DISPLAY_ILI9341     0x01
#define DISPLAY_ST7789      0x02
#define DISPLAY_SSD1306     0x03
#define DISPLAY_ST7735      0x04
#define DISPLAY_ILI9488     0x05
#define DISPLAY_GENERIC     0x10

// Color Helpers (RGB565)
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_CYAN          0x07FF
#define COLOR_MAGENTA       0xF81F

// Structures

// Command packet (incoming from host)
typedef struct {
    uint8_t command;
    uint8_t length;
    uint8_t payload[SPRITE_MAX_PAYLOAD];
    uint8_t checksum;
} SpritePacket;

// Sprite asset in memory
typedef struct {
    uint8_t id;
    uint16_t width;
    uint16_t height;
    uint16_t* data;     // RGB565 pixel data
} SpriteAsset;

// Display configuration
typedef struct {
    uint8_t driver_id;
    uint16_t width;
    uint16_t height;
    uint8_t rotation;
} DisplayConfig;

// Function Declarations (implemented in firmware)

// Protocol
uint8_t sprite_calc_checksum(const uint8_t* data, uint8_t len);
int sprite_parse_packet(const uint8_t* buffer, uint8_t len, SpritePacket* packet);

// Graphics engine
void sprite_init_display(uint8_t driver, uint16_t w, uint16_t h, uint8_t rot);
void sprite_clear(uint16_t color);
void sprite_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void sprite_draw_sprite(uint8_t id, int16_t x, int16_t y);
void sprite_draw_text(int16_t x, int16_t y, uint16_t color, const char* text);
void sprite_flush(void);

// Asset management
int sprite_load_asset(uint8_t id, uint16_t w, uint16_t h, const uint16_t* data);

#endif // SPRITE_CORE_H
