export declare class FloatRingBuffer {
    private readonly capacity;
    private readonly data;
    private index;
    private filled;
    private readonly initialValue;
    constructor(capacity: number, initialValue?: number);
    push(value: number): void;
    copyOrdered(destination: Float32Array): void;
    fill(value: number): void;
    getCurrentValue(): number;
    get length(): number;
}
