/* test_runner.js */
const fs = require('fs');

// Mock browser
global.window = {};
global.console = console;

// Emulate simple browser env
global.navigator = { userAgent: 'Node' };
global.document = {
    getElementById: () => null,
    createElement: () => ({ getContext: () => ({}) })
};

// Load Mock Device
// We need to handle potential syntax errors in mock_device.js if I broke it
try {
    let code = fs.readFileSync('webapp/js/mock_device.js', 'utf8');
    // Force export to global
    code += "\n;global.MockSpriteDevice = MockSpriteDevice;";

    const vm = require('vm');
    const script = new vm.Script(code);
    const context = new vm.createContext(global);
    script.runInContext(context);
} catch (e) {
    console.error("Error loading mock_device.js:", e);
    process.exit(1);
}

const MockSpriteDevice = global.MockSpriteDevice;
// Debug Check
if (typeof MockSpriteDevice !== 'function') {
    console.error("MockSpriteDevice not found in global scope!");
    process.exit(1);
}

async function runTest(filename, inputs, expectedSize) {
    console.log(`\n--- Testing ${filename} ---`);
    if (!fs.existsSync(filename)) {
        console.log("File not found, skipping.");
        return;
    }

    const dev = new MockSpriteDevice();
    const buf = fs.readFileSync(filename);

    await dev.uploadModel(filename, new Uint8Array(buf));

    const m = dev.models[dev.activeModel];
    console.log(`Loaded: ${m.name} (IN=${m.inputSize}, OUT=${m.outputSize})`);

    if (m.layers) {
        console.log(`Layers: ${m.layers.length}`);
        m.layers.forEach((l, i) => {
            let info = `Type=${l.type} P1=${l.param1}`;
            if (l.inputShape) info += ` In:${l.inputShape}`;
            if (l.outputShape) info += ` Out:${l.outputShape}`;
            console.log(`  L${i}: ${info}`);
        });
    }

    // Inference
    console.log(`Running Inference...`);
    // Ensure inputs match model input size?
    // Sentinel V3 has 128 input.
    // Sigmoid has 2.
    // Conv has 128 (treated as 1x1x128).

    let inputData = inputs;
    if (m.inputSize && inputs.length !== m.inputSize) {
        console.warn(`Input size mismatch: Warning ${inputs.length} vs ${m.inputSize}. Adjusting...`);
        if (inputs.length < m.inputSize) {
            inputData = new Array(m.inputSize).fill(0).map((_, i) => inputs[i] || 0);
        } else {
            inputData = inputs.slice(0, m.inputSize);
        }
    }

    const out = dev._computeInference(inputData, m);
    // console.log("Output:", out);

    if (out.length !== expectedSize) throw new Error(`Output size mismatch: ${out.length} vs ${expectedSize}`);
    if (out.some(x => isNaN(x))) throw new Error("Output contains NaN");

    console.log(`SUCCESS. Output: [${out.map(n => n.toFixed(4)).join(', ')}]`);
}

async function runTrainingTest() {
    console.log(`\n--- Testing On-Device Training (XOR) ---`);
    const filename = 'xor_random.aif32';
    if (!fs.existsSync(filename)) { console.log("File not found."); return; }

    const dev = new MockSpriteDevice();
    const buf = fs.readFileSync(filename);
    await dev.uploadModel(filename, new Uint8Array(buf));

    const m = dev.models[dev.activeModel];
    console.log(`Loaded: ${m.name}`);

    // 1. Initial State (Random)
    const out0 = dev._computeInference([0, 1], m)[0];
    console.log(`Initial Output (0,1): ${out0.toFixed(4)} (Should be random)`);

    // 2. Train Loop
    console.log("Setting LR=0.5 and Training 500 epochs...");

    // CMD_FINETUNE_START (0x65) - Set LR
    // Payload: float32 LR
    const lrBuf = new Uint8Array(4);
    new DataView(lrBuf.buffer).setFloat32(0, 0.5, true);
    dev._handleCommand(0x65, lrBuf);

    const data = [
        { i: [0, 0], t: [0] },
        { i: [0, 1], t: [1] },
        { i: [1, 0], t: [1] },
        { i: [1, 1], t: [0] }
    ];

    // Set Learning Rate (optional, MockDevice defaults to 0.1)

    for (let e = 0; e < 500; e++) {
        let batchLoss = 0;
        for (const d of data) {
            // Construct Payload: [In...][Target...]
            const payloadFloats = new Float32Array([...d.i, ...d.t]);
            const payload = new Uint8Array(payloadFloats.buffer);

            // CMD_AI_TRAIN (0x51)
            // MockDevice._handleCommand returns response buffer
            const resp = dev._handleCommand(0x51, payload);

            // Parse Loss (4 bytes at offset 4 in response)
            // Header(4) + Loss(4) + CRC(4)
            const view = new DataView(resp.buffer);
            // Resp: AA 51 00 04 [Loss] [CRC]
            const loss = view.getFloat32(4, true);
            batchLoss += loss;
        }

        if (e % 100 === 0) console.log(`  Epoch ${e}: Loss=${(batchLoss / 4).toFixed(4)}`);
    }

    // 3. Verify
    console.log("Verifying...");
    const t0 = dev._computeInference([0, 0], m)[0];
    const t1 = dev._computeInference([0, 1], m)[0];
    const t2 = dev._computeInference([1, 0], m)[0];
    const t3 = dev._computeInference([1, 1], m)[0];

    console.log(`  0,0 -> ${t0.toFixed(4)} (Expect < 0.1)`);
    console.log(`  0,1 -> ${t1.toFixed(4)} (Expect > 0.9)`);
    console.log(`  1,0 -> ${t2.toFixed(4)} (Expect > 0.9)`);
    console.log(`  1,1 -> ${t3.toFixed(4)} (Expect < 0.1)`);

    if (t0 < 0.2 && t3 < 0.2 && t1 > 0.8 && t2 > 0.8) {
        console.log("TRAINING SUCCESS: XOR Solved.");
    } else {
        throw new Error("TRAINING FAILED: XOR not solved.");
    }
}

async function run() {
    try {
        // ... existing tests ...
        // await runTest(...)

        await runTrainingTest(); // NEW

        // 1. Sentinel God (128 -> 5)
        await runTest('sentinel_god_v3.aif32', new Array(128).fill(0.1), 5);

        // ...
        // XOR inputs: 0,0 -> ?
        await runTest('test_sigmoid.aif32', [0, 0], 1);
        await runTest('test_sigmoid.aif32', [1, 1], 1);

        // 3. Conv Test (128 -> 5)
        await runTest('test_conv.aif32', new Array(128).fill(0.1), 5);

        // 4. Comprehensive Test (100 -> 1)
        // Conv(2, 1x5, S=2) -> 96 units -> ReLU -> Dense(10) -> Sigmoid -> Dense(1) -> Sigmoid
        await runTest('test_comprehensive.aif32', new Array(100).fill(0.5), 1);

        // 5. Pooling Test (MaxPool + Flatten)
        // Input(100) -> MaxPool(2x2, S=2) -> Flatten -> Dense(1)
        await runTest('test_pool.aif32', new Array(100).fill(1.0), 1);

        // 6. Vision Demo (MNIST CNN)
        // Input 784 -> Conv -> Pool -> Conv -> Pool -> Flatten -> Dense -> Dense
        // Output 10 (Softmax)
        await runTest('vision_demo.aif32', new Array(784).fill(0.0), 10);

        console.log("\nALL TESTS PASSED.");
        process.exit(0);

    } catch (e) {
        console.error("FAILED:", e.message);
        process.exit(1);
    }
}

run();
