/*
 * Sprite Display Abstraction Layer
 * Supports multiple display types: SSD1306, Simulated
 */

#ifndef SPRITE_DISPLAY_H
#define SPRITE_DISPLAY_H

#include <Arduino.h>
#include <Wire.h>

// Display configuration
#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 64

// SSD1306 I2C Configuration
#define SSD1306_I2C_ADDR 0x3C
#define SSD1306_I2C_SDA  4  // GP4
#define SSD1306_I2C_SCL  5  // GP5

// SSD1306 Commands
#define SSD1306_CMD_DISPLAY_OFF      0xAE
#define SSD1306_CMD_DISPLAY_ON       0xAF
#define SSD1306_CMD_SET_CONTRAST     0x81
#define SSD1306_CMD_NORMAL_DISPLAY   0xA6
#define SSD1306_CMD_INVERT_DISPLAY   0xA7
#define SSD1306_CMD_SET_MUX_RATIO    0xA8
#define SSD1306_CMD_SET_DISPLAY_OFFSET 0xD3
#define SSD1306_CMD_SET_START_LINE   0x40
#define SSD1306_CMD_SEGMENT_REMAP    0xA1
#define SSD1306_CMD_COM_SCAN_DEC     0xC8
#define SSD1306_CMD_SET_COM_PINS     0xDA
#define SSD1306_CMD_SET_CLOCK_DIV    0xD5
#define SSD1306_CMD_SET_PRECHARGE    0xD9
#define SSD1306_CMD_SET_VCOM_DETECT  0xDB
#define SSD1306_CMD_CHARGE_PUMP      0x8D
#define SSD1306_CMD_MEMORY_MODE      0x20
#define SSD1306_CMD_COLUMN_ADDR      0x21
#define SSD1306_CMD_PAGE_ADDR        0x22

// Abstract display interface
class SpriteDisplay {
public:
  virtual bool init() = 0;
  virtual void update(const uint8_t* framebuffer) = 0;
  virtual void updateRegion(const uint8_t* framebuffer, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) = 0;
  virtual void setContrast(uint8_t level) = 0;
  virtual const char* name() = 0;
  virtual ~SpriteDisplay() {}
};

// SSD1306 OLED Display (128x64, I2C)
class SSD1306Display : public SpriteDisplay {
private:
  TwoWire* wire;
  uint8_t i2c_addr;
  
  void sendCommand(uint8_t cmd) {
    wire->beginTransmission(i2c_addr);
    wire->write(0x00); // Command mode
    wire->write(cmd);
    wire->endTransmission();
  }
  
  void sendCommands(const uint8_t* cmds, size_t count) {
    wire->beginTransmission(i2c_addr);
    wire->write(0x00); // Command mode
    for (size_t i = 0; i < count; i++) {
      wire->write(cmds[i]);
    }
    wire->endTransmission();
  }
  
public:
  SSD1306Display(TwoWire* w = &Wire, uint8_t addr = SSD1306_I2C_ADDR) 
    : wire(w), i2c_addr(addr) {}
  
  bool init() override {
    // Initialize I2C
    wire->setSDA(SSD1306_I2C_SDA);
    wire->setSCL(SSD1306_I2C_SCL);
    wire->begin();
    wire->setClock(400000); // 400kHz I2C
    
    delay(100); // Power-up delay
    
    // Initialization sequence
    const uint8_t init_cmds[] = {
      SSD1306_CMD_DISPLAY_OFF,
      SSD1306_CMD_SET_CLOCK_DIV, 0x80,
      SSD1306_CMD_SET_MUX_RATIO, 0x3F,
      SSD1306_CMD_SET_DISPLAY_OFFSET, 0x00,
      SSD1306_CMD_SET_START_LINE | 0x00,
      SSD1306_CMD_CHARGE_PUMP, 0x14,
      SSD1306_CMD_MEMORY_MODE, 0x00, // Horizontal addressing
      SSD1306_CMD_SEGMENT_REMAP | 0x01,
      SSD1306_CMD_COM_SCAN_DEC,
      SSD1306_CMD_SET_COM_PINS, 0x12,
      SSD1306_CMD_SET_CONTRAST, 0xCF,
      SSD1306_CMD_SET_PRECHARGE, 0xF1,
      SSD1306_CMD_SET_VCOM_DETECT, 0x40,
      SSD1306_CMD_NORMAL_DISPLAY,
      SSD1306_CMD_DISPLAY_ON
    };
    
    sendCommands(init_cmds, sizeof(init_cmds));
    
    return true;
  }
  
  void update(const uint8_t* framebuffer) override {
    // Set column and page address range (full screen)
    const uint8_t addr_cmds[] = {
      SSD1306_CMD_COLUMN_ADDR, 0, 127,
      SSD1306_CMD_PAGE_ADDR, 0, 7
    };
    sendCommands(addr_cmds, sizeof(addr_cmds));
    
    // Send framebuffer data in chunks
    const size_t CHUNK_SIZE = 32;
    const size_t TOTAL_SIZE = (DISPLAY_WIDTH * DISPLAY_HEIGHT) / 8;
    
    for (size_t i = 0; i < TOTAL_SIZE; i += CHUNK_SIZE) {
      wire->beginTransmission(i2c_addr);
      wire->write(0x40); // Data mode
      
      size_t chunk = (i + CHUNK_SIZE > TOTAL_SIZE) ? (TOTAL_SIZE - i) : CHUNK_SIZE;
      wire->write(&framebuffer[i], chunk);
      
      wire->endTransmission();
    }
  }
  
  void updateRegion(const uint8_t* framebuffer, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) override {
    // Calculate page bounds (each page is 8 pixels tall)
    uint8_t page_start = y1 / 8;
    uint8_t page_end = y2 / 8;
    
    // Set address window
    const uint8_t addr_cmds[] = {
      SSD1306_CMD_COLUMN_ADDR, (uint8_t)x1, (uint8_t)x2,
      SSD1306_CMD_PAGE_ADDR, page_start, page_end
    };
    sendCommands(addr_cmds, sizeof(addr_cmds));
    
    // Send only the dirty region
    size_t width = x2 - x1 + 1;
    for (uint8_t page = page_start; page <= page_end; page++) {
      size_t offset = page * DISPLAY_WIDTH + x1;
      
      wire->beginTransmission(i2c_addr);
      wire->write(0x40); // Data mode
      wire->write(&framebuffer[offset], width);
      wire->endTransmission();
    }
  }
  
  void setContrast(uint8_t level) override {
    sendCommand(SSD1306_CMD_SET_CONTRAST);
    sendCommand(level);
  }
  
  const char* name() override {
    return "SSD1306";
  }
};

// Simulated Display (Serial output, current behavior)
class SimulatedDisplay : public SpriteDisplay {
public:
  bool init() override {
    return true; // No initialization needed
  }
  
  void update(const uint8_t* framebuffer) override {
    // Current behavior: just acknowledge
    // Actual rendering happens in verbose logging
  }
  
  void updateRegion(const uint8_t* framebuffer, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) override {
    // No-op for simulated
  }
  
  void setContrast(uint8_t level) override {
    // No-op for simulated
  }
  
  const char* name() override {
    return "Simulated";
  }
};

#endif // SPRITE_DISPLAY_H
