import { SerialVis } from './serialVis';
import { SmoothingKind, SmoothingOptions } from './smoothing';
import { FutureMaskKind, LineStyleKind } from './options';
import { clamp } from './utils';

declare global {
  interface Window {
    serialVis?: SerialVis;
  }
}

type NumericInput = HTMLInputElement & { value: string };

function bootstrap(): void {
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
} else {
  bootstrap();
}

function setupSettings(vis: SerialVis): void {
  const smoothingSelect = document.getElementById('smoothing-select') as HTMLSelectElement | null;
  const smoothingWindow = document.getElementById('smoothing-window') as NumericInput | null;
  const smoothingAlpha = document.getElementById('smoothing-alpha') as NumericInput | null;
  const smoothingCutoff = document.getElementById('smoothing-cutoff') as NumericInput | null;

  const lineColor = document.getElementById('line-color') as HTMLInputElement | null;
  const lineWidth = document.getElementById('line-width') as NumericInput | null;
  const lineStyle = document.getElementById('line-style') as HTMLSelectElement | null;

  const maskSelect = document.getElementById('mask-style') as HTMLSelectElement | null;
  const maskOpacity = document.getElementById('mask-opacity') as NumericInput | null;

  const progressToggle = document.getElementById('progress-toggle') as HTMLInputElement | null;
  const progressColor = document.getElementById('progress-color') as HTMLInputElement | null;
  const progressHeight = document.getElementById('progress-height') as NumericInput | null;

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
    const style = (lineStyle.value as LineStyleKind) || 'solid';
    vis.setLineStyle({ style });
  });

  maskSelect?.addEventListener('change', () => {
    const mask = maskSelect.value as FutureMaskKind;
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

function updateSmoothingInputsVisibility(
  kind: SmoothingKind,
  windowInput: NumericInput | null,
  alphaInput: NumericInput | null,
  cutoffInput: NumericInput | null
): void {
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

function collectSmoothingOptions(
  select: HTMLSelectElement | null,
  windowInput: NumericInput | null,
  alphaInput: NumericInput | null,
  cutoffInput: NumericInput | null
): SmoothingOptions {
  const kind = (select?.value as SmoothingKind) || 'none';
  const options: SmoothingOptions = { kind };
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

function startDemoStream(vis: SerialVis): void {
  const noiseSlider = document.getElementById('noise-amount') as NumericInput | null;
  const speedSlider = document.getElementById('speed-amount') as NumericInput | null;

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

  const loop = (time: number) => {
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
