"""
Sprite One - Automated Test Suite
Week 4 Day 24

Comprehensive tests for protocol, AI, graphics, and integration.

Usage:
    python test_suite.py --port COM3
    python test_suite.py --port /dev/ttyUSB0 --verbose
"""

import sys
import time
import argparse
from sprite_one import SpriteOne, SpriteOneError

class TestResults:
    """Track test results."""
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.errors = []
    
    def pass_test(self, name):
        self.passed += 1
        print(f"  ✓ {name}")
    
    def fail_test(self, name, reason):
        self.failed += 1
        self.errors.append((name, reason))
        print(f"  ✗ {name}: {reason}")
    
    def summary(self):
        total = self.passed + self.failed
        print(f"\n{'='*50}")
        print(f"Test Results: {self.passed}/{total} passed ({self.failed} failed)")
        if self.errors:
            print(f"\nFailed Tests:")
            for name, reason in self.errors:
                print(f"  - {name}: {reason}")
        print(f"{'='*50}\n")
        return self.failed == 0


def test_connection(sprite, results):
    """Test basic connectivity."""
    print("\n[1] Connection Tests")
    
    try:
        version = sprite.get_version()
        if len(version) == 3:
            results.pass_test(f"Get version (v{version[0]}.{version[1]}.{version[2]})")
        else:
            results.fail_test("Get version", "Invalid response")
    except Exception as e:
        results.fail_test("Get version", str(e))


def test_graphics(sprite, results):
    """Test graphics commands."""
    print("\n[2] Graphics Tests")
    
    # Clear
    try:
        sprite.clear()
        results.pass_test("Clear display")
    except Exception as e:
        results.fail_test("Clear display", str(e))
    
    # Pixel
    try:
        sprite.pixel(10, 10, 1)
        results.pass_test("Draw pixel")
    except Exception as e:
        results.fail_test("Draw pixel", str(e))
    
    # Rectangle
    try:
        sprite.rect(20, 20, 30, 15, 1)
        results.pass_test("Draw rectangle")
    except Exception as e:
        results.fail_test("Draw rectangle", str(e))
    
    # Text
    try:
        sprite.text(5, 5, "TEST", 1)
        results.pass_test("Draw text")
    except Exception as e:
        results.fail_test("Draw text", str(e))
    
    # Flush
    try:
        sprite.flush()
        results.pass_test("Flush display")
    except Exception as e:
        results.fail_test("Flush display", str(e))


def test_ai_training(sprite, results):
    """Test AI training."""
    print("\n[3] AI Training Tests")
    
    # Train model
    try:
        loss = sprite.ai_train(50)
        if loss < 1.0:  # Reasonable loss
            results.pass_test(f"Train model (loss: {loss:.4f})")
        else:
            results.fail_test("Train model", f"Loss too high: {loss}")
    except Exception as e:
        results.fail_test("Train model", str(e))
    
    # Get status
    try:
        status = sprite.ai_status()
        if status.get('model_loaded'):
            results.pass_test("Get AI status (model loaded)")
        else:
            results.fail_test("Get AI status", "Model not loaded")
    except Exception as e:
        results.fail_test("Get AI status", str(e))


def test_ai_inference(sprite, results):
    """Test AI inference."""
    print("\n[4] AI Inference Tests")
    
    # XOR test cases
    test_cases = [
        (0.0, 0.0, 0, "0 XOR 0 = 0"),
        (0.0, 1.0, 1, "0 XOR 1 = 1"),
        (1.0, 0.0, 1, "1 XOR 0 = 1"),
        (1.0, 1.0, 0, "1 XOR 1 = 0")
    ]
    
    for in0, in1, expected, desc in test_cases:
        try:
            result = sprite.ai_infer(in0, in1)
            prediction = 1 if result > 0.5 else 0
            if prediction == expected:
                results.pass_test(f"{desc} (result: {result:.3f})")
            else:
                results.fail_test(desc, f"Expected {expected}, got {prediction}")
        except Exception as e:
            results.fail_test(desc, str(e))


def test_ai_persistence(sprite, results):
    """Test model save/load."""
    print("\n[5] AI Persistence Tests")
    
    test_file = "/test_model.aif32"
    
    # Save model
    try:
        sprite.ai_save(test_file)
        results.pass_test("Save model to flash")
    except Exception as e:
        results.fail_test("Save model to flash", str(e))
        return  # Can't continue without save
    
    # List models
    try:
        models = sprite.ai_list_models()
        if test_file.strip('/') in [m.strip('/') for m in models]:
            results.pass_test(f"List models (found {len(models)})")
        else:
            results.fail_test("List models", f"Test model not in list: {models}")
    except Exception as e:
        results.fail_test("List models", str(e))
    
    # Load model
    try:
        sprite.ai_load(test_file)
        results.pass_test("Load model from flash")
    except Exception as e:
        results.fail_test("Load model from flash", str(e))
    
    # Verify after load
    try:
        result = sprite.ai_infer(1.0, 0.0)
        prediction = 1 if result > 0.5 else 0
        if prediction == 1:
            results.pass_test("Inference after load")
        else:
            results.fail_test("Inference after load", f"Wrong result: {result}")
    except Exception as e:
        results.fail_test("Inference after load", str(e))
    
    # Delete model
    try:
        sprite.ai_delete(test_file)
        results.pass_test("Delete model")
    except Exception as e:
        results.fail_test("Delete model", str(e))


def test_integration(sprite, results):
    """Test combined workflows."""
    print("\n[6] Integration Tests")
    
    # Full workflow: clear, train, infer, draw results
    try:
        # Clear display
        sprite.clear()
        
        # Train
        loss = sprite.ai_train(30)
        
        # Infer all cases
        results_list = []
        for a in [0, 1]:
            for b in [0, 1]:
                r = sprite.ai_infer(float(a), float(b))
                results_list.append(r)
        
        # Draw results
        sprite.rect(10, 10, 20, 20, 1)
        sprite.flush()
        
        if len(results_list) == 4:
            results.pass_test("Full workflow (graphics + AI)")
        else:
            results.fail_test("Full workflow", "Incomplete results")
    except Exception as e:
        results.fail_test("Full workflow", str(e))


def test_error_handling(sprite, results):
    """Test error conditions."""
    print("\n[7] Error Handling Tests")
    
    # Load non-existent model
    try:
        sprite.ai_load("/nonexistent.aif32")
        results.fail_test("Load missing model", "Should have raised error")
    except SpriteOneError:
        results.pass_test("Load missing model (error raised)")
    except Exception as e:
        results.fail_test("Load missing model", f"Wrong exception: {e}")
    
    # Delete non-existent model
    try:
        sprite.ai_delete("/nonexistent.aif32")
        results.fail_test("Delete missing model", "Should have raised error")
    except SpriteOneError:
        results.pass_test("Delete missing model (error raised)")
    except Exception as e:
        results.fail_test("Delete missing model", f"Wrong exception: {e}")


def run_stress_test(sprite, results, iterations=10):
    """Stress test with repeated operations."""
    print(f"\n[8] Stress Test ({iterations} iterations)")
    
    try:
        for i in range(iterations):
            # Graphics
            sprite.clear()
            sprite.rect(i * 5, i * 5, 10, 10, 1)
            sprite.flush()
            
            # AI
            result = sprite.ai_infer(float(i % 2), float((i // 2) % 2))
            
            if i % 5 == 0:
                print(f"    Progress: {i}/{iterations}")
        
        results.pass_test(f"Stress test ({iterations} iterations)")
    except Exception as e:
        results.fail_test("Stress test", str(e))


def main():
    parser = argparse.ArgumentParser(description='Sprite One Test Suite')
    parser.add_argument('--port', required=True, help='Serial port (e.g., COM3)')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    parser.add_argument('--stress', type=int, default=10, help='Stress test iterations')
    args = parser.parse_args()
    
    print("=" * 50)
    print("Sprite One - Automated Test Suite")
    print("Week 4 Day 24")
    print("=" * 50)
    
    results = TestResults()
    
    try:
        print(f"\nConnecting to {args.port}...")
        with SpriteOne(args.port) as sprite:
            print("Connected!\n")
            
            # Run all tests
            test_connection(sprite, results)
            test_graphics(sprite, results)
            test_ai_training(sprite, results)
            test_ai_inference(sprite, results)
            test_ai_persistence(sprite, results)
            test_integration(sprite, results)
            test_error_handling(sprite, results)
            run_stress_test(sprite, results, args.stress)
            
    except KeyboardInterrupt:
        print("\n\nTests interrupted by user")
    except Exception as e:
        print(f"\n\n[ERROR] Connection failed: {e}")
        print("Make sure Sprite One is connected and the port is correct.")
        return False
    
    # Show summary
    success = results.summary()
    return success


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
