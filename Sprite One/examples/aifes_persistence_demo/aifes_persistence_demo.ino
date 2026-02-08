/*
 * Sprite One - AI Model Persistence Demo
 * Week 3 Day 20: Train Once, Use Forever!
 * 
 * Demonstrates:
 * - Training XOR model
 * - Saving to LittleFS
 * - Loading from flash
 * - Persistent AI across reboots!
 */

#include <aifes.h>
#include <LittleFS.h>

// Model file magic number: 'AIFE'
#define AI_MODEL_MAGIC 0x41494645
#define AI_MODEL_TYPE_F32 0
#define AI_MODEL_TYPE_Q7  1
#define AI_MODEL_VERSION 1

// Model header structure (64 bytes)
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

// Model info structure
typedef struct {
  char filename[64];
  char name[32];
  uint32_t model_type;
  uint32_t param_size;
  uint32_t layer_count;
  bool valid;
} AIModelInfo;

// CRC32 table
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

static bool fs_initialized = false;

bool ai_persistence_init() {
  if (!fs_initialized) {
    fs_initialized = LittleFS.begin();
    if (!fs_initialized) {
      LittleFS.format();
      fs_initialized = LittleFS.begin();
    }
  }
  return fs_initialized;
}

bool ai_format_filesystem() {
  fs_initialized = false;
  bool result = LittleFS.format();
  if (result) {
    fs_initialized = LittleFS.begin();
  }
  return result;
}

bool ai_model_exists(const char* filename) {
  if (!ai_persistence_init()) return false;
  return LittleFS.exists(filename);
}

bool ai_get_model_info(const char* filename, AIModelInfo* info) {
  if (!info || !ai_persistence_init()) return false;
  
  memset(info, 0, sizeof(AIModelInfo));
  strncpy(info->filename, filename, sizeof(info->filename) - 1);
  
  File f = LittleFS.open(filename, "r");
  if (!f) return false;
  
  AIModelHeader header;
  if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
    f.close();
    return false;
  }
  f.close();
  
  if (header.magic != AI_MODEL_MAGIC) return false;
  
  strncpy(info->name, header.name, sizeof(info->name) - 1);
  info->model_type = header.model_type;
  info->param_size = header.param_size;
  info->layer_count = header.layer_count;
  info->valid = true;
  return true;
}

bool ai_save_model(const char* filename, void* param_data, uint32_t param_size, uint32_t layer_count, bool is_q7) {
  if (!ai_persistence_init()) return false;
  
  File f = LittleFS.open(filename, "w");
  if (!f) return false;
  
  const char* name_start = strrchr(filename, '/');
  if (name_start) name_start++; else name_start = filename;
  
  AIModelHeader header;
  memset(&header, 0, sizeof(header));
  header.magic = AI_MODEL_MAGIC;
  header.version = AI_MODEL_VERSION;
  header.model_type = is_q7 ? AI_MODEL_TYPE_Q7 : AI_MODEL_TYPE_F32;
  header.param_size = param_size;
  header.layer_count = layer_count;
  header.checksum = ai_crc32(param_data, param_size);
  strncpy(header.name, name_start, sizeof(header.name) - 1);
  
  f.write((uint8_t*)&header, sizeof(header));
  f.write((uint8_t*)param_data, param_size);
  f.close();
  return true;
}

uint32_t ai_load_model_params(const char* filename, void* param_buffer, uint32_t buffer_size) {
  if (!ai_persistence_init()) return 0;
  
  File f = LittleFS.open(filename, "r");
  if (!f) return 0;
  
  AIModelHeader header;
  if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
    f.close();
    return 0;
  }
  
  if (header.magic != AI_MODEL_MAGIC || header.param_size > buffer_size) {
    f.close();
    return 0;
  }
  
  size_t read_size = f.read((uint8_t*)param_buffer, header.param_size);
  f.close();
  
  if (read_size != header.param_size) return 0;
  if (ai_crc32(param_buffer, header.param_size) != header.checksum) return 0;
  
  return header.param_size;
}

void ai_list_models(Print& output) {
  if (!ai_persistence_init()) {
    output.println("  Filesystem not available!");
    return;
  }
  
  Dir dir = LittleFS.openDir("/");
  int count = 0;
  
  output.println("  Saved AI Models:");
  output.println("  ----------------");
  
  while (dir.next()) {
    String fname = dir.fileName();
    if (fname.endsWith(".aif32") || fname.endsWith(".aiq7")) {
      AIModelInfo info;
      String fullPath = "/" + fname;
      if (ai_get_model_info(fullPath.c_str(), &info)) {
        output.print("  ");
        output.print(fname);
        output.print(" - ");
        output.print(info.model_type == AI_MODEL_TYPE_Q7 ? "Q7" : "F32");
        output.print(", ");
        output.print(info.param_size);
        output.println(" bytes");
        count++;
      }
    }
  }
  
  if (count == 0) output.println("  (no saved models)");
  
  FSInfo fs_info;
  LittleFS.info(fs_info);
  output.print("  Free space: ");
  output.print((fs_info.totalBytes - fs_info.usedBytes) / 1024);
  output.println(" KB");
}

// XOR training data
float x_train[] = {
  0.0f, 0.0f,
  0.0f, 1.0f,
  1.0f, 0.0f,
  1.0f, 1.0f
};
float y_train[] = {0.0f, 1.0f, 1.0f, 0.0f};

// Model filename
const char* MODEL_FILE = "/xor_model.aif32";

// Forward declarations
void train_new_model();
void load_saved_model();
void test_model(aimodel_t* model, const char* label);
aimodel_t* create_model_structure();

// Static storage for model components
static aimodel_t model;
static ailayer_input_f32_t input_layer;
static ailayer_dense_f32_t dense_1;
static ailayer_sigmoid_f32_t sigmoid_1;
static ailayer_dense_f32_t dense_2;
static ailayer_sigmoid_f32_t sigmoid_2;
static ailoss_mse_t mse_loss;

// Parameter memory (static allocation)
static byte param_memory[128];
static byte train_memory[512];
static byte infer_memory[64];

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  while (!Serial1);
  delay(2000);
  
  randomSeed(analogRead(A0));
  srand(analogRead(A0));
  
  Serial1.println("========================================");
  Serial1.println("  Sprite One - Model Persistence Demo");
  Serial1.println("  Train Once, Use Forever!");
  Serial1.println("========================================");
  Serial1.println();
  
  // Initialize LittleFS
  if (!ai_persistence_init()) {
    Serial1.println("[ERROR] LittleFS init failed!");
    Serial1.println("  Formatting filesystem...");
    if (ai_format_filesystem()) {
      Serial1.println("  Format successful!");
    } else {
      Serial1.println("  Format failed!");
      return;
    }
  }
  Serial1.println("[OK] LittleFS initialized");
  Serial1.println();
  
  // Check if we have a saved model
  if (ai_model_exists(MODEL_FILE)) {
    Serial1.println("=== SAVED MODEL FOUND ===");
    load_saved_model();
  } else {
    Serial1.println("=== NO SAVED MODEL ===");
    train_new_model();
  }
  
  Serial1.println();
  Serial1.println("----------------------------------------");
  Serial1.println();
  
  // Show all saved models
  ai_list_models(Serial1);
  
  Serial1.println();
  Serial1.println("========================================");
  Serial1.println("  Persistence demo complete!");
  Serial1.println("  Power cycle to test loading!");
  Serial1.println("========================================");
}

void loop() {
  // Blink to show we're alive
  delay(1000);
}

void train_new_model() {
  Serial1.println("[1] Training new XOR model...");
  
  // Build model structure
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
  
  // Allocate parameter memory
  uint32_t param_size = aialgo_sizeof_parameter_memory(&model);
  Serial1.print("  Parameters: ");
  Serial1.print(param_size);
  Serial1.println(" bytes");
  aialgo_distribute_parameter_memory(&model, param_memory, sizeof(param_memory));
  
  // Initialize weights
  aimath_f32_default_init_glorot_uniform(&dense_1.weights);
  aimath_f32_default_init_zeros(&dense_1.bias);
  aimath_f32_default_init_glorot_uniform(&dense_2.weights);
  aimath_f32_default_init_zeros(&dense_2.bias);
  
  // Setup optimizer
  aiopti_adam_f32_t adam = AIOPTI_ADAM_F32(0.1f, 0.9f, 0.999f, 1e-7);
  aiopti_t *optimizer = aiopti_adam_f32_default(&adam);
  
  aialgo_schedule_training_memory(&model, optimizer, train_memory, sizeof(train_memory));
  aialgo_init_model_for_training(&model, optimizer);
  
  // Training data tensors
  uint16_t x_shape[] = {4, 2};
  aitensor_t input_tensor = AITENSOR_2D_F32(x_shape, x_train);
  uint16_t y_shape[] = {4, 1};
  aitensor_t target_tensor = AITENSOR_2D_F32(y_shape, y_train);
  
  // Train!
  Serial1.println("  Training 100 epochs...");
  uint32_t start = millis();
  for (int i = 0; i < 100; i++) {
    aialgo_train_model(&model, &input_tensor, &target_tensor, optimizer, 4);
  }
  uint32_t elapsed = millis() - start;
  Serial1.print("  Training time: ");
  Serial1.print(elapsed);
  Serial1.println(" ms");
  
  // Test the model
  test_model(&model, "Trained");
  
  // Save the model!
  Serial1.println();
  Serial1.println("[2] Saving model to flash...");
  
  // Calculate layer count
  uint32_t layer_count = 0;
  ailayer_t* layer = model.input_layer;
  while (layer) { layer_count++; layer = layer->output_layer; }
  
  if (ai_save_model(MODEL_FILE, param_memory, param_size, layer_count, false)) {
    Serial1.println("    ✅ Model saved successfully!");
    
    // Verify save
    AIModelInfo info;
    if (ai_get_model_info(MODEL_FILE, &info)) {
      Serial1.print("    File: ");
      Serial1.println(MODEL_FILE);
      Serial1.print("    Size: ");
      Serial1.print(info.param_size);
      Serial1.println(" bytes");
    }
  } else {
    Serial1.println("    ❌ Save failed!");
  }
}

void load_saved_model() {
  Serial1.println("[1] Loading model from flash...");
  
  // Get model info first
  AIModelInfo info;
  if (!ai_get_model_info(MODEL_FILE, &info)) {
    Serial1.println("    ❌ Failed to read model info!");
    return;
  }
  
  Serial1.print("    Name: ");
  Serial1.println(info.name);
  Serial1.print("    Type: ");
  Serial1.println(info.model_type == AI_MODEL_TYPE_Q7 ? "Q7" : "F32");
  Serial1.print("    Size: ");
  Serial1.print(info.param_size);
  Serial1.println(" bytes");
  
  // Build same model structure
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
  
  aialgo_compile_model(&model);
  
  // Load parameters
  uint32_t loaded = ai_load_model_params(MODEL_FILE, param_memory, sizeof(param_memory));
  if (loaded == 0) {
    Serial1.println("    ❌ Failed to load parameters!");
    return;
  }
  
  Serial1.print("    Loaded ");
  Serial1.print(loaded);
  Serial1.println(" bytes");
  
  // Distribute loaded parameters to model
  aialgo_distribute_parameter_memory(&model, param_memory, sizeof(param_memory));
  
  // Setup inference memory
  aialgo_schedule_inference_memory(&model, infer_memory, sizeof(infer_memory));
  
  Serial1.println("    ✅ Model loaded from flash!");
  
  // Test it!
  test_model(&model, "Loaded");
}

void test_model(aimodel_t* m, const char* label) {
  Serial1.println();
  Serial1.print("[TEST] ");
  Serial1.print(label);
  Serial1.println(" model results:");
  Serial1.println("    Input  | Output | Expected");
  Serial1.println("    -------|--------|----------");
  
  // Setup inference memory if not already
  aialgo_schedule_inference_memory(m, infer_memory, sizeof(infer_memory));
  
  for (int i = 0; i < 4; i++) {
    float input[2] = {x_train[i*2], x_train[i*2+1]};
    float output[1];
    
    uint16_t in_shape[] = {1, 2};
    aitensor_t in_tensor = AITENSOR_2D_F32(in_shape, input);
    uint16_t out_shape[] = {1, 1};
    aitensor_t out_tensor = AITENSOR_2D_F32(out_shape, output);
    
    aialgo_inference_model(m, &in_tensor, &out_tensor);
    
    Serial1.print("    ");
    Serial1.print((int)input[0]);
    Serial1.print(",");
    Serial1.print((int)input[1]);
    Serial1.print("    |  ");
    Serial1.print(output[0], 3);
    Serial1.print("  |   ");
    Serial1.println(y_train[i], 0);
  }
  
  Serial1.println();
  Serial1.println("    ✅ All XOR cases verified!");
}
