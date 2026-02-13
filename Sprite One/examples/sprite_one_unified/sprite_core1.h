/**
 * Sprite Core1 - Dual Core Command Queue
 * 
 * Implements a lock-free ring buffer for inter-core communication.
 * Core 0 (protocol handler) enqueues commands.
 * Core 1 (AI/display) dequeues and executes them.
 */

#ifndef SPRITE_CORE1_H
#define SPRITE_CORE1_H

#include <Arduino.h>

// Command queue entry
struct CommandQueueEntry {
  uint8_t cmd;
  uint8_t len;
  uint8_t payload[64];
};

// Lock-free ring buffer (single producer, single consumer)
template<int SIZE>
class CommandQueue {
private:
  volatile CommandQueueEntry queue[SIZE];
  volatile uint32_t head;  // Write index (Core 0)
  volatile uint32_t tail;  // Read index (Core 1)
  
public:
  CommandQueue() : head(0), tail(0) {}
  
  // Enqueue (called from Core 0)
  bool push(uint8_t cmd, const uint8_t* payload, uint8_t len) {
    uint32_t next = (head + 1) % SIZE;
    if (next == tail) return false;  // Queue full
    
   queue[head].cmd = cmd;
    queue[head].len = len;
    if (len > 0 && payload) {
      memcpy((void*)queue[head].payload, payload, min(len, 64));
    }
    
    asm volatile("" ::: "memory");  // Compiler barrier
    head = next;
    return true;
  }
  
  // Dequeue (called from Core 1)
  bool pop(CommandQueueEntry* out) {
    if (tail == head) return false;  // Queue empty
    
    out->cmd = queue[tail].cmd;
    out->len = queue[tail].len;
    memcpy(out->payload, (void*)queue[tail].payload, queue[tail].len);
    
    asm volatile("" ::: "memory");  // Compiler barrier
    tail = (tail + 1) % SIZE;
    return true;
  }
  
  uint32_t count() const {
    if (head >= tail) return head - tail;
    return SIZE - (tail - head);
  }
  
  bool isEmpty() const { return head == tail; }
  bool isFull() const { return ((head + 1) % SIZE) == tail; }
};

// Global command queue (16 entries max)
static CommandQueue<16> cmd_queue;

// Response queue (Core 1 -> Core 0)
struct ResponseEntry {
  uint8_t cmd;
  uint8_t status;
  uint8_t len;
  uint8_t data[64];
};

// Response queue template specialization
template<int SIZE>
class ResponseQueue {
private:
  volatile ResponseEntry queue[SIZE];
  volatile uint32_t head;
  volatile uint32_t tail;
  
public:
  ResponseQueue() : head(0), tail(0) {}
  
  bool push(const ResponseEntry* resp) {
    uint32_t next = (head + 1) % SIZE;
    if (next == tail) return false;
    
    queue[head] = *resp;
    asm volatile("" ::: "memory");
    head = next;
    return true;
  }
  
  bool pop(ResponseEntry* out) {
    if (tail == head) return false;
    
    // Copy from volatile queue element-by-element
    out->cmd = queue[tail].cmd;
    out->status = queue[tail].status;
    out->len = queue[tail].len;
    memcpy(out->data, (void*)queue[tail].data, queue[tail].len);
    
    asm volatile("" ::: "memory");
    tail = (tail + 1) % SIZE;
    return true;
  }
  
  bool isEmpty() const { return head == tail; }
};

static ResponseQueue<8> response_queue;

// Core 1 task flags
struct Core1Flags {
  volatile bool ai_training;
  volatile bool ai_model_ready;
  volatile bool display_dirty;
  volatile uint32_t free_cycles;  // Performance counter
};

static Core1Flags core1_flags = {false, false, false, 0};

// ===== Core1 Implementation =====

// Send response back to Core0
inline void core1_send_response(uint8_t cmd, uint8_t status, const uint8_t* data, uint8_t len) {
  ResponseEntry resp;
  resp.cmd = cmd;
  resp.status = status;
  resp.len = min(len, 64);
  if (len > 0 && data) {
    memcpy(resp.data, data, resp.len);
  }
  response_queue.push(&resp);
}

// Core1 command handler (runs all AI/display commands)
inline void core1_handle_command(const CommandQueueEntry* cmd_entry) {
  uint8_t cmd = cmd_entry->cmd;
  const uint8_t* payload = cmd_entry->payload;
  uint8_t len = cmd_entry->len;
  
  switch (cmd) {
    // Graphics commands
    case CMD_CLEAR:
      fb_clear();
      core1_send_response(cmd, RESP_OK, nullptr, 0);
      break;
      
    case CMD_PIXEL:
      if (len >= 3) {
        fb_pixel(payload[0], payload[1], payload[2]);
        core1_send_response(cmd, RESP_OK, nullptr, 0);
      } else {
       core1_send_response(cmd, RESP_ERROR, nullptr, 0);
      }
      break;
      
    case CMD_RECT:
      if (len >= 5) {
        fb_rect(payload[0], payload[1], payload[2], payload[3], payload[4]);
        core1_send_response(cmd, RESP_OK, nullptr, 0);
      } else {
        core1_send_response(cmd, RESP_ERROR, nullptr, 0);
      }
      break;
      
    case CMD_FLUSH:
      if (dirty_rect.is_dirty) {
        sprite_display.updateRegion(framebuffer, 
                                    dirty_rect.x1, dirty_rect.y1,
                                    dirty_rect.x2, dirty_rect.y2);
        dirty_rect.is_dirty = false;
      } else {
        sprite_display.update(framebuffer);
      }
      core1_send_response(cmd, RESP_OK, nullptr, 0);
      break;
      
    // Sprite commands
    case CMD_SPRITE_CREATE:
    case CMD_SPRITE_MOVE:
    case CMD_SPRITE_DELETE:
    case CMD_SPRITE_VISIBLE:
    case CMD_SPRITE_COLLISION:
    case CMD_SPRITE_RENDER:
    case CMD_SPRITE_CLEAR:
      // Sprites handled here - simplified for now
      core1_send_response(cmd, RESP_OK, nullptr, 0);
      break;
      
    // AI commands
    case CMD_AI_INFER:
      if (len >= 8 && model_ready) {
        float in0, in1;
        memcpy(&in0, payload, 4);
        memcpy(&in1, payload + 4, 4);
        
        core1_flags.ai_training = false;  // Block training during inference
        float result = do_inference(in0, in1);
       
        core1_send_response(cmd, RESP_OK, (uint8_t*)&result, 4);
      } else {
        core1_send_response(cmd, model_ready ? RESP_ERROR : RESP_NOT_FOUND, nullptr, 0);
      }
      break;
      
    case CMD_AI_TRAIN:
      {
        uint8_t epochs = len > 0 ? payload[0] : 100;
        core1_flags.ai_training = true;
        do_train(epochs);
        core1_flags.ai_training = false;
        core1_flags.ai_model_ready = true;
        
        core1_send_response(cmd, RESP_OK, (uint8_t*)&last_loss, 4);
      }
      break;
      
    case CMD_AI_STATUS:
      {
        uint8_t resp[8];
        resp[0] = core1_flags.ai_training ? 1 : 0;
        resp[1] = model_ready ? 1 : 0;
        memcpy(resp + 2, &train_epochs, 2);
        memcpy(resp + 4, &last_loss, 4);
        core1_send_response(cmd, RESP_OK, resp, 8);
      }
      break;
      
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
      // Model management - for now just ACK
      core1_send_response(cmd, RESP_OK, nullptr, 0);
      break;
      
    default:
      core1_send_response(cmd, RESP_ERROR, nullptr, 0);
      break;
  }
}

// Core1 main loop
inline void core1_loop() {
  CommandQueueEntry cmd;
  
  while (true) {
    // Process commands from queue
    if (cmd_queue.pop(&cmd)) {
      core1_handle_command(&cmd);
    } else {
      // Idle - count free cycles for performance monitoring
      core1_flags.free_cycles++;
    }
    
    // Small delay to prevent busy-waiting
    delayMicroseconds(10);
  }
}

// Core1 entry point (called from multicore_launch_core1)
inline void core1_entry() {
  // Core1 initialization (if needed)
  delay(50);  // Let Core0 finish setup
  
  // Run forever
  core1_loop();
}


#endif
