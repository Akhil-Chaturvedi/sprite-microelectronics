/**
 * Sprite One - Graphics Benchmark
 * 
 * Tests rendering performance of all primitives
 * Target: 60 FPS = 16.6ms per frame
 */

#include "framebuffer.h"
#include "sprite.h"

Framebuffer fb(320, 240);

// Benchmark results
struct BenchmarkResults {
    uint32_t clear_us;
    uint32_t rect_us;
    uint32_t line_us;
    uint32_t circle_us;
    uint32_t sprite_us;
    uint32_t total_frame_us;
    uint16_t fps;
};

BenchmarkResults results;

void setup() {
    Serial.begin(115200);
    Serial1.begin(115200);
    delay(1000);
    
    Serial1.println("========================================");
    Serial1.println("  Sprite One - Graphics Benchmark");
    Serial1.println("========================================");
    Serial1.println();
    
    // Initialize framebuffer
    if (!fb.begin()) {
        Serial1.println("[ERROR] Framebuffer init failed!");
        while(1);
    }
    
    Serial1.println("[READY] Starting benchmarks...");
    Serial1.println();
    
    runBenchmarks();
    printResults();
}

void loop() {
    // Test realistic scene rendering
    static uint32_t lastFrame = 0;
    static uint16_t frame_count = 0;
    static uint16_t x = 0, y = 60;
    static int16_t dx = 2, dy = 1;
    
    uint32_t now = millis();
    if (now - lastFrame >= 16) {  // Target 60 FPS
        uint32_t frame_start = micros();
        
        // Clear
        fb.clear(COLOR_BLACK);
        
        // Draw border
        fb.drawRect(0, 0, 320, 240, COLOR_GREEN);
        
        // Draw bouncing sprite
        SpriteRenderer::blitTransparent(&fb, &SPRITE_SMILEY_16x16, x, y);
        
        // Update position
        x += dx;
        y += dy;
        if (x >= 304 || x <= 0) dx = -dx;
        if (y >= 224 || y <= 0) dy = -dy;
        
        uint32_t frame_time = micros() - frame_start;
        
        // Print stats every 60 frames
        if (frame_count % 60 == 0) {
            Serial1.print("Frame: ");
            Serial1.print(frame_time);
            Serial1.print(" us, FPS: ");
            Serial1.println(1000000 / frame_time);
        }
        
        frame_count++;
        lastFrame = now;
    }
}

void runBenchmarks() {
    uint32_t start;
    
    // Benchmark 1: Clear Screen
    Serial1.print("[1/5] Clear screen... ");
    start = micros();
    for (int i = 0; i < 100; i++) {
        fb.clear(COLOR_BLACK);
    }
    results.clear_us = (micros() - start) / 100;
    Serial1.print(results.clear_us);
    Serial1.println(" us/op");
    
    // Benchmark 2: Fill Rectangle
    Serial1.print("[2/5] Fill rectangle (100x100)... ");
    start = micros();
    for (int i = 0; i < 100; i++) {
        fb.fillRect(10, 10, 100, 100, COLOR_RED);
    }
    results.rect_us = (micros() - start) / 100;
    Serial1.print(results.rect_us);
    Serial1.println(" us/op");
    
    // Benchmark 3: Draw Line
    Serial1.print("[3/5] Draw line (100px diagonal)... ");
    start = micros();
    for (int i = 0; i < 100; i++) {
        fb.drawLine(0, 0, 100, 100, COLOR_WHITE);
    }
    results.line_us = (micros() - start) / 100;
    Serial1.print(results.line_us);
    Serial1.println(" us/op");
    
    // Benchmark 4: Draw Circle
    Serial1.print("[4/5] Fill circle (r=50)... ");
    start = micros();
    for (int i = 0; i < 100; i++) {
        fb.fillCircle(160, 120, 50, COLOR_BLUE);
    }
    results.circle_us = (micros() - start) / 100;
    Serial1.print(results.circle_us);
    Serial1.println(" us/op");
    
    // Benchmark 5: Blit Sprite
    Serial1.print("[5/5] Blit sprite (16x16 transparent)... ");
    start = micros();
    for (int i = 0; i < 100; i++) {
        SpriteRenderer::blitTransparent(&fb, &SPRITE_SMILEY_16x16, 100, 100);
    }
    results.sprite_us = (micros() - start) / 100;
    Serial1.print(results.sprite_us);
    Serial1.println(" us/op");
    
    // Benchmark 6: Realistic Frame
    Serial1.print("[6/6] Realistic frame (clear + 10 sprites)... ");
    start = micros();
    for (int i = 0; i < 10; i++) {
        fb.clear(COLOR_BLACK);
        for (int j = 0; j < 10; j++) {
            SpriteRenderer::blitTransparent(&fb, &SPRITE_SMILEY_16x16, j * 20, j * 20);
        }
    }
    results.total_frame_us = (micros() - start) / 10;
    results.fps = 1000000 / results.total_frame_us;
    Serial1.print(results.total_frame_us);
    Serial1.print(" us/frame, ");
    Serial1.print(results.fps);
    Serial1.println(" FPS");
}

void printResults() {
    Serial1.println();
    Serial1.println("========================================");
    Serial1.println("  Benchmark Results");
    Serial1.println("========================================");
    Serial1.println();
    
    Serial1.println("Primitive Performance:");
    Serial1.print("  Clear (320x240):     ");
    Serial1.print(results.clear_us);
    Serial1.println(" us");
    Serial1.print("  Rect (100x100):      ");
    Serial1.print(results.rect_us);
    Serial1.println(" us");
    Serial1.print("  Line (100px):        ");
    Serial1.print(results.line_us);
    Serial1.println(" us");
    Serial1.print("  Circle (r=50):       ");
    Serial1.print(results.circle_us);
    Serial1.println(" us");
    Serial1.print("  Sprite (16x16):      ");
    Serial1.print(results.sprite_us);
    Serial1.println(" us");
    Serial1.println();
    
    Serial1.println("Frame Performance:");
    Serial1.print("  Frame time:          ");
    Serial1.print(results.total_frame_us);
    Serial1.println(" us");
    Serial1.print("  FPS:                 ");
    Serial1.println(results.fps);
    Serial1.println();
    
    if (results.fps >= 60) {
        Serial1.println("✅ TARGET ACHIEVED: 60+ FPS!");
    } else if (results.fps >= 30) {
        Serial1.println("⚠️  GOOD: 30+ FPS (playable)");
    } else {
        Serial1.println("❌ NEEDS OPTIMIZATION: <30 FPS");
    }
    
    Serial1.println();
    Serial1.println("========================================");
}
