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
  valueRange: { min: number; max: number };
  initialValue?: number;
  line: LineStyleOptions;
  smoothing: SmoothingOptions;
  futureMask: FutureMaskOptions;
  progressBar: ProgressBarOptions;
  containerBackground?: string;
}

export const defaultOptions: SerialVisOptions = {
  width: 640,
  height: 240,
  sampleCount: 512,
  valueRange: { min: -1, max: 1 },
  initialValue: 0,
  line: {
    color: '#7cf0ff',
    width: 2,
    style: 'solid'
  },
  smoothing: {
    kind: 'moving-average',
    windowSize: 5
  },
  futureMask: {
    kind: 'dim',
    color: '#0f172a',
    opacity: 0.25
  },
  progressBar: {
    enabled: true,
    color: '#38bdf8',
    height: 6,
    backgroundColor: 'rgba(56, 189, 248, 0.1)'
  },
  containerBackground: 'transparent'
};
