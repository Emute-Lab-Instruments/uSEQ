import { SmoothingOptions } from './smoothing';
import { SerialVisOptions, LineStyleOptions, FutureMaskOptions, ProgressBarOptions } from './options';
export interface SerialVisStateSnapshot {
    progress: number;
    latestValue: number;
    options: SerialVisOptions;
}
export declare class SerialVis {
    private options;
    private readonly container;
    private two;
    private buffer;
    private rawValues;
    private displayValues;
    private smoother;
    private linePath;
    private maskPath;
    private progressBackground?;
    private progressPath?;
    private needsRedraw;
    private progress;
    private isRunning;
    private readonly updateHandler;
    constructor(container: HTMLElement, options?: Partial<SerialVisOptions>);
    addSample(value: number): void;
    setSamples(values: ArrayLike<number>): void;
    setProgress(value: number): void;
    setSmoothing(options: SmoothingOptions): void;
    setLineStyle(options: Partial<LineStyleOptions>): void;
    setFutureMask(options: Partial<FutureMaskOptions>): void;
    setProgressBar(options: Partial<ProgressBarOptions>): void;
    resize(dimensions: {
        width?: number;
        height?: number;
        sampleCount?: number;
        valueRange?: {
            min?: number;
            max?: number;
        };
    }): void;
    updateOptions(options: Partial<SerialVisOptions>): void;
    snapshot(): SerialVisStateSnapshot;
    start(): void;
    stop(): void;
    dispose(): void;
    private applyOptions;
    private createRenderer;
    private rebuild;
    private buildScene;
    private createLinePath;
    private get graphHeight();
    private get graphTop();
    private getValueY;
    private applyLineStyle;
    private createMaskPath;
    private applyMaskStyle;
    private buildProgressBar;
    private onUpdate;
    private updateLinePath;
    private updateMask;
    private updateProgressPath;
}
