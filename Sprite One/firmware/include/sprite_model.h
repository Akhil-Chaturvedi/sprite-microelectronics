/*
 * Sprite Model Manager
 * Hot-swappable model system with on-device fine-tuning
 */

#ifndef SPRITE_MODEL_H
#define SPRITE_MODEL_H

#include <Arduino.h>
#include <LittleFS.h>

// Model header format (32 bytes, aligned)
#define MODEL_MAGIC 0x54525053  // "SPRT"
#define MODEL_VERSION 0x0001

struct ModelHeader {
  uint32_t magic;          // Must be MODEL_MAGIC
  uint16_t version;        // Format version
  uint8_t  input_size;     // Number of inputs
  uint8_t  output_size;    // Number of outputs
  uint8_t  hidden_size;    // Hidden layer size
  uint8_t  model_type;     // 0=F32, 1=Q7
  uint16_t reserved;
  uint32_t weights_crc;    // CRC32 of weight data
  char     name[16];       // Model name (null-terminated)
  // Total: 32 bytes
};

// Model types
#define MODEL_TYPE_F32  0
#define MODEL_TYPE_Q7   1

class ModelManager {
private:
  char active_model_path[32];
  ModelHeader active_header;
  bool has_active_model;
  
  // Calculate CRC32 (simple implementation)
  uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
      crc ^= data[i];
      for (int j = 0; j < 8; j++) {
        crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
      }
    }
    return ~crc;
  }
  
  // Validate model header
  bool validateHeader(const ModelHeader& hdr) {
    if (hdr.magic != MODEL_MAGIC) return false;
    if (hdr.version != MODEL_VERSION) return false;
    if (hdr.input_size == 0 || hdr.output_size == 0) return false;
    if (hdr.model_type > MODEL_TYPE_Q7) return false;
    return true;
  }
  
public:
  ModelManager() : has_active_model(false) {
    active_model_path[0] = '\0';
  }
  
  // List all model files
  uint8_t listModels(char models[][32], uint8_t max_models) {
    uint8_t count = 0;
    File root = LittleFS.open("/models", "r");
    if (!root || !root.isDirectory()) {
      LittleFS.mkdir("/models");  // Create if doesn't exist
      return 0;
    }
    
    File file = root.openNextFile();
    while (file && count < max_models) {
      if (!file.isDirectory() && strstr(file.name(), ".aif32")) {
        strncpy(models[count], file.name(), 31);
        models[count][31] = '\0';
        count++;
      }
      file = root.openNextFile();
    }
    
    return count;
  }
  
  // Get info about a model file
  bool getModelInfo(const char* filename, ModelHeader* header) {
    char path[48];
    snprintf(path, sizeof(path), "/models/%s", filename);
    
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    
    if (f.read((uint8_t*)header, sizeof(ModelHeader)) != sizeof(ModelHeader)) {
      f.close();
      return false;
    }
    
    f.close();
    return validateHeader(*header);
  }
  
  // Select active model
  bool selectModel(const char* filename) {
    ModelHeader hdr;
    if (!getModelInfo(filename, &hdr)) {
      return false;
    }
    
    snprintf(active_model_path, sizeof(active_model_path), "/models/%s", filename);
    memcpy(&active_header, &hdr, sizeof(ModelHeader));
    has_active_model = true;
    
    return true;
  }
  
  // Get active model info
  bool getActiveInfo(ModelHeader* header) {
    if (!has_active_model) return false;
    memcpy(header, &active_header, sizeof(ModelHeader));
    return true;
  }
  
  const char* getActivePath() {
    return has_active_model ? active_model_path : nullptr;
  }
  
  // Delete model
  bool deleteModel(const char* filename) {
    char path[48];
    snprintf(path, sizeof(path), "/models/%s", filename);
    
    // Don't delete active model
    if (has_active_model && strcmp(path, active_model_path) == 0) {
      return false;
    }
    
    return LittleFS.remove(path);
  }
  
  // Upload model (write in chunks)
  bool uploadModel(const char* filename, const uint8_t* data, size_t len) {
    // Write to temporary file first
    char temp_path[48];
    snprintf(temp_path, sizeof(temp_path), "/models/%s.tmp", filename);
    
    File f = LittleFS.open(temp_path, "w");
    if (!f) return false;
    
    size_t written = f.write(data, len);
    f.close();
    
    if (written != len) {
      LittleFS.remove(temp_path);
      return false;
    }
    
    // Validate header
    ModelHeader hdr;
    File validate = LittleFS.open(temp_path, "r");
    if (!validate || validate.read((uint8_t*)&hdr, sizeof(ModelHeader)) != sizeof(ModelHeader)) {
      if (validate) validate.close();
      LittleFS.remove(temp_path);
      return false;
    }
    validate.close();
    
    if (!validateHeader(hdr)) {
      LittleFS.remove(temp_path);
      return false;
    }
    
    // Atomic rename
    char final_path[48];
    snprintf(final_path, sizeof(final_path), "/models/%s", filename);
    LittleFS.remove(final_path);  // Remove old if exists
    LittleFS.rename(temp_path, final_path);
    
    return true;
  }
  
  // Check if has active model
  bool hasActive() const {
    return has_active_model;
  }
};

#endif // SPRITE_MODEL_H
