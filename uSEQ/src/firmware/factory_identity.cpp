#include "factory_identity.h"

#include <cstring>

namespace firmware
{
namespace
{
constexpr uint8_t MAGIC[8] = { 'U', 'S', 'E', 'Q', 'I', 'D', '1', '\0' };
constexpr size_t CRC_OFFSET = FACTORY_IDENTITY_RECORD_SIZE - 4;

void write_u32_le(uint8_t* output, uint32_t value)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
    output[2] = static_cast<uint8_t>(value >> 16);
    output[3] = static_cast<uint8_t>(value >> 24);
}

uint32_t read_u32_le(const uint8_t* input)
{
    return static_cast<uint32_t>(input[0]) |
           (static_cast<uint32_t>(input[1]) << 8) |
           (static_cast<uint32_t>(input[2]) << 16) |
           (static_cast<uint32_t>(input[3]) << 24);
}

void encode_text(uint8_t* output, size_t width, const char* text)
{
    std::memset(output, 0, width);
    if (text != nullptr && width > 0)
    {
        size_t length = 0;
        while (length < width && text[length] != '\0')
            ++length;
        std::memcpy(output, text, length);
    }
}

void decode_text(char* output, size_t output_size, const uint8_t* input,
                 size_t width)
{
    const size_t copy = output_size - 1 < width ? output_size - 1 : width;
    std::memcpy(output, input, copy);
    output[copy] = '\0';
}
} // namespace

uint32_t factory_identity_crc32(const uint8_t* data, size_t length)
{
    uint32_t crc = 0xffffffffu;
    for (size_t index = 0; index < length; ++index)
    {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

bool encode_factory_identity(const FactoryIdentity& identity, uint8_t* record,
                             size_t capacity)
{
    if (record == nullptr || capacity < FACTORY_IDENTITY_RECORD_SIZE)
        return false;
    std::memset(record, 0, FACTORY_IDENTITY_RECORD_SIZE);
    std::memcpy(record, MAGIC, sizeof(MAGIC));
    record[8]  = FACTORY_IDENTITY_SCHEMA;
    record[9]  = static_cast<uint8_t>(identity.mcu_family);
    record[10] = identity.default_i2c_address;
    write_u32_le(record + 12, identity.feature_bits);
    encode_text(record + 16, 16, identity.product);
    encode_text(record + 32, 8, identity.hardware_revision);
    encode_text(record + 40, 16, identity.assembly_variant);
    encode_text(record + 56, 16, identity.batch);
    encode_text(record + 72, 24, identity.serial);
    encode_text(record + 96, 10, identity.manufacture_date);
    write_u32_le(record + 106, identity.flash_size_bytes);
    write_u32_le(record + CRC_OFFSET,
                 factory_identity_crc32(record, CRC_OFFSET));
    return true;
}

bool decode_factory_identity(const uint8_t* record, size_t length,
                             FactoryIdentity& identity)
{
    if (record == nullptr || length < FACTORY_IDENTITY_RECORD_SIZE ||
        std::memcmp(record, MAGIC, sizeof(MAGIC)) != 0 ||
        record[8] != FACTORY_IDENTITY_SCHEMA ||
        read_u32_le(record + CRC_OFFSET) != factory_identity_crc32(record, CRC_OFFSET))
    {
        return false;
    }

    FactoryIdentity decoded;
    decoded.mcu_family = static_cast<McuFamily>(record[9]);
    if (decoded.mcu_family != McuFamily::RP2040 &&
        decoded.mcu_family != McuFamily::RP2350)
        return false;
    decoded.default_i2c_address = record[10];
    if (decoded.default_i2c_address > 126)
        return false;
    decoded.feature_bits     = read_u32_le(record + 12);
    decoded.flash_size_bytes = read_u32_le(record + 106);
    decode_text(decoded.product, sizeof(decoded.product), record + 16, 16);
    decode_text(decoded.hardware_revision, sizeof(decoded.hardware_revision), record + 32, 8);
    decode_text(decoded.assembly_variant, sizeof(decoded.assembly_variant), record + 40, 16);
    decode_text(decoded.batch, sizeof(decoded.batch), record + 56, 16);
    decode_text(decoded.serial, sizeof(decoded.serial), record + 72, 24);
    decode_text(decoded.manufacture_date, sizeof(decoded.manufacture_date), record + 96, 10);
    if (decoded.product[0] == '\0' || decoded.hardware_revision[0] == '\0')
        return false;
    identity = decoded;
    return true;
}

bool load_factory_identity(FactoryIdentity& identity)
{
#if defined(ARDUINO) && defined(USEQ_FACTORY_IDENTITY_FLASH_OFFSET)
    constexpr uintptr_t XIP_BASE = 0x10000000u;
    const auto* record = reinterpret_cast<const uint8_t*>(
        XIP_BASE + static_cast<uintptr_t>(USEQ_FACTORY_IDENTITY_FLASH_OFFSET));
    return decode_factory_identity(record, FACTORY_IDENTITY_RECORD_SIZE, identity);
#else
    (void)identity;
    return false;
#endif
}

const char* mcu_family_name(McuFamily family)
{
    switch (family)
    {
    case McuFamily::RP2040: return "rp2040";
    case McuFamily::RP2350: return "rp2350";
    default: return "unknown";
    }
}

} // namespace firmware
