/**
 * Sprite One - AIfES XOR Test (Simplified)
 * 
 * Week 3 Day 15-16: Verify AIfES compiles on RP2040
 * 
 * Based on AIfES examples from Fraunhofer IMS
 */

#include <aifes.h>  // Main AIfES header

void setup() {
    Serial.begin(115200);
    Serial1.begin(115200);
    delay(2000);
    
    Serial1.println("========================================");
    Serial1.println("  Sprite One - AIfES Compilation Test");
    Serial1.println("========================================");
    Serial1.println();
    
    // Simple tensor test to verify library loaded
    float test_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint16_t test_shape[2] = {2, 2};  // 2x2 matrix
    
    aitensor_t test_tensor = {
        .dtype = aif32,
        .dim = 2,
        .shape = test_shape,
        .data = test_data
    };
    
    Serial1.println("[INFO] AIfES Library Loaded!");
    Serial1.print("  Tensor dims: ");
    Serial1.println(test_tensor.dim);
    Serial1.print("  Tensor shape: [");
    Serial1.print(test_shape[0]);
    Serial1.print(", ");
    Serial1.print(test_shape[1]);
    Serial1.println("]");
    Serial1.print("  Tensor data: [");
    Serial1.print(test_data[0], 1);
    Serial1.print(", ");
    Serial1.print(test_data[1], 1);
    Serial1.print(", ");
    Serial1.print(test_data[2], 1);
    Serial1.print(", ");
    Serial1.print(test_data[3], 1);
    Serial1.println("]");
    
    Serial1.println();
    Serial1.println("========================================");
    Serial1.println("  ✅ AIfES COMPILATION SUCCESSFUL!");
    Serial1.println("  Library ready for neural networks!");
    Serial1.println("========================================");
    Serial1.println();
    
    Serial1.println("Next: Implement XOR training!");
}

void loop() {
    delay(1000);
}
