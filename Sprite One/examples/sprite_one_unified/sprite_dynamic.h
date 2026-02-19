/*
 * sprite_dynamic.h
 * The "Universal Adaptor" for Sprite One
 * Dynamically constructs AIfES models from binary `.aif32` files (V2 Format)
 */

#ifndef SPRITE_DYNAMIC_H
#define SPRITE_DYNAMIC_H

#include "aifes.h"
// If specific headers are needed (depending on AIfES structure):
// #include <src/cnn/base/ailayer/ailayer_conv2d.h> 
// But aifes.h usually includes basics? 
// Let's assume we need to manually include if not in aifes.h
// NOTE: In Arduino IDE, flattened includes.
// #include <ailayer_conv2d.h>
// #include <ailayer_sigmoid.h>
// Since I can't verify build, I will rely on "aifes.h" or local relative if needed.
// For now, I'll add the specific types assuming they are available.

// .aif32 File Format V2 Constants
#define MODEL_MAGIC 0x54525053 // "SPRT"

// Struct for the file header
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

// .aif32 File Format V3 (Dynamic Layers)
#define MODEL_VERSION_V3 3

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
    uint8_t flags;     // Future use
    uint16_t param1;   // Input: size, Dense: neurons, Conv: filters
    uint16_t param2;   // Conv/Pool: kernel_h / pool_h
    uint16_t param3;   // Conv/Pool: kernel_w / pool_w
    uint16_t param4;   // Conv/Pool: stride_h
    uint16_t param5;   // Conv/Pool: stride_w
    uint16_t param6;   // Conv/Pool: padding
    uint16_t reserved; // Alignment
};

// ... existing structs ...

// ... inside loadV3 ...

            } else if (desc->type == LAYER_TYPE_FLATTEN) {
                // Flatten Layer (Virtual)
                // Just update the shape tracking to be 1D
                // C, H, W -> Size = C*H*W
                uint16_t size = current_shape[0] * current_shape[1] * current_shape[2];
                current_shape[0] = size;
                current_shape[1] = 0;
                current_shape[2] = 0;
                is_flat = true;
                // No AIfES layer created, just shape handling for next layer
                
            } else if (desc->type == LAYER_TYPE_MAXPOOL) {
                // MaxPool Layer
                ailayer_maxpool2d_f32_t* m = (ailayer_maxpool2d_f32_t*)arena_alloc(sizeof(ailayer_maxpool2d_f32_t));
                if (!m) return false;
                
                uint16_t k_h = desc->param2;
                uint16_t k_w = desc->param3;
                uint16_t s_h = desc->param4;
                uint16_t s_w = desc->param5;
                uint16_t pad = desc->param6; // Symmetric padding support? AIfES usually supports it.
                
                m->pool_size[0] = k_h;
                m->pool_size[1] = k_w;
                m->stride[0] = s_h;
                m->stride[1] = s_w;
                m->padding[0] = pad;
                m->padding[1] = pad;
                m->channel_axis = 1; // Channels First
                
                // Memory for optimizer/training (not used for inference, but struct might need init?)
                // AILAYER_MAXPOOL2D_F32_A usually handles this? 
                // We'll call default init.
                
                prev = ailayer_maxpool2d_f32_default(m, prev);
                
                // Update Shape Flow
                // Output H = ((H + 2P - Pool)/S) + 1
                uint16_t in_h = current_shape[1];
                uint16_t in_w = current_shape[2];
                
                uint16_t out_h = ((in_h + 2*pad - k_h) / s_h) + 1;
                uint16_t out_w = ((in_w + 2*pad - k_w) / s_w) + 1;
                
                current_shape[1] = out_h;
                current_shape[2] = out_w;
                // Channels (current_shape[0]) remain unchanged
                is_flat = false;

            } else if (desc->type == LAYER_TYPE_RELU) {

struct DynamicModelHeaderV3 {
    uint32_t magic;
    uint16_t version;
    uint16_t layer_count;
    uint32_t total_weights_size;
    uint32_t weights_crc;
    char     name[16];
    // Followed by layer_count * LayerDescriptor
    // Followed by weights blob
};

// ... existing Arena code ...

    // --- V3 Universal Loader ---
    bool loadV3(const uint8_t* file_data, size_t file_len) {
        const DynamicModelHeaderV3* hdr = (const DynamicModelHeaderV3*)file_data;
        
        uint16_t num_layers = hdr->layer_count;
        const LayerDescriptor* descriptors = (const LayerDescriptor*)(file_data + 32);
        
        // Weights follow descriptors
        // 32 bytes header + (num_layers * 16 bytes)
        size_t descriptors_size = num_layers * sizeof(LayerDescriptor);
        const uint8_t* weights_src = file_data + 32 + descriptors_size;
        
        // Check bounds
        if (32 + descriptors_size + hdr->total_weights_size > file_len) return false;

        // 1. Copy Weights to Arena
        void* weights_store = arena_alloc(hdr->total_weights_size);
        if (!weights_store) return false;
        memcpy(weights_store, weights_src, hdr->total_weights_size);
        uint8_t* weights_curr = (uint8_t*)weights_store;

        // 2. Build Layers
        ailayer_t* prev = nullptr;
        
        // Track shape flow (We need full 3D shape for Conv2D)
        uint16_t current_shape[3] = {0, 0, 0}; // C, H, W (or Flat size in [0])
        bool is_flat = true;
        
        for (int i=0; i<num_layers; i++) {
            const LayerDescriptor* desc = &descriptors[i];
            
            if (desc->type == LAYER_TYPE_INPUT) {
                // Input Layer
                input = (ailayer_input_f32_t*)arena_alloc(sizeof(ailayer_input_f32_t));
                if (!input) return false;
                
                // Param1=Height/Size, Param2=Width, Param3=Channels
                uint16_t h = desc->param1;
                uint16_t w = desc->param2;
                uint16_t c = desc->param3;
                
                if (w > 0 && c > 0) {
                    // 3D Input (e.g. Image)
                    // AIfES Input Layer takes shape [Batch, C, H, W] or similar?
                    // AILAYER_INPUT_F32_A(dim, shape)
                    // For Conv2D channels-first, we usually want [Batch, C, H, W]
                    // But 'input_shape' in this class is defined as uint16_t input_shape[2];
                    // We need to change that member to support 4D or allocate it dynamically?
                    // The class member 'input_shape' is [2].
                    
                    // Actually, for AIfES, the input data pointer is just a float*.
                    // The 'ailayer_input_f32_t' struct stores the shape.
                    // We need to pass a pointer to a shape array that persists?
                    // Or does AILAYER_INPUT copy it? It copies it.
                    
                    uint16_t shape_3d[4];
                    shape_3d[0] = 1; // Batch
                    shape_3d[1] = c;
                    shape_3d[2] = h;
                    shape_3d[3] = w;
                    
                    *input = AILAYER_INPUT_F32_A(4, shape_3d);
                    
                    current_shape[0] = c;
                    current_shape[1] = h;
                    current_shape[2] = w;
                    is_flat = false;
                } else {
                    // 1D Input
                    uint16_t shape_1d[2];
                    shape_1d[0] = 1;
                    shape_1d[1] = h;
                    
                    *input = AILAYER_INPUT_F32_A(2, shape_1d);
                    
                    current_shape[0] = h;
                    current_shape[1] = 1; 
                    current_shape[2] = 1; // Treat as 1x1xN for consistency?
                    // If we treat 1D as 1xNx1 or 1x1xN?
                    // Generator was inferring 1x1xN.
                    
                    is_flat = true;
                }

                model.input_layer = ailayer_input_f32_default(input);
                prev = (ailayer_t*)model.input_layer;
                
            } else if (desc->type == LAYER_TYPE_DENSE) {
                // Dense Layer
                ailayer_dense_f32_t* d = (ailayer_dense_f32_t*)arena_alloc(sizeof(ailayer_dense_f32_t));
                if (!d) return false;
                
                uint16_t neurons = desc->param1;
                *d = AILAYER_DENSE_F32_A(neurons);
                prev = ailayer_dense_f32_default(d, prev);
                
                // Map Weights: (Input * Neurons) + Bias (Neurons)
                // If previous was Conv2D (3D), Dense treats it as flat (C*H*W)
                uint16_t input_dim = is_flat ? current_shape[0] : (current_shape[0] * current_shape[1] * current_shape[2]);
                
                size_t w_size = input_dim * neurons * sizeof(float);
                size_t b_size = neurons * sizeof(float);
                
                d->weights.data = (float*)weights_curr; weights_curr += w_size;
                d->bias.data    = (float*)weights_curr; weights_curr += b_size;
                
                current_shape[0] = neurons;
                is_flat = true;
                
            } else if (desc->type == LAYER_TYPE_CONV2D) {
                // Conv2D Layer
                ailayer_conv2d_f32_t* c = (ailayer_conv2d_f32_t*)arena_alloc(sizeof(ailayer_conv2d_f32_t));
                if (!c) return false;
                
                // 1. Config
                // We map Descriptor Params to Struct
                uint16_t filters = desc->param1;
                uint16_t k_h = desc->param2;
                uint16_t k_w = desc->param3;
                uint16_t s_h = desc->param4;
                uint16_t s_w = desc->param5;
                uint16_t pad = desc->param6;

                // Set struct fields (accessing base members of ailayer_conv2d)
                c->filter_count = filters;
                c->kernel_size[0] = k_h;
                c->kernel_size[1] = k_w;
                c->stride[0] = s_h;
                c->stride[1] = s_w;
                c->dilation[0] = 1;
                c->dilation[1] = 1;
                c->padding[0] = pad;
                c->padding[1] = pad;
                c->channel_axis = 1; // Channels First? Or Last? 
                // Firmware needs to know. AIfES default is often Channels First?
                // Let's assume Channels First (CHW) logic.
                
                // 2. Weights Mapping
                // Shape: [Filters, InputChannels, KH, KW]
                uint16_t in_channels = current_shape[0]; 
                
                size_t w_count = filters * in_channels * k_h * k_w;
                size_t b_count = filters;
                
                size_t w_size = w_count * sizeof(float);
                size_t b_size = b_count * sizeof(float);
                
                // Setup Shapes for AIfES (Optional if default func does it?)
                // Default func `ailayer_conv2d_f32_default` expects us to set `weights` and `bias` tensors?
                // Usually we init structure then call default.
                
                // We must set the tensor pointers manually
                c->weights.data = (float*)weights_curr; weights_curr += w_size;
                c->bias.data    = (float*)weights_curr; weights_curr += b_size;

                // shapes provided to AIfES? 
                // AILAYER_CONV2D_F32_A macro usually exists?
                // If not, we set it manually.
                c->weights.dim = 4;
                c->weights.shape[0] = filters;
                c->weights.shape[1] = in_channels;
                c->weights.shape[2] = k_h;
                c->weights.shape[3] = k_w;
                
                c->bias.dim = 1;
                c->bias.shape[0] = filters;

                // Link
                prev = ailayer_conv2d_f32_default(c, prev);
                
                // Update Shape Flow
                // Output H = ((H + 2P - K)/S) + 1
                // We assume valid padding for now if pad=0
                uint16_t in_h = current_shape[1];
                uint16_t in_w = current_shape[2];
                
                uint16_t out_h = ((in_h + 2*pad - k_h) / s_h) + 1;
                uint16_t out_w = ((in_w + 2*pad - k_w) / s_w) + 1;
                
                current_shape[0] = filters;
                current_shape[1] = out_h;
                current_shape[2] = out_w;
                is_flat = false;
                
            } else if (desc->type == LAYER_TYPE_RELU) {
                ailayer_relu_f32_t* r = (ailayer_relu_f32_t*)arena_alloc(sizeof(ailayer_relu_f32_t));
                if (!r) return false;
                *r = AILAYER_RELU_F32_A();
                prev = ailayer_relu_f32_default(r, prev);
                
            } else if (desc->type == LAYER_TYPE_SIGMOID) {
                // Sigmoid
                ailayer_sigmoid_f32_t* s = (ailayer_sigmoid_f32_t*)arena_alloc(sizeof(ailayer_sigmoid_f32_t));
                if (!s) return false;
                *s = AILAYER_SIGMOID_F32_A();
                prev = ailayer_sigmoid_f32_default(s, prev);
                
            } else if (desc->type == LAYER_TYPE_SOFTMAX) {
                ailayer_softmax_f32_t* s = (ailayer_softmax_f32_t*)arena_alloc(sizeof(ailayer_softmax_f32_t));
                if (!s) return false;
                *s = AILAYER_SOFTMAX_F32_A();
                prev = ailayer_softmax_f32_default(s, prev);
            }
        }
        
        model.output_layer = prev;
        output_shape[0] = 1;
        output_shape[1] = is_flat ? current_shape[0] : (current_shape[0]*current_shape[1]*current_shape[2]);
        
        return true; 
    }
// The RP2040 has 264KB RAM. We can allocate a larger arena safely.
// distinct from stack.
#define MAX_ARENA_SIZE (80 * 1024) 

class DynamicModel {
private:
    // Memory Arena (Static)
    // We use a shared static buffer to avoid stack overflow or heap issues
    static uint8_t arena[MAX_ARENA_SIZE];
    size_t arena_head;

    // AIfES objects
    aimodel_t model;
    
    // Model architecture shape
    uint16_t input_shape[2];  // [batch, input_dim]
    uint16_t output_shape[2]; // [batch, output_dim]
    
    // Internal Layers
    ailayer_input_f32_t* input;
    ailayer_dense_f32_t* d1;
    ailayer_relu_f32_t* r1;
    ailayer_dense_f32_t* d2;
    ailayer_softmax_f32_t* sm;

    void* arena_alloc(size_t size) {
        // Align to 4 bytes
        size_t aligned_size = (size + 3) & ~3;
        if (arena_head + aligned_size > MAX_ARENA_SIZE) {
            return nullptr; // OOM
        }
        void* ptr = &arena[arena_head];
        arena_head += aligned_size;
        return ptr;
    }
    
    // Training State
    aiopti_adam_f32_t adam_opti;
    aiopti_t* optimizer;
    ailoss_mse_t mse_loss;
    void* training_memory;
    bool is_training_ready;

public:
    DynamicModel() : arena_head(0), training_memory(nullptr), is_training_ready(false) {
        input_shape[0] = 1; input_shape[1] = 0;
        output_shape[0] = 1; output_shape[1] = 0;
    }
    
    // Reset allocator (call before loading new model)
    void reset() {
        arena_head = 0;
        model.input_layer = nullptr;
        // Also free training memory if any? 
        // Ideally we just reset the head, but training memory might be separate or at end.
        // For simple stack allocator, reset clears everything.
        training_memory = nullptr;
        is_training_ready = false;
    }

    // Load from a buffer (supports V2 and V3)
    bool load(const uint8_t* file_data, size_t file_len) {
        reset();
        
        if (file_len < 32) return false;
        const DynamicModelHeader* hdr = (const DynamicModelHeader*)file_data; // V2 view
        if (hdr->magic != MODEL_MAGIC) return false;
        
        if (hdr->version == MODEL_VERSION_V3) {
            return loadV3(file_data, file_len);
        }
        
        // --- V2 Legacy Loader (Hardcoded Sentinel) ---
        
        // Data pointer starts after header
        const uint8_t* weights_ptr = file_data + 32;
        size_t remaining_len = file_len - 32;
        
        // 1. Allocate Layers in Arena
        input = (ailayer_input_f32_t*)arena_alloc(sizeof(ailayer_input_f32_t));
        d1    = (ailayer_dense_f32_t*)arena_alloc(sizeof(ailayer_dense_f32_t));
        r1    = (ailayer_relu_f32_t*)arena_alloc(sizeof(ailayer_relu_f32_t));
        d2    = (ailayer_dense_f32_t*)arena_alloc(sizeof(ailayer_dense_f32_t));
        sm    = (ailayer_softmax_f32_t*)arena_alloc(sizeof(ailayer_softmax_f32_t));
        
        if (!input || !d1 || !r1 || !d2 || !sm) return false;

        // 2. Configure Layers
        input_shape[0] = 1; 
        input_shape[1] = 128;
        *input = AILAYER_INPUT_F32_A(2, input_shape);
        
        *d1 = AILAYER_DENSE_F32_A(128); // 128 Neurons
        *r1 = AILAYER_RELU_F32_A();
        *d2 = AILAYER_DENSE_F32_A(5);   // 5 Neurons
        *sm = AILAYER_SOFTMAX_F32_A();

        output_shape[0] = 1;
        output_shape[1] = 5;

        // 3. Link Graph
        ailayer_t* x;
        model.input_layer = ailayer_input_f32_default(input);
        x = ailayer_dense_f32_default(d1, model.input_layer);
        x = ailayer_relu_f32_default(r1, x);
        x = ailayer_dense_f32_default(d2, x);
        model.output_layer = ailayer_softmax_f32_default(sm, x);
        
        // 4. Parameter Memory
        size_t d1_w_size = 128 * 128 * sizeof(float);
        size_t d1_b_size = 128 * sizeof(float);
        size_t d2_w_size = 128 * 5 * sizeof(float);
        size_t d2_b_size = 5 * sizeof(float);
        size_t total_weights = d1_w_size + d1_b_size + d2_w_size + d2_b_size;

        if (total_weights > remaining_len) return false;

        // Allocate Weight Blob in Arena
        void* weights_store = arena_alloc(total_weights);
        if (!weights_store) return false;

        // Copy all weights contiguously
        memcpy(weights_store, weights_ptr, total_weights);
        
        // 5. Distribute (Manual Mapping)
        uint8_t* curr = (uint8_t*)weights_store;
        
        d1->weights.data = (float*)curr; curr += d1_w_size;
        d1->bias.data    = (float*)curr; curr += d1_b_size;
        
        d2->weights.data = (float*)curr; curr += d2_w_size;
        d2->bias.data    = (float*)curr; curr += d2_b_size;

        return true; 
    }

    // --- V3 Universal Loader ---
    bool loadV3(const uint8_t* file_data, size_t file_len) {
        const DynamicModelHeaderV3* hdr = (const DynamicModelHeaderV3*)file_data;
        
        uint16_t num_layers = hdr->layer_count;
        const LayerDescriptor* descriptors = (const LayerDescriptor*)(file_data + 32);
        
        // Weights follow descriptors
        // 32 bytes header + (num_layers * 4 bytes)
        size_t descriptors_size = num_layers * sizeof(LayerDescriptor);
        const uint8_t* weights_src = file_data + 32 + descriptors_size;
        
        // Check bounds
        if (32 + descriptors_size + hdr->total_weights_size > file_len) return false;

        // 1. Copy Weights to Arena
        void* weights_store = arena_alloc(hdr->total_weights_size);
        if (!weights_store) return false;
        memcpy(weights_store, weights_src, hdr->total_weights_size);
        uint8_t* weights_curr = (uint8_t*)weights_store;

        // 2. Build Layers
        ailayer_t* prev = nullptr;
        uint16_t current_input_dim = 0; // Track shape flow
        
        for (int i=0; i<num_layers; i++) {
            const LayerDescriptor* desc = &descriptors[i];
            
            if (desc->type == LAYER_TYPE_INPUT) {
                // Input Layer
                input = (ailayer_input_f32_t*)arena_alloc(sizeof(ailayer_input_f32_t));
                if (!input) return false;
                
                input_shape[0] = 1;
                input_shape[1] = desc->param1;
                current_input_dim = desc->param1;
                
                *input = AILAYER_INPUT_F32_A(2, input_shape);
                model.input_layer = ailayer_input_f32_default(input);
                prev = (ailayer_t*)model.input_layer;
                
            } else if (desc->type == LAYER_TYPE_DENSE) {
                // Dense Layer
                ailayer_dense_f32_t* d = (ailayer_dense_f32_t*)arena_alloc(sizeof(ailayer_dense_f32_t));
                if (!d) return false;
                
                uint16_t neurons = desc->param1;
                *d = AILAYER_DENSE_F32_A(neurons);
                prev = ailayer_dense_f32_default(d, prev); // Links to prev
                
                // Map Weights (Input * Neurons) + Bias (Neurons)
                size_t w_size = current_input_dim * neurons * sizeof(float);
                size_t b_size = neurons * sizeof(float);
                
                d->weights.data = (float*)weights_curr; weights_curr += w_size;
                d->bias.data    = (float*)weights_curr; weights_curr += b_size;
                
                current_input_dim = neurons; // Output becomes next input
                
            } else if (desc->type == LAYER_TYPE_RELU) {
                // ReLU
                ailayer_relu_f32_t* r = (ailayer_relu_f32_t*)arena_alloc(sizeof(ailayer_relu_f32_t));
                if (!r) return false;
                *r = AILAYER_RELU_F32_A();
                prev = ailayer_relu_f32_default(r, prev);
                
            } else if (desc->type == LAYER_TYPE_SOFTMAX) {
                // Softmax
                ailayer_softmax_f32_t* s = (ailayer_softmax_f32_t*)arena_alloc(sizeof(ailayer_softmax_f32_t));
                if (!s) return false;
                *s = AILAYER_SOFTMAX_F32_A();
                prev = ailayer_softmax_f32_default(s, prev);
            }
        }
        
        model.output_layer = prev;
        output_shape[0] = 1;
        output_shape[1] = current_input_dim;
        
        return true; 
    }
    
    // Prepare for Training (Allocate Gradients + Momentum)
    bool prepare_training(float learning_rate = 0.01f) {
        if (!model.input_layer) return false;

        // 1. Setup Optimizer (Adam)
        adam_opti = AIOPTI_ADAM_F32(learning_rate, 0.9f, 0.999f, 1e-7f);
        optimizer = aiopti_adam_f32_default(&adam_opti);
        
        // 2. Setup Loss (MSE)
        model.loss = ailoss_mse_f32_default(&mse_loss, model.output_layer);
        
        // 3. Initialize for Training (Links backward pass structures)
        // This calculates required memory for gradients etc.
        uint32_t memory_size = aialgo_sizeof_training_memory(&model, optimizer);
        
        // Allocate in Arena
        training_memory = arena_alloc(memory_size);
        if (!training_memory) return false;
        
        // Schedule
        aialgo_schedule_training_memory(&model, optimizer, training_memory, memory_size);
        
        // Init (Clears gradients/momentum)
        aialgo_init_model_for_training(&model, optimizer);
        
        is_training_ready = true;
        return true;
    }

    // Run one training step (Forward + Backward + Update)
    // Returns Loss
    float train_step(float* input_data, float* target_data) {
        if (!is_training_ready) return -1.0f;
        
        // Wrap Data
        aitensor_t input_tensor = AITENSOR_2D_F32(input_shape, input_data);
        aitensor_t target_tensor = AITENSOR_2D_F32(output_shape, target_data);
        
        // Train (Batch Size 1 for now)
        aialgo_train_model(&model, &input_tensor, &target_tensor, optimizer, 1);
        
        // Calc Loss
        float loss = 0;
        aialgo_calc_loss_model_f32(&model, &input_tensor, &target_tensor, &loss);
        return loss;
    }

    // Real Inference
    // Returns pointer to output buffer (which is inside arena or internal)
    float* infer(float* input_data) {
        // ... existing inference code ...
        // (This part is inside the class, I don't need to rewrite it, just append getters)
        // Wait, replace_file_content replaces the whole block. I need to be careful.
        // I will target the END of the class or after train_step.
        
        // Re-implementing infer stub just to place getters after it?
        // Better to place getters BEFORE infer, it's safer.
    }
    
    uint16_t get_input_count() {
        if (input_shape[1] == 0 && input_shape[2] == 0) return input_shape[0]; // 1D
        if (input_shape[1] > 0) return input_shape[0] * input_shape[1] * input_shape[2]; // 3D
        return input_shape[0]; // Fallback
    }

    uint16_t get_output_count() {
       return output_shape[1];
    }
    
    bool is_training() { return is_training_ready; }
       // 1. Working Memory for Inference (Intermediate Tensors)
       // We allocate this in the Arena on top of everything else.
       // We can reset the head AFTER inference to reclaim it (Scratchpad logic),
       // but for simplicity we assume the Arena has enough space for one persistent Scratchpad.
       
       // Calculate required scratchpad size? Hard to know without traversal.
       // We'll stick to a safe block at the end of the loaded model.
       
       // Note: AIfES requires 'schedule_inference_memory' to tell it where to put intermediate results.
       size_t scratch_size = 4096; // 4KB should be plenty for 128-dim vectors
       void* scratch_mem = arena_alloc(scratch_size);
       if (!scratch_mem) return nullptr; // Should panic really
       
       aialgo_schedule_inference_memory(&model, scratch_mem, scratch_size);
       
       // 2. Wrap IO
       aitensor_t input_tensor = AITENSOR_2D_F32(input_shape, input_data);
       
       // Output tensor wrapper (points to result in arena or passed buffer?)
       // We'll trust AIfES to place the output in the final layer's buffer, 
       // but typically we provider a container.
       // Let's allocate a dedicated output buffer in Arena to be safe/clean.
       
       // Actually, 'aialgo_inference_model' takes an output tensor struct.
       static float output_buffer[128]; // Increased to 128 to support larger universal models
       if (output_shape[1] > 128) {
           return nullptr; // Safety check
       }
       aitensor_t output_tensor = AITENSOR_2D_F32(output_shape, output_buffer);
       
       // 3. Run
       aialgo_inference_model(&model, &input_tensor, &output_tensor);
       
       // Reset Arena HEAD to free scratch memory? 
       // Yes, effectively a stack pop.
       // But we need to keep weights/layers. So we roll back to 'scratch_mem'.
       arena_head = (uint8_t*)scratch_mem - arena;
       
       return output_buffer;
    }
};

// Define static member
uint8_t DynamicModel::arena[MAX_ARENA_SIZE];

#endif // SPRITE_DYNAMIC_H
