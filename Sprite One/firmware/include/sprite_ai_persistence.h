/*
 * Sprite One - AI Model Persistence
 * LittleFS-based storage for trained neural networks
 * 
 * Features:
 * - Save/load F32 and Q7 models
 * - File-based named storage
 * - CRC32 integrity checking
 * - Multiple model support
 */

#ifndef SPRITE_AI_PERSISTENCE_H
#define SPRITE_AI_PERSISTENCE_H

#include <Arduino.h>
#include <aifes.h>

// Model file magic number: 'AIFE'
#define AI_MODEL_MAGIC 0x41494645

// Model type identifiers
#define AI_MODEL_TYPE_F32 0
#define AI_MODEL_TYPE_Q7  1

// Current format version
#define AI_MODEL_VERSION 1

// Model header structure (64 bytes)
typedef struct {
  uint32_t magic;           // Magic number (0x41494645)
  uint32_t version;         // Format version
  uint32_t model_type;      // 0=F32, 1=Q7
  uint32_t param_size;      // Size of parameter data
  uint32_t checksum;        // CRC32 of parameters
  uint32_t layer_count;     // Number of layers
  char name[32];            // Model name
  uint8_t reserved[8];      // Future use
} AIModelHeader;

// Model info structure for queries
typedef struct {
  char filename[64];
  char name[32];
  uint32_t model_type;
  uint32_t param_size;
  uint32_t layer_count;
  bool valid;
} AIModelInfo;

// Initialize the filesystem (called automatically if needed)
bool ai_persistence_init();

// Save a model to flash
// filename: full path like "/xor.aif32"
// model: pointer to compiled AIfES model
// is_q7: true for Q7 models, false for F32
// Returns: true on success
bool ai_save_model(const char* filename, aimodel_t* model, bool is_q7 = false);

// Load a model from flash
// filename: full path like "/xor.aif32"
// param_buffer: buffer to receive model parameters (must be large enough!)
// buffer_size: size of param_buffer
// Returns: loaded parameter size, or 0 on failure
uint32_t ai_load_model_params(const char* filename, void* param_buffer, uint32_t buffer_size);

// Check if a model file exists
bool ai_model_exists(const char* filename);

// Get model info without loading full parameters
bool ai_get_model_info(const char* filename, AIModelInfo* info);

// Delete a saved model
bool ai_delete_model(const char* filename);

// List all saved models to a Print interface (Serial, etc.)
void ai_list_models(Print& output);

// Get free filesystem space in bytes
uint32_t ai_get_free_space();

// Format the filesystem (erases all models!)
bool ai_format_filesystem();

// CRC32 utility (for verification)
uint32_t ai_crc32(const void* data, size_t length);

#endif // SPRITE_AI_PERSISTENCE_H
