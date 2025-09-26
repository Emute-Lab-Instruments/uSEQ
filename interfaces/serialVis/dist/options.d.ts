import { SmoothingOptions } from './smoothing';
export type LineStyleKind = 'solid' | 'dashed' | 'dotted';
export type FutureMaskKind = 'none' | 'dim' | 'solid';
export interface LineStyleOptions {
    color: string;
    width: number;
    style: LineStyleKind;
    dashPattern?: number[];
}
export interface FutureMaskOptions {
    kind: FutureMaskKind;
    color: string;
    opacity: number;
}
export interface ProgressBarOptions {
    enabled: boolean;
    color: string;
    height: number;
    backgroundColor: string;
}
export interface SerialVisOptions {
    width: number;
    height: number;
    sampleCount: number;
    valueRange: {
        min: number;
        max: number;
    };
    initialValue?: number;
    line: LineStyleOptions;
    smoothing: SmoothingOptions;
    futureMask: FutureMaskOptions;
    progressBar: ProgressBarOptions;
    containerBackground?: string;
}
export declare const defaultOptions: SerialVisOptions;
