/*
 * sentinel_model.h
 * Hybrid Flash/RAM Model Architecture
 * Defines the "Vector Brain" and "Plastic Head" structures.
 */

#ifndef SENTINEL_MODEL_H
#define SENTINEL_MODEL_H

#include <stdint.h>

// Vector Brain Configuration
#define VECTOR_DIM 128
#define MAX_VECTORS 16
#define VECTOR_STORE_MAGIC 0xBEEFCAFE

// Tensor Types
typedef struct {
    uint16_t dims[2]; // [Rows, Cols]
    float*   data;    // Pointer to data (Flash or RAM)
} Tensor;

// Output Head Descriptor
// Hardcoded for "Sentinel God V1" since we stripped metadata from file
struct SentinelConfig {
    static const int NUM_HEADS = 5;
    
    // Head 0: Classification (10)
    // Head 1: Depth (28x28)
    // Head 2: Vector (128)
    // Head 3: Saliency (7x7)
    // Head 4: Anomaly (1)
};

// Simplified parser for V1
class SentinelModelParser {
public:
    // With V1, we just verify the magic number
    bool parse(const uint8_t* model_data) {
        uint32_t magic = *(uint32_t*)model_data;
        return (magic == 0x54525053); // "SPRT"
    }
};

// The "Vector Store" - A simple associative memory
// Stores "Fingerprints" of known objects
struct VectorEntry {
    uint8_t id;
    uint8_t confidence; // 0-100
    char    label[16];  // "Cat", "Mailman", etc.
    float   embedding[VECTOR_DIM]; // 128-float vector
    uint32_t last_seen;
};

// Global Vector Memory (SRAM)
struct VectorSystem {
    uint32_t magic;
    uint8_t count;
    VectorEntry entries[MAX_VECTORS];
    
    // Find closest match to an input vector
    // Returns index of match, or -1 if unknown
    int find_match(const float* input_vec, float threshold = 0.8f) {
        int best_idx = -1;
        float best_score = -1.0f;
        
        for(int i=0; i<count; i++) {
            float score = cosine_similarity(input_vec, entries[i].embedding);
            if (score > best_score) {
                best_score = score;
                best_idx = i;
            }
        }
        
        if (best_score >= threshold) return best_idx;
        return -1;
    }
    
    void add_vector(const float* input_vec, const char* label) {
        if (count >= MAX_VECTORS) {
            // "Forgetting" logic: Overwrite oldest or least seen
            // Simple FIFO for now
            count = 0; 
        }
        
        VectorEntry* e = &entries[count];
        e->id = count;
        e->confidence = 100;
        e->last_seen = 0; // millis()
        strncpy(e->label, label, 15);
        memcpy(e->embedding, input_vec, sizeof(float) * VECTOR_DIM);
        
        count++;
    }
    
    // SIO-Accelerated Dot Product would go here
    float dot_product(const float* a, const float* b) {
        float sum = 0;
        for(int i=0; i<VECTOR_DIM; i++) sum += a[i] * b[i];
        return sum;
    }
    
    float magnitude(const float* a) {
        return sqrt(dot_product(a, a));
    }
    
    float cosine_similarity(const float* a, const float* b) {
        float dot = dot_product(a, b);
        float mag_a = magnitude(a);
        float mag_b = magnitude(b);
        if (mag_a == 0 || mag_b == 0) return 0;
        return dot / (mag_a * mag_b);
    }
};

#endif // SENTINEL_MODEL_H
