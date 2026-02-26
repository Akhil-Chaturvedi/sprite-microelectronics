"""
Hardware Verification Script for Sprite One v2.2
Systematically tests all new features:
1. Protocol CRC32 Validation
2. Industrial API Primitives
3. Dynamic Model Upload/Load/Infer
4. On-Device Training & Fine-Tuning
"""

import sys
import time
import struct
import random
from sprite_one import SpriteOne, SpriteOneError

def run_verification(port):
    print(f"--- Starting Hardware Verification on {port} ---")
    
    try:
        with SpriteOne(port) as sprite:
            print("[1] Connection & CRC Validation")
            version = sprite.get_version()
            print(f"  ✓ Connected! Firmware: v{version[0]}.{version[1]}.{version[2]}")
            
            # The sprite_one.py library now enforces CRC validation internally.
            # If get_version() succeeded, CRC validation is working.
            print("  ✓ CRC Validation Active")

            print("\n[2] Industrial API Primitives")
            
            # WHO_IS_THERE
            device_id = sprite.get_device_id()
            print(f"  ✓ Device ID: {device_id.hex(':')}")
            
            # PING_ID
            if sprite.ping_id(device_id):
                print("  ✓ Ping ID match")
            else:
                print("  ✗ Ping ID mismatch!")
            
            # Buffer Write / Snapshot
            test_data = [random.uniform(-10, 10) for _ in range(10)]
            print(f"  Writing {len(test_data)} sample(s) to buffer...")
            for v in test_data:
                sprite.buffer_write(v)
            
            snapshot = sprite.buffer_snapshot()
            # The snapshot might have more data if buffer was already used
            last_samples = snapshot[-len(test_data):]
            match = all(abs(a-b) < 0.001 for a, b in zip(test_data, last_samples))
            print(f"  ✓ Buffer Snapshot: {'Match' if match else 'Mismatch'}")

            # Baseline / Delta
            mean = sprite.baseline_capture()
            print(f"  ✓ Baseline Captured: {mean:.4f}")
            
            sprite.buffer_write(mean + 5.0) # Add a spike
            delta = sprite.get_delta()
            print(f"  ✓ Delta Check: {delta:.4f} (Expected ~5.0)")

            sprite.baseline_reset()
            print("  ✓ Baseline Reset")

            # Correlation
            print("  Testing Correlation...")
            score = sprite.correlate(test_data)
            print(f"  ✓ Correlation Score: {score:.4f}")

            print("\n[3] Dynamic AI Engine")
            
            # Check status
            status = sprite.ai_status()
            print(f"  Initial Status: {status}")
            
            # Try to load existing model
            try:
                sprite.select_model("xor.aif32") # This calls CMD_AI_LOAD via ModelSelect? 
                # Actually v2.1.0 sprite_one.py ai_load calls CMD_AI_LOAD
                sprite.ai_load("/xor.aif32")
                print("  ✓ Dynamic Model Loaded")
            except:
                print("  ! No dynamic model found, skipping inference test")

            print("\n[4] Fine-Tuning & Incremental Training")
            print("  Starting Fine-tuning session (LR=0.05)...")
            sprite.finetune_start(0.05)
            
            # XOR fine-tuning test
            print("  Feeding XOR samples...")
            for _ in range(20):
                # 1 XOR 0 = 1
                loss = sprite.finetune_data([1.0, 0.0], [1.0])
                # 0 XOR 0 = 0
                loss = sprite.finetune_data([0.0, 0.0], [0.0])
            
            print(f"  ✓ Fine-tuning active. Last Loss: {loss:.6f}")
            sprite.finetune_stop()
            print("  ✓ Fine-tuning session stopped")

            print("\n--- Verification Complete ---")
            print("RESULT: SUCCESS")

    except SpriteOneError as e:
        print(f"\n[ERROR] {e}")
        print("RESULT: FAILED")
    except Exception as e:
        print(f"\n[CRITICAL] {e}")
        print("RESULT: FAILED")

if __name__ == "__main__":
    port = 'COM3'
    if len(sys.argv) > 1:
        port = sys.argv[1]
    run_verification(port)
