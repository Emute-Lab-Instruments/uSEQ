export class FloatRingBuffer {
  private readonly data: Float32Array;
  private index = 0;
  private filled = false;
  private readonly initialValue: number;

  constructor(private readonly capacity: number, initialValue = 0) {
    if (capacity <= 0 || !Number.isFinite(capacity)) {
      throw new Error('Ring buffer capacity must be a positive finite number');
    }
    this.initialValue = initialValue;
    this.data = new Float32Array(capacity);
    this.data.fill(initialValue);
  }

  push(value: number): void {
    this.data[this.index] = value;
    this.index = (this.index + 1) % this.capacity;
    if (!this.filled && this.index === 0) {
      this.filled = true;
    }
  }

  copyOrdered(destination: Float32Array): void {
    if (destination.length < this.capacity) {
      throw new Error('Destination array is too small for buffer copy');
    }

    if (!this.filled) {
      // Not wrapped yet: copy written values in order, pad the rest with last known value.
      const lastValue = this.index > 0 ? this.data[(this.index - 1)] : this.initialValue;
      let destPos = 0;
      for (let i = 0; i < this.index; i++) {
        destination[destPos++] = this.data[i];
      }
      for (let i = this.index; i < this.capacity; i++) {
        destination[destPos++] = lastValue;
      }
      return;
    }

    const start = this.index;
    for (let i = 0; i < this.capacity; i++) {
      const sourceIndex = (start + i) % this.capacity;
      destination[i] = this.data[sourceIndex];
    }
  }

  fill(value: number): void {
    this.data.fill(value);
    this.index = 0;
    this.filled = true;
  }

  getCurrentValue(): number {
    const pos = (this.index - 1 + this.capacity) % this.capacity;
    return this.data[pos];
  }

  get length(): number {
    return this.capacity;
  }
}
