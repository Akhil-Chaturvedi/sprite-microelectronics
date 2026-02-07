/**
 * Sprite One - Firmware v2.1 (Graphics Edition)
 * 
 * Week 2: Now with actual rendering!
 * 
 * Features:
 * - RGB565 framebuffer (320x240)
 * - Primitive rendering (rect, line, circle)
 * - Command processing
 * 
 * License: MIT
 * (c) Sprite Microelectronics
 */

#include "framebuffer.h"

// Pin Definitions  
#define PIN_LED_STATUS  25    // Onboard LED - heartbeat
#define PIN_LED_RX      15    // RX activity indicator

// Protocol Constants
#define SPRITE_HEADER   0xAA
#define SPRITE_ACK      0x00
#define SPRITE_NAK      0x01

// Commands  
#define CMD_NOP         0x00
#define CMD_INIT        0x01
#define CMD_PING        0x02
#define CMD_GET_INFO    0x03
#define CMD_CLEAR       0x10
#define CMD_RECT        0x12
#define CMD_CIRCLE      0x15
#define CMD_LINE        0x16
#define CMD_FLUSH       0x2F

// Global State
Framebuffer fb(320, 240);
bool initialized = false;
uint16_t displayWidth = 320;
uint16_t displayHeight = 240;
uint32_t commandCount = 0;
uint32_t rxByteCount = 0;
uint32_t txByteCount = 0;
uint32_t errorCount = 0;
uint32_t lastHeartbeat = 0;

// ==============================================================================
// Command Processing
// ==============================================================================

void sendByte(uint8_t byte) {
    Serial.write(byte);
    txByteCount++;
}

void sendAck() {
    sendByte(SPRITE_ACK);
}

void sendNak() {
    sendByte(SPRITE_NAK);
}

void processCommand(uint8_t cmd, uint8_t* payload, uint8_t len) {
    commandCount++;
    
    // Blink RX LED
    digitalWrite(PIN_LED_RX, HIGH);
    
    Serial1.print("[CMD #");
    Serial1.print(commandCount);
    Serial1.print("] ");
    
    // Track timing
    uint32_t start = micros();
    
    switch (cmd) {
        case CMD_NOP:
            Serial1.println("NOP");
            sendAck();
            break;
            
        case CMD_PING:
            Serial1.println("PING");
            sendAck();
            sendByte(0x42); // Magic response
            break;
            
        case CMD_GET_INFO:
            Serial1.println("GET_INFO");
            sendAck();
            sendByte(2); // Major version
            sendByte(1); // Minor version (2.1)
            sendByte(displayWidth & 0xFF);
            sendByte(displayWidth >> 8);
            sendByte(displayHeight & 0xFF);
            sendByte(displayHeight >> 8);
            break;
            
        case CMD_INIT:
            if (len >= 6) {
                uint8_t driver = payload[0];
                displayWidth = payload[1] | (payload[2] << 8);
                displayHeight = payload[3] | (payload[4] << 8);
                uint8_t rotation = payload[5];
                
                Serial1.print("INIT - Driver:");
                Serial1.print(driver);
                Serial1.print(" Size:");
                Serial1.print(displayWidth);
                Serial1.print("x");
                Serial1.print(displayHeight);
                Serial1.print(" Rot:");
                Serial1.println(rotation);
                
                initialized = true;
                sendAck();
            } else {
                Serial1.println("INIT - ERROR");
                sendNak();
                errorCount++;
            }
            break;
            
        case CMD_CLEAR:
            if (len >= 2) {
                uint16_t color = payload[0] | (payload[1] << 8);
                Serial1.print("CLEAR - Color:0x");
                Serial1.print(color, HEX);
                
                fb.clear(color);
                
                uint32_t elapsed = micros() - start;
                Serial1.print(" (");
                Serial1.print(elapsed);
                Serial1.println(" us)");
                
                sendAck();
            } else {
                Serial1.println("CLEAR - ERROR");
                sendNak();
                errorCount++;
            }
            break;
            
        case CMD_RECT:
            if (len >= 10) {
                uint16_t x = payload[0] | (payload[1] << 8);
                uint16_t y = payload[2] | (payload[3] << 8);
                uint16_t w = payload[4] | (payload[5] << 8);
                uint16_t h = payload[6] | (payload[7] << 8);
                uint16_t color = payload[8] | (payload[9] << 8);
                
                Serial1.print("RECT - x:");
                Serial1.print(x);
                Serial1.print(" y:");
                Serial1.print(y);
                Serial1.print(" w:");
                Serial1.print(w);
                Serial1.print(" h:");
                Serial1.print(h);
                Serial1.print(" c:0x");
                Serial1.print(color, HEX);
                
                fb.fillRect(x, y, w, h, color);
                
                uint32_t elapsed = micros() - start;
                Serial1.print(" (");
                Serial1.print(elapsed);
                Serial1.println(" us)");
                
                sendAck();
            } else {
                Serial1.println("RECT - ERROR");
                sendNak();
                errorCount++;
            }
            break;
            
        case CMD_CIRCLE:
            if (len >= 8) {
                uint16_t x = payload[0] | (payload[1] << 8);
                uint16_t y = payload[2] | (payload[3] << 8);
                uint16_t r = payload[4] | (payload[5] << 8);
                uint16_t color = payload[6] | (payload[7] << 8);
                
                Serial1.print("CIRCLE - x:");
                Serial1.print(x);
                Serial1.print(" y:");
                Serial1.print(y);
                Serial1.print(" r:");
                Serial1.print(r);
                Serial1.print(" c:0x");
                Serial1.print(color, HEX);
                
                fb.fillCircle(x, y, r, color);
                
                uint32_t elapsed = micros() - start;
                Serial1.print(" (");
                Serial1.print(elapsed);
                Serial1.println(" us)");
                
                sendAck();
            } else {
                Serial1.println("CIRCLE - ERROR");
                sendNak();
                errorCount++;
            }
            break;
            
        case CMD_LINE:
            if (len >= 10) {
                uint16_t x0 = payload[0] | (payload[1] << 8);
                uint16_t y0 = payload[2] | (payload[3] << 8);
                uint16_t x1 = payload[4] | (payload[5] << 8);
                uint16_t y1 = payload[6] | (payload[7] << 8);
                uint16_t color = payload[8] | (payload[9] << 8);
                
                Serial1.print("LINE - (");
                Serial1.print(x0);
                Serial1.print(",");
                Serial1.print(y0);
                Serial1.print(")→(");
                Serial1.print(x1);
                Serial1.print(",");
                Serial1.print(y1);
                Serial1.print(") c:0x");
                Serial1.print(color, HEX);
                
                fb.drawLine(x0, y0, x1, y1, color);
                
                uint32_t elapsed = micros() - start;
                Serial1.print(" (");
                Serial1.print(elapsed);
                Serial1.println(" us)");
                
                sendAck();
            } else {
                Serial1.println("LINE - ERROR");
                sendNak();
                errorCount++;
            }
            break;
            
        case CMD_FLUSH:
            Serial1.print("FLUSH - Frame ");
            Serial1.print(commandCount);
            Serial1.println(" complete");
            
            // Print framebuffer stats
            Serial1.print("  Draw calls: ");
            Serial1.println(fb.getDrawCalls());
            Serial1.print("  Pixels: ");
            Serial1.println(fb.getPixelsDrawn());
            
            fb.resetStats();
            sendAck();
            break;
            
        default:
            Serial1.print("UNKNOWN CMD: 0x");
            Serial1.println(cmd, HEX);
            sendNak();
            errorCount++;
            break;
    }
    
    digitalWrite(PIN_LED_RX, LOW);
}

// ==============================================================================
// Packet Parser
// ==============================================================================

enum ParserState {
    WAIT_HEADER,
    READ_LENGTH,
    READ_COMMAND,
    READ_PAYLOAD
};

ParserState state = WAIT_HEADER;
uint8_t payloadLength = 0;
uint8_t command = 0;
uint8_t payload[256];
uint8_t payloadIndex = 0;

void processIncomingData() {
    while (Serial.available()) {
        uint8_t byte = Serial.read();
        rxByteCount++;
        
        switch (state) {
            case WAIT_HEADER:
                if (byte == SPRITE_HEADER) {
                    state = READ_LENGTH;
                }
                break;
                
            case READ_LENGTH:
                payloadLength = byte;
                state = READ_COMMAND;
                break;
                
            case READ_COMMAND:
                command = byte;
                payloadIndex = 0;
                
                if (payloadLength == 0) {
                    processCommand(command, nullptr, 0);
                    state = WAIT_HEADER;
                } else {
                    state = READ_PAYLOAD;
                }
                break;
                
            case READ_PAYLOAD:
                payload[payloadIndex++] = byte;
                
                if (payloadIndex >= payloadLength) {
                    processCommand(command, payload, payloadLength);
                    state = WAIT_HEADER;
                }
                break;
        }
    }
}

// ==============================================================================
// Setup & Loop
// ==============================================================================

void setup() {
    // Serial for communication
    Serial.begin(115200);
    
    // Serial1 for debugging
    Serial1.begin(115200);
    delay(500);
    
    Serial1.println();
    Serial1.println("========================================");
    Serial1.println("  Sprite One - Firmware v2.1");
    Serial1.println("  Graphics Engine Edition");
    Serial1.println("========================================");
    Serial1.println();
    
    // Configure LEDs
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_RX, OUTPUT);
    
    // Boot sequence
    for (int i = 0; i < 3; i++) {
        digitalWrite(PIN_LED_STATUS, HIGH);
        delay(100);
        digitalWrite(PIN_LED_STATUS, LOW);
        delay(100);
    }
    
    // Initialize framebuffer
    Serial1.println("[INIT] Initializing graphics...");
    if (!fb.begin()) {
        Serial1.println("[ERROR] Framebuffer init failed!");
        while (1) {
            digitalWrite(PIN_LED_STATUS, HIGH);
            delay(200);
            digitalWrite(PIN_LED_STATUS, LOW);
            delay(200);
        }
    }
    
    Serial1.println("[READY] Sprite One ready!");
    Serial1.println("[READY] Waiting for commands...");
    Serial1.println();
}

void loop() {
    // Process incoming data
    processIncomingData();
    
    // Heartbeat LED (1 Hz)
    uint32_t now = millis();
    if (now - lastHeartbeat >= 1000) {
        digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
        lastHeartbeat = now;
        
        // Print statistics every 10 seconds
        static uint32_t lastStats = 0;
        if (now - lastStats >= 10000) {
            Serial1.println();
            Serial1.println("=== Statistics ===");
            Serial1.print("Commands: ");
            Serial1.println(commandCount);
            Serial1.print("RX bytes: ");
            Serial1.println(rxByteCount);
            Serial1.print("TX bytes: ");
            Serial1.println(txByteCount);
            Serial1.print("Errors: ");
            Serial1.println(errorCount);
            Serial1.println();
            
            lastStats = now;
        }
    }
}
