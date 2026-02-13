/**
 * Sprite Trainer App v2
 * Main application logic with enhanced features
 */

// Globals
let device = null;
let trainer = null;
let dataInput = null;
let devices = [];
let chart = null;
let modelVisualizer = null;
let importedModel = null;
let isTraining = false;
let mockMode = false;
let mockDevice = null;

// DOM Elements
const elements = {};

// Logging with levels
function log(message, type = '') {
    const line = document.createElement('div');
    line.className = 'log ' + type;

    const time = new Date().toLocaleTimeString('en-US', {
        hour12: false,
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });

    line.innerHTML = `<span class="timestamp">[${time}]</span> <span>${message}</span>`;
    elements.consoleOutput.appendChild(line);
    elements.consoleOutput.scrollTop = elements.consoleOutput.scrollHeight;

    // Also log to browser console
    console.log(`[${type || 'log'}] ${message}`);
}

// Device Management
async function scanDevices() {
    if (mockMode) {
        log('Using mock device', 'info');
        mockDevice = new MockSpriteDevice();
        // Hook up UI logging
        mockDevice._log = (msg) => log(msg, 'info');
        devices = [mockDevice];
        selectDevice(mockDevice);
        renderDevices();

        // Attach simulated display
        const displayPanel = document.getElementById('display-panel');
        const displayCanvas = document.getElementById('sim-display-canvas');
        if (displayPanel && displayCanvas) {
            displayPanel.style.display = 'block';
            mockDevice.attachDisplay(displayCanvas);
        }

        log('Mock device connected with ' + mockDevice.models.length + ' models', 'success');
        return;
    }

    log('Scanning for Sprite devices...', 'info');
    elements.deviceList.innerHTML = '<div class="empty-state">Scanning...</div>';

    try {
        devices = await SpriteDevice.scan();

        if (devices.length === 0) {
            elements.deviceList.innerHTML = '<div class="empty-state">No devices found. Click to add.</div>';
            elements.deviceList.onclick = requestDevice;
        } else {
            renderDevices();
            log(`Found ${devices.length} device(s)`, 'success');
        }
    } catch (err) {
        log('Scan error: ' + err.message, 'error');
        elements.deviceList.innerHTML = '<div class="empty-state">Scan failed</div>';
    }
}

async function requestDevice() {
    try {
        const newDevice = await SpriteDevice.requestPort();
        const isSprite = await newDevice.probe();
        if (isSprite) {
            devices.push(newDevice);
            renderDevices();
            log('Device added: ' + newDevice.getDisplayName(), 'success');
        } else {
            log('Not a Sprite device', 'error');
        }
    } catch (err) {
        if (err.message.includes('cancelled')) {
            log('Request cancelled', 'info');
        } else {
            log('Connection error: ' + err.message, 'error');
        }
    }
}

function renderDevices() {
    elements.deviceList.innerHTML = '';
    elements.deviceList.onclick = null;

    for (const dev of devices) {
        const isMock = dev instanceof MockSpriteDevice;
        const item = document.createElement('div');
        item.className = 'device-item' + (dev === device ? ' selected' : '');
        item.innerHTML = `
      <span>${dev.getDisplayName()}</span>
      <span class="port">${isMock ? 'MOCK' : (dev.portInfo?.usbProductId ? 'USB' : 'UART')}</span>
    `;
        item.onclick = () => selectDevice(dev);
        elements.deviceList.appendChild(item);
    }
}

function selectDevice(dev) {
    device = dev;
    renderDevices();
    updateStatus();
    elements.deployBtn.disabled = false;
    if (elements.convertDeployBtn && importedModel) {
        elements.convertDeployBtn.disabled = false;
    }
    log('Selected: ' + dev.getDisplayName(), 'info');
}

function updateStatus() {
    elements.statusDot.classList.remove('connected', 'mock');
    if (device) {
        if (device instanceof MockSpriteDevice) {
            elements.statusDot.classList.add('mock');
        } else {
            elements.statusDot.classList.add('connected');
        }
        elements.statusText.textContent = device.getDisplayName();
    } else {
        elements.statusText.textContent = 'No device';
    }
}

// Training
async function startTraining() {
    if (isTraining) return;

    const epochs = parseInt(document.getElementById('epochs').value) || 50;
    const lr = parseFloat(document.getElementById('lr').value) || 0.01;
    const batchSize = parseInt(document.getElementById('batch-size').value) || 32;
    const arch = document.getElementById('model-arch').value;

    const { inputs, labels, numClasses } = dataInput.getTrainingData();

    if (inputs.length === 0) {
        log('No training data. Add samples first.', 'error');
        return;
    }

    if (numClasses < 2) {
        log('Need at least 2 classes', 'error');
        return;
    }

    // Get hidden size from architecture
    const hiddenSizes = { 'mlp-small': 8, 'mlp': 16, 'mlp-large': 32 };
    const hiddenSize = hiddenSizes[arch] || 16;

    log(`Training: ${inputs.length} samples, ${numClasses} classes, ${epochs} epochs`, 'info');

    // Show UI elements
    isTraining = true;
    elements.progressSection.style.display = 'block';
    elements.chartContainer.style.display = 'block';
    elements.trainBtn.style.display = 'none';
    elements.stopBtn.style.display = 'inline-flex';
    elements.trainBtn.disabled = true;
    document.body.classList.add('training');

    // Reset chart
    chart.reset();

    const inputSize = inputs[0].length;
    trainer.createModel(arch.replace('-small', '').replace('-large', ''), inputSize, numClasses, hiddenSize);

    // Show model architecture
    showModelArchitecture(inputSize, numClasses, hiddenSize);

    trainer.onProgress = (progress) => {
        elements.progressFill.style.width = progress.progress + '%';
        elements.progressText.textContent = Math.round(progress.progress) + '%';
        elements.etaText.textContent = 'ETA: ' + progress.eta;
        elements.lossValue.textContent = progress.loss.toFixed(4);
        elements.accuracyValue.textContent = (progress.accuracy * 100).toFixed(1) + '%';
        elements.epochValue.textContent = progress.epoch + '/' + epochs;

        // Update chart
        chart.addPoint(progress.loss, progress.accuracy);
    };

    try {
        const result = await trainer.train(inputs, labels, { epochs, lr, batchSize });
        log(`Training complete! Loss: ${result.loss.toFixed(4)}, Accuracy: ${(result.accuracy * 100).toFixed(1)}%`, 'success');
        elements.deployBtn.disabled = !device;
        elements.downloadModelBtn.disabled = false;
    } catch (err) {
        log('Training error: ' + err.message, 'error');
    }

    isTraining = false;
    elements.trainBtn.style.display = 'inline-flex';
    elements.stopBtn.style.display = 'none';
    elements.trainBtn.disabled = false;
    document.body.classList.remove('training');
}

function stopTraining() {
    if (trainer && isTraining) {
        trainer.stop();
        log('Training stopped by user', 'warning');
    }
}

function showModelArchitecture(inputSize, outputSize, hiddenSize) {
    elements.modelPanel.style.display = 'block';
    modelVisualizer.renderFromModel('mlp', inputSize, outputSize, hiddenSize);
}

async function deployModel() {
    if (!device || !trainer) {
        log('No device or model', 'error');
        return;
    }

    log('Exporting model to AIFes format...', 'info');

    try {
        const modelData = trainer.exportToAIFes();
        log(`Model size: ${modelData.length} bytes`, 'info');

        log('Uploading to device...', 'info');
        const success = await device.uploadModel('trained.aif32', modelData);

        if (success) {
            log('Model deployed successfully!', 'success');
        } else {
            log('Upload failed', 'error');
        }
    } catch (err) {
        log('Deploy error: ' + err.message, 'error');
    }
}

function downloadTrainedModel() {
    if (!trainer) return;

    try {
        const modelData = trainer.exportToAIFes();
        downloadBlob(modelData, 'trained_model.aif32');
        log('Model downloaded', 'success');
    } catch (err) {
        log('Download error: ' + err.message, 'error');
    }
}

function downloadBlob(data, filename) {
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

// Data Input Tabs
function setupTabs() {
    const tabs = document.querySelectorAll('.tab');
    tabs.forEach(tab => {
        tab.addEventListener('click', () => {
            tabs.forEach(t => t.classList.remove('active'));
            tab.classList.add('active');

            document.querySelectorAll('.tab-content').forEach(c => c.style.display = 'none');
            const content = document.getElementById('tab-' + tab.dataset.tab);
            if (content) content.style.display = 'block';

            if (tab.dataset.tab === 'webcam') {
                dataInput.initWebcam(elements.webcam);
            }
        });
    });
}

function setupDrawCanvas() {
    const canvas = elements.drawCanvas;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    let drawing = false;

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, 128, 128);
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 6;
    ctx.lineCap = 'round';

    canvas.addEventListener('mousedown', (e) => {
        drawing = true;
        ctx.beginPath();
        ctx.moveTo(e.offsetX, e.offsetY);
    });

    canvas.addEventListener('mousemove', (e) => {
        if (drawing) {
            ctx.lineTo(e.offsetX, e.offsetY);
            ctx.stroke();
        }
    });

    canvas.addEventListener('mouseup', () => drawing = false);
    canvas.addEventListener('mouseout', () => drawing = false);

    document.getElementById('clear-canvas').onclick = () => {
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, 128, 128);
    };

    document.getElementById('add-sample').onclick = () => {
        const classLabel = parseInt(document.getElementById('draw-class').value) || 0;
        const data = dataInput.captureFromCanvas(canvas);
        dataInput.addSample(data, classLabel);
        updateSampleDisplay();
        log(`Added sample for class ${classLabel}`, 'info');

        // Clear canvas
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, 128, 128);
    };
}

function setupFileInput() {
    const dropZone = document.getElementById('drop-zone');
    const fileInput = document.getElementById('file-input');
    const browseBtn = document.getElementById('browse-btn');

    if (!browseBtn) return;

    browseBtn.onclick = () => fileInput.click();

    fileInput.onchange = async (e) => {
        for (const file of e.target.files) {
            const classLabel = prompt(`Class label for ${file.name}:`);
            if (classLabel !== null) {
                const data = await dataInput.loadFromFile(file);
                dataInput.addSample(data, parseInt(classLabel));
                updateSampleDisplay();
                log(`Added ${file.name} as class ${classLabel}`, 'info');
            }
        }
    };

    dropZone.ondragover = (e) => {
        e.preventDefault();
        dropZone.classList.add('dragover');
    };

    dropZone.ondragleave = () => dropZone.classList.remove('dragover');

    dropZone.ondrop = async (e) => {
        e.preventDefault();
        dropZone.classList.remove('dragover');

        for (const file of e.dataTransfer.files) {
            if (file.type.startsWith('image/')) {
                const classLabel = prompt(`Class label for ${file.name}:`);
                if (classLabel !== null) {
                    const data = await dataInput.loadFromFile(file);
                    dataInput.addSample(data, parseInt(classLabel));
                    updateSampleDisplay();
                    log(`Added ${file.name} as class ${classLabel}`, 'info');
                }
            }
        }
    };
}

function setupWebcamCapture() {
    const captureBtn = document.getElementById('capture-btn');
    if (!captureBtn) return;

    captureBtn.onclick = () => {
        const classLabel = parseInt(document.getElementById('capture-class').value) || 0;
        const data = dataInput.captureFromWebcam(elements.webcam, elements.captureCanvas);
        dataInput.addSample(data, classLabel);
        updateSampleDisplay();
        log(`Captured sample for class ${classLabel}`, 'info');
    };
}


function setupTestMedia() {
    const uploadInput = document.getElementById('test-media-upload');
    const resetBtn = document.getElementById('reset-cam-btn');
    const webcam = document.getElementById('webcam');
    const testImage = document.getElementById('test-image');

    if (!uploadInput) return;

    uploadInput.onchange = (e) => {
        const file = e.target.files[0];
        if (!file) return;

        if (dataInput && dataInput.stopWebcam) dataInput.stopWebcam(); // Stop live stream if any

        const url = URL.createObjectURL(file);

        if (file.type.startsWith('image/')) {
            webcam.style.display = 'none';
            testImage.style.display = 'block';
            testImage.src = url;
            testImage.dataset.isTest = "true";
        } else if (file.type.startsWith('video/')) {
            testImage.style.display = 'none';
            webcam.style.display = 'block';
            webcam.srcObject = null;
            webcam.src = url;
            webcam.loop = true;
            webcam.play();
        }

        resetBtn.style.display = 'inline-block';
        log(`Loaded test media: ${file.name}`, 'info');
    };

    resetBtn.onclick = () => {
        webcam.src = "";
        webcam.srcObject = null;
        testImage.style.display = 'none';
        webcam.style.display = 'block';
        if (dataInput && dataInput.initWebcam) dataInput.initWebcam(webcam);
        resetBtn.style.display = 'none';
        uploadInput.value = "";
        log('Switched back to live webcam', 'info');
    };
}

// Model Import
function setupModelImport() {
    const dropZone = document.getElementById('model-drop-zone');
    const fileInput = document.getElementById('model-input');
    const browseBtn = document.getElementById('model-browse-btn');
    const convertBtn = document.getElementById('convert-deploy-btn');
    const downloadBtn = document.getElementById('download-converted-btn');
    const netronBtn = document.getElementById('view-netron-btn');

    if (!browseBtn) return;

    browseBtn.onclick = () => fileInput.click();

    fileInput.onchange = async (e) => {
        if (e.target.files.length) {
            await processModelFile(e.target.files[0]);
            e.target.value = ''; // Reset so same file can be selected again
        }
    };

    dropZone.ondragover = (e) => {
        e.preventDefault();
        dropZone.classList.add('dragover');
    };

    dropZone.ondragleave = () => dropZone.classList.remove('dragover');

    dropZone.ondrop = async (e) => {
        e.preventDefault();
        dropZone.classList.remove('dragover');

        const file = e.dataTransfer.files[0];
        if (file && (file.name.endsWith('.tflite') || file.name.endsWith('.h5') || file.name.endsWith('.aif32'))) {
            await processModelFile(file);
        } else {
            log('Please drop a .tflite, .h5, or .aif32 file', 'error');
        }
    };

    if (convertBtn) {
        convertBtn.onclick = async () => {
            if (!importedModel || !device) {
                log('No model or device connected', 'error');
                return;
            }

            log('Deploying converted model to device...', 'info');
            try {
                const success = await device.uploadModel(importedModel.name + '.aif32', importedModel.data);
                if (success) {
                    log('Model deployed successfully!', 'success');
                } else {
                    log('Upload failed', 'error');
                }
            } catch (err) {
                log('Deploy error: ' + err.message, 'error');
            }
        };
    }

    if (downloadBtn) {
        downloadBtn.onclick = () => {
            if (!importedModel) return;
            downloadBlob(importedModel.data, importedModel.name + '.aif32');
            log('Downloaded ' + importedModel.name + '.aif32', 'success');
        };
    }

    if (netronBtn) {
        netronBtn.onclick = () => {
            // Open Netron with the uploaded file
            // Since we can't directly pass the file, we'll open Netron and user can drag-drop
            window.open('https://netron.app/', '_blank');
            log('Opened Netron viewer. Drag your model file there to visualize.', 'info');
        };
    }
}

async function processModelFile(file) {
    log(`Loading ${file.name}...`, 'info');

    try {
        const buffer = await file.arrayBuffer();
        let result;

        if (file.name.endsWith('.aif32')) {
            // Direct load for AIF32 files
            log('Loading pre-compiled AIF32 model...', 'info');

            const dv = new DataView(buffer);
            const magic = dv.getUint32(0, true);
            let info = {
                inputSize: '?',
                outputSize: '?',
                hiddenSize: '?',
                numLayers: '?',
                numWeights: buffer.byteLength / 4,
                originalSize: buffer.byteLength,
                convertedSize: buffer.byteLength
            };

            if (magic === 0x54525053) { // "SPRT"
                info.version = dv.getUint16(4, true);
                info.inputSize = dv.getUint8(6);
                info.outputSize = dv.getUint8(7);
                info.hiddenSize = dv.getUint8(8);
                // info.modelType = dv.getUint8(9);
                info.numLayers = dv.getUint8(10);
            }

            result = {
                data: new Uint8Array(buffer),
                info: info
            };
        } else {
            // Convert TFLite/Keras
            result = convertTFLiteToAIFes(buffer, file.name.replace(/\.(tflite|h5)$/, ''));
        }

        importedModel = {
            name: file.name.replace(/\.(tflite|h5|aif32)$/, ''),
            data: result.data,
            info: result.info,
            originalFile: file
        };

        const modelInfo = document.getElementById('model-info');
        const details = document.getElementById('model-details');
        const nameEl = document.getElementById('imported-model-name');

        if (nameEl) nameEl.textContent = file.name;

        if (details) {
            details.innerHTML = `
        <div class="model-stat"><span>Input</span><strong>${result.info.inputSize}</strong></div>
        <div class="model-stat"><span>Output</span><strong>${result.info.outputSize}</strong></div>
        <div class="model-stat"><span>Hidden</span><strong>${result.info.hiddenSize}</strong></div>
        <div class="model-stat"><span>Layers</span><strong>${result.info.numLayers}</strong></div>
        <div class="model-stat"><span>Weights</span><strong>${result.info.numWeights.toLocaleString()}</strong></div>
        <div class="model-stat"><span>Size</span><strong>${formatBytes(result.info.originalSize)} → ${formatBytes(result.info.convertedSize)}</strong></div>
      `;
        }

        if (modelInfo) modelInfo.style.display = 'block';

        if (modelInfo) modelInfo.style.display = 'block';

        if (elements.convertDeployBtn) {
            elements.convertDeployBtn.disabled = !device;
        }

        log(`Converted: ${result.info.numLayers} layers, ${result.info.numWeights.toLocaleString()} weights`, 'success');

        // Show architecture
        if (modelVisualizer) {
            showModelArchitecture(result.info.inputSize, result.info.outputSize, result.info.hiddenSize);
        }

    } catch (err) {
        log('Conversion error: ' + err.message, 'error');
    }
}

function formatBytes(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
}

function updateSampleDisplay() {
    const count = dataInput.getSampleCount();
    const classes = dataInput.getClassCount();

    elements.sampleCount.textContent = count;
    elements.classCount.textContent = classes;
    elements.dataBadge.textContent = count + ' samples';

    // Update sample preview
    updateSamplePreview();
}

function updateSamplePreview() {
    const preview = document.getElementById('samples-preview');
    if (!preview || !dataInput) return;

    const samples = dataInput.getSamples();
    preview.innerHTML = '';

    // Show up to 20 recent samples
    const recentSamples = samples.slice(-20);

    for (const sample of recentSamples) {
        const thumb = document.createElement('div');
        thumb.className = 'sample-thumb';
        thumb.innerHTML = `<span class="class-badge">${sample.label}</span>`;
        // Could add actual thumbnail image here if we store it
        preview.appendChild(thumb);
    }
}

// Console
function setupConsole() {
    document.getElementById('clear-console').onclick = () => {
        elements.consoleOutput.innerHTML = '';
    };

    const toggleBtn = document.getElementById('toggle-console');
    if (toggleBtn) {
        toggleBtn.onclick = () => {
            document.getElementById('console').classList.toggle('collapsed');
        };
    }
}

// Init
document.addEventListener('DOMContentLoaded', () => {
    // Cache elements
    elements.statusDot = document.getElementById('status-dot');
    elements.statusText = document.getElementById('status-text');
    elements.refreshBtn = document.getElementById('refresh-btn');
    elements.deviceList = document.getElementById('device-list');
    elements.backendBadge = document.getElementById('backend-badge');
    elements.dataBadge = document.getElementById('data-badge');
    elements.trainBtn = document.getElementById('train-btn');
    elements.stopBtn = document.getElementById('stop-btn');
    elements.deployBtn = document.getElementById('deploy-btn');
    elements.convertDeployBtn = document.getElementById('convert-deploy-btn');
    elements.downloadModelBtn = document.getElementById('download-model-btn');
    elements.progressSection = document.getElementById('progress-section');
    elements.chartContainer = document.getElementById('chart-container');
    elements.progressFill = document.getElementById('progress-fill');
    elements.progressText = document.getElementById('progress-text');
    elements.etaText = document.getElementById('eta-text');
    elements.lossValue = document.getElementById('loss-value');
    elements.accuracyValue = document.getElementById('accuracy-value');
    elements.epochValue = document.getElementById('epoch-value');
    elements.consoleOutput = document.getElementById('console-output');
    elements.webcam = document.getElementById('webcam');
    elements.captureCanvas = document.getElementById('capture-canvas');
    elements.drawCanvas = document.getElementById('draw-canvas');
    elements.sampleCount = document.getElementById('sample-count');
    elements.classCount = document.getElementById('class-count');
    elements.modelPanel = document.getElementById('model-panel');

    // Init modules
    trainer = new SpriteTrainer();
    dataInput = new DataInput();

    // Init chart
    if (document.getElementById('training-chart')) {
        chart = new TrainingChart('training-chart');
    }

    // Init model visualizer
    if (document.getElementById('model-arch-display')) {
        modelVisualizer = new ModelVisualizer('model-arch-display');
    }

    // Update backend badge
    setTimeout(() => {
        const backend = trainer.getBackend();
        elements.backendBadge.textContent = backend.toUpperCase();
        if (backend === 'webgpu') {
            elements.backendBadge.classList.add('gpu');
            log('WebGPU acceleration enabled', 'success');
        } else {
            log('Using CPU backend (WebGPU not available)', 'warning');
        }
    }, 100);

    // Event listeners
    elements.refreshBtn.onclick = scanDevices;
    elements.trainBtn.onclick = startTraining;
    if (elements.stopBtn) elements.stopBtn.onclick = stopTraining;
    elements.deployBtn.onclick = deployModel;
    if (elements.downloadModelBtn) elements.downloadModelBtn.onclick = downloadTrainedModel;

    // Close model panel
    const closeModelPanel = document.getElementById('close-model-panel');
    if (closeModelPanel) {
        closeModelPanel.onclick = () => {
            elements.modelPanel.style.display = 'none';
        };
    }

    // Clear samples
    const clearSamplesBtn = document.getElementById('clear-samples-btn');
    if (clearSamplesBtn) {
        clearSamplesBtn.onclick = () => {
            dataInput.clearSamples();
            updateSampleDisplay();
            log('Cleared all samples', 'info');
        };
    }

    // Setup UI
    setupTabs();
    setupDrawCanvas();
    setupFileInput();
    setupWebcamCapture();
    setupTestMedia();
    setupModelImport();
    setupConsole();
    setupMockDevice();

    // Initial log
    log('Sprite Trainer v2.0 initialized', 'info');
    log('Toggle "Mock" to use simulated device, or click refresh to scan', 'info');
});

// Mock device setup
function setupMockDevice() {
    const toggle = document.getElementById('mock-toggle');
    if (!toggle) return;

    toggle.onchange = () => {
        mockMode = toggle.checked;
        const displayPanel = document.getElementById('display-panel');

        if (mockMode) {
            log('Mock mode enabled', 'info');
            scanDevices(); // Auto-connect mock
        } else {
            log('Mock mode disabled', 'info');
            if (displayPanel) displayPanel.style.display = 'none';
            device = null;
            mockDevice = null;
            devices = [];
            elements.deviceList.innerHTML = '<div class="empty-state">Click refresh to scan ports</div>';
            updateStatus();
        }
    };

    // Sim display controls
    const simClear = document.getElementById('sim-clear');
    const simDemo = document.getElementById('sim-demo');
    const simInfer = document.getElementById('sim-infer');

    if (simClear) {
        simClear.onclick = () => {
            if (mockDevice) {
                mockDevice.sendCommand(0x10, [0]); // Clear
                mockDevice.sendCommand(0x2F); // Flush
                log('Display cleared', 'info');
            }
        };
    }

    if (simDemo) {
        simDemo.onclick = async () => {
            if (!mockDevice) return;
            log('Drawing demo pattern...', 'info');

            // Clear
            await mockDevice.sendCommand(0x10, [0]);

            // Draw border rect
            await mockDevice.sendCommand(0x12, [0, 0, 128, 64, 1]);
            // Draw inner black rect
            await mockDevice.sendCommand(0x12, [2, 2, 124, 60, 0]);

            // Draw some rects as a pattern
            for (let i = 0; i < 5; i++) {
                const x = 10 + i * 22;
                const y = 10;
                const w = 16;
                const h = 16 + i * 6;
                await mockDevice.sendCommand(0x12, [x, y, w, h, 1]);
            }

            // Draw pixels as dots
            for (let i = 0; i < 20; i++) {
                const x = Math.floor(Math.random() * 128);
                const y = Math.floor(Math.random() * 64);
                await mockDevice.sendCommand(0x11, [x, y, 1]);
            }

            await mockDevice.sendCommand(0x2F); // Flush
            log('Demo pattern drawn', 'success');
        };
    }

    if (simInfer) {
        simInfer.onclick = async () => {
            if (!mockDevice) return;

            const tests = [[0, 0], [0, 1], [1, 0], [1, 1]];
            log('Running XOR inference test...', 'info');

            for (const [a, b] of tests) {
                const payload = new Uint8Array(8);
                const dv = new DataView(payload.buffer);
                dv.setFloat32(0, a, true);
                dv.setFloat32(4, b, true);
                const resp = await mockDevice.sendCommand(0x50, payload);

                if (resp && resp.length >= 7) {
                    const result = new DataView(resp.buffer, resp.byteOffset + 3).getFloat32(0, true);
                    log(`  ${a} XOR ${b} = ${result.toFixed(3)}`, 'success');
                }
            }
        };
    }
}
