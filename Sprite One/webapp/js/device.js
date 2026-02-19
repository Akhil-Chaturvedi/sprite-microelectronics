/**
 * Sprite Device Manager
 * WebSerial API for device detection and communication
 */

// Helper for CRC32
const crc32_table = new Uint32Array(256);
for (let i = 0; i < 256; i++) {
    let c = i;
    for (let j = 0; j < 8; j++) {
        c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
    }
    crc32_table[i] = c;
}

function crc32(data) {
    let crc = 0xFFFFFFFF;
    for (let i = 0; i < data.length; i++) {
        const byte = data[i];
        crc = (crc >>> 8) ^ crc32_table[(crc ^ byte) & 0xFF];
    }
    return (crc ^ 0xFFFFFFFF) >>> 0;
}

class SpriteDevice {
    constructor(port) {
        this.port = port;
        this.reader = null;
        this.writer = null;
        this.isConnected = false;
        this.version = null;
        this.portInfo = port.getInfo();
    }

    static async scan() {
        const devices = [];

        if (!('serial' in navigator)) {
            console.log('[Device] WebSerial not supported');
            return devices;
        }

        try {
            const ports = await navigator.serial.getPorts();

            for (const port of ports) {
                const device = new SpriteDevice(port);
                const isSprite = await device.probe();
                if (isSprite) {
                    devices.push(device);
                }
            }
        } catch (err) {
            console.error('[Device] Scan error:', err);
        }

        return devices;
    }

    static async requestPort() {
        if (!('serial' in navigator)) {
            throw new Error('WebSerial not supported');
        }

        const port = await navigator.serial.requestPort();
        const device = new SpriteDevice(port);
        return device;
    }

    async open(baudRate = 115200) {
        if (this.isConnected) return true;

        try {
            await this.port.open({ baudRate });
            this.reader = this.port.readable.getReader();
            this.writer = this.port.writable.getWriter();
            this.isConnected = true;
            return true;
        } catch (err) {
            console.error('[Device] Open error:', err);
            return false;
        }
    }

    async close() {
        if (!this.isConnected) return;

        try {
            if (this.reader) {
                await this.reader.cancel();
                this.reader.releaseLock();
                this.reader = null;
            }
            if (this.writer) {
                this.writer.releaseLock();
                this.writer = null;
            }
            await this.port.close();
            this.isConnected = false;
        } catch (err) {
            console.error('[Device] Close error:', err);
        }
    }

    async probe() {
        try {
            await this.open();

            // Send version command (With CRC32)
            // Header: AA, CMD(0F), LEN(00)
            const packet = new Uint8Array([0xAA, 0x0F, 0x00]);
            const crc = crc32(packet.slice(1)); // CRC of CMD+LEN+[DATA]

            const finalPacket = new Uint8Array(7);
            finalPacket.set(packet);
            new DataView(finalPacket.buffer).setUint32(3, crc, true);

            await this.writer.write(finalPacket);

            // Read response with timeout
            // Expected: AA CMD STATUS LEN [DATA] CRC
            const response = await this.readWithTimeout(100);

            if (response && response.length >= 8 && response[0] === 0xAA) {
                this.version = this.parseVersionResponse(response);
                await this.close();
                return true;
            }

            await this.close();
            return false;
        } catch (err) {
            try { await this.close(); } catch { }
            return false;
        }
    }

    async readWithTimeout(timeoutMs) {
        // ... implementation same ...
        // Maybe increase timeout slightly for CRC calc?
        const buffer = [];
        const startTime = Date.now();

        while (Date.now() - startTime < timeoutMs) {
            try {
                const { value, done } = await Promise.race([
                    this.reader.read(),
                    new Promise((_, reject) =>
                        setTimeout(() => reject(new Error('timeout')), 50)
                    )
                ]);

                if (done) break;
                if (value) buffer.push(...value);

                // Need at least header(4) + crc(4) = 8 bytes
                // But could vary.
            } catch {
                break;
            }
        }
        return new Uint8Array(buffer);
    }

    async sendCommand(cmd, payload = []) {
        if (!this.isConnected) {
            await this.open();
        }

        // Construct Packet: AA CMD LEN [DATA] CRC32
        const content = new Uint8Array([cmd, payload.length, ...payload]);
        const crc = crc32(content);

        const packet = new Uint8Array(1 + content.length + 4);
        packet[0] = 0xAA;
        packet.set(content, 1);
        new DataView(packet.buffer).setUint32(1 + content.length, crc, true);

        await this.writer.write(packet);

        return await this.readWithTimeout(500); // Increased timeout
    }

    async uploadModel(filename, data) {
        // 1. START (0x63)
        const filenameBytes = new TextEncoder().encode(filename);
        let resp = await this.sendCommand(0x63, filenameBytes);
        if (!resp || resp.length < 3 || resp[2] !== 0x00) {
            console.error("Upload Start Failed", resp);
            return false;
        }

        // 2. CHUNKS (0x68)
        const CHUNK_SIZE = 200; // Safe margin for 300 byte buffer
        const totalChunks = Math.ceil(data.length / CHUNK_SIZE);

        for (let i = 0; i < data.length; i += CHUNK_SIZE) {
            const chunk = data.slice(i, i + CHUNK_SIZE);
            resp = await this.sendCommand(0x68, chunk);
            if (!resp || resp.length < 3 || resp[2] !== 0x00) { // STATUS check (index 2 in AA CMD STATUS)
                console.error(`Upload Chunk Failed at ${i}`, resp);
                return false;
            }
            // Optional progress callback?
        }

        // 3. END (0x69)
        // Calc full file CRC
        const fullCrc = crc32(data);
        const crcBytes = new Uint8Array(4);
        new DataView(crcBytes.buffer).setUint32(0, fullCrc, true);

        resp = await this.sendCommand(0x69, crcBytes);
        if (!resp || resp.length < 3 || resp[2] !== 0x00) {
            console.error("Upload End Failed", resp);
            return false;
        }

        return true;
    }

    getDisplayName() {
        const info = this.portInfo;
        if (this.version) {
            return `Sprite One ${this.version}`;
        }
        if (info.usbVendorId) {
            return `USB Device (${info.usbVendorId.toString(16)}:${info.usbProductId?.toString(16) || '0000'})`;
        }
        return 'Serial Port';
    }
}

// Export for use
window.SpriteDevice = SpriteDevice;
