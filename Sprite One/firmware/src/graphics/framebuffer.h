// ==============================================================================
// Sprite One - Framebuffer Class
// ==============================================================================
// RGB565 framebuffer with clipping and basic operations
// Memory: 320x240x2 = 153,600 bytes (150 KB)
// ==============================================================================

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <Arduino.h>

// RGB565 color definitions
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_MAGENTA 0xF81F
#define COLOR_CYAN    0x07FF
#define COLOR_ORANGE  0xFD20
#define COLOR_PURPLE  0x780F
#define COLOR_GRAY    0x8410

// Default display size
#define FB_WIDTH  320
#define FB_HEIGHT 240

// Helper: Convert RGB888 to RGB565
inline uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Helper: Extract RGB from RGB565
inline void rgb565_to_rgb888(uint16_t color, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = (color >> 11) << 3;
    *g = ((color >> 5) & 0x3F) << 2;
    *b = (color & 0x1F) << 3;
}

class Framebuffer {
public:
    Framebuffer(uint16_t w = FB_WIDTH, uint16_t h = FB_HEIGHT);
    ~Framebuffer();
    
    // Initialization
    bool begin();
    
    // Basic operations
    void clear(uint16_t color = COLOR_BLACK);
    void setPixel(uint16_t x, uint16_t y, uint16_t color);
    uint16_t getPixel(uint16_t x, uint16_t y);
    
    // Primitives (implemented in primitives.cpp)
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    void drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
    void drawCircle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color);
    void fillCircle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color);
    
    // Getters
    uint16_t getWidth() const { return width; }
    uint16_t getHeight() const { return height; }
    uint16_t* getBuffer() { return buffer; }
    
    // Statistics
    uint32_t getDrawCalls() const { return draw_calls; }
    uint32_t getPixelsDrawn() const { return pixels_drawn; }
    void resetStats();
    
private:
    uint16_t* buffer;
    uint16_t width;
    uint16_t height;
    
    // Statistics
    uint32_t draw_calls;
    uint32_t pixels_drawn;
    
    // Clipping helpers
    bool clipPoint(int16_t* x, int16_t* y);
    bool clipRect(int16_t* x, int16_t* y, int16_t* w, int16_t* h);
};

#endif // FRAMEBUFFER_H
