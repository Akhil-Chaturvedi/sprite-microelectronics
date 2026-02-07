/*
 * Sprite One - AI Protocol Handler
 * Week 3 Day 21: Protocol Integration
 * 
 * Handles AI commands from host:
 * - CMD_AI_INFER (0x50): Run inference
 * - CMD_AI_TRAIN (0x51): Train model  
 * - CMD_AI_SAVE (0x53): Save to flash
 * - CMD_AI_LOAD (0x54): Load from flash
 * - CMD_AI_LIST (0x55): List models
 */

#ifndef SPRITE_AI_PROTOCOL_H
#define SPRITE_AI_PROTOCOL_H

#include <Arduino.h>
#include <aifes.h>

// AI Protocol Response Codes
#define AI_RESP_OK          0x00
#define AI_RESP_ERROR       0x01
#define AI_RESP_BUSY        0x02
#define AI_RESP_NOT_FOUND   0x03
#define AI_RESP_INVALID     0x04

// AI Engine States
#define AI_STATE_IDLE       0x00
#define AI_STATE_TRAINING   0x01
#define AI_STATE_INFERRING  0x02
#define AI_STATE_SAVING     0x03
#define AI_STATE_LOADING    0x04

// Model slot configuration
#define AI_MAX_MODELS       4
#define AI_MODEL_NAME_LEN   32

// AI Status structure
typedef struct {
  uint8_t state;
  uint8_t model_loaded;
  uint8_t model_type;      // 0=F32, 1=Q7
  uint16_t epochs_done;
  float last_loss;
  float last_inference[4]; // Last 4 outputs
} AIStatus;

// AI Protocol handler class
class SpriteAI {
public:
  SpriteAI();
  
  // Initialize AI engine
  bool begin();
  
  // Handle incoming command
  // Returns response code and fills response buffer
  uint8_t handleCommand(uint8_t cmd, const uint8_t* payload, uint8_t len,
                        uint8_t* response, uint8_t* resp_len);
  
  // Get current status
  void getStatus(AIStatus* status);
  
  // Direct API (for internal use)
  bool loadModel(const char* filename);
  bool saveModel(const char* filename);
  bool runInference(float* inputs, uint8_t num_inputs, 
                    float* outputs, uint8_t num_outputs);
  bool trainStep(float* inputs, float* targets, uint8_t samples);
  
private:
  AIStatus status;
  bool model_ready;
  
  // Command handlers
  uint8_t cmdInfer(const uint8_t* payload, uint8_t len, uint8_t* resp, uint8_t* resp_len);
  uint8_t cmdTrain(const uint8_t* payload, uint8_t len, uint8_t* resp, uint8_t* resp_len);
  uint8_t cmdSave(const uint8_t* payload, uint8_t len, uint8_t* resp, uint8_t* resp_len);
  uint8_t cmdLoad(const uint8_t* payload, uint8_t len, uint8_t* resp, uint8_t* resp_len);
  uint8_t cmdList(const uint8_t* payload, uint8_t len, uint8_t* resp, uint8_t* resp_len);
  uint8_t cmdDelete(const uint8_t* payload, uint8_t len, uint8_t* resp, uint8_t* resp_len);
  uint8_t cmdStatus(const uint8_t* payload, uint8_t len, uint8_t* resp, uint8_t* resp_len);
};

// Global instance (extern, defined in .cpp)
extern SpriteAI aiEngine;

#endif // SPRITE_AI_PROTOCOL_H
