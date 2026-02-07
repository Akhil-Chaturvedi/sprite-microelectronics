/*
 * Sprite One - AI Protocol Integration Demo
 * Week 3 Day 21: Complete AI System!
 * 
 * Demonstrates:
 * - Receiving AI commands over serial
 * - Running inference on request
 * - Training on demand
 * - Saving/loading models via protocol
 * - Full integration of all Week 3 features!
 */

#include <aifes.h>
#include <LittleFS.h>

// ============ Protocol Definitions ============
#define SPRITE_HEADER       0xAA
#define SPRITE_ACK          0x00
#define SPRITE_NAK          0x01

// AI Commands
#define CMD_AI_INFER        0x50
#define CMD_AI_TRAIN        0x51
#define CMD_AI_STATUS       0x52
#define CMD_AI_SAVE         0x53
#define CMD_AI_LOAD         0x54
#define CMD_AI_LIST         0x55
#define CMD_AI_DELETE       0x56

// AI Response Codes
#define AI_OK               0x00
#define AI_ERROR            0x01
#define AI_NOT_FOUND        0x02
#define AI_BUSY             0x03

// AI States
#define STATE_IDLE          0
#define STATE_TRAINING      1
#define STATE_INFERRING     2

// ============ Inline Persistence (from Day 20) ============
#define AI_MODEL_MAGIC 0x41494645

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t model_type;
  uint32_t param_size;
  uint32_t checksum;
  uint32_t layer_count;
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
bool init_fs() {
  if (!fs_init) {
    fs_init = LittleFS.begin();
    if (!fs_init) { LittleFS.format(); fs_init = LittleFS.begin(); }
  }
  return fs_init;
}

// ============ AI Engine State ============
static uint8_t ai_state = STATE_IDLE;
static bool model_loaded = false;
static float last_outputs[4] = {0};
static uint16_t training_epochs = 0;
static float last_loss = 0;

// XOR model components
static aimodel_t model;
static ailayer_input_f32_t input_layer;
static ailayer_dense_f32_t dense_1;
static ailayer_sigmoid_f32_t sigmoid_1;
static ailayer_dense_f32_t dense_2;
static ailayer_sigmoid_f32_t sigmoid_2;
static ailoss_mse_t mse_loss;
static byte param_memory[128];
static byte train_memory[512];
static byte infer_memory[64];

// XOR training data
float x_train[] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f};
float y_train[] = {0.0f, 1.0f, 1.0f, 0.0f};

// ============ Protocol Functions ============

uint8_t calc_checksum(const uint8_t* data, uint8_t len) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < len; i++) sum += data[i];
  return ~sum + 1;
}

void send_response(uint8_t cmd, uint8_t status, const uint8_t* data, uint8_t len) {
  Serial1.write(SPRITE_HEADER);
  Serial1.write(cmd);
  Serial1.write(status);
  Serial1.write(len);
  if (len > 0 && data != nullptr) {
    Serial1.write(data, len);
  }
  uint8_t chk = calc_checksum(data, len);
  Serial1.write(chk);
}

// ============ AI Functions ============

void build_model() {
  uint16_t input_shape[] = {1, 2};
  input_layer = AILAYER_INPUT_F32_A(2, input_shape);
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
  aialgo_distribute_parameter_memory(&model, param_memory, sizeof(param_memory));
}

void init_weights() {
  aimath_f32_default_init_glorot_uniform(&dense_1.weights);
  aimath_f32_default_init_zeros(&dense_1.bias);
  aimath_f32_default_init_glorot_uniform(&dense_2.weights);
  aimath_f32_default_init_zeros(&dense_2.bias);
}

uint8_t do_inference(float in0, float in1, float* result) {
  if (!model_loaded) return AI_NOT_FOUND;
  
  ai_state = STATE_INFERRING;
  
  float inputs[2] = {in0, in1};
  float output[1];
  
  uint16_t in_shape[] = {1, 2};
  aitensor_t in_tensor = AITENSOR_2D_F32(in_shape, inputs);
  uint16_t out_shape[] = {1, 1};
  aitensor_t out_tensor = AITENSOR_2D_F32(out_shape, output);
  
  aialgo_schedule_inference_memory(&model, infer_memory, sizeof(infer_memory));
  aialgo_inference_model(&model, &in_tensor, &out_tensor);
  
  *result = output[0];
  last_outputs[0] = output[0];
  
  ai_state = STATE_IDLE;
  return AI_OK;
}

uint8_t do_train(uint8_t epochs) {
  ai_state = STATE_TRAINING;
  
  build_model();
  init_weights();
  
  aiopti_adam_f32_t adam = AIOPTI_ADAM_F32(0.1f, 0.9f, 0.999f, 1e-7);
  aiopti_t *optimizer = aiopti_adam_f32_default(&adam);
  aialgo_schedule_training_memory(&model, optimizer, train_memory, sizeof(train_memory));
  aialgo_init_model_for_training(&model, optimizer);
  
  uint16_t x_shape[] = {4, 2};
  aitensor_t input_tensor = AITENSOR_2D_F32(x_shape, x_train);
  uint16_t y_shape[] = {4, 1};
  aitensor_t target_tensor = AITENSOR_2D_F32(y_shape, y_train);
  
  for (int i = 0; i < epochs; i++) {
    last_loss = aialgo_train_model(&model, &input_tensor, &target_tensor, optimizer, 4);
    training_epochs++;
  }
  
  model_loaded = true;
  ai_state = STATE_IDLE;
  return AI_OK;
}

uint8_t do_save(const char* filename) {
  if (!model_loaded) return AI_NOT_FOUND;
  if (!init_fs()) return AI_ERROR;
  
  File f = LittleFS.open(filename, "w");
  if (!f) return AI_ERROR;
  
  uint32_t param_size = aialgo_sizeof_parameter_memory(&model);
  
  AIModelHeader header;
  memset(&header, 0, sizeof(header));
  header.magic = AI_MODEL_MAGIC;
  header.version = 1;
  header.model_type = 0;
  header.param_size = param_size;
  header.layer_count = 5;
  header.checksum = ai_crc32(param_memory, param_size);
  strncpy(header.name, filename, sizeof(header.name) - 1);
  
  f.write((uint8_t*)&header, sizeof(header));
  f.write(param_memory, param_size);
  f.close();
  
  return AI_OK;
}

uint8_t do_load(const char* filename) {
  if (!init_fs()) return AI_ERROR;
  if (!LittleFS.exists(filename)) return AI_NOT_FOUND;
  
  File f = LittleFS.open(filename, "r");
  if (!f) return AI_ERROR;
  
  AIModelHeader header;
  if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
    f.close();
    return AI_ERROR;
  }
  
  if (header.magic != AI_MODEL_MAGIC) {
    f.close();
    return AI_ERROR;
  }
  
  build_model();
  
  if (f.read(param_memory, header.param_size) != header.param_size) {
    f.close();
    return AI_ERROR;
  }
  f.close();
  
  if (ai_crc32(param_memory, header.param_size) != header.checksum) {
    return AI_ERROR;
  }
  
  aialgo_distribute_parameter_memory(&model, param_memory, sizeof(param_memory));
  model_loaded = true;
  
  return AI_OK;
}

// ============ Command Handlers ============

void handle_cmd_infer(const uint8_t* payload, uint8_t len) {
  if (len < 8) {
    send_response(CMD_AI_INFER, AI_ERROR, nullptr, 0);
    return;
  }
  
  float in0, in1;
  memcpy(&in0, payload, 4);
  memcpy(&in1, payload + 4, 4);
  
  float result;
  uint8_t status = do_inference(in0, in1, &result);
  
  if (status == AI_OK) {
    send_response(CMD_AI_INFER, AI_OK, (uint8_t*)&result, 4);
  } else {
    send_response(CMD_AI_INFER, status, nullptr, 0);
  }
}

void handle_cmd_train(const uint8_t* payload, uint8_t len) {
  uint8_t epochs = (len > 0) ? payload[0] : 100;
  
  uint8_t status = do_train(epochs);
  
  uint8_t resp[4];
  memcpy(resp, &last_loss, 4);
  send_response(CMD_AI_TRAIN, status, resp, 4);
}

void handle_cmd_status(const uint8_t* payload, uint8_t len) {
  uint8_t resp[16];
  resp[0] = ai_state;
  resp[1] = model_loaded ? 1 : 0;
  resp[2] = (training_epochs >> 8) & 0xFF;
  resp[3] = training_epochs & 0xFF;
  memcpy(resp + 4, &last_loss, 4);
  memcpy(resp + 8, &last_outputs[0], 4);
  
  send_response(CMD_AI_STATUS, AI_OK, resp, 12);
}

void handle_cmd_save(const uint8_t* payload, uint8_t len) {
  char filename[32] = "/model.aif32";
  if (len > 0 && len < sizeof(filename)) {
    memcpy(filename, payload, len);
    filename[len] = 0;
  }
  
  uint8_t status = do_save(filename);
  send_response(CMD_AI_SAVE, status, nullptr, 0);
}

void handle_cmd_load(const uint8_t* payload, uint8_t len) {
  char filename[32] = "/model.aif32";
  if (len > 0 && len < sizeof(filename)) {
    memcpy(filename, payload, len);
    filename[len] = 0;
  }
  
  uint8_t status = do_load(filename);
  send_response(CMD_AI_LOAD, status, nullptr, 0);
}

void handle_cmd_list(const uint8_t* payload, uint8_t len) {
  if (!init_fs()) {
    send_response(CMD_AI_LIST, AI_ERROR, nullptr, 0);
    return;
  }
  
  uint8_t resp[128];
  uint8_t pos = 0;
  uint8_t count = 0;
  
  Dir dir = LittleFS.openDir("/");
  while (dir.next() && pos < 120) {
    String name = dir.fileName();
    if (name.endsWith(".aif32") || name.endsWith(".aiq7")) {
      uint8_t nlen = name.length();
      if (pos + nlen + 1 < 120) {
        resp[pos++] = nlen;
        memcpy(resp + pos, name.c_str(), nlen);
        pos += nlen;
        count++;
      }
    }
  }
  
  resp[pos++] = 0; // Terminator
  send_response(CMD_AI_LIST, AI_OK, resp, pos);
}

void handle_cmd_delete(const uint8_t* payload, uint8_t len) {
  if (len == 0) {
    send_response(CMD_AI_DELETE, AI_ERROR, nullptr, 0);
    return;
  }
  
  char filename[32];
  memcpy(filename, payload, len);
  filename[len] = 0;
  
  if (!init_fs() || !LittleFS.exists(filename)) {
    send_response(CMD_AI_DELETE, AI_NOT_FOUND, nullptr, 0);
    return;
  }
  
  if (LittleFS.remove(filename)) {
    send_response(CMD_AI_DELETE, AI_OK, nullptr, 0);
  } else {
    send_response(CMD_AI_DELETE, AI_ERROR, nullptr, 0);
  }
}

void process_command(uint8_t cmd, const uint8_t* payload, uint8_t len) {
  switch (cmd) {
    case CMD_AI_INFER:  handle_cmd_infer(payload, len); break;
    case CMD_AI_TRAIN:  handle_cmd_train(payload, len); break;
    case CMD_AI_STATUS: handle_cmd_status(payload, len); break;
    case CMD_AI_SAVE:   handle_cmd_save(payload, len); break;
    case CMD_AI_LOAD:   handle_cmd_load(payload, len); break;
    case CMD_AI_LIST:   handle_cmd_list(payload, len); break;
    case CMD_AI_DELETE: handle_cmd_delete(payload, len); break;
    default:
      send_response(cmd, AI_ERROR, nullptr, 0);
      break;
  }
}

// ============ Main Program ============

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  while (!Serial1);
  delay(2000);
  
  randomSeed(analogRead(A0));
  srand(analogRead(A0));
  
  Serial1.println("========================================");
  Serial1.println("  Sprite One - AI Protocol Demo");
  Serial1.println("  Week 3 Day 21 - Complete System!");
  Serial1.println("========================================");
  Serial1.println();
  
  // Initialize filesystem
  if (!init_fs()) {
    Serial1.println("[WARN] LittleFS init failed, formatting...");
    LittleFS.format();
    init_fs();
  }
  Serial1.println("[OK] LittleFS initialized");
  
  // Check for saved model
  if (LittleFS.exists("/model.aif32")) {
    Serial1.println("[OK] Loading saved model...");
    if (do_load("/model.aif32") == AI_OK) {
      Serial1.println("     Model loaded from flash!");
    }
  } else {
    Serial1.println("[INFO] No saved model, training XOR...");
    do_train(100);
    Serial1.println("[OK] Training complete!");
    do_save("/model.aif32");
    Serial1.println("[OK] Model saved!");
  }
  
  // Test inference
  Serial1.println();
  Serial1.println("=== XOR Inference Test ===");
  for (int i = 0; i < 4; i++) {
    float in0 = x_train[i*2];
    float in1 = x_train[i*2+1];
    float result;
    do_inference(in0, in1, &result);
    Serial1.print("  ");
    Serial1.print((int)in0);
    Serial1.print(" XOR ");
    Serial1.print((int)in1);
    Serial1.print(" = ");
    Serial1.print(result, 3);
    Serial1.print(" (expect ");
    Serial1.print(y_train[i], 0);
    Serial1.println(")");
  }
  
  Serial1.println();
  Serial1.println("=== Protocol Commands Ready ===");
  Serial1.println("  0x50: CMD_AI_INFER [in0, in1]");
  Serial1.println("  0x51: CMD_AI_TRAIN [epochs]");
  Serial1.println("  0x52: CMD_AI_STATUS");
  Serial1.println("  0x53: CMD_AI_SAVE [filename]");
  Serial1.println("  0x54: CMD_AI_LOAD [filename]");
  Serial1.println("  0x55: CMD_AI_LIST");
  Serial1.println("  0x56: CMD_AI_DELETE [filename]");
  Serial1.println();
  Serial1.println("Waiting for commands (AA XX LEN DATA CHK)...");
  Serial1.println("========================================");
}

// Protocol state machine
static uint8_t rx_state = 0;
static uint8_t rx_cmd = 0;
static uint8_t rx_len = 0;
static uint8_t rx_buf[64];
static uint8_t rx_pos = 0;

void loop() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    
    switch (rx_state) {
      case 0: // Wait for header
        if (b == SPRITE_HEADER) rx_state = 1;
        break;
        
      case 1: // Get command
        rx_cmd = b;
        rx_state = 2;
        break;
        
      case 2: // Get length
        rx_len = b;
        rx_pos = 0;
        rx_state = (rx_len > 0) ? 3 : 4;
        break;
        
      case 3: // Get payload
        if (rx_pos < sizeof(rx_buf)) {
          rx_buf[rx_pos++] = b;
        }
        if (rx_pos >= rx_len) rx_state = 4;
        break;
        
      case 4: // Get checksum (ignored for demo)
        process_command(rx_cmd, rx_buf, rx_len);
        rx_state = 0;
        break;
    }
  }
  
  delay(1);
}
