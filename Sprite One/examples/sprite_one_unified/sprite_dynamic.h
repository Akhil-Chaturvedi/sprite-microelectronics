/*
 * sprite_dynamic.h
 * The "Universal Adaptor" for Sprite One
 * Dynamically constructs AIfES models from binary `.aif32` files (V3 Format)
 * Includes on-device training (Backprop) support.
 */

#ifndef SPRITE_DYNAMIC_H
#define SPRITE_DYNAMIC_H

#include "aifes.h"

// .aif32 File Format Constants
#define MODEL_MAGIC 0x54525053 // "SPRT"
#define MODEL_VERSION_V2 2
#define MODEL_VERSION_V3 3

// Struct for the file header (Legacy V2)
struct DynamicModelHeader {
    uint32_t magic;
    uint16_t version;
    uint8_t  input_size;
    uint8_t  output_size;
    uint8_t  hidden_size;
    uint8_t  model_type;
    uint8_t  num_layers;
    uint8_t  reserved;
    uint32_t weights_crc;
    char     name[16];
};

// Header for V3 (Sequential Model)
struct DynamicModelHeaderV3 {
    uint32_t magic;
    uint16_t version;
    uint16_t layer_count;
    uint32_t total_weights_size;
    uint32_t weights_crc;
    char     name[16];
};

// Layer Types for V3
#define LAYER_TYPE_INPUT   0x01
#define LAYER_TYPE_DENSE   0x02
#define LAYER_TYPE_RELU    0x03
#define LAYER_TYPE_SIGMOID 0x04
#define LAYER_TYPE_SOFTMAX 0x05
#define LAYER_TYPE_CONV2D  0x06
#define LAYER_TYPE_FLATTEN 0x07
#define LAYER_TYPE_MAXPOOL 0x08

// 16-byte Descriptor to support Conv2D/MaxPool params
struct LayerDescriptor {
    uint8_t type;      
    uint8_t flags;     
    uint16_t param1;   // Input: size, Dense: neurons, Conv: filters
    uint16_t param2;   // Conv/Pool: kernel_h / pool_h
    uint16_t param3;   // Conv/Pool: kernel_w / pool_w
    uint16_t param4;   // Conv/Pool: stride_h
    uint16_t param5;   // Conv/Pool: stride_w
    uint16_t param6;   // Conv/Pool: padding
    uint16_t reserved; 
};

#define MAX_ARENA_SIZE (80 * 1024) 

class DynamicModel {
private:
    static uint8_t arena[MAX_ARENA_SIZE];
    size_t arena_head;

    aimodel_t model;
    
    // Model shape tracking
    uint16_t input_shape[4];  // [batch, c, h, w]
    uint8_t  input_dim;       // 2 or 4
    uint16_t output_shape[2]; // [batch, output_dim]
    
    // AIfES Optimizer and Loss
    aiopti_adam_f32_t adam_opti;
    aiopti_t* optimizer;
    ailoss_mse_t mse_loss;
    void* training_memory;
    bool is_training_ready;

    void* arena_alloc(size_t size) {
        size_t aligned_size = (size + 3) & ~3;
        if (arena_head + aligned_size > MAX_ARENA_SIZE) return nullptr;
        void* ptr = &arena[arena_head];
        arena_head += aligned_size;
        return ptr;
    }

    bool loadV3(const uint8_t* file_data, size_t file_len) {
        const DynamicModelHeaderV3* hdr = (const DynamicModelHeaderV3*)file_data;
        uint16_t num_layers = hdr->layer_count;
        const LayerDescriptor* descriptors = (const LayerDescriptor*)(file_data + 32);
        size_t descriptors_size = num_layers * sizeof(LayerDescriptor);
        const uint8_t* weights_src = file_data + 32 + descriptors_size;
        
        if (32 + descriptors_size + hdr->total_weights_size > file_len) return false;

        void* weights_store = arena_alloc(hdr->total_weights_size);
        if (!weights_store) return false;
        memcpy(weights_store, weights_src, hdr->total_weights_size);
        uint8_t* weights_curr = (uint8_t*)weights_store;

        ailayer_t* prev = nullptr;
        uint16_t current_shape[3] = {0, 0, 0}; // C, H, W
        bool is_flat = true;
        
        for (int i=0; i<num_layers; i++) {
            const LayerDescriptor* desc = &descriptors[i];
            
            if (desc->type == LAYER_TYPE_INPUT) {
                ailayer_input_f32_t* input_layer = (ailayer_input_f32_t*)arena_alloc(sizeof(ailayer_input_f32_t));
                if (!input_layer) return false;
                
                uint16_t h = desc->param1;
                uint16_t w = desc->param2;
                uint16_t c = desc->param3;
                
                if (w > 0 && c > 0) {
                    input_shape[0] = 1; input_shape[1] = c; input_shape[2] = h; input_shape[3] = w;
                    input_dim = 4;
                    *input_layer = AILAYER_INPUT_F32_A(4, input_shape);
                    current_shape[0] = c; current_shape[1] = h; current_shape[2] = w;
                    is_flat = false;
                } else {
                    input_shape[0] = 1; input_shape[1] = h;
                    input_dim = 2;
                    *input_layer = AILAYER_INPUT_F32_A(2, input_shape);
                    current_shape[0] = h; current_shape[1] = 1; current_shape[2] = 1;
                    is_flat = true;
                }
                model.input_layer = ailayer_input_f32_default(input_layer);
                prev = (ailayer_t*)model.input_layer;
                
            } else if (desc->type == LAYER_TYPE_DENSE) {
                ailayer_dense_f32_t* d = (ailayer_dense_f32_t*)arena_alloc(sizeof(ailayer_dense_f32_t));
                if (!d) return false;
                uint16_t neurons = desc->param1;
                *d = AILAYER_DENSE_F32_A(neurons);
                prev = ailayer_dense_f32_default(d, prev);
                
                uint16_t in_dim = is_flat ? current_shape[0] : (current_shape[0] * current_shape[1] * current_shape[2]);
                size_t w_size = in_dim * neurons * sizeof(float);
                size_t b_size = neurons * sizeof(float);
                d->weights.data = (float*)weights_curr; weights_curr += w_size;
                d->bias.data    = (float*)weights_curr; weights_curr += b_size;
                current_shape[0] = neurons;
                is_flat = true;
                
            } else if (desc->type == LAYER_TYPE_CONV2D) {
                ailayer_conv2d_f32_t* c = (ailayer_conv2d_f32_t*)arena_alloc(sizeof(ailayer_conv2d_f32_t));
                if (!c) return false;
                uint16_t filters = desc->param1;
                uint16_t k_h = desc->param2; uint16_t k_w = desc->param3;
                uint16_t s_h = desc->param4; uint16_t s_w = desc->param5;
                uint16_t pad = desc->param6;
                c->filter_count = filters;
                c->kernel_size[0] = k_h; c->kernel_size[1] = k_w;
                c->stride[0] = s_h; c->stride[1] = s_w;
                c->dilation[0] = 1; c->dilation[1] = 1;
                c->padding[0] = pad; c->padding[1] = pad;
                c->channel_axis = 1; 
                uint16_t in_channels = current_shape[0]; 
                size_t w_count = filters * in_channels * k_h * k_w;
                c->weights.data = (float*)weights_curr; weights_curr += w_count * sizeof(float);
                c->bias.data    = (float*)weights_curr; weights_curr += filters * sizeof(float);
                c->weights.dim = 4;
                c->weights.shape[0] = filters; c->weights.shape[1] = in_channels;
                c->weights.shape[2] = k_h; c->weights.shape[3] = k_w;
                c->bias.dim = 1; c->bias.shape[0] = filters;
                prev = ailayer_conv2d_f32_default(c, prev);
                uint16_t in_h = current_shape[1]; uint16_t in_w = current_shape[2];
                uint16_t out_h = ((in_h + 2*pad - k_h) / s_h) + 1;
                uint16_t out_w = ((in_w + 2*pad - k_w) / s_w) + 1;
                current_shape[0] = filters; current_shape[1] = out_h; current_shape[2] = out_w;
                is_flat = false;
                
            } else if (desc->type == LAYER_TYPE_RELU) {
                ailayer_relu_f32_t* r = (ailayer_relu_f32_t*)arena_alloc(sizeof(ailayer_relu_f32_t));
                if (!r) return false;
                *r = AILAYER_RELU_F32_A();
                prev = ailayer_relu_f32_default(r, prev);
                
            } else if (desc->type == LAYER_TYPE_SIGMOID) {
                ailayer_sigmoid_f32_t* s = (ailayer_sigmoid_f32_t*)arena_alloc(sizeof(ailayer_sigmoid_f32_t));
                if (!s) return false;
                *s = AILAYER_SIGMOID_F32_A();
                prev = ailayer_sigmoid_f32_default(s, prev);
                
            } else if (desc->type == LAYER_TYPE_SOFTMAX) {
                ailayer_softmax_f32_t* s = (ailayer_softmax_f32_t*)arena_alloc(sizeof(ailayer_softmax_f32_t));
                if (!s) return false;
                *s = AILAYER_SOFTMAX_F32_A();
                prev = ailayer_softmax_f32_default(s, prev);
                
            } else if (desc->type == LAYER_TYPE_FLATTEN) {
                uint16_t size = current_shape[0] * current_shape[1] * current_shape[2];
                current_shape[0] = size; current_shape[1] = 1; current_shape[2] = 1;
                is_flat = true;
                
            } else if (desc->type == LAYER_TYPE_MAXPOOL) {
                ailayer_maxpool2d_f32_t* m = (ailayer_maxpool2d_f32_t*)arena_alloc(sizeof(ailayer_maxpool2d_f32_t));
                if (!m) return false;
                uint16_t k_h = desc->param2; uint16_t k_w = desc->param3;
                uint16_t s_h = desc->param4; uint16_t s_w = desc->param5;
                uint16_t pad = desc->param6;
                m->pool_size[0] = k_h; m->pool_size[1] = k_w;
                m->stride[0] = s_h; m->stride[1] = s_w;
                m->padding[0] = pad; m->padding[1] = pad;
                m->channel_axis = 1;
                prev = ailayer_maxpool2d_f32_default(m, prev);
                uint16_t out_h = ((current_shape[1] + 2*pad - k_h) / s_h) + 1;
                uint16_t out_w = ((current_shape[2] + 2*pad - k_w) / s_w) + 1;
                current_shape[1] = out_h; current_shape[2] = out_w;
                is_flat = false;
            }
        }
        
        model.output_layer = prev;
        output_shape[0] = 1;
        output_shape[1] = is_flat ? current_shape[0] : (current_shape[0]*current_shape[1]*current_shape[2]);
        return true; 
    }

public:
    DynamicModel() : arena_head(0), training_memory(nullptr), is_training_ready(false) {}
    
    void reset() {
        arena_head = 0;
        model.input_layer = nullptr;
        training_memory = nullptr;
        is_training_ready = false;
    }

    bool load(const uint8_t* file_data, size_t file_len) {
        reset();
        if (file_len < 32) return false;
        const DynamicModelHeader* hdr = (const DynamicModelHeader*)file_data;
        if (hdr->magic != MODEL_MAGIC) return false;
        
        if (hdr->version == MODEL_VERSION_V3) {
            return loadV3(file_data, file_len);
        }
        
        // V2 Fallback (Fixed Sentinel topology)
        const uint8_t* weights_ptr = file_data + 32;
        input_shape[0] = 1; input_shape[1] = 128; input_dim = 2;
        output_shape[0] = 1; output_shape[1] = 5;
        
        ailayer_input_f32_t* in = (ailayer_input_f32_t*)arena_alloc(sizeof(ailayer_input_f32_t));
        ailayer_dense_f32_t* d1 = (ailayer_dense_f32_t*)arena_alloc(sizeof(ailayer_dense_f32_t));
        ailayer_relu_f32_t* r1 = (ailayer_relu_f32_t*)arena_alloc(sizeof(ailayer_relu_f32_t));
        ailayer_dense_f32_t* d2 = (ailayer_dense_f32_t*)arena_alloc(sizeof(ailayer_dense_f32_t));
        ailayer_softmax_f32_t* sm = (ailayer_softmax_f32_t*)arena_alloc(sizeof(ailayer_softmax_f32_t));
        
        if (!in || !d1 || !r1 || !d2 || !sm) return false;
        
        *in = AILAYER_INPUT_F32_A(2, input_shape);
        *d1 = AILAYER_DENSE_F32_A(128); *r1 = AILAYER_RELU_F32_A();
        *d2 = AILAYER_DENSE_F32_A(5);   *sm = AILAYER_SOFTMAX_F32_A();
        
        model.input_layer = ailayer_input_f32_default(in);
        ailayer_t* x = ailayer_dense_f32_default(d1, (ailayer_t*)model.input_layer);
        x = ailayer_relu_f32_default(r1, x);
        x = ailayer_dense_f32_default(d2, x);
        model.output_layer = ailayer_softmax_f32_default(sm, x);
        
        size_t w_size = (128*128 + 128 + 128*5 + 5)*sizeof(float);
        void* w_store = arena_alloc(w_size);
        if (!w_store) return false;
        memcpy(w_store, weights_ptr, w_size);
        
        uint8_t* curr = (uint8_t*)w_store;
        d1->weights.data = (float*)curr; curr += 128*128*4;
        d1->bias.data    = (float*)curr; curr += 128*4;
        d2->weights.data = (float*)curr; curr += 128*5*4;
        d2->bias.data    = (float*)curr; curr += 5*4;
        
        return true; 
    }

    bool prepare_training(float learning_rate = 0.01f) {
        if (!model.input_layer) return false;
        adam_opti = AIOPTI_ADAM_F32(learning_rate, 0.9f, 0.999f, 1e-7f);
        optimizer = aiopti_adam_f32_default(&adam_opti);
        model.loss = ailoss_mse_f32_default(&mse_loss, model.output_layer);
        
        uint32_t memory_size = aialgo_sizeof_training_memory(&model, optimizer);
        training_memory = arena_alloc(memory_size);
        if (!training_memory) return false;
        
        aialgo_schedule_training_memory(&model, optimizer, training_memory, memory_size);
        aialgo_init_model_for_training(&model, optimizer);
        is_training_ready = true;
        return true;
    }

    float train_step(float* input_data, float* target_data) {
        if (!is_training_ready) return -1.0f;
        aitensor_t in_tensor = AITENSOR_2D_F32(input_shape, input_data);
        aitensor_t tar_tensor = AITENSOR_2D_F32(output_shape, target_data);
        aialgo_train_model(&model, &in_tensor, &tar_tensor, optimizer, 1);
        float loss = 0;
        aialgo_calc_loss_model_f32(&model, &in_tensor, &tar_tensor, &loss);
        return loss;
    }

    float* infer(float* input_data) {
        size_t scratch_size = 4096;
        void* scratch_mem = arena_alloc(scratch_size);
        if (!scratch_mem) return nullptr;
        aialgo_schedule_inference_memory(&model, scratch_mem, scratch_size);
        
        aitensor_t in_tensor = AITENSOR_2D_F32(input_shape, input_data);
        static float output_buffer[128];
        aitensor_t out_tensor = AITENSOR_2D_F32(output_shape, output_buffer);
        
        aialgo_inference_model(&model, &in_tensor, &out_tensor);
        arena_head = (uint8_t*)scratch_mem - arena;
        return output_buffer;
    }
    
    uint16_t get_input_count() { return (input_dim == 2) ? input_shape[1] : (input_shape[1]*input_shape[2]*input_shape[3]); }
    uint16_t get_output_count() { return output_shape[1]; }
    bool is_training() { return is_training_ready; }
    bool is_loaded() { return model.input_layer != nullptr; }
};

uint8_t DynamicModel::arena[MAX_ARENA_SIZE];

#endif // SPRITE_DYNAMIC_H
