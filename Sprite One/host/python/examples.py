"""
Sprite One Python Examples
Week 4 Day 23

Simple examples showing how to use sprite_one.py library.
"""

from sprite_one import SpriteOne
import time

def example_ai_training():
    """Train and test XOR model."""
    print("=== AI Training Example ===\n")
    
    with SpriteOne('COM3') as sprite:  # Change port as needed
        # Train
        print("Training XOR model (100 epochs)...")
        loss = sprite.ai_train(100)
        print(f"Final loss: {loss:.6f}\n")
        
        # Test all cases
        print("Testing XOR:")
        for a in [0, 1]:
            for b in [0, 1]:
                result = sprite.ai_infer(float(a), float(b))
                print(f"  {a} XOR {b} = {result:.3f}")
        
        # Save
        sprite.ai_save("/xor.aif32")
        print("\nModel saved!")


def example_graphics():
    """Simple graphics demo."""
    print("=== Graphics Example ===\n")
    
    with SpriteOne('COM3') as sprite:
        sprite.clear()
        
        # Draw some rectangles
        sprite.rect(10, 10, 30, 20, color=1)
        sprite.rect(50, 10, 30, 20, color=1)
        sprite.rect(10, 40, 70, 10, color=1)
        
        # Draw text
        sprite.text(15, 15, "HELLO")
        
        # Update display
        sprite.flush()
        print("Graphics drawn!")


def example_model_management():
    """Demonstrate saving/loading models."""
    print("=== Model Management Example ===\n")
    
    with SpriteOne('COM3') as sprite:
        # Train and save
        print("Training model...")
        sprite.ai_train(50)
        sprite.ai_save("/test_model.aif32")
        
        # List models
        models = sprite.ai_list_models()
        print(f"\nSaved models: {models}")
        
        # Load model
        sprite.ai_load("/test_model.aif32")
        print("Model loaded!")
        
        # Test it
        result = sprite.ai_infer(1.0, 0.0)
        print(f"Inference test: 1 XOR 0 = {result:.3f}")


def example_continuous_inference():
    """Run continuous inference loop."""
    print("=== Continuous Inference Example ===\n")
    
    with SpriteOne('COM3') as sprite:
        print("Running 10 inferences...")
        
        for i in range(10):
            a = i % 2
            b = (i // 2) % 2
            result = sprite.ai_infer(float(a), float(b))
            print(f"  {i}: {a} XOR {b} = {result:.3f}")
            time.sleep(0.1)
        
        print("Done!")


if __name__ == "__main__":
    print("Sprite One Python Examples")
    print("=" * 40)
    print()
    print("Choose an example:")
    print("  1. AI Training")
    print("  2. Graphics")
    print("  3. Model Management")
    print("  4. Continuous Inference")
    print()
    
    choice = input("Enter choice (1-4): ")
    print()
    
    try:
        if choice == '1':
            example_ai_training()
        elif choice == '2':
            example_graphics()
        elif choice == '3':
            example_model_management()
        elif choice == '4':
            example_continuous_inference()
        else:
            print("Invalid choice!")
    except Exception as e:
        print(f"Error: {e}")
