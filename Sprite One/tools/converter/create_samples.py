"""
Create sample AIFes models for testing.
These can be used to test the device without training.
"""

import struct
import numpy as np
from pathlib import Path


def create_aif32_model(name: str, input_size: int, output_size: int, 
                       hidden_size: int, weights: np.ndarray) -> bytes:
    """
    Create an AIFes .aif32 model binary.
    
    Args:
        name: Model name (max 15 chars)
        input_size: Number of inputs
        output_size: Number of outputs
        hidden_size: Hidden layer neurons
        weights: Float32 weight array
    
    Returns:
        Complete .aif32 binary
    """
    # CRC32
    def crc32(data: bytes) -> int:
        crc = 0xFFFFFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1))
        return ~crc & 0xFFFFFFFF
    
    weights_bytes = weights.astype(np.float32).tobytes()
    
    # Header (32 bytes)
    header = bytearray(32)
    struct.pack_into('<I', header, 0, 0x54525053)  # Magic "SPRT"
    struct.pack_into('<H', header, 4, 0x0002)       # Version
    header[6] = input_size
    header[7] = output_size
    header[8] = hidden_size
    header[9] = 0  # F32
    header[10] = 2  # 2 layers (hidden + output)
    header[11] = 0  # Reserved
    struct.pack_into('<I', header, 12, crc32(weights_bytes))
    
    # Name
    name_bytes = name[:15].encode('utf-8')
    header[16:16 + len(name_bytes)] = name_bytes
    
    return bytes(header) + weights_bytes


def create_xor_model() -> bytes:
    """Create XOR model: 2 inputs, 1 output, 4 hidden neurons."""
    # XOR learned weights (approximate)
    # Layer 1: 2x4 weights + 4 biases
    w1 = np.array([
        [ 5.0,  5.0, -5.0, -5.0],  # input 1
        [ 5.0, -5.0,  5.0, -5.0],  # input 2
    ], dtype=np.float32)
    b1 = np.array([-2.5, 2.5, 2.5, -2.5], dtype=np.float32)
    
    # Layer 2: 4x1 weights + 1 bias
    w2 = np.array([
        [5.0], [-5.0], [-5.0], [5.0]
    ], dtype=np.float32)
    b2 = np.array([0.0], dtype=np.float32)
    
    weights = np.concatenate([w1.flatten(), b1, w2.flatten(), b2])
    
    return create_aif32_model("xor", 2, 1, 4, weights)


def create_and_model() -> bytes:
    """Create AND gate model: 2 inputs, 1 output, 2 hidden neurons."""
    w1 = np.array([
        [5.0, 5.0],
        [5.0, 5.0],
    ], dtype=np.float32)
    b1 = np.array([-7.5, -7.5], dtype=np.float32)
    
    w2 = np.array([[5.0], [5.0]], dtype=np.float32)
    b2 = np.array([-2.5], dtype=np.float32)
    
    weights = np.concatenate([w1.flatten(), b1, w2.flatten(), b2])
    return create_aif32_model("and", 2, 1, 2, weights)


def create_or_model() -> bytes:
    """Create OR gate model: 2 inputs, 1 output, 2 hidden neurons."""
    w1 = np.array([
        [5.0, 5.0],
        [5.0, 5.0],
    ], dtype=np.float32)
    b1 = np.array([-2.5, -2.5], dtype=np.float32)
    
    w2 = np.array([[5.0], [5.0]], dtype=np.float32)
    b2 = np.array([-2.5], dtype=np.float32)
    
    weights = np.concatenate([w1.flatten(), b1, w2.flatten(), b2])
    return create_aif32_model("or", 2, 1, 2, weights)


def create_simple_classifier() -> bytes:
    """
    Create simple 4-class classifier: 4 inputs, 4 outputs, 8 hidden.
    Random weights for testing upload/inference only.
    """
    np.random.seed(42)
    
    # Layer 1: 4x8
    w1 = np.random.randn(4, 8).astype(np.float32) * 0.5
    b1 = np.zeros(8, dtype=np.float32)
    
    # Layer 2: 8x4
    w2 = np.random.randn(8, 4).astype(np.float32) * 0.5
    b2 = np.zeros(4, dtype=np.float32)
    
    weights = np.concatenate([w1.flatten(), b1, w2.flatten(), b2])
    return create_aif32_model("classifier", 4, 4, 8, weights)


if __name__ == '__main__':
    output_dir = Path(__file__).parent / 'models'
    output_dir.mkdir(exist_ok=True)
    
    models = {
        'xor.aif32': create_xor_model(),
        'and.aif32': create_and_model(),
        'or.aif32': create_or_model(),
        'classifier.aif32': create_simple_classifier(),
    }
    
    for name, data in models.items():
        path = output_dir / name
        path.write_bytes(data)
        print(f"Created: {path} ({len(data)} bytes)")
    
    print(f"\nAll models saved to {output_dir}/")
