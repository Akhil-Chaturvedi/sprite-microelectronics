/**
 * TFLite to AIFes Converter (Browser)
 * Lightweight FlatBuffer parsing in JavaScript
 */

class TFLiteParser {
    constructor(buffer) {
        this.data = new Uint8Array(buffer);
        this.view = new DataView(buffer);
        this.model = null;
        this._parse();
    }

    _readUint32(offset) {
        return this.view.getUint32(offset, true);
    }

    _readInt32(offset) {
        return this.view.getInt32(offset, true);
    }

    _readUint16(offset) {
        return this.view.getUint16(offset, true);
    }

    _readInt8(offset) {
        return this.view.getInt8(offset);
    }

    _readUint8(offset) {
        return this.data[offset];
    }

    _readFloat32(offset) {
        return this.view.getFloat32(offset, true);
    }

    _readVectorOffset(tableOffset, fieldIndex) {
        const vtableOffset = tableOffset - this._readInt32(tableOffset);
        const vtableSize = this._readUint16(vtableOffset);

        const fieldOffsetPos = 4 + fieldIndex * 2;
        if (fieldOffsetPos >= vtableSize) return null;

        const fieldOffset = this._readUint16(vtableOffset + fieldOffsetPos);
        if (fieldOffset === 0) return null;

        return tableOffset + fieldOffset;
    }

    _readVector(offset) {
        const vecOffset = offset + this._readUint32(offset);
        const length = this._readUint32(vecOffset);
        return { length, dataOffset: vecOffset + 4 };
    }

    _parse() {
        const rootOffset = this._readUint32(0);

        this.model = {
            version: this._getModelVersion(rootOffset),
            buffers: this._getBuffers(rootOffset),
            subgraphs: this._getSubgraphs(rootOffset),
            operatorCodes: this._getOperatorCodes(rootOffset),
        };
    }

    _getModelVersion(root) {
        const offset = this._readVectorOffset(root, 0);
        return offset ? this._readUint32(offset) : 0;
    }

    _getBuffers(root) {
        const buffers = [];
        const offset = this._readVectorOffset(root, 3);
        if (!offset) return buffers;

        const { length, dataOffset } = this._readVector(offset);

        for (let i = 0; i < length; i++) {
            const bufferTable = dataOffset + i * 4;
            const bufferOffset = bufferTable + this._readUint32(bufferTable);

            const dataField = this._readVectorOffset(bufferOffset, 0);
            if (!dataField) {
                buffers.push(null);
            } else {
                const vec = this._readVector(dataField);
                buffers.push(this.data.slice(vec.dataOffset, vec.dataOffset + vec.length));
            }
        }

        return buffers;
    }

    _getOperatorCodes(root) {
        const codes = [];
        const offset = this._readVectorOffset(root, 1);
        if (!offset) return codes;

        const { length, dataOffset } = this._readVector(offset);

        for (let i = 0; i < length; i++) {
            const codeTable = dataOffset + i * 4;
            const codeOffset = codeTable + this._readUint32(codeTable);

            const builtinOffset = this._readVectorOffset(codeOffset, 1);
            if (builtinOffset) {
                codes.push(this._readInt8(builtinOffset));
            } else {
                const depOffset = this._readVectorOffset(codeOffset, 0);
                codes.push(depOffset ? this._readUint8(depOffset) : 0);
            }
        }

        return codes;
    }

    _getSubgraphs(root) {
        const subgraphs = [];
        const offset = this._readVectorOffset(root, 4);
        if (!offset) return subgraphs;

        const { length, dataOffset } = this._readVector(offset);

        for (let i = 0; i < length; i++) {
            const sgTable = dataOffset + i * 4;
            const sgOffset = sgTable + this._readUint32(sgTable);

            subgraphs.push({
                tensors: this._getTensors(sgOffset),
                operators: this._getOperators(sgOffset),
                inputs: this._getIOIndices(sgOffset, 2),
                outputs: this._getIOIndices(sgOffset, 3),
            });
        }

        return subgraphs;
    }

    _getTensors(subgraph) {
        const tensors = [];
        const offset = this._readVectorOffset(subgraph, 0);
        if (!offset) return tensors;

        const { length, dataOffset } = this._readVector(offset);

        for (let i = 0; i < length; i++) {
            const tTable = dataOffset + i * 4;
            const tOffset = tTable + this._readUint32(tTable);

            tensors.push({
                shape: this._getTensorShape(tOffset),
                type: this._getTensorType(tOffset),
                buffer: this._getTensorBuffer(tOffset),
                quantization: this._getTensorQuantization(tOffset),
            });
        }

        return tensors;
    }

    _getTensorShape(tensor) {
        const offset = this._readVectorOffset(tensor, 0);
        if (!offset) return [];

        const { length, dataOffset } = this._readVector(offset);
        const shape = [];
        for (let i = 0; i < length; i++) {
            shape.push(this._readInt32(dataOffset + i * 4));
        }
        return shape;
    }

    _getTensorType(tensor) {
        const offset = this._readVectorOffset(tensor, 1);
        return offset ? this._readUint8(offset) : 0;
    }

    _getTensorBuffer(tensor) {
        const offset = this._readVectorOffset(tensor, 2);
        return offset ? this._readUint32(offset) : 0;
    }

    _getTensorQuantization(tensor) {
        const offset = this._readVectorOffset(tensor, 4);
        if (!offset) return null;

        const quantOffset = offset + this._readUint32(offset);

        const scaleOffset = this._readVectorOffset(quantOffset, 1);
        const scales = [];
        if (scaleOffset) {
            const { length, dataOffset } = this._readVector(scaleOffset);
            for (let i = 0; i < length; i++) {
                scales.push(this._readFloat32(dataOffset + i * 4));
            }
        }

        const zpOffset = this._readVectorOffset(quantOffset, 2);
        const zeroPoints = [];
        if (zpOffset) {
            const { length, dataOffset } = this._readVector(zpOffset);
            for (let i = 0; i < length; i++) {
                // Read as int64, but JS only needs lower 32 bits for typical values
                zeroPoints.push(this._readInt32(dataOffset + i * 8));
            }
        }

        if (!scales.length && !zeroPoints.length) return null;
        return { scale: scales, zeroPoint: zeroPoints };
    }

    _getOperators(subgraph) {
        const operators = [];
        const offset = this._readVectorOffset(subgraph, 1);
        if (!offset) return operators;

        const { length, dataOffset } = this._readVector(offset);

        for (let i = 0; i < length; i++) {
            const opTable = dataOffset + i * 4;
            const opOffset = opTable + this._readUint32(opTable);

            operators.push({
                opcodeIndex: this._getOpOpcodeIndex(opOffset),
                inputs: this._getOpIO(opOffset, 1),
                outputs: this._getOpIO(opOffset, 2),
            });
        }

        return operators;
    }

    _getOpOpcodeIndex(op) {
        const offset = this._readVectorOffset(op, 0);
        return offset ? this._readUint32(offset) : 0;
    }

    _getOpIO(op, field) {
        const offset = this._readVectorOffset(op, field);
        if (!offset) return [];

        const { length, dataOffset } = this._readVector(offset);
        const indices = [];
        for (let i = 0; i < length; i++) {
            indices.push(this._readInt32(dataOffset + i * 4));
        }
        return indices;
    }

    _getIOIndices(subgraph, field) {
        const offset = this._readVectorOffset(subgraph, field);
        if (!offset) return [];

        const { length, dataOffset } = this._readVector(offset);
        const indices = [];
        for (let i = 0; i < length; i++) {
            indices.push(this._readInt32(dataOffset + i * 4));
        }
        return indices;
    }
}


class AIFesConverter {
    constructor(parser) {
        this.parser = parser;
        this.model = parser.model;
    }

    _dequantize(data, dtype, quant) {
        // Float32 passthrough
        if (dtype === 0) {
            return new Float32Array(data.buffer, data.byteOffset, data.length / 4);
        }

        let intData;
        if (dtype === 9) { // INT8
            intData = new Int8Array(data.buffer, data.byteOffset, data.length);
        } else if (dtype === 3) { // UINT8
            intData = new Uint8Array(data);
        } else if (dtype === 7) { // INT16
            intData = new Int16Array(data.buffer, data.byteOffset, data.length / 2);
        } else {
            return new Float32Array(data.buffer, data.byteOffset, data.length / 4);
        }

        const floatData = new Float32Array(intData.length);

        if (quant && quant.scale && quant.scale.length && quant.zeroPoint && quant.zeroPoint.length) {
            const scale = quant.scale[0];
            const zp = quant.zeroPoint[0];
            for (let i = 0; i < intData.length; i++) {
                floatData[i] = (intData[i] - zp) * scale;
            }
        } else {
            for (let i = 0; i < intData.length; i++) {
                floatData[i] = intData[i];
            }
        }

        return floatData;
    }

    _crc32(data) {
        let crc = 0xFFFFFFFF;
        for (let i = 0; i < data.length; i++) {
            crc ^= data[i];
            for (let j = 0; j < 8; j++) {
                crc = (crc >>> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        return (~crc) >>> 0;
    }

    convert(name = 'converted') {
        if (!this.model.subgraphs.length) {
            throw new Error('No subgraphs found');
        }

        const subgraph = this.model.subgraphs[0];
        const tensors = subgraph.tensors;
        const operators = subgraph.operators;
        const buffers = this.model.buffers;
        const opCodes = this.model.operatorCodes;

        // Analyze structure
        let inputSize = 1, outputSize = 1, hiddenSize = 8;

        if (subgraph.inputs.length && tensors[subgraph.inputs[0]].shape.length) {
            const shape = tensors[subgraph.inputs[0]].shape;
            inputSize = shape.slice(1).reduce((a, b) => a * b, 1) || shape[0];
        }

        if (subgraph.outputs.length && tensors[subgraph.outputs[0]].shape.length) {
            const shape = tensors[subgraph.outputs[0]].shape;
            outputSize = shape[shape.length - 1] || 1;
        }

        // Extract weights
        const allWeights = [];
        for (const op of operators) {
            const opcode = opCodes[op.opcodeIndex] || 0;

            for (const idx of op.inputs) {
                if (idx < 0 || idx >= tensors.length) continue;

                const tensor = tensors[idx];
                const bufferIdx = tensor.buffer;

                if (bufferIdx > 0 && bufferIdx < buffers.length && buffers[bufferIdx]) {
                    const weights = this._dequantize(
                        buffers[bufferIdx],
                        tensor.type,
                        tensor.quantization
                    );
                    allWeights.push(...weights);

                    // Estimate hidden size
                    if (opcode === 9 && tensor.shape.length >= 2) {
                        hiddenSize = Math.max(hiddenSize, tensor.shape[0]);
                    }
                }
            }
        }

        // Build .aif32
        const weightsArray = new Float32Array(allWeights);
        const weightsBytes = new Uint8Array(weightsArray.buffer);

        const header = new Uint8Array(32);
        const headerView = new DataView(header.buffer);

        // Magic "SPRT"
        headerView.setUint32(0, 0x54525053, true);
        // Version
        headerView.setUint16(4, 0x0002, true);
        // Sizes
        header[6] = Math.min(255, inputSize);
        header[7] = Math.min(255, outputSize);
        header[8] = Math.min(255, hiddenSize);
        header[9] = 0; // F32
        header[10] = operators.length;
        header[11] = 0;
        // CRC
        headerView.setUint32(12, this._crc32(weightsBytes), true);
        // Name
        const nameBytes = new TextEncoder().encode(name.slice(0, 15));
        header.set(nameBytes, 16);

        // Combine
        const result = new Uint8Array(32 + weightsBytes.length);
        result.set(header, 0);
        result.set(weightsBytes, 32);

        return {
            data: result,
            info: {
                inputSize,
                outputSize,
                hiddenSize,
                numLayers: operators.length,
                numWeights: allWeights.length,
                originalSize: this.parser.data.length,
                convertedSize: result.length,
            }
        };
    }
}


// Export for webapp
window.TFLiteParser = TFLiteParser;
window.AIFesConverter = AIFesConverter;

/**
 * Convert TFLite buffer to AIFes
 * @param {ArrayBuffer} buffer - TFLite file buffer
 * @param {string} name - Model name
 * @returns {Object} { data: Uint8Array, info: Object }
 */
window.convertTFLiteToAIFes = function (buffer, name = 'converted') {
    const parser = new TFLiteParser(buffer);
    const converter = new AIFesConverter(parser);
    return converter.convert(name);
};
