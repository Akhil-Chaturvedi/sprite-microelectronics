/*
 * Sprite One - XOR Training with AIfES
 * Week 3 Days 17-18: ON-DEVICE LEARNING SUCCESS!
 * 
 * Adapted from: AIfES 1_XOR_F32_training example
 * by Fraunhofer IMS
 * 
 * Network: 2-3(Sigmoid)-1(Sigmoid)
 * Optimizer: ADAM
 * Epochs: 100
 * 
 * This demonstrates true on-device learning - the $4 chip
 * trains a neural network from scratch using gradient descent!
 */

#include <aifes.h>

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  while (!Serial1);
  
  // IMPORTANT: AIfES requires random weights for training
  // Use analog noise as random seed
  randomSeed(analogRead(A0));
  srand(analogRead(A0));
  
  Serial1.println("========================================");
  Serial1.println("  Sprite One - XOR Training Demo");
  Serial1.println("  ON-DEVICE LEARNING!");
  Serial1.println("========================================");
  Serial1.println();
  
  // Start training automatically
  delay(1000);
  train_xor();
}

void loop() {
  delay(1000);
}

void train_xor() {
  uint32_t i;
  
  Serial1.println("[DATA] XOR Truth Table:");
  Serial1.println("  0 XOR 0 = 0");
  Serial1.println("  0 XOR 1 = 1");
  Serial1.println("  1 XOR 0 = 1");
  Serial1.println("  1 XOR 1 = 0");
  Serial1.println();
  
  // ===== TRAINING DATA =====
  
  // Input tensor (4 samples, 2 features)
  uint16_t input_shape[] = {4, 2};
  float input_data[4*2] = {
    0.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 0.0f,
    1.0f, 1.0f
  };
  aitensor_t input_tensor = AITENSOR_2D_F32(input_shape, input_data);
  
  // Target tensor (4 samples, 1 output)
  uint16_t target_shape[] = {4, 1};
  float target_data[4*1] = {
    0.0f,
    1.0f,
    1.0f,
    0.0f
  };
  aitensor_t target_tensor = AITENSOR_2D_F32(target_shape, target_data);
  
  // Output tensor (for results)
  uint16_t output_shape[] = {4, 1};
  float output_data[4*1];
  aitensor_t output_tensor = AITENSOR_2D_F32(output_shape, output_data);
  
  // ===== NEURAL NETWORK LAYERS =====
  
  Serial1.println("[INIT] Creating neural network...");
  Serial1.println("  Architecture: 2 -> 3 (Sigmoid) -> 1 (Sigmoid)");
  
  uint16_t input_layer_shape[] = {1, 2};
  ailayer_input_f32_t   input_layer     = AILAYER_INPUT_F32_A(2, input_layer_shape);
  ailayer_dense_f32_t   dense_layer_1   = AILAYER_DENSE_F32_A(3);  // Hidden: 3 neurons
  ailayer_sigmoid_f32_t sigmoid_layer_1 = AILAYER_SIGMOID_F32_A();
  ailayer_dense_f32_t   dense_layer_2   = AILAYER_DENSE_F32_A(1);  // Output: 1 neuron
  ailayer_sigmoid_f32_t sigmoid_layer_2 = AILAYER_SIGMOID_F32_A();
  
  ailoss_mse_t mse_loss;  // Mean squared error loss
  
  // ===== BUILD MODEL =====
  
  aimodel_t model;
  ailayer_t *x;
  
  // Connect layers
  model.input_layer = ailayer_input_f32_default(&input_layer);
  x = ailayer_dense_f32_default(&dense_layer_1, model.input_layer);
  x = ailayer_sigmoid_f32_default(&sigmoid_layer_1, x);
  x = ailayer_dense_f32_default(&dense_layer_2, x);
  x = ailayer_sigmoid_f32_default(&sigmoid_layer_2, x);
  model.output_layer = x;
  
  // Add loss function
  model.loss = ailoss_mse_f32_default(&mse_loss, model.output_layer);
  
  // Compile model
  aialgo_compile_model(&model);
  
  Serial1.println("  Model compiled!");
  Serial1.println();
  
  // ===== ALLOCATE PARAMETER MEMORY =====
  
  uint32_t parameter_memory_size = aialgo_sizeof_parameter_memory(&model);
  Serial1.print("[MEMORY] Parameters: ");
  Serial1.print(parameter_memory_size);
  Serial1.println(" bytes");
  
  byte *parameter_memory = (byte *) malloc(parameter_memory_size);
  aialgo_distribute_parameter_memory(&model, parameter_memory, parameter_memory_size);
  
  // ===== INITIALIZE WEIGHTS =====
  
  // Glorot uniform initialization (recommended)
  aimath_f32_default_init_glorot_uniform(&dense_layer_1.weights);
  aimath_f32_default_init_zeros(&dense_layer_1.bias);
  aimath_f32_default_init_glorot_uniform(&dense_layer_2.weights);
  aimath_f32_default_init_zeros(&dense_layer_2.bias);
  
  // ===== SETUP OPTIMIZER =====
  
  Serial1.println("[OPTIMIZER] ADAM (Learning rate: 0.1)");
  
  aiopti_adam_f32_t adam_opti = AIOPTI_ADAM_F32(0.1f, 0.9f, 0.999f, 1e-7);
  aiopti_t *optimizer = aiopti_adam_f32_default(&adam_opti);
  
  // ===== ALLOCATE TRAINING MEMORY =====
  
  uint32_t memory_size = aialgo_sizeof_training_memory(&model, optimizer);
  Serial1.print("[MEMORY] Training workspace: ");
  Serial1.print(memory_size);
  Serial1.println(" bytes");
  Serial1.println();
  
  byte *memory_ptr = (byte *) malloc(memory_size);
  
  if(memory_ptr == 0) {
    Serial1.println("ERROR: Not enough RAM for training!");
    while(1);
  }
  
  aialgo_schedule_training_memory(&model, optimizer, memory_ptr, memory_size);
  aialgo_init_model_for_training(&model, optimizer);
  
  // ===== BEFORE TRAINING =====
  
  aialgo_inference_model(&model, &input_tensor, &output_tensor);
  
  Serial1.println("[BEFORE] Untrained network predictions:");
  Serial1.println("  Input\t\tTarget\t\tOutput");
  for (i = 0; i < 4; i++) {
    Serial1.print("  ");
    Serial1.print((int)input_data[i*2]);
    Serial1.print(",");
    Serial1.print((int)input_data[i*2+1]);
    Serial1.print("\t\t");
    Serial1.print(target_data[i], 0);
    Serial1.print("\t\t");
    Serial1.println(output_data[i], 4);
  }
  Serial1.println();
  
  // ===== TRAINING =====
  
  uint32_t batch_size = 4;
  uint16_t epochs = 100;
  uint16_t print_interval = 10;
  
  Serial1.println("[TRAIN] Starting gradient descent...");
  Serial1.print("  Epochs: ");
  Serial1.println(epochs);
  Serial1.print("  Batch size: ");
  Serial1.println(batch_size);
  Serial1.println();
  
  uint32_t train_start = millis();
  float loss;
  
  for(i = 0; i < epochs; i++) {
    // One epoch of training
    aialgo_train_model(&model, &input_tensor, &target_tensor, optimizer, batch_size);
    
    // Print loss every 10 epochs
    if(i % print_interval == 0) {
      aialgo_calc_loss_model_f32(&model, &input_tensor, &target_tensor, &loss);
      Serial1.print("  Epoch ");
      Serial1.print(i);
      Serial1.print("/");
      Serial1.print(epochs);
      Serial1.print(" - Loss: ");
      Serial1.println(loss, 6);
    }
  }
  
  uint32_t train_time = millis() - train_start;
  
  Serial1.println();
  Serial1.print("[DONE] Training completed in ");
  Serial1.print(train_time);
  Serial1.println(" ms");
  Serial1.println();
  
  // ===== AFTER TRAINING =====
  
  aialgo_inference_model(&model, &input_tensor, &output_tensor);
  
  Serial1.println("[AFTER] Trained network predictions:");
  Serial1.println("  Input\t\tTarget\t\tOutput\t\tResult");
  
  bool all_correct = true;
  
  for (i = 0; i < 4; i++) {
    Serial1.print("  ");
    Serial1.print((int)input_data[i*2]);
    Serial1.print(",");
    Serial1.print((int)input_data[i*2+1]);
    Serial1.print("\t\t");
    Serial1.print(target_data[i], 0);
    Serial1.print("\t\t");
    Serial1.print(output_data[i], 4);
    Serial1.print("\t\t");
    
    // Check if correct (within 0.1 tolerance)
    bool correct = fabs(output_data[i] - target_data[i]) < 0.1f;
    if (correct) {
      Serial1.println("✓");
    } else {
      Serial1.println("✗");
      all_correct = false;
    }
  }
  
  Serial1.println();
  Serial1.println("========================================");
  if (all_correct) {
    Serial1.println("  ✅ TRAINING SUCCESS!");
    Serial1.println("  Neural network learned XOR!");
  } else {
    Serial1.println("  ⚠️  Training incomplete");
    Serial1.println("  Try more epochs or reset");
  }
  Serial1.println("========================================");
  Serial1.println();
  
  Serial1.println("[STATS] Summary:");
  Serial1.print("  Training time: ");
  Serial1.print(train_time);
  Serial1.println(" ms");
  Serial1.print("  Time per epoch: ");
  Serial1.print(train_time / epochs);
  Serial1.println(" ms");
  Serial1.print("  Parameters: ");
  Serial1.print((2*3 + 3) + (3*1 + 1));
  Serial1.println(" (9 + 4 = 13)");
  Serial1.print("  Memory used: ");
  Serial1.print(parameter_memory_size + memory_size);
  Serial1.println(" bytes");
  Serial1.println();
  Serial1.println("🎉 ON-DEVICE LEARNING WORKS!");
  Serial1.println("The $4 chip just trained a neural network!");
  
  // Cleanup
  free(parameter_memory);
  free(memory_ptr);
}
