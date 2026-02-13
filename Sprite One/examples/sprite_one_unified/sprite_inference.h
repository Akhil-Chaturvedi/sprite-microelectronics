#ifndef SPRITE_INFERENCE_H
#define SPRITE_INFERENCE_H

#include "aifes.h"

// Define INT8 Model Structures
ailayer_input_q7_t input_layer_int8;
ailayer_dense_q7_t dense_1_int8;
ailayer_sigmoid_q7_t sigmoid_1_int8;
ailayer_dense_q7_t dense_2_int8;
ailayer_sigmoid_q7_t sigmoid_2_int8;
aimodel_t model_int8;

// Memory buffers for INT8 model
// Size estimation: 
// XOR model is small, providing generous buffers
uint8_t param_mem_int8[2048]; 
uint8_t infer_mem_int8[1024];

// Quantization Parameters
aimath_q7_params_t input_q_params;
aimath_q7_params_t dense_1_out_params;
aimath_q7_params_t dense_2_out_params;

void build_model_int8() {
   uint16_t shape[] = {1, 2};
   
   // Input: Range [0, 1]
   aimath_q7_calc_q_params_from_f32(0.0f, 1.0f, &input_q_params);
   input_layer_int8 = AILAYER_INPUT_Q7_M(2, shape, &input_q_params);
   
   // Dense 1: Heuristic output range [-8, 8]
   // This covers the linear region of Sigmoid without saturation
   aimath_q7_calc_q_params_from_f32(-8.0f, 8.0f, &dense_1_out_params);
   dense_1_int8 = AILAYER_DENSE_Q7_A(3);
   dense_1_int8.base.result.tensor_params = &dense_1_out_params;
   
   // Sigmoid 1: Auto-configured {shift=8, zero=-128} -> [0, 1]
   sigmoid_1_int8 = AILAYER_SIGMOID_Q7_A();
   
   // Dense 2: Heuristic [-8, 8]
   aimath_q7_calc_q_params_from_f32(-8.0f, 8.0f, &dense_2_out_params);
   dense_2_int8 = AILAYER_DENSE_Q7_A(1);
   dense_2_int8.base.result.tensor_params = &dense_2_out_params;
   
   // Sigmoid 2: Auto-configured
   sigmoid_2_int8 = AILAYER_SIGMOID_Q7_A();
   
   // Link Layers
   ailayer_t *x;
   model_int8.input_layer = ailayer_input_q7_default(&input_layer_int8);
   x = ailayer_dense_q7_default(&dense_1_int8, model_int8.input_layer);
   x = ailayer_sigmoid_q7_default(&sigmoid_1_int8, x);
   x = ailayer_dense_q7_default(&dense_2_int8, x);
   model_int8.output_layer = ailayer_sigmoid_q7_default(&sigmoid_2_int8, x);
   
   // Distribute memory
   aialgo_distribute_parameter_memory(&model_int8, param_mem_int8, sizeof(param_mem_int8));
}

// Convert F32 weights to INT8
// Must be called after training or loading F32 model
void convert_f32_to_int8(ailayer_dense_f32_t *d1_f32, ailayer_dense_f32_t *d2_f32) {
   // Quantize Dense 1
   ailayer_dense_quantize_q7_from_f32(d1_f32, &dense_1_int8);
   
   // Quantize Dense 2
   ailayer_dense_quantize_q7_from_f32(d2_f32, &dense_2_int8);
}

// Optimized INT8 Inference
float do_inference_int8(float in0, float in1) {
   int8_t input_data[2];
   
   // Quantize inputs
   input_data[0] = FLOAT_TO_Q7(in0, input_q_params.shift, input_q_params.zero_point);
   input_data[1] = FLOAT_TO_Q7(in1, input_q_params.shift, input_q_params.zero_point);
   
   uint16_t in_shape[] = {1, 2};
   uint16_t out_shape[] = {1, 1};
   int8_t output_data[1];
   
   // Create temporary tensors wrapping the data
   aitensor_t in_tensor = AITENSOR_2D_Q7(in_shape, &input_q_params, input_data);
   
   // Output params come from the final sigmoid layer
   aimath_q7_params_t *out_p = (aimath_q7_params_t*)sigmoid_2_int8.base.result.tensor_params;
   aitensor_t out_tensor = AITENSOR_2D_Q7(out_shape, out_p, output_data);
   
   // Run Inference
   aialgo_schedule_inference_memory(&model_int8, infer_mem_int8, sizeof(infer_mem_int8));
   aialgo_inference_model(&model_int8, &in_tensor, &out_tensor);
   
   // De-quantize output
   return Q7_TO_FLOAT(output_data[0], out_p->shift, out_p->zero_point);
}

#endif // SPRITE_INFERENCE_H
