"""
Unit Tests for TFLite to AIFes Converter
Run: python -m pytest test_converter.py -v
Or: python test_converter.py
"""

import struct
import unittest
import numpy as np
from pathlib import Path
import sys
import os

# Add parent to path for imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tflite_to_aif32 import TFLiteParser, AIFesConverter, TFLiteType, TFLiteOp
from create_samples import create_aif32_model, create_xor_model


class TestCRC32(unittest.TestCase):
    """Test CRC32 calculation."""
    
    def test_empty_data(self):
        """CRC32 of empty data should be consistent."""
        converter = AIFesConverter.__new__(AIFesConverter)
        crc = converter._crc32(b'')
        self.assertEqual(crc, 0xFFFFFFFF ^ 0xFFFFFFFF)  # 0x00000000
    
    def test_known_values(self):
        """Test against known CRC32 values."""
        converter = AIFesConverter.__new__(AIFesConverter)
        
        # "123456789" has known CRC32 = 0xCBF43926
        crc = converter._crc32(b'123456789')
        self.assertEqual(crc, 0xCBF43926)
    
    def test_consistency(self):
        """Same data should produce same CRC."""
        converter = AIFesConverter.__new__(AIFesConverter)
        data = b'\x00\x01\x02\x03\x04\x05'
        
        crc1 = converter._crc32(data)
        crc2 = converter._crc32(data)
        
        self.assertEqual(crc1, crc2)
    
    def test_different_data(self):
        """Different data should produce different CRC."""
        converter = AIFesConverter.__new__(AIFesConverter)
        
        crc1 = converter._crc32(b'\x00\x00\x00\x00')
        crc2 = converter._crc32(b'\x00\x00\x00\x01')
        
        self.assertNotEqual(crc1, crc2)


class TestAIFesFormat(unittest.TestCase):
    """Test AIFes .aif32 format generation."""
    
    def test_header_magic(self):
        """Header should start with 'SPRT' magic."""
        model = create_xor_model()
        magic = struct.unpack('<I', model[:4])[0]
        self.assertEqual(magic, 0x54525053)  # "SPRT"
    
    def test_header_version(self):
        """Header should have version 2."""
        model = create_xor_model()
        version = struct.unpack('<H', model[4:6])[0]
        self.assertEqual(version, 0x0002)
    
    def test_header_sizes(self):
        """Header should have correct sizes for XOR model."""
        model = create_xor_model()
        
        input_size = model[6]
        output_size = model[7]
        hidden_size = model[8]
        model_type = model[9]
        
        self.assertEqual(input_size, 2)   # 2 inputs
        self.assertEqual(output_size, 1)  # 1 output
        self.assertEqual(hidden_size, 4)  # 4 hidden neurons
        self.assertEqual(model_type, 0)   # F32
    
    def test_header_name(self):
        """Header should contain model name."""
        model = create_xor_model()
        name = model[16:32].decode('utf-8').rstrip('\x00')
        self.assertEqual(name, 'xor')
    
    def test_header_crc_valid(self):
        """CRC in header should match weights."""
        model = create_xor_model()
        
        header_crc = struct.unpack('<I', model[12:16])[0]
        weights = model[32:]
        
        converter = AIFesConverter.__new__(AIFesConverter)
        calc_crc = converter._crc32(weights)
        
        self.assertEqual(header_crc, calc_crc)
    
    def test_total_size(self):
        """Model size = 32 byte header + weights."""
        weights = np.random.randn(100).astype(np.float32)
        model = create_aif32_model("test", 4, 2, 8, weights)
        
        expected_size = 32 + len(weights) * 4
        self.assertEqual(len(model), expected_size)
    
    def test_weights_float32(self):
        """Weights should be stored as float32."""
        weights = np.array([1.0, -1.0, 0.5, -0.5], dtype=np.float32)
        model = create_aif32_model("test", 2, 2, 2, weights)
        
        extracted = np.frombuffer(model[32:], dtype=np.float32)
        np.testing.assert_array_almost_equal(weights, extracted)


class TestDequantization(unittest.TestCase):
    """Test dequantization of quantized weights."""
    
    def setUp(self):
        # Create a minimal mock parser for testing
        self.converter = AIFesConverter.__new__(AIFesConverter)
        self.converter.parser = None
        self.converter.model = {'subgraphs': [], 'buffers': [], 'operator_codes': []}
    
    def test_float32_passthrough(self):
        """Float32 data should pass through unchanged."""
        data = np.array([1.0, 2.0, 3.0], dtype=np.float32).tobytes()
        result = self.converter._dequantize(data, TFLiteType.FLOAT32, None)
        
        expected = np.array([1.0, 2.0, 3.0], dtype=np.float32)
        np.testing.assert_array_equal(result, expected)
    
    def test_int8_dequantize(self):
        """INT8 data should be dequantized using scale and zero_point."""
        # int8 values: [0, 127, -128]
        data = np.array([0, 127, -128], dtype=np.int8).tobytes()
        quant = {'scale': [0.1], 'zero_point': [0]}
        
        result = self.converter._dequantize(data, TFLiteType.INT8, quant)
        
        expected = np.array([0.0, 12.7, -12.8], dtype=np.float32)
        np.testing.assert_array_almost_equal(result, expected, decimal=5)
    
    def test_uint8_dequantize(self):
        """UINT8 data should be dequantized correctly."""
        data = np.array([0, 128, 255], dtype=np.uint8).tobytes()
        quant = {'scale': [1.0], 'zero_point': [128]}
        
        result = self.converter._dequantize(data, TFLiteType.UINT8, quant)
        
        # (val - 128) * 1.0
        expected = np.array([-128.0, 0.0, 127.0], dtype=np.float32)
        np.testing.assert_array_almost_equal(result, expected, decimal=5)
    
    def test_int8_with_zero_point(self):
        """INT8 with non-zero zero_point."""
        data = np.array([10, 20, 30], dtype=np.int8).tobytes()
        quant = {'scale': [0.5], 'zero_point': [20]}
        
        result = self.converter._dequantize(data, TFLiteType.INT8, quant)
        
        # (val - 20) * 0.5
        expected = np.array([-5.0, 0.0, 5.0], dtype=np.float32)
        np.testing.assert_array_almost_equal(result, expected, decimal=5)
    
    def test_no_quantization(self):
        """INT8 without quant params should convert directly."""
        data = np.array([1, 2, 3], dtype=np.int8).tobytes()
        
        result = self.converter._dequantize(data, TFLiteType.INT8, None)
        
        expected = np.array([1.0, 2.0, 3.0], dtype=np.float32)
        np.testing.assert_array_almost_equal(result, expected)


class TestModelCreation(unittest.TestCase):
    """Test sample model creation."""
    
    def test_xor_model_structure(self):
        """XOR model should have correct structure."""
        model = create_xor_model()
        
        # Should be 32 header + weights
        # Layer 1: 2*4 + 4 = 12 floats
        # Layer 2: 4*1 + 1 = 5 floats
        # Total: 17 floats = 68 bytes
        expected_size = 32 + 17 * 4
        self.assertEqual(len(model), expected_size)
    
    def test_model_name_truncation(self):
        """Long names should be truncated to 15 chars."""
        weights = np.array([1.0], dtype=np.float32)
        long_name = "this_is_a_very_long_model_name"
        
        model = create_aif32_model(long_name, 1, 1, 1, weights)
        name = model[16:32].decode('utf-8').rstrip('\x00')
        
        self.assertEqual(len(name), 15)
        self.assertEqual(name, long_name[:15])
    
    def test_empty_weights(self):
        """Model with no weights."""
        weights = np.array([], dtype=np.float32)
        model = create_aif32_model("empty", 1, 1, 1, weights)
        
        self.assertEqual(len(model), 32)  # Just header


class TestRoundTrip(unittest.TestCase):
    """Test that we can read back what we write."""
    
    def test_parse_created_model(self):
        """Created model should parse correctly."""
        from io import BytesIO
        
        # Create a model
        weights = np.random.randn(50).astype(np.float32)
        model = create_aif32_model("roundtrip", 8, 4, 16, weights)
        
        # Parse header
        magic = struct.unpack('<I', model[:4])[0]
        version = struct.unpack('<H', model[4:6])[0]
        input_size = model[6]
        output_size = model[7]
        hidden_size = model[8]
        crc = struct.unpack('<I', model[12:16])[0]
        name = model[16:32].decode('utf-8').rstrip('\x00')
        
        self.assertEqual(magic, 0x54525053)
        self.assertEqual(version, 2)
        self.assertEqual(input_size, 8)
        self.assertEqual(output_size, 4)
        self.assertEqual(hidden_size, 16)
        self.assertEqual(name, "roundtrip")
        
        # Verify CRC
        converter = AIFesConverter.__new__(AIFesConverter)
        calc_crc = converter._crc32(model[32:])
        self.assertEqual(crc, calc_crc)
        
        # Verify weights
        extracted = np.frombuffer(model[32:], dtype=np.float32)
        np.testing.assert_array_almost_equal(weights, extracted)


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and error handling."""
    
    def test_max_sizes(self):
        """Max values for sizes (255)."""
        weights = np.array([1.0], dtype=np.float32)
        model = create_aif32_model("max", 255, 255, 255, weights)
        
        self.assertEqual(model[6], 255)
        self.assertEqual(model[7], 255)
        self.assertEqual(model[8], 255)
    
    def test_unicode_name(self):
        """Unicode in name should not crash."""
        weights = np.array([1.0], dtype=np.float32)
        # This might truncate or fail gracefully
        try:
            model = create_aif32_model("模型", 1, 1, 1, weights)
            # Should at least have correct header size
            self.assertEqual(len(model), 32 + 4)
        except UnicodeEncodeError:
            pass  # Acceptable to reject non-ASCII
    
    def test_large_weights(self):
        """Large weight arrays."""
        weights = np.random.randn(10000).astype(np.float32)
        model = create_aif32_model("large", 100, 100, 100, weights)
        
        self.assertEqual(len(model), 32 + 10000 * 4)
    
    def test_special_float_values(self):
        """Special float values (inf, nan, zero)."""
        weights = np.array([0.0, -0.0, np.inf, -np.inf, np.nan], dtype=np.float32)
        model = create_aif32_model("special", 5, 1, 1, weights)
        
        extracted = np.frombuffer(model[32:], dtype=np.float32)
        
        self.assertEqual(extracted[0], 0.0)
        self.assertEqual(extracted[1], 0.0)  # -0.0 == 0.0
        self.assertEqual(extracted[2], np.inf)
        self.assertEqual(extracted[3], -np.inf)
        self.assertTrue(np.isnan(extracted[4]))


class TestTFLiteOpCodes(unittest.TestCase):
    """Test TFLite operation code mappings."""
    
    def test_op_codes_defined(self):
        """Common op codes should be defined."""
        self.assertEqual(TFLiteOp.CONV_2D, 3)
        self.assertEqual(TFLiteOp.FULLY_CONNECTED, 9)
        self.assertEqual(TFLiteOp.RELU, 19)
        self.assertEqual(TFLiteOp.SOFTMAX, 25)
    
    def test_tensor_types_defined(self):
        """Common tensor types should be defined."""
        self.assertEqual(TFLiteType.FLOAT32, 0)
        self.assertEqual(TFLiteType.INT8, 9)
        self.assertEqual(TFLiteType.UINT8, 3)


class TestFileOperations(unittest.TestCase):
    """Test file I/O operations."""
    
    def test_write_and_read_model(self):
        """Model should survive write/read cycle."""
        import tempfile
        
        weights = np.random.randn(20).astype(np.float32)
        model = create_aif32_model("filetest", 4, 2, 8, weights)
        
        with tempfile.NamedTemporaryFile(suffix='.aif32', delete=False) as f:
            f.write(model)
            temp_path = f.name
        
        try:
            with open(temp_path, 'rb') as f:
                loaded = f.read()
            
            self.assertEqual(model, loaded)
        finally:
            os.unlink(temp_path)


if __name__ == '__main__':
    # Run with verbose output
    unittest.main(verbosity=2)
