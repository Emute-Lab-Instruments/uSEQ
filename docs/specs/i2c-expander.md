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
The bus operates at 400 kHz. A valid factory record's non-zero default address
wins. Otherwise a build may set `USEQ_I2C_CLIENT_ADDRESS` in `[1,126]`; the
final fallback derives an address from the RP2040 chip ID and maps zero to 1.

1.3 The expander image contains no signal engine or synth engine. It receives
already-evaluated values and maps them to its eight continuous output slots.

1.4 The current RP2040 expander profile declares 2 MiB flash and reserves the
final 4 KiB erase sector at offset `0x1ff000` as its factory partition. The
Arduino Pico core already excludes that EEPROM sector from application UF2s;
the expander runtime exposes no EEPROM write path. This is read-only by
firmware/build policy; external QSPI has no hardware write-protect boundary.

## 2. Discovery

2.1 The host scans addresses 1 through 126 during initialization. For each
acknowledging address it writes the eight ASCII bytes `$gettype` and requests
seven response bytes.

2.2 An output expander responds with `aout08` followed by NUL. The host records
at most five matching addresses. Discovery occurs at host startup and when the
editor sends the explicit JSON `rescan-modules` request; it is not continuous.

2.3 After the legacy type response, a current host writes `$identify` and asks
for the fixed 206-byte identity response. Its schema contains the encoded
factory record, firmware version, exact firmware target, protocol version, and
capability bits. Failure to answer keeps an older expander discoverable.

2.4 A missing or CRC-invalid factory record is not repaired or guessed. The
main reports the module as `unidentified-prototype`; editors must not select a
UF2 automatically. A valid record bubbles product, hardware revision,
assembly variant, batch, unit serial, manufacture date, MCU family, update
transport, and firmware identity through `hello.modules`.

## 3. Factory identity record

3.1 `factory_identity.{h,cpp}` defines a 128-byte little-endian record with
magic `USEQID1`, schema version 1, MCU family (`rp2040` or `rp2350`), default
I2C address, feature bits, product, hardware revision, assembly variant,
batch, serial, manufacture date, flash size, and CRC32 over bytes 0 through
123. CRC provides corruption detection only; it is not authentication. A
default I2C address of zero selects the chip-ID-derived policy, avoiding a
shared fixed address across production units; 1 through 126 requests a fixed
manufacturing-assigned address.

3.2 `scripts/prepare_factory_identity.py` emits a 4 KiB sector image, a
provision-only UF2 at the final sector of the selected flash size, and JSON
metadata for manufacturing review. Flash the identity UF2 once, verify it by
discovery/readback, then flash ordinary application UF2s. Never include a
factory-identity UF2 in the public firmware manifest.

## 4. Value frame

4.1 The host mirrors logical outputs `a1` through `a8` to every discovered
output expander after signal execution on each firmware tick.

4.2 A value frame is exactly:

| Offset | Size | Meaning |
|---|---:|---|
| 0 | 6 | ASCII `$vals` followed by NUL |
| 6 | 1 | value count, 1 through 8 |
| 7 | `count * 8` | IEEE-754 binary64 values, little-endian |

No padding, checksum, or terminator follows the final value. The frame length
must equal `7 + count * 8` exactly.

4.3 The client validates the complete frame before changing any output. A bad
prefix, invalid count, wrong length, or non-finite value rejects the whole
frame. Finite values are clamped to `[0,1]` at the electrical boundary. A
valid partial frame updates its first `count` channels and retains the other
channels.

## 5. Runtime safety

5.1 The hardware receive callback only copies complete Wire transactions into
a fixed-capacity single-producer/single-consumer queue. It does not decode,
allocate, or write GPIO.

5.2 The main firmware tick drains the queue in FIFO order before writing
outputs. If several valid packets are queued, the newest applied packet is the
observable vector for that tick.

5.3 The queue holds four transactions. When full it rejects the newest
transaction and preserves accepted order. Invalid packets, queue overflow,
NACK, and disconnection retain the last applied output vector.

5.4 Host broadcast uses bounded synchronous Wire transactions. One expander
frame is 71 bytes at 400 kHz. This deliberately trades tick cadence for the
required synth-free streaming path; its physical worst-case latency remains a
target measurement, not a native-test claim.

## 6. Evidence boundary

6.1 `test/firmware/test_i2c_expander.cpp` connects production host and client
`I2CNetwork` instances through `MockI2CBus`. It strictly covers build-mode
selection, factory codec/CRC, discovery/type and identity responses, all eight output
values, malformed and non-finite rejection, queue capacity, NACK hold, and
recovery.

6.2 The native fake bus does not exercise the RP2040 Wire peripheral, board
traces, pull-ups, voltage stages, PWM filtering, LED polarity, or connector
orientation. The `bringup-expander-L2` target build and synth-boundary ELF
inspection establish target composition, not those physical properties.
