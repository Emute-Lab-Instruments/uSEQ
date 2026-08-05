#ifndef USEQ_FIRMWARE_FACTORY_IDENTITY_H
#define USEQ_FIRMWARE_FACTORY_IDENTITY_H

#include <cstddef>
#include <cstdint>

namespace firmware
{

enum class McuFamily : uint8_t
{
    Unknown = 0,
    RP2040  = 1,
    RP2350  = 2,
};

namespace factory_feature
{
inline constexpr uint32_t USB_UPDATE = 1u << 0;
inline constexpr uint32_t I2C_OUTPUTS = 1u << 1;
} // namespace factory_feature

struct FactoryIdentity
{
    char product[17]          = {};
    char hardware_revision[9] = {};
    char assembly_variant[17] = {};
    char batch[17]            = {};
    char serial[25]           = {};
    char manufacture_date[11] = {}; // YYYY-MM-DD
    McuFamily mcu_family      = McuFamily::Unknown;
    uint8_t default_i2c_address = 0;
    uint32_t feature_bits       = 0;
    uint32_t flash_size_bytes   = 0;
};

inline constexpr size_t FACTORY_IDENTITY_RECORD_SIZE = 128;
inline constexpr uint8_t FACTORY_IDENTITY_SCHEMA = 1;

uint32_t factory_identity_crc32(const uint8_t* data, size_t length);
bool encode_factory_identity(const FactoryIdentity& identity, uint8_t* record,
                             size_t capacity);
bool decode_factory_identity(const uint8_t* record, size_t length,
                             FactoryIdentity& identity);

// Reads the reserved factory sector when a hardware profile supplies
// USEQ_FACTORY_IDENTITY_FLASH_OFFSET. Returns false for an erased/corrupt blob.
bool load_factory_identity(FactoryIdentity& identity);

const char* mcu_family_name(McuFamily family);

} // namespace firmware

#endif
