export type SmoothingKind = 'none' | 'moving-average' | 'exponential' | 'iir-lowpass';
export interface SmoothingOptions {
    kind: SmoothingKind;
    windowSize?: number;
    alpha?: number;
    cutoff?: number;
}
export interface Smoother {
    readonly kind: SmoothingKind;
    apply(input: Float32Array, output: Float32Array): void;
    configure(options: SmoothingOptions): void;
}
export declare class SmootherFactory {
    static create(options: SmoothingOptions): Smoother;
}
