/**
 * Mock Sprite Device
 * Simulates device responses for testing, development, and demos.
 * Implements the same interface as SpriteDevice but runs entirely in-browser.
 */

class MockSpriteDevice {
  constructor(name = 'Mock Sprite One') {
    this.displayName = name;
    this.isConnected = false;
    this.version = 'v2.0.0';
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
      await this._simulateLatency();
    }

    // Parse V2 Header
    const model = { name: filename, size: data.length };

    // Default fallback
    model.inputSize = 2;
    model.outputSize = 1;
    model.hiddenSize = 8;

    if (data.length >= 32) {
      const dv = new DataView(data.buffer, data.byteOffset, data.length);
      const magic = dv.getUint32(0, true);

      if (magic === 0x54525053) { // "SPRT"
        model.version = dv.getUint16(4, true);
        model.inputSize = dv.getUint8(6);
        model.outputSize = dv.getUint8(7);
        model.hiddenSize = dv.getUint8(8);
        model.modelType = dv.getUint8(9);
        model.numLayers = dv.getUint8(10);
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

    this._log(`Model ${filename} stored. IN:${model.inputSize} OUT:${model.outputSize} LAYERS:${model.numLayers || '?'}`);

    // TRIGGER SENTINEL GOD MODE if filename matches
    if (filename.includes('sentinel_god')) {
      this.startSentinelLoop();
    }

    return true;
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

      // Fake depth logic
      const depth = (100 / (best.bbox[2] + 1)).toFixed(2);
      // this._log(`[DEPTH] Est. Distance: ${depth}m`);

      // Vector Brain Logic
      if (best.score > 0.6) {
        // Recognized
      } else {
        this.sentinelState.memories++;
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
        return new Uint8Array([0xAA, 0x0F, 0x00, 2, 0, 0]); // v2.0.0

      // CLEAR
      case 0x10:
        this.framebuffer.fill(payload[0] ? 0xFF : 0x00);
        this._renderDisplay();
        return this._ack();

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
        return this._ack();
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
        return this._ack();
      }

      // TEXT
      case 0x21:
        // Accept and ACK (text rendering is complex, just acknowledge)
        return this._ack();

      // FLUSH
      case 0x2F:
        this._renderDisplay();
        return this._ack();

      // AI INFER
      case 0x50: {
        if (!this.aiState.model_loaded) {
          return this._nak();
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

        // Specific Simulation Logic based on known models
        if (model.name.includes('xor')) {
          const val = this._simulateXOR(inputs[0], inputs[1]);
          results.push(val);
        } else if (model.name.includes('sentinel')) {
          // Sentinel Output: [X, Y, W, H, Class] (5 floats)
          // Just return Mock Data
          results = [0.5, 0.5, 0.2, 0.2, 0.99];
        } else {
          // Generic Random/Pass-through for unknown models
          for (let i = 0; i < outSize; i++) {
            results.push(Math.random());
          }
        }

        // Pack Response
        const resp = new Uint8Array(outSize * 4);
        const view = new DataView(resp.buffer);
        for (let i = 0; i < outSize; i++) {
          view.setFloat32(i * 4, results[i] || 0.0, true);
        }

        return this._dataResponse(resp);
      }

      // AI TRAIN
      case 0x51: {
        const epochs = payload[0] || 100;
        // Simulate training with decreasing loss
        this.aiState.epochs += epochs;
        this.aiState.last_loss = Math.max(0.001, this.aiState.last_loss * 0.5);
        this.aiState.model_loaded = true;
        const lossBytes = new Uint8Array(4);
        new DataView(lossBytes.buffer).setFloat32(0, this.aiState.last_loss, true);
        return this._dataResponse(lossBytes);
      }

      // AI STATUS
      case 0x52: {
        const statusData = new Uint8Array(8);
        const sv = new DataView(statusData.buffer);
        statusData[0] = this.aiState.state;
        statusData[1] = this.aiState.model_loaded ? 1 : 0;
        sv.setUint16(2, this.aiState.epochs, true);
        sv.setFloat32(4, this.aiState.last_loss, true);
        return this._dataResponse(statusData);
      }

      // AI SAVE
      case 0x53: {
        const saveName = this._readString(payload);
        const existing = this.models.findIndex(m => m.name === saveName);
        if (existing < 0) {
          this.models.push({ name: saveName, size: 192, inputSize: 2, outputSize: 1, hiddenSize: 8 });
        }
        this._log(`Saved model: ${saveName}`);
        return this._ack();
      }

      // AI LOAD
      case 0x54: {
        const loadName = this._readString(payload);
        const model = this.models.find(m => m.name === loadName);
        if (model) {
          this.aiState.model_loaded = true;
          this._log(`Loaded model: ${loadName}`);
          return this._ack();
        }
        return this._notFound();
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
        return this._dataResponse(new Uint8Array(parts));
      }

      // AI DELETE
      case 0x56: {
        const delName = this._readString(payload);
        const idx = this.models.findIndex(m => m.name === delName);
        if (idx >= 0) {
          this.models.splice(idx, 1);
          this._log(`Deleted: ${delName}`);
          return this._ack();
        }
        return this._notFound();
      }

      // MODEL INFO
      case 0x60: {
        if (this.models.length === 0) return this._notFound();
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
        return this._dataResponse(info);
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
          return this._ack();
        }
        return this._notFound();
      }

      // MODEL UPLOAD
      case 0x63:
        // Handled by uploadModel() method
        return this._ack();

      // MODEL DELETE
      case 0x64:
        return this._handleCommand(0x56, payload);

      // FINETUNE START/DATA/STOP
      case 0x65:
      case 0x66:
      case 0x67:
        this._log('Finetune: stub acknowledged');
        return this._ack();

      default:
        this._log(`Unknown command: 0x${cmd.toString(16)}`);
        return this._nak();
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

  _ack() {
    return new Uint8Array([0xAA, 0x00, 0x00, 0x00]);
  }

  _nak() {
    return new Uint8Array([0xAA, 0x01, 0x00, 0x00]);
  }

  _notFound() {
    return new Uint8Array([0xAA, 0x02, 0x00, 0x00]);
  }

  _dataResponse(data) {
    const resp = new Uint8Array(4 + data.length);
    resp[0] = 0xAA;
    resp[1] = 0x00; // OK
    resp[2] = data.length;
    resp.set(data, 3);
    return resp;
  }

  _log(msg) {
    console.log(`[MockDevice] ${msg}`);
  }
}

// Export
window.MockSpriteDevice = MockSpriteDevice;
