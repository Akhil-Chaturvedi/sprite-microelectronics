// ==============================================================================
// Sprite One - Sprite Blitting Implementation
// ==============================================================================

#include "sprite.h"

// ==============================================================================
// Basic Blitting (No Transparency)
// ==============================================================================

void SpriteRenderer::blit(Framebuffer* fb, const Sprite* sprite, int16_t x, int16_t y) {
    uint16_t* buffer = fb->getBuffer();
    uint16_t fb_width = fb->getWidth();
    uint16_t fb_height = fb->getHeight();
    
    for (uint16_t row = 0; row < sprite->height; row++) {
        for (uint16_t col = 0; col < sprite->width; col++) {
            int16_t px = x + col;
            int16_t py = y + row;
            
            // Bounds check
            if (px < 0 || px >= fb_width || py < 0 || py >= fb_height) continue;
            
            uint16_t pixel = sprite->pixels[row * sprite->width + col];
            buffer[py * fb_width + px] = pixel;
        }
    }
}

// ==============================================================================
// Transparent Blitting (Color Key)
// ==============================================================================

void SpriteRenderer::blitTransparent(Framebuffer* fb, const Sprite* sprite, int16_t x, int16_t y) {
    uint16_t* buffer = fb->getBuffer();
    uint16_t fb_width = fb->getWidth();
    uint16_t fb_height = fb->getHeight();
    uint16_t transp = sprite->transparent_color;
    
    for (uint16_t row = 0; row < sprite->height; row++) {
        for (uint16_t col = 0; col < sprite->width; col++) {
            int16_t px = x + col;
            int16_t py = y + row;
            
            // Bounds check
            if (px < 0 || px >= fb_width || py < 0 || py >= fb_height) continue;
            
            uint16_t pixel = sprite->pixels[row * sprite->width + col];
            
            // Skip transparent pixels
            if (pixel == transp) continue;
            
            buffer[py * fb_width + px] = pixel;
        }
    }
}

// ==============================================================================
// Rotated Blitting
// ==============================================================================

void SpriteRenderer::blitRotated(Framebuffer* fb, const Sprite* sprite, int16_t x, int16_t y, SpriteRotation rotation) {
    uint16_t* buffer = fb->getBuffer();
    uint16_t fb_width = fb->getWidth();
    uint16_t fb_height = fb->getHeight();
    uint16_t transp = sprite->transparent_color;
    
    uint16_t w = sprite->width;
    uint16_t h = sprite->height;
    
    for (uint16_t row = 0; row < h; row++) {
        for (uint16_t col = 0; col < w; col++) {
            int16_t px, py;
            
            // Apply rotation transform
            switch (rotation) {
                case ROTATE_0:
                    px = x + col;
                    py = y + row;
                    break;
                case ROTATE_90:
                    px = x + (h - 1 - row);
                    py = y + col;
                    break;
                case ROTATE_180:
                    px = x + (w - 1 - col);
                    py = y + (h - 1 - row);
                    break;
                case ROTATE_270:
                    px = x + row;
                    py = y + (w - 1 - col);
                    break;
            }
            
            // Bounds check
            if (px < 0 || px >= fb_width || py < 0 || py >= fb_height) continue;
            
            uint16_t pixel = sprite->pixels[row * w + col];
            
            // Skip transparent pixels
            if (pixel == transp) continue;
            
            buffer[py * fb_width + px] = pixel;
        }
    }
}

// ==============================================================================
// Scaled Blitting (Nearest-Neighbor)
// ==============================================================================

void SpriteRenderer::blitScaled(Framebuffer* fb, const Sprite* sprite, int16_t x, int16_t y, uint8_t scale) {
    if (scale == 0) scale = 1;
    
    uint16_t* buffer = fb->getBuffer();
    uint16_t fb_width = fb->getWidth();
    uint16_t fb_height = fb->getHeight();
    uint16_t transp = sprite->transparent_color;
    
    for (uint16_t row = 0; row < sprite->height; row++) {
        for (uint16_t col = 0; col < sprite->width; col++) {
            uint16_t pixel = sprite->pixels[row * sprite->width + col];
            
            // Skip transparent pixels
            if (pixel == transp) continue;
            
            // Draw scaled pixel block
            for (uint8_t sy = 0; sy < scale; sy++) {
                for (uint8_t sx = 0; sx < scale; sx++) {
                    int16_t px = x + col * scale + sx;
                    int16_t py = y + row * scale + sy;
                    
                    // Bounds check
                    if (px < 0 || px >= fb_width || py < 0 || py >= fb_height) continue;
                    
                    buffer[py * fb_width + px] = pixel;
                }
            }
        }
    }
}

// ==============================================================================
// Helper Functions
// ==============================================================================

Sprite SpriteRenderer::createSprite(uint16_t w, uint16_t h, uint16_t transp_color, const uint16_t* data) {
    Sprite s;
    s.width = w;
    s.height = h;
    s.transparent_color = transp_color;
    s.pixels = data;
    return s;
}

// ==============================================================================
// Built-in Test Sprites
// ==============================================================================

// 8x8 Checkerboard
const uint16_t sprite_checkerboard_8x8[] PROGMEM = {
    COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK,
    COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE,
    COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK,
    COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE,
    COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK,
    COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE,
    COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK,
    COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE, COLOR_BLACK, COLOR_WHITE,
};

const Sprite SPRITE_CHECKERBOARD_8x8 = {
    8, 8, 0xF81F, sprite_checkerboard_8x8
};

// 16x16 Smiley Face
const uint16_t sprite_smiley_16x16[] PROGMEM = {
    0xF81F,0xF81F,0xF81F,0xF81F,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xF81F,0xF81F,0xF81F,0xF81F,
    0xF81F,0xF81F,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xF81F,0xF81F,
    0xF81F,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xF81F,
    0xF81F,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xF81F,
    0xFFE0,0xFFE0,0xFFE0,0x0000,0x0000,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0x0000,0x0000,0xFFE0,0xFFE0,0xFFE0,
    0xFFE0,0xFFE0,0xFFE0,0x0000,0x0000,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0x0000,0x0000,0xFFE0,0xFFE0,0xFFE0,
    0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,
    0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,
    0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,
    0xFFE0,0xFFE0,0x0000,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0x0000,0xFFE0,0xFFE0,
    0xFFE0,0xFFE0,0xFFE0,0x0000,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0x0000,0xFFE0,0xFFE0,0xFFE0,
    0xFFE0,0xFFE0,0xFFE0,0xFFE0,0x0000,0x0000,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0x0000,0x0000,0xFFE0,0xFFE0,0xFFE0,0xFFE0,
    0xF81F,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0x0000,0x0000,0x0000,0x0000,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xF81F,
    0xF81F,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xF81F,
    0xF81F,0xF81F,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xF81F,0xF81F,
    0xF81F,0xF81F,0xF81F,0xF81F,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xF81F,0xF81F,0xF81F,0xF81F,
};

const Sprite SPRITE_SMILEY_16x16 = {
    16, 16, 0xF81F, sprite_smiley_16x16
};

// 32x32 Player (simple character - red square with white border)
const uint16_t sprite_player_32x32[32*32] PROGMEM = {
    // Row 0-31: Too long to manually define, generate programmatically in test
};

const Sprite SPRITE_PLAYER_32x32 = {
    32, 32, 0xF81F, sprite_player_32x32
};
