/*
 * Sprite Transport Abstraction Layer
 * Supports both UART and USB-CDC transports
 */

#ifndef SPRITE_TRANSPORT_H
#define SPRITE_TRANSPORT_H

#include <Arduino.h>

// Abstract transport interface
class SpriteTransport {
public:
  virtual int available() = 0;
  virtual uint8_t read() = 0;
  virtual size_t write(uint8_t b) = 0;
  virtual size_t write(const uint8_t* buffer, size_t size) = 0;
  virtual void flush() = 0;
  virtual const char* name() = 0;
  virtual bool isConnected() = 0;
};

// UART Transport (Serial1, hardware UART)
class UARTTransport : public SpriteTransport {
private:
  HardwareSerial& serial;
  
public:
  UARTTransport(HardwareSerial& s) : serial(s) {}
  
  int available() override { return serial.available(); }
  uint8_t read() override { return serial.read(); }
  size_t write(uint8_t b) override { return serial.write(b); }
  size_t write(const uint8_t* buffer, size_t size) override { 
    return serial.write(buffer, size); 
  }
  void flush() override { serial.flush(); }
  const char* name() override { return "UART"; }
  bool isConnected() override { return true; } // UART always connected
};

// USB-CDC Transport (Serial, native USB)
class USBTransport : public SpriteTransport {
private:
  USBCDC& serial;
  
public:
  USBTransport(USBCDC& s) : serial(s) {}
  
  int available() override { return serial.available(); }
  uint8_t read() override { return serial.read(); }
  size_t write(uint8_t b) override { return serial.write(b); }
  size_t write(const uint8_t* buffer, size_t size) override { 
    return serial.write(buffer, size); 
  }
  void flush() override { serial.flush(); }
  const char* name() override { return "USB-CDC"; }
  bool isConnected() override { return serial; } // USB checks DTR/RTS
};

// Transport manager - auto-detects active interface
class TransportManager {
private:
  UARTTransport uart;
  USBTransport usb;
  SpriteTransport* active;
  
public:
  TransportManager() 
    : uart(Serial1), usb(Serial), active(nullptr) {}
  
  void begin(uint32_t baudrate = 115200) {
    Serial.begin(baudrate);   // USB-CDC (baud ignored on USB)
    Serial1.begin(baudrate);  // UART
    delay(100); // Stabilization
  }
  
  // Auto-detect which interface has data
  SpriteTransport* detect() {
    if (active) return active; // Already locked
    
    // Check USB first (higher priority/bandwidth)
    if (Serial.available()) {
      active = &usb;
      return active;
    }
    
    // Check UART second
    if (Serial1.available()) {
      active = &uart;
      return active;
    }
    
    return nullptr; // No data yet
  }
  
  SpriteTransport* getActive() { return active; }
  
  void reset() { active = nullptr; } // Allow re-detection
  
  // Direct access for debug output
  UARTTransport& getUART() { return uart; }
  USBTransport& getUSB() { return usb; }
};

#endif // SPRITE_TRANSPORT_H
