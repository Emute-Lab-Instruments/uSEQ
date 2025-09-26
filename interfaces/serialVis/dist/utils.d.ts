import { FutureMaskKind, LineStyleKind, SerialVisOptions } from './options';
export declare function clamp(value: number, min: number, max: number): number;
export declare function resolveOptions(options?: Partial<SerialVisOptions>): SerialVisOptions;
export declare function cloneOptions(source: SerialVisOptions): SerialVisOptions;
export declare function lineStyleToDash(style: LineStyleKind, custom?: number[]): number[] | undefined;
export declare function maskOpacity(kind: FutureMaskKind, baseOpacity: number): number;
