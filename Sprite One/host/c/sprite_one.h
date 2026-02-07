/**
 * Sprite One - C Host Library
 * Week 4 Day 23
 * 
 * Simple C library for embedded hosts (ESP32, STM32, etc.)
 * communicating with Sprite One via UART.
 */

#ifndef SPRITE_ONE_H
#define SPRITE_ONE_H

#include <stdint.h>
#include <stdbool.h>

// Protocol constants
#define SPRITE_HEADER       0xAA
#define SPRITE_ACK          0x00

// Command codes
#define CMD_VERSION         0x0F
#define CMD_CLEAR           0x10
#define CMD_PIXEL           0x11
#define CMD_RECT            0x12
#define CMD_TEXT            0x21
#define CMD_FLUSH           0x2F

#define CMD_AI_INFER        0x50
#define CMD_AI_TRAIN        0x51
#define CMD_AI_STATUS       0x52
#define CMD_AI_SAVE         0x53
#define CMD_AI_LOAD         0x54
#define CMD_AI_LIST         0x55

// Response codes
#define RESP_OK             0x00
#define RESP_ERROR          0x01
#define RESP_NOT_FOUND      0x02
#define RESP_BUSY           0x03

// AI Status structure
typedef struct {
    uint8_t state;
    bool model_loaded;
    uint16_t epochs;
    float last_loss;
} sprite_ai_status_t;

// Function pointers for UART operations
// User must implement these for their platform
typedef void (*sprite_uart_write_fn)(uint8_t byte);
typedef uint8_t (*sprite_uart_read_fn)(void);
typedef bool (*sprite_uart_available_fn)(void);

// Library context
typedef struct {
    sprite_uart_write_fn write_byte;
    sprite_uart_read_fn read_byte;
    sprite_uart_available_fn data_available;
    uint32_t timeout_ms;
} sprite_context_t;

// ===== Initialization =====

/**
 * Initialize Sprite One library.
 * 
 * @param ctx Context structure
 * @param write_fn Function to write byte to UART
 * @param read_fn Function to read byte from UART
 * @param available_fn Function to check if data available
 * @param timeout_ms Response timeout in milliseconds
 */
void sprite_init(sprite_context_t* ctx,
                 sprite_uart_write_fn write_fn,
                 sprite_uart_read_fn read_fn,
                 sprite_uart_available_fn available_fn,
                 uint32_t timeout_ms);

// ===== System Commands =====

/**
 * Get firmware version.
 * 
 * @param ctx Context
 * @param major Output: major version
 * @param minor Output: minor version
 * @param patch Output: patch version
 * @return true on success
 */
bool sprite_get_version(sprite_context_t* ctx, 
                        uint8_t* major, uint8_t* minor, uint8_t* patch);

// ===== Graphics Commands =====

/**
 * Clear display.
 * 
 * @param ctx Context
 * @param color Clear color (0 or 1)
 * @return true on success
 */
bool sprite_clear(sprite_context_t* ctx, uint8_t color);

/**
 * Draw pixel.
 * 
 * @param ctx Context
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Pixel color
 * @return true on success
 */
bool sprite_pixel(sprite_context_t* ctx, int16_t x, int16_t y, uint8_t color);

/**
 * Draw filled rectangle.
 * 
 * @param ctx Context
 * @param x X coordinate
 * @param y Y coordinate
 * @param w Width
 * @param h Height
 * @param color Fill color
 * @return true on success
 */
bool sprite_rect(sprite_context_t* ctx, 
                 int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

/**
 * Draw text.
 * 
 * @param ctx Context
 * @param x X coordinate
 * @param y Y coordinate
 * @param text Text string (null-terminated)
 * @param color Text color
 * @return true on success
 */
bool sprite_text(sprite_context_t* ctx, 
                 int16_t x, int16_t y, const char* text, uint8_t color);

/**
 * Flush framebuffer to display.
 * 
 * @param ctx Context
 * @return true on success
 */
bool sprite_flush(sprite_context_t* ctx);

// ===== AI Commands =====

/**
 * Run inference.
 * 
 * @param ctx Context
 * @param input0 First input value
 * @param input1 Second input value
 * @param output Output: inference result
 * @return true on success
 */
bool sprite_ai_infer(sprite_context_t* ctx, 
                     float input0, float input1, float* output);

/**
 * Train AI model.
 * 
 * @param ctx Context
 * @param epochs Number of training epochs
 * @param final_loss Output: final loss value
 * @return true on success
 */
bool sprite_ai_train(sprite_context_t* ctx, 
                     uint8_t epochs, float* final_loss);

/**
 * Get AI engine status.
 * 
 * @param ctx Context
 * @param status Output: status structure
 * @return true on success
 */
bool sprite_ai_status(sprite_context_t* ctx, sprite_ai_status_t* status);

/**
 * Save model to flash.
 * 
 * @param ctx Context
 * @param filename Model filename
 * @return true on success
 */
bool sprite_ai_save(sprite_context_t* ctx, const char* filename);

/**
 * Load model from flash.
 * 
 * @param ctx Context
 * @param filename Model filename
 * @return true on success
 */
bool sprite_ai_load(sprite_context_t* ctx, const char* filename);

#endif // SPRITE_ONE_H
