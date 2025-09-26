import { describe, expect, it } from 'vitest';
import { SmootherFactory } from './smoothing';

const SAMPLE = Float32Array.from([0, 4, 8, 4, 0]);

describe('Smoothing strategies', () => {
  it('none returns original data', () => {
    const smoother = SmootherFactory.create({ kind: 'none' });
    const dest = new Float32Array(SAMPLE.length);
    smoother.apply(SAMPLE, dest);
    expect(Array.from(dest)).toEqual(Array.from(SAMPLE));
  });

  it('moving average smooths peaks', () => {
    const smoother = SmootherFactory.create({ kind: 'moving-average', windowSize: 3 });
    const dest = new Float32Array(SAMPLE.length);
    smoother.apply(SAMPLE, dest);
    expect(dest[2]).toBeLessThan(SAMPLE[2]);
    expect(dest[2]).toBeGreaterThan(dest[1]);
  });

  it('exponential smoother reacts to new values while damping noise', () => {
    const smoother = SmootherFactory.create({ kind: 'exponential', alpha: 0.3 });
    const dest = new Float32Array(SAMPLE.length);
    smoother.apply(SAMPLE, dest);
    expect(dest[1]).toBeGreaterThan(dest[0]);
    expect(dest[2]).toBeLessThan(SAMPLE[2]);
  });

  it('low-pass smoothing converges to lower frequency components', () => {
    const smoother = SmootherFactory.create({ kind: 'iir-lowpass', cutoff: 0.05 });
    const dest = new Float32Array(SAMPLE.length);
    smoother.apply(SAMPLE, dest);
    const sourceDelta = Math.abs(SAMPLE[3] - SAMPLE[2]);
    const smoothedDelta = Math.abs(dest[3] - dest[2]);
    expect(smoothedDelta).toBeLessThan(sourceDelta);
  });
});
