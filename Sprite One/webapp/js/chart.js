/**
 * Training Chart
 * Canvas-based loss/accuracy visualization
 */

class TrainingChart {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.lossHistory = [];
        this.accuracyHistory = [];
        this.maxPoints = 200;

        // Colors
        this.lossColor = '#f44336';
        this.accuracyColor = '#4caf50';
        this.gridColor = '#333';
        this.textColor = '#888';
        this.bgColor = '#1a1a1a';

        // Layout
        this.padding = { top: 10, right: 40, bottom: 25, left: 45 };
    }

    reset() {
        this.lossHistory = [];
        this.accuracyHistory = [];
        this.draw();
    }

    addPoint(loss, accuracy) {
        this.lossHistory.push(loss);
        this.accuracyHistory.push(accuracy);

        // Limit points for performance
        if (this.lossHistory.length > this.maxPoints) {
            this.lossHistory.shift();
            this.accuracyHistory.shift();
        }

        this.draw();
    }

    draw() {
        const { width, height } = this.canvas;
        const ctx = this.ctx;

        // Clear
        ctx.fillStyle = this.bgColor;
        ctx.fillRect(0, 0, width, height);

        const chartWidth = width - this.padding.left - this.padding.right;
        const chartHeight = height - this.padding.top - this.padding.bottom;

        // Draw grid
        this.drawGrid(chartWidth, chartHeight);

        if (this.lossHistory.length < 2) {
            this.drawEmptyState();
            return;
        }

        // Calculate scales
        const maxLoss = Math.max(...this.lossHistory, 1);
        const xScale = chartWidth / (this.lossHistory.length - 1);

        // Draw loss line
        this.drawLine(
            this.lossHistory,
            maxLoss,
            xScale,
            chartHeight,
            this.lossColor
        );

        // Draw accuracy line (0-1 scale)
        this.drawLine(
            this.accuracyHistory,
            1,
            xScale,
            chartHeight,
            this.accuracyColor
        );

        // Draw axis labels
        this.drawAxisLabels(maxLoss, chartHeight);
    }

    drawGrid(chartWidth, chartHeight) {
        const ctx = this.ctx;
        ctx.strokeStyle = this.gridColor;
        ctx.lineWidth = 0.5;

        // Horizontal lines
        for (let i = 0; i <= 4; i++) {
            const y = this.padding.top + (chartHeight * i / 4);
            ctx.beginPath();
            ctx.moveTo(this.padding.left, y);
            ctx.lineTo(this.padding.left + chartWidth, y);
            ctx.stroke();
        }

        // Vertical lines (epochs)
        const numVLines = 5;
        for (let i = 0; i <= numVLines; i++) {
            const x = this.padding.left + (chartWidth * i / numVLines);
            ctx.beginPath();
            ctx.moveTo(x, this.padding.top);
            ctx.lineTo(x, this.padding.top + chartHeight);
            ctx.stroke();
        }
    }

    drawLine(data, maxVal, xScale, chartHeight, color) {
        const ctx = this.ctx;
        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';

        ctx.beginPath();

        for (let i = 0; i < data.length; i++) {
            const x = this.padding.left + i * xScale;
            const y = this.padding.top + chartHeight - (data[i] / maxVal * chartHeight);

            if (i === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        }

        ctx.stroke();

        // Draw endpoint dot
        if (data.length > 0) {
            const lastX = this.padding.left + (data.length - 1) * xScale;
            const lastY = this.padding.top + chartHeight - (data[data.length - 1] / maxVal * chartHeight);

            ctx.fillStyle = color;
            ctx.beginPath();
            ctx.arc(lastX, lastY, 4, 0, Math.PI * 2);
            ctx.fill();
        }
    }

    drawAxisLabels(maxLoss, chartHeight) {
        const ctx = this.ctx;
        ctx.fillStyle = this.textColor;
        ctx.font = '10px monospace';
        ctx.textAlign = 'right';

        // Loss axis (left)
        ctx.fillText(maxLoss.toFixed(2), this.padding.left - 5, this.padding.top + 4);
        ctx.fillText('0', this.padding.left - 5, this.padding.top + chartHeight + 4);

        // Accuracy axis (right)
        ctx.textAlign = 'left';
        const rightX = this.canvas.width - this.padding.right + 5;
        ctx.fillStyle = this.accuracyColor;
        ctx.fillText('100%', rightX, this.padding.top + 4);
        ctx.fillText('0%', rightX, this.padding.top + chartHeight + 4);

        // Epoch label
        ctx.fillStyle = this.textColor;
        ctx.textAlign = 'center';
        ctx.fillText(`Epoch ${this.lossHistory.length}`, this.canvas.width / 2, this.canvas.height - 5);
    }

    drawEmptyState() {
        const ctx = this.ctx;
        ctx.fillStyle = this.textColor;
        ctx.font = '12px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('Training data will appear here', this.canvas.width / 2, this.canvas.height / 2);
    }

    resize() {
        // Handle canvas resize if needed
        const dpr = window.devicePixelRatio || 1;
        const rect = this.canvas.getBoundingClientRect();

        this.canvas.width = rect.width * dpr;
        this.canvas.height = rect.height * dpr;

        this.ctx.scale(dpr, dpr);
        this.draw();
    }
}

// Model Architecture Visualizer
class ModelVisualizer {
    constructor(containerId) {
        this.container = document.getElementById(containerId);
    }

    render(layers) {
        if (!this.container) return;

        this.container.innerHTML = '';

        const viz = document.createElement('div');
        viz.className = 'arch-viz';

        layers.forEach((layer, i) => {
            const layerEl = document.createElement('div');
            layerEl.className = 'layer-node ' + this.getLayerClass(layer.type);

            layerEl.innerHTML = `
        <div class="layer-type">${layer.type}</div>
        <div class="layer-shape">${layer.shape || ''}</div>
        ${layer.params ? `<div class="layer-params">${layer.params} params</div>` : ''}
      `;

            viz.appendChild(layerEl);

            // Add arrow between layers
            if (i < layers.length - 1) {
                const arrow = document.createElement('div');
                arrow.className = 'layer-arrow';
                arrow.innerHTML = '→';
                viz.appendChild(arrow);
            }
        });

        this.container.appendChild(viz);
    }

    getLayerClass(type) {
        if (type.includes('Input')) return 'layer-input';
        if (type.includes('Dense') || type.includes('FullyConnected')) return 'layer-dense';
        if (type.includes('Conv')) return 'layer-conv';
        if (type.includes('Pool')) return 'layer-pool';
        if (type.includes('ReLU') || type.includes('Activation')) return 'layer-activation';
        if (type.includes('Softmax') || type.includes('Output')) return 'layer-output';
        return 'layer-other';
    }

    renderFromModel(architecture, inputSize, outputSize, hiddenSize) {
        const layers = [
            { type: 'Input', shape: `[${inputSize}]` },
            { type: 'Dense', shape: `[${hiddenSize}]`, params: inputSize * hiddenSize + hiddenSize },
            { type: 'ReLU' },
            { type: 'Dense', shape: `[${outputSize}]`, params: hiddenSize * outputSize + outputSize },
            { type: 'Output', shape: `[${outputSize}]` }
        ];

        this.render(layers);
    }
}

// Export
window.TrainingChart = TrainingChart;
window.ModelVisualizer = ModelVisualizer;
