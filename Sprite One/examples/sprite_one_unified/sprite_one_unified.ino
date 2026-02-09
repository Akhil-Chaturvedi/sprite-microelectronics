/*
 * Sprite One - Polished Unified Demo
 * Week 4 Day 27: Enhanced with better UX!
 * v1.2: USB-CDC Transport Support
 * v1.3: Real Display Driver (SSD1306)
 * 
 * Improvements:
 * - Better error handling
 * - Informative startup sequence
 * - Progress indicators
 * - Memory monitoring
 * - Graceful degradation
 * - Dual transport (UART + USB-CDC)
 * - Real SSD1306 OLED display support
 */

#include <aifes.h>
#include <LittleFS.h>
#include "sprite_transport.h"
#include "sprite_display.h"
#include "sprite_engine.h"
#include "sprite_model.h"

// Enhanced configuration
#define SPRITE_VERSION "1.5.0"
#define ENABLE_VERBOSE_LOGGING true
#define ENABLE_PROGRESS_BARS true

// Display configuration
// 0 = Simulated (serial output only)
// 1 = SSD1306 (128x64 OLED via I2C)
#define SPRITE_DISPLAY_TYPE 1

// Protocol
#define SPRITE_HEADER       0xAA
#define CMD_VERSION         0x0F
#define CMD_BUFFER_STATUS   0x0E
#define CMD_CLEAR           0x10
#define CMD_PIXEL           0x11
#define CMD_RECT            0x12
#define CMD_TEXT            0x21
#define CMD_FLUSH           0x2F

// Sprite commands
#define CMD_SPRITE_CREATE   0x30
#define CMD_SPRITE_MOVE     0x31
#define CMD_SPRITE_DELETE   0x32
#define CMD_SPRITE_VISIBLE  0x33
#define CMD_SPRITE_COLLISION 0x34
#define CMD_SPRITE_RENDER   0x35
#define CMD_SPRITE_CLEAR    0x36

#define CMD_AI_INFER        0x50
#define CMD_AI_TRAIN        0x51
#define CMD_AI_STATUS       0x52
#define CMD_AI_SAVE         0x53
#define CMD_AI_LOAD         0x54
#define CMD_AI_LIST         0x55
#define CMD_AI_DELETE       0x56

// Model management commands
#define CMD_MODEL_INFO      0x60
#define CMD_MODEL_LIST      0x61
#define CMD_MODEL_SELECT    0x62
#define CMD_MODEL_UPLOAD    0x63
#define CMD_MODEL_DELETE    0x64
#define CMD_FINETUNE_START  0x65
#define CMD_FINETUNE_DATA   0x66
#define CMD_FINETUNE_STOP   0x67

#define RESP_OK             0x00
#define RESP_ERROR          0x01
#define RESP_NOT_FOUND      0x02

// Display (simulated)
#define DISPLAY_W 128
#define DISPLAY_H 64
uint8_t framebuffer[DISPLAY_W * DISPLAY_H / 8];

// Dirty rectangle tracking for optimized updates  
struct {
  uint16_t x1, y1, x2, y2;
  bool is_dirty;
} dirty_rect = {0, 0, 0, 0, false};

void fb_mark_dirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  if (!dirty_rect.is_dirty) {
    dirty_rect.x1 = x;
    dirty_rect.y1 = y;
    dirty_rect.x2 = x + w - 1;
    dirty_rect.y2 = y + h - 1;
    dirty_rect.is_dirty = true;
  } else {
    // Expand dirty region
    if (x < dirty_rect.x1) dirty_rect.x1 = x;
    if (y < dirty_rect.y1) dirty_rect.y1 = y;
    if (x + w - 1 > dirty_rect.x2) dirty_rect.x2 = x + w - 1;
    if (y + h - 1 > dirty_rect.y2) dirty_rect.y2 = y + h - 1;
  }
}

void fb_clear() { 
  memset(framebuffer, 0, sizeof(framebuffer)); 
  dirty_rect = {0, 0, DISPLAY_W - 1, DISPLAY_H - 1, true};
}
void fb_pixel(int16_t x, int16_t y, uint8_t color) {
  if (x < 0 || x >= DISPLAY_W || y < 0 || y >= DISPLAY_H) return;
  uint16_t byte_idx = x + (y / 8) * DISPLAY_W;
  uint8_t bit = y % 8;
  if (color) framebuffer[byte_idx] |= (1 << bit);
  else framebuffer[byte_idx] &= ~(1 << bit);
  fb_mark_dirty(x, y, 1, 1);
}
void fb_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
  fb_mark_dirty(x, y, w, h);
  for (int16_t i = x; i < x + w; i++)
    for (int16_t j = y; j < y + h; j++)
      fb_pixel(i, j, color);
}

// AI Engine
#define AI_MODEL_MAGIC 0x41494645
typedef struct {
  uint32_t magic, version, model_type, param_size;
  uint32_t checksum, layer_count;
  char name[32];
  uint8_t reserved[8];
} AIModelHeader;

static const uint32_t crc32_table[16] = {
  0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
  0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
  0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
  0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};

uint32_t ai_crc32(const void* data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  const uint8_t* p = (const uint8_t*)data;
  for (size_t i = 0; i < len; i++) {
    crc = crc32_table[(crc ^ p[i]) & 0x0F] ^ (crc >> 4);
    crc = crc32_table[(crc ^ (p[i] >> 4)) & 0x0F] ^ (crc >> 4);
  }
  return ~crc;
}

static bool fs_init = false;
static bool model_ready = false;
static uint8_t ai_state = 0;
static float last_outputs[4] = {0};
static uint16_t train_epochs = 0;
static float last_loss = 0;

static aimodel_t model;
static ailayer_input_f32_t input_layer;
static ailayer_dense_f32_t dense_1, dense_2;
static ailayer_sigmoid_f32_t sigmoid_1, sigmoid_2;
static ailoss_mse_t mse_loss;
static byte param_mem[128], train_mem[512], infer_mem[64];

float x_train[] = {0, 0, 0, 1, 1, 0, 1, 1};
float y_train[] = {0, 1, 1, 0};

// ===== Transport Manager =====

TransportManager transport;
SpriteTransport* active_transport = nullptr;

// Debug output - always goes to both interfaces
void debug_print(const char* msg) {
  Serial.print(msg);   // USB
  Serial1.print(msg);  // UART
}

void debug_println(const char* msg) {
  Serial.println(msg);
  Serial1.println(msg);
}

// ===== Display Manager =====

#if SPRITE_DISPLAY_TYPE == 1
SSD1306Display sprite_display;
#else
SimulatedDisplay sprite_display;
#endif

// ===== Sprite Engine =====

SpriteEngine sprite_engine;

// Sprite data storage (up to 8 sprites, max 16x16 each = 32 bytes)
#define MAX_SPRITE_DATA_SIZE 256
static uint8_t sprite_data_pool[MAX_SPRITE_DATA_SIZE];
static uint16_t sprite_data_used = 0;

// ===== Model Manager =====

ModelManager model_manager;

// Upload state for chunked model upload
static uint8_t upload_buffer[512];
static uint16_t upload_buffer_pos = 0;
static uint16_t upload_total_size = 0;
static char upload_filename[32];

// ===== Enhanced Utilities =====

void print_progress_bar(uint8_t percent, const char* label) {
  #if ENABLE_PROGRESS_BARS
  Serial1.print(label);
  Serial1.print(" [");
  for (int i = 0; i < 20; i++) {
    if (i < percent / 5) Serial1.print("█");
    else Serial1.print("░");
  }
  Serial1.print("] ");
  Serial1.print(percent);
  Serial1.println("%");
  #endif
}

void log_info(const char* msg) {
  Serial1.print("[INFO] ");
  Serial1.println(msg);
}

void log_error(const char* msg) {
  Serial1.print("[ERROR] ");
  Serial1.println(msg);
}

void log_success(const char* msg) {
  Serial1.print("[✓] ");
  Serial1.println(msg);
}

uint32_t get_free_ram() {
  // Simplified - return runtimeGet variable space
  return rp2040.getFreeHeap();
}

void print_memory_status() {
  uint32_t free = get_free_ram();
  Serial1.print("RAM: ");
  Serial1.print(free / 1024);
  Serial1.print("KB free / 256KB total");
  if (free < 10000) Serial1.print(" ⚠️");
  debug_println("");
}

// ===== Filesystem =====

bool init_fs() {
  if (fs_init) return true;
  
  log_info("Initializing LittleFS...");
  fs_init = LittleFS.begin();
  
  if (!fs_init) {
    log_error("LittleFS mount failed, formatting...");
    if (LittleFS.format()) {
      fs_init = LittleFS.begin();
      if (fs_init) log_success("Filesystem formatted and mounted");
    }
  } else {
    log_success("LittleFS mounted");
  }
  
  if (fs_init) {
    FSInfo info;
    LittleFS.info(info);
    Serial1.print("  Total: ");
    Serial1.print(info.totalBytes / 1024);
    Serial1.print("KB, Used: ");
    Serial1.print(info.usedBytes / 1024);
    Serial1.println("KB");
  }
  
  return fs_init;
}

// ===== AI Model =====

void build_model() {
  uint16_t shape[] = {1, 2};
  input_layer = AILAYER_INPUT_F32_A(2, shape);
  dense_1 = AILAYER_DENSE_F32_A(3);
  sigmoid_1 = AILAYER_SIGMOID_F32_A();
  dense_2 = AILAYER_DENSE_F32_A(1);
  sigmoid_2 = AILAYER_SIGMOID_F32_A();
  
  ailayer_t *x;
  model.input_layer = ailayer_input_f32_default(&input_layer);
  x = ailayer_dense_f32_default(&dense_1, model.input_layer);
  x = ailayer_sigmoid_f32_default(&sigmoid_1, x);
  x = ailayer_dense_f32_default(&dense_2, x);
  model.output_layer = ailayer_sigmoid_f32_default(&sigmoid_2, x);
  model.loss = ailoss_mse_f32_default(&mse_loss, model.output_layer);
  
  aialgo_compile_model(&model);
  aialgo_distribute_parameter_memory(&model, param_mem, sizeof(param_mem));
}

void init_weights() {
  aimath_f32_default_init_glorot_uniform(&dense_1.weights);
  aimath_f32_default_init_zeros(&dense_1.bias);
  aimath_f32_default_init_glorot_uniform(&dense_2.weights);
  aimath_f32_default_init_zeros(&dense_2.bias);
}

float do_inference(float in0, float in1) {
  float inputs[2] = {in0, in1}, output[1];
  uint16_t in_shape[] = {1, 2}, out_shape[] = {1, 1};
  aitensor_t in_tensor = AITENSOR_2D_F32(in_shape, inputs);
  aitensor_t out_tensor = AITENSOR_2D_F32(out_shape, output);
  
  aialgo_schedule_inference_memory(&model, infer_mem, sizeof(infer_mem));
  aialgo_inference_model(&model, &in_tensor, &out_tensor);
  return output[0];
}

void do_train(uint8_t epochs) {
  ai_state = 1;
  log_info("Building neural network...");
  build_model();
  init_weights();
  
  log_info("Configuring ADAM optimizer...");
  aiopti_adam_f32_t adam = AIOPTI_ADAM_F32(0.1f, 0.9f, 0.999f, 1e-7);
  aiopti_t *optimizer = aiopti_adam_f32_default(&adam);
  aialgo_schedule_training_memory(&model, optimizer, train_mem, sizeof(train_mem));
  aialgo_init_model_for_training(&model, optimizer);
  
  uint16_t x_shape[] = {4, 2}, y_shape[] = {4, 1};
  aitensor_t input = AITENSOR_2D_F32(x_shape, x_train);
  aitensor_t target = AITENSOR_2D_F32(y_shape, y_train);
  
  Serial1.print("Training ");
  Serial1.print(epochs);
  Serial1.println(" epochs...");
  
  uint32_t start_time = millis();
  
  for (int i = 0; i < epochs; i++) {
    last_loss = aialgo_train_model(&model, &input, &target, optimizer, 4);
    train_epochs++;
    
    // Progress updates
    if (epochs > 20 && (i % (epochs/10) == 0 || i == epochs-1)) {
      uint8_t percent = (i * 100) / epochs;
      print_progress_bar(percent, "  ");
    }
  }
  
  uint32_t duration = millis() - start_time;
  
  Serial1.print("✓ Training complete in ");
  Serial1.print(duration);
  Serial1.print("ms (");
  Serial1.print(duration / epochs);
  Serial1.println("ms/epoch)");
  Serial1.print("  Final loss: ");
  Serial1.println(last_loss, 6);
  
  model_ready = true;
  ai_state = 0;
}

bool save_model(const char* filename) {
  if (!model_ready || !init_fs()) return false;
  
  log_info("Saving model to flash...");
  File f = LittleFS.open(filename, "w");
  if (!f) {
    log_error("Failed to open file for writing");
    return false;
  }
  
  uint32_t size = aialgo_sizeof_parameter_memory(&model);
  AIModelHeader h = {AI_MODEL_MAGIC, 1, 0, size, ai_crc32(param_mem, size), 5};
  safe_strcpy(h.name, filename, sizeof(h.name));
  
  f.write((uint8_t*)&h, sizeof(h));
  f.write(param_mem, size);
  f.close();
  
  Serial1.print("✓ Saved ");
  Serial1.print(size);
  Serial1.print(" bytes to ");
  Serial1.println(filename);
  return true;
}

bool load_model(const char* filename) {
  if (!init_fs() || !LittleFS.exists(filename)) {
    log_error("Model file not found");
    return false;
  }
  
  log_info("Loading model from flash...");
  File f = LittleFS.open(filename, "r");
  if (!f) {
    log_error("Failed to open file");
    return false;
  }
  
  AIModelHeader h;
  f.read((uint8_t*)&h, sizeof(h));
  
  if (h.magic != AI_MODEL_MAGIC) {
    log_error("Invalid model file format");
    f.close();
    return false;
  }
  
  build_model();
  f.read(param_mem, h.param_size);
  f.close();
  
  if (ai_crc32(param_mem, h.param_size) != h.checksum) {
    log_error("Model checksum mismatch - file corrupted!");
    return false;
  }
  
  aialgo_distribute_parameter_memory(&model, param_mem, sizeof(param_mem));
  model_ready = true;
  
  Serial1.print("✓ Loaded ");
  Serial1.print(h.param_size);
  Serial1.println(" bytes");
  return true;
}

// ===== Protocol =====

// Protocol state variables (declared early for use in handle_command)
static uint8_t rx_state = 0, rx_cmd, rx_len, rx_pos;
static uint8_t rx_buf[64];

void send_response(uint8_t cmd, uint8_t status, const uint8_t* data, uint8_t len) {
  if (!active_transport) return;
  
  active_transport->write(SPRITE_HEADER);
  active_transport->write(cmd);
  active_transport->write(status);
  active_transport->write(len);
  if (len > 0 && data) {
    for (uint8_t i = 0; i < len; i++) active_transport->write(data[i]);
  }
  active_transport->write((uint8_t)0x00); // Checksum placeholder
}

void handle_command(uint8_t cmd, const uint8_t* payload, uint8_t len) {
  switch (cmd) {
    case CMD_VERSION: {
      uint8_t ver[] = {1, 0, 0};
      send_response(cmd, RESP_OK, ver, 3);
      break;
    }
    
    case CMD_BUFFER_STATUS: {
      // Report available RX buffer space for flow control
      uint16_t free_space = sizeof(rx_buf);  // Full buffer size available
      uint8_t resp[2];
      memcpy(resp, &free_space, 2);
      send_response(cmd, RESP_OK, resp, 2);
      break;
    }
    
    case CMD_CLEAR:
      fb_clear();
      send_response(cmd, RESP_OK, nullptr, 0);
      break;
      
    case CMD_FLUSH:
      // Update physical display
      if (dirty_rect.is_dirty) {
        // Use dirty rectangle for partial update
        sprite_display.updateRegion(framebuffer, 
                                    dirty_rect.x1, dirty_rect.y1,
                                    dirty_rect.x2, dirty_rect.y2);
        
        uint16_t area = (dirty_rect.x2 - dirty_rect.x1 + 1) * 
                        (dirty_rect.y2 - dirty_rect.y1 + 1);
        #if SPRITE_VERBOSE
        Serial1.print("[FLUSH] Updated ");
        Serial1.print(area);
        Serial1.println(" pixels on display");
        #endif
        dirty_rect.is_dirty = false;  // Reset
      } else {
        // Full refresh if no dirty tracking
        sprite_display.update(framebuffer);
      }
      send_response(cmd, RESP_OK, nullptr, 0);
      break;
    
    // ===== Sprite Commands =====
    
    case CMD_SPRITE_CREATE: {
      // Payload: id(1) x(2) y(2) w(1) h(1) layer(1) flags(1) bitmap_data(...)
      if (len < 8) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      uint8_t id = payload[0];
      int16_t x, y;
      memcpy(&x, payload + 1, 2);
      memcpy(&y, payload + 3, 2);
      uint8_t w = payload[5];
      uint8_t h = payload[6];
      uint8_t layer = payload[7];
      uint8_t flags = len > 8 ? payload[8] : SPRITE_FLAG_VISIBLE;
      
      // Calculate bitmap size
      uint16_t bitmap_size = ((w * h) + 7) / 8;  // Bits to bytes
      if (9 + bitmap_size > len) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      // Allocate from pool
      if (sprite_data_used + bitmap_size > MAX_SPRITE_DATA_SIZE) {
        send_response(cmd, RESP_ERROR, nullptr, 0);  // Out of sprite memory
        break;
      }
      
      // Copy bitmap data
      memcpy(sprite_data_pool + sprite_data_used, payload + 9, bitmap_size);
      const uint8_t* sprite_data = sprite_data_pool + sprite_data_used;
      sprite_data_used += bitmap_size;
      
      // Add sprite
      bool ok = sprite_engine.add(id, x, y, w, h, sprite_data, flags, layer);
      send_response(cmd, ok ? RESP_OK : RESP_ERROR, nullptr, 0);
      break;
    }
    
    case CMD_SPRITE_MOVE: {
      // Payload: id(1) x(2) y(2)
      if (len < 5) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      uint8_t id = payload[0];
      int16_t x, y;
      memcpy(&x, payload + 1, 2);
      memcpy(&y, payload + 3, 2);
      
      bool ok = sprite_engine.move(id, x, y);
      send_response(cmd, ok ? RESP_OK : RESP_NOT_FOUND, nullptr, 0);
      break;
    }
    
    case CMD_SPRITE_DELETE: {
      // Payload: id(1)
      if (len < 1) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      bool ok = sprite_engine.remove(payload[0]);
      send_response(cmd, ok ? RESP_OK : RESP_NOT_FOUND, nullptr, 0);
      break;
    }
    
    case CMD_SPRITE_VISIBLE: {
      // Payload: id(1) visible(1)
      if (len < 2) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      bool ok = sprite_engine.setVisible(payload[0], payload[1] != 0);
      send_response(cmd, ok ? RESP_OK : RESP_NOT_FOUND, nullptr, 0);
      break;
    }
    
    case CMD_SPRITE_COLLISION: {
      // Payload: id_a(1) id_b(1)
      if (len < 2) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      bool collision = sprite_engine.checkCollision(payload[0], payload[1]);
      uint8_t result = collision ? 1 : 0;
      send_response(cmd, RESP_OK, &result, 1);
      break;
    }
    
    case CMD_SPRITE_RENDER: {
      // Render all sprites to framebuffer
      sprite_engine.render(framebuffer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
      send_response(cmd, RESP_OK, nullptr, 0);
      break;
    }
    
    case CMD_SPRITE_CLEAR: {
      // Clear all sprites
      sprite_engine.clear();
      sprite_data_used = 0;  // Reset data pool
      send_response(cmd, RESP_OK, nullptr, 0);
      break;
    }
      
    case CMD_AI_INFER: {
      if (len >= 8 && model_ready) {
        float in0, in1;
        memcpy(&in0, payload, 4);
        memcpy(&in1, payload + 4, 4);
        float result = do_inference(in0, in1);
        send_response(cmd, RESP_OK, (uint8_t*)&result, 4);
      } else {
        send_response(cmd, model_ready ? RESP_ERROR : RESP_NOT_FOUND, nullptr, 0);
      }
      break;
    }
    
    case CMD_AI_TRAIN: {
      uint8_t epochs = len > 0 ? payload[0] : 100;
      do_train(epochs);
      send_response(cmd, RESP_OK, (uint8_t*)&last_loss, 4);
      break;
    }
    
    case CMD_AI_STATUS: {
      uint8_t resp[8];
      resp[0] = ai_state;
      resp[1] = model_ready ? 1 : 0;
      memcpy(resp + 2, &train_epochs, 2);
      memcpy(resp + 4, &last_loss, 4);
      send_response(cmd, RESP_OK, resp, 8);
      break;
    }
    
    case CMD_AI_SAVE: {
      char fname[32] = "/model.aif32";
      if (len > 0 && len < 31) { memcpy(fname, payload, len); fname[len] = 0; }
      send_response(cmd, save_model(fname) ? RESP_OK : RESP_ERROR, nullptr, 0);
      break;
    }
    
    case CMD_AI_LOAD: {
      char fname[32] = "/model.aif32";
      if (len > 0 && len < 31) { memcpy(fname, payload, len); fname[len] = 0; }
      send_response(cmd, load_model(fname) ? RESP_OK : RESP_NOT_FOUND, nullptr, 0);
      break;
    }
    
    // ===== Model Management Commands =====
    
    case CMD_MODEL_INFO: {
      // Get active model info
      ModelHeader hdr;
      if (model_manager.getActiveInfo(&hdr)) {
        uint8_t resp[32];
        memcpy(resp, &hdr, sizeof(ModelHeader));
        send_response(cmd, RESP_OK, resp, sizeof(ModelHeader));
      } else {
        send_response(cmd, RESP_NOT_FOUND, nullptr, 0);
      }
      break;
    }
    
    case CMD_MODEL_LIST: {
      // List all models
      char models[8][32];
      uint8_t count = model_manager.listModels(models, 8);
      
      if (count == 0) {
        send_response(cmd, RESP_OK, nullptr, 0);
      } else {
        // Pack model names into response
        uint8_t resp[256];
        uint8_t pos = 0;
        for (uint8_t i = 0; i < count; i++) {
          uint8_t name_len = strlen(models[i]);
          resp[pos++] = name_len;
          memcpy(resp + pos, models[i], name_len);
          pos += name_len;
        }
        send_response(cmd, RESP_OK, resp, pos);
      }
      break;
    }
    
    case CMD_MODEL_SELECT: {
      // Select active model by filename
      if (len == 0 || len > 31) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      char filename[32];
      memcpy(filename, payload, len);
      filename[len] = '\0';
      
      bool ok = model_manager.selectModel(filename);
      send_response(cmd, ok ? RESP_OK : RESP_NOT_FOUND, nullptr, 0);
      break;
    }
    
    case CMD_MODEL_UPLOAD: {
      // Bulk upload model file
      // Payload: filename_len(1) filename(...) data(...)
      if (len < 2) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      uint8_t filename_len = payload[0];
      if (filename_len == 0 || filename_len > 31 || 1 + filename_len >= len) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      char filename[32];
      memcpy(filename, payload + 1, filename_len);
      filename[filename_len] = '\0';
      
      uint16_t data_len = len - 1 - filename_len;
      const uint8_t* data = payload + 1 + filename_len;
      
      bool ok = model_manager.uploadModel(filename, data, data_len);
      send_response(cmd, ok ? RESP_OK : RESP_ERROR, nullptr, 0);
      break;
    }
    
    case CMD_MODEL_DELETE: {
      // Delete model file
      if (len == 0 || len > 31) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      char filename[32];
      memcpy(filename, payload, len);
      filename[len] = '\0';
      
      bool ok = model_manager.deleteModel(filename);
      send_response(cmd, ok ? RESP_OK : RESP_NOT_FOUND, nullptr, 0);
      break;
    }
    
    case CMD_FINETUNE_START: {
      // Start fine-tuning session on active model
      // TODO: Implement fine-tuning logic
      send_response(cmd, RESP_OK, nullptr, 0);
      break;
    }
    
    case CMD_FINETUNE_DATA: {
      // Send training sample(s) for fine-tuning
      // Payload: num_samples(1) [input_data output_data]+
      // TODO: Implement incremental training
      send_response(cmd, RESP_OK, nullptr, 0);
      break;
    }
    
    case CMD_FINETUNE_STOP: {
      // End fine-tuning session, save updated weights
      // TODO: Save fine-tuned model
      send_response(cmd, RESP_OK, nullptr, 0);
      break;
    }
    
    default:
      send_response(cmd, RESP_ERROR, nullptr, 0);
      break;
  }
}

// ===== Main =====

void setup() {
  // Initialize both transports
  transport.begin(115200);
  delay(2000);
  
  randomSeed(analogRead(A0));
  srand(analogRead(A0));
  
  // Enhanced startup banner (debug output to both interfaces)
  debug_println("\n\n");
  debug_println("╔════════════════════════════════════════╗");
  debug_print("║     SPRITE ONE v");
  debug_print(SPRITE_VERSION);
  debug_println("                 ║");
  debug_println("║     Graphics & AI Accelerator          ║");
  debug_println("║     Dual Transport (UART + USB-CDC)    ║");
  debug_print("║     Display: ");
  debug_print(sprite_display.name());
  for(int i = strlen(sprite_display.name()); i < 27; i++) debug_print(" ");
  debug_println("║");
  debug_println("╚════════════════════════════════════════╝");
  debug_println("");
  
  // Initialize display
  log_info("Initializing display...");
  if (sprite_display.init()) {
    debug_print("  ✓ ");
    debug_print(sprite_display.name());
    debug_println(" ready");
  } else {
    log_error("Display initialization failed!");
  }
  debug_println("");
  
  // System info
  log_info("System Information:");
  debug_println("  CPU: RP2040 @ 133MHz");
  debug_println("  Flash: 2MB");
  debug_println("  RAM: 256KB");
  print_memory_status();
  debug_println("");
  
  // Initialize filesystem
  init_fs();
  debug_println("");
  
  // Initialize AI
  log_info("Initializing AI Engine...");
  if (LittleFS.exists("/model.aif32")) {
    if (load_model("/model.aif32")) {
      log_success("Model loaded from flash - ready for inference!");
    } else {
      log_error("Load failed - training new model");
      do_train(100);
      save_model("/model.aif32");
    }
  } else {
    log_info("No saved model found - training XOR network");
    do_train(100);
    save_model("/model.aif32");
  }
  
  debug_println("");
  
  // Test XOR
  Serial1.println("┌─────────────────────────────────────────┐");
  Serial1.println("│  XOR Neural Network Results             │");
  Serial1.println("├─────────────────────────────────────────┤");
  bool all_correct = true;
  for (int i = 0; i < 4; i++) {
    float result = do_inference(x_train[i*2], x_train[i*2+1]);
    last_outputs[i] = result;
    int prediction = result > 0.5 ? 1 : 0;
    int expected = (int)y_train[i];
    bool correct = (prediction == expected);
    if (!correct) all_correct = false;
    
    Serial1.print("│   ");
    Serial1.print((int)x_train[i*2]);
    Serial1.print(" XOR ");
    Serial1.print((int)x_train[i*2+1]);
    Serial1.print(" = ");
    Serial1.print(result, 3);
    Serial1.print(" → ");
    Serial1.print(prediction);
    Serial1.print(correct ? " ✓" : " ✗");
    Serial1.println("                   │");
  }
  Serial1.println("└─────────────────────────────────────────┘");
  
  if (all_correct) {
    log_success("All XOR test cases passed!");
  } else {
    log_error("Some test cases failed - may need retraining");
  }
  
  debug_println("");
  Serial1.println("┌─────────────────────────────────────────┐");
  Serial1.println("│  Protocol Commands Ready                │");
  Serial1.println("├─────────────────────────────────────────┤");
  Serial1.println("│  Graphics: CLEAR(10), RECT(12),         │");
  Serial1.println("│            FLUSH(2F)                     │");
  Serial1.println("│  AI: INFER(50), TRAIN(51), STATUS(52)   │");
  Serial1.println("│      SAVE(53), LOAD(54), LIST(55)       │");
  Serial1.println("│  System: VERSION(0F)                     │");
  Serial1.println("└─────────────────────────────────────────┘");
  debug_println("");
  Serial1.println("Ready for commands! (AA CMD LEN DATA CHK)");
  Serial1.println("========================================\n");
}

void loop() {
  // Auto-detect active transport on first data
  if (!active_transport) {
    active_transport = transport.detect();
    if (active_transport) {
      debug_print("\n[TRANSPORT] Detected: ");
      debug_println(active_transport->name());
    }
  }
  
  // Process data from active transport
  if (active_transport && active_transport->available()) {
    uint8_t b = active_transport->read();
    
    switch (rx_state) {
      case 0: if (b == SPRITE_HEADER) rx_state = 1; break;
      case 1: rx_cmd = b; rx_state = 2; break;
      case 2: rx_len = b; rx_pos = 0; rx_state = rx_len > 0 ? 3 : 4; break;
      case 3: 
        if (rx_pos < sizeof(rx_buf)) rx_buf[rx_pos++] = b;
        if (rx_pos >= rx_len) rx_state = 4;
        break;
      case 4:
        handle_command(rx_cmd, rx_buf, rx_len);
        rx_state = 0;
        break;
    }
  }
  delay(1);
}

// Helper for safe string copy (needed by save_model)
void safe_strcpy(char* dest, const char* src, size_t max_len) {
  if (!dest || !src || max_len == 0) return;
  strncpy(dest, src, max_len - 1);
  dest[max_len - 1] = '\0';
}
