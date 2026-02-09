"""
Model Normalizer
Convert various model formats (.h5, .pb, TF.js) to .tflite

This is the "heavy" step that requires TensorFlow.
Run separately from the lightweight extraction.

Dependencies: pip install tensorflow
"""

import os
import sys
import json
import tempfile
import shutil
from pathlib import Path
from typing import Optional

# Only import TensorFlow when this module is used
tf = None

def _ensure_tf():
    global tf
    if tf is None:
        import tensorflow
        tf = tensorflow
        # Suppress TF warnings
        os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'


def convert_keras_h5_to_tflite(h5_path: str, output_path: str) -> bool:
    """Convert Keras .h5 model to TFLite."""
    _ensure_tf()
    
    try:
        model = tf.keras.models.load_model(h5_path)
        converter = tf.lite.TFLiteConverter.from_keras_model(model)
        tflite_model = converter.convert()
        
        with open(output_path, 'wb') as f:
            f.write(tflite_model)
        
        return True
    except Exception as e:
        print(f"Error converting .h5: {e}")
        return False


def convert_saved_model_to_tflite(saved_model_dir: str, output_path: str) -> bool:
    """Convert TensorFlow SavedModel to TFLite."""
    _ensure_tf()
    
    try:
        converter = tf.lite.TFLiteConverter.from_saved_model(saved_model_dir)
        tflite_model = converter.convert()
        
        with open(output_path, 'wb') as f:
            f.write(tflite_model)
        
        return True
    except Exception as e:
        print(f"Error converting SavedModel: {e}")
        return False


def convert_tfjs_to_tflite(tfjs_json_path: str, output_path: str) -> bool:
    """
    Convert TensorFlow.js model to TFLite.
    Requires tensorflowjs package: pip install tensorflowjs
    """
    _ensure_tf()
    
    try:
        import tensorflowjs as tfjs
        
        # Create temp directory for intermediate SavedModel
        with tempfile.TemporaryDirectory() as tmpdir:
            saved_model_path = os.path.join(tmpdir, 'saved_model')
            
            # Convert TF.js to SavedModel
            tfjs.converters.convert_tf_saved_model(
                tfjs_json_path,
                saved_model_path
            )
            
            # Convert SavedModel to TFLite
            return convert_saved_model_to_tflite(saved_model_path, output_path)
            
    except ImportError:
        print("tensorflowjs not installed. Run: pip install tensorflowjs")
        return False
    except Exception as e:
        print(f"Error converting TF.js: {e}")
        return False


def convert_pb_to_tflite(pb_path: str, output_path: str, 
                         input_arrays: Optional[list] = None,
                         output_arrays: Optional[list] = None) -> bool:
    """Convert frozen .pb graph to TFLite."""
    _ensure_tf()
    
    try:
        converter = tf.compat.v1.lite.TFLiteConverter.from_frozen_graph(
            pb_path,
            input_arrays or ['input'],
            output_arrays or ['output']
        )
        tflite_model = converter.convert()
        
        with open(output_path, 'wb') as f:
            f.write(tflite_model)
        
        return True
    except Exception as e:
        print(f"Error converting .pb: {e}")
        return False


def normalize_to_tflite(input_path: str, output_path: Optional[str] = None) -> Optional[str]:
    """
    Auto-detect format and convert to .tflite.
    
    Supported formats:
    - .tflite (passthrough)
    - .h5 (Keras)
    - .pb (Frozen graph)
    - .json (TF.js)
    - directory (SavedModel)
    
    Returns output path on success, None on failure.
    """
    input_path = Path(input_path)
    
    if output_path is None:
        output_path = str(input_path.with_suffix('.tflite'))
    
    # Already TFLite
    if input_path.suffix.lower() == '.tflite':
        if str(input_path) != output_path:
            shutil.copy(input_path, output_path)
        return output_path
    
    # Keras .h5
    if input_path.suffix.lower() in ['.h5', '.keras']:
        if convert_keras_h5_to_tflite(str(input_path), output_path):
            return output_path
        return None
    
    # TF.js model.json
    if input_path.suffix.lower() == '.json':
        if convert_tfjs_to_tflite(str(input_path), output_path):
            return output_path
        return None
    
    # Frozen graph .pb
    if input_path.suffix.lower() == '.pb':
        if convert_pb_to_tflite(str(input_path), output_path):
            return output_path
        return None
    
    # SavedModel directory
    if input_path.is_dir():
        if convert_saved_model_to_tflite(str(input_path), output_path):
            return output_path
        return None
    
    print(f"Unknown format: {input_path.suffix}")
    return None


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python normalize.py input_model [output.tflite]")
        print("\nSupported formats:")
        print("  .tflite  - TensorFlow Lite (passthrough)")
        print("  .h5      - Keras model")
        print("  .keras   - Keras model")
        print("  .pb      - Frozen graph")
        print("  .json    - TensorFlow.js")
        print("  dir/     - SavedModel")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    
    result = normalize_to_tflite(input_path, output_path)
    
    if result:
        print(f"Success: {result}")
        print(f"Size: {os.path.getsize(result)} bytes")
    else:
        print("Conversion failed")
        sys.exit(1)
