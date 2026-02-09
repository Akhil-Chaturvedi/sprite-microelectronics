/**
 * Sprite Device Manager
 * WebSerial API for device detection and communication
 */

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

            // Send version command
            const cmd = new Uint8Array([0xAA, 0x0F, 0x00, 0x00]);
            await this.writer.write(cmd);

            // Read response with timeout
            const response = await this.readWithTimeout(100);

            if (response && response.length >= 4 && response[0] === 0xAA) {
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

                if (buffer.length >= 4) break;
            } catch {
                break;
            }
        }

        return new Uint8Array(buffer);
    }

    parseVersionResponse(data) {
        if (data.length < 4) return null;
        return `v${data[3] || 1}.${data[4] || 0}.${data[5] || 0}`;
    }

    async sendCommand(cmd, payload = []) {
        if (!this.isConnected) {
            await this.open();
        }

        const packet = new Uint8Array([0xAA, cmd, payload.length, ...payload, 0x00]);
        await this.writer.write(packet);

        return await this.readWithTimeout(200);
    }

    async uploadModel(filename, data) {
        // Payload: filename_len(1) + filename + data
        const filenameBytes = new TextEncoder().encode(filename);
        const payload = new Uint8Array([filenameBytes.length, ...filenameBytes, ...data]);

        const response = await this.sendCommand(0x63, payload);
        return response && response[1] === 0x00;
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
