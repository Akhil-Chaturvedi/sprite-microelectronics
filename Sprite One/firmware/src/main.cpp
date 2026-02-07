/**
 * Sprite One - Firmware Test (Wokwi Compatible)
 * 
 * This is a simplified version for testing in Wokwi simulator.
 * It demonstrates basic SPI slave functionality.
 * 
 * License: MIT
 * (c) Sprite Microelectronics
 */

#include <Arduino.h>
#include <SPI.h>

// ============================================================================
// Pin Definitions
// ============================================================================
#define PIN_LED_STATUS  2     // Green LED - shows we're alive
#define PIN_LED_CMD     3     // Red LED - blinks on command received

#define PIN_SPI_SCK     18    // SPI0 SCK
#define PIN_SPI_MOSI    19    // SPI0 TX (data from host)  
#define PIN_SPI_MISO    16    // SPI0 RX (data to host)
#define PIN_SPI_CS      17    // Chip select

// ============================================================================
// Protocol Constants
// ============================================================================
#define SPRITE_HEADER   0xAA
#define SPRITE_ACK      0x00
#define SPRITE_NAK      0x01

// Commands
#define CMD_NOP         0x00
#define CMD_INIT        0x01
#define CMD_CLEAR       0x10
#define CMD_RECT        0x12
#define CMD_FLUSH       0x2F

// ============================================================================
// Global State
// ============================================================================
bool initialized = false;
uint16_t displayWidth = 320;
uint16_t displayHeight = 240;
uint32_t commandCount = 0;

// ============================================================================
// Command Processing
// ============================================================================

void processCommand(uint8_t cmd, uint8_t* payload, uint8_t len) {
    commandCount++;
    
    // Blink command LED
    digitalWrite(PIN_LED_CMD, HIGH);
    
    switch (cmd) {
        case CMD_NOP:
            Serial.println("[CMD] NOP - Ping received");
            break;
            
        case CMD_INIT:
            if (len >= 6) {
                uint8_t driver = payload[0];
                displayWidth = payload[1] | (payload[2] << 8);
                displayHeight = payload[3] | (payload[4] << 8);
                uint8_t rotation = payload[5];
                
                Serial.printf("[CMD] INIT - Driver: %d, Size: %dx%d, Rot: %d\n", 
                              driver, displayWidth, displayHeight, rotation);
                initialized = true;
            }
            break;
            
        case CMD_CLEAR:
            if (len >= 2) {
                uint16_t color = payload[0] | (payload[1] << 8);
                Serial.printf("[CMD] CLEAR - Color: 0x%04X\n", color);
            }
            break;
            
        case CMD_RECT:
            if (len >= 6) {
                uint8_t x = payload[0];
                uint8_t y = payload[1];
                uint8_t w = payload[2];
                uint8_t h = payload[3];
                uint16_t color = payload[4] | (payload[5] << 8);
                Serial.printf("[CMD] RECT - x:%d y:%d w:%d h:%d color:0x%04X\n", 
                              x, y, w, h, color);
            }
            break;
            
        case CMD_FLUSH:
            Serial.println("[CMD] FLUSH - Frame complete");
            break;
            
        default:
            Serial.printf("[CMD] Unknown command: 0x%02X\n", cmd);
            break;
    }
    
    delay(50);
    digitalWrite(PIN_LED_CMD, LOW);
}

// ============================================================================
// SPI Slave (Software Implementation for Wokwi)
// ============================================================================

// Simple packet reception state machine
enum RxState { WAIT_HEADER, READ_CMD, READ_LEN, READ_PAYLOAD, READ_CHECKSUM };
RxState rxState = WAIT_HEADER;
uint8_t rxCmd = 0;
uint8_t rxLen = 0;
uint8_t rxPayload[255];
uint8_t rxPos = 0;
uint8_t rxChecksum = 0;

void processSerialByte(uint8_t byte) {
    switch (rxState) {
        case WAIT_HEADER:
            if (byte == SPRITE_HEADER) {
                rxState = READ_CMD;
                rxChecksum = 0;
            }
            break;
            
        case READ_CMD:
            rxCmd = byte;
            rxChecksum ^= byte;
            rxState = READ_LEN;
            break;
            
        case READ_LEN:
            rxLen = byte;
            rxChecksum ^= byte;
            rxPos = 0;
            if (rxLen == 0) {
                rxState = READ_CHECKSUM;
            } else {
                rxState = READ_PAYLOAD;
            }
            break;
            
        case READ_PAYLOAD:
            rxPayload[rxPos++] = byte;
            rxChecksum ^= byte;
            if (rxPos >= rxLen) {
                rxState = READ_CHECKSUM;
            }
            break;
            
        case READ_CHECKSUM:
            if (byte == rxChecksum) {
                // Valid packet!
                processCommand(rxCmd, rxPayload, rxLen);
                Serial.write(SPRITE_ACK);
            } else {
                Serial.printf("[ERR] Checksum mismatch: expected 0x%02X, got 0x%02X\n", 
                              rxChecksum, byte);
                Serial.write(SPRITE_NAK);
            }
            rxState = WAIT_HEADER;
            break;
    }
}

// ============================================================================
// Main Program
// ============================================================================

void setup() {
    Serial.begin(115200);
    
    // LED pins
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_CMD, OUTPUT);
    
    // Startup blink sequence
    for (int i = 0; i < 3; i++) {
        digitalWrite(PIN_LED_STATUS, HIGH);
        digitalWrite(PIN_LED_CMD, HIGH);
        delay(100);
        digitalWrite(PIN_LED_STATUS, LOW);
        digitalWrite(PIN_LED_CMD, LOW);
        delay(100);
    }
    
    Serial.println();
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║     Sprite One - Firmware v1.0.0       ║");
    Serial.println("║     Open Source Hardware Accelerator   ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.println();
    Serial.println("[INFO] Waiting for commands via Serial...");
    Serial.println("[INFO] Protocol: 0xAA + CMD + LEN + PAYLOAD + CHECKSUM");
    Serial.println();
    Serial.println("Test commands (paste these hex bytes):");
    Serial.println("  NOP:   AA 00 00 00");
    Serial.println("  CLEAR: AA 10 02 00 F8 EA   (Red)");
    Serial.println("  RECT:  AA 12 06 0A 0A 14 14 00 F8 04  (10,10,20,20,Red)");
    Serial.println();
    
    // Status LED on = ready
    digitalWrite(PIN_LED_STATUS, HIGH);
}

void loop() {
    // Process incoming serial bytes
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        processSerialByte(byte);
    }
    
    // Heartbeat on status LED
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink > 1000) {
        digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
        lastBlink = millis();
        
        // Print status
        if (commandCount > 0) {
            Serial.printf("[STATUS] Commands processed: %lu\n", commandCount);
        }
    }
}
