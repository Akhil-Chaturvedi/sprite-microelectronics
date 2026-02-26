/**
 * Mock Sprite Device
 * Simulates device responses for testing, development, and demos.
 * Implements the same interface as SpriteDevice but runs entirely in-browser.
 */

// Guard against double-loading (browser cache / multiple script tags)
if (typeof window !== 'undefined' && window.MockSpriteDevice) {
  // Already defined — nothing to do
} else {

  // Helper for CRC32
  function crc32(data) {
    let crc = 0xFFFFFFFF;
    for (let i = 0; i < data.length; i++) {
      const byte = data[i];
      crc = (crc >>> 8) ^ crc32_table[(crc ^ byte) & 0xFF];
    }
    return (crc ^ 0xFFFFFFFF) >>> 0;
  }

  const crc32_table = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let j = 0; j < 8; j++) {
      c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
    }
    crc32_table[i] = c;
  }

  class MockSpriteDevice {
    constructor(name = 'Mock Sprite One') {
      this.displayName = name;
      this.isConnected = false;
      this.version = 'v2.1.0';
      this.portInfo = { usbVendorId: 0x2E8A, usbProductId: 0x000A };

      // Simulated state
      this.framebuffer = new Uint8Array(128 * 64 / 8); // 1-bit display
      this.models = [
        { name: 'xor.aif32', size: 192, inputSize: 2, outputSize: 1, hiddenSize: 8 },
        { name: 'and.aif32', size: 192, inputSize: 2, outputSize: 1, hiddenSize: 8 },
      ];
      this.activeModel = 0;
      this.aiState = { state: 0, model_loaded: true, epochs: 0, last_loss: 0.5 };
      this.sprites = new Array(8).fill(null);

      // Sentinel Simulation State
      this.sentinelLoopId = null;
      this.sentinelState = {
        temp: 35.0,
        freq: 133,
        memories: 0,
        reflexActive: false
      };

      // Simulated latency (ms)
      this.latency = { min: 5, max: 25 };

      // Open API Primitive State
      this.deviceId = this._generateDeviceId();  // Unique 8-byte identity
      this.circularBuffer = [];                   // Rolling float samples (max 60)
      this.baseline = null;                        // Captured mean float, or null

      // Canvas for framebuffer visualization
      this.displayCanvas = null;
      this.displayCtx = null;
    }

    // --- Same interface as SpriteDevice ---

    static async scan() {
      // Always return one mock device
      return [new MockSpriteDevice()];
    }

    static async requestPort() {
      return new MockSpriteDevice();
    }

    async open() {
      this.isConnected = true;
      this._log('Connected (mock)');
      return true;
    }

    async close() {
      this.isConnected = false;
      if (this.sentinelLoopId) clearInterval(this.sentinelLoopId);
      this._log('Disconnected (mock)');
    }

    async probe() {
      this.isConnected = true;
      return true;
    }

    getDisplayName() {
      return this.displayName + ' ' + this.version;
    }

    // --- Protocol simulation ---

    async sendCommand(cmd, payload = []) {
      if (!this.isConnected) await this.open();
      await this._simulateLatency();
      return this._handleCommand(cmd, new Uint8Array(payload));
    }

    async uploadModel(filename, data) {
      if (!this.isConnected) await this.open();

      this._log(`Uploading ${filename} (${data.length} bytes)`);

      // Simulate chunked upload delay
      const chunks = Math.ceil(data.length / 256);
      for (let i = 0; i < chunks; i++) {
        if (i % 10 === 0) await new Promise(r => setTimeout(r, 1));
      }

      // Parse V3 Header
      const model = {
        name: filename,
        size: data.length,
        isLoaded: false,
        layers: []
      };
      model.inputSize = 0;
      model.outputSize = 0;

      if (data.length >= 32) {
        const dv = new DataView(data.buffer, data.byteOffset, data.length);
        const magic = dv.getUint32(0, true);

        if (magic === 0x54525053) { // "SPRT"
          const version = dv.getUint16(4, true);

          if (version === 3) {
            // --- V3 PARSER ---
            const layerCount = dv.getUint16(6, true);
            const totalWeights = dv.getUint32(8, true);

            // Parse 16-byte Descriptors
            let ptr = 32;
            let currentShape = [0]; // 1D or 3D

            for (let i = 0; i < layerCount; i++) {
              const type = dv.getUint8(ptr);
              // flags at ptr+1
              const param1 = dv.getUint16(ptr + 2, true);
              const param2 = dv.getUint16(ptr + 4, true);
              const param3 = dv.getUint16(ptr + 6, true);
              const param4 = dv.getUint16(ptr + 8, true);
              const param5 = dv.getUint16(ptr + 10, true);
              const param6 = dv.getUint16(ptr + 12, true);
              // reserved at ptr+14

              ptr += 16;

              // Store Layer Config
              const layer = { type, param1, param2, param3, param4, param5, param6 };

              if (type === 1) { // Input
                // param1=H/Size, param2=W, param3=C
                const h = param1;
                const w = param2;
                const c = param3;

                if (w > 0 && c > 0) {
                  // 3D Input
                  model.inputSize = c * h * w;
                  currentShape = [c, h, w];
                } else {
                  // 1D Input
                  model.inputSize = param1;
                  currentShape = [param1];
                }
              } else if (type === 2) { // Dense
                // param1 = neurons
                let inDim = 1;
                for (const d of currentShape) inDim *= d;

                layer.inputDim = inDim; // Helper for weights
                layer.outputDim = param1;
                currentShape = [param1];

              } else if (type === 6) { // Conv2D
                // param1=Filters, p2=KH, p3=KW, p4=SH, p5=SW, p6=Pad
                // Input assumed 3D (C, H, W)
                // If 1D, infer (1, 1, W)
                if (currentShape.length === 1) {
                  currentShape = [1, 1, currentShape[0]];
                }

                const [c_in, h_in, w_in] = currentShape;
                const h_out = Math.floor((h_in + 2 * param6 - param2) / param4) + 1;
                const w_out = Math.floor((w_in + 2 * param6 - param3) / param5) + 1;

                layer.inputShape = [...currentShape];
                layer.outputShape = [param1, h_out, w_out]; // Channels First
                currentShape = layer.outputShape;
              } else if (type === 8) { // MaxPool
                const k_h = param2;
                const k_w = param3;
                const s_h = param4;
                const s_w = param5;
                const pad = param6;
                // Output H = ((H + 2P - Pool)/S) + 1
                if (currentShape.length === 1) currentShape = [1, 1, currentShape[0]];
                const [c_in, h_in, w_in] = currentShape;

                const h_out = Math.floor((h_in + 2 * pad - k_h) / s_h) + 1;
                const w_out = Math.floor((w_in + 2 * pad - k_w) / s_w) + 1;

                layer.inputShape = [...currentShape];
                layer.outputShape = [c_in, h_out, w_out];
                currentShape = layer.outputShape;

              } else if (type === 7) { // Flatten
                let size = 1;
                for (const d of currentShape) size *= d;
                currentShape = [size];
              }

              model.layers.push(layer);
            }

            // Output Size is product of final shape
            let outSize = 1;
            for (const d of currentShape) outSize *= d;
            model.outputSize = outSize;

            // Weights
            try {
              // Copy weights to Float32Array
              const wBytes = data.slice(ptr, ptr + totalWeights);
              model.weightsData = new Float32Array(wBytes.buffer, wBytes.byteOffset, wBytes.byteLength / 4);
              model.isLoaded = true;
            } catch (e) {
              this._log("V3 Parse Error: " + e.message);
            }

          }
        }
      }

      // Add or replace model
      const existing = this.models.findIndex(m => m.name === filename);
      if (existing >= 0) {
        this.models[existing] = model;
      } else {
        this.models.push(model);
      }

      // Auto-select uploaded model
      this.activeModel = this.models.findIndex(m => m.name === filename);

      this._log(`Model ${filename} stored. Layers:${model.layers.length}`);

      // TRIGGER SENTINEL GOD MODE if filename matches
      if (filename.includes('sentinel_god')) {
        this.startSentinelLoop();
      }

      return true;
    }

    // --- Real Inference Engine (JS) ---

    _parseWeights(buffer, inputSize, hiddenSize, outputSize) {
      // Legacy helper for V2
      const floats = new Float32Array(buffer.buffer, buffer.byteOffset, buffer.byteLength / 4);
      let ptr = 0;
      const w1 = floats.slice(ptr, ptr + inputSize * hiddenSize); ptr += inputSize * hiddenSize;
      const b1 = floats.slice(ptr, ptr + hiddenSize); ptr += hiddenSize;
      const w2 = floats.slice(ptr, ptr + hiddenSize * outputSize); ptr += hiddenSize * outputSize;
      const b2 = floats.slice(ptr, ptr + outputSize); ptr += outputSize;
      return { w1, b1, w2, b2, inputSize, hiddenSize, outputSize };
    }

    _computeInference(inputs, model) {
      // Input Validation
      if (!model.isLoaded) return new Array(model.outputSize).fill(0);

      let x = Float32Array.from(inputs);
      let weightsPtr = 0;

      for (const layer of model.layers) {
        if (layer.type === 1) {
          // Input: No-op
        } else if (layer.type === 2) {
          // Dense
          const inDim = x.length;
          const outDim = layer.outputDim || layer.param1;
          const y = new Float32Array(outDim);

          // Get Weights
          const wCount = inDim * outDim;
          const w = model.weightsData.subarray(weightsPtr, weightsPtr + wCount);
          weightsPtr += wCount;

          const b = model.weightsData.subarray(weightsPtr, weightsPtr + outDim);
          weightsPtr += outDim;

          // MatMul
          for (let o = 0; o < outDim; o++) {
            let sum = b[o];
            for (let i = 0; i < inDim; i++) {
              sum += x[i] * w[o * inDim + i];
            }
            y[o] = sum;
          }
          x = y;

        } else if (layer.type === 3) {
          // ReLU
          for (let i = 0; i < x.length; i++) x[i] = Math.max(0, x[i]);

        } else if (layer.type === 4) {
          // Sigmoid
          for (let i = 0; i < x.length; i++) x[i] = 1.0 / (1.0 + Math.exp(-x[i]));

        } else if (layer.type === 5) {
          // Softmax
          let maxVal = -Infinity;
          for (let i = 0; i < x.length; i++) if (x[i] > maxVal) maxVal = x[i];
          let sum = 0;
          for (let i = 0; i < x.length; i++) {
            x[i] = Math.exp(x[i] - maxVal);
            sum += x[i];
          }
          for (let i = 0; i < x.length; i++) x[i] /= sum;

        } else if (layer.type === 6) {
          // Conv2D
          const filters = layer.param1;
          const kh = layer.param2;
          const kw = layer.param3;
          const sh = layer.param4;
          const sw = layer.param5;
          const pad = layer.param6;
          const [c_in, h_in, w_in] = layer.inputShape;
          const [c_out, h_out, w_out] = layer.outputShape;

          const y = new Float32Array(c_out * h_out * w_out);

          // Weights: [c_out][c_in][kh][kw]
          const wCount = c_out * c_in * kh * kw;
          const w = model.weightsData.subarray(weightsPtr, weightsPtr + wCount);
          weightsPtr += wCount;

          const b = model.weightsData.subarray(weightsPtr, weightsPtr + c_out);
          weightsPtr += c_out;

          // Flatten Helper Indices
          const x_idx = (c, h, w) => c * (h_in * w_in) + h * w_in + w;
          const w_idx = (co, ci, y, x) => co * (c_in * kh * kw) + ci * (kh * kw) + y * kw + x;
          const y_idx = (co, h, w) => co * (h_out * w_out) + h * w_out + w;

          for (let co = 0; co < c_out; co++) {
            const bias = b[co];
            for (let h = 0; h < h_out; h++) {
              for (let w_o = 0; w_o < w_out; w_o++) {
                let sum = bias;

                // Input patch
                const h_start = h * sh - pad;
                const w_start = w_o * sw - pad;

                for (let ci = 0; ci < c_in; ci++) {
                  for (let dy = 0; dy < kh; dy++) {
                    for (let dx = 0; dx < kw; dx++) {
                      const iy = h_start + dy;
                      const ix = w_start + dx;

                      if (iy >= 0 && iy < h_in && ix >= 0 && ix < w_in) {
                        const val = x[x_idx(ci, iy, ix)];
                        const weight = w[w_idx(co, ci, dy, dx)];
                        sum += val * weight;
                      }
                    }
                  }
                }
                y[y_idx(co, h, w_o)] = sum;
              }
            }
          }
          x = y;

        } else if (layer.type === 7) {
          // Flatten (Virtual)
          // No-op for 1D float array

        } else if (layer.type === 8) {
          // MaxPool (1D/2D)
          const kh = layer.param2;
          const kw = layer.param3;
          const sh = layer.param4;
          const sw = layer.param5;
          const pad = layer.param6;

          const [c_in, h_in, w_in] = layer.inputShape || [1, 1, x.length];
          const h_out = Math.floor((h_in + 2 * pad - kh) / sh) + 1;
          const w_out = Math.floor((w_in + 2 * pad - kw) / sw) + 1;

          const y = new Float32Array(c_in * h_out * w_out);

          const x_idx = (c, h, w) => c * (h_in * w_in) + h * w_in + w;
          const y_idx = (c, h, w) => c * (h_out * w_out) + h * w_out + w;

          for (let c = 0; c < c_in; c++) {
            for (let h = 0; h < h_out; h++) {
              for (let w = 0; w < w_out; w++) {
                let maxVal = -Infinity;

                const h_start = h * sh - pad;
                const w_start = w * sw - pad;

                for (let dy = 0; dy < kh; dy++) {
                  for (let dx = 0; dx < kw; dx++) {
                    const iy = h_start + dy;
                    const ix = w_start + dx;

                    if (iy >= 0 && iy < h_in && ix >= 0 && ix < w_in) {
                      const val = x[x_idx(c, iy, ix)];
                      if (val > maxVal) maxVal = val;
                    }
                  }
                }
                if (maxVal === -Infinity) maxVal = 0;
                y[y_idx(c, h, w)] = maxVal;
              }
            }
          }
          x = y;
        }
      }

      return Array.from(x);
    }

    startSentinelLoop() {
      if (this.sentinelLoopId) clearInterval(this.sentinelLoopId);

      this._log("SENTINEL: God Mode Activated.");
      this._log("SENTINEL: Core 1 (Reflex) Online.");
      this._log("SENTINEL: Core 0 (Brain) Online.");

      // Run faster to match real-time webcam (2FPS)
      this.sentinelLoopId = setInterval(() => {
        this._runSentinelCycle();
      }, 500);
    }

    async _runSentinelCycle() {
      // 1. Get Input Source (Webcam OR Test Image)
      const webcam = document.getElementById('webcam');
      const testImage = document.getElementById('test-image');

      let sourceElement = webcam;
      // If test image is loaded and visible, use it
      if (testImage && testImage.style.display !== 'none' && testImage.src) {
        sourceElement = testImage;
      } else if (!webcam || (webcam.readyState !== 4 && !webcam.src)) {
        // Fallback if no input (check src for video file, readyState for stream)
        if (Math.random() < 0.05) this._log("[STATUS] Searching for sensor input...");
        return;
      }

      this.currentSource = sourceElement; // Save for Renderer

      // 2. Load Vision Model (Once)
      if (!this.visionModel) {
        if (!window.cocoSsd) return; // Wait for script load
        this._log("SENTINEL: Loading Vision Neural Network...");
        try {
          this.visionModel = await cocoSsd.load();
          this._log("SENTINEL: Vision Cortex Online.");
        } catch (e) {
          this._log("SENTINEL: Vision Init Failed: " + e.message);
          return;
        }
      }

      // 3. Inference (Real)
      let predictions = [];
      try {
        predictions = await this.visionModel.detect(sourceElement);
      } catch (e) { /* ignore frame error */ }

      // --- RENDER UI TO FRAMEBUFFER ---
      // Clear display
      this.framebuffer.fill(0);

      // 4. Sentinel Logic
      if (predictions.length > 0) {
        // Found something interesting!
        const best = predictions[0];
        const label = best.class.charAt(0).toUpperCase() + best.class.slice(1);
        const conf = Math.floor(best.score * 100);

        // Trigger Reflex
        if (!this.sentinelState.reflexActive) {
          this._log("[SENTINEL] Reflex Triggered! Object Motion Detected.");
          this.sentinelState.reflexActive = true;
          this.sentinelState.freq = 250;
        }

        // Draw Bounding Box (Scaled to 128x64)
        // video is usually 640x480 or similar.
        const scaleX = 128 / sourceElement.videoWidth || 128 / sourceElement.naturalWidth || 1;
        const scaleY = 64 / sourceElement.videoHeight || 64 / sourceElement.naturalHeight || 1;

        const param = {
          x: Math.floor(best.bbox[0] * scaleX),
          y: Math.floor(best.bbox[1] * scaleY),
          w: Math.floor(best.bbox[2] * scaleX),
          h: Math.floor(best.bbox[3] * scaleY)
        };

        this._drawRect(param.x, param.y, param.w, param.h, 1);

        // Draw Label
        this._drawText(label, param.x, Math.max(0, param.y - 8));
        this._drawText(`${conf}%`, param.x, param.y + param.h + 2);

        this._log(`[VISION] Class: '${label}' (${conf}%)`);

        // --- CONNECT VISION TO BRAIN (REAL MATH) ---
        // The Hardware Sentinel receives a 128-float vector from the Host.
        // Here we simulate that extraction by converting BBox -> 128 floats.
        const featureVector = new Float32Array(128);
        // Fill with normalized bbox data to be deterministic
        const nx = best.bbox[0] / sourceElement.videoWidth;
        const ny = best.bbox[1] / sourceElement.videoHeight;
        const nw = best.bbox[2] / sourceElement.videoWidth;
        const nh = best.bbox[3] / sourceElement.videoHeight;

        // Pattern: [x, y, w, h, x, y, w, h...]
        for (let i = 0; i < 128; i += 4) {
          featureVector[i] = nx;
          featureVector[i + 1] = ny;
          featureVector[i + 2] = nw;
          featureVector[i + 3] = nh;
        }

        // Find the loaded Sentinel Model
        const sentinelModelIdx = this.models.findIndex(m => m.name.includes('sentinel_god'));
        if (sentinelModelIdx >= 0 && this.models[sentinelModelIdx].weights) {
          // Run Real Inference (JS Engine)
          const brainOutput = this._computeInference(featureVector, this.models[sentinelModelIdx]);

          // Output is 5 floats (Softmax probabilities)
          // Let's find the max class
          let maxIdx = 0;
          let maxScore = brainOutput[0];
          for (let i = 1; i < 5; i++) {
            if (brainOutput[i] > maxScore) {
              maxScore = brainOutput[i];
              maxIdx = i;
            }
          }

          const brainConf = (maxScore * 100).toFixed(1);
          const brainActions = ["IDLE", "TRACK", "ALERT", "SEARCH", "PING"];
          const action = brainActions[maxIdx];

          this._drawText(`BRAIN: ${action}`, 5, 5);
          this._drawText(`CONF: ${brainConf}%`, 5, 13);

          // this._log(`[BRAIN] Inferred Action: ${action} (${brainConf}%)`);
        } else {
          this._drawText("BRAIN: OFFLINE", 5, 5);
        }

      } else {
        // Nothing visible
        this.sentinelState.reflexActive = false;
        this.sentinelState.freq = 133;

        // Draw Scanning Text
        this._drawText("SCANNING...", 30, 28);
      }

      // Draw Stats Bar
      const tempStr = `${this.sentinelState.temp.toFixed(1)}C`;
      const freqStr = `${this.sentinelState.freq}MHz`;
      this._drawText(tempStr, 0, 56);
      this._drawText(freqStr, 80, 56);

      // Thermal Physics (Heat up if busy)
      if (this.sentinelState.freq > 200) this.sentinelState.temp += 0.2;
      else this.sentinelState.temp -= 0.1;

      // FLUSH to Canvas
      this._renderDisplay();
    }

    // --- Graphics Helpers ---

    _drawRect(imgX, imgY, w, h, color) {
      // Simple rect drawing
      for (let x = imgX; x < imgX + w; x++) {
        this._drawPixel(x, imgY, color);
        this._drawPixel(x, imgY + h, color);
      }
      for (let y = imgY; y < imgY + h; y++) {
        this._drawPixel(imgX, y, color);
        this._drawPixel(imgX + w, y, color);
      }
    }

    _drawPixel(x, y, color) {
      if (x < 0 || x >= 128 || y < 0 || y >= 64) return;
      const byteIndex = x + Math.floor(y / 8) * 128;
      const bit = y % 8;
      if (color) {
        this.framebuffer[byteIndex] |= (1 << bit);
      } else {
        this.framebuffer[byteIndex] &= ~(1 << bit);
      }
    }

    _drawText(text, x, y) {
      // Very simple 5x7 font rendering (mock)
      // We can't embed a full font here easily without bloating the file.
      // We'll just draw tiny rectangles representing chars or a placeholder.
      // ACTUALLY, let's just use Canvas API on the displayCtx for "Mock" Overlay if possible?
      // No, we want to simulate the framebuffer bits.
      // Let's implement a wrapper that draws to the canvas DIRECTLY for text 
      // since we want it to be readable, overlaying the pixel grid.
      // WAIT, the user wants to see what the DEVICE shows. The device shows pixels.
      // I will draw crude characters for A-Z.

      const font = {
        'A': [0x7E, 0x09, 0x09, 0x09, 0x7E],
        'B': [0x7F, 0x49, 0x49, 0x49, 0x36],
        'C': [0x3E, 0x41, 0x41, 0x41, 0x22],
        'S': [0x46, 0x49, 0x49, 0x49, 0x31],
        'C': [0x3E, 0x41, 0x41, 0x41, 0x22],
        'A': [0x7E, 0x09, 0x09, 0x09, 0x7E],
        'N': [0x7F, 0x08, 0x08, 0x08, 0x7F],
        // ... full font is too big.
      };

      // Fallback: Just draw a small box for each char
      let cursorX = x;
      for (let i = 0; i < text.length; i++) {
        // Draw a 3x5 box
        for (let dx = 0; dx < 3; dx++) {
          for (let dy = 0; dy < 5; dy++) {
            this._drawPixel(cursorX + dx, y + dy, 1);
          }
        }
        cursorX += 5;
      }
    }

    // Overriding _drawText to use Canvas Overlay for readability
    // This cheats a bit but makes the simulator much more usable
    _renderDisplay() {
      if (!this.displayCtx) return;

      const ctx = this.displayCtx;
      const scale = this.displayCanvas.width / 128;

      ctx.fillStyle = '#000';
      ctx.fillRect(0, 0, this.displayCanvas.width, this.displayCanvas.height);

      ctx.fillStyle = '#00bfff';
      for (let x = 0; x < 128; x++) {
        for (let y = 0; y < 64; y++) {
          const byteIndex = x + Math.floor(y / 8) * 128;
          const bit = y % 8;
          if (this.framebuffer[byteIndex] & (1 << bit)) {
            ctx.fillRect(x * scale, y * scale, scale, scale);
          }
        }
      }

      // Overlay Text helper (non-destructive to framebuffer, but persistent on screen)
      if (this.lastTextRequests && this.lastTextRequests.length) {
        ctx.fillStyle = '#00bfff';
        ctx.font = `${8 * scale}px monospace`;
        for (const req of this.lastTextRequests) {
          ctx.fillText(req.text, req.x * scale, (req.y + 8) * scale);
        }
      }
      this.lastTextRequests = []; // Clear frame
    }

    _drawText(text, x, y) {
      if (!this.lastTextRequests) this.lastTextRequests = [];
      this.lastTextRequests.push({ text, x, y });
    }

    // --- Command handler ---

    _handleCommand(cmd, payload) {
      switch (cmd) {
        // VERSION
        case 0x0F:
          // Manual construction with CRC
          return this._makeResponse(0x0F, 0x00, new Uint8Array([2, 2, 0])); // v2.2.0

        // CLEAR
        case 0x10:
          this.framebuffer.fill(payload[0] ? 0xFF : 0x00);
          this._renderDisplay();
          return this._ack(cmd);

        // PIXEL
        case 0x11: {
          const x = payload[0];
          const y = payload[1];
          const color = payload[2];
          if (x < 128 && y < 64) {
            const byteIndex = x + Math.floor(y / 8) * 128;
            const bit = y % 8;
            if (color) {
              this.framebuffer[byteIndex] |= (1 << bit);
            } else {
              this.framebuffer[byteIndex] &= ~(1 << bit);
            }
          }
          return this._ack(cmd);
        }

        // RECT
        case 0x12: {
          const [rx, ry, rw, rh, rc] = payload;
          for (let dy = 0; dy < rh && ry + dy < 64; dy++) {
            for (let dx = 0; dx < rw && rx + dx < 128; dx++) {
              const x = rx + dx;
              const y = ry + dy;
              const byteIndex = x + Math.floor(y / 8) * 128;
              const bit = y % 8;
              if (rc) {
                this.framebuffer[byteIndex] |= (1 << bit);
              } else {
                this.framebuffer[byteIndex] &= ~(1 << bit);
              }
            }
          }
          return this._ack(cmd);
        }

        // TEXT
        case 0x21:
          return this._ack(cmd);

        // FLUSH
        case 0x2F:
          this._renderDisplay();
          return this._ack(cmd);

        // AI INFER
        case 0x50: {
          if (!this.aiState.model_loaded) {
            return this._nak(cmd);
          }

          const model = this.models[this.activeModel] || this.models[0];
          const inSize = model.inputSize || 2;
          const outSize = model.outputSize || 1;

          // Read inputs (Variable size)
          const inputs = [];
          for (let i = 0; i < inSize; i++) {
            inputs.push(this._readFloat(payload, i * 4));
          }

          // Simulate Inference
          let results = [];

          // REAL SIMULATION: If we have weights, run the math!
          if (model.weights || model.isLoaded) {
            results = this._computeInference(inputs, model);
          } else {
            // Fallback for legacy/uninitialized models
            if (model.name.includes('xor')) {
              const val = this._simulateXOR(inputs[0], inputs[1]);
              results.push(val);
            } else {
              for (let i = 0; i < outSize; i++) results.push(Math.random());
            }
          }

          // Pack Response
          const resp = new Uint8Array(outSize * 4);
          const view = new DataView(resp.buffer);
          for (let i = 0; i < outSize; i++) {
            view.setFloat32(i * 4, results[i] || 0.0, true);
          }

          return this._dataResponse(cmd, resp);
        }

        // AI TRAIN
        case 0x51: {
          // Payload: [Input Floats] [Target Floats]
          if (!this.aiState.model_loaded) return this._error(cmd);

          const model = this.models[this.activeModel];
          if (!model) return this._error(cmd);

          // Parse Payload based on Model IO
          // We need exact dimensions.
          const inSize = model.inputSize;
          const outSize = model.outputSize;
          const expectedBytes = (inSize + outSize) * 4;

          if (payload.length < expectedBytes) return this._error(cmd);

          const inputs = [];
          const targets = [];
          const view = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);

          let ptr = 0;
          for (let i = 0; i < inSize; i++) { inputs.push(view.getFloat32(ptr, true)); ptr += 4; }
          for (let i = 0; i < outSize; i++) { targets.push(view.getFloat32(ptr, true)); ptr += 4; }

          // Run Training Step
          // Use a small learning rate default or global?
          // CMD_FINETUNE_START (0x65) sets it?
          // Let's assume 0.1 if not set.
          const lr = this.aiState.lr || 0.1;

          const loss = this._trainStep(model, inputs, targets, lr);

          this.aiState.last_loss = loss;
          this.aiState.epochs++;

          const lossBytes = new Uint8Array(4);
          new DataView(lossBytes.buffer).setFloat32(0, loss, true);
          return this._dataResponse(cmd, lossBytes);
        }

        // AI STATUS
        case 0x52: {
          const statusData = new Uint8Array(8);
          const sv = new DataView(statusData.buffer);
          statusData[0] = this.aiState.state;
          statusData[1] = this.aiState.model_loaded ? 1 : 0;
          sv.setUint16(2, this.aiState.epochs, true);
          sv.setFloat32(4, this.aiState.last_loss, true);
          return this._dataResponse(cmd, statusData);
        }

        // AI SAVE
        case 0x53: {
          const saveName = this._readString(payload);
          const existing = this.models.findIndex(m => m.name === saveName);
          if (existing < 0) {
            this.models.push({ name: saveName, size: 192, inputSize: 2, outputSize: 1, hiddenSize: 8 });
          }
          this._log(`Saved model: ${saveName}`);
          return this._ack(cmd);
        }

        // AI LOAD
        case 0x54: {
          const loadName = this._readString(payload);
          const model = this.models.find(m => m.name === loadName);
          if (model) {
            this.aiState.model_loaded = true;
            this._log(`Loaded model: ${loadName}`);
            return this._ack(cmd);
          }
          return this._notFound(cmd);
        }

        // AI LIST
        case 0x55: {
          const names = this.models.map(m => m.name);
          const parts = [];
          for (const name of names) {
            parts.push(name.length);
            for (let i = 0; i < name.length; i++) {
              parts.push(name.charCodeAt(i));
            }
          }
          parts.push(0); // terminator
          return this._dataResponse(cmd, new Uint8Array(parts));
        }

        // AI DELETE
        case 0x56: {
          const delName = this._readString(payload);
          const idx = this.models.findIndex(m => m.name === delName);
          if (idx >= 0) {
            this.models.splice(idx, 1);
            this._log(`Deleted: ${delName}`);
            return this._ack(cmd);
          }
          return this._notFound(cmd);
        }

        // MODEL INFO
        case 0x60: {
          if (this.models.length === 0) return this._notFound(cmd);
          const m = this.models[this.activeModel] || this.models[0];
          const info = new Uint8Array(32);
          const iv = new DataView(info.buffer);
          iv.setUint32(0, 0x54525053, true); // SPRT
          info[6] = m.inputSize || 2;
          info[7] = m.outputSize || 1;
          info[8] = m.hiddenSize || 8;
          // Write name
          const nameStr = m.name;
          for (let i = 0; i < nameStr.length && i < 16; i++) {
            info[16 + i] = nameStr.charCodeAt(i);
          }
          return this._dataResponse(cmd, info);
        }

        // MODEL LIST
        case 0x61:
          return this._handleCommand(0x55, payload); // Same as AI LIST

        // MODEL SELECT
        case 0x62: {
          const selName = this._readString(payload);
          const selIdx = this.models.findIndex(m => m.name === selName);
          if (selIdx >= 0) {
            this.activeModel = selIdx;
            this._log(`Selected: ${selName}`);
            return this._ack(cmd);
          }
          return this._notFound(cmd);
        }

        // MODEL UPLOAD
        case 0x63:
          // Handled by uploadModel() method usually called directly by UI? 
          // No, UI calls sendCommand for everything except upload?
          // Wait, uploadModel is a separate method in this class.
          // It is NOT called via sendCommand usually.
          // But if it IS called via simulated packet?
          // Let's assume ack for now.
          return this._ack(cmd);

        // MODEL DELETE
        case 0x64:
          return this._handleCommand(0x56, payload);

        // FINETUNE START (Set LR)
        case 0x65: {
          // Payload: [LR (float)]
          if (payload.length >= 4) {
            const lr = new DataView(payload.buffer, payload.byteOffset, payload.byteLength).getFloat32(0, true);
            this.aiState.lr = lr;
            this._log(`Finetune Start: LR set to ${lr}`);
          } else {
            this.aiState.lr = 0.01; // Default
            this._log(`Finetune Start: Default LR 0.01`);
          }
          return this._ack(cmd);
        }

        // FINETUNE DATA/STOP
        case 0x66:
        case 0x67:
          this._log('Finetune Data/Stop: stub acknowledged');
          return this._ack(cmd);

        // CHUNKED UPLOAD COMMANDS (MOCKED)
        // If the simulator receives these packets directly (e.g. from a test script?), 
        // we should handle them.
        // But usually `uploadModel` handles logic.
        // Let's just ACK them to match firmware behavior if probed.
        case 0x68: // CHUNK
        case 0x69: // END
          return this._ack(cmd);

        // ─── Open API Primitives (0xA0–0xA7) ────────────────────────────────

        // CMD_WHO_IS_THERE: return unique 8-byte device ID
        case 0xA0:
          return this._dataResponse(cmd, this.deviceId);

        // CMD_PING_ID: ACK only if payload matches this device's ID
        case 0xA1: {
          if (payload.length >= 8 && this._idsMatch(payload.subarray(0, 8)))
            return this._ack(cmd);
          return this._nak(cmd);
        }

        // CMD_BUFFER_WRITE: push one little-endian float32 into the circular buffer
        case 0xA2: {
          if (payload.length < 4) return this._nak(cmd);
          const sample = new DataView(payload.buffer, payload.byteOffset, 4).getFloat32(0, true);
          if (!isFinite(sample)) return this._nak(cmd);
          this.circularBuffer.push(sample);
          if (this.circularBuffer.length > 60) this.circularBuffer.shift();
          return this._ack(cmd);
        }

        // CMD_BUFFER_SNAPSHOT: return all buffered samples as packed float32s
        case 0xA3: {
          const out = new Uint8Array(this.circularBuffer.length * 4);
          const dv = new DataView(out.buffer);
          for (let i = 0; i < this.circularBuffer.length; i++)
            dv.setFloat32(i * 4, this.circularBuffer[i], true);
          return this._dataResponse(cmd, out);
        }

        // CMD_BASELINE_CAPTURE: freeze mean of current buffer as baseline
        case 0xA4: {
          if (this.circularBuffer.length === 0) return this._nak(cmd);
          this.baseline = this.circularBuffer.reduce((s, v) => s + v, 0) / this.circularBuffer.length;
          const out = new Uint8Array(4);
          new DataView(out.buffer).setFloat32(0, this.baseline, true);
          return this._dataResponse(cmd, out);
        }

        // CMD_BASELINE_RESET: clear baseline
        case 0xA5:
          this.baseline = null;
          return this._ack(cmd);

        // CMD_GET_DELTA: return |live_mean - baseline|
        case 0xA6: {
          if (this.baseline === null || this.circularBuffer.length === 0) return this._nak(cmd);
          const liveMean = this.circularBuffer.reduce((s, v) => s + v, 0) / this.circularBuffer.length;
          const out = new Uint8Array(4);
          new DataView(out.buffer).setFloat32(0, Math.abs(liveMean - this.baseline), true);
          return this._dataResponse(cmd, out);
        }

        // CMD_CORRELATE: normalized cross-correlation of live buffer vs reference payload
        case 0xA7: {
          if (payload.length < 4 || this.circularBuffer.length === 0) return this._nak(cmd);
          const refCount = Math.floor(payload.length / 4);
          const ref = [];
          const dv2 = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
          for (let i = 0; i < refCount; i++) ref.push(dv2.getFloat32(i * 4, true));
          const score = this._normalizedCrossCorr(this.circularBuffer, ref);
          const out = new Uint8Array(4);
          new DataView(out.buffer).setFloat32(0, score, true);
          return this._dataResponse(cmd, out);
        }

        default:
          this._log(`Unknown command: 0x${cmd.toString(16)}`);
          return this._nak(cmd);
      }
    }

    // --- Display rendering ---

    attachDisplay(canvasElement) {
      this.displayCanvas = canvasElement;
      this.displayCtx = canvasElement.getContext('2d');
      this._renderDisplay();
    }

    _renderDisplay() {
      if (!this.displayCtx) return;

      const ctx = this.displayCtx;
      const scale = this.displayCanvas.width / 128;

      ctx.fillStyle = '#000';
      ctx.fillRect(0, 0, this.displayCanvas.width, this.displayCanvas.height);

      ctx.fillStyle = '#00bfff';
      for (let x = 0; x < 128; x++) {
        for (let y = 0; y < 64; y++) {
          const byteIndex = x + Math.floor(y / 8) * 128;
          const bit = y % 8;
          if (this.framebuffer[byteIndex] & (1 << bit)) {
            ctx.fillRect(x * scale, y * scale, scale, scale);
          }
        }
      }

      // Overlay Text helper (non-destructive to framebuffer, but persistent on screen)
      if (this.lastTextRequests && this.lastTextRequests.length) {
        ctx.fillStyle = '#00bfff';
        ctx.font = `${8 * scale}px monospace`;
        for (const req of this.lastTextRequests) {
          ctx.fillText(req.text, req.x * scale, (req.y + 8) * scale);
        }
      }
      this.lastTextRequests = []; // Clear frame
    }

    // --- Helpers ---

    _captureGrayscale(video, width, height) {
      if (!this.captureCanvas) {
        this.captureCanvas = document.createElement('canvas');
      }
      this.captureCanvas.width = width;
      this.captureCanvas.height = height;
      const ctx = this.captureCanvas.getContext('2d');

      // Draw and Resize
      ctx.drawImage(video, 0, 0, width, height);

      // Get Pixels
      const imgData = ctx.getImageData(0, 0, width, height);
      const data = imgData.data;
      const floats = new Float32Array(width * height);

      // Convert RGB to Grayscale Float [0..1]
      for (let i = 0; i < floats.length; i++) {
        const r = data[i * 4];
        const g = data[i * 4 + 1];
        const b = data[i * 4 + 2];
        const avg = (r + g + b) / 3.0; // Simple average
        floats[i] = avg / 255.0;
      }
      return floats;
    }

    _simulateXOR(a, b) {
      // Simulate a trained XOR network
      const noise = (Math.random() - 0.5) * 0.04;
      const expected = (a > 0.5) !== (b > 0.5) ? 1 : 0;
      return Math.max(0, Math.min(1, expected + noise));
    }

    _readFloat(data, offset) {
      if (data.length < offset + 4) return 0;
      return new DataView(data.buffer, data.byteOffset + offset).getFloat32(0, true);
    }

    _readString(data) {
      let str = '';
      for (let i = 0; i < data.length && data[i] !== 0; i++) {
        str += String.fromCharCode(data[i]);
      }
      return str;
    }

    async _simulateLatency() {
      const ms = this.latency.min + Math.random() * (this.latency.max - this.latency.min);
      await new Promise(r => setTimeout(r, ms));
    }

    // --- Helper Methods ---

    _ack(cmd) {
      return this._makeResponse(cmd, 0x00, new Uint8Array(0));
    }

    _nak(cmd) {
      return this._makeResponse(cmd, 0x01, new Uint8Array(0));
    }

    _notFound(cmd) {
      return this._makeResponse(cmd, 0x02, new Uint8Array(0));
    }

    _dataResponse(cmd, data) {
      return this._makeResponse(cmd, 0x00, data);
    }

    _makeResponse(cmd, status, data) {
      const len = data.length;
      // Protocol: HEADER(1) CMD(1) STATUS(1) LEN(1) DATA(N) CRC(4)
      const resp = new Uint8Array(4 + len + 4);
      resp[0] = 0xAA;
      resp[1] = cmd;
      resp[2] = status;
      resp[3] = len;
      resp.set(data, 4);

      // CRC over CMD+STATUS+LEN+DATA (bytes 1 to end-4)
      const crcPart = resp.subarray(1, 4 + len);
      const crc = crc32(crcPart);

      // Write CRC at end
      const dv = new DataView(resp.buffer);
      // Offset is 4+len
      // Note: DataView works on the underlying ArrayBuffer.
      // If resp is a subarray (it's not here), we need to be careful.
      // But resp is new Uint8Array(size). offset is 0.
      dv.setUint32(4 + len, crc, true);

      return resp;
    }

    // ─── API Primitive Helpers ────────────────────────────────────────────

    /** Generate a random 8-byte Uint8Array as this device's unique identity. */
    _generateDeviceId() {
      const id = new Uint8Array(8);
      if (typeof crypto !== 'undefined' && crypto.getRandomValues) {
        crypto.getRandomValues(id);
      } else {
        for (let i = 0; i < 8; i++) id[i] = Math.floor(Math.random() * 256);
      }
      return id;
    }

    /** Return true if `incoming` (Uint8Array) byte-matches this device's ID. */
    _idsMatch(incoming) {
      if (incoming.length < 8) return false;
      for (let i = 0; i < 8; i++) {
        if (this.deviceId[i] !== incoming[i]) return false;
      }
      return true;
    }

    /**
     * Normalized cross-correlation between arrays a and b.
     * Returns a score in [0.0, 1.0] where 1.0 = perfect match.
     */
    _normalizedCrossCorr(a, b) {
      const len = Math.min(a.length, b.length);
      if (len === 0) return 0;
      let mA = 0, mB = 0;
      for (let i = 0; i < len; i++) { mA += a[i]; mB += b[i]; }
      mA /= len; mB /= len;
      let num = 0, dA = 0, dB = 0;
      for (let i = 0; i < len; i++) {
        const da = a[i] - mA, db = b[i] - mB;
        num += da * db; dA += da * da; dB += db * db;
      }
      const denom = Math.sqrt(dA * dB);
      if (denom === 0) return 1.0; // flat/identical signals
      return Math.max(0, Math.min(1, (num / denom + 1) / 2));
    }

    _log(msg) {
      console.log(`[MockDevice] ${msg}`);
    }

    // --- Training Engine (Backprop) ---

    _mapWeights(model) {
      if (model.weightsMapped) return;
      let ptr = 0;
      for (const layer of model.layers) {
        layer.wOffset = -1;
        layer.bOffset = -1;
        layer.wCount = 0;
        layer.bCount = 0;

        if (layer.type === 2) { // Dense
          const inDim = layer.inputDim; // Calculated during load?
          const outDim = layer.outputDim;

          // We need to re-calculate inputDim if not saved
          // But _parseV3Model saves it.
          if (!inDim) { /* Should error but assume 1? */ }

          layer.wOffset = ptr;
          layer.wCount = inDim * outDim;
          ptr += layer.wCount;

          layer.bOffset = ptr;
          layer.bCount = outDim;
          ptr += layer.bCount;
        } else if (layer.type === 6) { // Conv2D
          // filters * in * k * k
          // We need exact sizing. 
          // Assuming logic matches _computeInference
          const [c_in] = layer.inputShape;
          const filters = layer.param1;
          const kh = layer.param2;
          const kw = layer.param3;

          layer.wOffset = ptr;
          layer.wCount = filters * c_in * kh * kw;
          ptr += layer.wCount;

          layer.bOffset = ptr;
          layer.bCount = filters;
          ptr += layer.bCount;
        }
      }
      model.weightsMapped = true;
    }

    _trainStep(model, inputs, targets, lr) {
      this._mapWeights(model);
      const acts = []; // Activations
      let x = Float32Array.from(inputs);
      acts.push(x); // Input is act[0]

      // 1. Forward Pass (Cache)
      for (const layer of model.layers) {
        if (layer.type === 1) { // Input
          // No-op (Identity)
          acts.push(x);
        } else if (layer.type === 2) { // Dense
          const w = model.weightsData.subarray(layer.wOffset, layer.wOffset + layer.wCount);
          const b = model.weightsData.subarray(layer.bOffset, layer.bOffset + layer.bCount);
          const inDim = layer.inputDim;
          const outDim = layer.outputDim;
          const y = new Float32Array(outDim);

          for (let o = 0; o < outDim; o++) {
            let sum = b[o];
            for (let i = 0; i < inDim; i++) sum += x[i] * w[o * inDim + i];
            y[o] = sum;
          }
          x = y;
          acts.push(x);
        } else if (layer.type === 3) { // ReLU
          const y = new Float32Array(x.length);
          for (let i = 0; i < x.length; i++) y[i] = Math.max(0, x[i]);
          x = y;
          acts.push(x);
        } else if (layer.type === 4) { // Sigmoid
          const y = new Float32Array(x.length);
          for (let i = 0; i < x.length; i++) y[i] = 1.0 / (1.0 + Math.exp(-x[i]));
          x = y;
          acts.push(x);
        } else {
          // Unsupported layer for training (pass through)
          acts.push(x);
        }
      }

      // 2. Compute Loss (MSE)
      const output = x;
      let loss = 0;
      const grad = new Float32Array(output.length); // dL/dy
      for (let i = 0; i < output.length; i++) {
        const err = output[i] - targets[i];
        loss += val_sq(err);
        grad[i] = 2 * err; // MSE deriv: 2(y - t)
      }
      loss /= output.length;

      // 3. Backward Pass
      let g = grad;

      for (let i = model.layers.length - 1; i >= 0; i--) {
        const layer = model.layers[i];
        const input = acts[i]; // Input to this layer (output of prev)
        // Output of this layer is acts[i+1] (which corresponds to g)

        if (layer.type === 2) { // Dense
          const inDim = layer.inputDim;
          const outDim = layer.outputDim;

          // Gradient for Input (to pass down)
          const gInput = new Float32Array(inDim);

          const wStart = layer.wOffset;
          const w = model.weightsData; // Raw access for update

          for (let o = 0; o < outDim; o++) {
            const go = g[o];
            // Update Bias
            model.weightsData[layer.bOffset + o] -= lr * go;

            // Update Weights & Accumulate gInput
            for (let k = 0; k < inDim; k++) {
              const wIdx = wStart + o * inDim + k;
              gInput[k] += go * w[wIdx]; // Backprop
              w[wIdx] -= lr * go * input[k]; // Update
            }
          }
          g = gInput;

        } else if (layer.type === 3) { // ReLU
          // g = g * (input > 0 ? 1 : 0)
          for (let k = 0; k < g.length; k++) {
            if (input[k] <= 0) g[k] = 0;
          }
        } else if (layer.type === 4) { // Sigmoid
          // g = g * sig * (1 - sig)
          // We have output in acts[i+1] which IS sigmoid(input)
          const out = acts[i + 1];
          for (let k = 0; k < g.length; k++) {
            g[k] = g[k] * out[k] * (1 - out[k]);
          }
        }
      }

      return loss;
    }
  }

  function val_sq(x) { return x * x; }

  // Export to global scope
  if (typeof window !== 'undefined') window.MockSpriteDevice = MockSpriteDevice;
  if (typeof global !== 'undefined') global.MockSpriteDevice = MockSpriteDevice;

} // end duplicate-load guard
