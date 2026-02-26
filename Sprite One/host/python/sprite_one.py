"""
Sprite One - Python Host Library
v1.2: USB-CDC Transport Support

Simple Python library for controlling Sprite One via serial.
Supports all graphics and AI commands over UART or USB-CDC.

Usage:
    from sprite_one import SpriteOne
    
    # Auto-detect transport
    sprite = SpriteOne('/dev/ttyACM0')  # USB-CDC on Linux
    sprite = SpriteOne('COM3')          # Windows (auto-detect)
    
    # Explicit mode
    sprite = SpriteOne('/dev/ttyUSB0', mode='uart')  # Force UART
    
    # Train AI model
    sprite.ai_train(epochs=100)
    
    # Run inference
    result = sprite.ai_infer(1.0, 0.0)
    print(f"1 XOR 0 = {result}")
    
    # Graphics
    sprite.clear()
    sprite.rect(10, 10, 50, 30)
    sprite.flush()
"""

import serial
import struct
import time
import binascii
from typing import Optional, List, Tuple

# Protocol constants
SPRITE_HEADER = 0xAA
SPRITE_ACK = 0x00
SPRITE_NAK = 0x01

# Command codes
CMD_VERSION = 0x0F
CMD_CLEAR = 0x10
CMD_PIXEL = 0x11
CMD_RECT = 0x12
CMD_TEXT = 0x21
CMD_FLUSH = 0x2F

CMD_AI_INFER = 0x50
CMD_AI_TRAIN = 0x51
CMD_AI_STATUS = 0x52
CMD_AI_SAVE = 0x53
CMD_AI_LOAD = 0x54
CMD_AI_LIST = 0x55
CMD_AI_DELETE = 0x56

# Model management commands
CMD_MODEL_INFO = 0x60
CMD_MODEL_LIST = 0x61
CMD_MODEL_SELECT = 0x62
CMD_MODEL_UPLOAD = 0x63
CMD_MODEL_DELETE = 0x64
CMD_FINETUNE_START = 0x65
CMD_FINETUNE_DATA = 0x66
CMD_FINETUNE_STOP = 0x67

# Industrial API Primitives
CMD_WHO_IS_THERE = 0xA0
CMD_PING_ID = 0xA1
CMD_BUFFER_WRITE = 0xA2
CMD_BUFFER_SNAPSHOT = 0xA3
CMD_BASELINE_CAPTURE = 0xA4
CMD_BASELINE_RESET = 0xA5
CMD_GET_DELTA = 0xA6
CMD_CORRELATE = 0xA7

# Response codes
RESP_OK = 0x00
RESP_ERROR = 0x01
RESP_NOT_FOUND = 0x02
RESP_BUSY = 0x03


class SpriteOneError(Exception):
    """Exception raised for Sprite One communication errors."""
    pass


class SpriteOne:
    """
    Sprite One host library.
    
    Provides high-level interface to Sprite One graphics and AI accelerator.
    Supports both UART and USB-CDC transports.
    """
    
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 2.0, mode: str = 'auto'):
        """
        Initialize connection to Sprite One.
        
        Args:
            port: Serial port (e.g., 'COM3', '/dev/ttyUSB0', '/dev/ttyACM0')
            baudrate: Serial baudrate (default: 115200, ignored for USB-CDC)
            timeout: Read timeout in seconds (default: 2.0)
            mode: Transport mode - 'auto' (detect), 'usb' (USB-CDC), 'uart' (hardware UART)
        """
        self.mode = mode if mode != 'auto' else self._detect_transport(port)
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(0.1)  # Allow device to stabilize
    
    def _detect_transport(self, port: str) -> str:
        """
        Detect if port is USB-CDC or hardware UART.
        
        Args:
            port: Serial port string
            
        Returns:
            'usb' or 'uart'
        """
        port_lower = port.lower()
        # Linux: /dev/ttyACM* is USB-CDC, /dev/ttyUSB* is UART adapter  
        # Windows: USB Serial devices often in device name
        if 'acm' in port_lower or 'cdc' in port_lower:
            return 'usb'
        return 'uart'
        
    def close(self):
        """Close serial connection."""
        if self.ser and self.ser.is_open:
            self.ser.close()
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def _send_command(self, cmd: int, payload: bytes = b'') -> Tuple[int, bytes]:
        """
        Send command and receive response.
        
        Args:
            cmd: Command code
            payload: Command payload bytes
            
        Returns:
            Tuple of (status_code, response_data)
        """
        # Build packet
        packet = bytes([SPRITE_HEADER, cmd, len(payload)]) + payload
        packet += bytes([self._checksum(payload)])
        
        # Send
        self.ser.write(packet)
        
        # Read response header
        header = self.ser.read(4)
        if len(header) < 4:
            raise SpriteOneError("Timeout waiting for response")
        
        if header[0] != SPRITE_HEADER:
            raise SpriteOneError(f"Invalid response header: 0x{header[0]:02X}")
        
        resp_cmd = header[1]
        resp_status = header[2]
        resp_len = header[3]
        
        # Read data
        resp_data = b''
        if resp_len > 0:
            resp_data = self.ser.read(resp_len)
            if len(resp_data) < resp_len:
                raise SpriteOneError("Incomplete response data")
        
        # Read CRC32 trailer (4 bytes, little-endian)
        crc_trailer = self.ser.read(4)
        if len(crc_trailer) < 4:
            raise SpriteOneError("Missing CRC32 trailer in response")
        
        expected_crc = struct.unpack('<I', crc_trailer)[0]
        
        # Calculate CRC over [CMD, STATUS, LEN, DATA]
        actual_crc = binascii.crc32(header[1:4] + resp_data) & 0xFFFFFFFF
        
        if actual_crc != expected_crc:
            raise SpriteOneError(f"CRC mismatch! Expected 0x{expected_crc:08X}, got 0x{actual_crc:08X}")
        
        return resp_status, resp_data
    
    def _checksum(self, data: bytes) -> int:
        """Calculate simple checksum."""
        return (~sum(data) + 1) & 0xFF
    
    def send_bulk(self, data: bytes, chunk_size: Optional[int] = None) -> int:
        """
        Send bulk data (optimized for USB-CDC).
        
        Args:
            data: Data bytes to send
            chunk_size: Optional chunk size (auto-detected based on transport)
            
        Returns:
            Number of bytes sent
        """
        if chunk_size is None:
            # USB-CDC can handle larger chunks
            chunk_size = 4096 if self.mode == 'usb' else 64
        
        total_sent = 0
        for i in range(0, len(data), chunk_size):
            chunk = data[i:i+chunk_size]
            total_sent += self.ser.write(chunk)
        
        return total_sent

    
    # ===== System Commands =====
    
    def get_version(self) -> Tuple[int, int, int]:
        """
        Get firmware version.
        
        Returns:
            Tuple of (major, minor, patch)
        """
        status, data = self._send_command(CMD_VERSION)
        if status != RESP_OK or len(data) < 3:
            raise SpriteOneError(f"Version command failed: status={status}")
        return data[0], data[1], data[2]
    
    # ===== Graphics Commands =====
    
    def clear(self, color: int = 0):
        """Clear display to color."""
        status, _ = self._send_command(CMD_CLEAR, bytes([color]))
        if status != RESP_OK:
            raise SpriteOneError(f"Clear failed: status={status}")
    
    def pixel(self, x: int, y: int, color: int = 1):
        """Draw single pixel."""
        payload = struct.pack('<HHB', x, y, color)
        status, _ = self._send_command(CMD_PIXEL, payload)
        if status != RESP_OK:
            raise SpriteOneError(f"Pixel failed: status={status}")
    
    def rect(self, x: int, y: int, w: int, h: int, color: int = 1):
        """Draw filled rectangle."""
        payload = struct.pack('<HHHHB', x, y, w, h, color)
        status, _ = self._send_command(CMD_RECT, payload)
        if status != RESP_OK:
            raise SpriteOneError(f"Rect failed: status={status}")
    
    def text(self, x: int, y: int, text: str, color: int = 1):
        """Draw text string."""
        text_bytes = text.encode('ascii')
        payload = struct.pack('<HHB', x, y, color) + text_bytes
        status, _ = self._send_command(CMD_TEXT, payload)
        if status != RESP_OK:
            raise SpriteOneError(f"Text failed: status={status}")
    
    def flush(self):
        """Flush framebuffer to display."""
        status, _ = self._send_command(CMD_FLUSH)
        if status != RESP_OK:
            raise SpriteOneError(f"Flush failed: status={status}")
    
    # ===== AI Commands =====
    
    def ai_infer(self, input0: float, input1: float) -> float:
        """
        Run inference on loaded model.
        
        Args:
            input0: First input value
            input1: Second input value
            
        Returns:
            Output value
        """
        payload = struct.pack('<ff', input0, input1)
        status, data = self._send_command(CMD_AI_INFER, payload)
        
        if status == RESP_NOT_FOUND:
            raise SpriteOneError("No model loaded")
        elif status != RESP_OK:
            raise SpriteOneError(f"Inference failed: status={status}")
        
        if len(data) < 4:
            raise SpriteOneError("Invalid inference response")
        
        return struct.unpack('<f', data[:4])[0]
    
    def ai_train(self, epochs: int = 100) -> float:
        """
        Train AI model.
        
        Args:
            epochs: Number of training epochs
            
        Returns:
            Final loss value
        """
        payload = bytes([epochs])
        status, data = self._send_command(CMD_AI_TRAIN, payload)
        
        if status != RESP_OK:
            raise SpriteOneError(f"Training failed: status={status}")
        
        if len(data) >= 4:
            return struct.unpack('<f', data[:4])[0]
        return 0.0
    
    def ai_status(self) -> dict:
        """
        Get AI engine status.
        
        Returns:
            Dictionary with status information
        """
        status, data = self._send_command(CMD_AI_STATUS)
        
        if status != RESP_OK:
            raise SpriteOneError(f"Status command failed: status={status}")
        
        if len(data) < 8:
            return {}
        
        state = data[0]
        # 0=None, 1=Static, 2=Dynamic
        loaded_type = data[1]
        epochs = struct.unpack('<H', data[2:4])[0]
        loss = struct.unpack('<f', data[4:8])[0]
        
        result = {
            'state': state,
            'model_loaded': bool(loaded_type > 0),
            'model_type': 'Dynamic' if loaded_type == 2 else ('Static' if loaded_type == 1 else 'None'),
            'epochs': epochs,
            'last_loss': loss
        }
        
        if len(data) >= 12:
            in_c, out_c = struct.unpack('<HH', data[8:12])
            result['input_dim'] = in_c
            result['output_dim'] = out_c
            
        return result
    
    def ai_save(self, filename: str = "/model.aif32"):
        """Save current model to flash."""
        payload = filename.encode('ascii')
        status, _ = self._send_command(CMD_AI_SAVE, payload)
        
        if status != RESP_OK:
            raise SpriteOneError(f"Save failed: status={status}")
    
    def ai_load(self, filename: str = "/model.aif32"):
        """Load model from flash."""
        payload = filename.encode('ascii')
        status, _ = self._send_command(CMD_AI_LOAD, payload)
        
        if status == RESP_NOT_FOUND:
            raise SpriteOneError(f"Model not found: {filename}")
        elif status != RESP_OK:
            raise SpriteOneError(f"Load failed: status={status}")
    
    def ai_list_models(self) -> List[str]:
        """
        List saved models.
        
        Returns:
            List of model filenames
        """
        status, data = self._send_command(CMD_AI_LIST)
        
        if status != RESP_OK:
            raise SpriteOneError(f"List failed: status={status}")
        
        models = []
        pos = 0
        while pos < len(data):
            name_len = data[pos]
            if name_len == 0:
                break
            pos += 1
            if pos + name_len <= len(data):
                name = data[pos:pos+name_len].decode('ascii')
                models.append(name)
                pos += name_len
            else:
                break
        
        return models
    
    def ai_delete(self, filename: str):
        """Delete a saved model."""
        payload = filename.encode('ascii')
        status, _ = self._send_command(CMD_AI_DELETE, payload)
        
        if status == RESP_NOT_FOUND:
            raise SpriteOneError(f"Model not found: {filename}")
        elif status != RESP_OK:
            raise SpriteOneError(f"Delete failed: status={status}")

    # ===== Model Management (v1.5+) =====
    
    def model_info(self) -> dict:
        """
        Get active model information.
        
        Returns:
            Dict with model header fields, or None if no active model
        """
        status, data = self._send_command(CMD_MODEL_INFO)
        
        if status == RESP_NOT_FOUND:
            return None
        elif status != RESP_OK or len(data) < 32:
            raise SpriteOneError(f"Model info failed: status={status}")
        
        # Parse ModelHeader (32 bytes)
        magic, version, input_size, output_size, hidden_size, model_type, reserved, weights_crc = \
            struct.unpack('<IHBBBBHI', data[:16])
        name = data[16:32].decode('utf-8', errors='ignore').rstrip('\x00')
        
        return {
            'magic': hex(magic),
            'version': version,
            'input_size': input_size,
            'output_size': output_size,
            'hidden_size': hidden_size,
            'model_type': 'F32' if model_type == 0 else 'Q7',
            'weights_crc': hex(weights_crc),
            'name': name
        }
    
    def model_list(self) -> list:
        """
        List all models on device.
        
        Returns:
            List of model filenames
        """
        status, data = self._send_command(CMD_MODEL_LIST)
        
        if status != RESP_OK:
            raise SpriteOneError(f"Model list failed: status={status}")
        
        models = []
        pos = 0
        while pos < len(data):
            name_len = data[pos]
            if name_len == 0:
                break
            pos += 1
            if pos + name_len <= len(data):
                name = data[pos:pos+name_len].decode('ascii', errors='ignore')
                models.append(name)
                pos += name_len
            else:
                break
        
        return models
    
    def model_select(self, filename: str) -> bool:
        """
        Select and activate a model.
        
        Args:
            filename: Model filename to activate
            
        Returns:
            True if successful
        """
        payload = filename.encode('ascii')
        status, _ = self._send_command(CMD_MODEL_SELECT, payload)
        
        if status == RESP_NOT_FOUND:
            raise SpriteOneError(f"Model not found: {filename}")
        elif status != RESP_OK:
            raise SpriteOneError(f"Model select failed: status={status}")
        
        return True
    
    def model_upload(self, filename: str, data: bytes) -> bool:
        """
        Upload a model to device.
        
        Args:
            filename: Destination filename
            data: Model binary data (AIFes .aif32 format)
            
        Returns:
            True if successful
        """
        # Send filename + size first
        header = struct.pack('<H', len(data)) + filename.encode('ascii') + b'\x00'
        status, _ = self._send_command(CMD_MODEL_UPLOAD, header)
        
        if status == RESP_BUSY:
            raise SpriteOneError("Device busy, try again")
        elif status != RESP_OK:
            raise SpriteOneError(f"Upload init failed: status={status}")
        
        # Send data in chunks (max 256 bytes per chunk)
        chunk_size = 256
        for i in range(0, len(data), chunk_size):
            chunk = data[i:i+chunk_size]
            # Use a special continuation packet (same command, chunk offset)
            offset_header = struct.pack('<I', i)
            status, _ = self._send_command(CMD_MODEL_UPLOAD, offset_header + chunk)
            
            if status != RESP_OK:
                raise SpriteOneError(f"Upload chunk failed at offset {i}: status={status}")
        
        return True
    
    def model_delete(self, filename: str) -> bool:
        """
        Delete a model from device.
        
        Args:
            filename: Model filename to delete
            
        Returns:
            True if successful
        """
        payload = filename.encode('ascii')
        status, _ = self._send_command(CMD_MODEL_DELETE, payload)
        
        if status == RESP_NOT_FOUND:
            raise SpriteOneError(f"Model not found: {filename}")
        elif status != RESP_OK:
            raise SpriteOneError(f"Model delete failed: status={status}")
        
        return True
    
    def finetune_start(self, learning_rate: float = 0.01) -> bool:
        """
        Start fine-tuning session on active model.
        
        Args:
            learning_rate: Learning rate for fine-tuning
            
        Returns:
            True if session started
        """
        payload = struct.pack('<f', learning_rate)
        status, _ = self._send_command(CMD_FINETUNE_START, payload)
        
        if status != RESP_OK:
            raise SpriteOneError(f"Finetune start failed: status={status}")
        
        return True
    
    def finetune_data(self, inputs: list, outputs: list) -> float:
        """
        Send training sample for fine-tuning.
        
        Args:
            inputs: List of input floats
            outputs: List of expected output floats
            
        Returns:
            Current loss value
        """
        # Pack inputs and outputs as raw floats (no length prefixes)
        payload = struct.pack(f'<{len(inputs)}f', *inputs)
        payload += struct.pack(f'<{len(outputs)}f', *outputs)
        
        status, data = self._send_command(CMD_FINETUNE_DATA, payload)
        
        if status != RESP_OK:
            raise SpriteOneError(f"Finetune data failed: status={status}")
        
        # Response contains current loss
        if len(data) >= 4:
            loss = struct.unpack('<f', data[:4])[0]
            return loss
        
        return 0.0
    
    def finetune_stop(self, save: bool = True) -> dict:
        """
        Stop fine-tuning session.
        
        Args:
            save: Whether to save the updated model
            
        Returns:
            Dict with final stats (loss, samples trained)
        """
        payload = struct.pack('<B', 1 if save else 0)
        status, data = self._send_command(CMD_FINETUNE_STOP, payload)
        
        if status != RESP_OK:
            raise SpriteOneError(f"Finetune stop failed: status={status}")
        
        result = {'saved': save}
        if len(data) >= 8:
            loss, samples = struct.unpack('<fI', data[:8])
            result['final_loss'] = loss
            result['samples_trained'] = samples
        
        return result

    # ===== Industrial API Primitives (v2.2+) =====
    
    def get_device_id(self) -> bytes:
        """Get unique 8-byte device ID."""
        status, data = self._send_command(CMD_WHO_IS_THERE)
        if status != RESP_OK or len(data) < 8:
            raise SpriteOneError(f"WhoIsThere failed: status={status}")
        return data[:8]
    
    def ping_id(self, device_id: bytes) -> bool:
        """Verify device ID."""
        status, _ = self._send_command(CMD_PING_ID, device_id)
        return status == RESP_OK
    
    def buffer_write(self, value: float):
        """Push float value into circular buffer."""
        payload = struct.pack('<f', value)
        status, _ = self._send_command(CMD_BUFFER_WRITE, payload)
        if status != RESP_OK:
            raise SpriteOneError(f"Buffer write failed: status={status}")
            
    def buffer_snapshot(self) -> List[float]:
        """Get all samples from circular buffer."""
        status, data = self._send_command(CMD_BUFFER_SNAPSHOT)
        if status != RESP_OK:
            raise SpriteOneError(f"Buffer snapshot failed: status={status}")
        
        count = len(data) // 4
        if count == 0: return []
        return list(struct.unpack(f'<{count}f', data[:count*4]))

    def baseline_capture(self) -> float:
        """Capture current buffer mean as baseline."""
        status, data = self._send_command(CMD_BASELINE_CAPTURE)
        if status != RESP_OK or len(data) < 4:
            raise SpriteOneError(f"Baseline capture failed: status={status}")
        return struct.unpack('<f', data[:4])[0]
        
    def baseline_reset(self):
        """Clear baseline."""
        status, _ = self._send_command(CMD_BASELINE_RESET)
        if status != RESP_OK:
            raise SpriteOneError(f"Baseline reset failed: status={status}")
            
    def get_delta(self) -> float:
        """Get |live_mean - baseline|."""
        status, data = self._send_command(CMD_GET_DELTA)
        if status != RESP_OK or len(data) < 4:
            raise SpriteOneError(f"Get delta failed: status={status}")
        return struct.unpack('<f', data[:4])[0]
        
    def correlate(self, ref_data: List[float]) -> float:
        """Get normalized cross-correlation score."""
        count = len(ref_data)
        payload = struct.pack(f'<{count}f', *ref_data)
        status, data = self._send_command(CMD_CORRELATE, payload)
        if status != RESP_OK or len(data) < 4:
            raise SpriteOneError(f"Correlate failed: status={status}")
        return struct.unpack('<f', data[:4])[0]


# Example usage
if __name__ == "__main__":
    # Connect to Sprite One
    with SpriteOne('COM3') as sprite:  # Change to your port
        print("Connected to Sprite One!")
        
        # Get version
        version = sprite.get_version()
        print(f"Firmware: v{version[0]}.{version[1]}.{version[2]}")
        
        # Train model
        print("\nTraining XOR model...")
        loss = sprite.ai_train(epochs=100)
        print(f"Training complete! Final loss: {loss:.6f}")
        
        # Test inference
        print("\nTesting XOR:")
        test_cases = [(0, 0), (0, 1), (1, 0), (1, 1)]
        expected = [0, 1, 1, 0]
        
        for (a, b), exp in zip(test_cases, expected):
            result = sprite.ai_infer(float(a), float(b))
            prediction = 1 if result > 0.5 else 0
            check = "✓" if prediction == exp else "✗"
            print(f"  {a} XOR {b} = {result:.3f} → {prediction} {check}")
        
        # Save model
        sprite.ai_save("/xor.aif32")
        print("\nModel saved to flash!")
        
        # Graphics demo
        print("\nDrawing graphics...")
        sprite.clear()
        sprite.rect(10, 10, 50, 30)
        sprite.text(5, 5, "SPRITE ONE")
        sprite.flush()
        
        print("\nDone!")
