import struct
import random
import os
import binascii

# AIfES Model Format Constants
MODEL_MAGIC = 0x54525053  # "SPRT"
MODEL_VERSION = 0x0001    # Revert to V1 for WebApp compatibility
MODEL_TYPE_F32 = 0

"""
Sentinel Model Generator
------------------------
Generates a 'God Mode' model binary (.aif32) for the Sprite One Sentinel demo.
This model mimics a Vision -> Brain architecture with:
- Input: 128 (Feature Vector from Vision Model)
- Hidden: 128 (Dense + ReLU)
- Output: 5 (Class logic)

This script serves as a reference for manually generating AIfES V2 binaries.
"""

def generate_sentinel_model(filename="sentinel_god.aif32"):
    print(f"Generating God-Mode Sentinel Model (V1 Compatible): {filename}")
    
    # 1. Header (32 bytes) - Standard V1
    # ----------------------------------
    header = bytearray(32)
    struct.pack_into('<I', header, 0, MODEL_MAGIC)
    struct.pack_into('<H', header, 4, MODEL_VERSION)
    struct.pack_into('<B', header, 6, 1) # 1 Input
    struct.pack_into('<B', header, 7, 5) # 5 Outputs (Logical)
    struct.pack_into('<B', header, 8, 128) # Hidden size mock
    struct.pack_into('<B', header, 9, MODEL_TYPE_F32)
    
    # Name
    name = b"Sentinel God"
    header[16:16+len(name)] = name
    
    struct.pack_into('<I', header, 12, 0) # CRC placeholder
    
    # 2. Body (Dynamic Model Definition)
    # ----------------------------------
    # We will generate a Valid "God Mode" Model Structure
    # Input(128) -> Dense(128) -> ReLU -> Dense(5) -> Softmax
    
    body = bytearray()
    
    # --- Layer 1: Dense (128 -> 128) ---
    # In V2 Dynamic Format, we just pack the weights/biases sequentially
    # The firmware "Universally" knows a Dense layer needs (Input*Output) weights + (Output) biases
    
    # Weights: 128 * 128 floats
    w1_size = 128 * 128
    print(f"  Generating Dense1 Weights ({w1_size} floats)...")
    for _ in range(w1_size):
        body.extend(struct.pack('<f', random.uniform(-0.5, 0.5)))
        
    # Bias: 128 floats
    print(f"  Generating Dense1 Bias (128 floats)...")
    for _ in range(128):
        body.extend(struct.pack('<f', 0.0))
        
    # --- Layer 2: Dense (128 -> 5) ---
    w2_size = 128 * 5
    print(f"  Generating Dense2 Weights ({w2_size} floats)...")
    for _ in range(w2_size):
        body.extend(struct.pack('<f', random.uniform(-0.5, 0.5)))
        
    # Bias: 5 floats
    print(f"  Generating Dense2 Bias (5 floats)...")
    for _ in range(5):
        body.extend(struct.pack('<f', 0.0))
        
    # 3. Finalize
    content = body
    
    # Calculate CRC
    crc = binascii.crc32(content) & 0xFFFFFFFF
    struct.pack_into('<I', header, 12, crc)
    
    # Write File
    with open(filename, "wb") as f:
        f.write(header)
        f.write(content)
        
    print(f"  Success! Size: {len(header) + len(content)} bytes")
    print(f"  Generated Valid AIfES Binary for Dynamic Loader test.")

if __name__ == "__main__":
    generate_sentinel_model()
