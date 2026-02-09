/**
 * Sprite Training Engine
 * WebGPU with CPU fallback, ETA tracking
 */

class SpriteTrainer {
    constructor() {
        this.backend = 'cpu';
        this.isTraining = false;
        this.model = null;
        this.onProgress = null;
        this.startTime = null;

        this.checkBackend();
    }

    async checkBackend() {
        // Check for WebGPU support
        if ('gpu' in navigator) {
            try {
                const adapter = await navigator.gpu.requestAdapter();
                if (adapter) {
                    this.backend = 'webgpu';
                    console.log('[Training] WebGPU available');
                    return;
                }
            } catch (err) {
                console.log('[Training] WebGPU check failed:', err);
            }
        }

        console.log('[Training] Falling back to CPU');
        this.backend = 'cpu';
    }

    getBackend() {
        return this.backend;
    }

    createModel(arch, inputSize, outputSize, hiddenSize = 8) {
        // Simple model structure for export
        this.modelConfig = {
            arch,
            inputSize,
            outputSize,
            hiddenSize,
            weights: null
        };

        // Initialize weights randomly
        const weightsCount = inputSize * hiddenSize + hiddenSize + hiddenSize * outputSize + outputSize;
        this.modelConfig.weights = new Float32Array(weightsCount);
        for (let i = 0; i < weightsCount; i++) {
            this.modelConfig.weights[i] = (Math.random() - 0.5) * 0.5;
        }

        return this.modelConfig;
    }

    async train(data, labels, options = {}) {
        const epochs = options.epochs || 50;
        const lr = options.lr || 0.01;

        this.isTraining = true;
        this.startTime = Date.now();

        const inputSize = data[0].length;
        const outputSize = labels[0].length;
        const hiddenSize = options.hiddenSize || 8;

        if (!this.modelConfig) {
            this.createModel('mlp', inputSize, outputSize, hiddenSize);
        }

        let loss = 1.0;
        let accuracy = 0;

        for (let epoch = 0; epoch < epochs; epoch++) {
            if (!this.isTraining) break;

            // Simple forward/backward pass simulation
            // In production, use TF.js or WebGPU compute shaders
            const result = this.trainEpoch(data, labels, lr);
            loss = result.loss;
            accuracy = result.accuracy;

            // Calculate ETA
            const elapsed = Date.now() - this.startTime;
            const msPerEpoch = elapsed / (epoch + 1);
            const remaining = (epochs - epoch - 1) * msPerEpoch;
            const eta = this.formatTime(remaining);

            // Progress callback
            if (this.onProgress) {
                this.onProgress({
                    epoch: epoch + 1,
                    epochs,
                    loss,
                    accuracy,
                    eta,
                    progress: ((epoch + 1) / epochs) * 100
                });
            }

            // Yield to UI
            await new Promise(r => setTimeout(r, 0));
        }

        this.isTraining = false;
        return { loss, accuracy, weights: this.modelConfig.weights };
    }

    trainEpoch(data, labels, lr) {
        // Simplified training loop (gradient descent simulation)
        let totalLoss = 0;
        let correct = 0;
        const w = this.modelConfig.weights;
        const h = this.modelConfig.hiddenSize;
        const inSize = this.modelConfig.inputSize;
        const outSize = this.modelConfig.outputSize;

        for (let i = 0; i < data.length; i++) {
            const x = data[i];
            const y = labels[i];

            // Forward pass
            const hidden = new Float32Array(h);
            for (let j = 0; j < h; j++) {
                let sum = w[inSize * h + j]; // bias
                for (let k = 0; k < inSize; k++) {
                    sum += x[k] * w[k * h + j];
                }
                hidden[j] = this.sigmoid(sum);
            }

            const output = new Float32Array(outSize);
            const outOffset = inSize * h + h;
            for (let j = 0; j < outSize; j++) {
                let sum = w[outOffset + h * outSize + j]; // bias
                for (let k = 0; k < h; k++) {
                    sum += hidden[k] * w[outOffset + k * outSize + j];
                }
                output[j] = this.sigmoid(sum);
            }

            // Loss (MSE)
            let loss = 0;
            for (let j = 0; j < outSize; j++) {
                loss += Math.pow(y[j] - output[j], 2);
            }
            totalLoss += loss / outSize;

            // Accuracy (threshold 0.5)
            const predicted = output.map(v => v > 0.5 ? 1 : 0);
            if (predicted.every((v, idx) => v === Math.round(y[idx]))) {
                correct++;
            }

            // Backward pass (gradient descent)
            const outGrad = new Float32Array(outSize);
            for (let j = 0; j < outSize; j++) {
                outGrad[j] = (output[j] - y[j]) * output[j] * (1 - output[j]);
            }

            // Update output weights
            for (let j = 0; j < outSize; j++) {
                w[outOffset + h * outSize + j] -= lr * outGrad[j]; // bias
                for (let k = 0; k < h; k++) {
                    w[outOffset + k * outSize + j] -= lr * outGrad[j] * hidden[k];
                }
            }

            // Hidden gradients
            const hidGrad = new Float32Array(h);
            for (let j = 0; j < h; j++) {
                let sum = 0;
                for (let k = 0; k < outSize; k++) {
                    sum += outGrad[k] * w[outOffset + j * outSize + k];
                }
                hidGrad[j] = sum * hidden[j] * (1 - hidden[j]);
            }

            // Update hidden weights
            for (let j = 0; j < h; j++) {
                w[inSize * h + j] -= lr * hidGrad[j]; // bias
                for (let k = 0; k < inSize; k++) {
                    w[k * h + j] -= lr * hidGrad[j] * x[k];
                }
            }
        }

        return {
            loss: totalLoss / data.length,
            accuracy: correct / data.length
        };
    }

    sigmoid(x) {
        return 1 / (1 + Math.exp(-x));
    }

    formatTime(ms) {
        if (ms < 1000) return '<1s';
        const seconds = Math.floor(ms / 1000);
        if (seconds < 60) return `${seconds}s`;
        const minutes = Math.floor(seconds / 60);
        const secs = seconds % 60;
        return `${minutes}m ${secs}s`;
    }

    stop() {
        this.isTraining = false;
    }

    exportToAIFes() {
        if (!this.modelConfig || !this.modelConfig.weights) {
            throw new Error('No model to export');
        }

        const header = new Uint8Array(32);
        const view = new DataView(header.buffer);

        // Magic: "SPRT"
        view.setUint32(0, 0x54525053, true);
        // Version
        view.setUint16(4, 0x0001, true);
        // Sizes
        header[6] = this.modelConfig.inputSize;
        header[7] = this.modelConfig.outputSize;
        header[8] = this.modelConfig.hiddenSize;
        header[9] = 0; // F32 type

        // CRC (simplified)
        let crc = 0;
        for (let i = 0; i < this.modelConfig.weights.length; i++) {
            crc = (crc + Math.floor(this.modelConfig.weights[i] * 1000)) & 0xFFFFFFFF;
        }
        view.setUint32(12, crc, true);

        // Name
        const name = 'trained';
        for (let i = 0; i < name.length && i < 16; i++) {
            header[16 + i] = name.charCodeAt(i);
        }

        // Weights
        const weightsBuffer = new Uint8Array(this.modelConfig.weights.buffer);

        // Combine
        const result = new Uint8Array(header.length + weightsBuffer.length);
        result.set(header, 0);
        result.set(weightsBuffer, 32);

        return result;
    }
}

window.SpriteTrainer = SpriteTrainer;
