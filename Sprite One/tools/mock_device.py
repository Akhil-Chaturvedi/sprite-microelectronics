"""
Sprite One Mock Device Server

Creates a virtual serial port that simulates a real Sprite One device.
Responds to all protocol commands with realistic data.

Usage:
    Windows (requires com0com or similar virtual COM port driver):
        python mock_device.py COM10

    Without virtual ports (loopback test):
        python mock_device.py --loopback

    The mock device will respond to commands sent to the specified port.
"""

import struct
import time
import sys
import os
import threading

# Protocol constants
HEADER = 0xAA
RESP_OK = 0x00
RESP_ERROR = 0x01
RESP_NOT_FOUND = 0x02

# Commands
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
CMD_MODEL_INFO = 0x60
CMD_MODEL_LIST = 0x61
CMD_MODEL_SELECT = 0x62
CMD_MODEL_UPLOAD = 0x63
CMD_MODEL_DELETE = 0x64
CMD_FINETUNE_START = 0x65
CMD_FINETUNE_START = 0x65
CMD_FINETUNE_DATA = 0x66
CMD_FINETUNE_STOP = 0x67
CMD_BATCH = 0x70

# Sentinel "God Mode" Commands
CMD_SENTINEL_STATUS = 0x80 # Returns Temp, Freq, MemCount
CMD_SENTINEL_TRIGGER = 0x81 # Trigger a simulated reflex
CMD_SENTINEL_LEARN = 0x82 # Force learn current "view"

class MockDevice:
    """Simulates a Sprite One device."""

    def __init__(self):
        self.version = (2, 1, 0)  # Updated version
        self.framebuffer = bytearray(128 * 64 // 8)
        self.models = {
            'xor.aif32': {'input': 2, 'output': 1, 'hidden': 8, 'size': 192},
            'and.aif32': {'input': 2, 'output': 1, 'hidden': 8, 'size': 192},
        }
        self.active_model = 'xor.aif32'
        self.model_loaded = True
        self.epochs_trained = 0
        self.last_loss = 0.5
        
        # Sentinel State
        self.temp_c = 35.0
        self.freq_mhz = 133
        self.vector_mem_count = 1
        self.reflex_active = False

        self.command_handlers = {
            CMD_VERSION: self._cmd_version,
            CMD_CLEAR: self._cmd_clear,
            CMD_PIXEL: self._cmd_pixel,
            CMD_RECT: self._cmd_rect,
            CMD_TEXT: self._cmd_text,
            CMD_FLUSH: self._cmd_flush,
            CMD_AI_INFER: self._cmd_ai_infer,
            CMD_AI_TRAIN: self._cmd_ai_train,
            CMD_AI_STATUS: self._cmd_ai_status,
            CMD_AI_SAVE: self._cmd_ai_save,
            CMD_AI_LOAD: self._cmd_ai_load,
            CMD_AI_LIST: self._cmd_ai_list,
            CMD_AI_DELETE: self._cmd_ai_delete,
            CMD_MODEL_INFO: self._cmd_model_info,
            CMD_MODEL_LIST: self._cmd_ai_list,  # Same format
            CMD_MODEL_SELECT: self._cmd_model_select,
            CMD_MODEL_UPLOAD: self._cmd_model_upload,
            CMD_MODEL_DELETE: self._cmd_ai_delete,
            CMD_FINETUNE_START: self._cmd_ack,
            CMD_FINETUNE_DATA: self._cmd_ack,
            CMD_FINETUNE_STOP: self._cmd_ack,
            CMD_BATCH: self._cmd_batch,
            # Sentinel Handlers
            CMD_SENTINEL_STATUS: self._cmd_sentinel_status,
            CMD_SENTINEL_TRIGGER: self._cmd_sentinel_trigger,
            CMD_SENTINEL_LEARN: self._cmd_sentinel_learn,
        }

    def process_packet(self, data: bytes) -> bytes:
        """Process incoming packet, return response."""
        if len(data) < 3 or data[0] != HEADER:
            return self._make_response(RESP_ERROR)

        cmd = data[1]
        length = data[2]
        payload = data[3:3 + length] if length > 0 else b''

        handler = self.command_handlers.get(cmd)
        if handler:
            return handler(payload)
        else:
            print(f"  Unknown command: 0x{cmd:02X}")
            return self._make_response(RESP_ERROR)

    # --- Command handlers ---

    def _cmd_version(self, payload):
        return self._make_response(RESP_OK, bytes(self.version))

    def _cmd_clear(self, payload):
        color = payload[0] if payload else 0
        self.framebuffer = bytearray([0xFF if color else 0x00] * len(self.framebuffer))
        return self._make_response(RESP_OK)

    def _cmd_pixel(self, payload):
        if len(payload) >= 3:
            x, y, color = payload[0], payload[1], payload[2]
            if 0 <= x < 128 and 0 <= y < 64:
                byte_idx = x + (y // 8) * 128
                bit = y % 8
                if color:
                    self.framebuffer[byte_idx] |= (1 << bit)
                else:
                    self.framebuffer[byte_idx] &= ~(1 << bit)
        return self._make_response(RESP_OK)

    def _cmd_rect(self, payload):
        if len(payload) >= 5:
            rx, ry, rw, rh, color = payload[0], payload[1], payload[2], payload[3], payload[4]
            for dy in range(rh):
                for dx in range(rw):
                    x, y = rx + dx, ry + dy
                    if 0 <= x < 128 and 0 <= y < 64:
                        byte_idx = x + (y // 8) * 128
                        bit = y % 8
                        if color:
                            self.framebuffer[byte_idx] |= (1 << bit)
                        else:
                            self.framebuffer[byte_idx] &= ~(1 << bit)
        return self._make_response(RESP_OK)

    def _cmd_text(self, payload):
        return self._make_response(RESP_OK)

    def _cmd_flush(self, payload):
        # Count set pixels
        count = sum(bin(b).count('1') for b in self.framebuffer)
        print(f"  Display: {count} pixels lit")
        return self._make_response(RESP_OK)

    def _cmd_ai_infer(self, payload):
        if len(payload) >= 8:
            in0 = struct.unpack('<f', payload[0:4])[0]
            in1 = struct.unpack('<f', payload[4:8])[0]
            # Simulate XOR
            expected = 1.0 if (in0 > 0.5) != (in1 > 0.5) else 0.0
            import random
            result = max(0, min(1, expected + random.uniform(-0.02, 0.02)))
            return self._make_response(RESP_OK, struct.pack('<f', result))
        return self._make_response(RESP_ERROR)

    def _cmd_ai_train(self, payload):
        epochs = payload[0] if payload else 100
        self.epochs_trained += epochs
        self.last_loss = max(0.001, self.last_loss * 0.5)
        self.model_loaded = True
        print(f"  Trained {epochs} epochs, loss={self.last_loss:.4f}")
        # Simulate training time
        time.sleep(min(epochs * 0.03, 3.0))
        return self._make_response(RESP_OK, struct.pack('<f', self.last_loss))

    def _cmd_ai_status(self, payload):
        data = struct.pack('<BBHf',
                           0,  # state
                           1 if self.model_loaded else 0,
                           self.epochs_trained,
                           self.last_loss)
        return self._make_response(RESP_OK, data)

    def _cmd_ai_save(self, payload):
        name = payload.decode('ascii', errors='ignore').strip('\x00')
        if not name:
            name = 'model.aif32'
        self.models[name] = {'input': 2, 'output': 1, 'hidden': 8, 'size': 192}
        print(f"  Saved: {name}")
        return self._make_response(RESP_OK)

    def _cmd_ai_load(self, payload):
        name = payload.decode('ascii', errors='ignore').strip('\x00')
        if name in self.models:
            self.active_model = name
            self.model_loaded = True
            print(f"  Loaded: {name}")
            return self._make_response(RESP_OK)
        return self._make_response(RESP_NOT_FOUND)

    def _cmd_ai_list(self, payload):
        data = bytearray()
        for name in self.models:
            data.append(len(name))
            data.extend(name.encode('ascii'))
        data.append(0)  # Terminator
        return self._make_response(RESP_OK, bytes(data))

    def _cmd_ai_delete(self, payload):
        name = payload.decode('ascii', errors='ignore').strip('\x00')
        if name in self.models:
            del self.models[name]
            print(f"  Deleted: {name}")
            return self._make_response(RESP_OK)
        return self._make_response(RESP_NOT_FOUND)

    def _cmd_model_info(self, payload):
        if not self.active_model or self.active_model not in self.models:
            return self._make_response(RESP_NOT_FOUND)
        m = self.models[self.active_model]
        data = bytearray(32)
        struct.pack_into('<I', data, 0, 0x54525053)  # SPRT
        data[6] = m.get('input', 2)
        data[7] = m.get('output', 1)
        data[8] = m.get('hidden', 8)
        name_bytes = self.active_model.encode('ascii')[:16]
        data[16:16 + len(name_bytes)] = name_bytes
        return self._make_response(RESP_OK, bytes(data))

    def _cmd_model_select(self, payload):
        name = payload.decode('ascii', errors='ignore').strip('\x00')
        if name in self.models:
            self.active_model = name
            print(f"  Selected: {name}")
            return self._make_response(RESP_OK)
        return self._make_response(RESP_NOT_FOUND)

    def _cmd_model_upload(self, payload):
        if len(payload) < 2:
            return self._make_response(RESP_ERROR)
        name_len = payload[0]
        name = payload[1:1 + name_len].decode('ascii', errors='ignore')
        model_data = payload[1 + name_len:]
        self.models[name] = {
            'input': model_data[6] if len(model_data) > 6 else 2,
            'output': model_data[7] if len(model_data) > 7 else 1,
            'hidden': model_data[8] if len(model_data) > 8 else 8,
            'size': len(model_data),
        }
        print(f"  Uploaded: {name} ({len(model_data)} bytes)")
        return self._make_response(RESP_OK)

    # Sentinel Command Implementation
    def _cmd_sentinel_status(self, payload):
        # Simulate dynamic physics
        if self.freq_mhz > 200:
            self.temp_c += 0.5 # Heat up
        else:
            self.temp_c = max(25.0, self.temp_c - 0.2) # Cool down
            
        # Mock Thermal Throttling Logic
        if self.temp_c > 65.0:
            print("  [SENTINEL] Overheating! Throttling...")
            self.freq_mhz = 133
        
        data = struct.pack('<fII', self.temp_c, self.freq_mhz, self.vector_mem_count)
        return self._make_response(RESP_OK, data)

    def _cmd_sentinel_trigger(self, payload):
        self.reflex_active = True
        self.freq_mhz = 250 # Overclock on reflex
        print("  [SENTINEL] Reflex Triggered! Overclocking to 250MHz")
        return self._make_response(RESP_OK)

    def _cmd_sentinel_learn(self, payload):
        self.vector_mem_count += 1
        print(f"  [SENTINEL] Learned new vector. Total memories: {self.vector_mem_count}")
        return self._make_response(RESP_OK)

    def _cmd_ack(self, payload):
        return self._make_response(RESP_OK)

    def _cmd_batch(self, payload):
        """Process a batch of commands."""
        p = 0
        responses = bytearray()
        
        while p < len(payload):
            if p + 2 > len(payload): break
            cmd = payload[p]
            length = payload[p+1]
            p += 2
            
            if p + length > len(payload): break
            sub_payload = payload[p:p+length]
            p += length
            
            # Recursive execution
            handler = self.command_handlers.get(cmd)
            if handler:
                resp = handler(sub_payload)
                # In real firmware, responses are sent individually.
                # Here we just execute. For the mock loopback test, 
                # we might want to collect them, but the protocol 
                # says the batch command itself returns a final OK.
                # The sub-commands would send their own responses 
                # out the serial port in real life.
                # For this mock, we'll just return the final OK 
                # to satisfy the test harness.
                pass
                
        return self._make_response(RESP_OK)

    # --- Response construction ---

    def _make_response(self, status, data=b''):
        resp = bytearray([HEADER, status, len(data)])
        resp.extend(data)
        # Checksum (XOR)
        checksum = 0
        for b in resp[1:]:
            checksum ^= b
        resp.append(checksum)
        return bytes(resp)


def run_serial_server(port, baud=115200):
    """Run mock device on a serial port."""
    try:
        import serial
    except ImportError:
        print("Error: pyserial required. Install with: pip install pyserial")
        sys.exit(1)

    device = MockDevice()
    print(f"Mock Sprite One v{'.'.join(map(str, device.version))}")
    print(f"Listening on {port} at {baud} baud")
    print(f"Models: {list(device.models.keys())}")
    print("Press Ctrl+C to stop\n")

    try:
        ser = serial.Serial(port, baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"Error opening {port}: {e}")
        print("\nTo use mock device, you need a virtual serial port pair.")
        print("Windows: Install com0com (https://com0com.sourceforge.net/)")
        print("Linux: socat -d -d pty,raw,echo=0 pty,raw,echo=0")
        sys.exit(1)

    buffer = bytearray()

    try:
        while True:
            data = ser.read(64)
            if data:
                buffer.extend(data)

                # Process complete packets
                while len(buffer) >= 3 and buffer[0] == HEADER:
                    cmd = buffer[1]
                    length = buffer[2]
                    packet_len = 3 + length + 1  # header+cmd+len + payload + checksum

                    if len(buffer) < packet_len:
                        break  # Wait for more data

                    packet = bytes(buffer[:packet_len])
                    buffer = buffer[packet_len:]

                    cmd_name = {
                        0x0F: 'VERSION', 0x10: 'CLEAR', 0x11: 'PIXEL',
                        0x12: 'RECT', 0x21: 'TEXT', 0x2F: 'FLUSH',
                        0x50: 'INFER', 0x51: 'TRAIN', 0x52: 'STATUS',
                        0x53: 'SAVE', 0x54: 'LOAD', 0x55: 'LIST',
                        0x56: 'DELETE', 0x60: 'MODEL_INFO',
                        0x61: 'MODEL_LIST', 0x62: 'MODEL_SELECT',
                        0x63: 'MODEL_UPLOAD', 0x64: 'MODEL_DELETE',
                    }.get(cmd, f'0x{cmd:02X}')

                    print(f"[RX] {cmd_name} ({length}B payload)")
                    response = device.process_packet(packet)
                    ser.write(response)
                    print(f"[TX] {len(response)}B response")

                # Discard garbage
                if buffer and buffer[0] != HEADER:
                    buffer.clear()

    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()


def run_loopback_test():
    """Run a self-test without serial ports."""
    device = MockDevice()
    print("=== Mock Device Loopback Test ===\n")

    tests = [
        ("VERSION", bytes([HEADER, CMD_VERSION, 0, 0])),
        ("CLEAR", bytes([HEADER, CMD_CLEAR, 1, 0, 0])),
        ("RECT", bytes([HEADER, CMD_RECT, 5, 10, 10, 20, 15, 1, 0])),
        ("FLUSH", bytes([HEADER, CMD_FLUSH, 0, 0])),
        ("AI_TRAIN", bytes([HEADER, CMD_AI_TRAIN, 1, 50, 0])),
        ("AI_INFER", bytes([HEADER, CMD_AI_INFER, 8]) +
         struct.pack('<ff', 1.0, 0.0) + bytes([0])),
        ("AI_STATUS", bytes([HEADER, CMD_AI_STATUS, 0, 0])),
        ("AI_LIST", bytes([HEADER, CMD_AI_LIST, 0, 0])),
        ("AI_SAVE", bytes([HEADER, CMD_AI_SAVE, 8]) +
         b'test.ai\x00' + bytes([0])),
        ("MODEL_INFO", bytes([HEADER, CMD_MODEL_INFO, 0, 0])),
        ("BATCH_TEST", bytes([HEADER, CMD_BATCH, 8]) + 
         # Sub-command 1: CLEAR (1 byte payload)
         bytes([CMD_CLEAR, 1, 0]) +
         # Sub-command 2: RECT (5 byte payload) 
         bytes([CMD_RECT, 5, 0, 0, 10, 10, 1])),
        
        # Sentinel Legacy Tests
        ("SENTINEL_TRIG", bytes([HEADER, CMD_SENTINEL_TRIGGER, 0, 0])),
        ("SENTINEL_LRN",  bytes([HEADER, CMD_SENTINEL_LEARN, 0, 0])),
        ("SENTINEL_STAT", bytes([HEADER, CMD_SENTINEL_STATUS, 0, 0])),
    ]

    passed = 0
    for name, packet in tests:
        try:
            response = device.process_packet(packet)
            status = response[1] if len(response) > 1 else -1
            status_str = {0: 'OK', 1: 'ERROR', 2: 'NOT_FOUND'}.get(status, f'0x{status:02X}')
            data_len = response[2] if len(response) > 2 else 0

            if status == RESP_OK:
                print(f"  ✓ {name:15s} → {status_str} ({data_len}B data)")
                passed += 1
            else:
                print(f"  ✗ {name:15s} → {status_str}")
        except Exception as e:
            print(f"  ✗ {name:15s} → ERROR: {e}")

    print(f"\n{passed}/{len(tests)} tests passed")
    print(f"Models: {list(device.models.keys())}")


if __name__ == '__main__':
    if len(sys.argv) < 2 or sys.argv[1] == '--loopback':
        run_loopback_test()
    else:
        run_serial_server(sys.argv[1])
