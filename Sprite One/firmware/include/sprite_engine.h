/*
 * Sprite Engine for Sprite One
 * Hardware-accelerated sprite compositor
 */

#ifndef SPRITE_ENGINE_H
#define SPRITE_ENGINE_H

#include <Arduino.h>

// Configuration
#define MAX_SPRITES 8
#define SPRITE_FLAG_VISIBLE  0x01
#define SPRITE_FLAG_FLIP_H   0x02
#define SPRITE_FLAG_FLIP_V   0x04

// Sprite structure
struct Sprite {
  uint8_t id;              // Unique identifier
  int16_t x, y;            // Position (can be negative for off-screen)
  uint8_t w, h;            // Dimensions
  const uint8_t* data;     // Bitmap data (1 bit per pixel)
  uint8_t flags;           // Visibility, flip, etc.
  uint8_t layer;           // Z-order (0=back, 255=front)
};

// Sprite Engine
class SpriteEngine {
private:
  Sprite sprites[MAX_SPRITES];
  uint8_t sprite_count;
  
  // Find sprite by ID
  int8_t findSprite(uint8_t id) {
    for (uint8_t i = 0; i < sprite_count; i++) {
      if (sprites[i].id == id) return i;
    }
    return -1;
  }
  
  // Render single sprite to framebuffer
  void renderSprite(const Sprite& sprite, uint8_t* framebuffer, uint16_t fb_width, uint16_t fb_height) {
    if (!(sprite.flags & SPRITE_FLAG_VISIBLE)) return;
    if (!sprite.data) return;
    
    // Clip to screen bounds
    int16_t start_x = max(0, sprite.x);
    int16_t start_y = max(0, sprite.y);
    int16_t end_x = min((int16_t)fb_width, (int16_t)(sprite.x + sprite.w));
    int16_t end_y = min((int16_t)fb_height, (int16_t)(sprite.y + sprite.h));
    
    if (start_x >= end_x || start_y >= end_y) return; // Fully clipped
    
    // Render each pixel
    for (int16_t sy = start_y; sy < end_y; sy++) {
      for (int16_t sx = start_x; sx < end_x; sx++) {
        // Calculate sprite-local coordinates
        int16_t local_x = sx - sprite.x;
        int16_t local_y = sy - sprite.y;
        
        // Apply flips
        if (sprite.flags & SPRITE_FLAG_FLIP_H) {
          local_x = sprite.w - 1 - local_x;
        }
        if (sprite.flags & SPRITE_FLAG_FLIP_V) {
          local_y = sprite.h - 1 - local_y;
        }
        
        // Get pixel from sprite data
        uint16_t sprite_idx = local_y * sprite.w + local_x;
        uint8_t sprite_byte = sprite.data[sprite_idx / 8];
        uint8_t sprite_bit = sprite_byte & (1 << (7 - (sprite_idx % 8)));
        
        // If pixel is set, draw to framebuffer
        if (sprite_bit) {
          uint16_t fb_idx = sy * fb_width + sx;
          framebuffer[fb_idx / 8] |= (1 << (7 - (fb_idx % 8)));
        }
      }
    }
  }
  
  // Bubble sort sprites by layer (simple, good enough for 8 sprites)
  void sortByLayer() {
    for (uint8_t i = 0; i < sprite_count - 1; i++) {
      for (uint8_t j = 0; j < sprite_count - i - 1; j++) {
        if (sprites[j].layer > sprites[j + 1].layer) {
          Sprite temp = sprites[j];
          sprites[j] = sprites[j + 1];
          sprites[j + 1] = temp;
        }
      }
    }
  }
  
public:
  SpriteEngine() : sprite_count(0) {}
  
  // Add or update sprite
  bool add(uint8_t id, int16_t x, int16_t y, uint8_t w, uint8_t h, 
           const uint8_t* data, uint8_t flags = SPRITE_FLAG_VISIBLE, uint8_t layer = 0) {
    int8_t idx = findSprite(id);
    
    if (idx >= 0) {
      // Update existing
      sprites[idx].x = x;
      sprites[idx].y = y;
      sprites[idx].w = w;
      sprites[idx].h = h;
      sprites[idx].data = data;
      sprites[idx].flags = flags;
      sprites[idx].layer = layer;
    } else {
      // Add new
      if (sprite_count >= MAX_SPRITES) return false;
      
      sprites[sprite_count].id = id;
      sprites[sprite_count].x = x;
      sprites[sprite_count].y = y;
      sprites[sprite_count].w = w;
      sprites[sprite_count].h = h;
      sprites[sprite_count].data = data;
      sprites[sprite_count].flags = flags;
      sprites[sprite_count].layer = layer;
      sprite_count++;
    }
    
    sortByLayer();
    return true;
  }
  
  // Move sprite
  bool move(uint8_t id, int16_t x, int16_t y) {
    int8_t idx = findSprite(id);
    if (idx < 0) return false;
    
    sprites[idx].x = x;
    sprites[idx].y = y;
    return true;
  }
  
  // Remove sprite
  bool remove(uint8_t id) {
    int8_t idx = findSprite(id);
    if (idx < 0) return false;
    
    // Shift remaining sprites down
    for (uint8_t i = idx; i < sprite_count - 1; i++) {
      sprites[i] = sprites[i + 1];
    }
    sprite_count--;
    return true;
  }
  
  // Set visibility
  bool setVisible(uint8_t id, bool visible) {
    int8_t idx = findSprite(id);
    if (idx < 0) return false;
    
    if (visible) {
      sprites[idx].flags |= SPRITE_FLAG_VISIBLE;
    } else {
      sprites[idx].flags &= ~SPRITE_FLAG_VISIBLE;
    }
    return true;
  }
  
  // Check collision (AABB)
  bool checkCollision(uint8_t id_a, uint8_t id_b) {
    int8_t idx_a = findSprite(id_a);
    int8_t idx_b = findSprite(id_b);
    
    if (idx_a < 0 || idx_b < 0) return false;
    
    const Sprite& a = sprites[idx_a];
    const Sprite& b = sprites[idx_b];
    
    // AABB collision
    return !(a.x + a.w <= b.x || 
             b.x + b.w <= a.x ||
             a.y + a.h <= b.y ||
             b.y + b.h <= a.y);
  }
  
  // Render all sprites to framebuffer
  void render(uint8_t* framebuffer, uint16_t fb_width, uint16_t fb_height) {
    // Sprites are already sorted by layer
    for (uint8_t i = 0; i < sprite_count; i++) {
      renderSprite(sprites[i], framebuffer, fb_width, fb_height);
    }
  }
  
  // Clear all sprites
  void clear() {
    sprite_count = 0;
  }
  
  // Get sprite count
  uint8_t count() const {
    return sprite_count;
  }
};

#endif // SPRITE_ENGINE_H
