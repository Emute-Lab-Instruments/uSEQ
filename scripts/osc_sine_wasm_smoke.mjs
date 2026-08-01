#!/usr/bin/env -S node --no-warnings
//
// Runtime smoke test for the osc/sine NodeDef WASM artefact.
//
// VAL-DSP-005 (host-owned imported shared memory), VAL-DSP-007 (dynamic
// quantum), VAL-DSP-008 (measured amplitude), and VAL-DSP-009 (finite
// output) are exercised at runtime here, complementing the binary-inspection
// contract test in scripts/inspect_osc_sine_wasm.sh.
//
// Usage: node scripts/osc_sine_wasm_smoke.mjs [path/to/osc_sine.wasm]
//
// Exits 0 on success, non-zero on any contract violation.

import * as fs from 'node:fs';

const wasmPath = process.argv[2]
    || new URL('../wasm/osc_sine.wasm', import.meta.url).pathname;

if (!fs.existsSync(wasmPath)) {
    console.error(`FAIL: ${wasmPath} not found (run nodedef/build_osc_sine_wasm.sh)`);
    process.exit(1);
}

const wasmBytes = fs.readFileSync(wasmPath);

// Host-owned shared memory. 256 pages = 16 MiB (matches the WASM import spec).
// Node supports shared:true but the import path is identical either way; we
// test with shared:false here because the Node smoke test runs single-threaded
// and the browser is the real cross-thread consumer.
const memory = new WebAssembly.Memory({ initial: 256, maximum: 256 });

const { instance } = await WebAssembly.instantiate(wasmBytes, {
    env: { memory },
});

const e = instance.exports;
const STATE_BYTES = e.osc_sine_state_bytes();
const STATE_ALIGN = e.osc_sine_state_align();

console.log(`state_bytes=${STATE_BYTES} state_align=${STATE_ALIGN}`);
console.log(`fade_in_ms=${e.osc_sine_fade_in_ms()} fade_out_ms=${e.osc_sine_fade_out_ms()}`);
console.log(`min_quantum=${e.osc_sine_min_quantum()} max_quantum=${e.osc_sine_max_quantum()}`);
console.log(`sample_rate=${e.osc_sine_sample_rate()}`);

if (e.osc_sine_sample_rate_abi_version() !== 1) {
    console.error(`FAIL: unsupported sample-rate ABI ${e.osc_sine_sample_rate_abi_version()}`);
    process.exit(1);
}

// ── Registry metadata check (VAL-DSP-001) ────────────────────────────────
const regJsonPtr = e.osc_sine_registry_json();
const memView = new DataView(memory.buffer);
let regJson = '';
for (let i = regJsonPtr; i < memory.buffer.byteLength; i++) {
    const c = memView.getUint8(i);
    if (c === 0) break;
    regJson += String.fromCharCode(c);
}
console.log(`registry_json=${regJson}`);

const requiredJsonSubstrings = [
    '"name":"osc/sine"', '"version":2',
    '"audio_inputs":1', '"audio_input_names":["fm"]',
    '"audio_outputs":1', '"voice_fanout":false',
    '"name":"freq"', '"name":"amp"',
    '"fade_in_ms":10', '"fade_out_ms":30',
];
for (const s of requiredJsonSubstrings) {
    if (!regJson.includes(s)) {
        console.error(`FAIL: registry JSON missing ${s}`);
        process.exit(1);
    }
}

// ── Layout & init ────────────────────────────────────────────────────────
const stateOffset = 64; // 8-byte aligned, well inside the imported memory
if (e.osc_sine_validate_layout(stateOffset, STATE_BYTES) !== 1) {
    console.error('FAIL: validate_layout rejected the state offset');
    process.exit(1);
}
// ── Render 1 second of 440 Hz + 110 Hz FM at common render rates ────────
// VAL-DSP-007 (dynamic quantum), VAL-DSP-008 (measured amp),
// VAL-DSP-009 (finite output).
const FRAMES = 1920;
const heap = new Float64Array(memory.buffer);
const freqOffsetDoubles = 0;  // first double in the heap
const ampOffsetDoubles = 1;   // second double
const outOffsetDoubles = 4096; // separate region, 32 KB into the heap
const fmOffsetDoubles = 8192;

heap[freqOffsetDoubles] = 440.0;
heap[ampOffsetDoubles] = 0.2;

for (const renderRate of [44100, 48000, 96000]) {
    if (e.osc_sine_init(stateOffset, STATE_BYTES) !== 1) {
        console.error(`FAIL: init failed at ${renderRate} Hz`);
        process.exit(1);
    }

    let peak = 0.0;
    let crossings = 0;
    let prevSign = 0;
    let rendered = 0;
    while (rendered < renderRate) {
        const frameCount = Math.min(FRAMES, renderRate - rendered);
        heap.fill(110.0, fmOffsetDoubles, fmOffsetDoubles + frameCount);
        const ok = e.osc_sine_compute_fm_at_sample_rate(
            stateOffset,
            freqOffsetDoubles * 8,
            ampOffsetDoubles * 8,
            fmOffsetDoubles * 8,
            outOffsetDoubles * 8,
            frameCount,
            renderRate
        );
        if (ok !== 1) {
            console.error(`FAIL: compute returned ${ok} at ${renderRate} Hz frame ${rendered}`);
            process.exit(1);
        }
        for (let i = 0; i < frameCount; i++) {
            const v = heap[outOffsetDoubles + i];
            if (!Number.isFinite(v)) {
                console.error(`FAIL: non-finite sample at ${renderRate} Hz frame ${rendered + i}: ${v}`);
                process.exit(1);
            }
            const a = Math.abs(v);
            if (a > peak) peak = a;
            const sign = v > 0 ? 1 : (v < 0 ? -1 : 0);
            if (sign !== 0) {
                if (prevSign !== 0 && sign !== prevSign) crossings++;
                prevSign = sign;
            }
        }
        rendered += frameCount;
    }

    console.log(`render_rate=${renderRate} peak=${peak.toFixed(4)} zero_crossings=${crossings}`);

    // Peak should land near 0.2 (target amp); the first block ramps from
    // silence, so peak across 1 second approaches but does not quite hit it.
    if (peak < 0.18 || peak > 0.22) {
        console.error(`FAIL: peak ${peak} outside [0.18, 0.22] at ${renderRate} Hz`);
        process.exit(1);
    }

    // Base 440 + FM 110 is 550 Hz. Allow 5% for edge effects.
    const measuredHz = crossings / 2;
    if (Math.abs(measuredHz - 550) > 27.5) {
        console.error(`FAIL: measured frequency ${measuredHz} Hz at ${renderRate} Hz render rate`);
        process.exit(1);
    }
}

console.log('PASS: NodeDef WASM FM path preserves pitch at 44.1, 48, and 96 kHz');
