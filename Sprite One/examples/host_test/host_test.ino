/**
 * Host Test - Serial Version (Wokwi Compatible)
 * 
 * Sends test commands to Sprite One via Serial
 * Simulates what the real SPI communication will do
 */

// Protocol constants
#define SPRITE_HEADER 0xAA
#define SPRITE_ACK    0x00
#define SPRITE_NAK    0x01

// Commands
#define CMD_NOP       0x00
#define CMD_PING      0x02
#define CMD_GET_INFO  0x03
#define CMD_INIT      0x01
#define CMD_CLEAR     0x10
#define CMD_RECT      0x12
#define CMD_CIRCLE    0x15
#define CMD_TEXT      0x14
#define CMD_FLUSH     0x2F

// Global state
uint32_t testNumber = 0;

void setup() {
    Serial.begin(115200);
    Serial1.begin(115200);
    delay(1000);
    
    Serial1.println("========================================");
    Serial1.println("  Sprite One - Host Test (Serial)");
    Serial1.println("========================================");
    Serial1.println();
    
    delay(1000);
    
    runAllTests();
}

void loop() {
    // Run animation loop
    static uint16_t x = 0;
    static uint16_t y = 50;
    static int16_t dx = 5;
    static int16_t dy = 3;
    static uint32_t lastFrame = 0;
    
    uint32_t now = millis();
    if (now - lastFrame >= 100) {  // 10 FPS
        // Clear screen
        sendClear(0x0000);
        
        // Draw bouncing rectangle
        sendRect(x, y, 40, 40, 0xF800); // Red square
        
        // Draw border
        sendRect(0, 0, 320, 10, 0x07E0);      // Top - Green
        sendRect(0, 230, 320, 10, 0x07E0);    // Bottom - Green
        sendRect(0, 0, 10, 240, 0x07E0);      // Left - Green
        sendRect(310, 0, 10, 240, 0x07E0);    // Right - Green
        
        // Flush frame
        sendFlush();
        
        // Update position
        x += dx;
        y += dy;
        
        // Bounce off walls
        if (x >= 270 || x <= 10) dx = -dx;
        if (y >= 190 || y <= 10) dy = -dy;
        
        lastFrame = now;
    }
}

// ==================== Helper Functions ====================

void sendCommand(uint8_t cmd, uint8_t* payload, uint8_t len) {
    Serial.write(SPRITE_HEADER);
    Serial.write(len);
    Serial.write(cmd);
    
    for (uint8_t i = 0; i < len; i++) {
        Serial.write(payload[i]);
    }
    
    delay(10); // Give Sprite One time to process
}

uint8_t readResponse() {
    uint32_t start = millis();
    while (!Serial.available() && (millis() - start < 1000)) {
        delay(1);
    }
    
    if (Serial.available()) {
        return Serial.read();
    }
    return 0xFF; // Timeout
}

void printTest(const char* name) {
    testNumber++;
    Serial1.print("[TEST ");
    Serial1.print(testNumber);
    Serial1.print("] ");
    Serial1.println(name);
}

void printResult(bool pass) {
    if (pass) {
        Serial1.println("  ✓ PASS");
    } else {
        Serial1.println("  ✗ FAIL");
    }
    Serial1.println();
}

// ==================== Command Functions ====================

void sendInit(uint8_t driver, uint16_t width, uint16_t height, uint8_t rotation) {
    uint8_t payload[6];
    payload[0] = driver;
    payload[1] = width & 0xFF;
    payload[2] = width >> 8;
    payload[3] = height & 0xFF;
    payload[4] = height >> 8;
    payload[5] = rotation;
    
    sendCommand(CMD_INIT, payload, 6);
}

void sendClear(uint16_t color) {
    uint8_t payload[2];
    payload[0] = color & 0xFF;
    payload[1] = color >> 8;
    
    sendCommand(CMD_CLEAR, payload, 2);
}

void sendRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    uint8_t payload[10];
    payload[0] = x & 0xFF;
    payload[1] = x >> 8;
    payload[2] = y & 0xFF;
    payload[3] = y >> 8;
    payload[4] = w & 0xFF;
    payload[5] = w >> 8;
    payload[6] = h & 0xFF;
    payload[7] = h >> 8;
    payload[8] = color & 0xFF;
    payload[9] = color >> 8;
    
    sendCommand(CMD_RECT, payload, 10);
}

void sendCircle(uint16_t x, uint16_t y, uint16_t r, uint16_t color) {
    uint8_t payload[8];
    payload[0] = x & 0xFF;
    payload[1] = x >> 8;
    payload[2] = y & 0xFF;
    payload[3] = y >> 8;
    payload[4] = r & 0xFF;
    payload[5] = r >> 8;
    payload[6] = color & 0xFF;
    payload[7] = color >> 8;
    
    sendCommand(CMD_CIRCLE, payload, 8);
}

void sendText(uint16_t x, uint16_t y, uint16_t color, const char* text) {
    uint8_t len = strlen(text);
    uint8_t payload[256];
    payload[0] = x & 0xFF;
    payload[1] = x >> 8;
    payload[2] = y & 0xFF;
    payload[3] = y >> 8;
    payload[4] = color & 0xFF;
    payload[5] = color >> 8;
    
    for (uint8_t i = 0; i < len; i++) {
        payload[6 + i] = text[i];
    }
    
    sendCommand(CMD_TEXT, payload, 6 + len);
}

void sendFlush() {
    sendCommand(CMD_FLUSH, nullptr, 0);
}

// ==================== Test Functions ====================

void runAllTests() {
    Serial1.println("Starting test sequence...");
    Serial1.println();
    
    delay(500);
    
    // Test 1: Ping
    printTest("PING Test");
    sendCommand(CMD_PING, nullptr, 0);
    uint8_t resp = readResponse();
    printResult(resp == SPRITE_ACK);
    
    delay(500);
    
    // Test 2: Initialize
    printTest("Initialize Display");
    sendInit(0x01, 320, 240, 0);
    resp = readResponse();
    printResult(resp == SPRITE_ACK);
    
    delay(500);
    
    // Test 3: Clear Screen
    printTest("Clear Screen");
    sendClear(0x0000); // Black
    resp = readResponse();
    printResult(resp == SPRITE_ACK);
    
    delay(500);
    
    // Test 4: Draw Rectangles
    printTest("Draw Rectangles");
    sendRect(10, 10, 100, 50, 0xF800);   // Red
    sendRect(120, 10, 100, 50, 0x07E0);  // Green
    sendRect(230, 10, 80,50, 0x001F);   // Blue
    Serial1.println("  ✓ Commands sent");
    Serial1.println();
    
    delay(500);
    
    // Test 5: Draw Circles
    printTest("Draw Circles");
    sendCircle(160, 120, 50, 0xFFE0);    // Yellow
    sendCircle(160, 120, 30, 0xF81F);    // Magenta
    Serial1.println("  ✓ Commands sent");
    Serial1.println();
    
    delay(500);
    
    // Test 6: Draw Text
    printTest("Draw Text");
    sendText(10, 200, 0xFFFF, "Sprite One v2.0");
    Serial1.println("  ✓ Command sent");
    Serial1.println();
    
    delay(500);
    
    // Test 7: Flush
    printTest("Flush Frame");
    sendFlush();
    resp = readResponse();
    printResult(resp == SPRITE_ACK);
    
    Serial1.println("========================================");
    Serial1.println("  All tests complete!");
    Serial1.println("  Starting animation...");
    Serial1.println("========================================");
    Serial1.println();
}
