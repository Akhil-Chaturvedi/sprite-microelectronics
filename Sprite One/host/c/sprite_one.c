/**
 * Sprite One - C Host Library Implementation
 * Week 4 Day 23
 */

#include "sprite_one.h"
#include <string.h>

// Helper: Calculate checksum
static uint8_t calc_checksum(const uint8_t* data, uint8_t len) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (~sum + 1);
}

// Helper: Send command and receive response
static bool send_command(sprite_context_t* ctx, uint8_t cmd, 
                         const uint8_t* payload, uint8_t payload_len,
                         uint8_t* resp_data, uint8_t* resp_len) {
    // Send packet header
    ctx->write_byte(SPRITE_HEADER);
    ctx->write_byte(cmd);
    ctx->write_byte(payload_len);
    
    // Send payload
    for (uint8_t i = 0; i < payload_len; i++) {
        ctx->write_byte(payload[i]);
    }
    
    // Send checksum
    ctx->write_byte(calc_checksum(payload, payload_len));
    
    // Wait for response with timeout
    uint32_t start = 0; // Platform-specific: get milliseconds
    
    // Read header
    while (!ctx->data_available()) {
        // Timeout check (platform-specific)
    }
    
    uint8_t header = ctx->read_byte();
    if (header != SPRITE_HEADER) return false;
    
    uint8_t resp_cmd = ctx->read_byte();
    uint8_t resp_status = ctx->read_byte();
    uint8_t resp_data_len = ctx->read_byte();
    
    // Read response data
    for (uint8_t i = 0; i < resp_data_len && resp_data; i++) {
        resp_data[i] = ctx->read_byte();
    }
    if (resp_len) *resp_len = resp_data_len;
    
    // Read checksum (ignored)
    ctx->read_byte();
    
    return resp_status == RESP_OK;
}

// ===== Implementation =====

void sprite_init(sprite_context_t* ctx,
                 sprite_uart_write_fn write_fn,
                 sprite_uart_read_fn read_fn,
                 sprite_uart_available_fn available_fn,
                 uint32_t timeout_ms) {
    ctx->write_byte = write_fn;
    ctx->read_byte = read_fn;
    ctx->data_available = available_fn;
    ctx->timeout_ms = timeout_ms;
}

bool sprite_get_version(sprite_context_t* ctx, 
                        uint8_t* major, uint8_t* minor, uint8_t* patch) {
    uint8_t resp[3];
    uint8_t len;
    
    if (!send_command(ctx, CMD_VERSION, NULL, 0, resp, &len)) {
        return false;
    }
    
    if (len >= 3) {
        *major = resp[0];
        *minor = resp[1];
        *patch = resp[2];
        return true;
    }
    return false;
}

bool sprite_clear(sprite_context_t* ctx, uint8_t color) {
    uint8_t payload[1] = {color};
    return send_command(ctx, CMD_CLEAR, payload, 1, NULL, NULL);
}

bool sprite_pixel(sprite_context_t* ctx, int16_t x, int16_t y, uint8_t color) {
    uint8_t payload[5];
    memcpy(payload, &x, 2);
    memcpy(payload + 2, &y, 2);
    payload[4] = color;
    return send_command(ctx, CMD_PIXEL, payload, 5, NULL, NULL);
}

bool sprite_rect(sprite_context_t* ctx, 
                 int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    uint8_t payload[9];
    memcpy(payload, &x, 2);
    memcpy(payload + 2, &y, 2);
    memcpy(payload + 4, &w, 2);
    memcpy(payload + 6, &h, 2);
    payload[8] = color;
    return send_command(ctx, CMD_RECT, payload, 9, NULL, NULL);
}

bool sprite_text(sprite_context_t* ctx, 
                 int16_t x, int16_t y, const char* text, uint8_t color) {
    uint8_t payload[64];
    memcpy(payload, &x, 2);
    memcpy(payload + 2, &y, 2);
    payload[4] = color;
    
    uint8_t text_len = strlen(text);
    if (text_len > 58) text_len = 58;
    memcpy(payload + 5, text, text_len);
    
    return send_command(ctx, CMD_TEXT, payload, 5 + text_len, NULL, NULL);
}

bool sprite_flush(sprite_context_t* ctx) {
    return send_command(ctx, CMD_FLUSH, NULL, 0, NULL, NULL);
}

bool sprite_ai_infer(sprite_context_t* ctx, 
                     float input0, float input1, float* output) {
    uint8_t payload[8];
    memcpy(payload, &input0, 4);
    memcpy(payload + 4, &input1, 4);
    
    uint8_t resp[4];
    uint8_t len;
    
    if (!send_command(ctx, CMD_AI_INFER, payload, 8, resp, &len)) {
        return false;
    }
    
    if (len >= 4) {
        memcpy(output, resp, 4);
        return true;
    }
    return false;
}

bool sprite_ai_train(sprite_context_t* ctx, 
                     uint8_t epochs, float* final_loss) {
    uint8_t payload[1] = {epochs};
    uint8_t resp[4];
    uint8_t len;
    
    if (!send_command(ctx, CMD_AI_TRAIN, payload, 1, resp, &len)) {
        return false;
    }
    
    if (len >= 4 && final_loss) {
        memcpy(final_loss, resp, 4);
    }
    return true;
}

bool sprite_ai_status(sprite_context_t* ctx, sprite_ai_status_t* status) {
    uint8_t resp[8];
    uint8_t len;
    
    if (!send_command(ctx, CMD_AI_STATUS, NULL, 0, resp, &len)) {
        return false;
    }
    
    if (len >= 8) {
        status->state = resp[0];
        status->model_loaded = resp[1];
        memcpy(&status->epochs, resp + 2, 2);
        memcpy(&status->last_loss, resp + 4, 4);
        return true;
    }
    return false;
}

bool sprite_ai_save(sprite_context_t* ctx, const char* filename) {
    uint8_t len = strlen(filename);
    return send_command(ctx, CMD_AI_SAVE, (uint8_t*)filename, len, NULL, NULL);
}

bool sprite_ai_load(sprite_context_t* ctx, const char* filename) {
    uint8_t len = strlen(filename);
    return send_command(ctx, CMD_AI_LOAD, (uint8_t*)filename, len, NULL, NULL);
}
