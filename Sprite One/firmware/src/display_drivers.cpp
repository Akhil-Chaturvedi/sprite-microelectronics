/**
 * Sprite One - Display Drivers
 * 
 * Multi-display support for common SPI displays.
 * Designed to be as universal as possible.
 * 
 * Supported displays:
 * - ILI9341 (320x240)
 * - ST7789 (240x240, 240x320)
 * - ST7735 (128x160)
 * - SSD1306 (128x64 OLED)
 * - ILI9488 (480x320)
 * 
 * License: MIT
 */

#include <Arduino.h>
#include <SPI.h>
#include "sprite_core.h"

// External pin definitions from main.cpp
extern const int PIN_DISPLAY_DC;
extern const int PIN_DISPLAY_RST;
extern const int PIN_DISPLAY_CS;

// Current display driver
static uint8_t current_driver = 0;
static uint16_t display_width = 0;
static uint16_t display_height = 0;

// ============================================================================
// SPI Communication Helpers
// ============================================================================

static inline void dc_command() {
    digitalWrite(PIN_DISPLAY_DC, LOW);
}

static inline void dc_data() {
    digitalWrite(PIN_DISPLAY_DC, HIGH);
}

static inline void cs_select() {
    digitalWrite(PIN_DISPLAY_CS, LOW);
}

static inline void cs_deselect() {
    digitalWrite(PIN_DISPLAY_CS, HIGH);
}

static void write_command(uint8_t cmd) {
    dc_command();
    cs_select();
    SPI1.transfer(cmd);
    cs_deselect();
}

static void write_data(uint8_t data) {
    dc_data();
    cs_select();
    SPI1.transfer(data);
    cs_deselect();
}

static void write_data16(uint16_t data) {
    dc_data();
    cs_select();
    SPI1.transfer16(data);
    cs_deselect();
}

static void write_command_data(uint8_t cmd, const uint8_t* data, size_t len) {
    write_command(cmd);
    dc_data();
    cs_select();
    for (size_t i = 0; i < len; i++) {
        SPI1.transfer(data[i]);
    }
    cs_deselect();
}

// ============================================================================
// ILI9341 Driver (320x240)
// ============================================================================

static void ili9341_init(uint8_t rotation) {
    // Hardware reset
    digitalWrite(PIN_DISPLAY_RST, LOW);
    delay(10);
    digitalWrite(PIN_DISPLAY_RST, HIGH);
    delay(120);
    
    write_command(0x01);  // Software reset
    delay(150);
    
    write_command(0x11);  // Sleep out
    delay(500);
    
    // Power control
    uint8_t pwr1[] = {0x23};
    write_command_data(0xC0, pwr1, 1);
    uint8_t pwr2[] = {0x10};
    write_command_data(0xC1, pwr2, 1);
    
    // VCOM control
    uint8_t vcom1[] = {0x3E, 0x28};
    write_command_data(0xC5, vcom1, 2);
    uint8_t vcom2[] = {0x86};
    write_command_data(0xC7, vcom2, 1);
    
    // Memory access control (rotation)
    uint8_t madctl;
    switch (rotation) {
        case 1: madctl = 0x68; break;  // 90°
        case 2: madctl = 0xC8; break;  // 180°
        case 3: madctl = 0xA8; break;  // 270°
        default: madctl = 0x48; break; // 0°
    }
    uint8_t mac[] = {madctl};
    write_command_data(0x36, mac, 1);
    
    // Pixel format: 16-bit (RGB565)
    uint8_t pix[] = {0x55};
    write_command_data(0x3A, pix, 1);
    
    // Frame rate control
    uint8_t frc[] = {0x00, 0x18};
    write_command_data(0xB1, frc, 2);
    
    // Display function control
    uint8_t dfc[] = {0x08, 0x82, 0x27};
    write_command_data(0xB6, dfc, 3);
    
    write_command(0x11);  // Exit sleep
    delay(120);
    write_command(0x29);  // Display on
    delay(50);
}

static void ili9341_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t ca[] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    write_command_data(0x2A, ca, 4);
    uint8_t pa[] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    write_command_data(0x2B, pa, 4);
    write_command(0x2C);  // Memory write
}

// ============================================================================
// ST7789 Driver (240x240 / 240x320)
// ============================================================================

static void st7789_init(uint8_t rotation) {
    digitalWrite(PIN_DISPLAY_RST, LOW);
    delay(10);
    digitalWrite(PIN_DISPLAY_RST, HIGH);
    delay(120);
    
    write_command(0x01);  // Soft reset
    delay(150);
    
    write_command(0x11);  // Sleep out
    delay(500);
    
    // Rotation
    uint8_t madctl;
    switch (rotation) {
        case 1: madctl = 0x60; break;
        case 2: madctl = 0xC0; break;
        case 3: madctl = 0xA0; break;
        default: madctl = 0x00; break;
    }
    uint8_t mac[] = {madctl};
    write_command_data(0x36, mac, 1);
    
    // RGB565
    uint8_t pix[] = {0x55};
    write_command_data(0x3A, pix, 1);
    
    // Porch control
    uint8_t porch[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    write_command_data(0xB2, porch, 5);
    
    // Gate control
    uint8_t gate[] = {0x35};
    write_command_data(0xB7, gate, 1);
    
    // VCOM
    uint8_t vcom[] = {0x19};
    write_command_data(0xBB, vcom, 1);
    
    write_command(0x29);  // Display ON
    delay(50);
}

static void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t ca[] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    write_command_data(0x2A, ca, 4);
    uint8_t pa[] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    write_command_data(0x2B, pa, 4);
    write_command(0x2C);
}

// ============================================================================
// ST7735 Driver (128x160)
// ============================================================================

static void st7735_init(uint8_t rotation) {
    digitalWrite(PIN_DISPLAY_RST, LOW);
    delay(10);
    digitalWrite(PIN_DISPLAY_RST, HIGH);
    delay(120);
    
    write_command(0x01);  // Soft reset
    delay(150);
    
    write_command(0x11);  // Sleep out
    delay(500);
    
    // Color mode: 16-bit
    uint8_t cm[] = {0x05};
    write_command_data(0x3A, cm, 1);
    
    // Rotation
    uint8_t madctl;
    switch (rotation) {
        case 1: madctl = 0x60; break;
        case 2: madctl = 0xC0; break;
        case 3: madctl = 0xA0; break;
        default: madctl = 0x00; break;
    }
    uint8_t mac[] = {madctl};
    write_command_data(0x36, mac, 1);
    
    write_command(0x29);  // Display on
    delay(50);
}

static void st7735_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // ST7735 has offset for some modules
    x0 += 2; x1 += 2;
    y0 += 1; y1 += 1;
    
    uint8_t ca[] = {0x00, (uint8_t)x0, 0x00, (uint8_t)x1};
    write_command_data(0x2A, ca, 4);
    uint8_t pa[] = {0x00, (uint8_t)y0, 0x00, (uint8_t)y1};
    write_command_data(0x2B, pa, 4);
    write_command(0x2C);
}

// ============================================================================
// Unified Display API
// ============================================================================

void display_init(uint8_t driver_id, uint16_t w, uint16_t h, uint8_t rotation) {
    current_driver = driver_id;
    display_width = w;
    display_height = h;
    
    // Configure SPI1 for display (fast!)
    SPI1.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
    
    switch (driver_id) {
        case DISPLAY_ILI9341:
            ili9341_init(rotation);
            break;
        case DISPLAY_ST7789:
            st7789_init(rotation);
            break;
        case DISPLAY_ST7735:
            st7735_init(rotation);
            break;
        default:
            // Generic/unknown - try ILI9341 protocol
            ili9341_init(rotation);
            break;
    }
}

void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    switch (current_driver) {
        case DISPLAY_ILI9341:
            ili9341_set_window(x0, y0, x1, y1);
            break;
        case DISPLAY_ST7789:
            st7789_set_window(x0, y0, x1, y1);
            break;
        case DISPLAY_ST7735:
            st7735_set_window(x0, y0, x1, y1);
            break;
        default:
            ili9341_set_window(x0, y0, x1, y1);
            break;
    }
}

void display_write_framebuffer(uint16_t* fb, uint32_t size) {
    display_set_window(0, 0, display_width - 1, display_height - 1);
    
    dc_data();
    cs_select();
    
    // DMA would be better, but for now use fast SPI
    uint32_t pixel_count = size / 2;
    for (uint32_t i = 0; i < pixel_count; i++) {
        SPI1.transfer16(fb[i]);
    }
    
    cs_deselect();
}
