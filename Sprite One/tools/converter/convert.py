"""
Universal Model Converter
Convert any supported model format to .aif32

Funnel Architecture:
1. normalize.py: .h5/.pb/.json -> .tflite (requires TensorFlow)
2. tflite_to_aif32.py: .tflite -> .aif32 (lightweight, no TF)

Usage:
  python convert.py model.h5 output.aif32
  python convert.py model.tflite output.aif32
  python convert.py model.json output.aif32
"""

import sys
import os
import tempfile
from pathlib import Path

from tflite_to_aif32 import convert_tflite_to_aif32


def convert_to_aif32(input_path: str, output_path: str = None, name: str = None) -> dict:
    """
    Convert any supported model to .aif32.
    
    Supported inputs:
    - .tflite (direct conversion)
    - .h5, .keras (requires TensorFlow)
    - .pb (requires TensorFlow)
    - .json (TF.js, requires TensorFlow + tensorflowjs)
    
    Returns dict with conversion info.
    """
    input_path = Path(input_path)
    
    if output_path is None:
        output_path = str(input_path.with_suffix('.aif32'))
    
    if name is None:
        name = input_path.stem
    
    # If already .tflite, convert directly
    if input_path.suffix.lower() == '.tflite':
        return convert_tflite_to_aif32(str(input_path), output_path, name)
    
    # Otherwise, normalize first
    from normalize import normalize_to_tflite
    
    with tempfile.TemporaryDirectory() as tmpdir:
        temp_tflite = os.path.join(tmpdir, 'temp.tflite')
        
        print(f"Normalizing {input_path.suffix} -> .tflite...")
        result = normalize_to_tflite(str(input_path), temp_tflite)
        
        if result is None:
            raise ValueError(f"Failed to normalize {input_path}")
        
        print("Converting .tflite -> .aif32...")
        return convert_tflite_to_aif32(temp_tflite, output_path, name)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Universal Model Converter")
        print("=" * 40)
        print("\nUsage: python convert.py input_model [output.aif32] [name]")
        print("\nSupported input formats:")
        print("  .tflite  - TensorFlow Lite (lightweight)")
        print("  .h5      - Keras (requires tensorflow)")
        print("  .keras   - Keras (requires tensorflow)")
        print("  .pb      - Frozen graph (requires tensorflow)")
        print("  .json    - TF.js (requires tensorflow, tensorflowjs)")
        print("\nExamples:")
        print("  python convert.py mnist.tflite")
        print("  python convert.py gesture.h5 gesture.aif32")
        print("  python convert.py tfjs_model/model.json custom.aif32 my_model")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    name = sys.argv[3] if len(sys.argv) > 3 else None
    
    try:
        info = convert_to_aif32(input_path, output_path, name)
        
        print("\n" + "=" * 40)
        print("Conversion Successful!")
        print("=" * 40)
        print(f"Input:  {input_path} ({info['input_size']} bytes)")
        print(f"Output: {info['output_path']} ({info['output_size']} bytes)")
        print(f"\nModel Structure:")
        print(f"  Operators: {info['num_operators']}")
        print(f"  Tensors:   {info['num_tensors']}")
        print(f"  Buffers:   {info['num_buffers']}")
        print(f"\nLayers:")
        for i, layer in enumerate(info['layers']):
            print(f"  {i}: {layer['type']}")
        
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
