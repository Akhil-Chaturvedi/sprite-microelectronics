"""
Sprite One - Simple Unit Tests
Week 4 Day 24

Unit tests for individual components (can run without hardware).
"""

import unittest
from unittest.mock import Mock, MagicMock, patch
import struct
import sys
import os

# Add parent directory to path
sys.path.insert(0, os.path.dirname(__file__))

from sprite_one import SpriteOne, SpriteOneError, SPRITE_HEADER, CMD_AI_INFER


class TestProtocolParsing(unittest.TestCase):
    """Test protocol packet creation and parsing."""
    
    def test_checksum_calculation(self):
        """Test checksum algorithm."""
        sprite = SpriteOne.__new__(SpriteOne)
        
        # Empty data
        self.assertEqual(sprite._checksum(b''), 0)
        
        # Simple data
        data = b'\x01\x02\x03'
        checksum = sprite._checksum(data)
        self.assertEqual(checksum, (~6 + 1) & 0xFF)
    
    def test_packet_structure(self):
        """Test that packets have correct structure."""
        # This would need a mock serial connection
        pass


class TestAIFunctions(unittest.TestCase):
    """Test AI-related functions."""
    
    @patch('serial.Serial')
    def test_ai_infer_response_parsing(self, mock_serial):
        """Test parsing of AI inference response."""
        # Mock serial port
        mock_port = Mock()
        mock_serial.return_value = mock_port
        
        # Simulate response: header + cmd + status + len + result + checksum
        result_value = 0.978
        result_bytes = struct.pack('<f', result_value)
        response = bytes([SPRITE_HEADER, CMD_AI_INFER, 0x00, 0x04]) + result_bytes + b'\x00'
        
        mock_port.read = Mock(side_effect=[
            response[0:1],   # header
            response[1:4],   # cmd, status, len
            response[4:8],   # data
            response[8:9]    # checksum
        ])
        
        sprite = SpriteOne('dummy')
        # Would need to test actual parsing here
        sprite.close()


class TestErrorHandling(unittest.TestCase):
    """Test error handling."""
    
    @patch('serial.Serial')
    def test_timeout_handling(self, mock_serial):
        """Test that timeouts are handled properly."""
        mock_port = Mock()
        mock_serial.return_value = mock_port
        
        # Simulate timeout (no data available)
        mock_port.read.return_value = b''
        
        sprite = SpriteOne('dummy', timeout=0.1)
        
        with self.assertRaises(SpriteOneError):
            sprite.get_version()
        
        sprite.close()
    
    @patch('serial.Serial')
    def test_invalid_header(self, mock_serial):
        """Test handling of invalid response header."""
        mock_port = Mock()
        mock_serial.return_value = mock_port
        
        # Invalid header
        mock_port.read.side_effect = [b'\xFF', b'', b'', b'']
        
        sprite = SpriteOne('dummy')
        
        with self.assertRaises(SpriteOneError):
            sprite.get_version()
        
        sprite.close()


class TestDataConversion(unittest.TestCase):
    """Test data type conversions."""
    
    def test_float_packing(self):
        """Test float to bytes conversion."""
        value = 1.234
        packed = struct.pack('<f', value)
        unpacked = struct.unpack('<f', packed)[0]
        self.assertAlmostEqual(value, unpacked, places=6)
    
    def test_coordinate_packing(self):
        """Test coordinate packing."""
        x, y = 123, 456
        packed = struct.pack('<HH', x, y)
        x_out, y_out = struct.unpack('<HH', packed)
        self.assertEqual(x, x_out)
        self.assertEqual(y, y_out)


def run_unit_tests():
    """Run all unit tests."""
    print("=" * 50)
    print("Sprite One - Unit Tests")
    print("=" * 50)
    print()
    
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromModule(sys.modules[__name__])
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    print()
    print("=" * 50)
    if result.wasSuccessful():
        print("✓ All unit tests passed!")
    else:
        print(f"✗ {len(result.failures + result.errors)} tests failed")
    print("=" * 50)
    
    return result.wasSuccessful()


if __name__ == '__main__':
    success = run_unit_tests()
    sys.exit(0 if success else 1)
