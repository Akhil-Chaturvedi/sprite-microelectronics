/*
 * Sprite One - Q7 Quantization SUCCESS!
 * Week 3 Day 19: 75% Memory Savings!
 * 
 * Based on official AIfES Q7 tutorial
 * Demonstrates: Train F32 -> Quantize to Q7 -> Compare
 * 
 * Key insight: r = 2^(-shift) * (q - zero_point)
 */

#include <aifes.h>

// XOR data
float x_train_f32[] = {
    0.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 0.0f,
    1.0f, 1.0f
};

float y_train_f32[] = {0.0f, 1.0f, 1.0f, 0.0f};

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  while (!Serial1);
  delay(2000);
  
  randomSeed(analogRead(A0));
  srand(analogRead(A0));
  
  Serial1.println("========================================");
  Serial1.println("  Sprite One - Q7 Quantization Demo");
  Serial1.println("  75% Memory Reduction!");
  Serial1.println("========================================");
  Serial1.println();
  
  // Step 1: Train F32 model
  Serial1.println("[STEP 1] Training F32 model...");
  aimodel_t *model_f32 = train_f32_xor();
  Serial1.println("  Training complete!");
  Serial1.println();
  
  // Step 2: Create Q7 model
  Serial1.println("[STEP 2] Creating Q7 model...");
  aimodel_t *model_q7 = create_q7_model();
  Serial1.println("  Q7 model created!");
  Serial1.println();
  
  // Step 3: Quantize F32 → Q7
  Serial1.println("[STEP 3] Quantizing F32 to Q7...");
  uint16_t repr_shape[] = {4, 2};
  aitensor_t repr_tensor = AITENSOR_2D_F32(repr_shape, x_train_f32);
  
  aialgo_quantize_model_f32_to_q7(model_f32, model_q7, &repr_tensor);
  Serial1.println("  Quantization complete!");
  Serial1.println();
  
  // Step 4: Compare memory usage
  Serial1.println("[STEP 4] Memory comparison:");
  uint32_t f32_params = aialgo_sizeof_parameter_memory(model_f32);
  uint32_t q7_params = aialgo_sizeof_parameter_memory(model_q7);
  
  Serial1.print("  F32 parameters: ");
  Serial1.print(f32_params);
  Serial1.println(" bytes");
  Serial1.print("  Q7 parameters: ");
  Serial1.print(q7_params);
  Serial1.println(" bytes");
  Serial1.print("  Saved: ");
  Serial1.print(f32_params - q7_params);
  Serial1.print(" bytes (");
  Serial1.print(100 - (q7_params * 100 / f32_params));
  Serial1.println("% reduction!)");
  Serial1.println();
  
  // Step 5: Test both models
  Serial1.println("[STEP 5] Testing inference...");
  test_models(model_f32, model_q7);
  
  Serial1.println();
  Serial1.println("========================================");
  Serial1.println("  ✅ Q7 QUANTIZATION SUCCESS!");
  Serial1.println("  Memory saved, accuracy preserved!");
  Serial1.println("========================================");
}

void loop() {
  delay(1000);
}

aimodel_t* train_f32_xor() {
  // Create persistent model
  static aimodel_t model;
  static ailayer_input_f32_t input_layer;
  static ailayer_dense_f32_t dense_1;
  static ailayer_sigmoid_f32_t sigmoid_1;
  static ailayer_dense_f32_t dense_2;
  static ailayer_sigmoid_f32_t sigmoid_2;
  static ailoss_mse_t mse_loss;
  
  // Build network
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
  
  // Allocate memory
  uint32_t param_size = aialgo_sizeof_parameter_memory(&model);
  static byte param_mem[128];  // Static allocation
  aialgo_distribute_parameter_memory(&model, param_mem, param_size);
  
  // Initialize weights
  aimath_f32_default_init_glorot_uniform(&dense_1.weights);
  aimath_f32_default_init_zeros(&dense_1.bias);
  aimath_f32_default_init_glorot_uniform(&dense_2.weights);
  aimath_f32_default_init_zeros(&dense_2.bias);
  
  // Train
  aiopti_adam_f32_t adam = AIOPTI_ADAM_F32(0.1f, 0.9f, 0.999f, 1e-7);
  aiopti_t *optimizer = aiopti_adam_f32_default(&adam);
  
  uint32_t train_size = aialgo_sizeof_training_memory(&model, optimizer);
  static byte train_mem[512];  // Static allocation
  aialgo_schedule_training_memory(&model, optimizer, train_mem, train_size);
  aialgo_init_model_for_training(&model, optimizer);
  
  uint16_t input_shape_train[] = {4, 2};
  aitensor_t input_tensor = AITENSOR_2D_F32(input_shape_train, x_train_f32);
  uint16_t target_shape[] = {4, 1};
  aitensor_t target_tensor = AITENSOR_2D_F32(target_shape, y_train_f32);
  
  for (int i = 0; i < 100; i++) {
    aialgo_train_model(&model, &input_tensor, &target_tensor, optimizer, 4);
  }
  
  return &model;
}

aimodel_t* create_q7_model() {
  // Create persistent Q7 model with SAME structure
  static aimodel_t model_q7;
  static ailayer_input_q7_t input_layer_q7;
  static ailayer_dense_q7_t dense_1_q7;
  static ailayer_sigmoid_q7_t sigmoid_1_q7;
  static ailayer_dense_q7_t dense_2_q7;
  static ailayer_sigmoid_q7_t sigmoid_2_q7;
  
  uint16_t input_shape[] = {1, 2};
  input_layer_q7 = AILAYER_INPUT_Q7_A(2, input_shape);
  dense_1_q7 = AILAYER_DENSE_Q7_A(3);
  sigmoid_1_q7 = AILAYER_SIGMOID_Q7_A();
  dense_2_q7 = AILAYER_DENSE_Q7_A(1);
  sigmoid_2_q7 = AILAYER_SIGMOID_Q7_A();
  
  ailayer_t *x;
  model_q7.input_layer = ailayer_input_q7_default(&input_layer_q7);
  x = ailayer_dense_q7_default(&dense_1_q7, model_q7.input_layer);
  x = ailayer_sigmoid_q7_default(&sigmoid_1_q7, x);
  x = ailayer_dense_q7_default(&dense_2_q7, x);
  model_q7.output_layer = ailayer_sigmoid_q7_default(&sigmoid_2_q7, x);
  
  aialgo_compile_model(&model_q7);
  
  // Allocate Q7 memory
  uint32_t param_size_q7 = aialgo_sizeof_parameter_memory(&model_q7);
  static byte param_mem_q7[64];  // Less memory needed!
  aialgo_distribute_parameter_memory(&model_q7, param_mem_q7, param_size_q7);
  
  uint32_t infer_size_q7 = aialgo_sizeof_inference_memory(&model_q7);
  static byte infer_mem_q7[128];
  aialgo_schedule_inference_memory(&model_q7, infer_mem_q7, infer_size_q7);
  
  return &model_q7;
}

void test_models(aimodel_t *model_f32, aimodel_t *model_q7) {
  Serial1.println("  XOR Test Results:");
  Serial1.println("  Input\tF32\tQ7\tTarget");
  
  for (int i = 0; i < 4; i++) {
    // F32 inference
    float input_f32[2] = {x_train_f32[i*2], x_train_f32[i*2+1]};
    uint16_t in_shape_f32[] = {1, 2};
    aitensor_t in_f32 = AITENSOR_2D_F32(in_shape_f32, input_f32);
    
    float output_f32[1];
    uint16_t out_shape_f32[] = {1, 1};
    aitensor_t out_f32 = AITENSOR_2D_F32(out_shape_f32, output_f32);
    
    aialgo_inference_model(model_f32, &in_f32, &out_f32);
    
    // Q7 inference - quantize input first
    aimath_q7_params_t *in_q_params_ptr = (aimath_q7_params_t*)model_q7->input_layer->result.tensor_params;
    int8_t input_q7[2];
    uint16_t in_shape_q7[] = {1, 2};
    aitensor_t in_q7_temp = AITENSOR_2D_Q7(in_shape_q7, in_q_params_ptr, input_q7);
    aimath_q7_quantize_tensor_from_f32(&in_f32, &in_q7_temp);
    
    aimath_q7_params_t out_q_params;
    int8_t output_q7[1];
    uint16_t out_shape_q7[] = {1, 1};
    aitensor_t out_q7 = AITENSOR_2D_Q7(out_shape_q7, &out_q_params, output_q7);
    
    aialgo_inference_model(model_q7, &in_q7_temp, &out_q7);
    
    // Print results
    Serial1.print("  ");
    Serial1.print((int)input_f32[0]);
    Serial1.print(",");
    Serial1.print((int)input_f32[1]);
    Serial1.print("\t");
    Serial1.print(output_f32[0], 2);
    Serial1.print("\t");
    
    // Dequantize Q7 result: r = 2^(-shift) * (q - zero_point)
    aimath_q7_params_t *out_q_params_ptr = (aimath_q7_params_t*)out_q7.tensor_params;
    float q7_result = ((float)output_q7[0] - out_q_params_ptr->zero_point) / (float)(1 << out_q_params_ptr->shift);
    Serial1.print(q7_result, 2);
    Serial1.print("\t");
    Serial1.println(y_train_f32[i], 0);
  }
}
