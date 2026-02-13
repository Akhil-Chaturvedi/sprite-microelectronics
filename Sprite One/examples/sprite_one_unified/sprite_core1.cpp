/**
 * Sprite Core1 Implementation
 * Core1 runs AI inference, display updates, and sprite rendering
 * while Core0 handles protocol and command parsing.
 */

#if ENABLE_DUAL_CORE

// Send response back to Core0
void core1_send_response(uint8_t cmd, uint8_t status, const uint8_t* data, uint8_t len) {
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
void core1_handle_command(const CommandQueueEntry* cmd_entry) {
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
void core1_loop() {
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
void core1_entry() {
  // Core1 initialization (if needed)
  delay(50);  // Let Core0 finish setup
  
  // Run forever
  core1_loop();
}

#endif // ENABLE_DUAL_CORE
