/**
 * Sprite One - Enhanced Configuration & Utilities
 * Week 4 Day 27: Polish
 * 
 * Centralized configuration and helper utilities for better UX.
 */

#ifndef SPRITE_CONFIG_H
#define SPRITE_CONFIG_H

#include <Arduino.h>

// ============ VERSION ============
#define SPRITE_VERSION_MAJOR    1
#define SPRITE_VERSION_MINOR    0
#define SPRITE_VERSION_PATCH    0
#define SPRITE_BUILD_DATE       __DATE__
#define SPRITE_BUILD_TIME       __TIME__

// ============ DEBUG CONFIGURATION ============
#define SPRITE_DEBUG_ENABLED    true
#define SPRITE_VERBOSE          false
#define SPRITE_LOG_TIMING       false
#define SPRITE_LOG_MEMORY       false

// ============ HARDWARE CONFIGURATION ============
#define SPRITE_UART_BAUD        115200
#define SPRITE_UART_RX_BUF      256
#define SPRITE_UART_TX_BUF      256

// ============ AI CONFIGURATION ============
#define AI_DEFAULT_EPOCHS       100
#define AI_MAX_EPOCHS           1000
#define AI_LEARNING_RATE        0.1f
#define AI_MIN_LOSS_THRESHOLD   0.001f

// ============ PROTOCOL CONFIGURATION ============
#define PROTOCOL_TIMEOUT_MS     2000
#define PROTOCOL_RETRY_COUNT    3
#define PROTOCOL_MAX_PAYLOAD    255

// ============ FILESYSTEM CONFIGURATION ============
#define FS_AUTO_FORMAT          true
#define FS_MAX_OPEN_FILES       4
#define FS_CACHE_SIZE           512

// ============ DISPLAY CONFIGURATION ============
#define DISPLAY_WIDTH           128
#define DISPLAY_HEIGHT          64
#define DISPLAY_REFRESH_RATE    30  // Hz

// ============ LOGGING MACROS ============
#if SPRITE_DEBUG_ENABLED
  #define LOG_INFO(msg)         Serial1.print("[INFO] "); Serial1.println(msg)
  #define LOG_WARN(msg)         Serial1.print("[WARN] "); Serial1.println(msg)
  #define LOG_ERROR(msg)        Serial1.print("[ERROR] "); Serial1.println(msg)
  #define LOG_DEBUG(msg)        if(SPRITE_VERBOSE) { Serial1.print("[DEBUG] "); Serial1.println(msg); }
#else
  #define LOG_INFO(msg)
  #define LOG_WARN(msg)
  #define LOG_ERROR(msg)
  #define LOG_DEBUG(msg)
#endif

// ============ TIMING MACROS ============
#if SPRITE_LOG_TIMING
  #define TIME_START()          uint32_t _start = millis()
  #define TIME_END(name)        Serial1.print("[TIME] "); Serial1.print(name); \
                                Serial1.print(": "); Serial1.print(millis() - _start); \
                                Serial1.println(" ms")
#else
  #define TIME_START()
  #define TIME_END(name)
#endif

// ============ MEMORY LOGGING ============
#if SPRITE_LOG_MEMORY
  #define LOG_MEMORY()          log_memory_usage()
  void log_memory_usage() {
    extern char __StackLimit, __bss_end__;
    char top;
    Serial1.print("[MEM] Stack: ");
    Serial1.print(&top - __bss_end__);
    Serial1.println(" bytes");
  }
#else
  #define LOG_MEMORY()
#endif

// ============ ERROR CODES ============
enum SpriteError {
  ERR_OK = 0,
  ERR_INVALID_CMD = 1,
  ERR_INVALID_PARAM = 2,
  ERR_NOT_INITIALIZED = 3,
  ERR_TIMEOUT = 4,
  ERR_FS_ERROR = 5,
  ERR_MODEL_NOT_LOADED = 6,
  ERR_TRAINING_FAILED = 7,
  ERR_INFERENCE_FAILED = 8,
  ERR_CHECKSUM_MISMATCH = 9,
  ERR_OUT_OF_MEMORY = 10
};

// ============ STATUS INDICATORS ============
enum SpriteStatus {
  STATUS_IDLE = 0,
  STATUS_BUSY = 1,
  STATUS_TRAINING = 2,
  STATUS_INFERRING = 3,
  STATUS_SAVING = 4,
  STATUS_LOADING = 5,
  STATUS_ERROR = 255
};

// ============ HELPER FUNCTIONS ============

/**
 * Print startup banner with version info
 */
inline void print_startup_banner() {
  Serial1.println(F("╔════════════════════════════════════════╗"));
  Serial1.print(F("║     SPRITE ONE v"));
  Serial1.print(SPRITE_VERSION_MAJOR);
  Serial1.print(F("."));
  Serial1.print(SPRITE_VERSION_MINOR);
  Serial1.print(F("."));
  Serial1.print(SPRITE_VERSION_PATCH);
  Serial1.println(F("                 ║"));
  Serial1.println(F("║     Graphics & AI Accelerator          ║"));
  Serial1.println(F("╚════════════════════════════════════════╝"));
  Serial1.print(F("Build: "));
  Serial1.print(SPRITE_BUILD_DATE);
  Serial1.print(F(" "));
  Serial1.println(SPRITE_BUILD_TIME);
  Serial1.println();
}

/**
 * Format bytes into human-readable string
 */
inline String format_bytes(uint32_t bytes) {
  if (bytes < 1024) return String(bytes) + "B";
  if (bytes < 1024*1024) return String(bytes/1024) + "KB";
  return String(bytes/(1024*1024)) + "MB";
}

/**
 * Format milliseconds into human-readable string
 */
inline String format_time(uint32_t ms) {
  if (ms < 1000) return String(ms) + "ms";
  if (ms < 60000) return String(ms/1000.0, 1) + "s";
  return String(ms/60000) + "m " + String((ms%60000)/1000) + "s";
}

/**
 * Validate parameter range
 */
inline bool validate_range(int value, int min, int max, const char* name) {
  if (value < min || value > max) {
    LOG_ERROR(String(name) + " out of range: " + String(value));
    return false;
  }
  return true;
}

/**
 * Safe string copy with bounds checking
 */
inline void safe_strcpy(char* dest, const char* src, size_t max_len) {
  if (!dest || !src) return;
  strncpy(dest, src, max_len - 1);
  dest[max_len - 1] = '\0';
}

/**
 * Get free RAM
 */
inline uint32_t get_free_ram() {
  extern char __StackLimit, __bss_end__;
  char top;
  return &top - __bss_end__;
}

/**
 * Print system info
 */
inline void print_system_info() {
  Serial1.println(F("=== System Info ==="));
  Serial1.print(F("Flash: "));
  Serial1.print(format_bytes(2097152)); // 2MB
  Serial1.println();
  Serial1.print(F("RAM: "));
  Serial1.print(format_bytes(262144)); // 256KB
  Serial1.println();
  Serial1.print(F("Free RAM: "));
  Serial1.print(format_bytes(get_free_ram()));
  Serial1.println();
  Serial1.print(F("CPU: RP2040 @ "));
  Serial1.print(F_CPU / 1000000);
  Serial1.println(F(" MHz"));
  Serial1.println();
}

#endif // SPRITE_CONFIG_H
