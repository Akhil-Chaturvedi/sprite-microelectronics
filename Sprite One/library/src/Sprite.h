/**
 * Sprite Library - Arduino/ESP32 Host Interface
 * 
 * The simplest possible API for hardware-accelerated graphics.
 * Connect your Sprite One module and go!
 * 
 * Example:
 *   #include <Sprite.h>
 *   Sprite gfx;
 *   
 *   void setup() {
 *     gfx.begin();
 *   }
 *   
 *   void loop() {
 *     gfx.clear(BLACK);
 *     gfx.rect(10, 10, 50, 50, RED);
 *     gfx.flush();
 *     delay(16);
 *   }
 * 
 * License: MIT
 * (c) Sprite Microelectronics
 */

#ifndef SPRITE_H
#define SPRITE_H

#include <Arduino.h>
#include <SPI.h>

// ============================================================================
// Version
// ============================================================================
#define SPRITE_LIB_VERSION "1.0.0"

// ============================================================================
// Protocol Constants
// ============================================================================
#define SPRITE_HEADER       0xAA
#define SPRITE_ACK          0x00
#define SPRITE_NAK          0x01

// System commands
#define CMD_NOP             0x00
#define CMD_INIT            0x01
#define CMD_RESET           0x02
#define CMD_VERSION         0x0F

// Graphics commands
#define CMD_CLEAR           0x10
#define CMD_PIXEL           0x11
#define CMD_RECT            0x12
#define CMD_RECT_OUTLINE    0x13
#define CMD_LINE            0x14
#define CMD_CIRCLE          0x15
#define CMD_SPRITE          0x20
#define CMD_TEXT            0x21
#define CMD_FLUSH           0x2F

// AI commands
#define CMD_LOAD_MODEL      0x50
#define CMD_INFER           0x51
#define CMD_GET_RESULT      0x52

// Display IDs
#define DISPLAY_ILI9341     0x01
#define DISPLAY_ST7789      0x02
#define DISPLAY_SSD1306     0x03
#define DISPLAY_ST7735      0x04
#define DISPLAY_ILI9488     0x05
#define DISPLAY_AUTO        0x00  // Auto-detect (future)

// ============================================================================
// Color Definitions (RGB565)
// ============================================================================
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define BLACK       0x0000
#define WHITE       0xFFFF
#define RED         0xF800
#define GREEN       0x07E0
#define BLUE        0x001F
#define YELLOW      0xFFE0
#define CYAN        0x07FF
#define MAGENTA     0xF81F
#define ORANGE      0xFD20
#define GRAY        0x8410
#define DARK_GRAY   0x4208

// ============================================================================
// Sprite Class
// ============================================================================

class Sprite {
public:
    /**
     * Initialize the Sprite library.
     * 
     * @param csPin  Chip Select pin for Sprite One module
     * @param spiSpeed  SPI clock speed (default 20MHz)
     */
    void begin(uint8_t csPin = 10, uint32_t spiSpeed = 20000000);
    
    /**
     * Initialize the display connected to Sprite One.
     * 
     * @param display  Display type (DISPLAY_ILI9341, etc.)
     * @param width    Display width in pixels
     * @param height   Display height in pixels
     * @param rotation Rotation 0-3 (0°, 90°, 180°, 270°)
     */
    void initDisplay(uint8_t display, uint16_t width, uint16_t height, uint8_t rotation = 0);
    
    // === Graphics Commands ===
    
    /**
     * Clear the screen with a solid color.
     */
    void clear(uint16_t color = BLACK);
    
    /**
     * Draw a single pixel.
     */
    void pixel(int16_t x, int16_t y, uint16_t color);
    
    /**
     * Draw a filled rectangle.
     */
    void rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    
    /**
     * Draw a rectangle outline.
     */
    void rectOutline(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    
    /**
     * Draw a line.
     */
    void line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    
    /**
     * Draw a filled circle.
     */
    void circle(int16_t x, int16_t y, int16_t r, uint16_t color);
    
    /**
     * Draw text at position.
     */
    void text(int16_t x, int16_t y, const char* str, uint16_t color = WHITE);
    
    /**
     * Draw a pre-loaded sprite.
     */
    void sprite(uint8_t id, int16_t x, int16_t y);
    
    /**
     * Push the framebuffer to the display.
     * Call this after all your drawing commands.
     */
    void flush();
    
    // === AI/Math Commands (Future) ===
    
    /**
     * Run inference on loaded model.
     * Returns confidence value 0-100.
     */
    uint8_t predict(const uint8_t* input, uint8_t inputLen);
    
    // === Utility ===
    
    /**
     * Check if Sprite One is connected and responding.
     */
    bool isConnected();
    
    /**
     * Get the firmware version string.
     */
    const char* getVersion();

private:
    uint8_t _csPin;
    uint32_t _spiSpeed;
    uint16_t _width;
    uint16_t _height;
    
    // SPI helpers
    void sendCommand(uint8_t cmd, const uint8_t* payload = nullptr, uint8_t len = 0);
    uint8_t waitAck(uint16_t timeoutMs = 100);
    uint8_t calcChecksum(const uint8_t* data, uint8_t len);
};

// ============================================================================
// Implementation
// ============================================================================

inline void Sprite::begin(uint8_t csPin, uint32_t spiSpeed) {
    _csPin = csPin;
    _spiSpeed = spiSpeed;
    
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    
    SPI.begin();
}

inline void Sprite::initDisplay(uint8_t display, uint16_t width, uint16_t height, uint8_t rotation) {
    _width = width;
    _height = height;
    
    uint8_t payload[6] = {
        display,
        (uint8_t)(width & 0xFF),
        (uint8_t)(width >> 8),
        (uint8_t)(height & 0xFF),
        (uint8_t)(height >> 8),
        rotation
    };
    sendCommand(CMD_INIT, payload, 6);
}

inline void Sprite::clear(uint16_t color) {
    uint8_t payload[2] = {
        (uint8_t)(color & 0xFF),
        (uint8_t)(color >> 8)
    };
    sendCommand(CMD_CLEAR, payload, 2);
}

inline void Sprite::pixel(int16_t x, int16_t y, uint16_t color) {
    uint8_t payload[6] = {
        (uint8_t)(x & 0xFF), (uint8_t)(x >> 8),
        (uint8_t)(y & 0xFF), (uint8_t)(y >> 8),
        (uint8_t)(color & 0xFF), (uint8_t)(color >> 8)
    };
    sendCommand(CMD_PIXEL, payload, 6);
}

inline void Sprite::rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    uint8_t payload[6] = {
        (uint8_t)x, (uint8_t)y,
        (uint8_t)w, (uint8_t)h,
        (uint8_t)(color & 0xFF), (uint8_t)(color >> 8)
    };
    sendCommand(CMD_RECT, payload, 6);
}

inline void Sprite::rectOutline(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    uint8_t payload[6] = {
        (uint8_t)x, (uint8_t)y,
        (uint8_t)w, (uint8_t)h,
        (uint8_t)(color & 0xFF), (uint8_t)(color >> 8)
    };
    sendCommand(CMD_RECT_OUTLINE, payload, 6);
}

inline void Sprite::line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    uint8_t payload[6] = {
        (uint8_t)x1, (uint8_t)y1,
        (uint8_t)x2, (uint8_t)y2,
        (uint8_t)(color & 0xFF), (uint8_t)(color >> 8)
    };
    sendCommand(CMD_LINE, payload, 6);
}

inline void Sprite::circle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    uint8_t payload[5] = {
        (uint8_t)x, (uint8_t)y, (uint8_t)r,
        (uint8_t)(color & 0xFF), (uint8_t)(color >> 8)
    };
    sendCommand(CMD_CIRCLE, payload, 5);
}

inline void Sprite::text(int16_t x, int16_t y, const char* str, uint16_t color) {
    uint8_t len = strlen(str);
    if (len > 250) len = 250;  // Leave room for header
    
    uint8_t payload[255];
    payload[0] = (uint8_t)x;
    payload[1] = (uint8_t)y;
    payload[2] = (uint8_t)(color & 0xFF);
    payload[3] = (uint8_t)(color >> 8);
    payload[4] = len;
    memcpy(&payload[5], str, len);
    
    sendCommand(CMD_TEXT, payload, 5 + len);
}

inline void Sprite::sprite(uint8_t id, int16_t x, int16_t y) {
    uint8_t payload[5] = {
        id,
        (uint8_t)(x & 0xFF), (uint8_t)(x >> 8),
        (uint8_t)(y & 0xFF), (uint8_t)(y >> 8)
    };
    sendCommand(CMD_SPRITE, payload, 5);
}

inline void Sprite::flush() {
    sendCommand(CMD_FLUSH);
}

inline uint8_t Sprite::predict(const uint8_t* input, uint8_t inputLen) {
    // TODO: Implement AI inference
    return 0;
}

inline bool Sprite::isConnected() {
    sendCommand(CMD_NOP);
    return (waitAck(50) == SPRITE_ACK);
}

inline const char* Sprite::getVersion() {
    return SPRITE_LIB_VERSION;
}

// === Private Implementation ===

inline void Sprite::sendCommand(uint8_t cmd, const uint8_t* payload, uint8_t len) {
    SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);
    
    // Build packet: HEADER + CMD + LEN + PAYLOAD + CHECKSUM
    SPI.transfer(SPRITE_HEADER);
    SPI.transfer(cmd);
    SPI.transfer(len);
    
    uint8_t checksum = cmd ^ len;
    for (uint8_t i = 0; i < len; i++) {
        SPI.transfer(payload[i]);
        checksum ^= payload[i];
    }
    SPI.transfer(checksum);
    
    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
}

inline uint8_t Sprite::waitAck(uint16_t timeoutMs) {
    uint32_t start = millis();
    
    SPI.beginTransaction(SPISettings(_spiSpeed, MSBFIRST, SPI_MODE0));
    digitalWrite(_csPin, LOW);
    
    uint8_t response = SPRITE_NAK;
    while ((millis() - start) < timeoutMs) {
        response = SPI.transfer(0x00);  // Clock out response
        if (response == SPRITE_ACK || response == SPRITE_NAK) {
            break;
        }
        delayMicroseconds(100);
    }
    
    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
    
    return response;
}

inline uint8_t Sprite::calcChecksum(const uint8_t* data, uint8_t len) {
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

#endif // SPRITE_H
