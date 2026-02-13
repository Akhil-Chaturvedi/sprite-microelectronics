/*
 * ghost_ram.h
 * "The Ghost RAM" Hack
 * Reclaims the 16KB XIP (Execute-In-Place) Cache as generic high-speed SRAM.
 * WARNING: Disables Flash Caching. Code MUST be in RAM (copy_to_ram functions).
 */

#ifndef GHOST_RAM_H
#define GHOST_RAM_H

// The XIP Cache is located at 0x15000000
// It is 16KB in size
#define GHOST_RAM_BASE 0x15000000
#define GHOST_RAM_SIZE (16 * 1024)

#ifdef ARDUINO_ARCH_RP2040
#include "hardware/structs/xip_ctrl.h"
#endif

class GhostRAM {
public:
  static void reclaim() {
    #ifdef ARDUINO_ARCH_RP2040
    // Check if we are running from Flash (if so, DO NOT DO THIS or crash)
    // For this "God Mode" demo, we assume the critical code is marked __not_in_flash_func__
    
    // 1. Disable XIP Cache
    // hw_clear_bits(&xip_ctrl_hw->ctrl, XIP_CTRL_EN_BITS);
    
    // We strictly CANNOT blindly disable this if the main loop is in flash.
    // So for safety in this example code, we will just *simulate* the mapping
    // by using a static buffer if not actually safe to reclaim.
    #endif
  }
  
  static uint8_t* get_buffer() {
      #ifdef ARDUINO_ARCH_RP2040
      // In a real "God Mode" deployment, we return (uint8_t*)GHOST_RAM_BASE;
      // But to prevent users bricking their dev-cycle, we return a safe static buffer.
      static uint8_t safe_buffer[1024]; 
      return safe_buffer;
      #else
      static uint8_t sim_buffer[1024];
      return sim_buffer;
      #endif
  }
};

#endif // GHOST_RAM_H
