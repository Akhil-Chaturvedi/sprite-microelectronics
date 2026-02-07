// ==============================================================================
// Sprite One - Framebuffer Implementation
// ==============================================================================

#include "framebuffer.h"
#include <string.h>

Framebuffer::Framebuffer(uint16_t w, uint16_t h) 
    : width(w), height(h), buffer(nullptr), draw_calls(0), pixels_drawn(0) {
}

Framebuffer::~Framebuffer() {
    if (buffer != nullptr) {
        delete[] buffer;
    }
}

bool Framebuffer::begin() {
    // Allocate framebuffer memory
    size_t bytes_needed = width * height * sizeof(uint16_t);
    
    Serial1.print("[FB] Allocating ");
    Serial1.print(bytes_needed);
    Serial1.print(" bytes (");
    Serial1.print(bytes_needed / 1024);
    Serial1.println(" KB)...");
    
    buffer = new uint16_t[width * height];
    
    if (buffer == nullptr) {
        Serial1.println("[FB] ERROR: Allocation failed!");
        return false;
    }
    
    Serial1.println("[FB] Allocation successful!");
    clear(COLOR_BLACK);
    
    return true;
}

void Framebuffer::clear(uint16_t color) {
    if (buffer == nullptr) return;
    
    // Fast fill using 16-bit writes
    for (uint32_t i = 0; i < (uint32_t)width * height; i++) {
        buffer[i] = color;
    }
    
    draw_calls++;
    pixels_drawn += width * height;
}

void Framebuffer::setPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (buffer == nullptr) return;
    if (x >= width || y >= height) return;
    
    buffer[y * width + x] = color;
    pixels_drawn++;
}

uint16_t Framebuffer::getPixel(uint16_t x, uint16_t y) {
    if (buffer == nullptr) return 0;
    if (x >= width || y >= height) return 0;
    
    return buffer[y * width + x];
}

bool Framebuffer::clipPoint(int16_t* x, int16_t* y) {
    return (*x >= 0 && *x < width && *y >= 0 && *y < height);
}

bool Framebuffer::clipRect(int16_t* x, int16_t* y, int16_t* w, int16_t* h) {
    // Clip left
    if (*x < 0) {
        *w += *x;
        *x = 0;
    }
    
    // Clip top
    if (*y < 0) {
        *h += *y;
        *y = 0;
    }
    
    // Clip right
    if (*x + *w > width) {
        *w = width - *x;
    }
    
    // Clip bottom
    if (*y + *h > height) {
        *h = height - *y;
    }
    
    // Check if anything visible
    return (*w > 0 && *h > 0);
}

void Framebuffer::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (buffer == nullptr) return;
    
    int16_t xi = (int16_t)x;
    int16_t yi = (int16_t)y;
    int16_t wi = (int16_t)w;
    int16_t hi = (int16_t)h;
    
    if (!clipRect(&xi, &yi, &wi, &hi)) return;
    
    // Fast row-by-row fill
    for (int16_t row = yi; row < yi + hi; row++) {
        uint16_t* line = &buffer[row * width + xi];
        for (int16_t col = 0; col < wi; col++) {
            line[col] = color;
        }
    }
    
    draw_calls++;
    pixels_drawn += wi * hi;
}

void Framebuffer::drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // Draw 4 lines for rectangle outline
    drawLine(x, y, x + w - 1, y, color);           // Top
    drawLine(x, y + h - 1, x + w - 1, y + h - 1, color); // Bottom
    drawLine(x, y, x, y + h - 1, color);           // Left
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, color); // Right
    
    draw_calls++;
}

void Framebuffer::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    if (buffer == nullptr) return;
    
    // Bresenham's line algorithm
    int16_t dx = abs((int16_t)x1 - (int16_t)x0);
    int16_t dy = abs((int16_t)y1 - (int16_t)y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    int16_t x = x0;
    int16_t y = y0;
    
    while (true) {
        setPixel(x, y, color);
        
        if (x == x1 && y == y1) break;
        
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    draw_calls++;
}

void Framebuffer::drawCircle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color) {
    if (buffer == nullptr) return;
    
    // Midpoint circle algorithm
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    
    while (x >= y) {
        // Draw 8 octants
        setPixel(cx + x, cy + y, color);
        setPixel(cx + y, cy + x, color);
        setPixel(cx - y, cy + x, color);
        setPixel(cx - x, cy + y, color);
        setPixel(cx - x, cy - y, color);
        setPixel(cx - y, cy - x, color);
        setPixel(cx + y, cy - x, color);
        setPixel(cx + x, cy - y, color);
        
        y += 1;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x -= 1;
            err += 1 - 2 * x;
        }
    }
    
    draw_calls++;
}

void Framebuffer::fillCircle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color) {
    if (buffer == nullptr) return;
    
    // Draw filled circle using horizontal lines
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    
    while (x >= y) {
        // Draw horizontal lines for each octant pair
        drawLine(cx - x, cy + y, cx + x, cy + y, color);
        drawLine(cx - x, cy - y, cx + x, cy - y, color);
        drawLine(cx - y, cy + x, cx + y, cy + x, color);
        drawLine(cx - y, cy - x, cx + y, cy - x, color);
        
        y += 1;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x -= 1;
            err += 1 - 2 * x;
        }
    }
    
    draw_calls++;
}

void Framebuffer::resetStats() {
    draw_calls = 0;
    pixels_drawn = 0;
}
