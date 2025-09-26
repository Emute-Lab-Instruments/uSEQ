export class FloatRingBuffer {
    constructor(capacity, initialValue = 0) {
        this.capacity = capacity;
        this.index = 0;
        this.filled = false;
        if (capacity <= 0 || !Number.isFinite(capacity)) {
            throw new Error('Ring buffer capacity must be a positive finite number');
        }
        this.initialValue = initialValue;
        this.data = new Float32Array(capacity);
        this.data.fill(initialValue);
    }
    push(value) {
        this.data[this.index] = value;
        this.index = (this.index + 1) % this.capacity;
        if (!this.filled && this.index === 0) {
            this.filled = true;
        }
    }
    copyOrdered(destination) {
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
    fill(value) {
        this.data.fill(value);
        this.index = 0;
        this.filled = true;
    }
    getCurrentValue() {
        const pos = (this.index - 1 + this.capacity) % this.capacity;
        return this.data[pos];
    }
    get length() {
        return this.capacity;
    }
}
