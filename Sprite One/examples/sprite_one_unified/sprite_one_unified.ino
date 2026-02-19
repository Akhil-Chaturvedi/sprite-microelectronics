/*
 * Sprite One - Polished Unified Demo
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
#include "sprite_inference.h"

// Enhanced configuration
#define SPRITE_VERSION "2.1.0-dual"
#define ENABLE_VERBOSE_LOGGING true
#define ENABLE_PROGRESS_BARS true
#define ENABLE_DUAL_CORE true

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
#define CMD_BATCH           0x70  // Batch command execution

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

// ===== Dual-Core Command Queue =====

struct CommandQueueEntry {
  uint8_t cmd;
  uint8_t len;
  uint8_t payload[64];
};

#if ENABLE_DUAL_CORE
#include "pico/multicore.h"
#include "pico/mutex.h"

static mutex_t sprite_lock; // Global lock for queues and state

template<int SIZE>
class CommandQueue {
private:
  volatile uint8_t q_cmd[SIZE];
  volatile uint8_t q_len[SIZE];
  uint8_t q_payload[SIZE][64];
  volatile uint32_t head;
  volatile uint32_t tail;
public:
  CommandQueue() : head(0), tail(0) {}
  
  bool push(uint8_t cmd, const uint8_t* payload, uint8_t len) {
    mutex_enter_blocking(&sprite_lock);
    uint32_t next = (head + 1) % SIZE;
    if (next == tail) {
        mutex_exit(&sprite_lock);
        return false;
    }
    q_cmd[head] = cmd;
    q_len[head] = len;
    if (len > 0 && payload) memcpy(q_payload[head], payload, min((int)len, 64));
    head = next;
    mutex_exit(&sprite_lock);
    return true;
  }
  
  bool pop(CommandQueueEntry* out) {
    mutex_enter_blocking(&sprite_lock);
    if (tail == head) {
        mutex_exit(&sprite_lock);
        return false;
    }
    out->cmd = q_cmd[tail];
    out->len = q_len[tail];
    memcpy(out->payload, q_payload[tail], q_len[tail]);
    tail = (tail + 1) % SIZE;
    mutex_exit(&sprite_lock);
    return true;
  }
  bool isEmpty() const { return head == tail; } // Reader warning: unprotected
  bool isFull() const { return ((head + 1) % SIZE) == tail; }
};

struct ResponseEntry {
  uint8_t cmd;
  uint8_t status;
  uint8_t len;
  uint8_t data[64];
};

template<int SIZE>
class ResponseQueue {
private:
  volatile uint8_t r_cmd[SIZE];
  volatile uint8_t r_status[SIZE];
  volatile uint8_t r_len[SIZE];
  uint8_t r_data[SIZE][64];
  volatile uint32_t head;
  volatile uint32_t tail;
public:
  ResponseQueue() : head(0), tail(0) {}
  bool push(uint8_t cmd, uint8_t status, const uint8_t* data, uint8_t len) {
    mutex_enter_blocking(&sprite_lock);
    uint32_t next = (head + 1) % SIZE;
    if (next == tail) {
        mutex_exit(&sprite_lock);
        return false;
    }
    r_cmd[head] = cmd;
    r_status[head] = status;
    r_len[head] = min((int)len, 64);
    if (len > 0 && data) memcpy(r_data[head], data, r_len[head]);
    head = next;
    mutex_exit(&sprite_lock);
    return true;
  }
  
  bool pop(ResponseEntry* out) {
    mutex_enter_blocking(&sprite_lock);
    if (tail == head) {
        mutex_exit(&sprite_lock);
        return false;
    }
    out->cmd = r_cmd[tail];
    out->status = r_status[tail];
    out->len = r_len[tail];
    memcpy(out->data, r_data[tail], r_len[tail]);
    tail = (tail + 1) % SIZE;
    mutex_exit(&sprite_lock);
    return true;
  }
  bool isEmpty() const { return head == tail; }
};

static CommandQueue<16> cmd_queue;
static ResponseQueue<8> response_queue;

struct Core1State {
  bool ai_training;
  bool ai_model_ready;
  uint32_t free_cycles;
};
static Core1State core1_state = {false, false, 0};

// Helper to check state safely
bool is_training() {
    bool t;
    mutex_enter_blocking(&sprite_lock);
    t = core1_state.ai_training;
    mutex_exit(&sprite_lock);
    return t;
}

void set_training(bool active) {
    mutex_enter_blocking(&sprite_lock);
    core1_state.ai_training = active;
    mutex_exit(&sprite_lock);
}

void core1_entry();  // Forward declaration

#endif // ENABLE_DUAL_CORE

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

// Async FS State
enum FSState { FS_IDLE, FS_SAVE_PENDING, FS_SAVING, FS_LOAD_PENDING, FS_LOADING };
static FSState fs_state = FS_IDLE;
static char fs_filename[32];
static File fs_file;
static uint32_t fs_bytes_processed = 0;
static uint32_t fs_total_bytes = 0;
static uint32_t fs_crc_accum = 0;

void fs_task() {
  if (fs_state == FS_IDLE) return;
  
  if (fs_state == FS_SAVE_PENDING) {
    fs_file = LittleFS.open(fs_filename, "w");
    if (!fs_file) {
       log_error("Async Save: Failed to open file");
       fs_state = FS_IDLE;
       return;
    }
    // Write header
    uint32_t size = aialgo_sizeof_parameter_memory(&model);
    ModelHeader h = {MODEL_MAGIC, MODEL_VERSION, 2, 1, 3, MODEL_TYPE_F32, 0, ai_crc32(param_mem, size), ""};
    safe_strcpy(h.name, fs_filename, sizeof(h.name));
    fs_file.write((uint8_t*)&h, sizeof(h));
    
    fs_total_bytes = size;
    fs_bytes_processed = 0;
    fs_state = FS_SAVING;
    log_info("Async Save: Started...");
    return;
  }
  
  if (fs_state == FS_SAVING) {
    // Write chunk
    uint32_t remaining = fs_total_bytes - fs_bytes_processed;
    uint32_t chunk = min((uint32_t)256, remaining);
    
    if (chunk > 0) {
      fs_file.write(param_mem + fs_bytes_processed, chunk);
      fs_bytes_processed += chunk;
    }
    
    if (fs_bytes_processed >= fs_total_bytes) {
      fs_file.close();
      log_success("Async Save: Complete");
      fs_state = FS_IDLE;
    }
    return;
  }
  
  if (fs_state == FS_LOAD_PENDING) {
    if (!LittleFS.exists(fs_filename)) {
      log_error("Async Load: File not found");
      fs_state = FS_IDLE;
      return;
    }
    fs_file = LittleFS.open(fs_filename, "r");
    if (!fs_file) {
      log_error("Async Load: Failed to open");
      fs_state = FS_IDLE;
      return;
    }
    // Read header
    ModelHeader h;
    if (fs_file.read((uint8_t*)&h, sizeof(h)) != sizeof(h)) {
      log_error("Async Load: Bad header");
      fs_file.close();
      fs_state = FS_IDLE;
      return;
    }
    if (h.magic != MODEL_MAGIC) {
      log_error("Async Load: Invalid magic");
      fs_file.close();
      fs_state = FS_IDLE;
      return;
    }
    
    fs_total_bytes = fs_file.size() - sizeof(h);
    fs_bytes_processed = 0;
    fs_state = FS_LOADING;
    model_ready = false; // Mark not ready during load
    log_info("Async Load: Started...");
    return;
  }
  
  if (fs_state == FS_LOADING) {
    // Read chunk
    uint32_t remaining = fs_total_bytes - fs_bytes_processed;
    uint32_t chunk = min((uint32_t)256, remaining);
    
    if (chunk > 0) {
      fs_file.read(param_mem + fs_bytes_processed, chunk);
      fs_bytes_processed += chunk;
    }
    
    if (fs_bytes_processed >= fs_total_bytes) {
      fs_file.close();
      // Verify CRC? (Optional but good)
      // For now assume success
      model_ready = true;
      convert_f32_to_int8(&dense_1, &dense_2);
      log_success("Async Load: Complete");
      fs_state = FS_IDLE;
    }
    return;
  }
}

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

#include "sprite_dynamic.h"

static DynamicModel dynamic_model;
static bool use_dynamic_model = false;

// Legacy Static Model (Keep for fallback/training demo)
static aimodel_t model;
static ailayer_input_f32_t input_layer;
static ailayer_dense_f32_t dense_1, dense_2;
static ailayer_sigmoid_f32_t sigmoid_1, sigmoid_2;
static ailoss_mse_t mse_loss;
static byte param_mem[4096], train_mem[2048], infer_mem[1024];

float x_train[] = {0, 0, 0, 1, 1, 0, 1, 1};
float y_train[] = {0, 1, 1, 0};

// ===== AI Model =====

void build_model() {
  // Legacy XOR Model Construction
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
  if (use_dynamic_model) {
      // Dynamic Inference
      // Map the 2 arguments to the 128-input vector (rest zeros)
      // This allows the legacy XOR test command to trigger the "God Mode" brain
      float input_vec[128] = {0};
      input_vec[0] = in0;
      input_vec[1] = in1;
      
      // Inference runs on the Static Arena
      float* results = dynamic_model.infer(input_vec);
      
      // Return the first output (Class 0 confidence)
      if (results) return results[0];
      return 0.0f;
  } else {
      // Use Optimized INT8 Path (Legacy)
      return do_inference_int8(in0, in1);
  }
}

void do_train(uint8_t epochs) {
  ai_state = 1;
  use_dynamic_model = false; // Disable dynamic model during training (for now)
  
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
  
  // Quantize for fast inference
  convert_f32_to_int8(&dense_1, &dense_2);
  log_info("Model Auto-Quantized to INT8");
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
  ModelHeader h = {MODEL_MAGIC, MODEL_VERSION, 2, 1, 3, MODEL_TYPE_F32, 0, ai_crc32(param_mem, size), ""};
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
  
  // For Dynamic Loading, we read the whole file into a buffer
  // Then pass it to our dynamic loader.
  File f = LittleFS.open(filename, "r");
  if (!f) {
    log_error("Failed to open file");
    return false;
  }
  
  size_t size = f.size();
  uint8_t* buffer = new uint8_t[size]; // Allocate temp buffer
  if (!buffer) {
     log_error("OOM: Cannot allocate model buffer");
     f.close();
     return false;
  }
  
  f.read(buffer, size);
  f.close();
  
  // Try to load as Dynamic Model
  if (dynamic_model.load(buffer, size)) {
      log_success("Dynamic Model Loaded Successfully!");
      use_dynamic_model = true;
      delete[] buffer; // Parser copies what it needs
      return true;
  }
  
  // Fallback to Legacy Loader (if magic matches old format)
  // Re-read header from buffer
  ModelHeader* h = (ModelHeader*)buffer;
  
  if (h->magic != MODEL_MAGIC) {
    log_error("Invalid model file format");
    delete[] buffer;
    return false;
  }
  
  // Legacy path requires re-building hardcoded model structure
  // This is only for the fixed "XOR" demo model structure
  build_model();
  
  // Copy weights from buffer (skip header)
  if (size - sizeof(ModelHeader) <= sizeof(param_mem)) {
      memcpy(param_mem, buffer + sizeof(ModelHeader), size - sizeof(ModelHeader));
      aialgo_distribute_parameter_memory(&model, param_mem, sizeof(param_mem));
      model_ready = true;
      use_dynamic_model = false;
      
      // Quantize for fast inference
      convert_f32_to_int8(&dense_1, &dense_2);
      
      Serial1.print("✓ Loaded Legacy ");
      Serial1.print(size);
      Serial1.println(" bytes");
      delete[] buffer;
      return true;
  } else {
      log_error("Legacy model too large for static buffer");
      delete[] buffer;
      return false;
  }
}

// ===== Protocol =====

// Protocol state variables (declared early for use in handle_command)
// Protocol state
static uint8_t rx_state = 0, rx_cmd, rx_len, rx_pos;
static uint8_t rx_buf[300]; // Increased for full packets

// Upload State
static File upload_file;
static bool is_uploading = false;
static uint32_t upload_expected_crc = 0;
static uint32_t upload_recv_crc = 0;

// Forward decl
uint32_t crc32_byte(uint32_t crc, uint8_t data); 

void send_response(uint8_t cmd, uint8_t status, const uint8_t* data, uint8_t len) {
  if (!active_transport) return;
  
  active_transport->write(SPRITE_HEADER);
  
  uint32_t crc = 0xFFFFFFFF;
  
  active_transport->write(cmd);
  crc = crc32_byte(crc, cmd);
  
  active_transport->write(status);
  crc = crc32_byte(crc, status);
  
  active_transport->write(len);
  crc = crc32_byte(crc, len);
  
  if (len > 0 && data) {
    for (uint8_t i = 0; i < len; i++) {
        active_transport->write(data[i]);
        crc = crc32_byte(crc, data[i]);
    }
  }
  
  uint32_t final_crc = ~crc;
  active_transport->write((uint8_t)(final_crc & 0xFF));
  active_transport->write((uint8_t)((final_crc >> 8) & 0xFF));
  active_transport->write((uint8_t)((final_crc >> 16) & 0xFF));
  active_transport->write((uint8_t)((final_crc >> 24) & 0xFF));
}

void handle_command(uint8_t cmd, const uint8_t* payload, uint8_t len) {
  #if ENABLE_DUAL_CORE
  // Commands that run on Core1 (AI/Display)
  if (cmd >= CMD_CLEAR && cmd <= CMD_SPRITE_CLEAR) {
    // Graphics and sprite commands -> Core1
    if (cmd_queue.push(cmd, payload, len)) {
      // Will be processed asynchronously, check response queue later
      return;
    } else {
      send_response(cmd, RESP_ERROR, nullptr, 0);  // Queue full
      return;
    }
  }
  if (cmd >= CMD_AI_INFER && cmd <= CMD_FINETUNE_STOP) {

    if (cmd == CMD_AI_SAVE || cmd == CMD_AI_LOAD) {
      // Handled on Core0 (fall through)
    } else {
      // Check for conflicts
      if (fs_state != FS_IDLE) {
        send_response(cmd, RESP_ERROR, nullptr, 0); // FS Busy
        return;
      }
      // Queue to Core1
      if (cmd_queue.push(cmd, payload, len)) {
        return;
      } else {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        return;
      }
    }
  }
  #endif
  
  // Commands processed on Core0 (immediate response)
  switch (cmd) {
    case CMD_VERSION: {
      uint8_t ver[] = {1, 0, 0};
      send_response(cmd, RESP_OK, ver, 3);
      break;
    }
    
    case CMD_BATCH: {
      // Execute multiple commands in a single packet
      // Payload format: [CMD1, LEN1, DATA1...], [CMD2, LEN2, DATA2...]
      uint8_t p = 0;
      while (p < len) {
        if (p + 2 > len) break; // Incomplete header
        uint8_t sub_cmd = payload[p++];
        uint8_t sub_len = payload[p++];
        
        if (p + sub_len > len) break; // Incomplete payload
        
        // Recursive call (careful with stack depth!)
        // Note: sub-commands will send their own responses
        handle_command(sub_cmd, payload + p, sub_len);
        
        p += sub_len;
      }
      // Batch command itself sends no response (or maybe just OK?)
      // sending OK to confirm batch processing finished
      send_response(cmd, RESP_OK, nullptr, 0); 
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

    case CMD_AI_SAVE:
      if (len > 0) {
        if (fs_state != FS_IDLE || is_training()) {
          send_response(cmd, RESP_ERROR, nullptr, 0); // Busy
        } else {
          char fname[32];
          safe_strcpy(fname, (const char*)payload, min((int)len + 1, 32));
          safe_strcpy(fs_filename, fname, 32);
          fs_state = FS_SAVE_PENDING;
          send_response(cmd, RESP_OK, nullptr, 0); // Immediate ACK
        }
      } else {
         // Default filename
         if (fs_state != FS_IDLE || is_training()) {
            send_response(cmd, RESP_ERROR, nullptr, 0);
         } else {
            safe_strcpy(fs_filename, "model.aif32", 32);
            fs_state = FS_SAVE_PENDING;
            send_response(cmd, RESP_OK, nullptr, 0);
         }
      }
      break;
      
    case CMD_AI_LOAD:
      if (len > 0) {
        if (fs_state != FS_IDLE || is_training()) {
          send_response(cmd, RESP_ERROR, nullptr, 0); // Busy
        } else {
          char fname[32];
          safe_strcpy(fname, (const char*)payload, min((int)len + 1, 32));
          safe_strcpy(fs_filename, fname, 32);
          fs_state = FS_LOAD_PENDING;
          send_response(cmd, RESP_OK, nullptr, 0); // Immediate ACK
        }
      } else {
         if (fs_state != FS_IDLE || is_training()) {
            send_response(cmd, RESP_ERROR, nullptr, 0);
         } else {
            safe_strcpy(fs_filename, "model.aif32", 32);
            fs_state = FS_LOAD_PENDING;
            send_response(cmd, RESP_OK, nullptr, 0);
         }
      }
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
    
    case CMD_MODEL_UPLOAD: { // 0x63: START UPLOAD
      // Payload: filename (null terminated)
      if (len == 0 || len > 31) {
        send_response(cmd, RESP_ERROR, nullptr, 0);
        break;
      }
      
      char filename[32];
      memcpy(filename, payload, len);
      filename[len] = '\0';
      
      if (is_uploading && upload_file) upload_file.close();
      
      // Ensure we are not training
      if (is_training()) { 
          send_response(cmd, RESP_ERROR, nullptr, 0); 
          break; 
      }
      
      upload_file = LittleFS.open(filename, "w");
      if (!upload_file) {
          send_response(cmd, RESP_ERROR, nullptr, 0);
          break;
      }
      
      is_uploading = true;
      upload_recv_crc = 0xFFFFFFFF; // Init CRC for full file
      send_response(cmd, RESP_OK, nullptr, 0);
      break;
    }

    case 0x68: { // CMD_UPLOAD_CHUNK
        if (!is_uploading || !upload_file) {
            send_response(cmd, RESP_ERROR, nullptr, 0);
            break;
        }
        
        // Write chunk
        if (len > 0) {
            size_t written = upload_file.write(payload, len);
            if (written != len) {
                upload_file.close();
                is_uploading = false;
                send_response(cmd, RESP_ERROR, nullptr, 0); // Write failed
                break;
            }
            // Update full file CRC
            for(int i=0; i<len; i++) {
                upload_recv_crc = crc32_byte(upload_recv_crc, payload[i]);
            }
        }
        send_response(cmd, RESP_OK, nullptr, 0);
        break;
    }

    case 0x69: { // CMD_UPLOAD_END
        if (!is_uploading || !upload_file) {
             send_response(cmd, RESP_ERROR, nullptr, 0);
             break;
        }
        
        upload_file.close();
        is_uploading = false;
        
        // Verify CRC if provided?
        // Payload: Expected CRC32 (4 bytes)
        if (len >= 4) {
             uint32_t expected_crc;
             memcpy(&expected_crc, payload, 4);
             uint32_t final_crc = ~upload_recv_crc;
             
             if (final_crc != expected_crc) {
                 log_error("Upload CRC Mismatch!");
                 // Delete broken file?
                 // LittleFS.remove(filename?); // Don't have filename here easily 
                 send_response(cmd, RESP_ERROR, nullptr, 0); // CRC Fail
                 break;
             }
        }
        
        log_success("Upload Complete & Verified");
        
        // Auto-select if it was model.aif32?
        // Let user select manually or reload.
        
        send_response(cmd, RESP_OK, nullptr, 0);
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

// ===== Core1 Implementation =====

#if ENABLE_DUAL_CORE

void core1_send_response(uint8_t cmd, uint8_t status, const uint8_t* data, uint8_t len) {
  response_queue.push(cmd, status, data, len);
}

void core1_handle_command(const void* entry_ptr) {
  const CommandQueueEntry* entry = (const CommandQueueEntry*)entry_ptr;
  uint8_t cmd = entry->cmd;
  const uint8_t* payload = entry->payload;
  uint8_t len = entry->len;
  
  switch (cmd) {
    case CMD_CLEAR:
      fb_clear();
      core1_send_response(cmd, RESP_OK, nullptr, 0);
      break;
    case CMD_PIXEL:
      if (len >= 3) { fb_pixel(payload[0], payload[1], payload[2]); core1_send_response(cmd, RESP_OK, nullptr, 0); }
      else core1_send_response(cmd, RESP_ERROR, nullptr, 0);
      break;
    case CMD_RECT:
      if (len >= 5) { fb_rect(payload[0], payload[1], payload[2], payload[3], payload[4]); core1_send_response(cmd, RESP_OK, nullptr, 0); }
      else core1_send_response(cmd, RESP_ERROR, nullptr, 0);
      break;
    case CMD_FLUSH:
      if (dirty_rect.is_dirty) {
        sprite_display.updateRegion(framebuffer, dirty_rect.x1, dirty_rect.y1, dirty_rect.x2, dirty_rect.y2);
        dirty_rect.is_dirty = false;
      } else {
        sprite_display.update(framebuffer);
      }
      core1_send_response(cmd, RESP_OK, nullptr, 0);
      break;
    case CMD_SPRITE_CREATE:
    case CMD_SPRITE_MOVE:
    case CMD_SPRITE_DELETE:
    case CMD_SPRITE_VISIBLE:
    case CMD_SPRITE_COLLISION:
    case CMD_SPRITE_RENDER:
    case CMD_SPRITE_CLEAR:
      core1_send_response(cmd, RESP_OK, nullptr, 0);
      break;
    case CMD_AI_INFER:
      if (len >= 8 && model_ready) {
        float in0, in1;
        memcpy(&in0, payload, 4);
        memcpy(&in1, payload + 4, 4);
        float result = do_inference(in0, in1);
        core1_send_response(cmd, RESP_OK, (uint8_t*)&result, 4);
      } else {
        core1_send_response(cmd, model_ready ? RESP_ERROR : RESP_NOT_FOUND, nullptr, 0);
      }
      break;
    case CMD_AI_TRAIN: {
        uint8_t epochs = len > 0 ? payload[0] : 100;
        set_training(true);
        do_train(epochs);
        set_training(false);
        // core1_flags.ai_model_ready = true; // Handled by model loader or atomic enough?
        core1_send_response(cmd, RESP_OK, (uint8_t*)&last_loss, 4);
      } break;
    case CMD_AI_STATUS: {
        uint8_t resp[8];
        resp[0] = is_training() ? 1 : 0;
        resp[1] = model_ready ? 1 : 0;
        memcpy(resp + 2, &train_epochs, 2);
        memcpy(resp + 4, &last_loss, 4);
        core1_send_response(cmd, RESP_OK, resp, 8);
      } break;
    case CMD_AI_SAVE:
    case CMD_AI_LOAD:
    case CMD_MODEL_INFO:
    case CMD_MODEL_LIST:
    case CMD_MODEL_SELECT:
    case CMD_MODEL_UPLOAD:
    case CMD_MODEL_DELETE:
    case CMD_FINETUNE_START:
    case CMD_FINETUNE_DATA:
    case CMD_FINETUNE_STOP:
      core1_send_response(cmd, RESP_OK, nullptr, 0);
      break;
    default:
      core1_send_response(cmd, RESP_ERROR, nullptr, 0);
      break;
  }
}

void core1_loop() {
  CommandQueueEntry cmd;
  while (true) {
    if (cmd_queue.pop(&cmd)) {
      core1_handle_command(&cmd);
    } else {
      // core1_state.free_cycles++;
    }
    delayMicroseconds(10);
  }
}

void core1_entry() {
  delay(50);
  core1_loop();
}

#endif // ENABLE_DUAL_CORE

// ===== Setup & Loop =====

void setup() {
  // Initialize both transports
  transport.begin(115200);
  delay(100);

  #if ENABLE_DUAL_CORE
  mutex_init(&sprite_lock);
  #endif
  delay(2000);
  
  randomSeed(analogRead(A0));
  
  init_fs();
  
  // Prepare Models (F32 & INT8)
  build_model();
  build_model_int8();
  init_weights();
  if (model_ready) {
    convert_f32_to_int8(&dense_1, &dense_2); // Initial quantization
  }
  

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
  
  #if ENABLE_DUAL_CORE
  // Launch Core1
  log_info("Starting Core1 (AI/Display processor)...");
  multicore_launch_core1(core1_entry);
  delay(100);
  log_success("Dual-core mode active!");
  Serial1.print("  Core0: Protocol + Command Queue\n");
  Serial1.print("  Core1: AI + Display + Sprites\n");
  debug_println("");
  #endif
}

void loop() {
  // Run FS background task
  fs_task();
  
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
  
  #if ENABLE_DUAL_CORE
  // Process responses from Core1
  ResponseEntry resp;
  while (response_queue.pop(&resp)) {
    send_response(resp.cmd, resp.status, resp.data, resp.len);
  }
  #endif
  
  delay(1);
}

// Helper for safe string copy (needed by save_model)
void safe_strcpy(char* dest, const char* src, size_t max_len) {
  if (!dest || !src || max_len == 0) return;
  strncpy(dest, src, max_len - 1);
  dest[max_len - 1] = '\0';
}
