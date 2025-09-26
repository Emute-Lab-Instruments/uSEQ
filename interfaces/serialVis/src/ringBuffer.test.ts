import { describe, expect, it } from 'vitest';
import { FloatRingBuffer } from './ringBuffer';

describe('FloatRingBuffer', () => {
  it('maintains chronological order after wrap', () => {
    const buffer = new FloatRingBuffer(4, 0);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.push(4);
    buffer.push(5);

    const dest = new Float32Array(4);
    buffer.copyOrdered(dest);
    expect(Array.from(dest)).toEqual([2, 3, 4, 5]);
  });

  it('pads values with initial value before wrapping', () => {
    const buffer = new FloatRingBuffer(3, -1);
    buffer.push(10);
    buffer.push(20);
    const dest = new Float32Array(3);
    buffer.copyOrdered(dest);
    expect(Array.from(dest)).toEqual([10, 20, 20]);
  });

  it('fill replaces buffer contents', () => {
    const buffer = new FloatRingBuffer(3, 0);
    buffer.push(5);
    buffer.push(6);
    buffer.fill(2);
    const dest = new Float32Array(3);
    buffer.copyOrdered(dest);
    expect(Array.from(dest)).toEqual([2, 2, 2]);
  });
});
