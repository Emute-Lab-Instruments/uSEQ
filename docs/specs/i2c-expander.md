# I2C Output Expander

> Spec: host discovery and the synth-free eight-output expander transport.
> This is a control-rate transport between RP2040 modules, not a ModuLisp or
> synthesis execution profile.

## 1. Build and hardware profile

1.1 `bringup-expander-L2` defines `USEQHARDWARE_EXPANDER_OUT_0_1`,
`ENABLE_I2C_NETWORKING`, and `ENABLE_I2C_CLIENT`. The firmware selects client
mode at boot. A primary hardware profile selects host mode even where its
historical build flags also define client support.

1.2 The expander I2C pins are SDA GPIO4 and SCL GPIO1. GPIO0 remains LED 8.
The bus operates at 400 kHz. A build may set `USEQ_I2C_CLIENT_ADDRESS` to an
address in `[1,126]`; otherwise hardware derives the address from the final
byte of the RP2040 chip ID and maps zero to address 1.

1.3 The expander image contains no signal engine or synth engine. It receives
already-evaluated values and maps them to its eight continuous output slots.

## 2. Discovery

2.1 The host scans addresses 1 through 126 during initialization. For each
acknowledging address it writes the eight ASCII bytes `$gettype` and requests
seven response bytes.

2.2 An output expander responds with `aout08` followed by NUL. The host records
at most five matching addresses. Discovery currently occurs only at boot.

## 3. Value frame

3.1 The host mirrors logical outputs `a1` through `a8` to every discovered
output expander after signal execution on each firmware tick.

3.2 A value frame is exactly:

| Offset | Size | Meaning |
|---|---:|---|
| 0 | 6 | ASCII `$vals` followed by NUL |
| 6 | 1 | value count, 1 through 8 |
| 7 | `count * 8` | IEEE-754 binary64 values, little-endian |

No padding, checksum, or terminator follows the final value. The frame length
must equal `7 + count * 8` exactly.

3.3 The client validates the complete frame before changing any output. A bad
prefix, invalid count, wrong length, or non-finite value rejects the whole
frame. Finite values are clamped to `[0,1]` at the electrical boundary. A
valid partial frame updates its first `count` channels and retains the other
channels.

## 4. Runtime safety

4.1 The hardware receive callback only copies complete Wire transactions into
a fixed-capacity single-producer/single-consumer queue. It does not decode,
allocate, or write GPIO.

4.2 The main firmware tick drains the queue in FIFO order before writing
outputs. If several valid packets are queued, the newest applied packet is the
observable vector for that tick.

4.3 The queue holds four transactions. When full it rejects the newest
transaction and preserves accepted order. Invalid packets, queue overflow,
NACK, and disconnection retain the last applied output vector.

4.4 Host broadcast uses bounded synchronous Wire transactions. One expander
frame is 71 bytes at 400 kHz. This deliberately trades tick cadence for the
required synth-free streaming path; its physical worst-case latency remains a
target measurement, not a native-test claim.

## 5. Evidence boundary

5.1 `test/firmware/test_i2c_expander.cpp` connects production host and client
`I2CNetwork` instances through `MockI2CBus`. It strictly covers build-mode
selection, discovery/type response, frame encoding/decoding, all eight output
values, malformed and non-finite rejection, queue capacity, NACK hold, and
recovery.

5.2 The native fake bus does not exercise the RP2040 Wire peripheral, board
traces, pull-ups, voltage stages, PWM filtering, LED polarity, or connector
orientation. The `bringup-expander-L2` target build and synth-boundary ELF
inspection establish target composition, not those physical properties.
