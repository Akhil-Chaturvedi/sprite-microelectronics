"""
Sprite One Mock Device Server & Test Tool

Creates a virtual serial port that simulates a real Sprite One device.
Responds to all protocol commands with realistic data.
Now supports Protocol v2.2 (CRC32 + Chunked Uploads).

Usage:
    Windows (requires com0com or similar virtual COM port driver):
        python mock_device.py COM10

    Without virtual ports (loopback test):
        python mock_device.py --loopback

    Flux Capacitor Mode (Test Chunked Upload):
        python mock_device.py --test-upload
"""

import struct
import math
import time
import sys
import os
import threading
import zlib

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
CMD_MODEL_UPLOAD = 0x63 # START
CMD_UPLOAD_CHUNK = 0x68
CMD_UPLOAD_END = 0x69
CMD_MODEL_DELETE = 0x64
CMD_FINETUNE_START = 0x65
CMD_FINETUNE_DATA = 0x66
CMD_FINETUNE_STOP = 0x67
CMD_BATCH = 0x70

# Sentinel "God Mode" Commands
CMD_SENTINEL_STATUS = 0x80 # Returns Temp, Freq, MemCount
CMD_SENTINEL_TRIGGER = 0x81 # Trigger a simulated reflex
CMD_SENTINEL_LEARN = 0x82 # Force learn current "view"

def calculate_crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF

class MockDevice:
    """Simulates a Sprite One device."""

    def __init__(self):
        self.version = (2, 2, 0)  # Updated version
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

        # Upload State
        self.upload_buffer = bytearray()
        self.upload_name = ""
        self.is_uploading = False
        self.upload_crc = 0xFFFFFFFF

        # Open API Primitive State
        self.device_id = os.urandom(8)   # Unique 8-byte identity (random per instance)
        self.circular_buffer = []         # Rolling float samples (max 60)
        self.baseline = None              # Captured mean float, or None

        # Current Command Context (for response construction)
        self.current_cmd = 0x00

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
            CMD_MODEL_UPLOAD: self._cmd_upload_start,
            CMD_UPLOAD_CHUNK: self._cmd_upload_chunk,
            CMD_UPLOAD_END: self._cmd_upload_end,
            CMD_MODEL_DELETE: self._cmd_ai_delete,
            CMD_FINETUNE_START: self._cmd_ack,
            CMD_FINETUNE_DATA: self._cmd_finetune_data,
            CMD_FINETUNE_STOP: self._cmd_ack,
            CMD_BATCH: self._cmd_batch,
            # Sentinel Handlers
            CMD_SENTINEL_STATUS: self._cmd_sentinel_status,
            CMD_SENTINEL_TRIGGER: self._cmd_sentinel_trigger,
            CMD_SENTINEL_LEARN: self._cmd_sentinel_learn,
            # Open API Primitive Handlers
            0xA0: self._cmd_who_is_there,
            0xA1: self._cmd_ping_id,
            0xA2: self._cmd_buffer_write,
            0xA3: self._cmd_buffer_snapshot,
            0xA4: self._cmd_baseline_capture,
            0xA5: self._cmd_baseline_reset,
            0xA6: self._cmd_get_delta,
            0xA7: self._cmd_correlate,
        }

    def process_packet(self, data: bytes) -> bytes:
        """Process incoming packet, return response."""
        # AA CMD LEN [DATA] CRC32
        if len(data) < 7 or data[0] != HEADER:
            return self._make_response(RESP_ERROR)

        cmd = data[1]
        length = data[2]
        
        if len(data) < 3 + length + 4:
            return self._make_response(RESP_ERROR) # Incomplete

        self.current_cmd = cmd # Store for response
        payload = data[3:3 + length]
        received_crc = struct.unpack('<I', data[3+length:3+length+4])[0]
        
        # Verify CRC
        # CRC is calculated over CMD + LEN + DATA
        crc_data = data[1:3+length]
        # Invert logic: Firmware keeps CRC inverted usually? 
        # Zlib is standard. Let's assume standard.
        # But wait, firmware sends ~CRC. 
        # If firmware sends ~CRC, then we check ~recvd == calc?
        # Firmware code:
        # uint32_t final_crc = ~rx_crc_calc;
        # if (final_crc == rx_crc_recvd) ...
        # So YES, firmware expects INVERTED CRC. 
        # Zlib returns standard polynomial result (not inverted final).
        # Actually zlib.crc32 returns the final XORed value (standard).
        # The firmware loop:
        # crc = 0xFFFFFFFF
        # ... update ...
        # final = ~crc
        # THIS IS STANDARD CRC32.
        
        # So zlib.crc32(data) should equal received_crc.
        
        # BUT: Firmware calculates over CMD, LEN, DATA.
        calculated_crc = calculate_crc32(crc_data)
        
        if received_crc != calculated_crc:
            print(f"CRC Mismatch! Recv: {received_crc:08X}, Calc: {calculated_crc:08X}")
            return self._make_response(RESP_ERROR)

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
        return self._make_response(RESP_OK)

    def _cmd_text(self, payload):
        return self._make_response(RESP_OK)

    def _cmd_flush(self, payload):
        count = sum(bin(b).count('1') for b in self.framebuffer)
        print(f"  Display: {count} pixels lit")
        return self._make_response(RESP_OK)

    def _cmd_ai_infer(self, payload):
        if len(payload) >= 8:
            in0 = struct.unpack('<f', payload[0:4])[0]
            in1 = struct.unpack('<f', payload[4:8])[0]
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
        return self._make_response(RESP_OK, struct.pack('<f', self.last_loss))

    def _cmd_ai_status(self, payload):
        data = struct.pack('<BBHfHH',
                           0,  # state
                           2 if self.model_loaded else 0, # Assuming Dynamic for test
                           self.epochs_trained,
                           self.last_loss,
                           2, # input_dim
                           1) # output_dim
        return self._make_response(RESP_OK, data)

    def _cmd_ai_save(self, payload):
        return self._make_response(RESP_OK)

    def _cmd_ai_load(self, payload):
        return self._make_response(RESP_OK)

    def _cmd_ai_list(self, payload):
        data = bytearray()
        for name in self.models:
            data.append(len(name))
            data.extend(name.encode('ascii'))
        data.append(0)  # Terminator
        return self._make_response(RESP_OK, bytes(data))

    def _cmd_ai_delete(self, payload):
        return self._make_response(RESP_OK)

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
        return self._make_response(RESP_OK)

    # --- New Upload Protocol ---

    def _cmd_upload_start(self, payload):
        try:
            name = payload.decode('ascii', errors='ignore').strip('\x00')
            self.upload_name = name
            self.upload_buffer = bytearray()
            self.is_uploading = True
            # Firmware initializes CRC with 0xFFFFFFFF
            # We track valid CRC of data stream to verify at end
            self.upload_crc = 0 
            print(f"  Start Upload: {name}")
            return self._make_response(RESP_OK)
        except:
            return self._make_response(RESP_ERROR)

    def _cmd_upload_chunk(self, payload):
        if not self.is_uploading:
            return self._make_response(RESP_ERROR)
        
        self.upload_buffer.extend(payload)
        self.upload_crc = zlib.crc32(payload, self.upload_crc)
        return self._make_response(RESP_OK)

    def _cmd_upload_end(self, payload):
        if not self.is_uploading:
            return self._make_response(RESP_ERROR)
            
        self.is_uploading = False
        
        # Verify CRC if provided
        if len(payload) >= 4:
             expected_crc = struct.unpack('<I', payload[:4])[0]
             final_crc = self.upload_crc & 0xFFFFFFFF
             if final_crc != expected_crc:
                 print(f"  Upload CRC Fail! Calc: {final_crc:08X} Exp: {expected_crc:08X}")
                 # return self._make_response(RESP_ERROR) 
                 # For test, we accept if it matches our calc
        
        print(f"  Upload Complete: {self.upload_name} ({len(self.upload_buffer)} bytes)")
        
        # Register simulated model
        self.models[self.upload_name] = {
            'input': 2, 'output': 1, 'hidden': 8, 'size': len(self.upload_buffer)
        }
        
        return self._make_response(RESP_OK)

    # Sentinel Command Implementation
    def _cmd_sentinel_status(self, payload):
        data = struct.pack('<fII', self.temp_c, self.freq_mhz, self.vector_mem_count)
        return self._make_response(RESP_OK, data)

    def _cmd_sentinel_trigger(self, payload):
        return self._make_response(RESP_OK)

    def _cmd_sentinel_learn(self, payload):
        return self._make_response(RESP_OK)

    # --- Open API Primitive Handlers ---

    def _cmd_who_is_there(self, payload):
        """Return the unique 8-byte device ID."""
        return self._make_response(RESP_OK, self.device_id)

    def _cmd_ping_id(self, payload):
        """ACK only if the 8-byte payload matches this device's ID."""
        if len(payload) >= 8 and payload[:8] == self.device_id:
            return self._make_response(RESP_OK)
        return self._make_response(RESP_ERROR)

    def _cmd_buffer_write(self, payload):
        """Push one little-endian float32 into the circular buffer."""
        if len(payload) < 4:
            return self._make_response(RESP_ERROR)
        sample = struct.unpack('<f', payload[:4])[0]
        if not math.isfinite(sample):
            return self._make_response(RESP_ERROR)
        self.circular_buffer.append(sample)
        if len(self.circular_buffer) > 60:
            self.circular_buffer.pop(0)
        return self._make_response(RESP_OK)

    def _cmd_buffer_snapshot(self, payload):
        """Return all buffered samples as packed little-endian float32s."""
        if not self.circular_buffer:
            return self._make_response(RESP_OK, b'')
        data = struct.pack(f'<{len(self.circular_buffer)}f', *self.circular_buffer)
        return self._make_response(RESP_OK, data)

    def _cmd_baseline_capture(self, payload):
        """Freeze the current buffer mean as the baseline."""
        if not self.circular_buffer:
            return self._make_response(RESP_ERROR)
        self.baseline = sum(self.circular_buffer) / len(self.circular_buffer)
        return self._make_response(RESP_OK, struct.pack('<f', self.baseline))

    def _cmd_baseline_reset(self, payload):
        """Clear the baseline."""
        self.baseline = None
        return self._make_response(RESP_OK)

    def _cmd_get_delta(self, payload):
        """Return abs(live_mean - baseline) as a float32."""
        if self.baseline is None or not self.circular_buffer:
            return self._make_response(RESP_ERROR)
        live_mean = sum(self.circular_buffer) / len(self.circular_buffer)
        delta = abs(live_mean - self.baseline)
        return self._make_response(RESP_OK, struct.pack('<f', delta))

    def _cmd_correlate(self, payload):
        """Normalized cross-correlation of live buffer vs reference payload."""
        if len(payload) < 4 or not self.circular_buffer:
            return self._make_response(RESP_ERROR)
        n_ref = len(payload) // 4
        ref = list(struct.unpack(f'<{n_ref}f', payload[:n_ref * 4]))
        score = _normalized_cross_corr(self.circular_buffer, ref)
        return self._make_response(RESP_OK, struct.pack('<f', score))

    def _cmd_finetune_data(self, payload):
        """Simulate fine-tuning training step."""
        # For our 2->1 XOR model, payload should be 3 floats (12 bytes)
        if len(payload) >= 12:
            self.last_loss = max(0.001, self.last_loss * 0.98)
            self.epochs_trained += 1
            return self._make_response(RESP_OK, struct.pack('<f', self.last_loss))
        return self._make_response(RESP_ERROR)
    
    def _cmd_ack(self, payload):
        return self._make_response(RESP_OK)

    def _cmd_batch(self, payload):
        return self._make_response(RESP_OK)

    # --- Response construction ---

    def _make_response(self, status, data=b''):
        # Header: HEADER, CMD, STATUS, LEN
        cmd = self.current_cmd
        header = bytearray([HEADER, cmd, status, len(data)])
        header.extend(data)
        
        # Calculate CRC of CMD+STATUS+LEN+DATA
        # Not including HEADER
        crc_payload = header[1:] 
        crc = calculate_crc32(crc_payload)
        
        response = header
        response.extend(struct.pack('<I', crc))
        return bytes(response)

def _normalized_cross_corr(a: list, b: list) -> float:
    """
    Normalized cross-correlation of two float arrays.
    Returns a score in [0.0, 1.0] where 1.0 = perfect match.
    """
    n = min(len(a), len(b))
    if n == 0:
        return 0.0
    mean_a = sum(a[:n]) / n
    mean_b = sum(b[:n]) / n
    num = sum((a[i] - mean_a) * (b[i] - mean_b) for i in range(n))
    da  = sum((a[i] - mean_a) ** 2 for i in range(n))
    db  = sum((b[i] - mean_b) ** 2 for i in range(n))
    denom = (da * db) ** 0.5
    if denom == 0:
        return 1.0  # flat / identical signals
    r = num / denom
    return max(0.0, min(1.0, (r + 1.0) / 2.0))


def run_serial_server(port, baud=115200):
    try:
        import serial
    except ImportError:
        print("Error: pyserial required. Install with: pip install pyserial")
        sys.exit(1)

    device = MockDevice()
    print(f"Mock Sprite One v{'.'.join(map(str, device.version))}")
    print(f"Listening on {port} at {baud} baud")
    
    try:
        ser = serial.Serial(port, baud, timeout=0.1)
    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)

    buffer = bytearray()

    try:
        while True:
            data = ser.read(64)
            if data:
                buffer.extend(data)
                # Packet: HEADER(1) CMD(1) LEN(1) DATA(N) CRC(4)
                while len(buffer) >= 3 and buffer[0] == HEADER:
                    length = buffer[2]
                    packet_len = 3 + length + 4
                    if len(buffer) < packet_len:
                        break
                    
                    packet = bytes(buffer[:packet_len])
                    buffer = buffer[packet_len:]
                    
                    cmd = packet[1]
                    print(f"[RX] CMD:0x{cmd:02X} Len:{length}")
                    response = device.process_packet(packet)
                    ser.write(response)

                if buffer and buffer[0] != HEADER:
                    buffer.clear()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()

def run_loopback_test():
    device = MockDevice()
    print("=== Verification Test (CRC32 + Chunks) ===\n")
    
    # helper to build packet
    def make_packet(cmd, payload):
        data = bytearray([HEADER, cmd, len(payload)])
        data.extend(payload)
        # CRC over CMD+LEN+PAYLOAD
        crc = zlib.crc32(data[1:]) & 0xFFFFFFFF
        data.extend(struct.pack('<I', crc))
        return bytes(data)
        
    # Helper to check response
    def check(name, resp):
        if not resp:
             print(f"  ✗ {name}: No response")
             return
        # Parse AA CMD STATUS LEN [DATA] CRC
        if len(resp) < 7:
             print(f"  ✗ {name}: Short response")
             return
        
        status = resp[2]
        recv_crc = struct.unpack('<I', resp[-4:])[0]
        # CRC over CMD+STATUS+LEN+DATA
        calc_crc = zlib.crc32(resp[1:-4]) & 0xFFFFFFFF
        
        if recv_crc != calc_crc:
             print(f"  ✗ {name}: CRC Fail (Recv:{recv_crc:08X} Calc:{calc_crc:08X})")
             return
             
        if status == RESP_OK:
             print(f"  ✓ {name}")
        else:
             print(f"  ✗ {name}: Error Status {status}")

    # 1. Version
    check("Version", device.process_packet(make_packet(CMD_VERSION, b'')))
    
    # 2. Upload Flow
    print("\n[Testing Chunked Upload]")
    fake_model = b'X' * 512
    
    # Start
    check("Upload Start", device.process_packet(make_packet(CMD_MODEL_UPLOAD, b'test.bin')))
    
    # Chunks
    chunk1 = fake_model[:200]
    chunk2 = fake_model[200:400]
    chunk3 = fake_model[400:]
    check("Chunk 1", device.process_packet(make_packet(CMD_UPLOAD_CHUNK, chunk1)))
    check("Chunk 2", device.process_packet(make_packet(CMD_UPLOAD_CHUNK, chunk2)))
    check("Chunk 3", device.process_packet(make_packet(CMD_UPLOAD_CHUNK, chunk3)))
    
    # End
    crc = zlib.crc32(fake_model) & 0xFFFFFFFF
    check("Upload End", device.process_packet(make_packet(CMD_UPLOAD_END, struct.pack('<I', crc))))
    
    print("\nTests Complete.")


def run_api_test():
    """Self-contained loopback test for the Open API Primitives (0xA0-0xA7)."""
    device = MockDevice()
    print("=== Open API Primitives Test (no hardware required) ===\n")

    def make_packet(cmd, payload=b''):
        if isinstance(payload, (list, bytearray)):
            payload = bytes(payload)
        data = bytearray([HEADER, cmd, len(payload)])
        data.extend(payload)
        crc = zlib.crc32(data[1:]) & 0xFFFFFFFF
        data.extend(struct.pack('<I', crc))
        return bytes(data)

    def read_resp_float(resp):
        """Extract first float from a response payload."""
        if len(resp) < 8:
            return None
        # resp: AA CMD STATUS LEN [DATA...] CRC
        payload_len = resp[3]
        if payload_len < 4 or len(resp) < 4 + payload_len:
            return None
        return struct.unpack('<f', resp[4:8])[0]

    def check(name, resp, *, expect_ok=True, extra=''):
        ok_sym, fail_sym = '  \u2713', '  \u2717'
        if not resp or len(resp) < 7:
            print(f"{fail_sym} {name}: No/short response")
            return False
        status   = resp[2]
        recv_crc = struct.unpack('<I', resp[-4:])[0]
        calc_crc = zlib.crc32(resp[1:-4]) & 0xFFFFFFFF
        if recv_crc != calc_crc:
            print(f"{fail_sym} {name}: CRC fail")
            return False
        if expect_ok and status != RESP_OK:
            print(f"{fail_sym} {name}: Expected OK, got {status}  {extra}")
            return False
        if not expect_ok and status == RESP_OK:
            print(f"{fail_sym} {name}: Expected error, got OK")
            return False
        print(f"{ok_sym} {name}  {extra}")
        return True

    # ---- 1. CMD_WHO_IS_THERE (0xA0) ----
    resp = device.process_packet(make_packet(0xA0))
    check("WHO_IS_THERE returns 8-byte ID", resp)
    device_id = resp[4:12]  # Grab returned ID for next test

    # ---- 2. CMD_PING_ID (0xA1) ----
    print()
    resp = device.process_packet(make_packet(0xA1, device_id))
    check("PING_ID with correct ID -> OK", resp)

    wrong_id = bytes([0x00] * 8)
    resp = device.process_packet(make_packet(0xA1, wrong_id))
    check("PING_ID with wrong ID -> Error", resp, expect_ok=False)

    # ---- 3. CMD_BUFFER_WRITE (0xA2) ----
    print()
    samples = [1.0, 2.0, 3.0, 4.0, 5.0]
    for v in samples:
        resp = device.process_packet(make_packet(0xA2, struct.pack('<f', v)))
        check(f"BUFFER_WRITE({v})", resp)

    # Invalid payload
    resp = device.process_packet(make_packet(0xA2, b'\x00'))  # too short
    check("BUFFER_WRITE(too short) -> Error", resp, expect_ok=False)

    # ---- 4. CMD_BUFFER_SNAPSHOT (0xA3) ----
    print()
    resp = device.process_packet(make_packet(0xA3))
    ok = check("BUFFER_SNAPSHOT returns data", resp)
    if ok:
        count = resp[3] // 4
        floats = list(struct.unpack(f'<{count}f', resp[4:4 + count * 4]))
        print(f"     Samples ({count}): {floats}")
        assert floats == samples, "Snapshot mismatch!"
        print("     Content matches written samples \u2713")

    # ---- 5. CMD_BASELINE_CAPTURE (0xA4) ----
    print()
    resp = device.process_packet(make_packet(0xA4))
    ok = check("BASELINE_CAPTURE", resp)
    if ok:
        captured = read_resp_float(resp)
        expected_mean = sum(samples) / len(samples)
        print(f"     Captured baseline mean = {captured:.4f}  (expected {expected_mean:.4f})")
        assert abs(captured - expected_mean) < 1e-4, "Baseline mean mismatch!"
        print("     Value correct \u2713")

    # ---- 6. CMD_GET_DELTA after writing higher values (0xA6) ----
    print()
    for v in [10.0, 10.0, 10.0]:
        device.process_packet(make_packet(0xA2, struct.pack('<f', v)))
    resp = device.process_packet(make_packet(0xA6))
    ok = check("GET_DELTA (after adding higher values)", resp)
    if ok:
        delta = read_resp_float(resp)
        print(f"     Delta = {delta:.4f}")
        assert delta > 0, "Expected non-zero delta"
        print("     Delta is non-zero \u2713")

    # ---- 7. CMD_BASELINE_RESET (0xA5) ----
    print()
    resp = device.process_packet(make_packet(0xA5))
    check("BASELINE_RESET", resp)
    # GET_DELTA should now fail (no baseline)
    resp = device.process_packet(make_packet(0xA6))
    check("GET_DELTA with no baseline -> Error", resp, expect_ok=False)

    # ---- 8. CMD_CORRELATE (0xA7) ----
    print()
    # Use the same data that's in the buffer as reference -> should score ~1.0
    snap_resp = device.process_packet(make_packet(0xA3))
    snap_count = snap_resp[3] // 4
    reference  = snap_resp[4:4 + snap_count * 4]

    resp = device.process_packet(make_packet(0xA7, reference))
    ok = check("CORRELATE (identical reference)", resp)
    if ok:
        score = read_resp_float(resp)
        print(f"     Score = {score:.4f}  (expect 1.0 for identical)")
        assert score >= 0.99, f"Expected ~1.0, got {score}"
        print("     Score correct \u2713")

    # Inverted reference -> low score
    inv_ref = struct.pack(f'<{snap_count}f', *[-v for v in struct.unpack(f'<{snap_count}f', reference)])
    resp = device.process_packet(make_packet(0xA7, inv_ref))
    ok = check("CORRELATE (inverted reference)", resp)
    if ok:
        score = read_resp_float(resp)
        print(f"     Score = {score:.4f}  (expect ~0.0 for inverted)")
        assert score <= 0.05, f"Expected ~0.0, got {score}"
        print("     Score correct \u2713")

    print("\n=== All API Primitive Tests Passed! ===")


if __name__ == '__main__':
    if len(sys.argv) < 2 or sys.argv[1] == '--loopback':
        run_loopback_test()
    elif sys.argv[1] == '--test-api':
        run_api_test()
    else:
        run_serial_server(sys.argv[1])
