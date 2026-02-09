/**
 * Data Input Handler
 * Webcam, file upload, canvas drawing
 */

class DataInput {
    constructor() {
        this.samples = [];
        this.classes = new Set();
        this.currentClass = 0;
        this.webcamStream = null;
    }

    async initWebcam(videoElement) {
        try {
            this.webcamStream = await navigator.mediaDevices.getUserMedia({
                video: { width: 320, height: 240, facingMode: 'environment' }
            });
            videoElement.srcObject = this.webcamStream;
            return true;
        } catch (err) {
            console.error('[Input] Webcam error:', err);
            return false;
        }
    }

    captureFromWebcam(videoElement, canvasElement, size = 28) {
        const ctx = canvasElement.getContext('2d');
        canvasElement.width = size;
        canvasElement.height = size;

        // Draw scaled and grayscaled
        ctx.drawImage(videoElement, 0, 0, size, size);
        const imageData = ctx.getImageData(0, 0, size, size);

        // Convert to grayscale normalized array
        const data = new Float32Array(size * size);
        for (let i = 0; i < size * size; i++) {
            const r = imageData.data[i * 4];
            const g = imageData.data[i * 4 + 1];
            const b = imageData.data[i * 4 + 2];
            data[i] = (0.299 * r + 0.587 * g + 0.114 * b) / 255;
        }

        return data;
    }

    captureFromCanvas(canvasElement, size = 28) {
        const tempCanvas = document.createElement('canvas');
        tempCanvas.width = size;
        tempCanvas.height = size;
        const ctx = tempCanvas.getContext('2d');

        // Draw scaled
        ctx.drawImage(canvasElement, 0, 0, size, size);
        const imageData = ctx.getImageData(0, 0, size, size);

        // Convert to grayscale normalized
        const data = new Float32Array(size * size);
        for (let i = 0; i < size * size; i++) {
            const r = imageData.data[i * 4];
            const g = imageData.data[i * 4 + 1];
            const b = imageData.data[i * 4 + 2];
            data[i] = (0.299 * r + 0.587 * g + 0.114 * b) / 255;
        }

        return data;
    }

    async loadFromFile(file, size = 28) {
        return new Promise((resolve, reject) => {
            const img = new Image();
            img.onload = () => {
                const canvas = document.createElement('canvas');
                canvas.width = size;
                canvas.height = size;
                const ctx = canvas.getContext('2d');

                ctx.drawImage(img, 0, 0, size, size);
                const imageData = ctx.getImageData(0, 0, size, size);

                const data = new Float32Array(size * size);
                for (let i = 0; i < size * size; i++) {
                    const r = imageData.data[i * 4];
                    const g = imageData.data[i * 4 + 1];
                    const b = imageData.data[i * 4 + 2];
                    data[i] = (0.299 * r + 0.587 * g + 0.114 * b) / 255;
                }

                resolve(data);
            };
            img.onerror = reject;
            img.src = URL.createObjectURL(file);
        });
    }

    addSample(data, classLabel) {
        this.samples.push({ data, class: classLabel, label: classLabel });
        this.classes.add(classLabel);
    }

    getSamples() {
        return this.samples;
    }

    clearSamples() {
        this.samples = [];
        this.classes.clear();
    }

    getTrainingData() {
        const classArray = Array.from(this.classes).sort();
        const numClasses = classArray.length;

        const inputs = [];
        const labels = [];

        for (const sample of this.samples) {
            inputs.push(Array.from(sample.data));

            // One-hot encode
            const label = new Array(numClasses).fill(0);
            label[classArray.indexOf(sample.class)] = 1;
            labels.push(label);
        }

        return { inputs, labels, numClasses };
    }

    getSampleCount() {
        return this.samples.length;
    }

    getClassCount() {
        return this.classes.size;
    }

    setCurrentClass(classLabel) {
        this.currentClass = classLabel;
    }

    getCurrentClass() {
        return this.currentClass;
    }

    stopWebcam() {
        if (this.webcamStream) {
            this.webcamStream.getTracks().forEach(track => track.stop());
            this.webcamStream = null;
        }
    }
}

window.DataInput = DataInput;
