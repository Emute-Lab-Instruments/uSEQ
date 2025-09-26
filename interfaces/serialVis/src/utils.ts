import { FutureMaskKind, LineStyleKind, LineStyleOptions, SerialVisOptions, defaultOptions } from './options';

export function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

export function resolveOptions(options?: Partial<SerialVisOptions>): SerialVisOptions {
  const resolved: SerialVisOptions = cloneOptions(defaultOptions);
  if (!options) {
    return resolved;
  }

  assignLineOptions(resolved.line, options.line);
  if (typeof options.width === 'number') resolved.width = options.width;
  if (typeof options.height === 'number') resolved.height = options.height;
  if (typeof options.sampleCount === 'number') resolved.sampleCount = Math.max(16, Math.floor(options.sampleCount));
  if (options.valueRange) {
    resolved.valueRange = {
      min: options.valueRange.min ?? resolved.valueRange.min,
      max: options.valueRange.max ?? resolved.valueRange.max
    };
  }
  if (typeof options.initialValue === 'number') resolved.initialValue = options.initialValue;
  if (options.smoothing) resolved.smoothing = { ...resolved.smoothing, ...options.smoothing };
  if (options.futureMask) resolved.futureMask = { ...resolved.futureMask, ...options.futureMask };
  if (options.progressBar) resolved.progressBar = { ...resolved.progressBar, ...options.progressBar };
  if (typeof options.containerBackground === 'string') resolved.containerBackground = options.containerBackground;
  return resolved;
}

export function cloneOptions(source: SerialVisOptions): SerialVisOptions {
  return {
    width: source.width,
    height: source.height,
    sampleCount: source.sampleCount,
    valueRange: { ...source.valueRange },
    initialValue: source.initialValue,
    line: { ...source.line, dashPattern: source.line.dashPattern?.slice() },
    smoothing: { ...source.smoothing },
    futureMask: { ...source.futureMask },
    progressBar: { ...source.progressBar },
    containerBackground: source.containerBackground
  };
}

function assignLineOptions(target: LineStyleOptions, source?: Partial<LineStyleOptions>): void {
  if (!source) return;
  if (typeof source.color === 'string') target.color = source.color;
  if (typeof source.width === 'number') target.width = source.width;
  if (typeof source.style === 'string') target.style = source.style;
  if (source.dashPattern) target.dashPattern = source.dashPattern.slice();
}

export function lineStyleToDash(style: LineStyleKind, custom?: number[]): number[] | undefined {
  if (custom && custom.length) {
    return custom;
  }
  switch (style) {
    case 'dashed':
      return [8, 4];
    case 'dotted':
      return [2, 6];
    case 'solid':
    default:
      return undefined;
  }
}

export function maskOpacity(kind: FutureMaskKind, baseOpacity: number): number {
  const clampedBase = clamp(baseOpacity, 0, 1);
  switch (kind) {
    case 'none':
      return 0;
    case 'solid':
      return clampedBase;
    case 'dim':
    default:
      return Math.min(1, clampedBase * 0.6 + 0.1);
  }
}
