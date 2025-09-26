import { SerialVis } from './serialVis';
import { clamp } from './utils';
function bootstrap() {
    const container = document.getElementById('serial-vis');
    if (!container) {
        throw new Error('Missing container element with id "serial-vis"');
    }
    const vis = new SerialVis(container, {
        width: container.clientWidth || 720,
        height: 280,
        sampleCount: 720,
        valueRange: { min: -1.2, max: 1.2 },
        initialValue: 0,
        smoothing: { kind: 'moving-average', windowSize: 12 },
        line: { color: '#38f2ff', width: 2.5, style: 'solid' },
        futureMask: { kind: 'dim', color: '#0f172a', opacity: 0.35 },
        progressBar: { enabled: true, color: '#22d3ee', backgroundColor: 'rgba(34, 211, 238, 0.1)', height: 8 },
        containerBackground: 'transparent'
    });
    window.serialVis = vis;
    setupSettings(vis);
    startDemoStream(vis);
}
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bootstrap, { once: true });
}
else {
    bootstrap();
}
function setupSettings(vis) {
    const smoothingSelect = document.getElementById('smoothing-select');
    const smoothingWindow = document.getElementById('smoothing-window');
    const smoothingAlpha = document.getElementById('smoothing-alpha');
    const smoothingCutoff = document.getElementById('smoothing-cutoff');
    const lineColor = document.getElementById('line-color');
    const lineWidth = document.getElementById('line-width');
    const lineStyle = document.getElementById('line-style');
    const maskSelect = document.getElementById('mask-style');
    const maskOpacity = document.getElementById('mask-opacity');
    const progressToggle = document.getElementById('progress-toggle');
    const progressColor = document.getElementById('progress-color');
    const progressHeight = document.getElementById('progress-height');
    smoothingSelect?.addEventListener('change', () => {
        const options = collectSmoothingOptions(smoothingSelect, smoothingWindow, smoothingAlpha, smoothingCutoff);
        vis.setSmoothing(options);
        updateSmoothingInputsVisibility(options.kind, smoothingWindow, smoothingAlpha, smoothingCutoff);
    });
    smoothingWindow?.addEventListener('input', () => {
        const options = collectSmoothingOptions(smoothingSelect, smoothingWindow, smoothingAlpha, smoothingCutoff);
        vis.setSmoothing(options);
    });
    smoothingAlpha?.addEventListener('input', () => {
        const options = collectSmoothingOptions(smoothingSelect, smoothingWindow, smoothingAlpha, smoothingCutoff);
        vis.setSmoothing(options);
    });
    smoothingCutoff?.addEventListener('input', () => {
        const options = collectSmoothingOptions(smoothingSelect, smoothingWindow, smoothingAlpha, smoothingCutoff);
        vis.setSmoothing(options);
    });
    lineColor?.addEventListener('input', () => {
        vis.setLineStyle({ color: lineColor.value });
    });
    lineWidth?.addEventListener('input', () => {
        vis.setLineStyle({ width: Number(lineWidth.value) });
    });
    lineStyle?.addEventListener('change', () => {
        const style = lineStyle.value || 'solid';
        vis.setLineStyle({ style });
    });
    maskSelect?.addEventListener('change', () => {
        const mask = maskSelect.value;
        vis.setFutureMask({ kind: mask });
    });
    maskOpacity?.addEventListener('input', () => {
        vis.setFutureMask({ opacity: clamp(Number(maskOpacity.value), 0, 1) });
    });
    progressToggle?.addEventListener('change', () => {
        vis.setProgressBar({ enabled: progressToggle.checked });
    });
    progressColor?.addEventListener('input', () => {
        vis.setProgressBar({ color: progressColor.value });
    });
    progressHeight?.addEventListener('input', () => {
        vis.setProgressBar({ height: Math.max(2, Number(progressHeight.value)) });
    });
    if (smoothingSelect) {
        const options = collectSmoothingOptions(smoothingSelect, smoothingWindow, smoothingAlpha, smoothingCutoff);
        updateSmoothingInputsVisibility(options.kind, smoothingWindow, smoothingAlpha, smoothingCutoff);
    }
}
function updateSmoothingInputsVisibility(kind, windowInput, alphaInput, cutoffInput) {
    if (windowInput) {
        windowInput.parentElement?.classList.toggle('hidden', kind !== 'moving-average');
    }
    if (alphaInput) {
        alphaInput.parentElement?.classList.toggle('hidden', kind !== 'exponential');
    }
    if (cutoffInput) {
        cutoffInput.parentElement?.classList.toggle('hidden', kind !== 'iir-lowpass');
    }
}
function collectSmoothingOptions(select, windowInput, alphaInput, cutoffInput) {
    const kind = select?.value || 'none';
    const options = { kind };
    if (kind === 'moving-average' && windowInput) {
        options.windowSize = Math.max(1, Math.floor(Number(windowInput.value) || 1));
    }
    if (kind === 'exponential' && alphaInput) {
        options.alpha = clamp(Number(alphaInput.value) || 0.2, 0.01, 1);
    }
    if (kind === 'iir-lowpass' && cutoffInput) {
        options.cutoff = clamp(Number(cutoffInput.value) || 0.1, 0.001, 0.5);
    }
    return options;
}
function startDemoStream(vis) {
    const noiseSlider = document.getElementById('noise-amount');
    const speedSlider = document.getElementById('speed-amount');
    const state = {
        phase: 0,
        progress: 0,
        lastTime: 0,
        noiseScale: noiseSlider ? Number(noiseSlider.value) : 0.05,
        speed: speedSlider ? Number(speedSlider.value) : 0.0025
    };
    noiseSlider?.addEventListener('input', () => {
        state.noiseScale = Number(noiseSlider.value);
    });
    speedSlider?.addEventListener('input', () => {
        state.speed = Number(speedSlider.value);
    });
    const loop = (time) => {
        if (!state.lastTime) {
            state.lastTime = time;
        }
        const dt = (time - state.lastTime) || 16;
        state.lastTime = time;
        const speed = state.speed;
        state.phase += dt * speed;
        const base = Math.sin(state.phase * 0.0015) * 0.7 + Math.cos(state.phase * 0.0007) * 0.3;
        const noise = (Math.random() - 0.5) * state.noiseScale;
        vis.addSample(base + noise);
        state.progress += dt * 0.0002;
        if (state.progress > 1) {
            state.progress -= 1;
        }
        vis.setProgress(state.progress);
        requestAnimationFrame(loop);
    };
    requestAnimationFrame(loop);
}
