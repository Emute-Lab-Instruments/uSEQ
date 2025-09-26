import Two from 'two.js';
import { FloatRingBuffer } from './ringBuffer';
import { Smoother, SmootherFactory, SmoothingOptions } from './smoothing';
import { SerialVisOptions, LineStyleOptions, FutureMaskOptions, ProgressBarOptions } from './options';
import { clamp, resolveOptions, lineStyleToDash, maskOpacity, cloneOptions } from './utils';

type RendererWithDom = { domElement?: HTMLElement; setSize?: (width: number, height: number) => void };

function makeAnchor(x: number, y: number, command: Two.Commands = Two.Commands.line): Two.Anchor {
  return new Two.Anchor(x, y, 0, 0, 0, 0, command);
}

function getRendererDom(renderer: Two.Renderer | undefined): HTMLElement | null {
  if (!renderer) {
    return null;
  }
  const candidate = (renderer as unknown as RendererWithDom).domElement;
  return candidate ?? null;
}

function setRendererSize(renderer: Two.Renderer | undefined, width: number, height: number): void {
  const setter = (renderer as unknown as RendererWithDom)?.setSize;
  if (typeof setter === 'function') {
    setter.call(renderer, width, height);
  }
}

export interface SerialVisStateSnapshot {
  progress: number;
  latestValue: number;
  options: SerialVisOptions;
}

const GRAPH_MARGIN = 8;

export class SerialVis {
  private options: SerialVisOptions;
  private readonly container: HTMLElement;
  private two: Two;
  private buffer: FloatRingBuffer;
  private rawValues: Float32Array;
  private displayValues: Float32Array;
  private smoother: Smoother;
  private linePath!: Two.Path;
  private maskPath!: Two.Path;
  private progressBackground?: Two.Path;
  private progressPath?: Two.Path;
  private needsRedraw = true;
  private progress = 0;
  private isRunning = false;
  private readonly updateHandler = () => this.onUpdate();

  constructor(container: HTMLElement, options?: Partial<SerialVisOptions>) {
    if (!container) {
      throw new Error('SerialVis requires a container element');
    }
    this.container = container;
    this.options = resolveOptions(options);
    this.buffer = new FloatRingBuffer(this.options.sampleCount, this.options.initialValue ?? 0);
    this.rawValues = new Float32Array(this.options.sampleCount);
    this.displayValues = new Float32Array(this.options.sampleCount);
    this.smoother = SmootherFactory.create(this.options.smoothing);
    this.two = this.createRenderer(this.options);
    this.buildScene();
    this.start();
  }

  addSample(value: number): void {
    if (!Number.isFinite(value)) {
      return;
    }
    this.buffer.push(value);
    this.needsRedraw = true;
  }

  setSamples(values: ArrayLike<number>): void {
    const len = Math.min(values.length, this.buffer.length);
    for (let i = 0; i < len; i++) {
      this.buffer.push(values[i]);
    }
    this.needsRedraw = true;
  }

  setProgress(value: number): void {
    const clamped = clamp(value, 0, 1);
    if (clamped !== this.progress) {
      this.progress = clamped;
      this.needsRedraw = true;
    }
  }

  setSmoothing(options: SmoothingOptions): void {
    this.options.smoothing = { ...this.options.smoothing, ...options };
    this.smoother = SmootherFactory.create(this.options.smoothing);
    this.needsRedraw = true;
  }

  setLineStyle(options: Partial<LineStyleOptions>): void {
    this.options.line = { ...this.options.line, ...options };
    this.applyLineStyle();
    this.needsRedraw = true;
  }

  setFutureMask(options: Partial<FutureMaskOptions>): void {
    this.options.futureMask = { ...this.options.futureMask, ...options };
    this.applyMaskStyle();
    this.needsRedraw = true;
  }

  setProgressBar(options: Partial<ProgressBarOptions>): void {
    this.options.progressBar = { ...this.options.progressBar, ...options };
    this.buildProgressBar();
    this.needsRedraw = true;
  }

  resize(dimensions: { width?: number; height?: number; sampleCount?: number; valueRange?: { min?: number; max?: number } }): void {
    const updated: Partial<SerialVisOptions> = { ...dimensions } as Partial<SerialVisOptions>;
    if (dimensions.valueRange) {
      updated.valueRange = {
        min: dimensions.valueRange.min ?? this.options.valueRange.min,
        max: dimensions.valueRange.max ?? this.options.valueRange.max
      };
    }
    this.applyOptions(updated, true);
  }

  updateOptions(options: Partial<SerialVisOptions>): void {
    this.applyOptions(options, false);
  }

  snapshot(): SerialVisStateSnapshot {
    return {
      progress: this.progress,
      latestValue: this.buffer.getCurrentValue(),
      options: cloneOptions(this.options)
    };
  }

  start(): void {
    if (this.isRunning) {
      return;
    }
    this.two.bind('update', this.updateHandler);
    this.two.play();
    this.isRunning = true;
  }

  stop(): void {
    if (!this.isRunning) {
      return;
    }
    this.two.unbind('update', this.updateHandler);
    this.two.pause();
    this.isRunning = false;
  }

  dispose(): void {
    this.stop();
    this.two.clear();
    const dom = getRendererDom(this.two.renderer);
    if (dom && dom.parentElement === this.container) {
      this.container.removeChild(dom);
    }
  }

  private applyOptions(options: Partial<SerialVisOptions>, forceRebuild: boolean): void {
    let needsRebuild = forceRebuild;
    if (typeof options.width === 'number' && options.width !== this.options.width) {
      this.options.width = options.width;
      needsRebuild = true;
    }
    if (typeof options.height === 'number' && options.height !== this.options.height) {
      this.options.height = options.height;
      needsRebuild = true;
    }
    if (typeof options.sampleCount === 'number' && options.sampleCount !== this.options.sampleCount) {
      this.options.sampleCount = Math.max(16, Math.floor(options.sampleCount));
      needsRebuild = true;
    }
    if (options.valueRange) {
      this.options.valueRange = {
        min: options.valueRange.min ?? this.options.valueRange.min,
        max: options.valueRange.max ?? this.options.valueRange.max
      };
    }
    if (typeof options.initialValue === 'number') {
      this.options.initialValue = options.initialValue;
      this.buffer.fill(options.initialValue);
      this.needsRedraw = true;
    }
    if (options.smoothing) {
      this.options.smoothing = { ...this.options.smoothing, ...options.smoothing };
      this.smoother = SmootherFactory.create(this.options.smoothing);
      this.needsRedraw = true;
    }
    if (options.line) {
      this.options.line = { ...this.options.line, ...options.line };
      this.applyLineStyle();
      this.needsRedraw = true;
    }
    if (options.futureMask) {
      this.options.futureMask = { ...this.options.futureMask, ...options.futureMask };
      this.applyMaskStyle();
      this.needsRedraw = true;
    }
    if (options.progressBar) {
      this.options.progressBar = { ...this.options.progressBar, ...options.progressBar };
      this.buildProgressBar();
      this.needsRedraw = true;
    }
    if (typeof options.containerBackground === 'string') {
      this.options.containerBackground = options.containerBackground;
      const dom = getRendererDom(this.two.renderer);
      if (dom) {
        dom.style.background = options.containerBackground;
      }
    }

    if (needsRebuild) {
      this.rebuild();
    }
  }

  private createRenderer(options: SerialVisOptions): Two {
    const instance = new Two({
      type: Two.Types.canvas,
      width: options.width,
      height: options.height,
      autostart: false
    });
    instance.appendTo(this.container);
    const dom = getRendererDom(instance.renderer);
    if (dom) {
      dom.style.background = options.containerBackground ?? 'transparent';
    }
    return instance;
  }

  private rebuild(): void {
    this.stop();
    this.two.clear();
    this.buffer = new FloatRingBuffer(this.options.sampleCount, this.options.initialValue ?? 0);
    this.rawValues = new Float32Array(this.options.sampleCount);
    this.displayValues = new Float32Array(this.options.sampleCount);
    this.smoother = SmootherFactory.create(this.options.smoothing);
    this.two.width = this.options.width;
    this.two.height = this.options.height;
    setRendererSize(this.two.renderer, this.options.width, this.options.height);
    this.buildScene();
    this.needsRedraw = true;
    this.start();
  }

  private buildScene(): void {
    this.linePath = this.createLinePath();
    this.applyLineStyle();
    this.maskPath = this.createMaskPath();
    this.applyMaskStyle();
    this.buildProgressBar();
    this.needsRedraw = true;
  }

  private createLinePath(): Two.Path {
    const vertices: Two.Anchor[] = [];
    const width = this.options.width - GRAPH_MARGIN * 2;
    const sampleCount = this.options.sampleCount;
    const dx = sampleCount > 1 ? width / (sampleCount - 1) : width;
    const baseY = this.getValueY(this.options.initialValue ?? 0);
    for (let i = 0; i < sampleCount; i++) {
      const command = i === 0 ? Two.Commands.move : Two.Commands.line;
      vertices.push(makeAnchor(GRAPH_MARGIN + dx * i, baseY, command));
    }
    const path = new Two.Path(vertices, false, false);
    path.noFill();
    path.cap = 'round';
    path.join = 'round';
    this.two.add(path);
    return path;
  }

  private get graphHeight(): number {
    const progressSpace = this.options.progressBar.enabled ? this.options.progressBar.height + GRAPH_MARGIN : 0;
    return this.options.height - GRAPH_MARGIN * 2 - progressSpace;
  }

  private get graphTop(): number {
    return GRAPH_MARGIN;
  }

  private getValueY(value: number): number {
    const { min, max } = this.options.valueRange;
    const range = max - min || 1;
    const normalized = clamp((value - min) / range, 0, 1);
    const top = this.graphTop;
    const height = this.graphHeight;
    return top + (1 - normalized) * height;
  }

  private applyLineStyle(): void {
    if (!this.linePath) return;
    const style = this.options.line;
    this.linePath.stroke = style.color;
    this.linePath.linewidth = style.width;
    const dash = lineStyleToDash(style.style, style.dashPattern);
    const dashes = this.linePath.dashes;
    dashes.length = 0;
    if (dash) {
      for (let i = 0; i < dash.length; i++) {
        dashes.push(dash[i]);
      }
    }
  }

  private createMaskPath(): Two.Path {
    const top = this.graphTop;
    const bottom = top + this.graphHeight;
    const vertices = [
      makeAnchor(this.options.width, top, Two.Commands.move),
      makeAnchor(this.options.width, bottom, Two.Commands.line),
      makeAnchor(this.options.width, bottom, Two.Commands.line),
      makeAnchor(this.options.width, top, Two.Commands.line)
    ];
    const path = new Two.Path(vertices, true, false);
    path.noStroke();
    this.two.add(path);
    return path;
  }

  private applyMaskStyle(): void {
    if (!this.maskPath) return;
    const maskOpts = this.options.futureMask;
    this.maskPath.fill = maskOpts.color;
    const opacity = maskOpacity(maskOpts.kind, maskOpts.opacity);
    this.maskPath.opacity = opacity;
  }

  private buildProgressBar(): void {
    if (this.progressBackground) {
      this.two.remove(this.progressBackground);
      this.progressBackground = undefined;
    }
    if (this.progressPath) {
      this.two.remove(this.progressPath);
      this.progressPath = undefined;
    }

    if (!this.options.progressBar.enabled) {
      return;
    }
    const height = this.options.progressBar.height;
    const yTop = this.options.height - GRAPH_MARGIN - height;
    const yBottom = this.options.height - GRAPH_MARGIN;
    const backgroundVertices = [
      makeAnchor(GRAPH_MARGIN, yTop, Two.Commands.move),
      makeAnchor(GRAPH_MARGIN, yBottom, Two.Commands.line),
      makeAnchor(this.options.width - GRAPH_MARGIN, yBottom, Two.Commands.line),
      makeAnchor(this.options.width - GRAPH_MARGIN, yTop, Two.Commands.line)
    ];
    const backgroundPath = new Two.Path(backgroundVertices, true, false);
    backgroundPath.noStroke();
    backgroundPath.fill = this.options.progressBar.backgroundColor;
    this.two.add(backgroundPath);

    const progressVertices = [
      makeAnchor(GRAPH_MARGIN, yTop, Two.Commands.move),
      makeAnchor(GRAPH_MARGIN, yBottom, Two.Commands.line),
      makeAnchor(GRAPH_MARGIN, yBottom, Two.Commands.line),
      makeAnchor(GRAPH_MARGIN, yTop, Two.Commands.line)
    ];
    const progressPath = new Two.Path(progressVertices, true, false);
    progressPath.noStroke();
    progressPath.fill = this.options.progressBar.color;
    this.two.add(progressPath);

    this.progressBackground = backgroundPath;
    this.progressPath = progressPath;
  }

  private onUpdate(): void {
    if (!this.needsRedraw) {
      return;
    }
    this.buffer.copyOrdered(this.rawValues);
    this.smoother.apply(this.rawValues, this.displayValues);
    this.updateLinePath();
    this.updateMask();
    this.updateProgressPath();
    this.needsRedraw = false;
  }

  private updateLinePath(): void {
    const vertices = this.linePath.vertices;
    const sampleCount = this.options.sampleCount;
    if (vertices.length !== sampleCount) {
      return;
    }
    const width = this.options.width - GRAPH_MARGIN * 2;
    const dx = sampleCount > 1 ? width / (sampleCount - 1) : width;
    for (let i = 0; i < sampleCount; i++) {
      const vertex = vertices[i];
      vertex.x = GRAPH_MARGIN + dx * i;
      vertex.y = this.getValueY(this.displayValues[i]);
    }
  }

  private updateMask(): void {
    const mask = this.maskPath;
    if (mask.opacity <= 0.001) {
      return;
    }
    const top = this.graphTop;
    const bottom = top + this.graphHeight;
    const width = this.options.width - GRAPH_MARGIN * 2;
    const left = GRAPH_MARGIN + this.progress * width;
    const vertices = mask.vertices;
    vertices[0].x = left;
    vertices[0].y = top;
    vertices[1].x = left;
    vertices[1].y = bottom;
    vertices[2].x = this.options.width - GRAPH_MARGIN;
    vertices[2].y = bottom;
    vertices[3].x = this.options.width - GRAPH_MARGIN;
    vertices[3].y = top;
  }

  private updateProgressPath(): void {
    if (!this.progressPath) {
      return;
    }
    const progress = clamp(this.progress, 0, 1);
    const width = this.options.width - GRAPH_MARGIN * 2;
    const x = GRAPH_MARGIN + width * progress;
    const vertices = this.progressPath.vertices;
    vertices[2].x = x;
    vertices[3].x = x;
  }
}
