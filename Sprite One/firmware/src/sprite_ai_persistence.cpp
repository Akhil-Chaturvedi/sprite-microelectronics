/*
 * Sprite One - AI Model Persistence Implementation
 * LittleFS-based storage for trained neural networks
 */

#include "sprite_ai_persistence.h"
#include <LittleFS.h>

// Filesystem initialization flag
static bool fs_initialized = false;

// CRC32 lookup table
static const uint32_t crc32_table[256] = {
  0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
  0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
  0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
  0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
  0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
  0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
  0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
  0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
  0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
  0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
  0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
  0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
  0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
  0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
  0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
  0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
  0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
  0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
  0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
  0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
  0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
  0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
  0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
  0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
  0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
  0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
  0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
  0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
  0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
  0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
  0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
  0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
  0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
  0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
  0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
  0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
  0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
  0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
  0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
  0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
  0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
  0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
  0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32_t ai_crc32(const void* data, size_t length) {
  uint32_t crc = 0xFFFFFFFF;
  const uint8_t* bytes = (const uint8_t*)data;
  
  for (size_t i = 0; i < length; i++) {
    crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
  }
  
  return crc ^ 0xFFFFFFFF;
}

bool ai_persistence_init() {
  if (!fs_initialized) {
    fs_initialized = LittleFS.begin();
    if (!fs_initialized) {
      // Try formatting if mount fails
      LittleFS.format();
      fs_initialized = LittleFS.begin();
    }
  }
  return fs_initialized;
}

bool ai_save_model(const char* filename, aimodel_t* model, bool is_q7) {
  if (!ai_persistence_init()) {
    return false;
  }
  
  // Get parameter size
  uint32_t param_size = aialgo_sizeof_parameter_memory(model);
  if (param_size == 0) {
    return false;
  }
  
  // Open file for writing
  File f = LittleFS.open(filename, "w");
  if (!f) {
    return false;
  }
  
  // Count layers
  uint32_t layer_count = 0;
  ailayer_t* layer = model->input_layer;
  while (layer != nullptr) {
    layer_count++;
    layer = layer->output_layer;
  }
  
  // Extract model name from filename
  const char* name_start = strrchr(filename, '/');
  if (name_start) name_start++; else name_start = filename;
  
  // Build header
  AIModelHeader header;
  memset(&header, 0, sizeof(header));
  header.magic = AI_MODEL_MAGIC;
  header.version = AI_MODEL_VERSION;
  header.model_type = is_q7 ? AI_MODEL_TYPE_Q7 : AI_MODEL_TYPE_F32;
  header.param_size = param_size;
  header.layer_count = layer_count;
  strncpy(header.name, name_start, sizeof(header.name) - 1);
  
  // Allocate temp buffer for parameters
  uint8_t* param_buffer = (uint8_t*)malloc(param_size);
  if (!param_buffer) {
    f.close();
    return false;
  }
  
  // Copy parameters from model
  // Note: We need to traverse layers and copy their parameter memory
  // For now, use the model's parameter base if available
  ailayer_t* current = model->input_layer;
  uint8_t* dest = param_buffer;
  
  while (current != nullptr) {
    // Copy trainable parameters (weights, biases)
    if (current->trainable_params) {
      for (uint32_t i = 0; i < current->trainable_params_count; i++) {
        aitensor_t* param_tensor = current->trainable_params[i];
        if (param_tensor && param_tensor->data) {
          size_t tensor_size = 1;
          for (int d = 0; d < param_tensor->dim; d++) {
            tensor_size *= param_tensor->shape[d];
          }
          // Size depends on dtype
          if (is_q7) {
            tensor_size *= 1;  // Q7 = 1 byte
          } else {
            tensor_size *= 4;  // F32 = 4 bytes
          }
          
          if (dest + tensor_size <= param_buffer + param_size) {
            memcpy(dest, param_tensor->data, tensor_size);
            dest += tensor_size;
          }
        }
      }
    }
    current = current->output_layer;
  }
  
  // Calculate checksum
  header.checksum = ai_crc32(param_buffer, param_size);
  
  // Write header
  size_t written = f.write((uint8_t*)&header, sizeof(header));
  if (written != sizeof(header)) {
    free(param_buffer);
    f.close();
    return false;
  }
  
  // Write parameters
  written = f.write(param_buffer, param_size);
  free(param_buffer);
  f.close();
  
  return written == param_size;
}

uint32_t ai_load_model_params(const char* filename, void* param_buffer, uint32_t buffer_size) {
  if (!ai_persistence_init()) {
    return 0;
  }
  
  File f = LittleFS.open(filename, "r");
  if (!f) {
    return 0;
  }
  
  // Read header
  AIModelHeader header;
  if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
    f.close();
    return 0;
  }
  
  // Validate magic
  if (header.magic != AI_MODEL_MAGIC) {
    f.close();
    return 0;
  }
  
  // Check buffer size
  if (header.param_size > buffer_size) {
    f.close();
    return 0;
  }
  
  // Read parameters
  size_t read_size = f.read((uint8_t*)param_buffer, header.param_size);
  f.close();
  
  if (read_size != header.param_size) {
    return 0;
  }
  
  // Verify checksum
  uint32_t calc_crc = ai_crc32(param_buffer, header.param_size);
  if (calc_crc != header.checksum) {
    return 0;  // Corrupted!
  }
  
  return header.param_size;
}

bool ai_model_exists(const char* filename) {
  if (!ai_persistence_init()) {
    return false;
  }
  return LittleFS.exists(filename);
}

bool ai_get_model_info(const char* filename, AIModelInfo* info) {
  if (!info || !ai_persistence_init()) {
    return false;
  }
  
  memset(info, 0, sizeof(AIModelInfo));
  strncpy(info->filename, filename, sizeof(info->filename) - 1);
  
  File f = LittleFS.open(filename, "r");
  if (!f) {
    return false;
  }
  
  AIModelHeader header;
  if (f.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
    f.close();
    return false;
  }
  f.close();
  
  if (header.magic != AI_MODEL_MAGIC) {
    return false;
  }
  
  strncpy(info->name, header.name, sizeof(info->name) - 1);
  info->model_type = header.model_type;
  info->param_size = header.param_size;
  info->layer_count = header.layer_count;
  info->valid = true;
  
  return true;
}

bool ai_delete_model(const char* filename) {
  if (!ai_persistence_init()) {
    return false;
  }
  return LittleFS.remove(filename);
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
    
    // Check if it's an AI model file
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
        output.print(" bytes, ");
        output.print(info.layer_count);
        output.println(" layers");
        count++;
      }
    }
  }
  
  if (count == 0) {
    output.println("  (no saved models)");
  }
  
  output.print("  Free space: ");
  output.print(ai_get_free_space() / 1024);
  output.println(" KB");
}

uint32_t ai_get_free_space() {
  if (!ai_persistence_init()) {
    return 0;
  }
  
  FSInfo info;
  LittleFS.info(info);
  return info.totalBytes - info.usedBytes;
}

bool ai_format_filesystem() {
  fs_initialized = false;
  bool result = LittleFS.format();
  if (result) {
    fs_initialized = LittleFS.begin();
  }
  return result;
}
