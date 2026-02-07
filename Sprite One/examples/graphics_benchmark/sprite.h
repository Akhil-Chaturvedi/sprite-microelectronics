// ==============================================================================
// Sprite One - Sprite Blitting System
// ==============================================================================
// Supports transparent sprites, rotation, and scaling
// ==============================================================================

#ifndef SPRITE_H
#define SPRITE_H

#include <Arduino.h>
#include "framebuffer.h"

// Sprite structure
struct Sprite {
    uint16_t width;
    uint16_t height;
    uint16_t transparent_color;  // Color key for transparency (0xFFFF = magenta default)
    const uint16_t* pixels;      // Pointer to pixel data (ROM or RAM)
};

// Rotation modes
enum SpriteRotation {
    ROTATE_0 = 0,
    ROTATE_90 = 1,
    ROTATE_180 = 2,
    ROTATE_270 = 3
};

// Sprite rendering functions
class SpriteRenderer {
public:
    // Basic blitting
    static void blit(Framebuffer* fb, const Sprite* sprite, int16_t x, int16_t y);
    
    // Blitting with transparency
    static void blitTransparent(Framebuffer* fb, const Sprite* sprite, int16_t x, int16_t y);
    
    // Blitting with rotation
    static void blitRotated(Framebuffer* fb, const Sprite* sprite, int16_t x, int16_t y, SpriteRotation rotation);
    
    // Blitting with scaling (2x, 4x nearest-neighbor)
    static void blitScaled(Framebuffer* fb, const Sprite* sprite, int16_t x, int16_t y, uint8_t scale);
    
    // Helper: Create sprite from raw data
    static Sprite createSprite(uint16_t w, uint16_t h, uint16_t transp_color, const uint16_t* data);
};

// ==============================================================================
// Built-in Test Sprites
// ==============================================================================

// 8x8 test pattern (checkerboard)
extern const uint16_t sprite_checkerboard_8x8[];
extern const Sprite SPRITE_CHECKERBOARD_8x8;

// 16x16 smiley face
extern const uint16_t sprite_smiley_16x16[];
extern const Sprite SPRITE_SMILEY_16x16;

// 32x32 player character
extern const uint16_t sprite_player_32x32[];
extern const Sprite SPRITE_PLAYER_32x32;

#endif // SPRITE_H
