"""
TFLite to AIFes (.aif32) Converter
Lightweight FlatBuffers-based extraction - no TensorFlow dependency for extraction

Architecture:
1. Normalize: Convert .h5/.js to .tflite (requires TensorFlow - separate step)
2. Extract: Read .tflite using FlatBuffers schema (this script - lightweight)
3. Transpile: Write .aif32 format

Dependencies: pip install flatbuffers numpy
"""

import struct
import numpy as np
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any

# ===== AIFes Model Format =====
# Header: 32 bytes
#   magic (4): "SPRT" = 0x54525053
#   version (2): format version
#   input_size (1): number of inputs
#   output_size (1): number of outputs
#   hidden_size (1): hidden layer neurons
#   model_type (1): 0=F32, 1=Q7
#   num_layers (1): number of layers
#   reserved (1): padding
#   weights_crc (4): CRC32 of weights
#   name (16): model name

AIFES_MAGIC = 0x54525053  # "SPRT"
AIFES_VERSION = 0x0002
MODEL_TYPE_F32 = 0
MODEL_TYPE_Q7 = 1

# TFLite BuiltinOperator codes (from schema)
class TFLiteOp:
    ADD = 0
    AVERAGE_POOL_2D = 1
    CONCATENATION = 2
    CONV_2D = 3
    DEPTHWISE_CONV_2D = 4
    FULLY_CONNECTED = 9
    MAX_POOL_2D = 17
    MUL = 18
    RELU = 19
    RELU6 = 21
    RESHAPE = 22
    SOFTMAX = 25
    TANH = 28
    LOGISTIC = 14  # Sigmoid

# TFLite TensorType
class TFLiteType:
    FLOAT32 = 0
    FLOAT16 = 1
    INT32 = 2
    UINT8 = 3
    INT64 = 4
    STRING = 5
    BOOL = 6
    INT16 = 7
    COMPLEX64 = 8
    INT8 = 9


class TFLiteParser:
    """
    Lightweight TFLite parser using direct FlatBuffer reading.
    No TensorFlow dependency required.
    """
    
    def __init__(self, data: bytes):
        self.data = data
        self.model = None
        self._parse()
    
    def _read_uint32(self, offset: int) -> int:
        return struct.unpack_from('<I', self.data, offset)[0]
    
    def _read_int32(self, offset: int) -> int:
        return struct.unpack_from('<i', self.data, offset)[0]
    
    def _read_uint16(self, offset: int) -> int:
        return struct.unpack_from('<H', self.data, offset)[0]
    
    def _read_int16(self, offset: int) -> int:
        return struct.unpack_from('<h', self.data, offset)[0]
    
    def _read_uint8(self, offset: int) -> int:
        return self.data[offset]
    
    def _read_int8(self, offset: int) -> int:
        return struct.unpack_from('<b', self.data, offset)[0]
    
    def _read_float32(self, offset: int) -> float:
        return struct.unpack_from('<f', self.data, offset)[0]
    
    def _read_vector_offset(self, table_offset: int, field_index: int) -> Optional[int]:
        """Read offset to a vector field in a FlatBuffer table."""
        vtable_offset = table_offset - self._read_int32(table_offset)
        vtable_size = self._read_uint16(vtable_offset)
        
        field_offset_pos = 4 + field_index * 2
        if field_offset_pos >= vtable_size:
            return None
        
        field_offset = self._read_uint16(vtable_offset + field_offset_pos)
        if field_offset == 0:
            return None
        
        return table_offset + field_offset
    
    def _read_vector(self, offset: int) -> Tuple[int, int]:
        """Read vector length and data offset."""
        vec_offset = offset + self._read_uint32(offset)
        length = self._read_uint32(vec_offset)
        return length, vec_offset + 4
    
    def _parse(self):
        """Parse the TFLite model structure."""
        # FlatBuffer root table offset
        root_offset = self._read_uint32(0)
        
        self.model = {
            'version': self._get_model_version(root_offset),
            'description': self._get_model_description(root_offset),
            'buffers': self._get_buffers(root_offset),
            'subgraphs': self._get_subgraphs(root_offset),
            'operator_codes': self._get_operator_codes(root_offset),
        }
    
    def _get_model_version(self, root: int) -> int:
        offset = self._read_vector_offset(root, 0)
        if offset is None:
            return 0
        return self._read_uint32(offset)
    
    def _get_model_description(self, root: int) -> str:
        offset = self._read_vector_offset(root, 2)
        if offset is None:
            return ""
        length, data_offset = self._read_vector(offset)
        return self.data[data_offset:data_offset + length].decode('utf-8', errors='ignore')
    
    def _get_buffers(self, root: int) -> List[bytes]:
        """Extract all weight buffers."""
        buffers = []
        offset = self._read_vector_offset(root, 3)
        if offset is None:
            return buffers
        
        length, vec_offset = self._read_vector(offset)
        
        for i in range(length):
            buffer_table = vec_offset + i * 4
            buffer_offset = buffer_table + self._read_uint32(buffer_table)
            
            # Get data field (field 0)
            data_field = self._read_vector_offset(buffer_offset, 0)
            if data_field is None:
                buffers.append(b'')
            else:
                data_len, data_offset = self._read_vector(data_field)
                buffers.append(self.data[data_offset:data_offset + data_len])
        
        return buffers
    
    def _get_operator_codes(self, root: int) -> List[int]:
        """Get list of operator codes used in the model."""
        codes = []
        offset = self._read_vector_offset(root, 1)
        if offset is None:
            return codes
        
        length, vec_offset = self._read_vector(offset)
        
        for i in range(length):
            code_table = vec_offset + i * 4
            code_offset = code_table + self._read_uint32(code_table)
            
            # BuiltinCode is field 1 (deprecated field 0 for backwards compat)
            builtin_offset = self._read_vector_offset(code_offset, 1)
            if builtin_offset is not None:
                codes.append(self._read_int8(builtin_offset))
            else:
                # Try deprecated field 0
                dep_offset = self._read_vector_offset(code_offset, 0)
                if dep_offset is not None:
                    codes.append(self._read_uint8(dep_offset))
                else:
                    codes.append(0)
        
        return codes
    
    def _get_subgraphs(self, root: int) -> List[Dict]:
        """Parse subgraphs (usually just one main graph)."""
        subgraphs = []
        offset = self._read_vector_offset(root, 4)
        if offset is None:
            return subgraphs
        
        length, vec_offset = self._read_vector(offset)
        
        for i in range(length):
            subgraph_table = vec_offset + i * 4
            subgraph_offset = subgraph_table + self._read_uint32(subgraph_table)
            
            subgraphs.append({
                'tensors': self._get_tensors(subgraph_offset),
                'operators': self._get_operators(subgraph_offset),
                'inputs': self._get_io_indices(subgraph_offset, 2),
                'outputs': self._get_io_indices(subgraph_offset, 3),
            })
        
        return subgraphs
    
    def _get_tensors(self, subgraph: int) -> List[Dict]:
        """Parse tensor definitions."""
        tensors = []
        offset = self._read_vector_offset(subgraph, 0)
        if offset is None:
            return tensors
        
        length, vec_offset = self._read_vector(offset)
        
        for i in range(length):
            tensor_table = vec_offset + i * 4
            tensor_offset = tensor_table + self._read_uint32(tensor_table)
            
            tensor = {
                'shape': self._get_tensor_shape(tensor_offset),
                'type': self._get_tensor_type(tensor_offset),
                'buffer': self._get_tensor_buffer(tensor_offset),
                'name': self._get_tensor_name(tensor_offset),
                'quantization': self._get_tensor_quantization(tensor_offset),
            }
            tensors.append(tensor)
        
        return tensors
    
    def _get_tensor_shape(self, tensor: int) -> List[int]:
        offset = self._read_vector_offset(tensor, 0)
        if offset is None:
            return []
        length, data_offset = self._read_vector(offset)
        shape = []
        for i in range(length):
            shape.append(self._read_int32(data_offset + i * 4))
        return shape
    
    def _get_tensor_type(self, tensor: int) -> int:
        offset = self._read_vector_offset(tensor, 1)
        if offset is None:
            return TFLiteType.FLOAT32
        return self._read_uint8(offset)
    
    def _get_tensor_buffer(self, tensor: int) -> int:
        offset = self._read_vector_offset(tensor, 2)
        if offset is None:
            return 0
        return self._read_uint32(offset)
    
    def _get_tensor_name(self, tensor: int) -> str:
        offset = self._read_vector_offset(tensor, 3)
        if offset is None:
            return ""
        length, data_offset = self._read_vector(offset)
        return self.data[data_offset:data_offset + length].decode('utf-8', errors='ignore')
    
    def _get_tensor_quantization(self, tensor: int) -> Optional[Dict]:
        offset = self._read_vector_offset(tensor, 4)
        if offset is None:
            return None
        
        quant_offset = offset + self._read_uint32(offset)
        
        # Scale (field 1)
        scale_offset = self._read_vector_offset(quant_offset, 1)
        scales = []
        if scale_offset is not None:
            length, data_offset = self._read_vector(scale_offset)
            for i in range(length):
                scales.append(self._read_float32(data_offset + i * 4))
        
        # Zero point (field 2)
        zp_offset = self._read_vector_offset(quant_offset, 2)
        zero_points = []
        if zp_offset is not None:
            length, data_offset = self._read_vector(zp_offset)
            for i in range(length):
                zero_points.append(self._read_int64(data_offset + i * 8))
        
        if not scales and not zero_points:
            return None
        
        return {
            'scale': scales,
            'zero_point': zero_points,
        }
    
    def _read_int64(self, offset: int) -> int:
        return struct.unpack_from('<q', self.data, offset)[0]
    
    def _get_operators(self, subgraph: int) -> List[Dict]:
        """Parse operators (layers)."""
        operators = []
        offset = self._read_vector_offset(subgraph, 1)
        if offset is None:
            return operators
        
        length, vec_offset = self._read_vector(offset)
        
        for i in range(length):
            op_table = vec_offset + i * 4
            op_offset = op_table + self._read_uint32(op_table)
            
            operators.append({
                'opcode_index': self._get_op_opcode_index(op_offset),
                'inputs': self._get_op_io(op_offset, 1),
                'outputs': self._get_op_io(op_offset, 2),
            })
        
        return operators
    
    def _get_op_opcode_index(self, op: int) -> int:
        offset = self._read_vector_offset(op, 0)
        if offset is None:
            return 0
        return self._read_uint32(offset)
    
    def _get_op_io(self, op: int, field: int) -> List[int]:
        offset = self._read_vector_offset(op, field)
        if offset is None:
            return []
        length, data_offset = self._read_vector(offset)
        indices = []
        for i in range(length):
            indices.append(self._read_int32(data_offset + i * 4))
        return indices
    
    def _get_io_indices(self, subgraph: int, field: int) -> List[int]:
        offset = self._read_vector_offset(subgraph, field)
        if offset is None:
            return []
        length, data_offset = self._read_vector(offset)
        indices = []
        for i in range(length):
            indices.append(self._read_int32(data_offset + i * 4))
        return indices


class AIFesConverter:
    """
    Convert parsed TFLite model to AIFes .aif32 format.
    """
    
    def __init__(self, parser: TFLiteParser):
        self.parser = parser
        self.model = parser.model
    
    def _dequantize(self, data: bytes, dtype: int, quant: Optional[Dict]) -> np.ndarray:
        """Convert quantized data to float32."""
        if dtype == TFLiteType.FLOAT32:
            return np.frombuffer(data, dtype=np.float32)
        
        if dtype == TFLiteType.INT8:
            int_data = np.frombuffer(data, dtype=np.int8)
        elif dtype == TFLiteType.UINT8:
            int_data = np.frombuffer(data, dtype=np.uint8)
        elif dtype == TFLiteType.INT16:
            int_data = np.frombuffer(data, dtype=np.int16)
        elif dtype == TFLiteType.INT32:
            int_data = np.frombuffer(data, dtype=np.int32)
        else:
            return np.frombuffer(data, dtype=np.float32)
        
        if quant and quant.get('scale') and quant.get('zero_point'):
            scale = quant['scale'][0] if quant['scale'] else 1.0
            zp = quant['zero_point'][0] if quant['zero_point'] else 0
            return (int_data.astype(np.float32) - zp) * scale
        
        return int_data.astype(np.float32)
    
    def _crc32(self, data: bytes) -> int:
        """Calculate CRC32."""
        crc = 0xFFFFFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1))
        return ~crc & 0xFFFFFFFF
    
    def convert(self, name: str = "converted") -> bytes:
        """
        Convert to AIFes format.
        Returns the complete .aif32 binary.
        """
        if not self.model['subgraphs']:
            raise ValueError("No subgraphs found in model")
        
        subgraph = self.model['subgraphs'][0]
        tensors = subgraph['tensors']
        operators = subgraph['operators']
        buffers = self.model['buffers']
        op_codes = self.model['operator_codes']
        
        # Analyze model structure
        input_indices = subgraph['inputs']
        output_indices = subgraph['outputs']
        
        # Get input/output dimensions
        input_size = 1
        output_size = 1
        hidden_size = 8  # Default
        
        if input_indices and tensors[input_indices[0]]['shape']:
            shape = tensors[input_indices[0]]['shape']
            input_size = np.prod(shape[1:]) if len(shape) > 1 else shape[0]
        
        if output_indices and tensors[output_indices[0]]['shape']:
            shape = tensors[output_indices[0]]['shape']
            output_size = shape[-1] if shape else 1
        
        # Extract and flatten all weights
        all_weights = []
        for op in operators:
            opcode = op_codes[op['opcode_index']] if op['opcode_index'] < len(op_codes) else 0
            
            # Process weight tensors (skip activation inputs)
            for idx in op['inputs']:
                if idx < 0 or idx >= len(tensors):
                    continue
                
                tensor = tensors[idx]
                buffer_idx = tensor['buffer']
                
                if buffer_idx > 0 and buffer_idx < len(buffers) and buffers[buffer_idx]:
                    weights = self._dequantize(
                        buffers[buffer_idx],
                        tensor['type'],
                        tensor['quantization']
                    )
                    all_weights.extend(weights.tolist())
                    
                    # Estimate hidden size from first dense layer
                    if opcode == TFLiteOp.FULLY_CONNECTED and tensor['shape']:
                        if len(tensor['shape']) >= 2:
                            hidden_size = max(hidden_size, tensor['shape'][0])
        
        # Build .aif32 binary
        weights_array = np.array(all_weights, dtype=np.float32)
        weights_bytes = weights_array.tobytes()
        
        # Header (32 bytes)
        header = bytearray(32)
        struct.pack_into('<I', header, 0, AIFES_MAGIC)
        struct.pack_into('<H', header, 4, AIFES_VERSION)
        header[6] = min(255, int(input_size))
        header[7] = min(255, int(output_size))
        header[8] = min(255, int(hidden_size))
        header[9] = MODEL_TYPE_F32
        header[10] = len(operators)
        header[11] = 0  # Reserved
        struct.pack_into('<I', header, 12, self._crc32(weights_bytes))
        
        # Name (16 bytes max)
        name_bytes = name[:15].encode('utf-8')
        header[16:16 + len(name_bytes)] = name_bytes
        
        return bytes(header) + weights_bytes
    
    def get_model_info(self) -> Dict[str, Any]:
        """Get extracted model information for debugging."""
        if not self.model['subgraphs']:
            return {}
        
        subgraph = self.model['subgraphs'][0]
        op_codes = self.model['operator_codes']
        
        layers = []
        for op in subgraph['operators']:
            opcode = op_codes[op['opcode_index']] if op['opcode_index'] < len(op_codes) else -1
            op_name = {
                TFLiteOp.CONV_2D: 'Conv2D',
                TFLiteOp.DEPTHWISE_CONV_2D: 'DepthwiseConv2D',
                TFLiteOp.FULLY_CONNECTED: 'Dense',
                TFLiteOp.RELU: 'ReLU',
                TFLiteOp.RELU6: 'ReLU6',
                TFLiteOp.SOFTMAX: 'Softmax',
                TFLiteOp.LOGISTIC: 'Sigmoid',
                TFLiteOp.TANH: 'Tanh',
                TFLiteOp.RESHAPE: 'Reshape',
                TFLiteOp.MAX_POOL_2D: 'MaxPool2D',
                TFLiteOp.AVERAGE_POOL_2D: 'AvgPool2D',
            }.get(opcode, f'Op_{opcode}')
            
            layers.append({
                'type': op_name,
                'inputs': op['inputs'],
                'outputs': op['outputs'],
            })
        
        return {
            'version': self.model['version'],
            'description': self.model['description'],
            'num_buffers': len(self.model['buffers']),
            'num_tensors': len(subgraph['tensors']),
            'num_operators': len(subgraph['operators']),
            'input_indices': subgraph['inputs'],
            'output_indices': subgraph['outputs'],
            'layers': layers,
        }


def convert_tflite_to_aif32(input_path: str, output_path: str, name: str = None) -> Dict:
    """
    Convert a .tflite file to .aif32 format.
    
    Args:
        input_path: Path to input .tflite file
        output_path: Path to output .aif32 file
        name: Model name (default: input filename)
    
    Returns:
        Dict with conversion info
    """
    input_file = Path(input_path)
    if not input_file.exists():
        raise FileNotFoundError(f"Input file not found: {input_path}")
    
    if name is None:
        name = input_file.stem
    
    # Read and parse
    with open(input_path, 'rb') as f:
        data = f.read()
    
    parser = TFLiteParser(data)
    converter = AIFesConverter(parser)
    
    # Get info before conversion
    info = converter.get_model_info()
    
    # Convert
    aif32_data = converter.convert(name)
    
    # Write output
    with open(output_path, 'wb') as f:
        f.write(aif32_data)
    
    info['input_size'] = len(data)
    info['output_size'] = len(aif32_data)
    info['output_path'] = output_path
    
    return info


if __name__ == '__main__':
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python tflite_to_aif32.py input.tflite [output.aif32]")
        sys.exit(1)
    
    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else input_path.replace('.tflite', '.aif32')
    
    try:
        info = convert_tflite_to_aif32(input_path, output_path)
        print(f"Converted: {input_path} -> {output_path}")
        print(f"  Input size: {info['input_size']} bytes")
        print(f"  Output size: {info['output_size']} bytes")
        print(f"  Layers: {info['num_operators']}")
        print(f"  Tensors: {info['num_tensors']}")
        for i, layer in enumerate(info['layers']):
            print(f"    {i}: {layer['type']}")
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
