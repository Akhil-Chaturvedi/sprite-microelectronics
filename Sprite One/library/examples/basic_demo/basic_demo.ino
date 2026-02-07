/**
 * Sprite One - Basic Demo
 * 
 * This example shows the simplest possible usage of the Sprite library.
 * Connect your Sprite One module via SPI and see hardware-accelerated graphics!
 * 
 * Wiring (Arduino Uno/Nano):
 *   Arduino     Sprite One
 *   -------     ----------
 *   D10  (CS)   CS
 *   D11  (MOSI) MOSI
 *   D12  (MISO) MISO
 *   D13  (SCK)  SCK
 *   5V          VCC
 *   GND         GND
 * 
 * License: MIT
 */

#include <Sprite.h>

// Create Sprite instance
Sprite gfx;

// Animation variables
int16_t ballX = 50;
int16_t ballY = 50;
int8_t ballDX = 3;
int8_t ballDY = 2;
const int16_t ballSize = 20;

// Display size
const int16_t WIDTH = 320;
const int16_t HEIGHT = 240;

void setup() {
    Serial.begin(115200);
    Serial.println("Sprite One - Basic Demo");
    
    // Initialize Sprite library (CS on pin 10)
    gfx.begin(10);
    
    // Initialize display (ILI9341, 320x240, no rotation)
    gfx.initDisplay(DISPLAY_ILI9341, WIDTH, HEIGHT, 0);
    
    // Check connection
    if (gfx.isConnected()) {
        Serial.println("Sprite One connected!");
    } else {
        Serial.println("Error: Sprite One not found!");
        while (1) delay(100);
    }
    
    // Clear screen
    gfx.clear(BLACK);
    gfx.flush();
    
    delay(500);
}

void loop() {
    // === Clear previous frame ===
    gfx.clear(BLACK);
    
    // === Draw border ===
    gfx.rectOutline(0, 0, WIDTH, HEIGHT, GRAY);
    
    // === Update ball position ===
    ballX += ballDX;
    ballY += ballDY;
    
    // Bounce off walls
    if (ballX <= 0 || ballX >= WIDTH - ballSize) {
        ballDX = -ballDX;
        ballX = constrain(ballX, 0, WIDTH - ballSize);
    }
    if (ballY <= 0 || ballY >= HEIGHT - ballSize) {
        ballDY = -ballDY;
        ballY = constrain(ballY, 0, HEIGHT - ballSize);
    }
    
    // === Draw ball ===
    gfx.rect(ballX, ballY, ballSize, ballSize, RED);
    
    // === Draw some rectangles ===
    gfx.rect(10, 10, 40, 30, GREEN);
    gfx.rect(WIDTH - 50, 10, 40, 30, BLUE);
    gfx.rect(10, HEIGHT - 40, 40, 30, YELLOW);
    gfx.rect(WIDTH - 50, HEIGHT - 40, 40, 30, CYAN);
    
    // === Draw text ===
    gfx.text(WIDTH/2 - 60, 10, "Sprite One", WHITE);
    
    // === Push to display ===
    gfx.flush();
    
    // Small delay for ~60 FPS
    delay(16);
}
